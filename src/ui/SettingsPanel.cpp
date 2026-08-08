#include "ui/SettingsPanel.hpp"

#include <cstring>

#include "world/TerrainSettingsSchema.hpp"

// Sem RAYGUI_IMPLEMENTATION: aqui so as declaracoes. O corpo da biblioteca e
// compilado como C em ui/raygui_impl.c - ver o comentario la para o motivo.
#include "raygui.h"

namespace ui {
namespace {

constexpr float kPanelWidth = 384.0f;
constexpr float kRowHeight = 22.0f;
constexpr float kRowGap = 4.0f;
constexpr float kSectionGap = 12.0f;
constexpr float kLabelWidth = 150.0f;
constexpr float kValueWidth = 62.0f;
constexpr float kPadding = 10.0f;

}  // namespace

SettingsPanel::Result SettingsPanel::Draw(world::TerrainSettings& settings,
                                          ViewSettings& view,
                                          const char* statusText) {
    Result result;
    if (!visible) {
        dragging_ = false;
        return result;
    }

    std::size_t floatCount = 0;
    const world::FloatField* floats = world::FloatFields(floatCount);
    std::size_t intCount = 0;
    const world::IntField* ints = world::IntFields(intCount);

    // ---- geometria -------------------------------------------------------
    const float screenH = static_cast<float>(GetScreenHeight());
    const Rectangle panel{8.0f, 8.0f, kPanelWidth, screenH - 16.0f};

    // Altura do conteudo: uma linha por campo mais um cabecalho por secao.
    int sectionCount = 0;
    const char* section = nullptr;
    for (std::size_t i = 0; i < floatCount; ++i) {
        if (section == nullptr || std::strcmp(section, floats[i].section) != 0) {
            section = floats[i].section;
            ++sectionCount;
        }
    }
    // +4 linhas e +1 secao para o bloco de visualizacao desenhado a mao.
    const float contentH =
        (static_cast<float>(floatCount + intCount + 4) *
         (kRowHeight + kRowGap)) +
        (static_cast<float>(sectionCount + 1) * kSectionGap) + 60.0f;
    const Rectangle content{0.0f, 0.0f, panel.width - 20.0f, contentH};

    result.mouseOverPanel = CheckCollisionPointRec(GetMousePosition(), panel);

    // ---- moldura e botoes ------------------------------------------------
    GuiPanel(panel, "TERRENO");

    float y = panel.y + 30.0f;
    const float buttonW = (panel.width - kPadding * 2.0f - 12.0f) / 4.0f;
    float x = panel.x + kPadding;

    if (GuiButton(Rectangle{x, y, buttonW, 24.0f}, "Salvar")) {
        result.saveRequested = true;
    }
    x += buttonW + 4.0f;
    if (GuiButton(Rectangle{x, y, buttonW, 24.0f}, "Carregar")) {
        result.loadRequested = true;
    }
    x += buttonW + 4.0f;
    if (GuiButton(Rectangle{x, y, buttonW, 24.0f}, "Padrao")) {
        result.resetRequested = true;
    }
    x += buttonW + 4.0f;
    if (GuiButton(Rectangle{x, y, buttonW, 24.0f}, "Regerar")) {
        result.regenerateRequested = true;
    }

    y += 28.0f;

    // Semente: campo proprio porque e inteiro sem sinal e nao entra na tabela
    // de floats.
    int seed = static_cast<int>(settings.seed);
    const int previousSeed = seed;
    GuiLabel(Rectangle{panel.x + kPadding, y, kLabelWidth, kRowHeight},
             "semente");
    if (GuiSpinner(Rectangle{panel.x + kLabelWidth + kPadding, y,
                             panel.width - kLabelWidth - kPadding * 2.0f,
                             kRowHeight},
                   nullptr, &seed, 0, 999999, false)) {
        // nada: a comparacao abaixo cobre a mudanca
    }
    if (seed != previousSeed) {
        settings.seed = static_cast<std::uint32_t>(seed < 0 ? 0 : seed);
        result.changed = true;
        // Semente nova so faz sentido aplicada de uma vez.
        result.released = true;
    }

    y += kRowHeight + 6.0f;
    GuiLabel(Rectangle{panel.x + kPadding, y, panel.width - kPadding * 2.0f,
                       kRowHeight},
             statusText);
    y += kRowHeight + kPadding;

    // ---- lista rolavel ---------------------------------------------------
    // Os limites saem do `y` corrente, nao de uma constante de altura de
    // cabecalho: qualquer controle acrescentado acima empurra a lista sozinho.
    // Com um valor fixo, incluir uma linha no cabecalho fazia a lista subir por
    // cima dela e cortar o texto ao meio.
    const Rectangle listBounds{panel.x, y, panel.width,
                               panel.y + panel.height - y - kPadding};

    // `viewArea`, nao `view`: o parametro ViewSettings ja ocupa esse nome.
    Rectangle viewArea{0};
    GuiScrollPanel(listBounds, nullptr, content, &scroll_, &viewArea);

    BeginScissorMode(static_cast<int>(viewArea.x), static_cast<int>(viewArea.y),
                     static_cast<int>(viewArea.width),
                     static_cast<int>(viewArea.height));

    float rowY = listBounds.y + scroll_.y + kPadding;
    const float rowX = listBounds.x + kPadding;
    const float sliderW =
        content.width - kLabelWidth - kValueWidth - kPadding * 2.0f;

    // ---- visualizacao: primeiro, e fora da tabela do terreno -------------
    // Estes nao regeram nada, so mudam quanto do mundo aparece.
    GuiLabel(Rectangle{rowX, rowY, content.width, kRowHeight},
             "- Visualizacao -");
    rowY += kRowHeight;

    {
        GuiLabel(Rectangle{rowX, rowY, kLabelWidth, kRowHeight},
                 "distancia (chunks)");
        float radius = static_cast<float>(view.viewRadius);
        const float radiusBefore = radius;
        GuiSliderBar(Rectangle{rowX + kLabelWidth, rowY, sliderW, kRowHeight},
                     nullptr, nullptr, &radius, 2.0f, 24.0f);
        if (radius != radiusBefore) {
            view.viewRadius = static_cast<int>(radius + 0.5f);
            result.viewChanged = true;
        }
        GuiLabel(Rectangle{rowX + kLabelWidth + sliderW + 4.0f, rowY,
                           kValueWidth, kRowHeight},
                 TextFormat("%d", view.viewRadius));
        rowY += kRowHeight + kRowGap;

        GuiCheckBox(Rectangle{rowX, rowY, 16.0f, 16.0f},
                    " neblina acompanha a distancia", &view.fogFollowsRadius);
        rowY += kRowHeight + kRowGap;

        if (!view.fogFollowsRadius) {
            GuiLabel(Rectangle{rowX, rowY, kLabelWidth, kRowHeight},
                     "neblina");
            const float fogBefore = view.fogDistance;
            GuiSliderBar(
                Rectangle{rowX + kLabelWidth, rowY, sliderW, kRowHeight},
                nullptr, nullptr, &view.fogDistance, 60.0f, 3000.0f);
            if (view.fogDistance != fogBefore) result.viewChanged = true;
            GuiLabel(Rectangle{rowX + kLabelWidth + sliderW + 4.0f, rowY,
                               kValueWidth, kRowHeight},
                     TextFormat("%.0f", view.fogDistance));
            rowY += kRowHeight + kRowGap;
        }
    }

    section = nullptr;
    for (std::size_t i = 0; i < floatCount; ++i) {
        const world::FloatField& f = floats[i];

        if (section == nullptr || std::strcmp(section, f.section) != 0) {
            section = f.section;
            rowY += kSectionGap;
            GuiLabel(Rectangle{rowX, rowY, content.width, kRowHeight},
                     TextFormat("- %s -", section));
            rowY += kRowHeight;
        }

        GuiLabel(Rectangle{rowX, rowY, kLabelWidth, kRowHeight}, f.label);

        float value = settings.*f.member;
        const float before = value;
        GuiSliderBar(Rectangle{rowX + kLabelWidth, rowY, sliderW, kRowHeight},
                     nullptr, nullptr, &value, f.min, f.max);
        if (value != before) {
            settings.*f.member = value;
            result.changed = true;
        }

        GuiLabel(Rectangle{rowX + kLabelWidth + sliderW + 4.0f, rowY,
                           kValueWidth, kRowHeight},
                 TextFormat("%.4g", value));

        // Os inteiros da mesma secao entram logo depois dos floats dela.
        rowY += kRowHeight + kRowGap;

        const bool lastOfSection =
            (i + 1 >= floatCount) ||
            std::strcmp(f.section, floats[i + 1].section) != 0;
        if (!lastOfSection) continue;

        for (std::size_t j = 0; j < intCount; ++j) {
            if (std::strcmp(ints[j].section, f.section) != 0) continue;

            GuiLabel(Rectangle{rowX, rowY, kLabelWidth, kRowHeight},
                     ints[j].label);

            float intValue = static_cast<float>(settings.*ints[j].member);
            const float intBefore = intValue;
            GuiSliderBar(
                Rectangle{rowX + kLabelWidth, rowY, sliderW, kRowHeight},
                nullptr, nullptr, &intValue,
                static_cast<float>(ints[j].min),
                static_cast<float>(ints[j].max));
            if (intValue != intBefore) {
                settings.*ints[j].member = static_cast<int>(intValue + 0.5f);
                result.changed = true;
            }

            GuiLabel(Rectangle{rowX + kLabelWidth + sliderW + 4.0f, rowY,
                               kValueWidth, kRowHeight},
                     TextFormat("%d", settings.*ints[j].member));
            rowY += kRowHeight + kRowGap;
        }
    }

    EndScissorMode();

    // ---- deteccao de soltura --------------------------------------------
    // O mundo inteiro e regerado quando um valor muda, e isso custa dezenas de
    // milissegundos. Fazer isso a cada frame de arrasto travaria o slider,
    // entao a regeneracao espera o botao ser solto.
    const bool holding = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (result.changed && holding) dragging_ = true;
    if (dragging_ && !holding) {
        dragging_ = false;
        result.released = true;
    }

    return result;
}

}  // namespace ui
