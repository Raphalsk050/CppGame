#pragma once

#include <cstdint>

#include "field/ScalarField.hpp"
#include "raylib.h"
#include "world/Noise.hpp"

namespace world {

// ===========================================================================
//  ESTE E O ARQUIVO PARA EDITAR.
//
//  Todo o formato do mundo sai daqui. O resto do projeto (chunks, streaming,
//  marching cubes, render) nao sabe nada sobre montanhas, cavernas ou biomas -
//  so pede densidade e cor.
//
//  CONVENCAO DE SINAL: densidade NEGATIVA e solido, POSITIVA e ar, e a
//  superficie e o zero. E a mesma convencao de field/ScalarField.hpp, e e ela
//  que faz o gradiente apontar para fora e as normais sairem certas.
//
//  REGRA QUE NAO PODE SER QUEBRADA: tudo que multiplica ou soma ALTURA tem de
//  ser CONTINUO em (x, z). Uma versao anterior modulava a amplitude das
//  montanhas por PickBiome, que e uma escolha discreta - e cada fronteira de
//  bioma virava um paredao vertical de dezenas de unidades, em curvas fechadas
//  que seguiam o contorno do ruido de bioma. Bioma pode escolher COR (ali a
//  descontinuidade se dissolve no sombreamento); nunca ALTURA. Quem modula
//  relevo por regiao e ReliefScale, que varia suavemente.
//
//  Para um mundo plano: zere hillHeight e mountainHeight.
// ===========================================================================

// Um bioma e um PONTO no diagrama temperatura x umidade, com a paleta dele.
//
// A posicao no diagrama e o que permite misturar biomas de forma continua em
// vez de escolher um. Escolha discreta produz fronteira dura e mancha de cor
// chapada - a marca registrada de bioma gerado por if/else. Aqui cada ponto do
// mundo recebe uma media ponderada de TODOS os biomas, pesada pela distancia
// no diagrama, entao nao existe fronteira: existe gradiente.
// Um bioma e um PONTO no diagrama de Whittaker, em unidades FISICAS.
//
// Whittaker (1975) classifica os biomas terrestres por temperatura media anual
// e precipitacao media anual - e a classificacao padrao em ecologia. Usar as
// coordenadas reais, em vez de dois numeros inventados em [0,1], faz o mapa de
// biomas herdar as relacoes verdadeiras: nao existe floresta tropical a -5 C,
// nem tundra com 300 cm de chuva.
struct Biome {
    const char* name;
    float temperature;    // media anual, em graus Celsius
    float precipitation;  // media anual, em cm/ano
    Color lowland;        // vegetacao ou solo exposto da baixada
    Color rock;           // rocha exposta nas encostas
};

struct TerrainSettings {
    std::uint32_t seed = 1337u;

    // --- CHAO E MAR ------------------------------------------------------
    // A terra fica ACIMA do nivel do mar por padrao. E essa folga que faz a
    // agua aparecer so onde o terreno desce ate ela - rios, lagos nas
    // depressoes - em vez de alagar a paisagem inteira.
    // Convencao do Minecraft: nivel do mar em y=62, terreno logo acima, e
    // montanhas chegando a ~250. Com 1 unidade = 1 m isso da um mundo em que
    // um jogador de 1,8 m tem escala correta contra o relevo.
    float groundLevel = 78.0f;
    float seaLevel = 62.0f;

    // --- COLINAS ---------------------------------------------------------
    // Ondulacao de base, presente no mundo inteiro.
    //
    // A FREQUENCIA importa tanto quanto a amplitude: 13 unidades espalhadas por
    // 200 (escala 0.005) dao ~7% de inclinacao, que le como duna, nao como
    // colina. Encurtar o comprimento de onda e o que produz encosta de verdade.
    float hillHeight = 26.0f;
    float hillScale = 0.009f;
    int hillOctaves = 4;

    // --- MONTANHAS -------------------------------------------------------
    // Ruido ridged (cristas afiadas) elevado a mountainSharpness.
    float mountainHeight = 175.0f;
    float mountainScale = 0.0032f;
    int mountainOctaves = 4;
    // Expoente da crista. 2.0 achata os flancos e so o pico sobe, o que da
    // morros arredondados; 1.3 mantem a encosta inteira ingreme, que e o
    // perfil das montanhas do Minecraft.
    float mountainSharpness = 1.3f;

    // --- DETALHE FINO ----------------------------------------------------
    // A causa real do terreno parecer massa de modelar NAO era falta de
    // resolucao: e que a funcao de altura nao tinha conteudo de alta
    // frequencia. Com comprimento de onda minimo de ~90 m, nada abaixo disso
    // existia - e aumentar a resolucao do voxel nao inventa detalhe que a
    // funcao nao tem. Esta camada e o detalhe que faltava.
    float detailHeight = 3.2f;    // amplitude, em metros
    float detailScale = 0.06f;    // ~17 m de comprimento de onda
    int detailOctaves = 4;
    // O detalhe cresce com a inclinacao: rocha exposta e rugosa, campo e liso.
    float detailSlopeGain = 2.2f;

