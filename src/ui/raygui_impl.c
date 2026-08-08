// Implementacao da raygui, isolada numa unidade de traducao C.
//
// A raygui e C e usa operador ternario entre enumeracoes DIFERENTES
// (GuiControlProperty vs GuiDefaultProperty). Isso e legal em C, foi
// deprecado no C++20 e virou ill-formed no C++26 - que e o padrao deste
// projeto. Compilar o corpo dela como C++ produz erro em ~6 pontos do
// cabecalho, sem correcao possivel do nosso lado.
//
// Compilando aqui como C, o codigo dela e valido, e o C++ inclui raygui.h
// SEM a macro de implementacao para pegar so as declaracoes - que o proprio
// cabecalho ja protege com extern "C".
#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
