#pragma once

// ── 引擎内置着色器源码 ──────────────────────────────────────
// 从 Sandbox/main.cpp 移入引擎内部，由 SceneRenderer 自动加载使用

namespace Engine { namespace Shaders {

// ── Phong Lit 着色器 ────────────────────────────────────────

inline const char* LitVertex = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

out vec3 vFragPos;
out vec3 vNormal;
out vec2 vTexCoord;
out vec4 vFragPosLightSpace;
out mat3 vTBN;

uniform mat4 uVP;
uniform mat4 uModel;
uniform mat3 uNormalMat;  // CPU 预计算: mat3(transpose(inverse(uModel)))
uniform mat4 uLightSpaceMat;

void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vFragPos = wp.xyz;
    vNormal = uNormalMat * aNormal;
    vTexCoord = aTexCoord;
    vFragPosLightSpace = uLightSpaceMat * wp;

    vec3 T = normalize(uNormalMat * aTangent);
    vec3 B = normalize(uNormalMat * aBitangent);
    vec3 N = normalize(vNormal);
    vTBN = mat3(T, B, N);

    gl_Position = uVP * wp;
}
)";

inline const char* LitFragment = R"(
#version 450 core
in vec3 vFragPos;
in vec3 vNormal;
in vec2 vTexCoord;
in vec4 vFragPosLightSpace;
in mat3 vTBN;
out vec4 FragColor;

uniform vec3 uMatDiffuse;
uniform vec3 uMatSpecular;
uniform float uShininess;
uniform vec3 uDirLightDir;
uniform vec3 uDirLightColor;

#define MAX_PL 8
uniform int uPLCount;
uniform vec3 uPLPos[MAX_PL];
uniform vec3 uPLColor[MAX_PL];
uniform float uPLIntensity[MAX_PL];
uniform float uPLConstant[MAX_PL];
uniform float uPLLinear[MAX_PL];
uniform float uPLQuadratic[MAX_PL];

#define MAX_SL 4
uniform int uSLCount;
uniform vec3  uSLPos[MAX_SL];
uniform vec3  uSLDir[MAX_SL];
uniform vec3  uSLColor[MAX_SL];
uniform float uSLIntensity[MAX_SL];
uniform float uSLInnerCut[MAX_SL];  // cos(inner cutoff)
uniform float uSLOuterCut[MAX_SL];  // cos(outer cutoff)
uniform float uSLConstant[MAX_SL];
uniform float uSLLinear[MAX_SL];
uniform float uSLQuadratic[MAX_SL];

uniform vec3 uViewPos;
uniform int uUseTex;
uniform sampler2D uTex;

uniform sampler2D uShadowMap;
uniform int uShadowEnabled;

uniform sampler2D uNormalMap;
uniform int uUseNormalMap;

uniform float uAmbientStrength;  // 环境光系数（默认 0.15）

float CalcShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 0.0;

    // 动态 slope-scale bias
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);

    // PCF 3x3
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float d = texture(uShadowMap, proj.xy + vec2(x,y) * texelSize).r;
            shadow += (proj.z - bias > d) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

void main() {
    vec3 N = normalize(vNormal);
    // 法线贴图采样
    if (uUseNormalMap == 1) {
        vec3 mapN = texture(uNormalMap, vTexCoord).rgb;
        mapN = mapN * 2.0 - 1.0; // [0,1] -> [-1,1]
        N = normalize(vTBN * mapN);
    }
    vec3 V = normalize(uViewPos - vFragPos);
    vec3 base = uMatDiffuse;
    if (uUseTex == 1) base = texture(uTex, vTexCoord).rgb;

    // ── 阴影计算 ───────────────────────────────
    vec3 L = normalize(-uDirLightDir);
    float shadow = 0.0;
    if (uShadowEnabled == 1) {
        shadow = CalcShadow(vFragPosLightSpace, N, L);
    }

    // ── 方向光 ────────────────────────────────
    float diff = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), uShininess);
    vec3 result = (uAmbientStrength * base + (1.0 - shadow) * (diff * base + spec * uMatSpecular)) * uDirLightColor;

    // ── 点光 ──────────────────────────────────────────────
    for (int i = 0; i < uPLCount; i++) {
        vec3 pL = normalize(uPLPos[i] - vFragPos);
        float d = length(uPLPos[i] - vFragPos);
        float att = 1.0 / (uPLConstant[i] + uPLLinear[i]*d + uPLQuadratic[i]*d*d);
        float pDiff = max(dot(N, pL), 0.0);
        vec3 pH = normalize(pL + V);
        float pSpec = pow(max(dot(N, pH), 0.0), uShininess);
        result += (pDiff * base + pSpec * uMatSpecular) * uPLColor[i] * uPLIntensity[i] * att;
    }

    // ── 聚光灯 ────────────────────────────────────────────
    for (int i = 0; i < uSLCount; i++) {
        vec3 sL = normalize(uSLPos[i] - vFragPos);
        float d = length(uSLPos[i] - vFragPos);

        // 内外锥衰减
        float theta = dot(sL, normalize(-uSLDir[i]));
        float epsilon = uSLInnerCut[i] - uSLOuterCut[i];
        float spotAtt = clamp((theta - uSLOuterCut[i]) / epsilon, 0.0, 1.0);

        // 距离衰减
        float att = 1.0 / (uSLConstant[i] + uSLLinear[i]*d + uSLQuadratic[i]*d*d);

        float sDiff = max(dot(N, sL), 0.0);
        vec3 sH = normalize(sL + V);
        float sSpec = pow(max(dot(N, sH), 0.0), uShininess);

        result += (sDiff * base + sSpec * uMatSpecular) * uSLColor[i] * uSLIntensity[i] * att * spotAtt;
    }

    FragColor = vec4(result, 1.0);
}
)";

// ── Emissive 自发光着色器 ───────────────────────────────────

inline const char* EmissiveVertex = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
uniform mat4 uVP;
uniform mat4 uModel;
void main() { gl_Position = uVP * uModel * vec4(aPos, 1.0); }
)";

inline const char* EmissiveFragment = R"(
#version 450 core
out vec4 FragColor;
uniform vec3 uColor;
void main() { FragColor = vec4(uColor, 1.0); }
)";

}} // namespace Engine::Shaders
