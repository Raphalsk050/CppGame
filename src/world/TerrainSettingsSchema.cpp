#include "world/TerrainSettingsSchema.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace world {
namespace {

// Os limites de cada slider. Escolhidos para cobrir a faixa util sem permitir
// valores que travam a maquina - amplitude de montanha em milhares, por
// exemplo, faria o ChunkManager gerar centenas de camadas verticais.
constexpr FloatField kFloatFields[] = {
    {"Chao e mar", "groundLevel", "altura do chao",
     &TerrainSettings::groundLevel, -40.0f, 80.0f},
    {"Chao e mar", "seaLevel", "nivel do mar", &TerrainSettings::seaLevel,
     -40.0f, 60.0f},

    {"Colinas", "hillHeight", "amplitude", &TerrainSettings::hillHeight, 0.0f,
     60.0f},
    {"Colinas", "hillScale", "frequencia", &TerrainSettings::hillScale,
     0.0005f, 0.060f},

    {"Montanhas", "mountainHeight", "amplitude",
     &TerrainSettings::mountainHeight, 0.0f, 200.0f},
    {"Montanhas", "mountainScale", "frequencia",
     &TerrainSettings::mountainScale, 0.0002f, 0.020f},
    {"Montanhas", "mountainSharpness", "perfil da encosta",
     &TerrainSettings::mountainSharpness, 0.6f, 3.0f},

    {"Relevo continental", "reliefScale", "frequencia",
     &TerrainSettings::reliefScale, 0.0002f, 0.010f},
    {"Relevo continental", "reliefLow", "inicio", &TerrainSettings::reliefLow,
     0.0f, 1.0f},
    {"Relevo continental", "reliefHigh", "fim", &TerrainSettings::reliefHigh,
     0.0f, 1.0f},

    {"Erosao", "erosionStrength", "intensidade",
     &TerrainSettings::erosionStrength, 0.0f, 1.0f},
    {"Erosao", "erosionScale", "frequencia", &TerrainSettings::erosionScale,
     0.0002f, 0.010f},

    {"Rios e vales", "riverScale", "frequencia", &TerrainSettings::riverScale,
     0.0002f, 0.010f},
    {"Rios e vales", "riverWidth", "largura do canal",
     &TerrainSettings::riverWidth, 0.0f, 0.20f},
    {"Rios e vales", "valleyWidth", "largura do vale",
     &TerrainSettings::valleyWidth, 0.0f, 0.60f},
    {"Rios e vales", "riverDepth", "profundidade do leito",
     &TerrainSettings::riverDepth, 0.0f, 40.0f},
    {"Rios e vales", "valleyDepth", "rebaixo do vale",
     &TerrainSettings::valleyDepth, 0.0f, 40.0f},
    {"Rios e vales", "riverMountainResistance", "resistencia da montanha",
     &TerrainSettings::riverMountainResistance, 0.0f, 1.0f},
    {"Rios e vales", "riverWidthVariation", "variacao de largura",
     &TerrainSettings::riverWidthVariation, 0.0f, 1.0f},
    {"Rios e vales", "riverWidthScale", "escala da variacao",
     &TerrainSettings::riverWidthScale, 0.001f, 0.050f},
    {"Rios e vales", "riverAltitudeLimit", "altitude maxima",
     &TerrainSettings::riverAltitudeLimit, -20.0f, 120.0f},
    {"Rios e vales", "riverAltitudeFade", "transicao de altitude",
     &TerrainSettings::riverAltitudeFade, 1.0f, 60.0f},

    {"Terracos", "terraceStrength", "intensidade",
     &TerrainSettings::terraceStrength, 0.0f, 1.0f},
    {"Terracos", "terraceStep", "altura da camada",
     &TerrainSettings::terraceStep, 0.5f, 30.0f},
    {"Terracos", "terraceStart", "altura inicial",
     &TerrainSettings::terraceStart, -40.0f, 100.0f},
    {"Terracos", "terraceFade", "transicao", &TerrainSettings::terraceFade,
     0.5f, 60.0f},

    {"Cavernas", "caveStrength", "intensidade",
     &TerrainSettings::caveStrength, 0.0f, 4.0f},
    {"Cavernas", "caveScale", "frequencia", &TerrainSettings::caveScale,
     0.005f, 0.120f},
    {"Cavernas", "caveThreshold", "largura do tunel",
     &TerrainSettings::caveThreshold, 0.0f, 0.35f},
    {"Cavernas", "caveDepthBelowSurface", "profundidade minima",
     &TerrainSettings::caveDepthBelowSurface, 0.0f, 40.0f},

    {"Cores por altitude", "beachHeight", "praia ate",
     &TerrainSettings::beachHeight, 0.0f, 30.0f},
    {"Cores por altitude", "grassHeight", "vegetacao ate",
     &TerrainSettings::grassHeight, 0.0f, 90.0f},
    {"Cores por altitude", "rockHeight", "rocha ate",
     &TerrainSettings::rockHeight, 0.0f, 160.0f},
    {"Cores por altitude", "snowHeight", "neve a partir de",
     &TerrainSettings::snowHeight, 0.0f, 250.0f},
    {"Cores por altitude", "strataContrast", "contraste dos estratos",
     &TerrainSettings::strataContrast, 0.0f, 0.6f},

    {"Biomas", "temperatureScale", "escala de temperatura",
     &TerrainSettings::temperatureScale, 0.0002f, 0.010f},
    {"Biomas", "humidityScale", "escala de umidade",
     &TerrainSettings::humidityScale, 0.0002f, 0.010f},
};

constexpr IntField kIntFields[] = {
    {"Colinas", "hillOctaves", "oitavas", &TerrainSettings::hillOctaves, 1, 8},
    {"Montanhas", "mountainOctaves", "oitavas",
     &TerrainSettings::mountainOctaves, 1, 8},
};

}  // namespace