    // Ruido 3D aplicado direto na DENSIDADE, nao na altura. E o unico termo
    // capaz de criar saliencia e reentrancia - um heightfield, por definicao,
    // so tem uma altura por coluna e nunca produz beirada nem gruta.
    // CALIBRAGEM, nao chute: para a densidade deixar de ser monotona em y - que
    // e a condicao para existir saliencia - e preciso
    //     overhangStrength * d(ruido)/dy > 1
    // O gradiente do Perlin vale ~1,5 x frequencia, e a frequencia vertical
    // aqui e overhangScale * 1,6. Com escala 0,055 isso da 0,13, entao a forca
    // tem de passar de ~7,6. Com 2,6 e escala 0,021 (a primeira tentativa) o
    // produto dava 0,13 - sete vezes abaixo do limiar, e o terreno continuava
    // um heightfield puro apesar do ruido 3D estar la.
    //
    // Subir a FREQUENCIA em vez da amplitude e o que permite passar do limiar
    // sem deformar a silhueta da montanha.
    float overhangStrength = 15.0f;
    float overhangScale = 0.055f;

    // --- RIOS E VALES ----------------------------------------------------
    // O canal segue onde um ruido 2D cruza zero: como esse cruzamento e uma
    // curva continua e sinuosa, sai rio serpenteando sem nenhuma simulacao de
    // erosao. O vale e o mesmo campo com largura muito maior, rebaixando o
    // entorno em rampa - sem ele o rio seria uma valeta cortada em terreno
    // plano em vez de correr no fundo de um vale.
    float riverScale = 0.0016f;
    float riverWidth = 0.022f;   // largura do canal
    float valleyWidth = 0.150f;  // largura do vale ao redor
    float riverDepth = 9.0f;     // quanto o leito desce abaixo do nivel do mar
    float valleyDepth = 16.0f;   // quanto o vale rebaixa o entorno
    // Rios perdem forca onde ha montanha, senao cortariam canyons pelos picos.
    float riverMountainResistance = 0.75f;
    // Modula a largura ao longo do curso. Sem isto o rio tem espessura
    // constante do inicio ao fim, que e a marca mais obvia de "gerado por
    // formula" - rio de verdade alarga e estreita.
    float riverWidthVariation = 0.65f;
    float riverWidthScale = 0.008f;

    // COMPORTA DE ALTITUDE - nao remova.
    // O rio escava puxando a altura ate o leito, e esse puxao e proporcional a
    // DIFERENCA entre os dois. Numa montanha de 100 unidades a diferenca e
    // enorme, e mesmo um canal enfraquecido abria fendas verticais de dezenas
    // de unidades cortando o pico ao meio. Aqui o rio desaparece acima de
    // riverAltitudeLimit, que e o que faz sentido fisicamente: rio corre em
    // terra baixa, nao no alto da serra.
    // RELATIVO ao nivel do mar (o codigo soma seaLevel). Absoluto quebraria
    // toda vez que a escala do mundo mudasse.
    float riverAltitudeLimit = 22.0f;
    // Transicao LONGA. Curta demais, o rio termina num toco abrupto encravado
    // na encosta; longa, ele afina at sumir como cabeceira de riacho.
    float riverAltitudeFade = 55.0f;

    // Concavidade minima para haver rio. Este e o termo que resolve o
    // problema de fundo: um contorno de ruido nao sabe o que e "para baixo", e
    // por isso atravessava divisor de aguas e morro. Medindo se o terreno esta
    // localmente REBAIXADO em relacao a vizinhanca, o rio so aparece onde a
    // agua de fato se acumularia - e o fundo de vale ja desce sozinho, entao o
    // curso passa a acompanhar o relevo sem simular escoamento.
    float riverConcavityBias = 0.35f;
    float riverConcavitySample = 26.0f;  // distancia de amostragem, em unidades

    // --- RELEVO CONTINENTAL ----------------------------------------------
    // Campo CONTINUO em [0,1] que decide onde ha cordilheira e onde ha
    // planicie. Substitui o antigo multiplicador por bioma: como varia
    // suavemente, a montanha nasce e morre em rampa em vez de degrau.
    // A banda ESTREITA e o que separa planicie de cordilheira. Larga demais,
    // tudo vira terreno medio e a paisagem fica sem contraste.
    float reliefScale = 0.0018f;
    float reliefLow = 0.46f;   // abaixo disto: planicie
    float reliefHigh = 0.60f;  // acima disto: amplitude cheia

