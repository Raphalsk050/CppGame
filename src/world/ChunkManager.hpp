#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "mc/MeshData.hpp"
#include "mc/SampleGrid.hpp"
#include "world/Chunk.hpp"
#include "world/TerrainEdits.hpp"
#include "world/TerrainGenerator.hpp"

namespace world {

struct ChunkStats {
    int loaded = 0;
    int withGeometry = 0;
    int pending = 0;
    int generatedThisFrame = 0;
    int triangles = 0;
    int drawn = 0;
};

// Streaming de chunks ao redor da camera.
//
// Duas propriedades que valem destacar:
//
// - UM UNICO buffer de densidade para o mundo inteiro. Depois de virar malha na
//   GPU, a grade escalar nao serve para mais nada, entao a mesma SampleGrid de
//   rascunho e reposicionada para gerar cada chunk. Guardar uma grade por chunk
//   custaria ~150 KB cada e nao compraria nada.
//
// - ORCAMENTO POR FRAME. Gerar tudo que falta de uma vez trava a janela por
//   varios segundos no startup e a cada avanco da camera. Aqui so um punhado de
//   chunks e gerado por frame, os mais proximos primeiro, e o mundo aparece
//   progressivamente sem perder o frame rate.
class ChunkManager {
public:
    ChunkManager(const TerrainGenerator& generator, int viewRadius);

    // Carrega o que falta perto da camera e descarta o que ficou longe.
    void Update(Vector3 cameraPosition);

    void Draw(Color tint, bool wireframe);

    // Joga fora tudo e regenera. Usar depois de mexer nas equacoes de terreno.
    // Preserva as escavacoes do jogador.
    void Invalidate();

    // ---- edicao do terreno ----------------------------------------------

    // Primeiro ponto solido ao longo do raio, ja considerando as escavacoes.
    // Devolve false se o raio nao encontrar terreno dentro de maxDistance.
    bool Raycast(Vector3 origin, Vector3 direction, float maxDistance,
                 Vector3& hitPoint) const;

    // Aplica a edicao e remalha na hora os chunks carregados que ela alcanca.
    // Chunks ainda nao carregados nao precisam de nada: quando forem gerados,
    // ja vao ler a edicao do acervo.
    void ApplyEdit(const SphereEdit& edit);

    std::size_t EditCount() const { return edits_.Count(); }
    void ClearEdits();

    void SetViewRadius(int radius);
    int ViewRadius() const { return viewRadius_; }

    // Shader aplicado a cada chunk assim que ele gera geometria. Guardado aqui
    // porque chunks nascem a qualquer momento durante o streaming.
    void SetShader(Shader shader);

    const ChunkStats& Stats() const { return stats_; }

    static ChunkCoord CoordFromWorld(Vector3 position);

private:
    void RefreshQueue(ChunkCoord center);
    void UnloadFarChunks(ChunkCoord center);
    void VerticalRange(int& minY, int& maxY) const;

    const TerrainGenerator* generator_;
    int viewRadius_;
    // Chunks gerados por frame. Baixo demais e o mundo aparece aos pedacos
    // por varios segundos; alto demais e o frame trava na geracao. 16 mantem
    // o preenchimento rapido sem perder os 60 FPS.
    int budgetPerFrame_ = 16;

    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash>
        chunks_;
    std::vector<ChunkCoord> queue_;  // pendentes, mais proximo no fim
    ChunkCoord lastCenter_{};
    // Flag explicita em vez de uma coordenada-sentinela: a comparacao de
    // centro so olha x e z, entao qualquer sentinela com x=z=0 passaria
    // despercebida - e o primeiro refresh nunca aconteceria.
    bool queueDirty_ = true;

    mc::SampleGrid scratchGrid_;
    mc::MeshData scratchMesh_;
    Shader shader_{};
    TerrainEdits edits_;

    ChunkStats stats_{};
};

}  // namespace world
