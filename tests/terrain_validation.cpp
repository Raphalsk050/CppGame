#include "world/TerrainGenerator.hpp"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <map>
#include <string>

int main() {
    world::TerrainGenerator gen{};
    const auto& st = gen.Settings();
    int falhas = 0;
    const float PLAYER = 1.8f;   // altura do jogador, em unidades = metros

    // --- 1. escala: relevo em metros coerente com o jogador ---------------
    {
        float lo=1e9f, hi=-1e9f; double soma=0; int n=0;
        for (int i=0;i<40000;++i) {
            const float x=(i%200)*60.0f-6000.0f, z=(i/200)*60.0f-6000.0f;
            const float h=gen.SurfaceHeight(x,z);
            lo=std::min(lo,h); hi=std::max(hi,h); soma+=h; ++n;
        }
        const float relevo = hi-lo;
        const bool ok = relevo > 50.0f && relevo < 600.0f;
        std::printf("escala: altura %.0f..%.0f m (media %.0f), relevo %.0f m = %.0f alturas de jogador  %s\n",
                    lo,hi,soma/n,relevo,relevo/PLAYER, ok?"ok":"FALHOU");
        if(!ok) ++falhas;
        std::printf("        nivel do mar %.0f, jogador %.1f m -> %.0f%% de um voxel de 1 m\n",
                    st.seaLevel, PLAYER, PLAYER*100);
    }

    // --- 2. caminhabilidade: inclinacao tipica -----------------------------
    {
        int amostras=0, caminhavel=0;
        for (int i=0;i<20000;++i) {
            const float x=(i%140)*40.0f-2800.0f, z=(i/140)*40.0f-2800.0f;
            const float h=gen.SurfaceHeight(x,z);
            if (h < st.seaLevel) continue;             // so terra firme
            const float d=1.0f;                        // um passo de 1 m
            const float gx=gen.SurfaceHeight(x+d,z)-h, gz=gen.SurfaceHeight(x,z+d)-h;
            const float inclin=std::sqrt(gx*gx+gz*gz)/d;
            ++amostras;
            if (inclin < 1.0f) ++caminhavel;           // menos de 45 graus
        }
        const double frac = 100.0*caminhavel/std::max(1,amostras);
        const bool ok = frac > 70.0;
        std::printf("caminhabilidade: %.1f%% do terreno abaixo de 45 graus (passo de 1 m)  %s\n",
                    frac, ok?"ok":"FALHOU");
        if(!ok) ++falhas;
    }

    // --- 3. detalhe fino: rugosidade em escala pequena --------------------
    {
        double soma=0; int n=0;
        for (int i=0;i<8000;++i) {
            const float x=(i%90)*70.0f-3000.0f, z=(i/90)*70.0f-3000.0f;
            const float h=gen.SurfaceHeight(x,z);
            // desvio em relacao a media dos vizinhos a 3 m: mede so a alta
            // frequencia, ignorando a forma geral da montanha
            const float m=0.25f*(gen.SurfaceHeight(x+3,z)+gen.SurfaceHeight(x-3,z)
                                +gen.SurfaceHeight(x,z+3)+gen.SurfaceHeight(x,z-3));
            soma += std::fabs(h-m); ++n;
        }
        const double rug = soma/n;
        const bool ok = rug > 0.15;
        std::printf("detalhe fino: rugosidade media %.3f m em escala de 3 m  %s\n",
                    rug, ok?"ok":"FALHOU (superficie lisa demais)");
        if(!ok) ++falhas;
    }

    // --- 4. saliencias: coluna com mais de um cruzamento ------------------
    //     Um heightfield tem exatamente 1 por coluna, por definicao.
    {
        int colunas=0, comSaliencia=0, maxCruz=0;
        for (int i=0;i<6000;++i) {
            const float x=(i%80)*55.0f-2200.0f, z=(i/80)*55.0f-2200.0f;
            const float h=gen.SurfaceHeight(x,z);
            int cruz=0; float ant=gen.Sample({x,h-60.0f,z});
            for (float y=h-60.0f; y<=h+60.0f; y+=0.5f) {
                const float d=gen.Sample({x,y,z});
                if ((d<0.0f) != (ant<0.0f)) ++cruz;
                ant=d;
            }
            ++colunas; if (cruz>1) ++comSaliencia;
            maxCruz=std::max(maxCruz,cruz);
        }
        const double frac=100.0*comSaliencia/colunas;
        const bool ok = frac > 0.4 && maxCruz > 1;
        std::printf("saliencias 3D: %.1f%% das colunas tem >1 cruzamento (max %d)  %s\n",
                    frac, maxCruz, ok?"ok":"FALHOU (so heightfield)");
        if(!ok) ++falhas;
    }

    // --- 5. clima ainda coerente apos a mudanca de escala -----------------
    {
        float t0,p0,t1,p1;
        gen.ClimateAt(0,0, st.seaLevel, t0,p0);
        gen.ClimateAt(0,0, st.seaLevel+180.0f, t1,p1);
        const bool ok = (t0-t1) > 10.0f;
        std::printf("clima: cume 180 m acima do mar e %.1f C mais frio  %s\n",
                    t0-t1, ok?"ok":"FALHOU (sem neve nos picos)");
        if(!ok) ++falhas;

        std::map<std::string,int> conta; int tot=0;
        for (int i=0;i<160;++i) for(int j=0;j<160;++j)
            { conta[gen.PickBiome((i-80)*140.0f,(j-80)*140.0f).name]++; ++tot; }
        std::printf("        %zu biomas em 22x22 km:", conta.size());
        for (auto&[k,v]:conta) std::printf(" %s(%.0f%%)", k.c_str(), 100.0*v/tot);
        std::printf("\n");
        if (conta.size() < 4) { ++falhas; std::printf("        FALHOU: pouca variedade\n"); }
    }

    std::printf("\nfalhas: %d\n", falhas);
    return falhas;
}
