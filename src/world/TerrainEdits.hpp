#pragma once

#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <vector>

#include "raylib.h"
#include "world/Chunk.hpp"

namespace world {

// Uma escavacao (ou deposito) esferica no terreno.
//
// Com marching cubes editar terreno e trivial: como a superficie e so o zero
// de um campo escalar, cavar e SOMAR densidade positiva numa regiao - o campo
// e a malha se reorganizam sozinhos, sem nenhum tratamento especial de
// topologia. Um buraco atravessando uma parede, um tunel que encontra uma
// caverna natural, uma ponte que se parte em dois: tudo sai do mesmo somatorio.
struct SphereEdit {
    Vector3 center;
    float radius;
    float strength;  // > 0 escava (empurra para o ar), < 0 preenche
};

// Contribuicao de uma edicao na densidade de um ponto. A queda quadratica ate
// a borda evita que o buraco fique com quina - a superficie nova encontra a
// antiga com derivada continua.
inline float EditContribution(const SphereEdit& edit, Vector3 p) {
    const float dx = p.x - edit.center.x;
    const float dy = p.y - edit.center.y;
    const float dz = p.z - edit.center.z;
    const float distanceSq = dx * dx + dy * dy + dz * dz;
    const float radiusSq = edit.radius * edit.radius;
    if (distanceSq >= radiusSq) return 0.0f;

    const float t = 1.0f - std::sqrt(distanceSq) / edit.radius;
    return edit.strength * t * t;
}

// Acervo de edicoes do jogador, indexado por chunk.
//
// Guardar a LISTA de edicoes em vez do volume de densidade editado e o que
// mantem o custo baixo: um chunk editado continua sem armazenar seus ~150 KB
// de escalares, e a densidade e sempre recomputada como
// "terreno procedural + soma das edicoes que alcancam o ponto".
class TerrainEdits {
public:
    // Registra a edicao e devolve os chunks que precisam ser remalhados.
    std::vector<ChunkCoord> Add(const SphereEdit& edit);

    // Contribuicao acumulada num ponto. Como cada edicao e inserida em TODOS
    // os chunks que sua esfera toca, basta consultar o balde do chunk do
    // ponto - nao ha risco de perder uma esfera que vem do chunk vizinho.
    float DeltaAt(Vector3 p) const;

    // Edicoes que alcancam um chunk. nullptr se nenhuma.
    const std::vector<SphereEdit>* ForChunk(ChunkCoord coord) const;

    std::size_t Count() const { return count_; }
    void Clear();

private:
    std::unordered_map<ChunkCoord, std::vector<SphereEdit>, ChunkCoordHash>
        byChunk_;
    std::size_t count_ = 0;
};

}  // namespace world
