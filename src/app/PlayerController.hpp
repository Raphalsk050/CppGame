#pragma once

#include "raylib.h"
#include "world/ChunkManager.hpp"

namespace app {

// Dimensões do jogador, em METROS — a mesma régua do mundo (ver a decisão de
// escala em PROGRESS.md). São as do Minecraft, que é a referência do projeto.
inline constexpr float kPlayerHeight = 1.8f;
inline constexpr float kPlayerEyeHeight = 1.62f;
inline constexpr float kPlayerRadius = 0.3f;
// Altura que o jogador sobe sem pular, para não travar em cada pedra de 1 m.
inline constexpr float kStepHeight = 0.6f;

enum class MoveMode {
    Walk,       // gravidade, colisão, pulo
    Spectator,  // voo livre atravessando tudo
};

// Movimento e colisão do jogador.
//
// A colisão testa o CAMPO ESCALAR, não a malha. A malha está na GPU e fatiada
// em chunks que aparecem e somem com o streaming; o campo é uma função
// consultável em qualquer ponto do mundo, inclusive onde nenhum chunk foi
// gerado ainda. Testar contra a malha exigiria trazer geometria de volta para
// a CPU e lidar com o chunk que ainda não chegou.
class PlayerController {
public:
    void SetPosition(Vector3 position) { position_ = position; }
    Vector3 Position() const { return position_; }
    Vector3 EyePosition() const {
        return {position_.x, position_.y + kPlayerEyeHeight, position_.z};
    }

    MoveMode Mode() const { return mode_; }
    void SetMode(MoveMode mode);
    void ToggleMode();

    bool OnGround() const { return onGround_; }
    Vector3 Velocity() const { return velocity_; }
    bool IsSubmerged(float seaLevel) const {
        return EyePosition().y < seaLevel;
    }

    // `wish` é a direção desejada em espaço de mundo, já normalizada.
    // `jump` só tem efeito com os pés no chão.
    void Update(const world::ChunkManager& world, Vector3 wish, bool jump,
                bool sprint, float dt);

private:
    bool Solid(const world::ChunkManager& world, Vector3 point) const;
    bool Blocked(const world::ChunkManager& world, Vector3 feet) const;
    // Move num eixo com varredura em passos curtos. Passo longo demais
    // atravessa parede fina entre dois frames.
    float SweepAxis(const world::ChunkManager& world, Vector3 from, float delta,
                    int axis) const;

    Vector3 position_{0.0f, 0.0f, 0.0f};  // nos PÉS, não nos olhos
    Vector3 velocity_{0.0f, 0.0f, 0.0f};
    MoveMode mode_ = MoveMode::Spectator;
    bool onGround_ = false;
};

}  // namespace app
