#include "world/Chunk.hpp"

namespace world {

Chunk::Chunk(ChunkCoord coord, int lod) : coord_(coord), lod_(lod) {}

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
    return BoundingBox{min, Vector3{min.x + kChunkSize, min.y + kChunkSize,
                                    min.z + kChunkSize}};
}

void Chunk::Upload(const mc::MeshData& mesh, int triangles) {
    if (triangles <= 0 || mesh.VertexCount() == 0) return;

    // Capacidade exata: o terreno so muda quando o chunk e regerado do zero,
    // entao nao ha motivo para reservar folga.
    mesh_.emplace(static_cast<int>(mesh.VertexCount()));
    mesh_->Update(mesh);
    triangleCount_ = triangles;
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
