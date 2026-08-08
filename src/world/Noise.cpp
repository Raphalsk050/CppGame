#include "world/Noise.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace world {
namespace {

// Curva de suavizacao de Perlin: 6t^5 - 15t^4 + 10t^3. Primeira e segunda
// derivadas zero nas pontas, que e o que evita a descontinuidade visivel nas
// fronteiras da grade do ruido.
inline float Fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

inline float Lerp(float a, float b, float t) { return a + t * (b - a); }

// Produto escalar entre o vetor-distancia e um dos 12 gradientes de aresta do
// cubo, escolhido pelos 4 bits baixos do hash.
inline float Grad(int hash, float x, float y, float z) {
    const int h = hash & 15;
    const float u = (h < 8) ? x : y;
    const float v = (h < 4) ? y : ((h == 12 || h == 14) ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

}  // namespace

Noise::Noise(std::uint32_t seed) : seed_(seed) {
    std::array<int, 256> base{};
    std::iota(base.begin(), base.end(), 0);

    // mt19937 com seed explicita: o mundo tem que sair identico a cada
    // execucao para a mesma seed.
    std::mt19937 rng(seed);
    std::shuffle(base.begin(), base.end(), rng);

    for (int i = 0; i < 256; ++i) {
        perm_[i] = base[i];
        perm_[i + 256] = base[i];
    }
}

float Noise::Perlin3(float x, float y, float z) const {
    const float fx = std::floor(x);
    const float fy = std::floor(y);
    const float fz = std::floor(z);

    // Celula da grade (& 255 fecha o periodo em 256).
    const int X = static_cast<int>(fx) & 255;
    const int Y = static_cast<int>(fy) & 255;
    const int Z = static_cast<int>(fz) & 255;

    // Posicao relativa dentro da celula.
    x -= fx;
    y -= fy;
    z -= fz;

    const float u = Fade(x);
    const float v = Fade(y);
    const float w = Fade(z);

    const int A = perm_[X] + Y;
    const int AA = perm_[A] + Z;
    const int AB = perm_[A + 1] + Z;
    const int B = perm_[X + 1] + Y;
    const int BA = perm_[B] + Z;
    const int BB = perm_[B + 1] + Z;

    // Interpola os 8 cantos: primeiro em x, depois y, depois z.
    return Lerp(
        Lerp(Lerp(Grad(perm_[AA], x, y, z), Grad(perm_[BA], x - 1, y, z), u),
             Lerp(Grad(perm_[AB], x, y - 1, z),
                  Grad(perm_[BB], x - 1, y - 1, z), u),
             v),
        Lerp(Lerp(Grad(perm_[AA + 1], x, y, z - 1),
                  Grad(perm_[BA + 1], x - 1, y, z - 1), u),
             Lerp(Grad(perm_[AB + 1], x, y - 1, z - 1),
                  Grad(perm_[BB + 1], x - 1, y - 1, z - 1), u),
             v),
        w);
}

float Noise::Fbm3(float x, float y, float z, int octaves, float lacunarity,
                  float gain) const {
    float sum = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float total = 0.0f;  // soma das amplitudes, para normalizar no fim

    for (int i = 0; i < octaves; ++i) {
        sum += amplitude *
               Perlin3(x * frequency, y * frequency, z * frequency);
        total += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }

    return (total > 0.0f) ? sum / total : 0.0f;
}

float Noise::Fbm2(float x, float y, int octaves, float lacunarity,
                  float gain) const {
    return Fbm3(x, 0.5f, y, octaves, lacunarity, gain);
}

float Noise::Ridged3(float x, float y, float z, int octaves, float lacunarity,
                     float gain) const {
    float sum = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float total = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        // 1 - |n| poe o valor maximo exatamente onde o ruido cruza zero.
        // Como esse cruzamento e uma superficie fina, o resultado sao cristas
        // estreitas em vez de topos arredondados.
        const float n = Perlin3(x * frequency, y * frequency, z * frequency);
        sum += amplitude * (1.0f - std::fabs(n));
        total += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }

    return (total > 0.0f) ? sum / total : 0.0f;
}

float Noise::Ridged2(float x, float y, int octaves, float lacunarity,
                     float gain) const {
    return Ridged3(x, 0.5f, y, octaves, lacunarity, gain);
}

}  // namespace world
