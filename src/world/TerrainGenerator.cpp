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
constexpr Biome kDesert{"deserto", Color{226, 205, 148, 255},
                        Color{184, 146, 104, 255}};
constexpr Biome kPlains{"planicie", Color{126, 182, 88, 255},
                        Color{156, 128, 100, 255}};
constexpr Biome kForest{"floresta", Color{74, 136, 62, 255},
                        Color{138, 116, 92, 255}};
constexpr Biome kSavanna{"savana", Color{176, 182, 92, 255},
                         Color{170, 136, 96, 255}};
constexpr Biome kTundra{"tundra", Color{164, 186, 168, 255},
                        Color{132, 136, 140, 255}};

// Cores fixas das faixas que nao dependem de bioma.
constexpr Color kDeepWaterBed{46, 68, 84, 255};
constexpr Color kWetSand{202, 190, 152, 255};
constexpr Color kSand{228, 214, 168, 255};
constexpr Color kSnow{244, 247, 250, 255};

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
void TerrainGenerator::RiverFactors(float x, float z, float& valley,
                                    float& channel) const {
    const float n = noise_.Fbm2(x * settings_.riverScale,
                                z * settings_.riverScale, 4);
    const float distance = std::fabs(n);

    channel = 1.0f - SmoothStep(0.0f, settings_.riverWidth, distance);
    valley = 1.0f - SmoothStep(0.0f, settings_.valleyWidth, distance);
}

float TerrainGenerator::SurfaceHeight(float x, float z) const {
    float height = settings_.groundLevel;

    // --- colinas: fbm comum, ondulacao arredondada -------------------------
    if (settings_.hillHeight != 0.0f) {
        height += settings_.hillHeight *
                  noise_.Fbm2(x * settings_.hillScale, z * settings_.hillScale,
                              settings_.hillOctaves);
    }

    // --- montanhas: ridged ao quadrado, moduladas por regiao ---------------
    // O relevo e calculado uma vez e reaproveitado pelos rios: e a mesma
    // pergunta ("estou em regiao montanhosa?") e o ruido nao e barato.
    const float relief = (settings_.mountainHeight != 0.0f ||
                          settings_.riverMountainResistance > 0.0f)
                             ? ReliefScale(x, z)
                             : 0.0f;

    if (settings_.mountainHeight != 0.0f) {
        const float r = noise_.Ridged2(x * settings_.mountainScale,
                                       z * settings_.mountainScale,
                                       settings_.mountainOctaves);
        height += settings_.mountainHeight * r * r * relief;
    }

    height = ApplyTerraces(height);

    // --- rios: depois dos terracos -----------------------------------------
    // A ordem importa. Terracar o leito do rio produziria degraus dentro da
    // agua; escavando por ultimo, o canal sai liso mesmo com terracos ligados.
    if (settings_.riverDepth != 0.0f || settings_.valleyDepth != 0.0f) {
        float valley = 0.0f;
        float channel = 0.0f;
        RiverFactors(x, z, valley, channel);

        // Montanha resiste: sem isto o rio abriria canyon reto pelos picos.
        const float resistance =
            1.0f - relief * settings_.riverMountainResistance;
        valley *= resistance;
        channel *= resistance;

        // O vale rebaixa o entorno em rampa larga...
        height -= settings_.valleyDepth * valley * valley;

        // ...e o canal puxa o leito para baixo do nivel do mar, para a lamina
        // d'agua cobri-lo.
        const float bed = settings_.seaLevel - settings_.riverDepth;
        height += (bed - height) * channel;
    }

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
const Biome& TerrainGenerator::PickBiome(float x, float z) const {
    // Dois campos independentes, como temperatura/umidade do Minecraft. Os
    // deslocamentos grandes evitam que fiquem correlacionados.
    const float temperature = ToUnit(noise_.Fbm2(
        x * settings_.temperatureScale, z * settings_.temperatureScale, 3));
    const float humidity =
        ToUnit(noise_.Fbm2((x + 8192.0f) * settings_.humidityScale,
                           (z - 8192.0f) * settings_.humidityScale, 3));

    // Cascata simples de proposito: da para ler e da para estender sem
    // entender o resto do sistema.
    if (temperature < 0.35f) return kTundra;
    if (temperature > 0.68f) return (humidity < 0.42f) ? kDesert : kSavanna;
    return (humidity > 0.55f) ? kForest : kPlains;
}

Color TerrainGenerator::SurfaceColor(Vector3 position, Vector3 normal) const {
    const Biome& biome = PickBiome(position.x, position.z);
    const float height = position.y - settings_.seaLevel;

    // A paisagem inteira sai desta lista. Mexer aqui muda o mundo mais
    // rapido que mexer em qualquer equacao.
    const ColorStop stops[] = {
        {-18.0f, kDeepWaterBed},
        {-2.0f, kWetSand},
        {settings_.beachHeight, kSand},
        {settings_.grassHeight, biome.lowland},
        {settings_.rockHeight, biome.rock},
        {settings_.rockHeight + 16.0f, kSnow},
    };
    Color color = EvaluateRamp(stops, 6, height);

    // Encosta ingreme mostra rocha, independente da altitude - e o que impede
    // grama de "escorrer" pelas paredes verticais.
    const float steep = 1.0f - SmoothStep(0.45f, 0.80f, normal.y);
    color = MixColor(color, biome.rock, steep * 0.85f);

    // Estratos: escurece a base de cada camada, na mesma cadencia dos
    // terracos. So acima de terraceStart, onde os terracos existem.
    if (settings_.strataContrast > 0.0f && settings_.terraceStep > 0.0f) {
        const float k = position.y / settings_.terraceStep;
        const float frac = k - std::floor(k);
        const float visible =
            SmoothStep(settings_.terraceStart,
                       settings_.terraceStart + settings_.terraceFade,
                       position.y);
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
