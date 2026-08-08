#include "app/Application.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "raymath.h"
#include "world/TerrainSettingsSchema.hpp"

namespace app {
namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;

constexpr float kMouseSensitivity = 0.0022f;
constexpr float kMinSpeed = 4.0f;
constexpr float kMaxSpeed = 400.0f;

// Alcance da ferramenta, em unidades de mundo.
constexpr float kReachDistance = 90.0f;
// Intervalo entre escavacoes com o botao segurado.
constexpr float kDigInterval = 0.06f;

}  // namespace

std::string Options::Usage() {
    return "uso: CppGame [opcoes]\n"
           "  --radius N         raio de visao em chunks (padrao 5)\n"
           "  --seed N           semente do mundo (padrao 1337)\n"
           "  --frames N         sai depois de N frames (verificacao)\n"
           "  --screenshot PATH  exporta PNG do ultimo frame\n";
}

Options Options::Parse(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        const bool hasValue = (i + 1 < argc);

        if (std::strcmp(arg, "--radius") == 0 && hasValue) {
            options.viewRadius = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--seed") == 0 && hasValue) {
            options.seed =
                static_cast<unsigned int>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(arg, "--frames") == 0 && hasValue) {
            options.exitAfterFrames = std::atoi(argv[++i]);
        } else if (std::strcmp(arg, "--screenshot") == 0 && hasValue) {
            options.screenshotPath = argv[++i];
        }
    }
    options.viewRadius = std::clamp(options.viewRadius, 1, 16);
    return options;
}

namespace {

world::TerrainSettings MakeSettings(unsigned int seed,
                                    const std::string& settingsPath) {
    world::TerrainSettings settings;
    settings.seed = seed;
    // Se existir um arquivo salvo pelo painel, ele manda. Assim os valores
    // achados no slider sobrevivem ao fechar o programa.
    world::LoadSettings(settings, settingsPath);
    // Tudo o mais fica no padrao de world/TerrainGenerator.hpp, que e o mundo
    // plano. Para dar relevo, mexa la - nao aqui.
    return settings;
}

}  // namespace

Application::Application(const Options& options)
    : options_(options),
      // As flags TEM que chegar antes do InitWindow - o construtor da
      // raylib::Window chama SetConfigFlags primeiro, por isso elas vao aqui e
      // nao numa chamada depois, que nao teria efeito.
      //   RESIZABLE: permite arrastar a borda e maximizar.
      //   HIGHDPI:   sem isso a janela sai borrada em tela Retina.
      window_(kWindowWidth, kWindowHeight, "Mundo procedural - marching cubes",
              FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI),
      generator_(MakeSettings(options.seed, options.settingsPath)),
            // Deixa dois nucleos livres: a thread principal ainda desenha e o
      // driver de GL usa outra. Encher todos so aumenta a disputa.
      chunks_(generator_, options.viewRadius,
              std::max(1, static_cast<int>(
                              std::thread::hardware_concurrency()) - 2)) {
    window_.SetTargetFPS(60);

    chunks_.SetShader(shader_.Get());

    draft_ = generator_.Settings();
    view_.viewRadius = options.viewRadius;
    status_ = "TAB abre e fecha este painel";

    // Comeca acima do nivel do chao, olhando para o horizonte.
    // Acima do relevo tipico (o terreno vai de ~51 a ~254 m, com o mar em 62).
    // O valor anterior, 28, ficava ABAIXO do nivel do mar depois que a escala
    // do mundo passou para metros - o jogo abria enterrado.
    camera_.position = {0.0f, 210.0f, 0.0f};
    camera_.target = {90.0f, 160.0f, 90.0f};
    camera_.up = {0.0f, 1.0f, 0.0f};
    camera_.fovy = 70.0f;
    camera_.projection = CAMERA_PERSPECTIVE;

    DisableCursor();
}

