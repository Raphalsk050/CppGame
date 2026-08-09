// Valida as cavernas sem abrir janela.
//
// Espeleogenese real: abaixo do lencol freatico a dissolucao e isotropica e o
// conduto sai TUBULAR; acima, a agua percola por gravidade e entrincheira,
// dando POCO VERTICAL. O teste mede a razao entre a extensao vertical e a
// horizontal do vazio nas duas zonas - se a fisica foi implementada, a razao
// tem de ser maior acima do lencol.
#include "world/TerrainGenerator.hpp"
#include <cstdio>
#include <cmath>
#include <algorithm>

using namespace world;

int main() {
    TerrainGenerator gen{};
    const auto& st = gen.Settings();
    int falhas = 0;
    auto vazio = [&](float x,float y,float z){ return gen.Sample({x,y,z}) > 0.0f; };

    // --- 1. existe vazio no subsolo --------------------------------------
    {
        int dentro=0, oco=0;
        for (int i=0;i<120000;++i) {
            const float x=(i%50)*23.0f, z=((i/50)%50)*23.0f;
            const float y=st.seaLevel - 10.0f - (i/2500)*3.0f;
            if (y > gen.SurfaceHeight(x,z)) continue;
            ++dentro; if (vazio(x,y,z)) ++oco;
        }
        const double frac=100.0*oco/std::max(1,dentro);
        const bool ok = frac > 1.0 && frac < 45.0;
        std::printf("vazio no subsolo: %.1f%% de %d amostras  %s\n", frac, dentro,
                    ok?"ok":(frac<=1.0?"FALHOU (sem cavernas)":"FALHOU (queijo suico)"));
        if(!ok) ++falhas;
    }

    // --- 2. perfil muda com o lencol freatico ----------------------------
    // Mede, em cada zona, ate onde o vazio se estende na vertical e na
    // horizontal a partir de um ponto oco.
    {
        auto perfil = [&](float yBase)->double {
            double soma=0; int n=0;
            for (int i=0;i<40000 && n<160;++i) {
                const float x=(i%180)*37.0f, z=((i/180)%180)*37.0f;
                const float y=yBase + (i%7)*2.0f;
                if (y > gen.SurfaceHeight(x,z)-st.caveDepthBelowSurface) continue;
                if (!vazio(x,y,z)) continue;
                int v=0,h=0;
                for (float d=1.0f; d<=14.0f; d+=1.0f) {
                    if (vazio(x,y+d,z)||vazio(x,y-d,z)) ++v;
                    if (vazio(x+d,y,z)||vazio(x-d,y,z)) ++h;
                }
                if (h>0) { soma += (double)v/h; ++n; }
            }
            return n? soma/n : 0.0;
        };
        const double vadosa  = perfil(st.waterTableLevel + 25.0f);
        const double freatic = perfil(st.waterTableLevel - 45.0f);
        const bool ok = vadosa > freatic;
        std::printf("perfil: vertical/horizontal = %.2f na zona vadosa (acima do lencol)\n"
                    "        contra %.2f na freatica (abaixo)  %s\n",
                    vadosa, freatic, ok?"ok":"FALHOU (perfil identico nas duas zonas)");
        if(!ok) ++falhas;
    }

    // --- 3. camaras existem no fundo -------------------------------------
    // Uma camara e um vazio LARGO; um tunel e estreito. Conta pontos com muito
    // espaco livre ao redor.
    {
        int salao=0, testes=0;
        const float y = st.seaLevel - st.chamberMinDepth - 25.0f;
        for (int i=0;i<26000;++i) {
            const float x=(i%160)*31.0f, z=(i/160)*31.0f;
            if (!vazio(x,y,z)) continue;
            ++testes;
            int livres=0;
            for (float d=2.0f; d<=12.0f; d+=2.0f)
                for (int a=0;a<4;++a) {
                    const float ang=a*1.5708f;
                    if (vazio(x+std::cos(ang)*d, y, z+std::sin(ang)*d)) ++livres;
                }
            if (livres >= 18) ++salao;
        }
        const double frac = testes? 100.0*salao/testes : 0.0;
        const bool ok = frac > 3.0;
        std::printf("camaras: %.1f%% dos vazios profundos sao saloes largos (de %d)  %s\n",
                    frac, testes, ok?"ok":"FALHOU (so tuneis estreitos)");
        if(!ok) ++falhas;
    }

    std::printf("\nfalhas: %d\n", falhas);
    return falhas;
}
