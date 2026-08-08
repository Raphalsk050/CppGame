#pragma once

#include <cstddef>
#include <optional>

#include "mc/MeshData.hpp"
#include "raylib.h"
#include "render/DynamicMesh.hpp"

namespace world {

// Lado do chunk em celulas e em unidades de mundo.
//
// kVoxelSize e o controle de RESOLUCAO: subir para 2.0 quadruplica a area da
// celula e corta os triangulos na mesma proporcao. kChunkCells e o controle de
// GRANULARIDADE do streaming: baixa-lo faz chunks menores com o MESMO detalhe,
// o que so aumenta o numero de VAOs e draw calls.
inline constexpr int kChunkCells = 32;
inline constexpr float kVoxelSize = 1.0f;
inline constexpr float kChunkSize = kChunkCells * kVoxelSize;

// A grade de amostragem leva uma celula de folga em cada face. Ver o comentario
// de `margin` em mc/Polygonizer.hpp: sem essa folga, o gradiente na fronteira
// cairia em diferenca lateral e os dois chunks vizinhos calculariam normais
// diferentes para o mesmo vertice - emenda visivel em cada borda.
inline constexpr int kChunkPadding = 1;
inline constexpr int kChunkGridResolution = kChunkCells + 2 * kChunkPadding;

struct ChunkCoord {
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const ChunkCoord&) const = default;
};

struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord& c) const noexcept {
        // Mistura multiplicativa com primos grandes; chunks vizinhos precisam
        // cair em baldes bem separados.
        std::size_t h = static_cast<std::size_t>(c.x) * 73856093u;
        h ^= static_cast<std::size_t>(c.y) * 19349663u;
        h ^= static_cast<std::size_t>(c.z) * 83492791u;
        return h;
    }
};

// Um chunk e apenas o RECURSO DE GPU do pedaco de mundo.
//
// Toda a geracao - amostrar o campo, marching cubes, cor por vertice - mora no
// ChunkBuilder e roda em worker. A separacao existe porque o contexto OpenGL
// pertence a uma unica thread: o que sobrou aqui e exatamente o que so a
// thread principal pode fazer.
class Chunk {
public:
    explicit Chunk(ChunkCoord coord);

    // Sobe geometria ja pronta para a GPU. THREAD PRINCIPAL apenas.
    void Upload(const mc::MeshData& mesh, int triangles);

    void Draw(Color tint) const;
    void DrawWires(Color tint) const;
    void SetShader(Shader shader);

    ChunkCoord Coord() const { return coord_; }
    Vector3 WorldMin() const;
    Vector3 Center() const;
    BoundingBox Bounds() const;

    bool HasGeometry() const { return mesh_.has_value(); }
    int TriangleCount() const { return triangleCount_; }

private:
    ChunkCoord coord_;
    std::optional<render::DynamicMesh> mesh_;
    int triangleCount_ = 0;
};

}  // namespace world
