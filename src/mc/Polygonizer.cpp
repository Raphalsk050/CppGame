#include "mc/Polygonizer.hpp"

#include <algorithm>
#include <cmath>

#include "mc/Tables.hpp"
#include "raymath.h"

namespace mc {
namespace {

// Ponto onde a aresta cruza o isolevel, por interpolacao linear dos valores
// nos dois cantos. E este passo que transforma a saida de blocos de voxel numa
// superficie lisa - sem ele os vertices cairiam sempre no meio da aresta.
struct EdgeCrossing {
    Vector3 position;
    Vector3 normal;
};

inline EdgeCrossing InterpolateEdge(float iso, Vector3 pa, Vector3 pb,
                                    Vector3 na, Vector3 nb, float va,
                                    float vb) {
    const float denom = vb - va;
    // Os dois cantos estao em lados opostos, entao denom so seria zero com
    // ambos exatamente no isolevel - caso em que a aresta nem seria cortada.
    // O guarda evita NaN se um campo devolver valores degenerados.
    const float t = (std::fabs(denom) > 1e-12f) ? (iso - va) / denom : 0.5f;
    return {
        Vector3Lerp(pa, pb, t),
        Vector3Normalize(Vector3Lerp(na, nb, t)),
    };
}

}  // namespace

PolygonizeStats Polygonize(const SampleGrid& grid, float iso, MeshData& out,
                           int margin) {
    PolygonizeStats stats{};
    out.Clear();

    const int res = grid.Resolution();
    const int begin = std::max(0, margin);
    const int end = res - begin;
    if (end <= begin) return stats;

    for (int z = begin; z < end; ++z) {
        for (int y = begin; y < end; ++y) {
            for (int x = begin; x < end; ++x) {
                ++stats.cellsVisited;

                float value[8];
                Vector3 position[8];
                Vector3 normal[8];
                int caseIndex = 0;

                for (int c = 0; c < 8; ++c) {
                    const int cx = x + kCornerOffset[c][0];
                    const int cy = y + kCornerOffset[c][1];
                    const int cz = z + kCornerOffset[c][2];

                    value[c] = grid.ValueAt(cx, cy, cz);
                    position[c] = grid.PositionAt(cx, cy, cz);
                    normal[c] = grid.GradientAt(cx, cy, cz);

                    // Bit ligado = canto dentro. "Dentro" e abaixo do
                    // isolevel: a convencao que as tabelas assumem.
                    if (value[c] < iso) caseIndex |= (1 << c);
                }

                const std::uint16_t cutEdges = kEdgeTable[caseIndex];
                // Casos 0 e 255: celula inteiramente fora ou dentro. Numa
                // grade tipica isso descarta a grande maioria das celulas.
                if (cutEdges == 0) continue;

                EdgeCrossing crossing[12];
                for (int e = 0; e < 12; ++e) {
                    if ((cutEdges & (1u << e)) == 0) continue;
                    const int a = kEdgeCorners[e][0];
                    const int b = kEdgeCorners[e][1];
                    crossing[e] =
                        InterpolateEdge(iso, position[a], position[b],
                                        normal[a], normal[b], value[a],
                                        value[b]);
                }

                const std::int8_t* tri = kTriTable[caseIndex];
                for (int i = 0; i < 15 && tri[i] != -1; i += 3) {
                    for (int k = 0; k < 3; ++k) {
                        const EdgeCrossing& v = crossing[tri[i + k]];
                        out.PushVertex(v.position, v.normal);
                    }
                    ++stats.triangles;
                }
                ++stats.cellsOnSurface;
            }
        }
    }

    return stats;
}

}  // namespace mc
