#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/light.h"

#include <glm/glm.hpp>

namespace Engine {

// ── 阴影映射 ────────────────────────────────────────────────
// 方向光 Shadow Map — 深度 FBO + 正交投影Light Space Matrix
// 支持 PCF 软阴影

struct ShadowMapConfig {
    u32 Resolution = 2048;        // 阴影贴图分辨率
    f32 OrthoSize  = 20.0f;      // 正交投影范围
    f32 NearPlane  = 0.1f;
    f32 FarPlane   = 50.0f;
};

class ShadowMap {
public:
    static void Init(const ShadowMapConfig& config = {});
    static void Shutdown();

    /// 开始深度 Pass（绑定阴影 FBO，设置 light VP）
    static void BeginShadowPass(const DirectionalLight& light,
                                const glm::vec3& sceneCenter = {0,0,0});

    /// 结束深度 Pass（解绑 FBO）
    static void EndShadowPass();

    /// 获取深度着色器（用于深度 Pass 渲染实体）
    static Ref<Shader> GetDepthShader();

    /// 获取 Light Space 矩阵（用于采样阴影）
    static const glm::mat4& GetLightSpaceMatrix();

    /// 获取阴影深度纹理 ID（绑定到 Lit Shader）
    static u32 GetShadowTextureID();

    /// 获取阴影分辨率
    static u32 GetResolution();

private:
    static u32 s_FBO;
    static u32 s_DepthTexture;
    static u32 s_Resolution;
    static Ref<Shader> s_DepthShader;
    static glm::mat4 s_LightSpaceMat;
    static ShadowMapConfig s_Config;
};

} // namespace Engine
