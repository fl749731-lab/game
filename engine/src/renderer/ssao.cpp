#include "engine/renderer/ssao.h"
#include "engine/renderer/g_buffer.h"
#include "engine/renderer/screen_quad.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <vector>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

u32 SSAO::s_SSAO_FBO = 0;
u32 SSAO::s_SSAO_Texture = 0;
u32 SSAO::s_Blur_FBO = 0;
u32 SSAO::s_Blur_Texture = 0;
u32 SSAO::s_NoiseTex = 0;

Ref<Shader> SSAO::s_SSAOShader = nullptr;
Ref<Shader> SSAO::s_BlurShader = nullptr;

u32 SSAO::s_Width = 0;
u32 SSAO::s_Height = 0;
f32 SSAO::s_Radius = 0.5f;
f32 SSAO::s_Bias = 0.025f;
f32 SSAO::s_Intensity = 1.5f;
bool SSAO::s_Enabled = true;

// ── 采样核 ──────────────────────────────────────────────────

static std::vector<glm::vec3> s_Kernel;
static const int KERNEL_SIZE = 32;

// ── SSAO 着色器 ─────────────────────────────────────────────

static const char* ssaoVert = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 1.0);
}
)";

static const char* ssaoFrag = R"(
#version 450 core
in vec2 vUV;
out float FragColor;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D uNoiseTex;

uniform vec3 uSamples[32];
uniform mat4 uProjection;
uniform vec2 uNoiseScale;
uniform float uRadius;
uniform float uBias;
uniform float uIntensity;

void main() {
    vec3 fragPos = texture(gPosition, vUV).xyz;
    vec3 normal  = normalize(texture(gNormal, vUV).xyz);
    vec3 randVec = normalize(texture(uNoiseTex, vUV * uNoiseScale).xyz);

    // Gram-Schmidt: 构造 TBN 从切线空间到视空间
    vec3 tangent   = normalize(randVec - normal * dot(randVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < 32; i++) {
        // 采样点在世界空间
        vec3 samplePos = fragPos + TBN * uSamples[i] * uRadius;

        // 投影到屏幕空间
        vec4 offset = uProjection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz  = offset.xyz * 0.5 + 0.5;

        // 深度比较
        float sampleDepth = texture(gPosition, offset.xy).z;

        // 范围检查 — 避免远处物体影响
        float rangeCheck = smoothstep(0.0, 1.0, uRadius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + uBias ? 1.0 : 0.0) * rangeCheck;
    }
    occlusion = 1.0 - (occlusion / 32.0) * uIntensity;
    FragColor = occlusion;
}
)";

// ── 模糊着色器 ──────────────────────────────────────────────

static const char* blurVert = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 1.0);
}
)";

static const char* blurFrag = R"(
#version 450 core
in vec2 vUV;
out float FragColor;

uniform sampler2D uSSAO;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(uSSAO, 0));
    float result = 0.0;
    for (int x = -2; x < 2; x++) {
        for (int y = -2; y < 2; y++) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(uSSAO, vUV + offset).r;
        }
    }
    FragColor = result / 16.0;
}
)";

// ── 初始化 ──────────────────────────────────────────────────

