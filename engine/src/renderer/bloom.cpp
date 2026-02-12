#include "engine/renderer/bloom.h"
#include "engine/renderer/screen_quad.h"
#include "engine/core/log.h"

#include <glad/glad.h>

namespace Engine {

u32 Bloom::s_BrightFBO = 0;
u32 Bloom::s_BrightTexture = 0;
u32 Bloom::s_PingFBO = 0;
u32 Bloom::s_PingTexture = 0;
u32 Bloom::s_PongFBO = 0;
u32 Bloom::s_PongTexture = 0;
// QuadVAO/VBO 已迁移至 ScreenQuad，此处不再需要
Ref<Shader> Bloom::s_BrightShader = nullptr;
Ref<Shader> Bloom::s_BlurShader = nullptr;
u32 Bloom::s_Width = 0;
u32 Bloom::s_Height = 0;
f32 Bloom::s_Threshold = 1.0f;
f32 Bloom::s_Intensity = 0.5f;
u32 Bloom::s_Iterations = 5;
bool Bloom::s_Enabled = true;

// ── 亮度提取着色器 ──────────────────────────────────────────

static const char* brightVert = R"(
#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

static const char* brightFrag = R"(
#version 450 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uHDRBuffer;
uniform float uThreshold;

void main() {
    vec3 color = texture(uHDRBuffer, vTexCoord).rgb;
    // 计算亮度 (感知亮度公式)
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    if (brightness > uThreshold) {
        FragColor = vec4(color, 1.0);
    } else {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
)";

// ── 高斯模糊着色器 ──────────────────────────────────────────

static const char* blurVert = R"(
#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

static const char* blurFrag = R"(
#version 450 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uImage;
uniform bool uHorizontal;

// 13-tap 高斯核（适合 Bloom，柔和效果）
const float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec2 texelSize = 1.0 / textureSize(uImage, 0);
    vec3 result = texture(uImage, vTexCoord).rgb * weight[0];

    if (uHorizontal) {
        for (int i = 1; i < 5; i++) {
            result += texture(uImage, vTexCoord + vec2(texelSize.x * i, 0.0)).rgb * weight[i];
            result += texture(uImage, vTexCoord - vec2(texelSize.x * i, 0.0)).rgb * weight[i];
        }
    } else {
        for (int i = 1; i < 5; i++) {
            result += texture(uImage, vTexCoord + vec2(0.0, texelSize.y * i)).rgb * weight[i];
            result += texture(uImage, vTexCoord - vec2(0.0, texelSize.y * i)).rgb * weight[i];
        }
    }

    FragColor = vec4(result, 1.0);
}
)";

// ── 初始化 ──────────────────────────────────────────────────

void Bloom::Init(u32 width, u32 height) {
    s_Width = width;
    s_Height = height;

    // 使用共享 ScreenQuad
    ScreenQuad::Init();

    s_BrightShader = std::make_shared<Shader>(brightVert, brightFrag);
    s_BlurShader = std::make_shared<Shader>(blurVert, blurFrag);

    CreateFBOs(width, height);

    LOG_INFO("[Bloom] 初始化完成 (%ux%u, 阈值=%.1f, 强度=%.1f, 迭代=%u)",
             width, height, s_Threshold, s_Intensity, s_Iterations);
}

void Bloom::Shutdown() {
    // ScreenQuad 由外部统一管理生命周期，不在此释放
    if (s_BrightFBO)    { glDeleteFramebuffers(1, &s_BrightFBO); s_BrightFBO = 0; }
    if (s_BrightTexture){ glDeleteTextures(1, &s_BrightTexture); s_BrightTexture = 0; }
    if (s_PingFBO)      { glDeleteFramebuffers(1, &s_PingFBO);   s_PingFBO = 0; }
    if (s_PingTexture)  { glDeleteTextures(1, &s_PingTexture);   s_PingTexture = 0; }
    if (s_PongFBO)      { glDeleteFramebuffers(1, &s_PongFBO);   s_PongFBO = 0; }
    if (s_PongTexture)  { glDeleteTextures(1, &s_PongTexture);   s_PongTexture = 0; }
    s_BrightShader.reset();
    s_BlurShader.reset();
    LOG_DEBUG("[Bloom] 已清理");
}

