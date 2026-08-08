#pragma once

#include "mc/MeshData.hpp"
#include "raylib.h"

namespace render {

// Malha da raylib remontada a cada frame, sem recriar recursos de GPU.
//
// O caminho ingenuo - UnloadModel + LoadModelFromMesh por frame - destroi e
// recria VAO/VBO 60 vezes por segundo. Aqui os buffers sao alocados uma vez no
// pior caso, subidos com UploadMesh(dynamic = true), e cada frame so reescreve
// o prefixo usado com UpdateMeshBuffer.
//
// O truque que permite a contagem de triangulos variar por frame: DrawMesh usa
// `mesh.vertexCount` como contagem do draw call quando a malha nao e indexada
// (confirmado em rmodels.c). Entao basta encolher esse campo - o VBO continua
// com o tamanho maximo, mas so o prefixo valido e desenhado.
//
// A classe tem posse dos recursos e nao e copiavel: duas copias chamariam
// UnloadModel no mesmo VAO.
class DynamicMesh {
public:
    // Capacidade sugerida para uma grade de resolucao `resolution`. O numero de
    // celulas que cruzam a superficie cresce com o quadrado da resolucao, nao
    // com o cubo, entao a folga aqui e sobre res^2.
    static int RecommendedCapacity(int resolution);

    explicit DynamicMesh(int maxVertices);
    ~DynamicMesh();

    DynamicMesh(const DynamicMesh&) = delete;
    DynamicMesh& operator=(const DynamicMesh&) = delete;
    DynamicMesh(DynamicMesh&& other) noexcept;
    DynamicMesh& operator=(DynamicMesh&& other) noexcept;

    // Envia a geometria do frame. Trunca (sem estourar) se `data` passar da
    // capacidade; nesse caso Overflowed() passa a devolver true, para o
    // descarte aparecer na tela em vez de virar buraco silencioso na malha.
    void Update(const mc::MeshData& data);

    void Draw(Vector3 position, Color tint) const;
    void DrawWires(Vector3 position, Color tint) const;

    // Troca o shader do material. Precisa ser por malha porque DrawModel usa
    // `material.shader` e ignora o estado de BeginShaderMode - definir o
    // shader "globalmente" antes do draw nao teria efeito nenhum.
    void SetShader(Shader shader);

    int Capacity() const { return capacity_; }
    int VertexCount() const { return vertexCount_; }
    int TriangleCount() const { return vertexCount_ / 3; }
    bool Overflowed() const { return overflowed_; }

private:
    void Release();

    Model model_{};
    int capacity_ = 0;
    int vertexCount_ = 0;
    bool overflowed_ = false;
    bool owns_ = false;
};

}  // namespace render
