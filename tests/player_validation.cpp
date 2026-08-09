// Valida a colisao do jogador sem abrir janela.
//
// O caso que importa e o tunelamento: a 40 m/s um frame de 16 ms desloca
// 0,64 m, o suficiente para comecar de um lado de uma parede e terminar do
// outro sem que nenhum teste pontual tenha visto o solido no meio. So a
// varredura pega isso, e so um teste que force velocidade alta prova que a
// varredura funciona.
#include "app/PlayerController.hpp"
#include "world/ChunkManager.hpp"
#include "world/TerrainGenerator.hpp"
#include <cstdio>
#include <cmath>
#include <algorithm>

using namespace app;
using namespace world;

int main() {
    TerrainGenerator gen{};
    // 0 threads de verdade nao sao usadas: o teste so consulta DensityAt.
    ChunkManager mundo(gen, 2, 1);
    const auto& st = gen.Settings();
    int falhas = 0;

    auto solido = [&](Vector3 p){ return mundo.DensityAt(p) < 0.0f; };

    // Acha um ponto de terreno firme para os testes.
    Vector3 chao{0,0,0}; bool achou=false;
    for (int i=0;i<4000 && !achou;++i) {
        const float x=(i%64)*40.0f-1280.0f, z=(i/64)*40.0f-1280.0f;
        const float h=gen.SurfaceHeight(x,z);
        if (h > st.seaLevel+12.0f) { chao={x,h+2.0f,z}; achou=true; }
    }
    std::printf("ponto de teste: %.0f %.0f %.0f\n", chao.x, chao.y, chao.z);

    // --- 1. gravidade poe o jogador no chao -------------------------------
    {
        PlayerController p; p.SetMode(MoveMode::Walk);
        p.SetPosition({chao.x, chao.y+30.0f, chao.z});
        for (int i=0;i<600;++i) p.Update(mundo, {0,0,0}, false, false, 1.0f/60.0f);
        const bool ok = p.OnGround() && !solido({p.Position().x, p.Position().y+0.9f, p.Position().z});
        std::printf("queda: parou em y=%.2f, no chao=%d, nao enterrado=%d  %s\n",
                    p.Position().y, p.OnGround(),
                    !solido({p.Position().x,p.Position().y+0.9f,p.Position().z}),
                    ok?"ok":"FALHOU");
        if(!ok) ++falhas;
    }

    // --- 2. nao atravessa terreno em velocidade alta ----------------------
    {
        int atravessou=0, testes=0;
        for (int t=0;t<60;++t) {
            PlayerController p; p.SetMode(MoveMode::Walk);
            // nasce no ar e cai, depois corre em direcao aleatoria
            const float ang = t*0.1047f;
            Vector3 ini{chao.x + std::cos(ang)*3.0f, chao.y+10.0f, chao.z + std::sin(ang)*3.0f};
            p.SetPosition(ini);
            for (int i=0;i<200;++i) p.Update(mundo,{0,0,0},false,false,1.0f/60.0f);

            const Vector3 dir{std::cos(ang),0.0f,std::sin(ang)};
            for (int i=0;i<400;++i) {
                p.Update(mundo, dir, false, true, 1.0f/60.0f);   // correndo
                // o corpo nunca pode estar dentro do solido
                const Vector3 pos=p.Position();
                if (solido({pos.x,pos.y+0.9f,pos.z})) { ++atravessou; break; }
            }
            ++testes;
        }
        const bool ok = atravessou==0;
        std::printf("corrida: %d de %d trajetos terminaram dentro do solido  %s\n",
                    atravessou, testes, ok?"ok":"FALHOU (tunelamento)");
        if(!ok) ++falhas;
    }

    // --- 3. pulo tem altura plausivel -------------------------------------
    {
        PlayerController p; p.SetMode(MoveMode::Walk);
        p.SetPosition({chao.x, chao.y+8.0f, chao.z});
        for (int i=0;i<400;++i) p.Update(mundo,{0,0,0},false,false,1.0f/60.0f);
        const float base = p.Position().y;
        float topo = base;
        for (int i=0;i<120;++i) {
            p.Update(mundo,{0,0,0}, i<3, false, 1.0f/60.0f);
            topo = std::max(topo, p.Position().y);
        }
        const float alt = topo-base;
        const bool ok = alt > 0.8f && alt < 2.5f;
        std::printf("pulo: %.2f m  %s\n", alt, ok?"ok":"FALHOU");
        if(!ok) ++falhas;
    }

    // --- 4. espectador atravessa (e o esperado) ---------------------------
    {
        PlayerController p; p.SetMode(MoveMode::Spectator);
        p.SetPosition({chao.x, chao.y-40.0f, chao.z});   // dentro da montanha
        const Vector3 antes = p.Position();
        for (int i=0;i<60;++i) p.Update(mundo,{1,0,0},false,false,1.0f/60.0f);
        const bool ok = std::fabs(p.Position().x-antes.x) > 5.0f;
        std::printf("espectador: andou %.1f m dentro do solido  %s\n",
                    std::fabs(p.Position().x-antes.x), ok?"ok":"FALHOU");
        if(!ok) ++falhas;
    }

    std::printf("\nfalhas: %d\n", falhas);
    return falhas;
}
