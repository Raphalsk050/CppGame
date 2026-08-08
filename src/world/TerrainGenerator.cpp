#include "world/TerrainGenerator.hpp"

#include <algorithm>
#include <cmath>

namespace world {
namespace {

// ---------------------------------------------------------------------------
//  TABELA DE BIOMAS
//
//  Bioma escolhe COR, nunca altura (ver a regra no topo do .hpp). Para
//  acrescentar um: adicione a entrada aqui e o ramo em PickBiome.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
//  BIOMAS - coordenadas de Whittaker, ancoradas em regioes reais
//
//  Temperatura em C (media anual) e precipitacao em cm/ano. Os valores nao sao
//  estimados: cada linha esta ancorada numa regiao real cujo clima e conhecido,
//  citada no comentario. Isso importa porque as RELACOES entre biomas passam a
//  ser as verdadeiras - deserto quente e seco fica longe de floresta tropical
//  quente e umida no eixo da chuva, e nao ao lado dela por acaso do ruido.
// ---------------------------------------------------------------------------
constexpr Biome kBiomes[] = {
    // --- frio ---------------------------------------------------------
    // Tundra artica (norte do Alasca / Nunavut, Canada): ~-12 C, ~25 cm.
    {"tundra",            -12.0f,  25.0f, {186, 194, 178, 255}, {132, 134, 136, 255}},
    // Taiga / floresta boreal (interior do Canada, Escandinavia): -3 C, 45 cm.
    {"taiga",              -3.0f,  45.0f, { 74, 106,  80, 255}, {114, 112, 106, 255}},
    // Costa oeste da Noruega: fria E muito chuvosa (orografica do Atlantico).
    {"floresta boreal umida", 5.0f, 200.0f, { 58, 104,  70, 255}, {108, 110, 108, 255}},

    // --- temperado -----------------------------------------------------
    // Estepe / pradaria (Alberta, Canada; estepes eurasianas): 8 C, 40 cm.
    {"pradaria",            8.0f,  40.0f, {158, 168,  96, 255}, {142, 126,  98, 255}},
    // Floresta decidua temperada (leste do Canada, centro da Europa).
    {"floresta temperada", 12.0f, 110.0f, { 92, 148,  62, 255}, {128, 114,  94, 255}},
    // Floresta temperada chuvosa (costa da Colombia Britanica): 12 C, 250 cm.
    {"floresta chuvosa",   12.0f, 250.0f, { 54, 118,  58, 255}, {112, 108,  96, 255}},

    // --- quente --------------------------------------------------------
    // Saara / Namibe (Africa): quente e praticamente sem chuva.
    {"deserto",            28.0f,   6.0f, {216, 196, 146, 255}, {184, 150, 106, 255}},
    // Caatinga (nordeste do Brasil) e Sahel: semiarido quente.
    {"semiarido",          26.0f,  55.0f, {184, 174, 108, 255}, {168, 140, 102, 255}},
    // Savana / cerrado (Serengeti; cerrado brasileiro): 24 C, 100 cm.
    {"savana",             24.0f, 100.0f, {166, 176,  86, 255}, {160, 134,  98, 255}},
    // Floresta tropical sazonal (Mata Atlantica de interior, Congo norte).
    {"floresta sazonal",   25.0f, 170.0f, { 84, 148,  56, 255}, {130, 112,  88, 255}},
    // Amazonia / bacia do Congo: quente e muito chuvosa.
    {"floresta tropical",  26.0f, 280.0f, { 40, 112,  44, 255}, {110, 100,  82, 255}},
    // Pantanal / manguezal: quente, encharcado, vegetacao escura.
    {"pantanal",           25.0f, 340.0f, { 62, 108,  58, 255}, {104,  98,  84, 255}},
};
constexpr int kBiomeCount = sizeof(kBiomes) / sizeof(kBiomes[0]);

// Faixas usadas para normalizar as duas dimensoes antes de medir distancia. Sem
// isso a precipitacao (0..350) dominaria a temperatura (-15..30) e o bioma
// dependeria quase so da chuva.
constexpr float kTemperatureRange = 45.0f;
constexpr float kPrecipitationRange = 350.0f;

// Cores fixas das faixas que nao dependem de bioma.
constexpr Color kDeepWaterBed{44, 62, 76, 255};
constexpr Color kWetSand{178, 168, 132, 255};
constexpr Color kSand{216, 202, 155, 255};
constexpr Color kSnow{246, 249, 252, 255};
// Rocha nua. Independe de bioma: a face ingreme de uma montanha e a mesma
// pedra em qualquer clima.
constexpr Color kStone{122, 122, 124, 255};

inline float Clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

inline float ToUnit(float noise) { return Clamp01(noise * 0.5f + 0.5f); }

// Interpolacao com derivada zero nas pontas. Usada em toda transicao que
// precisa ser suave - o projeto inteiro depende de nao introduzir quina.
inline float SmoothStep(float edge0, float edge1, float x) {
    if (edge1 <= edge0) return (x < edge0) ? 0.0f : 1.0f;
    const float t = Clamp01((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

inline unsigned char MixChannel(unsigned char a, unsigned char b, float t) {
    return static_cast<unsigned char>(std::lround(
        static_cast<float>(a) +
        (static_cast<float>(b) - static_cast<float>(a)) * t));
}

Color MixColor(Color a, Color b, float t) {
    t = Clamp01(t);
    return Color{MixChannel(a.r, b.r, t), MixChannel(a.g, b.g, t),
                 MixChannel(a.b, b.b, t), 255};
}

Color Darken(Color c, float amount) {
    return MixColor(c, Color{18, 20, 26, 255}, amount);
}

// Rampa de cor por altitude: pares (altura, cor) em ordem crescente,
// interpolados linearmente. Editar a paisagem e mexer nesta lista.
struct ColorStop {
    float height;  // relativa ao nivel do mar
    Color color;
};

Color EvaluateRamp(const ColorStop* stops, int count, float height) {
    if (height <= stops[0].height) return stops[0].color;
    for (int i = 1; i < count; ++i) {
        if (height <= stops[i].height) {
            const float span = stops[i].height - stops[i - 1].height;
            const float t = (span > 0.0f)
                                ? (height - stops[i - 1].height) / span
                                : 1.0f;
            // SmoothStep no lugar de linear: evita a "banda de Mach", a linha
            // que o olho enxerga na quebra de derivada de um degrade linear.
            return MixColor(stops[i - 1].color, stops[i].color,
                            SmoothStep(0.0f, 1.0f, t));
        }
    }
    return stops[count - 1].color;
}

}  // namespace

TerrainGenerator::TerrainGenerator(TerrainSettings settings)
    : settings_(settings), noise_(settings.seed) {}

void TerrainGenerator::SetSettings(const TerrainSettings& settings) {
    const bool reseed = settings.seed != settings_.seed;
    settings_ = settings;
    if (reseed) noise_ = Noise(settings_.seed);
}

// ===========================================================================
//  RELEVO
// ===========================================================================

// Onde ha cordilheira e onde ha planicie. Continuo por construcao: e ruido
// suave passado por SmoothStep, sem nenhuma decisao discreta no caminho.
float TerrainGenerator::ReliefScale(float x, float z) const {
    const float n = ToUnit(noise_.Fbm2(x * settings_.reliefScale,
                                       z * settings_.reliefScale, 3));
    return SmoothStep(settings_.reliefLow, settings_.reliefHigh, n);
}

// Quantiza a altura em camadas. O patamar ocupa a maior parte do degrau e a
// subida acontece numa fatia curta - e essa proporcao que faz ler como
// estrato de rocha em vez de escada regular.
float TerrainGenerator::ApplyTerraces(float height) const {
    if (settings_.terraceStrength <= 0.0f || settings_.terraceStep <= 0.0f) {
        return height;
    }

    // Entra gradualmente com a altitude: a baixada continua lisa.
    const float amount =
        settings_.terraceStrength *
        SmoothStep(settings_.terraceStart,
                   settings_.terraceStart + settings_.terraceFade, height);
    if (amount <= 0.0f) return height;

    const float k = height / settings_.terraceStep;
    const float base = std::floor(k);
    const float frac = k - base;
    const float shaped = SmoothStep(0.55f, 0.85f, frac);
    const float terraced = (base + shaped) * settings_.terraceStep;

    return height + (terraced - height) * amount;
}

// O eixo do rio e o lugar onde um ruido 2D cruza zero. Esse cruzamento e uma
// curva continua e sinuosa que nunca se fecha em circulo nem se cruza - ou
// seja, ja tem a forma de um rio, sem precisar simular erosao nem tracar
// caminho. |n| perto de zero mede a distancia ao eixo.
void TerrainGenerator::RiverFactors(float x, float z, float gate, float& valley,
                                    float& channel) const {
    const float n = noise_.Fbm2(x * settings_.riverScale,
                                z * settings_.riverScale, 4);
    const float distance = std::fabs(n);

    // Largura modulada ao longo do curso. Um rio de espessura constante e a
    // assinatura mais visivel de terreno gerado por formula.
    float width = settings_.riverWidth;
    if (settings_.riverWidthVariation > 0.0f) {
        const float w = ToUnit(noise_.Fbm2(x * settings_.riverWidthScale,
                                           z * settings_.riverWidthScale, 2));
        width *= 1.0f + settings_.riverWidthVariation * (w * 2.0f - 1.0f);
    }

    // O gate entra na LARGURA, nao so na profundidade. Aplicado apenas a
    // profundidade, o rio terminava com largura cheia e leito raso - um toco
    // reto encravado na encosta. Afinando a largura junto, ele se estreita ate
    // virar cabeceira e desaparecer.
    width *= gate;
    if (width <= 0.0005f) {
        channel = 0.0f;
        valley = 0.0f;
        return;
    }

    channel = 1.0f - SmoothStep(0.0f, width, distance);
    valley = 1.0f - SmoothStep(0.0f, settings_.valleyWidth, distance);
}

// Achata o relevo onde e alto. E o analogo do parametro de erosao do
// Minecraft 1.18: sem ele, hills e montanhas cobrem o mundo uniformemente e
// nao existe o contraste de planicie extensa contra encosta ingreme.
float TerrainGenerator::Erosion(float x, float z) const {
    if (settings_.erosionStrength <= 0.0f) return 0.0f;
    const float n = ToUnit(noise_.Fbm2(x * settings_.erosionScale,
                                       z * settings_.erosionScale, 3));
    return SmoothStep(0.45f, 0.85f, n) * settings_.erosionStrength;
}

// Relevo sem rios: colinas + montanhas + terracos.
float TerrainGenerator::BaseHeight(float x, float z) const {
    const float flatten = 1.0f - Erosion(x, z);

    float height = settings_.groundLevel;

    if (settings_.hillHeight != 0.0f) {
        height += settings_.hillHeight * flatten *
                  noise_.Fbm2(x * settings_.hillScale, z * settings_.hillScale,
                              settings_.hillOctaves);
    }

    if (settings_.mountainHeight != 0.0f) {
        const float r = noise_.Ridged2(x * settings_.mountainScale,
                                       z * settings_.mountainScale,
                                       settings_.mountainOctaves);
        // O expoente controla o PERFIL da encosta, nao a altura: quanto maior,
        // mais o flanco achata e so a crista sobe.
        const float shaped = std::pow(r, settings_.mountainSharpness);
        height += settings_.mountainHeight * shaped * ReliefScale(x, z) * flatten;
    }

    return ApplyTerraces(height);
}

// Diferenca entre a media da vizinhanca e o ponto: positivo em depressao.
//
// E este termo que da ao rio a nocao de "para baixo" que um contorno de ruido
// nao tem. Sem ele o curso atravessava divisor de aguas e subia encosta, que e
// o que fazia os rios nao terem sentido. Aqui o rio so existe onde o terreno
// ja forma calha - e calha desce sozinha.
float TerrainGenerator::Concavity(float x, float z) const {
    const float d = settings_.riverConcavitySample;
    if (d <= 0.0f) return 0.0f;

    const float here = BaseHeight(x, z);
    const float average = 0.25f * (BaseHeight(x + d, z) + BaseHeight(x - d, z) +
                                   BaseHeight(x, z + d) + BaseHeight(x, z - d));
    // Normaliza pela distancia para o valor nao depender da escala escolhida.
    return (average - here) / d * 10.0f;
}

float TerrainGenerator::SurfaceHeight(float x, float z) const {
    float height = BaseHeight(x, z);

    if (settings_.riverDepth == 0.0f && settings_.valleyDepth == 0.0f) {
        return height;
    }

    // --- comporta: onde pode haver rio -------------------------------------
    // Tres condicoes multiplicadas, todas continuas:
    //   altitude baixa, terreno concavo (calha), fora de regiao montanhosa.
    const float lowland =
        1.0f - SmoothStep(settings_.riverAltitudeLimit,
                          settings_.riverAltitudeLimit +
                              settings_.riverAltitudeFade,
                          height);

    const float concave =
        SmoothStep(settings_.riverConcavityBias * 0.4f,
                   settings_.riverConcavityBias, Concavity(x, z));

    const float away =
        1.0f - ReliefScale(x, z) * settings_.riverMountainResistance;

    const float gate = lowland * concave * away;
    if (gate <= 0.0f) return height;

    float valley = 0.0f;
    float channel = 0.0f;
    RiverFactors(x, z, gate, valley, channel);

    // O vale rebaixa o entorno em rampa larga...
    height -= settings_.valleyDepth * valley * valley * gate;

    // ...e o canal puxa o leito para baixo do nivel do mar, para a lamina
    // d'agua cobri-lo.
    const float bed = settings_.seaLevel - settings_.riverDepth;
    height += (bed - height) * channel;

    return height;
}

// ===========================================================================
//  DENSIDADE
// ===========================================================================
float TerrainGenerator::DensityAt(Vector3 p, float surfaceHeight) const {
    // Termo base. Sozinho, isto E o mundo plano: negativo abaixo da altura da
    // superficie (solido), positivo acima (ar), zero exatamente nela.
    float density = p.y - surfaceHeight;

    // --- cavernas ---------------------------------------------------------
    // Somar valor POSITIVO abre vazio, porque empurra a densidade para o lado
    // do ar. O tunel fica onde o ruido 3D passa perto de zero: como esse
    // cruzamento e uma superficie fina no volume, o conjunto vira uma rede de
    // galerias conectadas em vez de bolhas isoladas.
    if (settings_.caveStrength > 0.0f) {
        const float depth = surfaceHeight - p.y;  // >0 abaixo da superficie
        if (depth > settings_.caveDepthBelowSurface) {
            const float n = noise_.Perlin3(p.x * settings_.caveScale,
                                           p.y * settings_.caveScale,
                                           p.z * settings_.caveScale);
            const float tunnel = settings_.caveThreshold - std::fabs(n);
            if (tunnel > 0.0f) {
                // Esvanece perto do teto para a caverna nao cortar o terreno
                // num degrau reto.
                const float fade =
                    Clamp01((depth - settings_.caveDepthBelowSurface) / 5.0f);
                density += settings_.caveStrength * tunnel * fade;
            }
        }
    }

    return density;
}

// ===========================================================================
//  BIOMAS E COR
// ===========================================================================
// Relevo GROSSEIRO, so a estrutura de grande escala das serras.
//
// Existe por performance: o calculo orografico precisa amostrar o terreno
// varias vezes a barlavento, e usar BaseHeight (14 oitavas) tres vezes por
// VERTICE seria proibitivo. A sombra de chuva depende so da massa da montanha,
// nao do detalhe fino, entao 2 oitavas bastam.
float TerrainGenerator::CoarseHeight(float x, float z) const {
    if (settings_.mountainHeight == 0.0f) return settings_.groundLevel;
    const float r = noise_.Ridged2(x * settings_.mountainScale,
                                   z * settings_.mountainScale, 2);
    const float relief = SmoothStep(settings_.reliefLow, settings_.reliefHigh,
                                    ToUnit(noise_.Fbm2(x * settings_.reliefScale,
                                                       z * settings_.reliefScale,
                                                       2)));
    return settings_.groundLevel +
           settings_.mountainHeight * r * r * relief;
}

// Clima num ponto, DERIVADO DO RELEVO.
//
// Tres mecanismos reais, nenhum sorteado:
//
// 1. Gradiente termico vertical: o ar esfria ~6,5 C por km de altitude
//    (environmental lapse rate). E por isso que ha neve e tundra no alto da
//    montanha mesmo no tropico.
//
// 2. Efeito orografico: ar umido forcado a subir uma serra condensa e chove na
//    encosta de barlavento; do outro lado desce ja seco. E a sombra de chuva,
//    e e ela que poe deserto imediatamente ao lado de floresta - relacao que
//    ruido independente nunca produziria.
//
// 3. Variacao regional de fundo, para nao ficar tudo determinado pelo relevo.
void TerrainGenerator::Climate(float x, float z, float altitude,
                               float& temperature,
                               float& precipitation) const {
    // --- temperatura ------------------------------------------------------
    const float regional =
        noise_.Fbm2(x * settings_.temperatureScale,
                    z * settings_.temperatureScale, 3);
    temperature =
        settings_.seaLevelTemperature + settings_.temperatureVariation * regional;

    // Lapse rate. A altitude vira quilometros pela escala do mundo.
    const float km = std::max(0.0f, altitude - settings_.seaLevel) *
                     settings_.metersPerUnit / 1000.0f;
    temperature -= settings_.lapseRatePerKm * km;

    // --- precipitacao -----------------------------------------------------
    const float base =
        noise_.Fbm2((x + 8192.0f) * settings_.humidityScale,
                    (z - 8192.0f) * settings_.humidityScale, 3);
    precipitation =
        settings_.basePrecipitation + settings_.precipitationVariation * base;

    if (settings_.orographicStrength > 0.0f) {
        // Normaliza a direcao do vento uma vez.
        const float wl = std::sqrt(settings_.windDirectionX *
                                       settings_.windDirectionX +
                                   settings_.windDirectionZ *
                                       settings_.windDirectionZ);
        if (wl > 0.0f) {
            const float wx = settings_.windDirectionX / wl;
            const float wz = settings_.windDirectionZ / wl;
            const float d = settings_.orographicDistance;

            // Maior obstaculo entre este ponto e o vento que vem de la.
            const float up1 = CoarseHeight(x - wx * d * 0.45f, z - wz * d * 0.45f);
            const float up2 = CoarseHeight(x - wx * d, z - wz * d);
            const float barrier = std::max(up1, up2) - altitude;

            // Sombra de chuva: quanto de serra ficou no caminho.
            if (barrier > 0.0f) {
                const float shadow = SmoothStep(0.0f, 60.0f, barrier) *
                                     settings_.orographicStrength;
                precipitation *= (1.0f - shadow);
            } else {
                // Encosta de barlavento: o ar esta subindo aqui, entao chove
                // mais. `-barrier` e o quanto este ponto se ergueu acima do que
                // vinha antes dele.
                const float rise = SmoothStep(0.0f, 60.0f, -barrier);
                precipitation += settings_.windwardGain * rise;
            }
        }
    }

    precipitation = std::max(0.0f, precipitation);
}

const Biome& TerrainGenerator::PickBiome(float x, float z) const {
    const float altitude = BaseHeight(x, z);
    float temperature = 0.0f;
    float precipitation = 0.0f;
    Climate(x, z, altitude, temperature, precipitation);

    int best = 0;
    float bestDistance = 1e30f;
    for (int i = 0; i < kBiomeCount; ++i) {
        const float dt =
            (temperature - kBiomes[i].temperature) / kTemperatureRange;
        const float dp =
            (precipitation - kBiomes[i].precipitation) / kPrecipitationRange;
        const float d = dt * dt + dp * dp;
        if (d < bestDistance) {
            bestDistance = d;
            best = i;
        }
    }
    return kBiomes[best];
}

// Media ponderada de TODOS os biomas pela distancia no diagrama de Whittaker.
//
// E aqui que morre a fronteira dura. Com escolha discreta, dois vertices
// vizinhos caindo em lados opostos de um limiar recebiam cores completamente
// diferentes - a linha reta que aparecia cortando o campo.
TerrainGenerator::BiomePalette TerrainGenerator::BlendBiomes(
    float x, float z, float altitude) const {
    float temperature = 0.0f;
    float precipitation = 0.0f;
    Climate(x, z, altitude, temperature, precipitation);

    // Mosqueado: perturba o clima em alta frequencia, o que na natureza
    // corresponde a variacao local de solo e drenagem. Como entra ANTES da
    // mistura, uma clareira de campo pode surgir dentro da floresta.
    if (settings_.biomeMottleStrength > 0.0f) {
        const float s = settings_.biomeMottleScale;
        temperature += settings_.biomeMottleStrength * 3.0f *
                       noise_.Fbm2(x * s, z * s, 3);
        precipitation += settings_.biomeMottleStrength * 45.0f *
                         noise_.Fbm2((x - 991.0f) * s, (z + 617.0f) * s, 3);
    }

    float lowR = 0.0f, lowG = 0.0f, lowB = 0.0f;
    float rockR = 0.0f, rockG = 0.0f, rockB = 0.0f;
    float total = 0.0f;

    for (int i = 0; i < kBiomeCount; ++i) {
        // Eixos normalizados: sem isso a precipitacao (0..350) dominaria a
        // temperatura (-15..30) e o bioma dependeria quase so da chuva.
        const float dt =
            (temperature - kBiomes[i].temperature) / kTemperatureRange;
        const float dp =
            (precipitation - kBiomes[i].precipitation) / kPrecipitationRange;
        const float d2 = dt * dt + dp * dp + 0.0015f;

        const float w = std::pow(1.0f / d2, settings_.biomeBlendSharpness);

        lowR += w * kBiomes[i].lowland.r;
        lowG += w * kBiomes[i].lowland.g;
        lowB += w * kBiomes[i].lowland.b;
        rockR += w * kBiomes[i].rock.r;
        rockG += w * kBiomes[i].rock.g;
        rockB += w * kBiomes[i].rock.b;
        total += w;
    }

    const float inv = (total > 0.0f) ? 1.0f / total : 0.0f;
    const auto to8 = [](float v) {
        return static_cast<unsigned char>(std::clamp(v, 0.0f, 255.0f));
    };

    return BiomePalette{
        Color{to8(lowR * inv), to8(lowG * inv), to8(lowB * inv), 255},
        Color{to8(rockR * inv), to8(rockG * inv), to8(rockB * inv), 255},
    };
}

Color TerrainGenerator::SurfaceColor(Vector3 position, Vector3 normal) const {
    // A altitude entra no clima: e ela que esfria o topo da montanha.
    const BiomePalette biome = BlendBiomes(position.x, position.z, position.y);
    const float height = position.y - settings_.seaLevel;

    // A paisagem inteira sai desta lista. Mexer aqui muda o mundo mais
    // rapido que mexer em qualquer equacao.
    const ColorStop stops[] = {
        {-18.0f, kDeepWaterBed},
        {-1.0f, kWetSand},
        {settings_.beachHeight, kSand},
        {settings_.grassHeight, biome.lowland},
        {settings_.rockHeight, biome.rock},
        {settings_.snowHeight, kSnow},
    };
    Color color = EvaluateRamp(stops, 6, height);

    // A INCLINACAO manda mais que a altitude. Numa montanha real quase toda a
    // face e rocha nua: vegetacao so agarra onde o terreno e razoavelmente
    // plano. Sem este termo forte, a grama escorre pelas paredes verticais e a
    // montanha vira um monte verde.
    // Faixa LARGA de proposito. Com 0.62..0.90 a rocha entrava num
    // intervalo curto de inclinacao, e numa encosta lisa a normal atravessa
    // esse intervalo em poucos vertices - o que aparecia como aresta reta
    // separando pedra de grama, sem nenhuma feicao da geometria por baixo.
    const float flatness = SmoothStep(0.42f, 0.93f, normal.y);
    color = MixColor(kStone, color, flatness);

    // Faixa fina de areia na linha d'agua, so onde o terreno e plano. E o que
    // da margem ao rio em vez de a agua encostar direto na vegetacao.
    const float shore = (1.0f - SmoothStep(0.0f, settings_.beachHeight * 2.0f,
                                           std::fabs(height))) *
                        flatness;
    color = MixColor(color, kSand, shore * 0.8f);

    // Estratos: escurece a base de cada camada, na mesma cadencia dos
    // terracos.
    //
    // Multiplicar por terraceStrength e essencial. Antes disso, o listrado
    // aparecia mesmo com os terracos DESLIGADOS - faixas horizontais pintadas
    // sobre encosta lisa, que nao correspondiam a nenhuma feicao da geometria.
    if (settings_.strataContrast > 0.0f && settings_.terraceStep > 0.0f &&
        settings_.terraceStrength > 0.0f) {
        const float k = position.y / settings_.terraceStep;
        const float frac = k - std::floor(k);
        const float visible =
            SmoothStep(settings_.terraceStart,
                       settings_.terraceStart + settings_.terraceFade,
                       position.y) *
            settings_.terraceStrength;
        color = Darken(color,
                       settings_.strataContrast * (1.0f - frac) * visible);
    }

    // Escurece com a profundidade abaixo da superficie: e a unica pista de
    // altura dentro das cavernas, onde nao ha horizonte nem ceu.
    const float depth = SurfaceHeight(position.x, position.z) - position.y;
    if (depth > 0.0f) color = Darken(color, Clamp01(depth / 45.0f) * 0.6f);

    return color;
}

void TerrainGenerator::HeightBounds(float& minHeight, float& maxHeight) const {
    // Limites conservadores a partir das amplitudes. ReliefScale e no maximo
    // 1, entao a montanha nao passa de mountainHeight.
    const float hills = std::fabs(settings_.hillHeight);
    const float mountains = std::fabs(settings_.mountainHeight);

    // O piso tem de incluir o leito do rio: ele desce abaixo do nivel do mar,
    // que pode estar bem abaixo do chao. Errar para baixo aqui faria o
    // ChunkManager descartar como "solido macico" justamente a camada onde o
    // rio corre - e o rio sumiria.
    const float riverBed = settings_.seaLevel - std::fabs(settings_.riverDepth);
    const float valleyFloor =
        settings_.groundLevel - hills - std::fabs(settings_.valleyDepth);

    minHeight = std::min(riverBed, valleyFloor) - 2.0f;
    maxHeight = settings_.groundLevel + hills + mountains +
                settings_.terraceStep + 2.0f;
}

}  // namespace world
