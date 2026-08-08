#pragma once

#include "mc/MeshData.hpp"
#include "mc/SampleGrid.hpp"

namespace mc {

struct PolygonizeStats {
    int cellsVisited = 0;
    int cellsOnSurface = 0;  // celulas que geraram ao menos um triangulo
    int triangles = 0;
};

// Converte a grade amostrada na sopa de triangulos da isosuperficie em `iso`.
//
// Nao recebe o campo: posicao E normal saem por interpolacao dos dados que a
// SampleGrid ja carrega. Isso mantem o algoritmo puramente geometrico e sem
// template, entao ele compila uma vez so em vez de por tipo de campo.
//
// `margin` ignora essa quantidade de celulas em cada face da grade. Serve para
// terreno em chunks: amostrando com uma celula de folga em volta e marchando
// so o interior, todo ponto usado tem vizinho dos dois lados e o gradiente sai
// por diferenca central. Sem isso os pontos da fronteira cairiam na diferenca
// lateral, e dois chunks vizinhos calculariam normais diferentes para o mesmo
// vertice - uma emenda de iluminacao visivel a cada borda de chunk.
PolygonizeStats Polygonize(const SampleGrid& grid, float iso, MeshData& out,
                           int margin = 0);

}  // namespace mc