    // --- EROSAO ----------------------------------------------------------
    // Campo continuo que ACHATA o relevo onde e alto, criando os grandes
    // trechos planos que contrastam com as encostas. E o parametro que o
    // Minecraft 1.18 usa para o mesmo fim.
    float erosionScale = 0.0009f;
    float erosionStrength = 0.75f;

    // --- TERRACOS --------------------------------------------------------
    // Camadas horizontais de rocha, no estilo mesa/planalto estratificado.
    // DESLIGADO por padrao (terreno liso). Suba terraceStrength para 0.8 e a
    // paisagem vira platos em degraus.
    float terraceStep = 5.0f;       // altura de cada camada
    float terraceStrength = 0.0f;   // 0 = liso, 1 = degrau total
    float terraceStart = 100.0f;     // altura onde os degraus comecam
    float terraceFade = 14.0f;      // em quantas unidades entram por completo

    // --- CAVERNAS --------------------------------------------------------
    // Termo que EMPURRA a densidade para positivo (= abre vazio) onde o ruido
    // 3D forma tuneis. Zero desliga - e tambem faz o gerenciador de chunks
    // parar de gerar o subsolo inteiro, o que e bem mais barato.
    float caveStrength = 1.0f;
    float caveScale = 0.035f;
    float caveThreshold = 0.10f;
    float caveDepthBelowSurface = 8.0f;

    // --- CORES POR ALTITUDE ----------------------------------------------
    // Alturas RELATIVAS ao nivel do mar, com transicao suave entre faixas.
    //
    // A FAIXA DE AREIA TEM DE SER ESTREITA. Com beach=2.5 e grass=26, terreno
    // a 11 unidades caia em 36% do caminho areia->grama: ou seja, quase tudo
    // ficava cor de areia, e a neblina lavava isso ate o branco. A vegetacao
    // tem de comecar logo acima da linha d'agua, como na natureza.
    float beachHeight = 2.0f;
    float grassHeight = 9.0f;
    float rockHeight = 85.0f;
    float snowHeight = 150.0f;
    // Escurece faixas horizontais dentro da rocha, na mesma cadencia dos
    // terracos. E o detalhe que faz a parede parecer estratificada.
    float strataContrast = 0.16f;

    // --- CLIMA -----------------------------------------------------------
    // O clima e derivado do RELEVO, nao sorteado independente dele. Tres
    // mecanismos reais, cada um com a constante fisica correspondente.

    // EXAGERO VERTICAL DO CLIMA - decisao consciente, nao erro de escala.
    //
    // O mundo usa 1 unidade = 1 METRO, para o jogador (1,8 un) e as cavernas
    // terem tamanho coerente. Mas com montanhas de ~250 m, o lapse rate real
    // de 6,5 C/km daria 1,6 C no cume: neve nenhuma, tundra nenhuma.
    //
    // Na Terra a linha de neve dos Alpes fica em 2500-3000 m. Para ter cume
    // nevado num mundo de 250 m e preciso comprimir a vertical - que e
    // exatamente o que o Minecraft faz de forma implicita, colocando neve por
    // volta de y=160. Aqui isso fica EXPLICITO: para efeito de clima, cada
    // unidade de altitude conta como `climateVerticalExaggeration` metros.
    // A geometria, a colisao e o jogador continuam em metros de verdade.
    float climateVerticalExaggeration = 12.0f;

    // GRADIENTE TERMICO VERTICAL. O ar esfria ~6,5 C por 1000 m de altitude
    // (environmental lapse rate). E o motivo fisico de haver neve e tundra no
    // alto de montanha mesmo no tropico - e, no gerador anterior, o motivo de
    // as montanhas ficarem verdes ate o cume.
    float lapseRatePerKm = 6.5f;

    // Temperatura ao nivel do mar: media e variacao regional.
    float seaLevelTemperature = 22.0f;  // C
    float temperatureVariation = 14.0f;  // C de amplitude regional
    float temperatureScale = 0.0007f;

    // Precipitacao de base, antes do efeito orografico.
    float basePrecipitation = 110.0f;  // cm/ano
    float precipitationVariation = 90.0f;
    float humidityScale = 0.0011f;

    // EFEITO OROGRAFICO. Ar umido forcado a subir uma serra se resfria,
    // condensa e chove na encosta a barlavento; do outro lado desce ja seco -
    // a sombra de chuva. E o mecanismo que poe deserto ao lado de floresta sem
    // precisar de nenhum ruido extra.
    float windDirectionX = 0.86f;  // vetor do vento dominante
    float windDirectionZ = 0.51f;
    float orographicDistance = 260.0f;  // ate onde procurar a serra a barlavento
    float orographicStrength = 0.85f;   // 1 = sombra total atras da serra
    float windwardGain = 90.0f;         // cm/ano extra na encosta que sobe

