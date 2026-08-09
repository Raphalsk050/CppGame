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
uniform float fogDistance;
// > 0 quando a camera esta debaixo d'agua.
uniform float underwater;
uniform float seaLevel;

out vec4 finalColor;

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 toLight = normalize(-lightDirection);
    vec3 toView = normalize(viewPosition - fragPosition);

    // Difusa com wrap PEQUENO. A versao anterior usava wrap total
    // (dot*0.5+0.5), que ilumina ate as faces viradas para longe do sol e
    // achata o relevo inteiro; o wrap curto mantem o lado sombreado escuro sem
    // virar silhueta preta.
    float lambert = dot(normal, toLight);
    float diffuse = max(lambert * 0.72 + 0.28, 0.0);

    // Ambiente hemisferico: luz do ceu por cima, rebote do chao por baixo.
    // Custa quase nada e da muito mais leitura de volume que um ambiente
    // constante, principalmente dentro das cavernas. Mantido BAIXO de
    // proposito - ambiente forte lava a cor da superficie ate o branco.
    float sky = 0.5 + 0.5 * normal.y;
    vec3 ambient = mix(vec3(0.17, 0.19, 0.23), vec3(0.36, 0.40, 0.48), sky);

    // Especular Blinn-Phong bem discreta, so para dar relevo as cristas.
    vec3 halfway = normalize(toLight + toView);
    float specular = pow(max(dot(normal, halfway), 0.0), 32.0) * 0.12;

    vec3 base = fragColor.rgb * colDiffuse.rgb;
    vec3 lit = base * (ambient + vec3(1.0, 0.97, 0.90) * diffuse * 0.92);
    lit += vec3(specular);

    // Neblina pela distancia: esconde a borda onde os chunks acabam, que sem
    // isso aparece como um recorte reto no horizonte.
    //
    // A queda e EXPONENCIAL AO QUADRADO, nao exponencial simples. A versao
    // anterior, 1-exp(-d*0.0028), ja punha 24% de neblina a 100 unidades e 66%
    // na borda da visao - era ela, e nao a paleta, a maior responsavel pelo
    // terreno sair lavado. Com o quadrado, o primeiro terco da distancia fica
    // praticamente limpo e a neblina se concentra no horizonte.
    float distance = length(viewPosition - fragPosition);

    if (underwater > 0.5) {
        // ABSORCAO SELETIVA POR COMPRIMENTO DE ONDA.
        //
        // Debaixo d'agua a luz nao esmaece por igual: o vermelho e absorvido
        // primeiro, depois o verde, e o azul e o que penetra mais fundo. E
        // exatamente por isso que o mar limpo parece azul e que tudo perde a
        // cor com a profundidade. Os coeficientes seguem a ordem relativa dos
        // Kd de Jerlov (agua tipo I-II): vermelho atenua ~8x mais rapido que
        // o azul.
        vec3 kd = vec3(0.115, 0.032, 0.018);

        // Caminho optico: da camera ate o fragmento, mais a profundidade -
        // a luz que chega ali ja atravessou a coluna de agua de cima.
        float depth = max(seaLevel - fragPosition.y, 0.0);
        vec3 transmit = exp(-kd * (distance + depth * 0.6));

        // Cor da agua espalhada, que substitui o que foi absorvido.
        vec3 aguaProfunda = vec3(0.02, 0.16, 0.26);
        lit = lit * transmit + aguaProfunda * (1.0 - transmit);

        finalColor = vec4(lit, 1.0);
        return;
    }

    float f = distance / max(fogDistance, 1.0);
    float fog = 1.0 - exp(-f * f);
    lit = mix(lit, vec3(0.66, 0.78, 0.93), clamp(fog, 0.0, 1.0));

    finalColor = vec4(lit, 1.0);
}
)GLSL";

}  // namespace

SurfaceShader::SurfaceShader() {
    shader_ = LoadShaderFromMemory(kVertexShader, kFragmentShader);

    viewPositionLoc_ = GetShaderLocation(shader_, "viewPosition");
    lightDirectionLoc_ = GetShaderLocation(shader_, "lightDirection");
    fogDistanceLoc_ = GetShaderLocation(shader_, "fogDistance");
    underwaterLoc_ = GetShaderLocation(shader_, "underwater");
    seaLevelLoc_ = GetShaderLocation(shader_, "seaLevel");
    SetFogDistance(600.0f);
    SetUnderwater(false, 0.0f);

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

void SurfaceShader::SetFogDistance(float distance) const {
    if (!IsValid() || fogDistanceLoc_ == -1) return;
    SetShaderValue(shader_, fogDistanceLoc_, &distance, SHADER_UNIFORM_FLOAT);
}

void SurfaceShader::SetUnderwater(bool submerged, float seaLevel) const {
    if (!IsValid()) return;
    const float flag = submerged ? 1.0f : 0.0f;
    if (underwaterLoc_ != -1) {
        SetShaderValue(shader_, underwaterLoc_, &flag, SHADER_UNIFORM_FLOAT);
    }
    if (seaLevelLoc_ != -1) {
        SetShaderValue(shader_, seaLevelLoc_, &seaLevel, SHADER_UNIFORM_FLOAT);
    }
}

}  // namespace render
