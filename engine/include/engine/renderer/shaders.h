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

// 自发光支持
uniform int uIsEmissive;
uniform vec3 uEmissiveColor;
uniform float uEmissiveIntensity;

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
        float epsilon = max(uSLInnerCut[i] - uSLOuterCut[i], 0.001);
        float spotAtt = clamp((theta - uSLOuterCut[i]) / epsilon, 0.0, 1.0);

        // 距离衰减
        float att = 1.0 / (uSLConstant[i] + uSLLinear[i]*d + uSLQuadratic[i]*d*d);

        float sDiff = max(dot(N, sL), 0.0);
        vec3 sH = normalize(sL + V);
        float sSpec = pow(max(dot(N, sH), 0.0), uShininess);

        result += (sDiff * base + sSpec * uMatSpecular) * uSLColor[i] * uSLIntensity[i] * att * spotAtt;
    }

    // ── 自发光叠加（HDR! 允许 >1.0 以激活 Bloom）─────────
    if (uIsEmissive == 1) {
        result += uEmissiveColor * uEmissiveIntensity;
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

// ── 延迟渲染: G-Buffer 几何 Shader ─────────────────────────

inline const char* GBufferVertex = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

out vec3 vFragPos;
out vec3 vNormal;
out vec2 vTexCoord;
out mat3 vTBN;

uniform mat4 uVP;
uniform mat4 uModel;
uniform mat3 uNormalMat;

void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vFragPos = wp.xyz;
    vNormal  = normalize(uNormalMat * aNormal);
    vTexCoord = aTexCoord;

    vec3 T = normalize(uNormalMat * aTangent);
    vec3 B = normalize(uNormalMat * aBitangent);
    vec3 N = vNormal;
    vTBN = mat3(T, B, N);

    gl_Position = uVP * wp;
}
)";

inline const char* GBufferFragment = R"(
#version 450 core
layout(location = 0) out vec3 gPosition;   // RT0: 世界空间位置
layout(location = 1) out vec3 gNormal;     // RT1: 世界空间法线
layout(location = 2) out vec4 gAlbedoSpec; // RT2: Albedo.rgb + Specular
layout(location = 3) out vec4 gEmissive;   // RT3: Emissive.rgb + Reserved

in vec3 vFragPos;
in vec3 vNormal;
in vec2 vTexCoord;
in mat3 vTBN;

uniform vec3  uMatDiffuse;
uniform vec3  uMatSpecular;
uniform float uShininess;
uniform int   uUseTex;
uniform sampler2D uTex;
uniform sampler2D uNormalMap;
uniform int   uUseNormalMap;

// 自发光
uniform int   uIsEmissive;
uniform vec3  uEmissiveColor;
uniform float uEmissiveIntensity;

void main() {
    // 位置
    gPosition = vFragPos;

    // 法线 (支持法线贴图)
    vec3 N = normalize(vNormal);
    if (uUseNormalMap == 1) {
        vec3 mapN = texture(uNormalMap, vTexCoord).rgb * 2.0 - 1.0;
        N = normalize(vTBN * mapN);
    }
    gNormal = N;

    // 漫反射颜色 + 高光强度
    vec3 albedo = uMatDiffuse;
    if (uUseTex == 1) albedo = texture(uTex, vTexCoord).rgb;
    float specPower = uShininess / 256.0; // 归一化到 [0, 1]
    gAlbedoSpec = vec4(albedo, specPower);

    // 自发光
    if (uIsEmissive == 1) {
        gEmissive = vec4(uEmissiveColor * uEmissiveIntensity, 1.0);
    } else {
        gEmissive = vec4(0.0);
    }
}
)";

// ── 延迟渲染: 光照 Pass Shader ─────────────────────────────

inline const char* DeferredLightVertex = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPos, 1.0);
}
)";

inline const char* DeferredLightFragment = R"(
#version 450 core
out vec4 FragColor;
in vec2 vTexCoord;

// G-Buffer 纹理
uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D gEmissive;

// 方向光
uniform vec3  uDirLightDir;
uniform vec3  uDirLightColor;

// 点光源
#define MAX_PL 8
uniform int   uPLCount;
uniform vec3  uPLPos[MAX_PL];
uniform vec3  uPLColor[MAX_PL];
uniform float uPLIntensity[MAX_PL];
uniform float uPLConstant[MAX_PL];
uniform float uPLLinear[MAX_PL];
uniform float uPLQuadratic[MAX_PL];

// 聚光灯
#define MAX_SL 4
uniform int   uSLCount;
uniform vec3  uSLPos[MAX_SL];
uniform vec3  uSLDir[MAX_SL];
uniform vec3  uSLColor[MAX_SL];
uniform float uSLIntensity[MAX_SL];
uniform float uSLInnerCut[MAX_SL];
uniform float uSLOuterCut[MAX_SL];
uniform float uSLConstant[MAX_SL];
uniform float uSLLinear[MAX_SL];
uniform float uSLQuadratic[MAX_SL];

