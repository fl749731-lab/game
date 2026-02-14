#include "engine/renderer/cascaded_shadow_map.h"
#include "engine/core/resource_manager.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <algorithm>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

std::array<u32, CSM_CASCADE_COUNT> CascadedShadowMap::s_FBOs = {};
std::array<u32, CSM_CASCADE_COUNT> CascadedShadowMap::s_DepthTextures = {};
std::array<f32, CSM_CASCADE_COUNT + 1> CascadedShadowMap::s_SplitDistances = {};
std::array<glm::mat4, CSM_CASCADE_COUNT> CascadedShadowMap::s_LightSpaceMatrices = {};
u32 CascadedShadowMap::s_Resolution = 2048;
Ref<Shader> CascadedShadowMap::s_DepthShader = nullptr;
CSMConfig CascadedShadowMap::s_Config = {};

// ── 深度着色器源码 ──────────────────────────────────────────

static const char* s_CSMDepthVS = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
uniform mat4 uLightSpace;
uniform mat4 uModel;
void main() {
    gl_Position = uLightSpace * uModel * vec4(aPos, 1.0);
}
)";

static const char* s_CSMDepthFS = R"(
#version 450 core
void main() { /* 只写深度 */ }
)";

// ── 初始化 ──────────────────────────────────────────────────

