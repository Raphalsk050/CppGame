#pragma once

#include <string>

#include "raylib-cpp.hpp"
#include "render/SurfaceShader.hpp"
#include "ui/SettingsPanel.hpp"
#include "world/ChunkManager.hpp"
#include "world/TerrainGenerator.hpp"

namespace app {

struct Options {
    int viewRadius = 30;
    unsigned int seed = 1337u;

    // Modo de verificacao automatica: roda N frames, opcionalmente exporta um
    // PNG do ultimo, e sai. Permite checar que a cena de fato renderiza sem
    // depender de alguem olhando a janela.
    int exitAfterFrames = -1;
    std::string screenshotPath;

    // Arquivo de configuracao do terreno. Carregado no startup se existir, e
    // destino do botao Salvar.
    std::string settingsPath = "terrain.cfg";

    static Options Parse(int argc, char** argv);
    static std::string Usage();
};

class Application {
public:
    explicit Application(const Options& options);

    void Run();

private:
    void HandleInput();
    void HandleDigging();
    void DrawSettingsPanel();
    void ApplySettings();
    void DrawWater() const;
    void DrawHud() const;

    Options options_;

    // A janela cria o contexto de GL, entao vem antes de tudo que aloca
    // recurso de GPU - a ordem inversa de destruicao garante que shader e
    // chunks soltem seus recursos antes de CloseWindow.
    raylib::Window window_;

    render::SurfaceShader shader_;
    Camera3D camera_{};

    world::TerrainGenerator generator_;
    world::ChunkManager chunks_;

    ui::SettingsPanel panel_;
    // Copia editavel. O gerador so recebe os valores quando o usuario solta o
    // controle - ver o comentario de deteccao de soltura em SettingsPanel.
    world::TerrainSettings draft_{};
    ui::ViewSettings view_{};
    std::string status_;

    bool wireframe_ = false;
    bool cursorCaptured_ = true;
    float moveSpeed_ = 24.0f;

    // Escavacao. O raio e do jogador; o cooldown existe para segurar o botao
    // nao gerar uma edicao por frame - seriam 60 esferas por segundo no
    // acervo, cada uma remalhando os chunks vizinhos.
    float digRadius_ = 3.5f;
    float digCooldown_ = 0.0f;
    bool aimValid_ = false;
    Vector3 aimPoint_{};
};

}  // namespace app
