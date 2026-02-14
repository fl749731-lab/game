#include "engine/renderer/volumetric.h"
#include "engine/renderer/cascaded_shadow_map.h"
#include "engine/renderer/screen_quad.h"
#include "engine/core/resource_manager.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

Scope<Framebuffer> VolumetricLighting::s_HalfResFBO = nullptr;
Ref<Shader> VolumetricLighting::s_RayMarchShader = nullptr;
Ref<Shader> VolumetricLighting::s_CompositeShader = nullptr;
u32 VolumetricLighting::s_HalfWidth = 0;
u32 VolumetricLighting::s_HalfHeight = 0;
VolumetricConfig VolumetricLighting::s_Config = {};

// ── Ray-march 着色器源码 ────────────────────────────────────

static const char* s_RayMarchVS = R"(
#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

static const char* s_RayMarchFS = R"(
#version 450 core
out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uDepthTex;
uniform sampler2D uCSMShadowMap[4];
uniform mat4  uCSMLightSpace[4];
uniform float uCSMSplitDist[4];
uniform mat4  uInvViewProj;
uniform mat4  uView;
uniform vec3  uLightDir;
uniform vec3  uLightColor;
uniform vec3  uCamPos;
uniform float uMaxDist;
uniform int   uSteps;
uniform float uDensity;
uniform float uScattering;
uniform float uHeightFalloff;
uniform float uBaseHeight;
uniform vec3  uFogColor;
uniform float uLightIntensity;

// Henyey-Greenstein 相函数
float HGPhase(float cosTheta, float g) {
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * 3.14159265 * pow(denom, 1.5));
}

// 从深度缓冲重建世界位置
vec3 WorldPosFromDepth(vec2 uv, float depth) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldPos = uInvViewProj * clipPos;
    return worldPos.xyz / worldPos.w;
}

// 简单的 CSM 采样 (体积光用，无 PCF)
float SampleCSMShadow(vec3 worldPos) {
    vec4 viewPos = uView * vec4(worldPos, 1.0);
    float d = -viewPos.z;
    int cascade = 0;
    for (int i = 0; i < 4; i++) {
        if (d < uCSMSplitDist[i]) { cascade = i; break; }
        cascade = i;
    }
    vec4 lsPos = uCSMLightSpace[cascade] * vec4(worldPos, 1.0);
    vec3 proj = lsPos.xyz / lsPos.w * 0.5 + 0.5;
    if (proj.z > 1.0) return 0.0;

    float shadowDepth;
    if (cascade == 0) shadowDepth = texture(uCSMShadowMap[0], proj.xy).r;
    else if (cascade == 1) shadowDepth = texture(uCSMShadowMap[1], proj.xy).r;
    else if (cascade == 2) shadowDepth = texture(uCSMShadowMap[2], proj.xy).r;
    else shadowDepth = texture(uCSMShadowMap[3], proj.xy).r;

    return (proj.z - 0.005 > shadowDepth) ? 0.0 : 1.0;
}

void main() {
    float depth = texture(uDepthTex, vUV).r;
    vec3 worldPos = WorldPosFromDepth(vUV, depth);
    vec3 rayDir = normalize(worldPos - uCamPos);

    float maxDist = min(length(worldPos - uCamPos), uMaxDist);
    float stepSize = maxDist / float(uSteps);

    vec3 accumulated = vec3(0.0);
    float transmittance = 1.0;

    float cosAngle = dot(rayDir, normalize(-uLightDir));
    float phase = HGPhase(cosAngle, uScattering);

    for (int i = 0; i < uSteps; i++) {
        float t = (float(i) + 0.5) * stepSize;
        vec3 samplePos = uCamPos + rayDir * t;

        // 高度衰减
        float heightFactor = exp(-max(samplePos.y - uBaseHeight, 0.0) * uHeightFalloff);
        float localDensity = uDensity * heightFactor;

        // 阴影采样（在光照处才有散射）
        float shadow = SampleCSMShadow(samplePos);

        // 散射贡献
        vec3 scattering = shadow * phase * uLightColor * uLightIntensity;
        vec3 ambient = uFogColor * 0.05;

        accumulated += transmittance * (scattering + ambient) * localDensity * stepSize;
        transmittance *= exp(-localDensity * stepSize);

        if (transmittance < 0.01) break;
    }

    FragColor = vec4(accumulated, 1.0 - transmittance);
}
)";

// ── 合成着色器 ──────────────────────────────────────────────

static const char* s_CompositeVS = R"(
#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

static const char* s_CompositeFS = R"(
#version 450 core
out vec4 FragColor;
in vec2 vUV;
uniform sampler2D uHDR;
uniform sampler2D uVolumetric;

void main() {
    vec3 hdr = texture(uHDR, vUV).rgb;
    vec4 vol = texture(uVolumetric, vUV);
    // 体积雾合成: fog + scene * transmittance
    vec3 result = vol.rgb + hdr * (1.0 - vol.a);
    FragColor = vec4(result, 1.0);
}
)";

