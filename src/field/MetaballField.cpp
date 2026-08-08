#include "field/MetaballField.hpp"

#include <algorithm>
#include <cmath>

namespace field {
namespace {

constexpr int kMinBalls = 1;
constexpr int kMaxBalls = 16;

}  // namespace

MetaballField::MetaballField(int count) { Rebuild(count); }

void MetaballField::SetBallCount(int count) {
    count = std::clamp(count, kMinBalls, kMaxBalls);
    if (count != BallCount()) Rebuild(count);
}

void MetaballField::Rebuild(int count) {
    count = std::clamp(count, kMinBalls, kMaxBalls);
    balls_.clear();
    balls_.reserve(static_cast<std::size_t>(count));

    // Parametros deterministicos: mesma cena a cada execucao, o que torna
    // qualquer regressao visual reproduzivel. Os multiplos irracionais
    // espalham as fases para as bolas nao orbitarem em sincronia.
    for (int i = 0; i < count; ++i) {
        const float t = static_cast<float>(i);
        Ball ball{};
        ball.strength = 0.28f + 0.10f * std::sin(t * 1.7f);
        ball.orbit = 0.85f + 0.25f * std::cos(t * 2.3f);
        ball.frequency = {0.70f + 0.13f * t, 0.55f + 0.17f * t,
                          0.61f + 0.11f * t};
        ball.phase = {t * 1.618f, t * 2.399f, t * 0.917f};
        ball.center = {0.0f, 0.0f, 0.0f};
        balls_.push_back(ball);
    }

    Update(0.0f);
}

void MetaballField::Update(float time) {
    for (Ball& b : balls_) {
        b.center = {
            b.orbit * std::sin(time * b.frequency.x + b.phase.x),
            b.orbit * std::sin(time * b.frequency.y + b.phase.y),
            b.orbit * std::sin(time * b.frequency.z + b.phase.z),
        };
    }
}

}  // namespace field
