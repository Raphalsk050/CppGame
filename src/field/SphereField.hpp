#pragma once

#include "field/ScalarField.hpp"
#include "raylib.h"
#include "raymath.h"

namespace field {

// Esfera como SDF puro: distancia ate a casca, negativa dentro.
//
// Este e o campo de referencia do projeto. Ele tem resultado analitico
// conhecido, entao serve para conferir se o poligonizador esta correto antes
// de culpar um campo mais complicado: a isosuperficie em iso = 0 tem que ser
// uma esfera lisa de raio `radius`, iluminada por fora.
class SphereField {
public:
    explicit SphereField(float radius = 1.25f) : radius_(radius) {}

    float Sample(Vector3 p) const { return Vector3Length(p) - radius_; }

    float Radius() const { return radius_; }
    void SetRadius(float radius) { radius_ = radius; }

private:
    float radius_;
};

static_assert(ScalarField<SphereField>);

}  // namespace field
