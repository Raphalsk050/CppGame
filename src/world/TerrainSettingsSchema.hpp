#pragma once

#include <cstddef>
#include <string>

#include "world/TerrainGenerator.hpp"

namespace world {

// Descricao dos campos de TerrainSettings, dirigindo TANTO os sliders do HUD
// QUANTO o arquivo de save.
//
// A alternativa - uma lista de sliders na UI e outra no serializador - deixa
// as duas livres para divergirem: um campo novo ganha slider mas some ao
// salvar, ou e salvo com nome que a UI nao mostra. Com ponteiro-para-membro,
// acrescentar uma configuracao e acrescentar UMA linha na tabela, e slider,
// gravacao e leitura aparecem juntos.
struct FloatField {
    const char* section;
    const char* key;    // nome no arquivo; nunca renomear sem quebrar saves
    const char* label;  // texto na tela
    float TerrainSettings::* member;
    float min;
    float max;
};

struct IntField {
    const char* section;
    const char* key;
    const char* label;
    int TerrainSettings::* member;
    int min;
    int max;
};

const FloatField* FloatFields(std::size_t& count);
const IntField* IntFields(std::size_t& count);

// Formato texto simples "chave = valor", uma por linha. Linhas desconhecidas
// sao ignoradas na leitura, entao um arquivo de uma versao antiga carrega sem
// erro - os campos ausentes ficam no padrao.
bool SaveSettings(const TerrainSettings& settings, const std::string& path);
bool LoadSettings(TerrainSettings& settings, const std::string& path);

}  // namespace world