void SSAO::Init(u32 width, u32 height) {
    s_Width = width;
    s_Height = height;

    // 创建采样核和噪声纹理
    CreateKernelAndNoise();

    // SSAO FBO (单通道 R16F)
    glGenFramebuffers(1, &s_SSAO_FBO);
    glGenTextures(1, &s_SSAO_Texture);

    glBindTexture(GL_TEXTURE_2D, s_SSAO_Texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, s_SSAO_FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_SSAO_Texture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 模糊 FBO
    glGenFramebuffers(1, &s_Blur_FBO);
    glGenTextures(1, &s_Blur_Texture);

    glBindTexture(GL_TEXTURE_2D, s_Blur_Texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, s_Blur_FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_Blur_Texture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 着色器
    s_SSAOShader = std::make_shared<Shader>(ssaoVert, ssaoFrag);
    s_BlurShader = std::make_shared<Shader>(blurVert, blurFrag);

    // 设置采样核 uniform
    s_SSAOShader->Bind();
    for (int i = 0; i < KERNEL_SIZE; i++) {
        char name[32];
        snprintf(name, sizeof(name), "uSamples[%d]", i);
        s_SSAOShader->SetVec3(name, s_Kernel[i].x, s_Kernel[i].y, s_Kernel[i].z);
    }
    s_SSAOShader->SetInt("gPosition", 0);
    s_SSAOShader->SetInt("gNormal", 1);
    s_SSAOShader->SetInt("uNoiseTex", 2);

    LOG_INFO("[SSAO] 初始化完成 (%ux%u, %d 采样核)", width, height, KERNEL_SIZE);
}

void SSAO::Shutdown() {
    if (s_SSAO_FBO) { glDeleteFramebuffers(1, &s_SSAO_FBO); s_SSAO_FBO = 0; }
    if (s_SSAO_Texture) { glDeleteTextures(1, &s_SSAO_Texture); s_SSAO_Texture = 0; }
    if (s_Blur_FBO) { glDeleteFramebuffers(1, &s_Blur_FBO); s_Blur_FBO = 0; }
    if (s_Blur_Texture) { glDeleteTextures(1, &s_Blur_Texture); s_Blur_Texture = 0; }
    if (s_NoiseTex) { glDeleteTextures(1, &s_NoiseTex); s_NoiseTex = 0; }
    s_SSAOShader.reset();
    s_BlurShader.reset();
}

void SSAO::Resize(u32 width, u32 height) {
    s_Width = width;
    s_Height = height;

    glBindTexture(GL_TEXTURE_2D, s_SSAO_Texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, nullptr);

    glBindTexture(GL_TEXTURE_2D, s_Blur_Texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, nullptr);
}

// ── 生成 SSAO ───────────────────────────────────────────────

void SSAO::Generate(const f32* projectionMatrix) {
    if (!s_Enabled) return;

    // Pass 1: SSAO
    glBindFramebuffer(GL_FRAMEBUFFER, s_SSAO_FBO);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, s_Width, s_Height);

    s_SSAOShader->Bind();
    s_SSAOShader->SetMat4("uProjection", projectionMatrix);
    s_SSAOShader->SetFloat("uRadius", s_Radius);
    s_SSAOShader->SetFloat("uBias", s_Bias);
    s_SSAOShader->SetFloat("uIntensity", s_Intensity);
    s_SSAOShader->SetVec2("uNoiseScale",
        (f32)s_Width / 4.0f, (f32)s_Height / 4.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, GBuffer::GetPositionTexture());
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, GBuffer::GetNormalTexture());
    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, s_NoiseTex);

    ScreenQuad::Draw();

    // Pass 2: 模糊
    glBindFramebuffer(GL_FRAMEBUFFER, s_Blur_FBO);
    glClear(GL_COLOR_BUFFER_BIT);

    s_BlurShader->Bind();
    s_BlurShader->SetInt("uSSAO", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_SSAO_Texture);

    ScreenQuad::Draw();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

u32 SSAO::GetOcclusionTexture() {
    return s_Blur_Texture;
}

// ── 采样核和噪声 ────────────────────────────────────────────

void SSAO::CreateKernelAndNoise() {
    std::mt19937 rng(42);
    std::uniform_real_distribution<f32> dist(0.0f, 1.0f);

    // 半球采样核
    s_Kernel.resize(KERNEL_SIZE);
    for (int i = 0; i < KERNEL_SIZE; i++) {
        glm::vec3 sample(
            dist(rng) * 2.0f - 1.0f,
            dist(rng) * 2.0f - 1.0f,
            dist(rng)              // 半球: z > 0
        );
        sample = glm::normalize(sample) * dist(rng);

        // 加速插值: 靠近原点的采样权重更大
        f32 scale = (f32)i / (f32)KERNEL_SIZE;
        scale = 0.1f + 0.9f * scale * scale; // lerp(0.1, 1.0, scale^2)
        sample *= scale;

        s_Kernel[i] = sample;
    }

    // 4x4 随机旋转噪声纹理
    std::vector<glm::vec3> noise(16);
    for (int i = 0; i < 16; i++) {
        noise[i] = {
            dist(rng) * 2.0f - 1.0f,
            dist(rng) * 2.0f - 1.0f,
            0.0f
        };
    }

    glGenTextures(1, &s_NoiseTex);
    glBindTexture(GL_TEXTURE_2D, s_NoiseTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGB, GL_FLOAT, noise.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

// ── 参数控制 ────────────────────────────────────────────────

void SSAO::SetRadius(f32 radius) { s_Radius = radius; }
void SSAO::SetBias(f32 bias) { s_Bias = bias; }
void SSAO::SetIntensity(f32 intensity) { s_Intensity = intensity; }
void SSAO::SetEnabled(bool enabled) { s_Enabled = enabled; }
bool SSAO::IsEnabled() { return s_Enabled; }

} // namespace Engine
