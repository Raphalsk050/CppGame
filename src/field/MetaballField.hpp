#pragma once

#include <vector>

#include "field/ScalarField.hpp"
#include "raylib.h"
#include "raymath.h"

namespace field {

// Metaballs: soma de nucleos 1/r^2, o caso classico onde marching cubes ganha
// de geometria explicita - as bolas se fundem e se separam sozinhas, sem
// nenhum tratamento especial de topologia.
//
// O somatorio cresce PARA DENTRO, o contrario da convencao do projeto, entao
// Sample devolve `1 - soma`. Sem essa negacao o indice de caso sai invertido,
// os triangulos saem com winding ao contrario e o backface culling engole a
// malha - o bug classico de quem implementa metaballs pela primeira vez.
class MetaballField {
public:
    explicit MetaballField(int count = 6);

    // Reposiciona as bolas em curvas de Lissajous. `time` em segundos.
    void Update(float time);

    // Definido inline de proposito: e chamado uma vez por ponto de grade
    // (~118 mil em 48^3) dentro de um laco templated, e precisa inlinar.
    float Sample(Vector3 p) const {
        float sum = 0.0f;
        for (const Ball& b : balls_) {
            const float dx = p.x - b.center.x;
            const float dy = p.y - b.center.y;
            const float dz = p.z - b.center.z;
            // O epsilon evita a singularidade exata no centro da bola.
            sum += b.strength / (dx * dx + dy * dy + dz * dz + 1e-4f);
        }
        return 1.0f - sum;
    }

    int BallCount() const { return static_cast<int>(balls_.size()); }
    void SetBallCount(int count);

private:
    struct Ball {
        Vector3 center;
        float strength;
        Vector3 frequency;  // frequencia de cada eixo na curva de Lissajous
        Vector3 phase;
        float orbit;  // amplitude da orbita
    };

    void Rebuild(int count);

    std::vector<Ball> balls_;
};

static_assert(ScalarField<MetaballField>);

}  // namespace field
