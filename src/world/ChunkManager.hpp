#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "render/Frustum.hpp"
#include "world/Chunk.hpp"
#include "world/ChunkBuilder.hpp"
#include "world/TerrainEdits.hpp"
#include "world/TerrainGenerator.hpp"

namespace world {

struct ChunkStats {
    int loaded = 0;
    int withGeometry = 0;
    int pending = 0;      // na fila do pool
    int inFlight = 0;     // enviados e ainda nao recebidos
    int uploadedThisFrame = 0;
    int triangles = 0;
    int drawn = 0;        // sobreviveram ao frustum culling
    int lodCounts[4] = {0, 0, 0, 0};
    int culled = 0;
    int threads = 0;
};

// Streaming de chunks ao redor da camera.
//
// Divisao de trabalho entre threads: o ChunkBuilder gera a geometria em
// paralelo (campo escalar, marching cubes, cor) e esta classe faz o upload
// para a GPU na thread principal, que e a unica que pode tocar em GL.
class ChunkManager {
public:
    ChunkManager(const TerrainGenerator& generator, int viewRadius,
                 int threadCount);

    // Carrega o que falta perto da camera, recebe o que os workers terminaram e
    // descarta o que ficou longe.
    void Update(Vector3 cameraPosition);

    // Desenha so o que intersecta o tronco de visao.
    void Draw(const Camera3D& camera, Color tint, bool wireframe);

    // Joga fora tudo e regenera. Preserva as escavacoes do jogador.
    void Invalidate();

    void SetViewRadius(int radius);
    int ViewRadius() const { return viewRadius_; }

    void SetShader(Shader shader);

    // ---- edicao do terreno ----------------------------------------------

    bool Raycast(Vector3 origin, Vector3 direction, float maxDistance,
                 Vector3& hitPoint) const;

    // Densidade num ponto qualquer, ja com as escavacoes. Negativo e solido.
    // Publico porque a colisao do jogador testa o CAMPO, nao a malha: a malha
    // esta na GPU e fatiada em chunks que aparecem e somem com o streaming,
    // enquanto o campo responde em qualquer lugar - inclusive onde nenhum
    // chunk foi gerado ainda.
    float DensityAt(Vector3 p) const {
        return generator_->Sample(p) + edits_.DeltaAt(p);
    }
    void ApplyEdit(const SphereEdit& edit);

    std::size_t EditCount() const { return edits_.Count(); }
    void ClearEdits();

    const ChunkStats& Stats() const { return stats_; }

    static ChunkCoord CoordFromWorld(Vector3 position);

private:
    void RefreshQueue(ChunkCoord center);
    void UnloadFarChunks(ChunkCoord center);
    void VerticalRange(int& minY, int& maxY) const;
    // Nivel de detalhe pelo afastamento em chunks. Fica aqui, e nao no
    // builder, porque depende da posicao da camera.
    int LodForCoord(ChunkCoord coord, ChunkCoord center) const;
    void SubmitPending();
    void CollectFinished();

    const TerrainGenerator* generator_;
    int viewRadius_;

    // Quantos jobs podem estar em voo. Limitar evita encher a fila do pool com
    // milhares de chunks que ficariam obsoletos assim que a camera andasse.
    int maxInFlight_ = 64;
    // Uploads de GPU por frame. Diferente do orcamento antigo: agora a geracao
    // nao custa nada na thread principal, so o upload - entao pode ser bem
    // mais alto sem perder frame.
    int uploadsPerFrame_ = 24;

    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash>
        chunks_;
    // Chunks ja enviados ao pool. Impede reenviar o mesmo enquanto ele esta
    // sendo gerado - sem isto a fila encheria de duplicatas a cada refresh.
    std::unordered_set<ChunkCoord, ChunkCoordHash> inFlight_;

    // Distancia, em chunks, a partir da qual cada nivel entra. Perto o
    // terreno precisa do detalhe; longe ele ocupa poucos pixels e a resolucao
    // cheia seria desperdicio puro.
    int lodDistance_[4] = {0, 3, 7, 13};

    std::vector<ChunkCoord> queue_;  // pendentes, mais proximo no fim
    ChunkCoord lastCenter_{};
    bool queueDirty_ = true;

    ChunkBuilder builder_;
    std::vector<ChunkBuild> harvest_;  // reusado entre frames

    render::Frustum frustum_;
    Shader shader_{};
    TerrainEdits edits_;

    ChunkStats stats_{};
};

}  // namespace world
