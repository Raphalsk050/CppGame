#include "world/TerrainEdits.hpp"

#include <algorithm>

namespace world {
namespace {

int FloorDiv(float value, float divisor) {
    return static_cast<int>(std::floor(value / divisor));
}

}  // namespace

std::vector<ChunkCoord> TerrainEdits::Add(const SphereEdit& edit) {
    std::vector<ChunkCoord> affected;

    // Caixa da esfera em coordenadas de chunk. A folga de uma celula cobre o
    // padding de amostragem: um chunk cuja borda encosta na esfera ainda
    // precisa dela para o gradiente sair certo.
    const float pad = edit.radius + kVoxelSize;
    const int minX = FloorDiv(edit.center.x - pad, kChunkSize);
    const int maxX = FloorDiv(edit.center.x + pad, kChunkSize);
    const int minY = FloorDiv(edit.center.y - pad, kChunkSize);
    const int maxY = FloorDiv(edit.center.y + pad, kChunkSize);
    const int minZ = FloorDiv(edit.center.z - pad, kChunkSize);
    const int maxZ = FloorDiv(edit.center.z + pad, kChunkSize);

    for (int z = minZ; z <= maxZ; ++z) {
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const ChunkCoord coord{x, y, z};
                byChunk_[coord].push_back(edit);
                affected.push_back(coord);
            }
        }
    }

    ++count_;
    return affected;
}

float TerrainEdits::DeltaAt(Vector3 p) const {
    const ChunkCoord coord{FloorDiv(p.x, kChunkSize), FloorDiv(p.y, kChunkSize),
                           FloorDiv(p.z, kChunkSize)};
    const auto it = byChunk_.find(coord);
    if (it == byChunk_.end()) return 0.0f;

    float delta = 0.0f;
    for (const SphereEdit& edit : it->second) {
        delta += EditContribution(edit, p);
    }
    return delta;
}

const std::vector<SphereEdit>* TerrainEdits::ForChunk(ChunkCoord coord) const {
    const auto it = byChunk_.find(coord);
    return (it == byChunk_.end()) ? nullptr : &it->second;
}

void TerrainEdits::Clear() {
    byChunk_.clear();
    count_ = 0;
}

}  // namespace world
