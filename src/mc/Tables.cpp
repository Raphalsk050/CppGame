#include "mc/Tables.hpp"

namespace mc {
namespace {

// Canto `corner` esta dentro da superficie no caso `caseIndex`?
constexpr bool CornerInside(int caseIndex, int corner) {
    return (caseIndex & (1 << corner)) != 0;
}

// Uma aresta so e cortada quando seus dois cantos caem em lados opostos.
constexpr bool EdgeIsCut(int caseIndex, int edge) {
    return CornerInside(caseIndex, kEdgeCorners[edge][0]) !=
           CornerInside(caseIndex, kEdgeCorners[edge][1]);
}

std::string Describe(int caseIndex, int slot, const std::string& what) {
    return "kTriTable[" + std::to_string(caseIndex) + "][" +
           std::to_string(slot) + "]: " + what;
}

}  // namespace

bool ValidateTables(std::string& error) {
    for (int c = 0; c < 256; ++c) {
        int count = 0;
        bool terminated = false;

        for (int i = 0; i < 16; ++i) {
            const int edge = kTriTable[c][i];

            if (edge == -1) {
                terminated = true;
                continue;
            }
            // Depois do primeiro -1 nao pode voltar a ter dado util, senao o
            // laco do poligonizador para cedo e perde triangulos.
            if (terminated) {
                error = Describe(c, i, "indice apos o terminador -1");
                return false;
            }
            if (edge < 0 || edge > 11) {
                error = Describe(c, i, "indice de aresta fora de [0,11]");
                return false;
            }
            // A invariante central: aresta usada tem que ser aresta cortada.
            if (!EdgeIsCut(c, edge)) {
                error = Describe(c, i, "aresta " + std::to_string(edge) +
                                           " nao cruza a isosuperficie");
                return false;
            }
            ++count;
        }

        if (count % 3 != 0) {
            error = "kTriTable[" + std::to_string(c) +
                    "]: quantidade de indices nao e multipla de 3";
            return false;
        }

        // Triangulo degenerado: duas arestas iguais gera area zero.
        for (int t = 0; t < count; t += 3) {
            const int a = kTriTable[c][t];
            const int b = kTriTable[c][t + 1];
            const int d = kTriTable[c][t + 2];
            if (a == b || b == d || a == d) {
                error = "kTriTable[" + std::to_string(c) + "]: triangulo " +
                        std::to_string(t / 3) + " tem arestas repetidas";
                return false;
            }
        }

        // Toda aresta cortada precisa aparecer em algum triangulo, senao a
        // malha fica com buraco naquela celula.
        for (int e = 0; e < 12; ++e) {
            const bool cut = EdgeIsCut(c, e);
            const bool used = (kEdgeTable[c] & (1u << e)) != 0;
            if (cut != used) {
                error = "caso " + std::to_string(c) + ": aresta " +
                        std::to_string(e) +
                        (cut ? " e cortada mas nao e usada"
                             : " e usada mas nao e cortada");
                return false;
            }
        }
    }

    // Um caso e seu complemento descrevem a mesma superficie (so muda qual
    // lado e o interior), entao tem que citar exatamente as mesmas arestas.
    for (int c = 0; c < 256; ++c) {
        if (kEdgeTable[c] != kEdgeTable[255 - c]) {
            error = "casos " + std::to_string(c) + " e " +
                    std::to_string(255 - c) + " usam arestas diferentes";
            return false;
        }
    }

    error.clear();
    return true;
}

}  // namespace mc
