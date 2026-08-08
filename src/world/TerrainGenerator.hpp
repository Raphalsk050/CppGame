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

// Um bioma define so paleta. A influencia dele no relevo foi removida de
// proposito - ver a regra acima.
struct Biome {
    const char* name;
    Color lowland;  // vegetacao ou areia da baixada
    Color rock;     // rocha exposta nas encostas
};

struct TerrainSettings {
    std::uint32_t seed = 1337u;

    // --- CHAO E MAR ------------------------------------------------------
    // A terra fica ACIMA do nivel do mar por padrao. E essa folga que faz a
    // agua aparecer so onde o terreno desce ate ela - rios, lagos nas
    // depressoes - em vez de alagar a paisagem inteira.
    float groundLevel = 11.0f;
    float seaLevel = 0.0f;

    // --- COLINAS ---------------------------------------------------------
    // Ondulacao de base, presente no mundo inteiro.
    float hillHeight = 13.0f;
    float hillScale = 0.005f;
    int hillOctaves = 4;

    // --- MONTANHAS -------------------------------------------------------
    // Ruido ridged (cristas afiadas) elevado ao quadrado: achata os vales e
    // deixa so as cristas subirem.
    float mountainHeight = 62.0f;
    float mountainScale = 0.0018f;
    int mountainOctaves = 5;

    // --- RIOS E VALES ----------------------------------------------------
    // O canal segue onde um ruido 2D cruza zero: como esse cruzamento e uma
    // curva continua e sinuosa, sai rio serpenteando sem nenhuma simulacao de
    // erosao. O vale e o mesmo campo com largura muito maior, rebaixando o
    // entorno em rampa - sem ele o rio seria uma valeta cortada em terreno
    // plano em vez de correr no fundo de um vale.
    float riverScale = 0.0016f;
    float riverWidth = 0.030f;   // largura do canal
    float valleyWidth = 0.190f;  // largura do vale ao redor
    float riverDepth = 9.0f;     // quanto o leito desce abaixo do nivel do mar
    float valleyDepth = 9.0f;    // quanto o vale rebaixa o entorno
    // Rios perdem forca onde ha montanha, senao cortariam canyons pelos picos.
    float riverMountainResistance = 0.75f;

    // --- RELEVO CONTINENTAL ----------------------------------------------
    // Campo CONTINUO em [0,1] que decide onde ha cordilheira e onde ha
    // planicie. Substitui o antigo multiplicador por bioma: como varia
    // suavemente, a montanha nasce e morre em rampa em vez de degrau.
    float reliefScale = 0.0011f;
    float reliefLow = 0.42f;   // abaixo disto: planicie
    float reliefHigh = 0.78f;  // acima disto: amplitude cheia

    // --- TERRACOS --------------------------------------------------------
    // Camadas horizontais de rocha, no estilo mesa/planalto estratificado.
    // DESLIGADO por padrao (terreno liso). Suba terraceStrength para 0.8 e a
    // paisagem vira platos em degraus.
    float terraceStep = 5.0f;       // altura de cada camada
    float terraceStrength = 0.0f;   // 0 = liso, 1 = degrau total
    float terraceStart = 20.0f;     // altura onde os degraus comecam
    float terraceFade = 14.0f;      // em quantas unidades entram por completo

    // --- CAVERNAS --------------------------------------------------------
    // Termo que EMPURRA a densidade para positivo (= abre vazio) onde o ruido
    // 3D forma tuneis. Zero desliga - e tambem faz o gerenciador de chunks
    // parar de gerar o subsolo inteiro, o que e bem mais barato.
    float caveStrength = 1.0f;
    float caveScale = 0.035f;
    float caveThreshold = 0.10f;
    float caveDepthBelowSurface = 6.0f;

    // --- CORES POR ALTITUDE ----------------------------------------------
    // Alturas RELATIVAS ao nivel do mar, com transicao suave entre faixas. E
    // daqui que vem a leitura de altitude da paisagem: areia, vegetacao,
    // rocha, neve.
    float beachHeight = 2.5f;
    float grassHeight = 26.0f;
    float rockHeight = 58.0f;
    // Escurece faixas horizontais dentro da rocha, na mesma cadencia dos
    // terracos. E o detalhe que faz a parede parecer estratificada.
    float strataContrast = 0.16f;

    // --- BIOMAS ----------------------------------------------------------
    float temperatureScale = 0.0009f;
    float humidityScale = 0.0013f;
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

    const Biome& PickBiome(float x, float z) const;

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

    // Proximidade do eixo do rio, em [0,1]: 1 no centro do canal, 0 fora.
    // `valley` sai com a largura do vale, `channel` com a do leito.
    void RiverFactors(float x, float z, float& valley, float& channel) const;

    // Quantiza a altura em camadas, so acima de terraceStart - a baixada
    // continua lisa.
    float ApplyTerraces(float height) const;

    TerrainSettings settings_;
    Noise noise_;
};

static_assert(field::ScalarField<TerrainGenerator>);

}  // namespace world