// ── 初始化 ──────────────────────────────────────────────────

void VolumetricLighting::Init(u32 width, u32 height) {
    s_HalfWidth  = width / 2;
    s_HalfHeight = height / 2;

    // 半分辨率 FBO (RGBA16F)
    FramebufferSpec spec;
    spec.Width  = s_HalfWidth;
    spec.Height = s_HalfHeight;
    spec.ColorFormats = { TextureFormat::RGBA16F };
    spec.DepthAttachment = false;
    s_HalfResFBO = CreateScope<Framebuffer>(spec);

    // 加载 Shader
    s_RayMarchShader = ResourceManager::LoadShader("volumetric_raymarch",
        s_RayMarchVS, s_RayMarchFS);
    s_CompositeShader = ResourceManager::LoadShader("volumetric_composite",
        s_CompositeVS, s_CompositeFS);

    LOG_INFO("[Volumetric] Init OK (%ux%u half-res)", s_HalfWidth, s_HalfHeight);
}

void VolumetricLighting::Shutdown() {
    s_HalfResFBO.reset();
    s_RayMarchShader.reset();
    s_CompositeShader.reset();
    LOG_DEBUG("[Volumetric] Shutdown");
}

void VolumetricLighting::Resize(u32 width, u32 height) {
    s_HalfWidth  = width / 2;
    s_HalfHeight = height / 2;
    if (s_HalfResFBO) s_HalfResFBO->Resize(s_HalfWidth, s_HalfHeight);
}

// ── Ray-march 生成 ──────────────────────────────────────────

void VolumetricLighting::Generate(
    const float* viewMat, const float* projMat,
    const float* invViewProjMat,
    const glm::vec3& lightDir,
    const glm::vec3& lightColor,
    u32 depthTexture)
{
    if (!s_Config.Enabled) return;

    s_HalfResFBO->Bind();
    glViewport(0, 0, s_HalfWidth, s_HalfHeight);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    s_RayMarchShader->Bind();

    // 深度纹理
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    s_RayMarchShader->SetInt("uDepthTex", 0);

    // CSM 阴影贴图 (复用 CSM 纹理绑定)
    u32 csmUnit = 1;
    CascadedShadowMap::BindCascadeTextures(csmUnit);
    CascadedShadowMap::SetUniforms(s_RayMarchShader.get(), csmUnit);

    // 矩阵
    s_RayMarchShader->SetMat4("uInvViewProj", invViewProjMat);
    s_RayMarchShader->SetMat4("uView", viewMat);

    // 光照参数
    s_RayMarchShader->SetVec3("uLightDir", lightDir.x, lightDir.y, lightDir.z);
    s_RayMarchShader->SetVec3("uLightColor", lightColor.x, lightColor.y, lightColor.z);

    // 体积参数
    s_RayMarchShader->SetFloat("uMaxDist", s_Config.MaxDistance);
    s_RayMarchShader->SetInt("uSteps", s_Config.Steps);
    s_RayMarchShader->SetFloat("uDensity", s_Config.Density);
    s_RayMarchShader->SetFloat("uScattering", s_Config.Scattering);
    s_RayMarchShader->SetFloat("uHeightFalloff", s_Config.FogHeightFalloff);
    s_RayMarchShader->SetFloat("uBaseHeight", s_Config.FogBaseHeight);
    s_RayMarchShader->SetVec3("uFogColor", s_Config.FogColor.x, s_Config.FogColor.y, s_Config.FogColor.z);
    s_RayMarchShader->SetFloat("uLightIntensity", s_Config.LightIntensity);

    // Camera pos (extract from view matrix inverse)
    glm::mat4 viewM = glm::make_mat4(viewMat);
    glm::vec3 camPos = glm::vec3(glm::inverse(viewM)[3]);
    s_RayMarchShader->SetVec3("uCamPos", camPos.x, camPos.y, camPos.z);

    ScreenQuad::Draw();

    s_HalfResFBO->Unbind();
    glEnable(GL_DEPTH_TEST);
}

// ── 合成到 HDR ──────────────────────────────────────────────

void VolumetricLighting::Composite(u32 hdrTexture) {
    if (!s_Config.Enabled) return;

    s_CompositeShader->Bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);
    s_CompositeShader->SetInt("uHDR", 0);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, s_HalfResFBO->GetColorAttachmentID());
    s_CompositeShader->SetInt("uVolumetric", 1);

    ScreenQuad::Draw();
}

// ── Getter ──────────────────────────────────────────────────

u32 VolumetricLighting::GetVolumetricTexture() {
    return s_HalfResFBO ? s_HalfResFBO->GetColorAttachmentID() : 0;
}

bool VolumetricLighting::IsEnabled() { return s_Config.Enabled; }
VolumetricConfig& VolumetricLighting::GetConfig() { return s_Config; }

} // namespace Engine
