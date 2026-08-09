// Valida o LOD sem abrir janela.
//
// A rachadura entre niveis e o problema classico do marching cubes com LOD:
// dois chunks vizinhos com resolucoes diferentes amostram o campo em pontos
// diferentes na fronteira, e as superficies nao se encontram. Aqui a fenda e
// MEDIDA, em vez de procurada a olho.
#include "mc/Polygonizer.hpp"
#include "mc/SampleGrid.hpp"
#include "world/Chunk.hpp"
#include "world/TerrainGenerator.hpp"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>

using namespace world;

// Poligoniza um chunk num nivel e devolve os vertices (sem saia).
static void Build(const TerrainGenerator& gen, ChunkCoord c, int lod,
                  mc::MeshData& out) {
    const int res = LodGridResolution(lod);
    const float voxel = LodVoxelSize(lod);
    const float pad = kChunkPadding * voxel;
    const Vector3 min{c.x * kChunkSize, c.y * kChunkSize, c.z * kChunkSize};

    mc::SampleGrid grid({0,0,0}, {1,1,1}, res);
    grid.SetBounds({min.x-pad, min.y-pad, min.z-pad},
                   {min.x+kChunkSize+pad, min.y+kChunkSize+pad, min.z+kChunkSize+pad});
    grid.SampleColumns(
        [&](float x, float z){ return gen.SurfaceHeight(x,z); },
        [&](float h, Vector3 p){ return gen.DensityAt(p,h); });
    mc::Polygonize(grid, 0.0f, out, kChunkPadding);
}

int main() {
    TerrainGenerator gen{};
    int falhas = 0;

    // --- 1. resolucao cai pela metade a cada nivel ------------------------
    {
        bool ok = true;
        std::printf("niveis:");
        for (int l = 0; l <= kMaxLod; ++l) {
            std::printf("  LOD%d=%d celulas (voxel %.1f m)", l, LodCells(l), LodVoxelSize(l));
            if (l > 0 && LodCells(l) > LodCells(l-1)) ok = false;
        }
        std::printf("  %s\n", ok?"ok":"FALHOU");
        if(!ok) ++falhas;
    }

    // --- 2. custo cai com o nivel ----------------------------------------
    {
        // Procura um chunk que REALMENTE tenha superficie. Sem isso o teste
        // passa a toa: um chunk vazio da 0 em todos os niveis e "0 <= 0" e
        // verdade - validaria nada.
        ChunkCoord c{0,0,0};
        int base = 0;
        for (int cx = -4; cx <= 4 && base < 200; ++cx)
            for (int cy = 1; cy <= 5 && base < 200; ++cy)
                for (int cz = -4; cz <= 4 && base < 200; ++cz) {
                    mc::MeshData m; Build(gen, {cx,cy,cz}, 0, m);
                    if ((int)m.TriangleCount() > base) { base = (int)m.TriangleCount(); c = {cx,cy,cz}; }
                }

        if (base < 200) {
            std::printf("triangulos por nivel: FALHOU (nao achei chunk com superficie)\n");
            ++falhas;
        } else {
            int anterior = 1<<30; bool ok = true;
            std::printf("triangulos por nivel no chunk (%d,%d,%d):", c.x,c.y,c.z);
            for (int l = 0; l <= kMaxLod; ++l) {
                mc::MeshData m; Build(gen, c, l, m);
                const int t = (int)m.TriangleCount();
                std::printf("  LOD%d=%d", l, t);
                if (t > anterior) ok = false;
                anterior = t;
            }
            // O nivel mais grosso tem de custar bem menos que o mais fino.
            mc::MeshData f0, f3; Build(gen,c,0,f0); Build(gen,c,kMaxLod,f3);
            const double reducao = 100.0*(1.0 - (double)f3.TriangleCount()/f0.TriangleCount());
            std::printf("  (LOD%d corta %.0f%%)  %s\n", kMaxLod, reducao,
                        (ok && reducao > 70.0)?"ok":"FALHOU");
            if(!(ok && reducao > 70.0)) ++falhas;
        }
    }

    // --- 3. TAMANHO DA FENDA entre niveis vizinhos ------------------------
    // Mede, ao longo da fronteira comum, a maior diferenca vertical entre a
    // superficie do chunk fino e a do grosso. E esse valor que a saia precisa
    // cobrir.
    {
        float piorFenda = 0.0f;
        int amostras = 0;
        for (int cx = 0; cx < 6; ++cx) {
            const ChunkCoord a{cx, 2, 0};
            mc::MeshData fino, grosso;
            Build(gen, a, 0, fino);
            Build(gen, a, 2, grosso);
            if (fino.positions.empty() || grosso.positions.empty()) continue;

            const float borda = a.x * kChunkSize;   // plano x = borda
            const float eps = 0.05f;
            for (const Vector3& v : fino.positions) {
                if (std::fabs(v.x - borda) > eps) continue;
                // vertice mais proximo na malha grossa, no mesmo plano
                float melhor = 1e9f;
                for (const Vector3& w : grosso.positions) {
                    if (std::fabs(w.x - borda) > eps) continue;
                    if (std::fabs(w.z - v.z) > LodVoxelSize(2)) continue;
                    melhor = std::min(melhor, std::fabs(w.y - v.y));
                }
                if (melhor < 1e8f) { piorFenda = std::max(piorFenda, melhor); ++amostras; }
            }
        }
        const float saia = LodVoxelSize(2) * 2.5f;
        const bool ok = (amostras == 0) || (piorFenda < saia);
        std::printf("fenda entre LOD0 e LOD2: pior %.2f m em %d pontos; saia cobre %.2f m  %s\n",
                    piorFenda, amostras, saia, ok?"ok":"FALHOU (saia curta demais)");
        if(!ok) ++falhas;
    }

    std::printf("\nfalhas: %d\n", falhas);
    return falhas;
}
