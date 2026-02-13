#include "engine/renderer/ssr.h"
#include "engine/renderer/g_buffer.h"
#include "engine/renderer/screen_quad.h"
#include "engine/renderer/shader.h"
#include "engine/core/resource_manager.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

u32  SSR::s_FBO = 0;
u32  SSR::s_ReflectionTex = 0;
u32  SSR::s_Width = 0;
u32  SSR::s_Height = 0;
bool SSR::s_Enabled = true;
i32  SSR::s_MaxSteps = 64;
f32  SSR::s_StepSize = 0.05f;
f32  SSR::s_Thickness = 0.5f;

// ── SSR Shader (内联) ───────────────────────────────────────

static const char* s_SSRVertex = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPos, 1.0);
}
)";

static const char* s_SSRFragment = R"(
#version 450 core
out vec4 FragColor;
in vec2 vTexCoord;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;   // a = metallic
uniform sampler2D gEmissive;     // a = roughness
uniform sampler2D uSceneColor;   // HDR 场景颜色

uniform mat4  uProjection;
uniform mat4  uView;
uniform int   uMaxSteps;
uniform float uStepSize;
uniform float uThickness;
uniform vec2  uScreenSize;

// 将世界空间位置转换为屏幕 UV
vec2 worldToUV(vec3 worldPos, out float depth) {
    vec4 clipPos = uProjection * uView * vec4(worldPos, 1.0);
    vec3 ndc = clipPos.xyz / clipPos.w;
    depth = ndc.z;
    return ndc.xy * 0.5 + 0.5;
}

void main() {
    vec3 FragPos  = texture(gPosition,  vTexCoord).rgb;
    vec3 Normal   = texture(gNormal,    vTexCoord).rgb;
    float Metallic  = texture(gAlbedoSpec, vTexCoord).a;
    float Roughness = texture(gEmissive,   vTexCoord).a;

    // 只有足够光滑、金属度较高的表面才生成反射
    if (dot(Normal, Normal) < 0.001 || Roughness > 0.7 || Metallic < 0.1) {
        FragColor = vec4(0.0);
        return;
    }

    vec3 N = normalize(Normal);

    // 视图空间中的反射方向
    vec3 viewPos = (uView * vec4(FragPos, 1.0)).xyz;
    vec3 viewN   = normalize(mat3(uView) * N);
    vec3 viewDir = normalize(viewPos);
    vec3 reflectDir = reflect(viewDir, viewN);

    // 光线步进 (Ray Marching)
    vec3 rayPos = viewPos;
    vec3 rayStep = reflectDir * uStepSize;

    for (int i = 0; i < uMaxSteps; i++) {
        rayPos += rayStep;

        // 投影回屏幕空间
        vec4 projPos = uProjection * vec4(rayPos, 1.0);
        vec3 ndc = projPos.xyz / projPos.w;
        vec2 sampleUV = ndc.xy * 0.5 + 0.5;

        // 边界检查
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 ||
            sampleUV.y < 0.0 || sampleUV.y > 1.0) {
            break;
        }

        // 采样该位置的 G-Buffer 深度
        vec3 sampledPos = texture(gPosition, sampleUV).rgb;
        vec3 sampledView = (uView * vec4(sampledPos, 1.0)).xyz;

        // 深度比较
        float depthDiff = rayPos.z - sampledView.z;
        if (depthDiff > 0.0 && depthDiff < uThickness) {
            // 命中！采样场景颜色作为反射
            vec3 reflColor = texture(uSceneColor, sampleUV).rgb;

            // 根据粗糙度和步进距离衰减
            float fadeStep = 1.0 - float(i) / float(uMaxSteps);
            float fadeRoughness = 1.0 - Roughness;
            float fadeEdge = 1.0 - max(
                abs(sampleUV.x - 0.5) * 2.0,
                abs(sampleUV.y - 0.5) * 2.0
            );
            fadeEdge = clamp(fadeEdge * 4.0, 0.0, 1.0);

            float alpha = fadeStep * fadeRoughness * fadeEdge * Metallic;

            FragColor = vec4(reflColor, alpha);
            return;
        }
    }

    // 未命中
    FragColor = vec4(0.0);
}
)";

