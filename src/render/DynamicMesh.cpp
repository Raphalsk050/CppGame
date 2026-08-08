#include "render/DynamicMesh.hpp"

#include <algorithm>
#include <cstring>

#include "rlgl.h"

namespace render {
namespace {

// Vector3 tem que ser tres floats contiguos para os vetores de MeshData irem
// direto para a GPU sem repacking.
static_assert(sizeof(Vector3) == 3 * sizeof(float),
              "Vector3 precisa ser 3 floats contiguos");
static_assert(sizeof(Color) == 4 * sizeof(unsigned char),
              "Color precisa ser 4 bytes contiguos");

constexpr int kMaxCapacity = 4'000'000;

}  // namespace

int DynamicMesh::RecommendedCapacity(int resolution) {
    // Apenas as celulas atravessadas pela superficie geram triangulos, e elas
    // formam uma casca: sao O(res^2), nao O(res^3). O fator 48 (media bem
    // acima dos ~2 triangulos por celula tipicos) da margem para campos
    // bastante recortados sem estourar a memoria.
    const long long capacity = 48LL * resolution * resolution;
    return static_cast<int>(std::clamp<long long>(capacity, 3, kMaxCapacity));
}

DynamicMesh::DynamicMesh(int maxVertices) {
    capacity_ = std::max(3, maxVertices - (maxVertices % 3));

    // Zero-inicializar e obrigatorio: a Mesh da raylib 6.0 tem ponteiros de
    // skinning e animacao que o UploadMesh consulta e o UnloadMesh libera.
    Mesh mesh = {};
    mesh.vertexCount = capacity_;
    mesh.triangleCount = capacity_ / 3;

    // MemAlloc (= RL_CALLOC) e obrigatorio aqui: UnloadMesh chama RL_FREE
    // nesses ponteiros. Alocar com new[] ou entregar vector::data() e crash
    // na destruicao.
    const unsigned int bytes =
        static_cast<unsigned int>(capacity_) * 3u * sizeof(float);
    mesh.vertices = static_cast<float*>(MemAlloc(bytes));
    mesh.normals = static_cast<float*>(MemAlloc(bytes));

    // O buffer de cor precisa existir ja no UploadMesh, senao a raylib nem
    // cria o VBO e o UpdateMeshBuffer depois escreveria em VBO zero.
    // MemAlloc zera a memoria, e cor zerada tem alfa 0 - malha invisivel.
    // Por isso o preenchimento explicito com branco.
    const unsigned int colorBytes =
        static_cast<unsigned int>(capacity_) * 4u * sizeof(unsigned char);
    mesh.colors = static_cast<unsigned char*>(MemAlloc(colorBytes));
    std::memset(mesh.colors, 0xFF, colorBytes);

    // dynamic = true: VBOs com GL_DYNAMIC_DRAW, dimensionados pelo
    // vertexCount atual - por isso subimos ja na capacidade maxima.
    UploadMesh(&mesh, true);

    model_ = LoadModelFromMesh(mesh);
    owns_ = true;

    // Nada a desenhar ate o primeiro Update.
    model_.meshes[0].vertexCount = 0;
    model_.meshes[0].triangleCount = 0;
}

DynamicMesh::~DynamicMesh() { Release(); }

DynamicMesh::DynamicMesh(DynamicMesh&& other) noexcept
    : model_(other.model_),
      capacity_(other.capacity_),
      vertexCount_(other.vertexCount_),
      overflowed_(other.overflowed_),
      owns_(other.owns_) {
    other.owns_ = false;
    other.model_ = {};
    other.capacity_ = 0;
    other.vertexCount_ = 0;
}

DynamicMesh& DynamicMesh::operator=(DynamicMesh&& other) noexcept {
    if (this != &other) {
        Release();
        model_ = other.model_;
        capacity_ = other.capacity_;
        vertexCount_ = other.vertexCount_;
        overflowed_ = other.overflowed_;
        owns_ = other.owns_;
        other.owns_ = false;
        other.model_ = {};
        other.capacity_ = 0;
        other.vertexCount_ = 0;
    }
    return *this;
}

void DynamicMesh::Release() {
    if (owns_) {
        // Libera VAO, VBOs, os buffers de MemAlloc e o material default.
        UnloadModel(model_);
        owns_ = false;
    }
    model_ = {};
    capacity_ = 0;
    vertexCount_ = 0;
}

void DynamicMesh::Update(const mc::MeshData& data) {
    if (!owns_) return;

    Mesh& mesh = model_.meshes[0];

    int count = static_cast<int>(
        std::min<std::size_t>(data.VertexCount(), static_cast<std::size_t>(
                                                      capacity_)));
    count -= count % 3;  // nunca deixar um triangulo pela metade
    overflowed_ = data.VertexCount() > static_cast<std::size_t>(count);

    if (count > 0) {
        const int bytes = count * 3 * static_cast<int>(sizeof(float));
        // Direto do MeshData para o VBO. Os buffers de CPU em mesh.vertices /
        // mesh.normals existem porque UploadMesh precisa de ponteiros validos
        // para dimensionar os VBOs e UnloadMesh os libera; seu conteudo nao e
        // lido depois, entao copiar para eles seria trabalho jogado fora.
        UpdateMeshBuffer(mesh, RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION,
                         data.positions.data(), bytes, 0);
        UpdateMeshBuffer(mesh, RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL,
                         data.normals.data(), bytes, 0);

        // Sem cores o buffer fica no branco inicial, e o shader multiplica
        // por colDiffuse normalmente.
        if (data.HasColors()) {
            UpdateMeshBuffer(mesh, RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR,
                             data.colors.data(),
                             count * 4 * static_cast<int>(sizeof(unsigned char)),
                             0);
        }
    }

    vertexCount_ = count;
    mesh.vertexCount = count;
    mesh.triangleCount = count / 3;
}

void DynamicMesh::SetShader(Shader shader) {
    if (!owns_ || shader.id == 0) return;
    for (int i = 0; i < model_.materialCount; ++i) {
        model_.materials[i].shader = shader;
    }
}

void DynamicMesh::Draw(Vector3 position, Color tint) const {
    if (!owns_ || vertexCount_ == 0) return;
    DrawModel(model_, position, 1.0f, tint);
}

void DynamicMesh::DrawWires(Vector3 position, Color tint) const {
    if (!owns_ || vertexCount_ == 0) return;
    DrawModelWires(model_, position, 1.0f, tint);
}

}  // namespace render
