#include "engine/renderer/shadow_map.h"
#include "engine/core/resource_manager.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

u32 ShadowMap::s_FBO = 0;
u32 ShadowMap::s_DepthTexture = 0;
u32 ShadowMap::s_Resolution = 2048;
Ref<Shader> ShadowMap::s_DepthShader = nullptr;
glm::mat4 ShadowMap::s_LightSpaceMat = glm::mat4(1.0f);
ShadowMapConfig ShadowMap::s_Config = {};

// ── 深度着色器源码 ──────────────────────────────────────────

static const char* s_DepthVS = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
uniform mat4 uLightSpace;
uniform mat4 uModel;
void main() {
    gl_Position = uLightSpace * uModel * vec4(aPos, 1.0);
}
)";

static const char* s_DepthFS = R"(
#version 450 core
void main() { /* 只写深度 */ }
)";

// ── 初始化 ──────────────────────────────────────────────────

void ShadowMap::Init(const ShadowMapConfig& config) {
    s_Config = config;
    s_Resolution = config.Resolution;

    // 创建深度 FBO
    glGenFramebuffers(1, &s_FBO);

    // 创建深度纹理
    glGenTextures(1, &s_DepthTexture);
    glBindTexture(GL_TEXTURE_2D, s_DepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                 s_Resolution, s_Resolution, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // 绑定到 FBO
    glBindFramebuffer(GL_FRAMEBUFFER, s_FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, s_DepthTexture, 0);
    GLenum none = GL_NONE;
    glDrawBuffers(1, &none); // 无颜色输出
    // glReadBuffer 在自定义 GLAD 中不可用，跳过

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR("[ShadowMap] FBO 创建失败！");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 深度 Shader
    s_DepthShader = ResourceManager::LoadShader("shadow_depth",
        s_DepthVS, s_DepthFS);

    LOG_INFO("[ShadowMap] 初始化完成 (%ux%u)", s_Resolution, s_Resolution);
}

void ShadowMap::Shutdown() {
    if (s_DepthTexture) { glDeleteTextures(1, &s_DepthTexture); s_DepthTexture = 0; }
    if (s_FBO) { glDeleteFramebuffers(1, &s_FBO); s_FBO = 0; }
    s_DepthShader.reset();
    LOG_DEBUG("[ShadowMap] 已清理");
}

// ── 深度 Pass ───────────────────────────────────────────────

void ShadowMap::BeginShadowPass(const DirectionalLight& light,
                                 const glm::vec3& sceneCenter) {
    // 计算 Light Space 矩阵
    f32 sz = s_Config.OrthoSize;
    glm::vec3 lightDir = glm::normalize(light.Direction);
    glm::vec3 lightPos = sceneCenter - lightDir * (s_Config.FarPlane * 0.5f);

    glm::mat4 lightView = glm::lookAt(lightPos,
                                       sceneCenter,
                                       glm::vec3(0, 1, 0));
    glm::mat4 lightProj = glm::ortho(-sz, sz, -sz, sz,
                                      s_Config.NearPlane, s_Config.FarPlane);
    s_LightSpaceMat = lightProj * lightView;

    // 绑定阴影 FBO
    glViewport(0, 0, s_Resolution, s_Resolution);
    glBindFramebuffer(GL_FRAMEBUFFER, s_FBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    // 使用正面剔除避免 Peter Panning
    glCullFace(GL_FRONT);

    s_DepthShader->Bind();
    s_DepthShader->SetMat4("uLightSpace", glm::value_ptr(s_LightSpaceMat));
}

void ShadowMap::EndShadowPass() {
    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ── Getter ──────────────────────────────────────────────────

Ref<Shader> ShadowMap::GetDepthShader() { return s_DepthShader; }
const glm::mat4& ShadowMap::GetLightSpaceMatrix() { return s_LightSpaceMat; }
u32 ShadowMap::GetShadowTextureID() { return s_DepthTexture; }
u32 ShadowMap::GetResolution() { return s_Resolution; }

} // namespace Engine