static Ref<Shader> s_SSRShader;

// ── 初始化/释放 ─────────────────────────────────────────────

void SSR::Init(u32 width, u32 height) {
    s_Width = width;
    s_Height = height;

    // 创建 SSR FBO + 纹理
    glGenFramebuffers(1, &s_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, s_FBO);

    glGenTextures(1, &s_ReflectionTex);
    glBindTexture(GL_TEXTURE_2D, s_ReflectionTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0,
                 GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, s_ReflectionTex, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 编译 Shader
    s_SSRShader = ResourceManager::LoadShader("ssr", s_SSRVertex, s_SSRFragment);

    LOG_INFO("[SSR] 初始化完成 (%ux%u), 最大步进=%d", width, height, s_MaxSteps);
}

void SSR::Shutdown() {
    if (s_FBO) { glDeleteFramebuffers(1, &s_FBO); s_FBO = 0; }
    if (s_ReflectionTex) { glDeleteTextures(1, &s_ReflectionTex); s_ReflectionTex = 0; }
    s_SSRShader.reset();
}

void SSR::Resize(u32 width, u32 height) {
    s_Width = width;
    s_Height = height;
    if (s_ReflectionTex) {
        glBindTexture(GL_TEXTURE_2D, s_ReflectionTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0,
                     GL_RGBA, GL_FLOAT, nullptr);
    }
}

// ── 生成反射 ────────────────────────────────────────────────

void SSR::Generate(const f32* projMatrix, const f32* viewMatrix,
                   u32 hdrTexture) {
    if (!s_Enabled || !s_SSRShader) return;

    glBindFramebuffer(GL_FRAMEBUFFER, s_FBO);
    glViewport(0, 0, s_Width, s_Height);
    glClear(GL_COLOR_BUFFER_BIT);

    s_SSRShader->Bind();
    s_SSRShader->SetMat4("uProjection", projMatrix);
    s_SSRShader->SetMat4("uView", viewMatrix);
    s_SSRShader->SetInt("uMaxSteps", s_MaxSteps);
    s_SSRShader->SetFloat("uStepSize", s_StepSize);
    s_SSRShader->SetFloat("uThickness", s_Thickness);
    s_SSRShader->SetVec2("uScreenSize", (f32)s_Width, (f32)s_Height);

    // 绑定 G-Buffer 纹理
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, GBuffer::GetPositionTexture());
    s_SSRShader->SetInt("gPosition", 0);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, GBuffer::GetNormalTexture());
    s_SSRShader->SetInt("gNormal", 1);

    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, GBuffer::GetAlbedoSpecTexture());
    s_SSRShader->SetInt("gAlbedoSpec", 2);

    glActiveTexture(GL_TEXTURE0 + 3);
    glBindTexture(GL_TEXTURE_2D, GBuffer::GetEmissiveTexture());
    s_SSRShader->SetInt("gEmissive", 3);

    glActiveTexture(GL_TEXTURE0 + 4);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);
    s_SSRShader->SetInt("uSceneColor", 4);

    ScreenQuad::Draw();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ── 访问器 ──────────────────────────────────────────────────

u32  SSR::GetReflectionTexture() { return s_ReflectionTex; }
bool SSR::IsEnabled()            { return s_Enabled; }
void SSR::SetEnabled(bool e)     { s_Enabled = e; }
void SSR::SetMaxSteps(i32 s)     { s_MaxSteps = s; }
void SSR::SetStepSize(f32 s)     { s_StepSize = s; }
void SSR::SetThickness(f32 t)    { s_Thickness = t; }

} // namespace Engine