    // DOMAIN WARPING: deforma as coordenadas ANTES de amostrar temperatura e
    // umidade. Sem isto, as manchas de bioma sao as curvas de nivel de um fbm
    // - bolhas lisas e arredondadas, que o olho reconhece na hora como ruido.
    // Com o warp elas ganham reentrancias, peninsulas e ilhas.
    float biomeWarpStrength = 320.0f;
    float biomeWarpScale = 0.0016f;

    // MOSQUEADO: variacao de alta frequencia DENTRO do mesmo bioma, o que na
    // referencia aerea aparece como manchas de mata mais escura no meio do
    // pasto. Sem isto cada bioma e um campo de cor perfeitamente uniforme.
    float biomeMottleStrength = 0.40f;
    float biomeMottleScale = 0.020f;

    // Concentracao da mistura entre biomas. Baixo demais e tudo vira uma media
    // acinzentada; alto demais volta a parecer escolha discreta, com
    // fronteira visivel.
    float biomeBlendSharpness = 2.6f;
};

class TerrainGenerator {
public:
    explicit TerrainGenerator(TerrainSettings settings = {});

    // ---- as equacoes ----------------------------------------------------

    // Altura da superficie em (x, z), ja terracada.
    float SurfaceHeight(float x, float z) const;

    // Densidade num ponto, dado o resultado de SurfaceHeight naquela coluna.
    // Separado de SurfaceHeight de proposito: a altura e 2D e cara, a
    // densidade e 3D e barata. Ver SampleGrid::SampleColumns.
    float DensityAt(Vector3 p, float surfaceHeight) const;

    // Versao autocontida, satisfaz o concept ScalarField. Mais lenta
    // (recalcula a altura por ponto); util para consultas avulsas.
    float Sample(Vector3 p) const {
        return DensityAt(p, SurfaceHeight(p.x, p.z));
    }

    // Bioma DOMINANTE em (x, z). Serve para rotular no HUD; a cor NAO sai
    // daqui, porque uma escolha discreta produziria fronteira visivel.
    const Biome& PickBiome(float x, float z) const;

    // Clima num ponto. Publico porque e a consulta que valida a fisica do
    // gerador e que o posicionamento de estruturas vai precisar.
    void ClimateAt(float x, float z, float altitude, float& temperature,
                   float& precipitation) const {
        Climate(x, z, altitude, temperature, precipitation);
    }

    // Paleta ja misturada entre todos os biomas, continua em (x, z).
    struct BiomePalette {
        Color lowland;
        Color rock;
    };
    BiomePalette BlendBiomes(float x, float z, float altitude) const;

    // Cor de um vertice ja gerado. Roda por vertice, nao por ponto de grade.
    Color SurfaceColor(Vector3 position, Vector3 normal) const;

    // ---- suporte --------------------------------------------------------

    void HeightBounds(float& minHeight, float& maxHeight) const;
    bool HasCaves() const { return settings_.caveStrength > 0.0f; }

    const TerrainSettings& Settings() const { return settings_; }
    void SetSettings(const TerrainSettings& settings);

private:
    // Fator continuo de relevo em [0,1]. Ver o comentario de reliefScale.
    float ReliefScale(float x, float z) const;

    // Quanto achatar o relevo aqui, em [0,1]. Ver o comentario de erosionScale.
    float Erosion(float x, float z) const;

    // Temperatura e umidade em [0,1], ja com as coordenadas deformadas pelo
    // domain warp. Ponto unico de leitura do clima: PickBiome e BlendBiomes
    // tem de concordar, senao o rotulo do HUD nao bate com a cor da tela.
    // Clima derivado do relevo. `altitude` entra pelo lapse rate.
    void Climate(float x, float z, float altitude, float& temperature,
                 float& precipitation) const;

    // Relevo grosseiro (2 oitavas), so para o calculo orografico.
    float CoarseHeight(float x, float z) const;

    // Proximidade do eixo do rio, em [0,1]: 1 no centro do canal, 0 fora.
    // `valley` sai com a largura do vale, `channel` com a do leito.
    void RiverFactors(float x, float z, float gate, float& valley,
                      float& channel) const;

    // Relevo SEM os rios. Existe separado porque a concavidade precisa medir a
    // vizinhanca, e usar SurfaceHeight ali seria recursao infinita.
    float BaseHeight(float x, float z) const;

    // Quanto o terreno esta rebaixado em relacao a vizinhanca. Positivo em
    // fundo de vale, negativo em crista. E o substituto local do calculo de
    // escoamento: onde a agua se acumularia.
    float Concavity(float x, float z) const;

    // Quantiza a altura em camadas, so acima de terraceStart - a baixada
    // continua lisa.
    float ApplyTerraces(float height) const;

    TerrainSettings settings_;
    Noise noise_;
};

static_assert(field::ScalarField<TerrainGenerator>);

}  // namespace world
