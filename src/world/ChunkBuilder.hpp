#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "mc/MeshData.hpp"
#include "world/Chunk.hpp"
#include "world/TerrainEdits.hpp"
#include "world/TerrainGenerator.hpp"

namespace world {

// Geometria pronta para upload: so dados de CPU, nada de GPU.
struct ChunkBuild {
    ChunkCoord coord;
    int lod = 0;
    mc::MeshData mesh;
    int triangles = 0;
};

// Pool de threads que gera geometria de chunk fora da thread principal.
//
// O CORTE E AQUI E NAO EM OUTRO LUGAR: o contexto OpenGL pertence a uma unica
// thread, entao criar VAO/VBO em worker e erro. Mas a parte cara - amostrar o
// campo escalar (~39 mil pontos por chunk, cada um com varias oitavas de
// ruido), rodar marching cubes e calcular cor por vertice - e CPU pura e nao
// toca em GL. Essa parte vem para ca; a thread principal so recebe vetores
// prontos e faz o upload.
//
// Sobre seguranca de dados:
//
// - TerrainGenerator e consultado por const de varias threads. Isso e seguro
//   porque a tabela de permutacao do ruido e imutavel depois do construtor -
//   nao ha cache nem estado por chamada. Cuidado ao acrescentar memoizacao la:
//   viraria corrida de dados imediatamente.
//
// - As edicoes sao COPIADAS para dentro do job na hora do envio, em vez de
//   lidas do TerrainEdits pelo worker. Assim escavar durante a geracao nao
//   corre com quem esta gerando, e o pool nunca precisa travar o acervo.
//
// - Cada worker tem sua propria SampleGrid de rascunho (~600 KB). Compartilhar
//   uma so, como na versao serial, seria a corrida mais obvia possivel.
class ChunkBuilder {
public:
    ChunkBuilder(const TerrainGenerator& generator, int threadCount);
    ~ChunkBuilder();

    ChunkBuilder(const ChunkBuilder&) = delete;
    ChunkBuilder& operator=(const ChunkBuilder&) = delete;

    // Enfileira a geracao. `edits` e copiado de proposito - ver acima.
    void Submit(ChunkCoord coord, int lod, std::vector<SphereEdit> edits);

    // Retira ate `max` resultados prontos. Devolve quantos saiu.
    int Collect(std::vector<ChunkBuild>& out, int max);

    // Descarta o que ainda nao comecou. Os jobs em execucao terminam e seus
    // resultados sao jogados fora pelo geracao-token.
    void CancelPending();

    int QueuedCount() const;
    int ReadyCount() const;
    int ThreadCount() const { return static_cast<int>(workers_.size()); }

private:
    struct Job {
        ChunkCoord coord;
        int lod = 0;
        std::vector<SphereEdit> edits;
        std::uint64_t generation = 0;
    };

    void WorkerLoop();

    const TerrainGenerator* generator_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Job> queue_;
    std::vector<ChunkBuild> ready_;
    // Incrementado a cada CancelPending. Resultado com token velho e
    // descartado: sem isso, mudar as equacoes do terreno faria chegarem malhas
    // do mundo antigo depois da regeneracao.
    std::uint64_t generation_ = 0;

    std::atomic<bool> running_{true};
    std::vector<std::thread> workers_;
};

}  // namespace world