void CascadedShadowMap::Init(const CSMConfig& config) {
    s_Config = config;
    s_Resolution = config.Resolution;

    // 为每个级联创建独立的 FBO + 深度纹理
    for (u32 i = 0; i < CSM_CASCADE_COUNT; i++) {
        // 深度纹理
        glGenTextures(1, &s_DepthTextures[i]);
        glBindTexture(GL_TEXTURE_2D, s_DepthTextures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                     s_Resolution, s_Resolution, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        // FBO
        glGenFramebuffers(1, &s_FBOs[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, s_FBOs[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D, s_DepthTextures[i], 0);
        GLenum none = GL_NONE;
        glDrawBuffers(1, &none);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            LOG_ERROR("[CSM] FBO %u creation failed!", i);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 深度 Shader
    s_DepthShader = ResourceManager::LoadShader("csm_depth",
        s_CSMDepthVS, s_CSMDepthFS);

    LOG_INFO("[CSM] Init OK (%ux%u x %u cascades)", s_Resolution, s_Resolution, CSM_CASCADE_COUNT);
}

void CascadedShadowMap::Shutdown() {
    for (u32 i = 0; i < CSM_CASCADE_COUNT; i++) {
        if (s_DepthTextures[i]) { glDeleteTextures(1, &s_DepthTextures[i]); s_DepthTextures[i] = 0; }
        if (s_FBOs[i]) { glDeleteFramebuffers(1, &s_FBOs[i]); s_FBOs[i] = 0; }
    }
    s_DepthShader.reset();
    LOG_DEBUG("[CSM] Shutdown");
}

// ── 级联分割 ────────────────────────────────────────────────

void CascadedShadowMap::CalculateSplits(f32 nearPlane, f32 farPlane) {
    // 限制远平面到阴影距离
    f32 shadowFar = std::min(farPlane, s_Config.ShadowDistance);
    f32 lambda = s_Config.SplitLambda;
    f32 range = shadowFar - nearPlane;
    f32 ratio = shadowFar / nearPlane;

    s_SplitDistances[0] = nearPlane;
    for (u32 i = 1; i < CSM_CASCADE_COUNT; i++) {
        f32 p = (f32)i / (f32)CSM_CASCADE_COUNT;
        // Log split: near * (far/near)^p
        f32 logSplit  = nearPlane * std::pow(ratio, p);
        // Uniform split: near + (far-near) * p
        f32 uniSplit  = nearPlane + range * p;
        // Lambda blend
        s_SplitDistances[i] = lambda * logSplit + (1.0f - lambda) * uniSplit;
    }
    s_SplitDistances[CSM_CASCADE_COUNT] = shadowFar;
}

// ── 计算单个级联的 Light Space 矩阵 ────────────────────────

glm::mat4 CascadedShadowMap::CalculateLightSpaceMatrix(
    const PerspectiveCamera& camera,
    const DirectionalLight& light,
    f32 nearSplit, f32 farSplit)
{
    // 构建此级联范围的投影矩阵
    f32 fov = camera.GetFOV();
    f32 aspect = camera.GetAspect();
    glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, nearSplit, farSplit);
    glm::mat4 invVP = glm::inverse(proj * camera.GetViewMatrix());

    // 提取视锥 8 个角点到世界空间
    glm::vec3 corners[8];
    int idx = 0;
    for (int x = 0; x <= 1; x++) {
        for (int y = 0; y <= 1; y++) {
            for (int z = 0; z <= 1; z++) {
                glm::vec4 ndc(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
                glm::vec4 world = invVP * ndc;
                corners[idx++] = glm::vec3(world) / world.w;
            }
        }
    }

    // 计算视锥中心
    glm::vec3 center(0.0f);
    for (int i = 0; i < 8; i++) center += corners[i];
    center /= 8.0f;

    // 光源视图矩阵
    glm::vec3 lightDir = glm::normalize(light.Direction);
    glm::mat4 lightView = glm::lookAt(center - lightDir * 50.0f,
                                       center,
                                       glm::vec3(0, 1, 0));

    // 计算光照空间中的 AABB
    glm::vec3 minBounds(std::numeric_limits<f32>::max());
    glm::vec3 maxBounds(std::numeric_limits<f32>::lowest());
    for (int i = 0; i < 8; i++) {
        glm::vec4 ls = lightView * glm::vec4(corners[i], 1.0f);
        minBounds = glm::min(minBounds, glm::vec3(ls));
        maxBounds = glm::max(maxBounds, glm::vec3(ls));
    }

    // 扩展 Z 范围以包含阴影投射体
    f32 zMargin = 50.0f;
    minBounds.z -= zMargin;
    maxBounds.z += zMargin;

    // 稳定化: 将投影范围对齐到纹素网格（消除阴影游泳）
    f32 worldUnitsPerTexel = glm::max(maxBounds.x - minBounds.x,
                                       maxBounds.y - minBounds.y)
                             / (f32)s_Resolution;
    minBounds.x = std::floor(minBounds.x / worldUnitsPerTexel) * worldUnitsPerTexel;
    minBounds.y = std::floor(minBounds.y / worldUnitsPerTexel) * worldUnitsPerTexel;
    maxBounds.x = std::ceil(maxBounds.x / worldUnitsPerTexel) * worldUnitsPerTexel;
    maxBounds.y = std::ceil(maxBounds.y / worldUnitsPerTexel) * worldUnitsPerTexel;

    glm::mat4 lightProj = glm::ortho(minBounds.x, maxBounds.x,
                                      minBounds.y, maxBounds.y,
                                      minBounds.z, maxBounds.z);

    return lightProj * lightView;
}

// ── 每帧更新 ────────────────────────────────────────────────

void CascadedShadowMap::UpdateCascades(const PerspectiveCamera& camera,
                                        const DirectionalLight& light) {
    // 计算分割距离
    CalculateSplits(camera.GetNearClip(), camera.GetFarClip());

    // 计算每个级联的 Light Space 矩阵
    for (u32 i = 0; i < CSM_CASCADE_COUNT; i++) {
        f32 near = s_SplitDistances[i];
        f32 far  = s_SplitDistances[i + 1];

        // 级联重叠（平滑过渡）
        if (i > 0) {
            f32 overlap = (far - near) * s_Config.CascadeOverlap;
            near -= overlap;
        }

        s_LightSpaceMatrices[i] = CalculateLightSpaceMatrix(camera, light, near, far);
    }
}

// ── 深度 Pass ───────────────────────────────────────────────

void CascadedShadowMap::BeginCascadePass(u32 cascadeIndex) {
    glViewport(0, 0, s_Resolution, s_Resolution);
    glBindFramebuffer(GL_FRAMEBUFFER, s_FBOs[cascadeIndex]);
    glClear(GL_DEPTH_BUFFER_BIT);

    // 正面剔除避免 Peter Panning
    glCullFace(GL_FRONT);

    s_DepthShader->Bind();
    s_DepthShader->SetMat4("uLightSpace",
        glm::value_ptr(s_LightSpaceMatrices[cascadeIndex]));
}

void CascadedShadowMap::EndCascadePass() {
    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ── 绑定级联纹理 ────────────────────────────────────────────

u32 CascadedShadowMap::BindCascadeTextures(u32 startUnit) {
    for (u32 i = 0; i < CSM_CASCADE_COUNT; i++) {
        glActiveTexture(GL_TEXTURE0 + startUnit + i);
        glBindTexture(GL_TEXTURE_2D, s_DepthTextures[i]);
    }
    return startUnit + CSM_CASCADE_COUNT;
}

void CascadedShadowMap::SetUniforms(Shader* shader, u32 startUnit) {
    // 级联阴影贴图
    for (u32 i = 0; i < CSM_CASCADE_COUNT; i++) {
        std::string name = "uCSMShadowMap[" + std::to_string(i) + "]";
        shader->SetInt(name, (int)(startUnit + i));
    }

    // Light Space 矩阵
    for (u32 i = 0; i < CSM_CASCADE_COUNT; i++) {
        std::string name = "uCSMLightSpace[" + std::to_string(i) + "]";
        shader->SetMat4(name, glm::value_ptr(s_LightSpaceMatrices[i]));
    }

    // 分割距离 (视图空间)
    for (u32 i = 0; i < CSM_CASCADE_COUNT; i++) {
        std::string name = "uCSMSplitDist[" + std::to_string(i) + "]";
        shader->SetFloat(name, s_SplitDistances[i + 1]);
    }

    shader->SetInt("uCSMEnabled", 1);
    shader->SetInt("uCSMCascadeCount", CSM_CASCADE_COUNT);
}

// ── Getter ──────────────────────────────────────────────────

Ref<Shader> CascadedShadowMap::GetDepthShader() { return s_DepthShader; }

const std::array<f32, CSM_CASCADE_COUNT + 1>& CascadedShadowMap::GetSplitDistances() {
    return s_SplitDistances;
}

const std::array<glm::mat4, CSM_CASCADE_COUNT>& CascadedShadowMap::GetLightSpaceMatrices() {
    return s_LightSpaceMatrices;
}

u32 CascadedShadowMap::GetResolution() { return s_Resolution; }
CSMConfig& CascadedShadowMap::GetConfig() { return s_Config; }

} // namespace Engine