// Camera livre no estilo de criativo do Minecraft: mouse olha, WASD anda no
// plano, espaco/shift sobe e desce. Escrita a mao em vez de UpdateCamera
// porque os modos prontos da raylib ou prendem no chao ou exigem arrastar o
// mouse com o botao pressionado.
void Application::HandleInput() {
    const float dt = GetFrameTime();

    // TAB abre o painel e solta o cursor junto: com o mouse capturado nao ha
    // como clicar num slider.
    if (IsKeyPressed(KEY_TAB)) {
        panel_.visible = !panel_.visible;
        cursorCaptured_ = !panel_.visible;
        if (cursorCaptured_) {
            DisableCursor();
        } else {
            EnableCursor();
        }
    }

    // ---- orientacao ------------------------------------------------------
    Vector3 forward = Vector3Normalize(
        Vector3Subtract(camera_.target, camera_.position));

    if (cursorCaptured_) {
        const Vector2 delta = GetMouseDelta();

        // Yaw em torno do eixo vertical do mundo, nao do eixo local: sem isso
        // a camera adquire roll e o horizonte entorta.
        forward = Vector3RotateByAxisAngle(forward, Vector3{0.0f, 1.0f, 0.0f},
                                           -delta.x * kMouseSensitivity);

        const Vector3 right =
            Vector3Normalize(Vector3CrossProduct(forward, Vector3{0, 1, 0}));
        const float pitch = -delta.y * kMouseSensitivity;

        // Trava antes do polo: passar de 90 graus inverteria a imagem e o
        // produto vetorial com o eixo vertical degeneraria.
        const float currentPitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
        const float limit = 1.5f;  // ~86 graus
        const float clamped =
            std::clamp(currentPitch + pitch, -limit, limit) - currentPitch;
        forward = Vector3RotateByAxisAngle(forward, right, clamped);
    }

    // ---- deslocamento ----------------------------------------------------
    if (IsKeyDown(KEY_LEFT_SHIFT)) moveSpeed_ *= 1.0f + 2.0f * dt;
    if (IsKeyDown(KEY_LEFT_CONTROL)) moveSpeed_ *= 1.0f - 2.0f * dt;
    moveSpeed_ = std::clamp(moveSpeed_, kMinSpeed, kMaxSpeed);

    const Vector3 flatForward = Vector3Normalize(
        Vector3{forward.x, 0.0f, forward.z});
    const Vector3 right =
        Vector3Normalize(Vector3CrossProduct(flatForward, Vector3{0, 1, 0}));

    Vector3 move{0.0f, 0.0f, 0.0f};
    if (IsKeyDown(KEY_W)) move = Vector3Add(move, flatForward);
    if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, flatForward);
    if (IsKeyDown(KEY_D)) move = Vector3Add(move, right);
    if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, right);
    if (IsKeyDown(KEY_SPACE)) move.y += 1.0f;
    if (IsKeyDown(KEY_LEFT_ALT)) move.y -= 1.0f;

    if (Vector3LengthSqr(move) > 0.0f) {
        move = Vector3Scale(Vector3Normalize(move), moveSpeed_ * dt);
        camera_.position = Vector3Add(camera_.position, move);
    }

    camera_.target = Vector3Add(camera_.position, forward);

    // ---- teclas de cena --------------------------------------------------
    if (IsKeyPressed(KEY_F)) wireframe_ = !wireframe_;

    // Tela cheia sem borda em vez de ToggleFullscreen: no macOS o modo
    // exclusivo troca a resolucao do monitor e costuma brigar com o gerenciador
    // de janelas; o sem borda so redimensiona para o tamanho da tela.
    if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();

    if (IsKeyPressed(KEY_UP)) chunks_.SetViewRadius(chunks_.ViewRadius() + 1);
    if (IsKeyPressed(KEY_DOWN)) chunks_.SetViewRadius(chunks_.ViewRadius() - 1);

    // Tamanho da ferramenta de escavacao.
    const float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        digRadius_ = std::clamp(digRadius_ + wheel * 0.5f, 1.0f, 12.0f);
    }

    // Regenera tudo do zero - util depois de mexer nas equacoes e recompilar.
    if (IsKeyPressed(KEY_R)) chunks_.Invalidate();
    // Desfaz todas as escavacoes.
    if (IsKeyPressed(KEY_C)) chunks_.ClearEdits();

    HandleDigging();
}

