#pragma once

#include <string>

#include "raylib.h"
#include "world/TerrainGenerator.hpp"

namespace ui {

// Painel de ajuste do terreno, gerado a partir de world/TerrainSettingsSchema.
// Nao ha lista de sliders escrita a mao: acrescentar um campo na tabela do
// schema faz o slider aparecer aqui sozinho.
class SettingsPanel {
public:
    struct Result {
        bool changed = false;      // algum valor mexeu neste frame
        bool released = false;     // usuario soltou o controle (hora de aplicar)
        bool saveRequested = false;
        bool loadRequested = false;
        bool resetRequested = false;
        bool regenerateRequested = false;
        bool mouseOverPanel = false;
    };

    // Desenha e edita `settings` no lugar.
    Result Draw(world::TerrainSettings& settings, const char* statusText);

    bool visible = false;

private:
    Vector2 scroll_{0.0f, 0.0f};
    // Guarda se algum controle estava sendo arrastado no frame anterior, para
    // detectar a soltura. Regenerar o mundo a cada pixel de arrasto travaria a
    // janela; regenerar so ao soltar mantem o slider fluido.
    bool dragging_ = false;
};

}  // namespace ui
