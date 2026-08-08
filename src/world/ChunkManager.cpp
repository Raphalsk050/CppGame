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

ChunkManager::ChunkManager(const TerrainGenerator& generator, int viewRadius)
    : generator_(&generator),
      viewRadius_(std::max(1, viewRadius)),
      scratchGrid_(Vector3{0.0f, 0.0f, 0.0f}, Vector3{1.0f, 1.0f, 1.0f},
                   kChunkGridResolution) {
    scratchMesh_.Reserve(1u << 16);
}

void ChunkManager::SetViewRadius(int radius) {
    radius = std::clamp(radius, 1, 16);
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
                if (chunks_.find(coord) == chunks_.end()) {
                    queue_.push_back(coord);
                }
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

void ChunkManager::Update(Vector3 cameraPosition) {
    const ChunkCoord center = CoordFromWorld(cameraPosition);

    // A fila so e recalculada quando a camera muda de chunk. Refazer isso todo
    // frame seria varredura de (2r+1)^2 * camadas a toa.
    if (queueDirty_ || center.x != lastCenter_.x || center.z != lastCenter_.z) {
        lastCenter_ = center;
        queueDirty_ = false;
        UnloadFarChunks(center);
        RefreshQueue(center);
    }

    stats_.generatedThisFrame = 0;

    for (int i = 0; i < budgetPerFrame_ && !queue_.empty(); ++i) {
        const ChunkCoord coord = queue_.back();
        queue_.pop_back();

        // Pode ter sido gerado por outro caminho enquanto estava na fila.
        if (chunks_.find(coord) != chunks_.end()) continue;

        auto chunk = std::make_unique<Chunk>(coord);
        chunk->Generate(*generator_, scratchGrid_, scratchMesh_,
                        edits_.ForChunk(coord));
        chunk->SetShader(shader_);
        chunks_.emplace(coord, std::move(chunk));
        ++stats_.generatedThisFrame;
    }

    stats_.loaded = static_cast<int>(chunks_.size());
    stats_.pending = static_cast<int>(queue_.size());
    stats_.withGeometry = 0;
    stats_.triangles = 0;
    for (const auto& [coord, chunk] : chunks_) {
        if (chunk->HasGeometry()) {
            ++stats_.withGeometry;
            stats_.triangles += chunk->TriangleCount();
        }
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
    float previousDensity = densityAt(origin);

    // Camera dentro do solido: nada a acertar a frente.
    if (previousDensity < 0.0f) return false;

    for (float t = kStep; t <= maxDistance; t += kStep) {
        const Vector3 p{origin.x + dir.x * t, origin.y + dir.y * t,
                        origin.z + dir.z * t};
        const float density = densityAt(p);

        if (density < 0.0f) {
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
        previousDensity = density;
    }

    return false;
}

void ChunkManager::ApplyEdit(const SphereEdit& edit) {
    const std::vector<ChunkCoord> affected = edits_.Add(edit);

    // Remalha na hora. Sao poucos chunks (a esfera de escavacao e pequena
    // perto de um chunk), entao nao vale passar pela fila de streaming - o
    // jogador precisa ver o buraco no mesmo frame do clique.
    for (const ChunkCoord& coord : affected) {
        const auto it = chunks_.find(coord);
        if (it == chunks_.end()) continue;
        it->second->Generate(*generator_, scratchGrid_, scratchMesh_,
                             edits_.ForChunk(coord));
        it->second->SetShader(shader_);
    }
}

void ChunkManager::ClearEdits() {
    edits_.Clear();
    Invalidate();
}

void ChunkManager::Draw(Color tint, bool wireframe) {
    stats_.drawn = 0;
    for (const auto& [coord, chunk] : chunks_) {
        if (!chunk->HasGeometry()) continue;
        if (wireframe) {
            chunk->DrawWires(tint);
        } else {
            chunk->Draw(tint);
        }
        ++stats_.drawn;
    }
}

}  // namespace world
