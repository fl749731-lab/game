#include "engine/renderer/ibl.h"
#include "engine/renderer/screen_quad.h"
#include "engine/core/resource_manager.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

u32 IBL::s_EnvCubemap     = 0;
u32 IBL::s_IrradianceMap  = 0;
u32 IBL::s_PrefilterMap   = 0;
u32 IBL::s_BRDF_LUT       = 0;
u32 IBL::s_CaptureFBO     = 0;
u32 IBL::s_CaptureRBO     = 0;
Ref<Shader> IBL::s_EquirectShader   = nullptr;
Ref<Shader> IBL::s_IrradianceShader = nullptr;
Ref<Shader> IBL::s_PrefilterShader  = nullptr;
Ref<Shader> IBL::s_BRDFShader       = nullptr;
f32  IBL::s_Intensity = 1.0f;
bool IBL::s_Ready     = false;

// ── BRDF LUT 着色器 ─────────────────────────────────────────

static const char* s_BRDF_VS = R"(
#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

static const char* s_BRDF_FS = R"(
#version 450 core
out vec2 FragColor;
in vec2 vUV;

const float PI = 3.14159265359;

// Van der Corput 低差异序列
float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    vec3 up = abs(N.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

void main() {
    float NdotV = vUV.x;
    float roughness = vUV.y;

    vec3 V = vec3(sqrt(1.0 - NdotV*NdotV), 0.0, NdotV);
    vec3 N = vec3(0.0, 0.0, 1.0);

    float A = 0.0, B = 0.0;
    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; i++) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0) {
            float G = GeometrySmith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);
            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    FragColor = vec2(A, B);
}
)";

// ── 初始化 ──────────────────────────────────────────────────

void IBL::Init() {
    // 创建离屏捕获 FBO
    glGenFramebuffers(1, &s_CaptureFBO);
    glGenRenderbuffers(1, &s_CaptureRBO);

    // 编译 BRDF LUT shader
    s_BRDFShader = ResourceManager::LoadShader("ibl_brdf", s_BRDF_VS, s_BRDF_FS);

    // 生成 BRDF LUT
    GenerateBRDF_LUT();

    LOG_INFO("[IBL] 初始化完成 (BRDF LUT: %u)", s_BRDF_LUT);
}

void IBL::Shutdown() {
    if (s_EnvCubemap)    { glDeleteTextures(1, &s_EnvCubemap);    s_EnvCubemap = 0;    }
    if (s_IrradianceMap) { glDeleteTextures(1, &s_IrradianceMap); s_IrradianceMap = 0; }
    if (s_PrefilterMap)  { glDeleteTextures(1, &s_PrefilterMap);  s_PrefilterMap = 0;  }
    if (s_BRDF_LUT)      { glDeleteTextures(1, &s_BRDF_LUT);     s_BRDF_LUT = 0;     }
    if (s_CaptureFBO)    { glDeleteFramebuffers(1, &s_CaptureFBO);  s_CaptureFBO = 0;  }
    if (s_CaptureRBO)    { glDeleteRenderbuffers(1, &s_CaptureRBO); s_CaptureRBO = 0;  }

    s_EquirectShader.reset();
    s_IrradianceShader.reset();
    s_PrefilterShader.reset();
    s_BRDFShader.reset();
    s_Ready = false;

    LOG_DEBUG("[IBL] 已清理");
}

// ── BRDF 积分 LUT ──────────────────────────────────────────

void IBL::GenerateBRDF_LUT() {
    glGenTextures(1, &s_BRDF_LUT);
    glBindTexture(GL_TEXTURE_2D, s_BRDF_LUT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, s_CaptureFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_BRDF_LUT, 0);

    glViewport(0, 0, 512, 512);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    s_BRDFShader->Bind();
    ScreenQuad::Draw();

    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ── 绑定 IBL 纹理 ──────────────────────────────────────────

void IBL::Bind(u32 irradianceUnit, u32 prefilterUnit, u32 brdfLUTUnit) {
    if (!s_Ready && s_BRDF_LUT == 0) return;

    if (s_IrradianceMap) {
        glActiveTexture(GL_TEXTURE0 + irradianceUnit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, s_IrradianceMap);
    }
    if (s_PrefilterMap) {
        glActiveTexture(GL_TEXTURE0 + prefilterUnit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, s_PrefilterMap);
    }
    if (s_BRDF_LUT) {
        glActiveTexture(GL_TEXTURE0 + brdfLUTUnit);
        glBindTexture(GL_TEXTURE_2D, s_BRDF_LUT);
    }
}

void IBL::SetUniforms(Shader* shader, u32 irradianceUnit, u32 prefilterUnit, u32 brdfLUTUnit) {
    shader->SetInt("uIrradianceMap", (int)irradianceUnit);
    shader->SetInt("uPrefilterMap", (int)prefilterUnit);
    shader->SetInt("uBRDF_LUT", (int)brdfLUTUnit);
    shader->SetFloat("uIBLIntensity", s_Intensity);
}

// ── 环境贴图加载 (桩) ───────────────────────────────────────
// 完整实现需要 stb_image HDR 加载 + equirect-to-cubemap 转换

void IBL::LoadEnvironmentMap(const char* hdrPath) {
    LOG_INFO("[IBL] 加载环境贴图: %s (需完整 HDR 加载实现)", hdrPath);
    // TODO: stbi_loadf → equirect-to-cubemap → irradiance → prefilter
    // 目前仅标记为已就绪以便测试
    s_Ready = true;
}

void IBL::ComputeFromCubemap(u32 envCubemap) {
    s_EnvCubemap = envCubemap;
    // TODO: GenerateIrradianceMap(envCubemap);
    // TODO: GeneratePrefilterMap(envCubemap);
    s_Ready = true;
    LOG_INFO("[IBL] 从立方体贴图 %u 计算 IBL (需完整实现)", envCubemap);
}

void IBL::GenerateIrradianceMap(u32 /*envCubemap*/) {
    // TODO: 辐照度卷积
}

void IBL::GeneratePrefilterMap(u32 /*envCubemap*/) {
    // TODO: 预滤波 (多 mip)
}

u32 IBL::EquirectToCubemap(u32 /*hdrTexture*/) {
    // TODO: 等距柱状投影 → 立方体贴图
    return 0;
}

} // namespace Engine


