#include "engine/renderer/post_process.h"
#include "engine/renderer/screen_quad.h"
#include "engine/core/log.h"

#include <glad/glad.h>

namespace Engine {

// QuadVAO/VBO 已迁移至 ScreenQuad，此处不再需要
Ref<Shader> PostProcess::s_Shader = nullptr;
f32 PostProcess::s_Exposure = 1.0f;
f32 PostProcess::s_Gamma = 2.2f;
f32 PostProcess::s_VignetteStrength = 0.3f;

static const char* ppVertSrc = R"(
#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

static const char* ppFragSrc = R"(
#version 450 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uScreen;
uniform sampler2D uBloomTex;
uniform float uExposure;
uniform float uGamma;
uniform float uVignetteStrength;
uniform float uBloomIntensity;
uniform int uBloomEnabled;

void main() {
    vec3 color = texture(uScreen, vTexCoord).rgb;

    // Bloom 混合
    if (uBloomEnabled == 1) {
        vec3 bloom = texture(uBloomTex, vTexCoord).rgb;
        color += bloom * uBloomIntensity;
    }

    // 色调映射 (Reinhard)
    color = vec3(1.0) - exp(-color * uExposure);

    // 伽马校正
    color = pow(color, vec3(1.0 / uGamma));

    // 暗角效果
    vec2 center = vTexCoord - 0.5;
    float vignette = 1.0 - dot(center, center) * uVignetteStrength * 4.0;
    color *= clamp(vignette, 0.0, 1.0);

    FragColor = vec4(color, 1.0);
}
)";

void PostProcess::Init() {
    // 使用共享 ScreenQuad
    ScreenQuad::Init();

    s_Shader = std::make_shared<Shader>(ppVertSrc, ppFragSrc);
    LOG_INFO("[后处理] 初始化完成");
}

void PostProcess::Shutdown() {
    // ScreenQuad 由外部统一管理生命周期
    s_Shader.reset();
    LOG_DEBUG("[后处理] 已清理");
}

void PostProcess::Draw(u32 sourceTextureID) {
    Draw(sourceTextureID, 0, 0.0f);
}

void PostProcess::Draw(u32 sourceTextureID, u32 bloomTextureID, f32 bloomIntensity) {
    s_Shader->Bind();
    s_Shader->SetInt("uScreen", 0);
    s_Shader->SetFloat("uExposure", s_Exposure);
    s_Shader->SetFloat("uGamma", s_Gamma);
    s_Shader->SetFloat("uVignetteStrength", s_VignetteStrength);

    if (bloomTextureID != 0 && bloomIntensity > 0.0f) {
        s_Shader->SetInt("uBloomEnabled", 1);
        s_Shader->SetFloat("uBloomIntensity", bloomIntensity);
        s_Shader->SetInt("uBloomTex", 1);
        glActiveTexture(GL_TEXTURE0 + 1);
        glBindTexture(GL_TEXTURE_2D, bloomTextureID);
    } else {
        s_Shader->SetInt("uBloomEnabled", 0);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTextureID);

    glDisable(GL_DEPTH_TEST);
    ScreenQuad::Draw();
    glEnable(GL_DEPTH_TEST);
}

Ref<Shader> PostProcess::GetShader() { return s_Shader; }
void PostProcess::SetExposure(f32 exposure) { s_Exposure = exposure; }
void PostProcess::SetGamma(f32 gamma) { s_Gamma = gamma; }
void PostProcess::SetVignetteStrength(f32 strength) { s_VignetteStrength = strength; }

} // namespace Engine
