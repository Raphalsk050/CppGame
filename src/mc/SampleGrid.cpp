#include "mc/SampleGrid.hpp"

#include <algorithm>

namespace mc {
namespace {

constexpr int kMinResolution = 2;
constexpr int kMaxResolution = 128;

}  // namespace

SampleGrid::SampleGrid(Vector3 min, Vector3 max, int resolution)
    : min_(min), max_(max), resolution_(std::clamp(resolution, kMinResolution,
                                                   kMaxResolution)) {
    Allocate();
}

void SampleGrid::SetResolution(int resolution) {
    resolution = std::clamp(resolution, kMinResolution, kMaxResolution);
    if (resolution == resolution_) return;
    resolution_ = resolution;
    Allocate();
}

void SampleGrid::SetBounds(Vector3 min, Vector3 max) {
    min_ = min;
    max_ = max;
    cell_ = {
        (max_.x - min_.x) / static_cast<float>(resolution_),
        (max_.y - min_.y) / static_cast<float>(resolution_),
        (max_.z - min_.z) / static_cast<float>(resolution_),
    };
}

void SampleGrid::Allocate() {
    const int n = PointsPerAxis();
    cell_ = {
        (max_.x - min_.x) / static_cast<float>(resolution_),
        (max_.y - min_.y) / static_cast<float>(resolution_),
        (max_.z - min_.z) / static_cast<float>(resolution_),
    };
    const std::size_t count = static_cast<std::size_t>(n) * n * n;
    values_.assign(count, 0.0f);
    gradients_.assign(count, Vector3{0.0f, 0.0f, 0.0f});
}

void SampleGrid::FillConstant(float value) {
    std::fill(values_.begin(), values_.end(), value);
    std::fill(gradients_.begin(), gradients_.end(), Vector3{0.0f, 0.0f, 0.0f});
}

// Diferencas centrais sobre os valores ja amostrados. Nas bordas da grade cai
// para diferenca lateral, usando o proprio ponto como vizinho ausente - dai o
// divisor variavel (2 celulas no interior, 1 na borda).
//
// O gradiente aponta na direcao de CRESCIMENTO do campo. Como a convencao do
// projeto e "dentro = valor menor" (ver field/ScalarField.hpp), ele ja aponta
// para fora do solido e serve como normal sem troca de sinal.
void SampleGrid::ComputeGradients() {
    const int n = PointsPerAxis();

    for (int z = 0; z < n; ++z) {
        const int zm = (z > 0) ? z - 1 : z;
        const int zp = (z < n - 1) ? z + 1 : z;
        const float dzScale =
            (zp > zm) ? 1.0f / (static_cast<float>(zp - zm) * cell_.z) : 0.0f;

        for (int y = 0; y < n; ++y) {
            const int ym = (y > 0) ? y - 1 : y;
            const int yp = (y < n - 1) ? y + 1 : y;
            const float dyScale =
                (yp > ym) ? 1.0f / (static_cast<float>(yp - ym) * cell_.y)
                          : 0.0f;

            for (int x = 0; x < n; ++x) {
                const int xm = (x > 0) ? x - 1 : x;
                const int xp = (x < n - 1) ? x + 1 : x;
                const float dxScale =
                    (xp > xm) ? 1.0f / (static_cast<float>(xp - xm) * cell_.x)
                              : 0.0f;

                gradients_[Index(x, y, z)] = {
                    (values_[Index(xp, y, z)] - values_[Index(xm, y, z)]) *
                        dxScale,
                    (values_[Index(x, yp, z)] - values_[Index(x, ym, z)]) *
                        dyScale,
                    (values_[Index(x, y, zp)] - values_[Index(x, y, zm)]) *
                        dzScale,
                };
            }
        }
    }
}

}  // namespace mc
