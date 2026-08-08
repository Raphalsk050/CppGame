#include "render/Frustum.hpp"

#include <cmath>

#include "raymath.h"
#include "rlgl.h"

namespace render {
namespace {

Vector4 NormalizePlane(Vector4 plane) {
    const float length = std::sqrt(plane.x * plane.x + plane.y * plane.y +
                                   plane.z * plane.z);
    if (length <= 0.0f) return plane;
    return Vector4{plane.x / length, plane.y / length, plane.z / length,
                   plane.w / length};
}

}  // namespace

void Frustum::Update(const Camera3D& camera, float aspect) {
    // Reproduz exatamente o que a raylib monta em BeginMode3D. Se a projecao
    // aqui divergir da usada no desenho, o descarte tira chunks que estavam
    // visiveis - buracos piscando nas bordas da tela.
    const Matrix projection =
        MatrixPerspective(camera.fovy * DEG2RAD, aspect, rlGetCullDistanceNear(),
                          rlGetCullDistanceFar());
    const Matrix view =
        MatrixLookAt(camera.position, camera.target, camera.up);

    // MatrixMultiply(a, b) da raylib resulta em b*a na notacao matematica,
    // entao isto e projection * view - o MVP para vetor-coluna, clip = M*v.
    const Matrix m = MatrixMultiply(view, projection);

    // Extracao de Gribb-Hartmann, direto da condicao de clipping
    // -w <= x,y,z <= w: cada plano e a LINHA do eixo somada ou subtraida da
    // linha de w.
    //
    // ATENCAO ao layout: a Matrix da raylib guarda M[linha][coluna] em
    // m[coluna*4 + linha]. A linha 0 e portanto (m0, m4, m8, m12) e a linha 3 e
    // (m3, m7, m11, m15) - nao (m0,m1,m2,m3). Combinar os campos na ordem
    // errada monta o frustum transposto, que ainda parece plausivel (descarta
    // o que esta atras) mas troca os planos de perto e longe.
    const float rx[4] = {m.m0, m.m4, m.m8, m.m12};   // linha 0
    const float ry[4] = {m.m1, m.m5, m.m9, m.m13};   // linha 1
    const float rz[4] = {m.m2, m.m6, m.m10, m.m14};  // linha 2
    const float rw[4] = {m.m3, m.m7, m.m11, m.m15};  // linha 3

    const auto add = [](const float a[4], const float b[4]) {
        return Vector4{a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]};
    };
    const auto sub = [](const float a[4], const float b[4]) {
        return Vector4{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]};
    };

    planes_[0] = NormalizePlane(add(rw, rx));  // esquerda: w + x >= 0
    planes_[1] = NormalizePlane(sub(rw, rx));  // direita:  w - x >= 0
    planes_[2] = NormalizePlane(add(rw, ry));  // baixo:    w + y >= 0
    planes_[3] = NormalizePlane(sub(rw, ry));  // cima:     w - y >= 0
    planes_[4] = NormalizePlane(add(rw, rz));  // perto:    w + z >= 0
    planes_[5] = NormalizePlane(sub(rw, rz));  // longe:    w - z >= 0
}

bool Frustum::Intersects(const BoundingBox& box) const {
    for (const Vector4& plane : planes_) {
        // "Vertice positivo": o canto da caixa mais avancado na direcao da
        // normal do plano. Se ELE esta atras do plano, todos os outros sete
        // tambem estao, e a caixa inteira esta fora. Testar so este canto
        // resolve a caixa toda com um produto escalar.
        const Vector3 positive{
            (plane.x >= 0.0f) ? box.max.x : box.min.x,
            (plane.y >= 0.0f) ? box.max.y : box.min.y,
            (plane.z >= 0.0f) ? box.max.z : box.min.z,
        };

        if (plane.x * positive.x + plane.y * positive.y +
                plane.z * positive.z + plane.w <
            0.0f) {
            return false;
        }
    }
    return true;
}

}  // namespace render
