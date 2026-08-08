#pragma once

#include <array>
#include <cstdint>

namespace world {

// Perlin gradient noise (versao "improved", 2002) com tabela de permutacao
// semeada, mais os combinadores usuais.
//
// Implementado aqui em vez de vir de biblioteca porque as equacoes de terreno
// sao o ponto que voce vai editar, e elas ficam ilegiveis se o ruido for uma
// caixa-preta. Tudo abaixo tem periodo 256 no espaco de entrada - com as
// escalas tipicas (0.005 a 0.1) isso da centenas de milhares de unidades de
// mundo antes de repetir.
class Noise {
public:
    explicit Noise(std::uint32_t seed = 1337u);

    // Ruido base. Saida aproximadamente em [-1, 1], media zero.
    float Perlin3(float x, float y, float z) const;
    float Perlin2(float x, float y) const { return Perlin3(x, 0.5f, y); }

    // Soma de oitavas: cada uma com o dobro da frequencia e metade da
    // amplitude. E o que transforma ruido liso em relevo com detalhe em
    // varias escalas. Saida normalizada para ~[-1, 1].
    float Fbm3(float x, float y, float z, int octaves,
               float lacunarity = 2.0f, float gain = 0.5f) const;
    float Fbm2(float x, float y, int octaves, float lacunarity = 2.0f,
               float gain = 0.5f) const;

    // Ruido "dobrado": 1 - |perlin|, acumulado em oitavas. Cria cristas
    // afiadas em vez de ondulacoes suaves - e a forma classica de montanha e
    // de cordilheira. Saida em ~[0, 1].
    float Ridged2(float x, float y, int octaves, float lacunarity = 2.0f,
                  float gain = 0.5f) const;

    // Mesma ideia em 3D, usada para tuneis de caverna.
    float Ridged3(float x, float y, float z, int octaves,
                  float lacunarity = 2.0f, float gain = 0.5f) const;

    std::uint32_t Seed() const { return seed_; }

private:
    std::uint32_t seed_;
    std::array<int, 512> perm_{};  // tabela duplicada, evita mod na indexacao
};

}  // namespace world
