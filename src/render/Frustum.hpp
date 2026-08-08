#pragma once

#include "raylib.h"

namespace render {

// Os seis planos do tronco de visao, para descartar o que esta fora da tela
// antes de mandar draw call.
//
// Sem isto, todo chunk carregado vira draw call - inclusive os que estao atras
// da camera, que sao pouco menos da metade deles. A GPU descartaria os
// triangulos depois de transformar cada vertice; o teste aqui custa seis
// produtos escalares por chunk e evita o trabalho inteiro.
class Frustum {
public:
    // `aspect` tem de ser o mesmo que a raylib usa no BeginMode3D
    // (largura/altura da tela), senao os planos laterais nao batem com o que
    // aparece e chunks somem nas bordas.
    void Update(const Camera3D& camera, float aspect);

    // Falso apenas quando a caixa esta COMPLETAMENTE fora. Pode dar verdadeiro
    // para caixas fora (falso positivo perto das quinas), o que e inofensivo -
    // o erro proibido seria o contrario.
    bool Intersects(const BoundingBox& box) const;

private:
    // Cada plano guardado como (a, b, c, d) de ax + by + cz + d = 0, com a
    // normal apontando para DENTRO do tronco.
    Vector4 planes_[6]{};
};

}  // namespace render
