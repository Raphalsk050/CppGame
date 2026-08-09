#include "app/PlayerController.hpp"

#include <algorithm>
#include <cmath>

namespace app {
namespace {

constexpr float kGravity = -26.0f;      // m/s^2, exagerado como no Minecraft
constexpr float kJumpSpeed = 8.4f;      // ~1,25 m de altura de pulo
constexpr float kWalkSpeed = 4.3f;      // m/s, a velocidade de caminhada
constexpr float kSprintSpeed = 7.2f;
constexpr float kFlySpeed = 40.0f;
constexpr float kTerminalVelocity = -78.0f;

// Passo da varredura de colisão. Menor que o raio do jogador, senão um passo
// pode pular inteiramente por dentro de uma parede fina.
constexpr float kSweepStep = 0.15f;

}  // namespace

void PlayerController::SetMode(MoveMode mode) {
    mode_ = mode;
    velocity_ = {0.0f, 0.0f, 0.0f};
    onGround_ = false;
}

void PlayerController::ToggleMode() {
    SetMode(mode_ == MoveMode::Walk ? MoveMode::Spectator : MoveMode::Walk);
}

bool PlayerController::Solid(const world::ChunkManager& world,
                             Vector3 point) const {
    // Densidade negativa é sólido — a convenção do projeto inteiro.
    return world.DensityAt(point) < 0.0f;
}

// O jogador é aproximado por um cilindro. Testar só o eixo central deixaria o
// corpo entrar na parede até o centro encostar; os quatro pontos na borda
// fazem o raio valer.
bool PlayerController::Blocked(const world::ChunkManager& world,
                               Vector3 feet) const {
    const float r = kPlayerRadius * 0.85f;
    const float offsets[5][2] = {
        {0.0f, 0.0f}, {r, 0.0f}, {-r, 0.0f}, {0.0f, r}, {0.0f, -r},
    };
    // Três alturas: pés, cintura e cabeça. Só os pés deixaria a cabeça
    // atravessar um teto baixo.
    const float heights[3] = {0.15f, kPlayerHeight * 0.5f,
                              kPlayerHeight - 0.15f};

    for (const auto& o : offsets) {
        for (const float h : heights) {
            if (Solid(world, Vector3{feet.x + o[0], feet.y + h, feet.z + o[1]})) {
                return true;
            }
        }
    }
    return false;
}

// Avança num eixo em passos curtos e devolve o quanto conseguiu andar.
//
// VARREDURA, não teste pontual: a 40 m/s um frame de 16 ms desloca 0,64 m, o
// suficiente para começar de um lado de uma parede e terminar do outro sem que
// nenhum teste tenha visto o sólido no meio.
float PlayerController::SweepAxis(const world::ChunkManager& world,
                                  Vector3 from, float delta, int axis) const {
    if (delta == 0.0f) return 0.0f;

    const float direction = (delta > 0.0f) ? 1.0f : -1.0f;
    float remaining = std::fabs(delta);
    float moved = 0.0f;

    while (remaining > 0.0f) {
        const float step = std::min(kSweepStep, remaining);
        Vector3 next = from;
        (&next.x)[axis] += direction * (moved + step);

        if (Blocked(world, next)) break;

        moved += step;
        remaining -= step;
    }

    return moved * direction;
}

void PlayerController::Update(const world::ChunkManager& world, Vector3 wish,
                              bool jump, bool sprint, float dt) {
    dt = std::min(dt, 0.05f);  // um frame longo não pode teleportar o jogador

    if (mode_ == MoveMode::Spectator) {
        // Atravessa tudo, sem gravidade: é a câmera de inspeção do terreno.
        const float speed = kFlySpeed * (sprint ? 3.0f : 1.0f);
        position_.x += wish.x * speed * dt;
        position_.y += wish.y * speed * dt;
        position_.z += wish.z * speed * dt;
        velocity_ = {0.0f, 0.0f, 0.0f};
        onGround_ = false;
        return;
    }

    // ---- caminhada -------------------------------------------------------
    const float speed = sprint ? kSprintSpeed : kWalkSpeed;
    velocity_.x = wish.x * speed;
    velocity_.z = wish.z * speed;

    if (jump && onGround_) velocity_.y = kJumpSpeed;
    velocity_.y = std::max(velocity_.y + kGravity * dt, kTerminalVelocity);

    // Horizontal primeiro, um eixo de cada vez: assim esbarrar numa parede em
    // X não cancela o movimento em Z, e o jogador desliza ao longo dela em vez
    // de grudar.
    const float dx = SweepAxis(world, position_, velocity_.x * dt, 0);
    position_.x += dx;
    const float dz = SweepAxis(world, position_, velocity_.z * dt, 2);
    position_.z += dz;

    // Degrau: se travou na horizontal mas há espaço livre um pouco acima,
    // sobe. É o que permite andar por terreno irregular sem pular a cada pedra.
    const bool travouX = std::fabs(dx) < std::fabs(velocity_.x * dt) * 0.5f;
    const bool travouZ = std::fabs(dz) < std::fabs(velocity_.z * dt) * 0.5f;
    if (onGround_ && (travouX || travouZ)) {
        const Vector3 acima{position_.x, position_.y + kStepHeight, position_.z};
        if (!Blocked(world, acima)) {
            const float sx = SweepAxis(world, acima, velocity_.x * dt, 0);
            const float sz = SweepAxis(world, acima, velocity_.z * dt, 2);
            if (std::fabs(sx) > std::fabs(dx) || std::fabs(sz) > std::fabs(dz)) {
                position_ = Vector3{acima.x + sx, acima.y, acima.z + sz};
            }
        }
    }

    // Vertical.
    const float dy = SweepAxis(world, position_, velocity_.y * dt, 1);
    position_.y += dy;

    // Bateu descendo = está no chão. Bateu subindo = teto, zera a subida para
    // não ficar colado nele.
    const bool bateu = std::fabs(dy) < std::fabs(velocity_.y * dt) * 0.5f;
    if (bateu) {
        onGround_ = (velocity_.y < 0.0f);
        velocity_.y = 0.0f;
    } else {
        onGround_ = false;
    }

    // Rede de segurança: se acordou dentro do sólido (terreno regenerado sob os
    // pés, ou spawn ruim), sobe até sair. Sem isto o jogador fica preso para
    // sempre, porque toda direção está bloqueada.
    if (Blocked(world, position_)) {
        for (int i = 0; i < 400 && Blocked(world, position_); ++i) {
            position_.y += 0.25f;
        }
        velocity_ = {0.0f, 0.0f, 0.0f};
    }
}

}  // namespace app
