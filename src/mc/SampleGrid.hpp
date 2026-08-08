#pragma once

#include <cstddef>
#include <vector>

#include "field/ScalarField.hpp"
#include "raylib.h"

namespace mc {

// Grade regular de amostras do campo escalar, com o gradiente de cada ponto.
//
// Duas decisoes que valem explicar:
//
// 1. Amostrar POR PONTO, nao por celula. Celulas vizinhas compartilham cantos;
//    avaliar os 8 cantos de cada celula independentemente avaliaria o campo
//    ~8x mais que o necessario (884k contra 118k chamadas numa grade 48^3).
//
// 2. Guardar o gradiente aqui. Ele sai por diferencas centrais sobre os
//    valores JA amostrados - custo de leitura de array, sem nenhuma avaliacao
//    extra do campo. Com isso o poligonizador interpola a normal na aresta do
//    mesmo jeito que interpola a posicao, e nao precisa conhecer o campo:
//    Polygonize() recebe so a grade. O campo existe apenas nesta classe.
class SampleGrid {
public:
    SampleGrid(Vector3 min, Vector3 max, int resolution);

    // Redimensiona a grade. Invalida as amostras: chame SampleField depois.
    void SetResolution(int resolution);

    // Reposiciona a grade no espaco sem realocar nada. E o que permite uma
    // unica grade de rascunho servir a geracao de todos os chunks: a memoria
    // da densidade nao precisa sobreviver a malha.
    void SetBounds(Vector3 min, Vector3 max);

    // Unico ponto do projeto que toca no campo. Templated para o Sample do
    // campo inlinar dentro do laco - com chamada virtual aqui o custo
    // dominaria o frame.
    template <field::ScalarField F>
    void SampleField(const F& field) {
        const int n = PointsPerAxis();
        for (int z = 0; z < n; ++z) {
            for (int y = 0; y < n; ++y) {
                for (int x = 0; x < n; ++x) {
                    values_[Index(x, y, z)] = field.Sample(PositionAt(x, y, z));
                }
            }
        }
        ComputeGradients();
    }

    // Variante para campos cujo relevo e uma funcao de (x, z) - o caso do
    // terreno. `columnFn(x, z)` roda UMA vez por coluna vertical e devolve o
    // que for caro e constante nela (tipicamente a altura da superficie);
    // `densityFn(coluna, posicao)` roda por ponto.
    //
    // Sem essa separacao, uma altura com 5 oitavas de ruido 2D seria
    // recalculada uma vez por ponto - 33x de trabalho redundante numa grade
    // de 33 pontos de altura.
    template <class ColumnFn, class DensityFn>
    void SampleColumns(ColumnFn&& columnFn, DensityFn&& densityFn) {
        const int n = PointsPerAxis();
        for (int z = 0; z < n; ++z) {
            for (int x = 0; x < n; ++x) {
                const Vector3 base = PositionAt(x, 0, z);
                const auto column = columnFn(base.x, base.z);
                for (int y = 0; y < n; ++y) {
                    const Vector3 p{base.x, min_.y + static_cast<float>(y) * cell_.y,
                                    base.z};
                    values_[Index(x, y, z)] = densityFn(column, p);
                }
            }
        }
        ComputeGradients();
    }

    // Preenche a grade inteira com um valor constante e zera os gradientes.
    // Atalho para chunks inteiramente solidos ou inteiramente vazios, que nao
    // precisam de nenhuma avaliacao de campo.
    void FillConstant(float value);

    int Resolution() const { return resolution_; }
    int PointsPerAxis() const { return resolution_ + 1; }
    Vector3 Min() const { return min_; }
    Vector3 Max() const { return max_; }
    Vector3 CellSize() const { return cell_; }

    float ValueAt(int x, int y, int z) const { return values_[Index(x, y, z)]; }
    Vector3 GradientAt(int x, int y, int z) const {
        return gradients_[Index(x, y, z)];
    }

    Vector3 PositionAt(int x, int y, int z) const {
        return {
            min_.x + static_cast<float>(x) * cell_.x,
            min_.y + static_cast<float>(y) * cell_.y,
            min_.z + static_cast<float>(z) * cell_.z,
        };
    }

private:
    std::size_t Index(int x, int y, int z) const {
        const int n = PointsPerAxis();
        return static_cast<std::size_t>((z * n + y) * n + x);
    }

    void Allocate();
    void ComputeGradients();

    Vector3 min_;
    Vector3 max_;
    Vector3 cell_;
    int resolution_;
    std::vector<float> values_;
    std::vector<Vector3> gradients_;
};

}  // namespace mc
