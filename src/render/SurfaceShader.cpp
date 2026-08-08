#include "render/SurfaceShader.hpp"

#include "raymath.h"

namespace render {
namespace {

// Os nomes `vertexPosition`, `vertexNormal`, `vertexColor`, `mvp`, `matModel`,
// `matNormal` e `colDiffuse` nao sao arbitrarios: a raylib liga esses
// atributos e uniformes automaticamente ao carregar o shader (ver
// RL_DEFAULT_SHADER_ATTRIB_NAME_* em rlgl.h). Renomear qualquer um quebra a
// ligacao silenciosamente - a malha renderiza preta em vez de dar erro.
constexpr const char* kVertexShader = R"GLSL(#version 330

in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

out vec3 fragPosition;
out vec3 fragNormal;
out vec4 fragColor;

void main()
{
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 1.0)));
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

constexpr const char* kFragmentShader = R"GLSL(#version 330

in vec3 fragPosition;
in vec3 fragNormal;
in vec4 fragColor;

uniform vec4 colDiffuse;
uniform vec3 viewPosition;
uniform vec3 lightDirection;

out vec4 finalColor;

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 toLight = normalize(-lightDirection);
    vec3 toView = normalize(viewPosition - fragPosition);

    // Difusa Lambert com wrap: o termo nao cai a zero abruptamente, o que
    // evita que o lado sombreado do terreno vire uma silhueta preta chapada.
    float diffuse = max(dot(normal, toLight) * 0.5 + 0.5, 0.0);
    diffuse = diffuse * diffuse;

    // Ambiente hemisferico: luz do ceu por cima, rebote do chao por baixo.
    // Custa quase nada e da muito mais leitura de volume que um ambiente
    // constante, principalmente dentro das cavernas.
    float sky = 0.5 + 0.5 * normal.y;
    vec3 ambient = mix(vec3(0.10, 0.11, 0.15), vec3(0.42, 0.48, 0.60), sky);

    // Especular Blinn-Phong bem discreta, so para dar relevo as cristas.
    vec3 halfway = normalize(toLight + toView);
    float specular = pow(max(dot(normal, halfway), 0.0), 32.0) * 0.12;

    vec3 base = fragColor.rgb * colDiffuse.rgb;
    vec3 lit = base * (ambient + vec3(1.0, 0.96, 0.88) * diffuse * 0.85);
    lit += vec3(specular);

    // Neblina pela distancia: esconde a borda onde os chunks acabam, que sem
    // isso aparece como um recorte reto no horizonte.
    float distance = length(viewPosition - fragPosition);
    float fog = 1.0 - exp(-distance * 0.0028);
    lit = mix(lit, vec3(0.62, 0.74, 0.90), clamp(fog, 0.0, 1.0));

    finalColor = vec4(lit, 1.0);
}
)GLSL";

}  // namespace

SurfaceShader::SurfaceShader() {
    shader_ = LoadShaderFromMemory(kVertexShader, kFragmentShader);

    viewPositionLoc_ = GetShaderLocation(shader_, "viewPosition");
    lightDirectionLoc_ = GetShaderLocation(shader_, "lightDirection");

    // Sol fixo, vindo de cima e de lado. Direcao normalizada aqui para o
    // fragment shader nao ter que fazer isso por pixel.
    const Vector3 light = Vector3Normalize(Vector3{-0.45f, -1.0f, -0.35f});
    if (lightDirectionLoc_ != -1) {
        SetShaderValue(shader_, lightDirectionLoc_, &light, SHADER_UNIFORM_VEC3);
    }
}

SurfaceShader::~SurfaceShader() {
    if (IsValid()) UnloadShader(shader_);
}

bool SurfaceShader::IsValid() const { return shader_.id > 0; }

void SurfaceShader::ApplyTo(Model& model) const {
    if (!IsValid()) return;
    for (int i = 0; i < model.materialCount; ++i) {
        model.materials[i].shader = shader_;
    }
}

void SurfaceShader::SetViewPosition(Vector3 position) const {
    if (!IsValid() || viewPositionLoc_ == -1) return;
    SetShaderValue(shader_, viewPositionLoc_, &position, SHADER_UNIFORM_VEC3);
}

}  // namespace render
