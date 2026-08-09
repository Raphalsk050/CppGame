#pragma once

#include "raylib.h"

namespace render {

// Shader de iluminacao para a isosuperficie.
//
// O shader default da raylib nao ilumina nada: ele multiplica textura por
// colDiffuse e pronto. Numa malha branca sem textura isso pinta a superficie
// de branco chapado - as normais interpoladas pelo poligonizador nao teriam
// nenhum efeito visivel, e o relevo da superficie ficaria invisivel.
//
// GLSL embutido no binario em vez de arquivo .vs/.fs em disco: o executavel
// continua sendo uma coisa so, sem diretorio de assets para achar em runtime.
class SurfaceShader {
public:
    SurfaceShader();
    ~SurfaceShader();

    SurfaceShader(const SurfaceShader&) = delete;
    SurfaceShader& operator=(const SurfaceShader&) = delete;

    // Instala o shader no material do modelo. Precisa ser refeito sempre que o
    // modelo e recriado (a DynamicMesh cria um material default novo).
    void ApplyTo(Model& model) const;

    // A especular depende da posicao da camera, entao muda a cada frame.
    void SetViewPosition(Vector3 position) const;

    // Distancia em que a neblina fecha. Deve acompanhar a distancia de
    // renderizacao: neblina curta demais esconde terreno carregado, longa
    // demais deixa a borda dos chunks aparecer como recorte reto.
    void SetFogDistance(float distance) const;

    // Liga a absorcao seletiva por canal usada debaixo d'agua. Precisa do
    // nivel do mar para saber a profundidade da coluna acima do fragmento.
    void SetUnderwater(bool submerged, float seaLevel) const;

    bool IsValid() const;

    Shader Get() const { return shader_; }

private:
    Shader shader_{};
    int viewPositionLoc_ = -1;
    int lightDirectionLoc_ = -1;
    int fogDistanceLoc_ = -1;
    int underwaterLoc_ = -1;
    int seaLevelLoc_ = -1;
};

}  // namespace render