// Escavar com marching cubes e somar densidade positiva numa esfera: o campo
// muda, a malha e refeita a partir dele, e a superficie nova ja nasce fechada.
// Nao existe caso especial para "buraco que atravessa a parede" ou "tunel que
// encontra caverna" - e tudo o mesmo somatorio.
void Application::HandleDigging() {
    const Vector3 direction =
        Vector3Subtract(camera_.target, camera_.position);

    // Mira: refeita todo frame para a esfera-fantasma acompanhar a camera.
    aimValid_ = chunks_.Raycast(camera_.position, direction, kReachDistance,
                                aimPoint_);

    digCooldown_ -= GetFrameTime();
    // Com o painel aberto o clique e da UI, nao da picareta.
    if (panel_.visible || digCooldown_ > 0.0f || !aimValid_) return;

    const bool dig = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const bool fill = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    if (!dig && !fill) return;

    // A intensidade acompanha o raio: para vazar uma esfera de raio R e
    // preciso empurrar a densidade em ~R, ja que perto da superficie ela varia
    // cerca de uma unidade por unidade de distancia.
    const float strength = digRadius_ * (dig ? 1.6f : -1.6f);

    // Ao preencher, o centro recua um pouco na direcao da camera: o material
    // novo encosta na superficie em vez de nascer enterrado nela.
    const Vector3 unit = Vector3Normalize(direction);
    const float offset = fill ? -digRadius_ * 0.5f : 0.0f;
    const Vector3 center = Vector3Add(
        aimPoint_, Vector3Scale(unit, offset));

    chunks_.ApplyEdit(world::SphereEdit{center, digRadius_, strength});
    digCooldown_ = kDigInterval;
}

// Aplica o rascunho no gerador e reconstroi o mundo. Custa dezenas de
// milissegundos com raio grande, por isso so e chamado quando o usuario solta
// um controle - nunca durante o arrasto.
void Application::ApplySettings() {
    generator_.SetSettings(draft_);
    // Invalidate preserva as escavacoes: elas sao guardadas por posicao no
    // mundo, nao dentro do chunk, entao sobrevivem a troca de terreno.
    chunks_.Invalidate();
}

void Application::DrawSettingsPanel() {
    const ui::SettingsPanel::Result result =
        panel_.Draw(draft_, view_, status_.c_str());

    // Mudanca de visualizacao nao regera nada: so ajusta quantos chunks ficam
    // carregados e onde a neblina fecha.
    if (result.viewChanged) {
        chunks_.SetViewRadius(view_.viewRadius);
        status_ = "distancia ajustada";
    }

    if (result.released) {
        ApplySettings();
        status_ = "aplicado";
    }

    if (result.regenerateRequested) {
        ApplySettings();
        status_ = "regerado";
    }

    if (result.resetRequested) {
        const std::uint32_t seed = draft_.seed;
        draft_ = world::TerrainSettings{};
        draft_.seed = seed;  // a semente nao faz parte do "padrao de forma"
        ApplySettings();
        status_ = "restaurado o padrao";
    }

    if (result.saveRequested) {
        status_ = world::SaveSettings(draft_, options_.settingsPath)
                      ? "salvo em " + options_.settingsPath
                      : "FALHA ao salvar em " + options_.settingsPath;
    }

    if (result.loadRequested) {
        if (world::LoadSettings(draft_, options_.settingsPath)) {
            ApplySettings();
            status_ = "carregado de " + options_.settingsPath;
        } else {
            status_ = "nao achei " + options_.settingsPath;
        }
    }
}

// Lamina d'agua: um unico quad acompanhando a camera em XZ, grande o bastante
// para alcancar a neblina. Nao usa o shader do terreno - agua chapada e
// justamente o visual pretendido, e assim ela nao precisa de normais.
void Application::DrawWater() const {
    const float sea = generator_.Settings().seaLevel;
    const float extent =
        static_cast<float>(chunks_.ViewRadius()) * world::kChunkSize * 2.2f;

    DrawPlane(Vector3{camera_.position.x, sea, camera_.position.z},
              Vector2{extent, extent}, Color{58, 110, 165, 205});
}

