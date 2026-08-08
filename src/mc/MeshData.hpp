#pragma once

#include <cstddef>
#include <vector>

#include "raylib.h"

namespace mc {

// Sopa de triangulos nao indexada: cada tres vertices consecutivos formam um
// triangulo, sem reuso.
//
// Nao usar indices e deliberado. O campo `Mesh::indices` da raylib e
// `unsigned short*`, teto de 65535 vertices - uma grade 48^3 de metaballs
// passa disso com folga e o overflow silencioso vira lixo geometrico.
// Vertices duplicados custam memoria, mas removem o limite e dispensam a
// soldagem de vertices.
struct MeshData {
    std::vector<Vector3> positions;
    std::vector<Vector3> normals;

    // Cor por vertice. O poligonizador NAO preenche isto - ele so conhece
    // geometria. Quem gera o terreno pinta depois, num passe sobre os
    // vertices, e e assim que o bioma vira cor sem o marching cubes precisar
    // saber o que e um bioma. Vazio significa "sem cor" (a malha sai branca).
    std::vector<Color> colors;

    void Clear() {
        positions.clear();
        normals.clear();
        colors.clear();
    }

    // Mantem a capacidade entre frames: a partir do segundo frame o
    // poligonizador nao realoca mais nada.
    void Reserve(std::size_t vertices) {
        positions.reserve(vertices);
        normals.reserve(vertices);
        colors.reserve(vertices);
    }

    void PushVertex(Vector3 position, Vector3 normal) {
        positions.push_back(position);
        normals.push_back(normal);
    }

    bool HasColors() const { return colors.size() == positions.size(); }

    std::size_t VertexCount() const { return positions.size(); }
    std::size_t TriangleCount() const { return positions.size() / 3; }
};

}  // namespace mc
