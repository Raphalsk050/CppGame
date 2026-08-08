#include "world/TerrainGenerator.hpp"
#include <cstdio>
#include <map>
#include <string>
#include <cmath>

int main() {
    world::TerrainGenerator gen{};
    const auto& st = gen.Settings();
    int falhas = 0;

    // --- 1. lapse rate: subir tem de esfriar, na taxa certa ---------------
    {
        float t0=0, p0=0, t1=0, p1=0;
        gen.ClimateAt(0, 0, st.seaLevel, t0, p0);
        gen.ClimateAt(0, 0, st.seaLevel + 100.0f, t1, p1);
        const float km = 100.0f * st.climateVerticalExaggeration / 1000.0f;
        const float esperado = st.lapseRatePerKm * km;
        const float obtido = t0 - t1;
        const bool ok = std::fabs(obtido - esperado) < 0.01f;
        std::printf("lapse rate: +100 un -> %.2f C mais frio (esperado %.2f)  %s\n",
                    obtido, esperado, ok?"ok":"FALHOU");
        if(!ok) ++falhas;
    }

    // --- 2. sombra de chuva: barlavento mais umido que sotavento ---------
    {
        // Varre procurando uma serra e compara os dois lados dela.
        int comparacoes=0, corretas=0;
        const float wx=st.windDirectionX, wz=st.windDirectionZ;
        const float wl=std::sqrt(wx*wx+wz*wz);
        for (int i=0;i<4000;++i) {
            const float x = (i%80)*260.0f - 10000.0f;
            const float z = (i/80)*260.0f - 6000.0f;
            const float hc = gen.SurfaceHeight(x,z);
            // RELATIVO ao mar: limiar absoluto nao sobrevive a mudanca de
            // escala do mundo, e deixava o teste comparar terreno plano.
            if (hc < st.seaLevel + 90.0f) continue;
            const float d = 300.0f;
            const float bx = x - wx/wl*d, bz = z - wz/wl*d;  // barlavento
            const float sx = x + wx/wl*d, sz = z + wz/wl*d;  // sotavento
            float tb,pb,ts,ps;
            gen.ClimateAt(bx,bz, gen.SurfaceHeight(bx,bz), tb,pb);
            gen.ClimateAt(sx,sz, gen.SurfaceHeight(sx,sz), ts,ps);
            ++comparacoes;
            if (pb > ps) ++corretas;
        }
        const double frac = comparacoes? 100.0*corretas/comparacoes : 0;
        const bool ok = frac > 60.0;
        std::printf("sombra de chuva: barlavento mais umido em %.1f%% de %d serras  %s\n",
                    frac, comparacoes, ok?"ok":"FALHOU");
        if(!ok) ++falhas;
    }

    // --- 3. distribuicao de biomas num territorio grande -----------------
    {
        std::map<std::string,int> conta;
        int total=0;
        for (int i=0;i<200;++i) for(int j=0;j<200;++j) {
            const float x=(i-100)*90.0f, z=(j-100)*90.0f;
            conta[gen.PickBiome(x,z).name]++; ++total;
        }
        std::printf("\ndistribuicao de biomas em %d amostras (18x18 km):\n", total);
        for (auto& [nome,n] : conta)
            std::printf("  %-24s %5.1f%%\n", nome.c_str(), 100.0*n/total);
        const bool ok = conta.size() >= 4;
        std::printf("  -> %zu biomas distintos  %s\n", conta.size(), ok?"ok":"FALHOU (pouca variedade)");
        if(!ok) ++falhas;
    }

    // --- 4. coerencia: nada tropical no alto da montanha -----------------
    {
        int violacoes=0, picos=0;
        for (int i=0;i<20000;++i) {
            const float x=(i%200)*70.0f-7000.0f, z=(i/200)*70.0f-3500.0f;
            const float h=gen.SurfaceHeight(x,z);
            if (h < st.seaLevel + 130.0f) continue;   // relativo ao mar
            ++picos;
            const std::string b = gen.PickBiome(x,z).name;
            if (b.find("tropical")!=std::string::npos ||
                b.find("savana")!=std::string::npos) ++violacoes;
        }
        const bool ok = (picos==0) || (violacoes*100/picos < 2);
        std::printf("\ncoerencia altitude: %d picos (>mar+130), %d com bioma quente  %s\n",
                    picos, violacoes, ok?"ok":"FALHOU");
        if(!ok) ++falhas;
    }
    std::printf("\nfalhas: %d\n", falhas);
    return falhas;
}
