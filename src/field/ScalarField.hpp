#pragma once

#include <concepts>

#include "raylib.h"

namespace field {

// Contrato minimo de um campo escalar amostravel em R^3.
//
// CONVENCAO DE SINAL (vale para todo o projeto): valores ABAIXO do isolevel
// sao o interior do solido. E a mesma convencao das tabelas em mc/Tables.hpp,
// e e o que faz o gradiente apontar para fora - ou seja, servir direto como
// normal, e o winding dos triangulos sair coerente com o backface culling.
// Um campo cujo interior tem valor alto (metaballs) precisa ser negado na
// propria funcao Sample, nao no consumidor.
template <typename T>
concept ScalarField = requires(const T& f, Vector3 p) {
    { f.Sample(p) } -> std::convertible_to<float>;
};

}  // namespace field