void Application::DrawHud() const {
    const world::ChunkStats& stats = chunks_.Stats();
    const world::TerrainSettings& settings = generator_.Settings();
    const world::Biome& biome =
        generator_.PickBiome(camera_.position.x, camera_.position.z);

    DrawRectangle(8, 8, 340, 196, Fade(BLACK, 0.55f));
    DrawRectangleLines(8, 8, 340, 196, Fade(LIGHTGRAY, 0.35f));

    int y = 16;
    const auto line = [&y](const char* text, Color color) {
        DrawText(text, 18, y, 18, color);
        y += 21;
    };

    line(TextFormat("%d FPS", GetFPS()), LIME);
    line(TextFormat("pos  %.0f  %.0f  %.0f", camera_.position.x,
                    camera_.position.y, camera_.position.z),
         RAYWHITE);
    line(TextFormat("bioma: %s", biome.name), YELLOW);
    line(TextFormat("chunks: %d  (%d com malha)  raio %d", stats.loaded,
                    stats.withGeometry, chunks_.ViewRadius()),
         RAYWHITE);
    line(TextFormat("desenhados: %d   descartados: %d", stats.drawn,
                    stats.culled),
         SKYBLUE);
    line(TextFormat("fila: %d   em voo: %d   threads: %d", stats.pending,
                    stats.inFlight, stats.threads),
         stats.pending > 0 ? ORANGE : GRAY);
    line(TextFormat("triangulos: %d", stats.triangles), SKYBLUE);
    line(TextFormat("colina: %.0f   montanha: %.0f   caverna: %.2f",
                    settings.hillHeight, settings.mountainHeight,
                    settings.caveStrength),
         GRAY);
    line(TextFormat("escavacoes: %d   ferramenta: %.1f",
                    static_cast<int>(chunks_.EditCount()), digRadius_),
         GRAY);
    line(TextFormat("velocidade: %.0f", moveSpeed_), GRAY);

    // Mira no centro da tela.
    const int cx = GetScreenWidth() / 2;
    const int cy = GetScreenHeight() / 2;
    const Color aimColor = aimValid_ ? Color{255, 255, 255, 220}
                                     : Color{255, 255, 255, 70};
    DrawLine(cx - 9, cy, cx - 3, cy, aimColor);
    DrawLine(cx + 3, cy, cx + 9, cy, aimColor);
    DrawLine(cx, cy - 9, cx, cy - 3, aimColor);
    DrawLine(cx, cy + 3, cx, cy + 9, aimColor);

    DrawText("BOTAO ESQ escavar   BOTAO DIR preencher   RODA tamanho   "
             "WASD mover   SPACE/ALT subir-descer   F wireframe   C limpar",
             18, GetScreenHeight() - 28, 17, Fade(RAYWHITE, 0.75f));
}

void Application::Run() {
    int frame = 0;

    while (!window_.ShouldClose()) {
        HandleInput();

        chunks_.Update(camera_.position);
        shader_.SetViewPosition(camera_.position);

        // A neblina tem de fechar um pouco ALEM do ultimo chunk carregado,
        // senao a borda da regiao aparece como recorte reto no horizonte.
        if (view_.fogFollowsRadius) {
            view_.fogDistance = static_cast<float>(chunks_.ViewRadius()) *
                                world::kChunkSize * 1.15f;
        }
        shader_.SetFogDistance(view_.fogDistance);

        BeginDrawing();
        // Mesma cor da neblina do shader: o terreno distante dissolve no ceu
        // em vez de terminar num recorte.
        ClearBackground(Color{158, 189, 230, 255});

        BeginMode3D(camera_);
        chunks_.Draw(camera_, WHITE, wireframe_);

        // Esfera-fantasma do alvo, para o jogador ver onde a escavacao vai
        // cair antes de clicar.
        if (aimValid_) {
            DrawSphereWires(aimPoint_, digRadius_, 8, 8,
                            Color{255, 255, 255, 90});
        }

        // Depois do terreno: e translucida, entao precisa do z-buffer ja
        // preenchido para compor certo com o fundo do mar.
        DrawWater();
        EndMode3D();

        DrawHud();
        DrawSettingsPanel();
        EndDrawing();

        ++frame;

        if (options_.exitAfterFrames > 0 && frame >= options_.exitAfterFrames) {
            if (!options_.screenshotPath.empty()) {
                // LoadImageFromScreen + ExportImage em vez de TakeScreenshot:
                // TakeScreenshot prefixa o nome com o basePath interno da
                // raylib e quebra com caminho absoluto.
                Image shot = LoadImageFromScreen();
                ExportImage(shot, options_.screenshotPath.c_str());
                UnloadImage(shot);
            }
            break;
        }
    }
}

}  // namespace app