const FloatField* FloatFields(std::size_t& count) {
    count = sizeof(kFloatFields) / sizeof(kFloatFields[0]);
    return kFloatFields;
}

const IntField* IntFields(std::size_t& count) {
    count = sizeof(kIntFields) / sizeof(kIntFields[0]);
    return kIntFields;
}

bool SaveSettings(const TerrainSettings& settings, const std::string& path) {
    std::ofstream out(path);
    if (!out) return false;

    out << "# Configuracao de terreno - CppGame\n";
    out << "# Gerado pelo painel do HUD. Editavel a mao.\n\n";
    out << "seed = " << settings.seed << "\n";

    const char* section = nullptr;
    for (const FloatField& f : kFloatFields) {
        if (section == nullptr || std::string(section) != f.section) {
            section = f.section;
            out << "\n# --- " << section << " ---\n";
        }
        out << f.key << " = " << (settings.*f.member) << "\n";
    }
    for (const IntField& f : kIntFields) {
        out << f.key << " = " << (settings.*f.member) << "\n";
    }

    return out.good();
}

bool LoadSettings(TerrainSettings& settings, const std::string& path) {
    std::ifstream in(path);
    if (!in) return false;

    std::string line;
    while (std::getline(in, line)) {
        // Corta comentario e espacos.
        const std::size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);

        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        const auto trim = [](std::string& s) {
            const std::size_t first = s.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                s.clear();
                return;
            }
            const std::size_t last = s.find_last_not_of(" \t\r\n");
            s = s.substr(first, last - first + 1);
        };
        trim(key);
        trim(value);
        if (key.empty() || value.empty()) continue;

        if (key == "seed") {
            settings.seed = static_cast<std::uint32_t>(
                std::strtoul(value.c_str(), nullptr, 10));
            continue;
        }

        bool matched = false;
        for (const FloatField& f : kFloatFields) {
            if (key == f.key) {
                settings.*f.member = std::strtof(value.c_str(), nullptr);
                matched = true;
                break;
            }
        }
        if (matched) continue;

        for (const IntField& f : kIntFields) {
            if (key == f.key) {
                settings.*f.member = std::atoi(value.c_str());
                break;
            }
        }
        // Chave desconhecida: ignorada de proposito, para arquivo de versao
        // antiga continuar carregando.
    }

    return true;
}

}  // namespace world
