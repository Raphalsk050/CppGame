#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "mc/MeshData.hpp"
#include "mc/SampleGrid.hpp"
#include "raylib.h"
#include "render/DynamicMesh.hpp"
#include "world/TerrainGenerator.hpp"

namespace world {

// Lado do chunk em celulas e em unidades de mundo (voxel de 1 unidade).
inline constexpr int kChunkCells = 32;
inline constexpr float kVoxelSize = 1.0f;
inline constexpr float kChunkSize = kChunkCells * kVoxelSize;

// A grade de amostragem leva uma celula de folga em cada face. Ver o comentario
// de `margin` em mc/Polygonizer.hpp: sem essa folga, o gradiente na fronteira
// cairia em diferenca lateral e os dois chunks vizinhos calculariam normais
// diferentes para o mesmo vertice - emenda visivel em cada borda.
inline constexpr int kChunkPadding = 1;
inline constexpr int kChunkGridResolution = kChunkCells + 2 * kChunkPadding;

// Definido em world/TerrainEdits.hpp. Declarado aqui em vez de incluido
// porque aquele cabecalho precisa de ChunkCoord: incluir nos dois sentidos
// seria ciclo. Como so aparece atras de ponteiro, a declaracao basta.
struct SphereEdit;

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

class Chunk {
public:
    explicit Chunk(ChunkCoord coord);

    // Gera densidade e malha. `grid` e `scratch` sao buffers de rascunho
    // compartilhados por TODOS os chunks: a densidade nao precisa sobreviver a
    // malha, entao guardar uma grade por chunk desperdicaria centenas de MB.
    //
    // `edits` sao as escavacoes do jogador que alcancam este chunk (nullptr se
    // nenhuma). Como a densidade e sempre recomputada do zero, regerar um
    // chunk editado e o mesmo caminho de codigo de gerar um virgem.
    void Generate(const TerrainGenerator& generator, mc::SampleGrid& grid,
                  mc::MeshData& scratch,
                  const std::vector<SphereEdit>* edits = nullptr);

    void Draw(Color tint) const;
    void DrawWires(Color tint) const;

    // Sem efeito se o chunk nao gerou geometria.
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
