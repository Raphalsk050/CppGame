#include "world/ChunkBuilder.hpp"

#include <algorithm>
#include <cmath>

#include "mc/Polygonizer.hpp"
#include "mc/SampleGrid.hpp"

namespace world {
namespace {

constexpr float kIsoLevel = 0.0f;

// Gera a geometria de um chunk. Roda em worker; nao pode tocar em nada de GL.
// Saia: cortina vertical pendurada na borda do chunk.
//
// E o tratamento de RACHADURA entre niveis de LOD. Dois chunks vizinhos com
// resolucoes diferentes amostram o campo em pontos diferentes na fronteira, e
// por isso suas superficies nao se encontram exatamente - sobra uma fresta por
// onde se ve o vazio. A saia nao conserta a fresta: ela pendura uma parede
// atras dela, e a fresta deixa de ser um buraco para virar uma faixa fina de
// terreno visto de lado.
//
// Foi escolhida no lugar do Transvoxel (que costura de verdade) porque nao
// exige transcrever a tabela de 512 configuracoes de celula de transicao -
// mesmo risco de erro silencioso que a triTable teve, e sem validador pronto.
// Se a saia aparecer na pratica, Transvoxel e o proximo passo.
void AppendSkirts(mc::MeshData& mesh, Vector3 chunkMin, float chunkSize,
                  float depth) {
    if (mesh.positions.size() < 3 || depth <= 0.0f) return;

    const float eps = chunkSize * 0.002f;
    const float x0 = chunkMin.x, x1 = chunkMin.x + chunkSize;
    const float z0 = chunkMin.z, z1 = chunkMin.z + chunkSize;

    // Um vertice esta na borda se encosta num dos quatro planos laterais.
    const auto onBorder = [&](Vector3 v) {
        return std::fabs(v.x - x0) < eps || std::fabs(v.x - x1) < eps ||
               std::fabs(v.z - z0) < eps || std::fabs(v.z - z1) < eps;
    };
    // Dois vertices estao na MESMA borda? So aresta assim gera saia - uma
    // aresta que cruza o interior nao faz fronteira com ninguem.
    const auto sameBorder = [&](Vector3 a, Vector3 b) {
        return (std::fabs(a.x - x0) < eps && std::fabs(b.x - x0) < eps) ||
               (std::fabs(a.x - x1) < eps && std::fabs(b.x - x1) < eps) ||
               (std::fabs(a.z - z0) < eps && std::fabs(b.z - z0) < eps) ||
               (std::fabs(a.z - z1) < eps && std::fabs(b.z - z1) < eps);
    };

    const std::size_t original = mesh.positions.size();
    for (std::size_t t = 0; t + 2 < original; t += 3) {
        for (int e = 0; e < 3; ++e) {
            const Vector3 a = mesh.positions[t + e];
            const Vector3 b = mesh.positions[t + (e + 1) % 3];
            if (!onBorder(a) || !onBorder(b) || !sameBorder(a, b)) continue;

            const Vector3 aDown{a.x, a.y - depth, a.z};
            const Vector3 bDown{b.x, b.y - depth, b.z};
            // Normal do vertice de cima, para a cortina receber a mesma luz da
            // superficie e nao aparecer como uma faixa de cor diferente.
            const Vector3 na = mesh.normals[t + e];
            const Vector3 nb = mesh.normals[t + (e + 1) % 3];
            const Color ca = mesh.colors.empty() ? Color{255, 255, 255, 255}
                                                 : mesh.colors[t + e];
            const Color cb = mesh.colors.empty() ? Color{255, 255, 255, 255}
                                                 : mesh.colors[t + (e + 1) % 3];

            // Duas faces por aresta (frente e verso): a saia e vista dos dois
            // lados dependendo de onde o observador esta.
            const Vector3 quad[6] = {a, aDown, b, b, aDown, bDown};
            const Vector3 quadN[6] = {na, na, nb, nb, na, nb};
            const Color quadC[6] = {ca, ca, cb, cb, ca, cb};
            for (int i = 0; i < 6; ++i) {
                mesh.positions.push_back(quad[i]);
                mesh.normals.push_back(quadN[i]);
                if (!mesh.colors.empty()) mesh.colors.push_back(quadC[i]);
            }
            for (int i = 5; i >= 0; --i) {
                mesh.positions.push_back(quad[i]);
                mesh.normals.push_back(quadN[i]);
                if (!mesh.colors.empty()) mesh.colors.push_back(quadC[i]);
            }
        }
    }
}

void BuildGeometry(const TerrainGenerator& generator, ChunkCoord coord, int lod,
                   const std::vector<SphereEdit>& edits, mc::SampleGrid& grid,
                   mc::MeshData& scratch, ChunkBuild& out) {
    out.coord = coord;
    out.lod = lod;
    out.triangles = 0;
    out.mesh.Clear();

    const Vector3 min{static_cast<float>(coord.x) * kChunkSize,
                      static_cast<float>(coord.y) * kChunkSize,
                      static_cast<float>(coord.z) * kChunkSize};
    // O passo da amostragem depende do nivel: o chunk cobre sempre a mesma
    // area de mundo, com menos celulas.
    const float voxel = LodVoxelSize(lod);
    const float pad = kChunkPadding * voxel;
    grid.SetResolution(LodGridResolution(lod));

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

    // A saia so e necessaria onde pode haver vizinho de outro nivel. Em LOD 0 o
    // vizinho mais grosso pode estar do lado, entao vale para todos os niveis.
    // A profundidade acompanha o tamanho da celula: o descasamento entre dois
    // niveis e da ordem da celula maior.
    AppendSkirts(scratch, min, kChunkSize, voxel * 2.5f);

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

void ChunkBuilder::Submit(ChunkCoord coord, int lod,
                          std::vector<SphereEdit> edits) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(Job{coord, lod, std::move(edits), generation_});
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
    // Alocada no maior tamanho possivel; SetResolution encolhe por job.
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
        BuildGeometry(*generator_, job.coord, job.lod, job.edits, grid,
                      scratch, build);

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
