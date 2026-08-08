#include "world/ChunkBuilder.hpp"

#include <algorithm>

#include "mc/Polygonizer.hpp"
#include "mc/SampleGrid.hpp"

namespace world {
namespace {

constexpr float kIsoLevel = 0.0f;

// Gera a geometria de um chunk. Roda em worker; nao pode tocar em nada de GL.
void BuildGeometry(const TerrainGenerator& generator, ChunkCoord coord,
                   const std::vector<SphereEdit>& edits, mc::SampleGrid& grid,
                   mc::MeshData& scratch, ChunkBuild& out) {
    out.coord = coord;
    out.triangles = 0;
    out.mesh.Clear();

    const Vector3 min{static_cast<float>(coord.x) * kChunkSize,
                      static_cast<float>(coord.y) * kChunkSize,
                      static_cast<float>(coord.z) * kChunkSize};
    const float pad = kChunkPadding * kVoxelSize;

    // Descarte barato: chunk todo acima da maior altura possivel e so ar; todo
    // abaixo da menor e solido macico. Chunk com edicao nunca e descartado -
    // cavar dentro de solido cria superficie onde o teste diria que nao ha.
    if (edits.empty()) {
        float minHeight = 0.0f;
        float maxHeight = 0.0f;
        generator.HeightBounds(minHeight, maxHeight);

        if (min.y > maxHeight) return;
        if (min.y + kChunkSize < minHeight && !generator.HasCaves()) return;
    }

    grid.SetBounds(Vector3{min.x - pad, min.y - pad, min.z - pad},
                   Vector3{min.x + kChunkSize + pad, min.y + kChunkSize + pad,
                           min.z + kChunkSize + pad});

    // A altura da superficie e 2D e cara; a densidade e 3D e barata. Calcular a
    // altura uma vez por coluna evita repeti-la para cada ponto vertical.
    grid.SampleColumns(
        [&generator](float x, float z) {
            return generator.SurfaceHeight(x, z);
        },
        [&generator, &edits](float surfaceHeight, Vector3 p) {
            float density = generator.DensityAt(p, surfaceHeight);
            for (const SphereEdit& edit : edits) {
                density += EditContribution(edit, p);
            }
            return density;
        });

    const mc::PolygonizeStats stats =
        mc::Polygonize(grid, kIsoLevel, scratch, kChunkPadding);
    if (stats.triangles == 0) return;

    scratch.colors.resize(scratch.positions.size());
    for (std::size_t i = 0; i < scratch.positions.size(); ++i) {
        scratch.colors[i] =
            generator.SurfaceColor(scratch.positions[i], scratch.normals[i]);
    }

    // Move em vez de copiar: sao centenas de KB por chunk atravessando a
    // fronteira de thread.
    out.mesh = std::move(scratch);
    out.triangles = stats.triangles;
    scratch = mc::MeshData{};
}

}  // namespace

ChunkBuilder::ChunkBuilder(const TerrainGenerator& generator, int threadCount)
    : generator_(&generator) {
    // Uma folga em relacao ao total de nucleos: a thread principal ainda tem de
    // desenhar, e o driver de GL usa mais uma.
    threadCount = std::clamp(threadCount, 1, 16);
    workers_.reserve(static_cast<std::size_t>(threadCount));
    for (int i = 0; i < threadCount; ++i) {
        workers_.emplace_back([this] { WorkerLoop(); });
    }
}

ChunkBuilder::~ChunkBuilder() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        queue_.clear();
    }
    cv_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
}

void ChunkBuilder::Submit(ChunkCoord coord, std::vector<SphereEdit> edits) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(Job{coord, std::move(edits), generation_});
    }
    cv_.notify_one();
}

int ChunkBuilder::Collect(std::vector<ChunkBuild>& out, int max) {
    std::lock_guard<std::mutex> lock(mutex_);

    const int count =
        std::min<int>(max, static_cast<int>(ready_.size()));
    for (int i = 0; i < count; ++i) {
        out.push_back(std::move(ready_[static_cast<std::size_t>(i)]));
    }
    ready_.erase(ready_.begin(), ready_.begin() + count);
    return count;
}

void ChunkBuilder::CancelPending() {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
    ready_.clear();
    ++generation_;
}

int ChunkBuilder::QueuedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(queue_.size());
}

int ChunkBuilder::ReadyCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(ready_.size());
}

void ChunkBuilder::WorkerLoop() {
    // Rascunhos por thread. Alocados uma vez e reusados por todos os jobs deste
    // worker: sao ~600 KB de densidade mais gradientes, caros de realocar.
    mc::SampleGrid grid(Vector3{0.0f, 0.0f, 0.0f}, Vector3{1.0f, 1.0f, 1.0f},
                        kChunkGridResolution);
    mc::MeshData scratch;
    scratch.Reserve(1u << 15);

    while (true) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !running_ || !queue_.empty(); });
            if (!running_) return;
            job = std::move(queue_.front());
            queue_.pop_front();
        }

        ChunkBuild build;
        BuildGeometry(*generator_, job.coord, job.edits, grid, scratch, build);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            // O mundo pode ter sido regerado enquanto este job rodava. Entregar
            // agora colocaria geometria do terreno antigo no mundo novo.
            if (job.generation == generation_) {
                ready_.push_back(std::move(build));
            }
        }
    }
}

}  // namespace world
