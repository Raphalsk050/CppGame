#include "world/Chunk.hpp"

#include "mc/Polygonizer.hpp"
#include "world/TerrainEdits.hpp"

namespace world {
namespace {

constexpr float kIsoLevel = 0.0f;

}  // namespace

Chunk::Chunk(ChunkCoord coord) : coord_(coord) {}

Vector3 Chunk::WorldMin() const {
    return {
        static_cast<float>(coord_.x) * kChunkSize,
        static_cast<float>(coord_.y) * kChunkSize,
        static_cast<float>(coord_.z) * kChunkSize,
    };
}

Vector3 Chunk::Center() const {
    const Vector3 min = WorldMin();
    return {min.x + kChunkSize * 0.5f, min.y + kChunkSize * 0.5f,
            min.z + kChunkSize * 0.5f};
}

BoundingBox Chunk::Bounds() const {
    const Vector3 min = WorldMin();
    return BoundingBox{
        min, Vector3{min.x + kChunkSize, min.y + kChunkSize,
                     min.z + kChunkSize}};
}

void Chunk::Generate(const TerrainGenerator& generator, mc::SampleGrid& grid,
                     mc::MeshData& scratch,
                     const std::vector<SphereEdit>* edits) {
    mesh_.reset();
    triangleCount_ = 0;

    const Vector3 min = WorldMin();
    const float pad = kChunkPadding * kVoxelSize;
    const bool hasEdits = (edits != nullptr && !edits->empty());

    // ---- descarte barato ------------------------------------------------
    // Um chunk inteiramente acima da maior altura possivel e so ar; um
    // inteiramente abaixo da menor e solido macico. Nos dois casos o marching
    // cubes produziria zero triangulos, entao nem vale amostrar. Sem isto, um
    // mundo plano gastaria o mesmo tempo nos chunks do ceu e do subsolo que na
    // camada da superficie.
    //
    // Um chunk COM EDICOES nunca e descartado: cavar dentro de solido macico
    // cria superficie exatamente onde o teste diria que nao ha nenhuma.
    if (!hasEdits) {
        float minHeight = 0.0f;
        float maxHeight = 0.0f;
        generator.HeightBounds(minHeight, maxHeight);

        const float chunkTop = min.y + kChunkSize;
        if (min.y > maxHeight) return;  // tudo ar
        // Com cavernas ligadas o subsolo deixa de ser macico e precisa ser
        // gerado.
        if (chunkTop < minHeight && !generator.HasCaves()) return;  // solido
    }

    // ---- amostragem ------------------------------------------------------
    grid.SetBounds(
        Vector3{min.x - pad, min.y - pad, min.z - pad},
        Vector3{min.x + kChunkSize + pad, min.y + kChunkSize + pad,
                min.z + kChunkSize + pad});

    // A altura da superficie e 2D e cara; a densidade e 3D e barata. Calcular
    // a altura uma vez por coluna evita repeti-la para cada ponto vertical.
    grid.SampleColumns(
        [&generator](float x, float z) {
            return generator.SurfaceHeight(x, z);
        },
        [&generator, edits](float surfaceHeight, Vector3 p) {
            float density = generator.DensityAt(p, surfaceHeight);
            // As escavacoes entram como mais um termo do somatorio. E so isso:
            // o marching cubes reencontra a superficie sozinho.
            if (edits != nullptr) {
                for (const SphereEdit& edit : *edits) {
                    density += EditContribution(edit, p);
                }
            }
            return density;
        });

    // ---- malha -----------------------------------------------------------
    const mc::PolygonizeStats stats =
        mc::Polygonize(grid, kIsoLevel, scratch, kChunkPadding);

    if (stats.triangles == 0) return;

    // Cor por vertice: e aqui que o bioma vira pixel. Fica fora do
    // poligonizador de proposito - ele so conhece geometria.
    scratch.colors.resize(scratch.positions.size());
    for (std::size_t i = 0; i < scratch.positions.size(); ++i) {
        scratch.colors[i] =
            generator.SurfaceColor(scratch.positions[i], scratch.normals[i]);
    }

    // Capacidade exata: o terreno nao muda depois de gerado, entao nao ha
    // motivo para reservar folga.
    mesh_.emplace(static_cast<int>(scratch.VertexCount()));
    mesh_->Update(scratch);
    triangleCount_ = stats.triangles;
}

void Chunk::SetShader(Shader shader) {
    if (mesh_) mesh_->SetShader(shader);
}

void Chunk::Draw(Color tint) const {
    if (mesh_) mesh_->Draw(Vector3{0.0f, 0.0f, 0.0f}, tint);
}

void Chunk::DrawWires(Color tint) const {
    if (mesh_) mesh_->DrawWires(Vector3{0.0f, 0.0f, 0.0f}, tint);
}

}  // namespace world