// ── 创建内部 FBO ────────────────────────────────────────────

void Bloom::CreateFBOs(u32 width, u32 height) {
    // 清除旧的
    if (s_BrightFBO)    { glDeleteFramebuffers(1, &s_BrightFBO); s_BrightFBO = 0; }
    if (s_BrightTexture){ glDeleteTextures(1, &s_BrightTexture); s_BrightTexture = 0; }
    if (s_PingFBO)      { glDeleteFramebuffers(1, &s_PingFBO);   s_PingFBO = 0; }
    if (s_PingTexture)  { glDeleteTextures(1, &s_PingTexture);   s_PingTexture = 0; }
    if (s_PongFBO)      { glDeleteFramebuffers(1, &s_PongFBO);   s_PongFBO = 0; }
    if (s_PongTexture)  { glDeleteTextures(1, &s_PongTexture);   s_PongTexture = 0; }

    // Bloom 在半分辨率工作（性能优化）
    u32 bw = width / 2;
    u32 bh = height / 2;
    if (bw == 0) bw = 1;
    if (bh == 0) bh = 1;

    auto createFBO = [](u32& fbo, u32& tex, u32 w, u32 h) {
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    };

    createFBO(s_BrightFBO, s_BrightTexture, bw, bh);
    createFBO(s_PingFBO, s_PingTexture, bw, bh);
    createFBO(s_PongFBO, s_PongTexture, bw, bh);

    s_Width = width;
    s_Height = height;
}

// ── Bloom 处理 ──────────────────────────────────────────────

u32 Bloom::Process(u32 hdrInputTexture) {
    if (!s_Enabled || !s_BrightShader || !s_BlurShader) return 0;

    u32 bw = s_Width / 2;
    u32 bh = s_Height / 2;
    if (bw == 0) bw = 1;
    if (bh == 0) bh = 1;

    // ── Pass 1: 亮度提取 ────────────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, s_BrightFBO);
    glViewport(0, 0, bw, bh);
    glClear(GL_COLOR_BUFFER_BIT);

    s_BrightShader->Bind();
    s_BrightShader->SetInt("uHDRBuffer", 0);
    s_BrightShader->SetFloat("uThreshold", s_Threshold);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrInputTexture);

    glDisable(GL_DEPTH_TEST);
    ScreenQuad::Draw();

    // ── Pass 2: Ping-Pong 高斯模糊 ──────────────────────────
    s_BlurShader->Bind();
    s_BlurShader->SetInt("uImage", 0);

    bool horizontal = true;
    u32 inputTex = s_BrightTexture;

    glBindVertexArray(ScreenQuad::GetVAO());  // 保持绑定，循环内直接 draw
    for (u32 i = 0; i < s_Iterations * 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, horizontal ? s_PingFBO : s_PongFBO);
        s_BlurShader->SetInt("uHorizontal", horizontal ? 1 : 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, inputTex);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        inputTex = horizontal ? s_PingTexture : s_PongTexture;
        horizontal = !horizontal;
    }
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 返回最后写入的纹理
    return inputTex;
}

void Bloom::Resize(u32 width, u32 height) {
    CreateFBOs(width, height);
}

// ── 参数控制 ────────────────────────────────────────────────

void Bloom::SetThreshold(f32 threshold) { s_Threshold = threshold; }
f32 Bloom::GetThreshold() { return s_Threshold; }
void Bloom::SetIntensity(f32 intensity) { s_Intensity = intensity; }
f32 Bloom::GetIntensity() { return s_Intensity; }
void Bloom::SetIterations(u32 iterations) { s_Iterations = iterations; }
u32 Bloom::GetIterations() { return s_Iterations; }
void Bloom::SetEnabled(bool enabled) { s_Enabled = enabled; }
bool Bloom::IsEnabled() { return s_Enabled; }
u32 Bloom::GetBloomTexture() { return s_PongTexture; }

} // namespace Engine