// 阴影
uniform sampler2D uShadowMap;
uniform int   uShadowEnabled;
uniform mat4  uLightSpaceMat;

// 摄像机
uniform vec3  uViewPos;
uniform float uAmbientStrength;

float CalcShadow(vec3 fragPos, vec3 normal, vec3 lightDir) {
    vec4 lsPos = uLightSpaceMat * vec4(fragPos, 1.0);
    vec3 proj = lsPos.xyz / lsPos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 0.0;

    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float d = texture(uShadowMap, proj.xy + vec2(x,y) * texelSize).r;
            shadow += (proj.z - bias > d) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

void main() {
    // 从 G-Buffer 采样
    vec3  FragPos   = texture(gPosition,   vTexCoord).rgb;
    vec3  Normal    = texture(gNormal,     vTexCoord).rgb;
    vec3  Albedo    = texture(gAlbedoSpec, vTexCoord).rgb;
    float Specular  = texture(gAlbedoSpec, vTexCoord).a;
    vec3  Emissive  = texture(gEmissive,   vTexCoord).rgb;

    // 如果法线为零向量说明该像素没有几何体, 丢弃
    if (dot(Normal, Normal) < 0.001) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 N = normalize(Normal);
    vec3 V = normalize(uViewPos - FragPos);
    float shininess = Specular * 256.0; // 反归一化

    // ── 阴影 ────────────────────────────────────
    vec3 L = normalize(-uDirLightDir);
    float shadow = 0.0;
    if (uShadowEnabled == 1) {
        shadow = CalcShadow(FragPos, N, L);
    }

    // ── 方向光 ──────────────────────────────────
    float diff = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), shininess);
    vec3 result = (uAmbientStrength * Albedo
                + (1.0 - shadow) * (diff * Albedo + spec * vec3(Specular)))
                * uDirLightColor;

    // ── 点光源 ──────────────────────────────────
    for (int i = 0; i < uPLCount; i++) {
        vec3 pL = normalize(uPLPos[i] - FragPos);
        float d = length(uPLPos[i] - FragPos);
        float att = 1.0 / (uPLConstant[i] + uPLLinear[i]*d + uPLQuadratic[i]*d*d);
        float pDiff = max(dot(N, pL), 0.0);
        vec3 pH = normalize(pL + V);
        float pSpec = pow(max(dot(N, pH), 0.0), shininess);
        result += (pDiff * Albedo + pSpec * vec3(Specular))
                * uPLColor[i] * uPLIntensity[i] * att;
    }

    // ── 聚光灯 ──────────────────────────────────
    for (int i = 0; i < uSLCount; i++) {
        vec3 sL = normalize(uSLPos[i] - FragPos);
        float d = length(uSLPos[i] - FragPos);
        float theta = dot(sL, normalize(-uSLDir[i]));
        float epsilon = max(uSLInnerCut[i] - uSLOuterCut[i], 0.001);
        float spotAtt = clamp((theta - uSLOuterCut[i]) / epsilon, 0.0, 1.0);
        float att = 1.0 / (uSLConstant[i] + uSLLinear[i]*d + uSLQuadratic[i]*d*d);
        float sDiff = max(dot(N, sL), 0.0);
        vec3 sH = normalize(sL + V);
        float sSpec = pow(max(dot(N, sH), 0.0), shininess);
        result += (sDiff * Albedo + sSpec * vec3(Specular))
                * uSLColor[i] * uSLIntensity[i] * att * spotAtt;
    }

    // ── 自发光叠加 (HDR) ────────────────────────
    result += Emissive;

    FragColor = vec4(result, 1.0);
}
)";

// ── 延迟渲染: G-Buffer 可视化调试 Shader ──────────────────

inline const char* GBufferDebugVertex = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPos, 1.0);
}
)";

inline const char* GBufferDebugFragment = R"(
#version 450 core
out vec4 FragColor;
in vec2 vTexCoord;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D gEmissive;
uniform int uDebugMode; // 0=Position, 1=Normal, 2=Albedo, 3=Specular, 4=Emissive, 5=Depth

void main() {
    vec3 color;
    if (uDebugMode == 0) {
        color = texture(gPosition, vTexCoord).rgb * 0.1; // 缩放使之可见
    } else if (uDebugMode == 1) {
        color = texture(gNormal, vTexCoord).rgb * 0.5 + 0.5; // [-1,1] -> [0,1]
    } else if (uDebugMode == 2) {
        color = texture(gAlbedoSpec, vTexCoord).rgb;
    } else if (uDebugMode == 3) {
        float s = texture(gAlbedoSpec, vTexCoord).a;
        color = vec3(s);
    } else if (uDebugMode == 4) {
        color = texture(gEmissive, vTexCoord).rgb;
    }
    FragColor = vec4(color, 1.0);
}
)";

}} // namespace Engine::Shaders

