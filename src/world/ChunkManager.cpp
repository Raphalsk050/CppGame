#include "world/ChunkManager.hpp"

#include <algorithm>
#include <cmath>

namespace world {
namespace {

// Profundidade extra gerada quando ha cavernas: sem elas o subsolo e macico e
// nao produz triangulo nenhum.
constexpr float kCaveDepth = 96.0f;

int FloorDiv(float value, float divisor) {
    return static_cast<int>(std::floor(value / divisor));
}

}  // namespace

ChunkCoord ChunkManager::CoordFromWorld(Vector3 position) {
    return ChunkCoord{
        FloorDiv(position.x, kChunkSize),
        FloorDiv(position.y, kChunkSize),
        FloorDiv(position.z, kChunkSize),
    };
}

ChunkManager::ChunkManager(const TerrainGenerator& generator, int viewRadius,
                           int threadCount)
    : generator_(&generator),
      viewRadius_(std::max(1, viewRadius)),
      builder_(generator, threadCount) {
    stats_.threads = builder_.ThreadCount();
}

void ChunkManager::SetViewRadius(int radius) {
    radius = std::clamp(radius, 1, 24);
    if (radius == viewRadius_) return;
    viewRadius_ = radius;
    queueDirty_ = true;
}

void ChunkManager::SetShader(Shader shader) {
    shader_ = shader;
    for (auto& [coord, chunk] : chunks_) chunk->SetShader(shader_);
}

void ChunkManager::Invalidate() {
    chunks_.clear();
    queue_.clear();
    inFlight_.clear();
    // Descarta tambem o que ja esta pronto no pool: seria geometria do terreno
    // anterior chegando depois da troca.
    builder_.CancelPending();
    queueDirty_ = true;
}

// Faixa vertical de chunks que pode conter superficie, deduzida das amplitudes
// do gerador. Num mundo plano isto e uma ou duas camadas - o resto do volume
// nem entra na fila.
void ChunkManager::VerticalRange(int& minY, int& maxY) const {
    float minHeight = 0.0f;
    float maxHeight = 0.0f;
    generator_->HeightBounds(minHeight, maxHeight);

    if (generator_->HasCaves()) minHeight -= kCaveDepth;

    minY = FloorDiv(minHeight, kChunkSize);
    maxY = FloorDiv(maxHeight, kChunkSize);
    if (maxY < minY) maxY = minY;
}

void ChunkManager::RefreshQueue(ChunkCoord center) {
    queue_.clear();

    int minY = 0;
    int maxY = 0;
    VerticalRange(minY, maxY);

    const int r = viewRadius_;
    for (int dz = -r; dz <= r; ++dz) {
        for (int dx = -r; dx <= r; ++dx) {
            // Raio circular em vez de quadrado: evita gerar os cantos, que
            // ficam mais longe que a distancia de visao em todas as direcoes.
            if (dx * dx + dz * dz > r * r) continue;

            for (int y = minY; y <= maxY; ++y) {
                const ChunkCoord coord{center.x + dx, y, center.z + dz};
                if (inFlight_.find(coord) != inFlight_.end()) continue;

                // Entra na fila se falta OU se esta no nivel errado - e assim
                // que aproximar da camera aumenta o detalhe.
                const auto it = chunks_.find(coord);
                if (it != chunks_.end() &&
                    it->second->Lod() == LodForCoord(coord, center)) {
                    continue;
                }
                queue_.push_back(coord);
            }
        }
    }

    // Mais distante primeiro: o consumo tira do fim do vetor, entao o chunk
    // mais proximo da camera e o primeiro a ser gerado.
    std::sort(queue_.begin(), queue_.end(),
              [center](const ChunkCoord& a, const ChunkCoord& b) {
                  const long long da =
                      1LL * (a.x - center.x) * (a.x - center.x) +
                      1LL * (a.z - center.z) * (a.z - center.z) +
                      1LL * (a.y - center.y) * (a.y - center.y);
                  const long long db =
                      1LL * (b.x - center.x) * (b.x - center.x) +
                      1LL * (b.z - center.z) * (b.z - center.z) +
                      1LL * (b.y - center.y) * (b.y - center.y);
                  return da > db;
              });
}

void ChunkManager::UnloadFarChunks(ChunkCoord center) {
    // Uma folga de 1 chunk alem do raio evita carregar e descarregar em
    // sequencia quando a camera fica oscilando em cima de uma fronteira.
    const int limit = (viewRadius_ + 1) * (viewRadius_ + 1);

    for (auto it = chunks_.begin(); it != chunks_.end();) {
        const int dx = it->first.x - center.x;
        const int dz = it->first.z - center.z;
        if (dx * dx + dz * dz > limit) {
            it = chunks_.erase(it);
        } else {
            ++it;
        }
    }
}

int ChunkManager::LodForCoord(ChunkCoord coord, ChunkCoord center) const {
    const int dx = coord.x - center.x;
    const int dz = coord.z - center.z;
    const int d2 = dx * dx + dz * dz;

    int lod = 0;
    for (int i = kMaxLod; i >= 1; --i) {
        if (d2 >= lodDistance_[i] * lodDistance_[i]) {
            lod = i;
            break;
        }
    }
    return lod;
}

void ChunkManager::SubmitPending() {
    while (!queue_.empty() &&
           static_cast<int>(inFlight_.size()) < maxInFlight_) {
        const ChunkCoord coord = queue_.back();
        queue_.pop_back();

        const int lod = LodForCoord(coord, lastCenter_);

        // Ja carregado NO NIVEL CERTO? nada a fazer. Se o nivel mudou (a camera
        // se aproximou), reenvia para regerar com mais detalhe.
        const auto existing = chunks_.find(coord);
        if (existing != chunks_.end() && existing->second->Lod() == lod) continue;
        if (!inFlight_.insert(coord).second) continue;

        // As edicoes sao copiadas aqui, na thread principal. O worker nunca le
        // o acervo, entao escavar durante a geracao nao corre com ninguem.
        const std::vector<SphereEdit>* edits = edits_.ForChunk(coord);
        builder_.Submit(coord, lod, edits ? *edits : std::vector<SphereEdit>{});
    }
}

void ChunkManager::CollectFinished() {
    harvest_.clear();
    builder_.Collect(harvest_, uploadsPerFrame_);

    stats_.uploadedThisFrame = static_cast<int>(harvest_.size());

    for (ChunkBuild& build : harvest_) {
        inFlight_.erase(build.coord);

        // Pode ter sido descarregado enquanto era gerado (camera andou).
        auto chunk = std::make_unique<Chunk>(build.coord, build.lod);
        if (build.triangles > 0) {
            // Unico ponto de contato com a GPU em todo o caminho de geracao.
            chunk->Upload(build.mesh, build.triangles);
            chunk->SetShader(shader_);
        }
        chunks_[build.coord] = std::move(chunk);
    }
}

void ChunkManager::Update(Vector3 cameraPosition) {
    const ChunkCoord center = CoordFromWorld(cameraPosition);

    // Mudar de chunk altera a distancia de TODOS os chunks, e portanto o nivel
    // de varios deles - por isso a fila e refeita, nao so complementada.
    if (queueDirty_ || center.x != lastCenter_.x || center.z != lastCenter_.z) {
        lastCenter_ = center;
        queueDirty_ = false;
        UnloadFarChunks(center);
        RefreshQueue(center);
    }

    SubmitPending();
    CollectFinished();

    stats_.loaded = static_cast<int>(chunks_.size());
    stats_.pending = static_cast<int>(queue_.size());
    stats_.inFlight = static_cast<int>(inFlight_.size());
    stats_.withGeometry = 0;
    stats_.triangles = 0;
    for (int& n : stats_.lodCounts) n = 0;
    for (const auto& [coord, chunk] : chunks_) {
        if (!chunk->HasGeometry()) continue;
        ++stats_.withGeometry;
        stats_.triangles += chunk->TriangleCount();
        const int lod = std::clamp(chunk->Lod(), 0, kMaxLod);
        ++stats_.lodCounts[lod];
    }
}

// ===========================================================================
//  EDICAO DO TERRENO
// ===========================================================================

// Ray marching contra o campo escalar: caminha em passos fixos ate a densidade
// virar negativa (solido), depois refina por bisseccao.
//
// Marchar o campo em vez de intersectar a malha e o caminho certo aqui - a
// malha esta na GPU e dividida em chunks, enquanto o campo e uma funcao
// consultavel em qualquer ponto. O passo tem que ser menor que a menor feicao
// que se quer acertar, senao o raio "pula" paredes finas.
bool ChunkManager::Raycast(Vector3 origin, Vector3 direction,
                           float maxDistance, Vector3& hitPoint) const {
    constexpr float kStep = 0.35f;
    constexpr int kRefineIterations = 12;

    const float length = std::sqrt(direction.x * direction.x +
                                   direction.y * direction.y +
                                   direction.z * direction.z);
    if (length <= 0.0f) return false;
    const Vector3 dir{direction.x / length, direction.y / length,
                      direction.z / length};

    const auto densityAt = [this](Vector3 p) {
        return generator_->Sample(p) + edits_.DeltaAt(p);
    };

    float previousT = 0.0f;

    // Camera dentro do solido: nada a acertar a frente.
    if (densityAt(origin) < 0.0f) return false;

    for (float t = kStep; t <= maxDistance; t += kStep) {
        const Vector3 p{origin.x + dir.x * t, origin.y + dir.y * t,
                        origin.z + dir.z * t};

        if (densityAt(p) < 0.0f) {
            // Bisseccao entre o ultimo ponto no ar e o primeiro no solido.
            float lo = previousT;
            float hi = t;
            for (int i = 0; i < kRefineIterations; ++i) {
                const float mid = (lo + hi) * 0.5f;
                const Vector3 q{origin.x + dir.x * mid, origin.y + dir.y * mid,
                                origin.z + dir.z * mid};
                if (densityAt(q) < 0.0f) {
                    hi = mid;
                } else {
                    lo = mid;
                }
            }
            hitPoint = Vector3{origin.x + dir.x * hi, origin.y + dir.y * hi,
                               origin.z + dir.z * hi};
            return true;
        }

        previousT = t;
    }

    return false;
}

void ChunkManager::ApplyEdit(const SphereEdit& edit) {
    const std::vector<ChunkCoord> affected = edits_.Add(edit);

    // Reenvia os chunks atingidos ao pool. Sao poucos - a esfera de escavacao e
    // pequena perto de um chunk - e o resultado chega no frame seguinte ou no
    // outro, o que na pratica e imperceptivel.
    for (const ChunkCoord& coord : affected) {
        const auto it = chunks_.find(coord);
        if (it == chunks_.end()) continue;
        if (!inFlight_.insert(coord).second) continue;

        // Regera no MESMO nivel em que o chunk esta: escavar nao e motivo para
        // mudar o detalhe, e trocar o nivel aqui faria a malha "pular".
        const std::vector<SphereEdit>* list = edits_.ForChunk(coord);
        builder_.Submit(coord, it->second->Lod(),
                        list ? *list : std::vector<SphereEdit>{});
    }
}

void ChunkManager::ClearEdits() {
    edits_.Clear();
    Invalidate();
}

void ChunkManager::Draw(const Camera3D& camera, Color tint, bool wireframe) {
    // O aspecto vem do framebuffer, nao da janela logica: com HIGHDPI os dois
    // diferem, e usar o errado desalinharia os planos laterais do que aparece.
    const float aspect = static_cast<float>(GetRenderWidth()) /
                         static_cast<float>(GetRenderHeight());
    frustum_.Update(camera, aspect);

    stats_.drawn = 0;
    stats_.culled = 0;

    for (const auto& [coord, chunk] : chunks_) {
        if (!chunk->HasGeometry()) continue;

        if (!frustum_.Intersects(chunk->Bounds())) {
            ++stats_.culled;
            continue;
        }

        if (wireframe) {
            chunk->DrawWires(tint);
        } else {
            chunk->Draw(tint);
        }
        ++stats_.drawn;
    }
}

}  // namespace world
