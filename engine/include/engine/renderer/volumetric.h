#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/framebuffer.h"

#include <glm/glm.hpp>

namespace Engine {

// ── 体积光/雾 (Volumetric Lighting & Fog) ──────────────────
//
// Ray-marching 体积雾 + 方向光体积散射 (God Rays)
// 在延迟光照之后、后处理之前执行
// 半分辨率渲染 + 双边上采样 → 性能友好

struct VolumetricConfig {
    u32  Steps           = 64;       // Ray-march 步数
    f32  Density         = 0.02f;    // 雾密度
    f32  Scattering      = 0.6f;     // 散射系数 (Henyey-Greenstein g)
    f32  MaxDistance      = 80.0f;    // 最大ray-march距离
    f32  FogHeightFalloff = 0.1f;    // 高度衰减因子
    f32  FogBaseHeight    = 0.0f;    // 雾基准高度
    glm::vec3 FogColor   = {0.7f, 0.75f, 0.85f}; // 雾颜色
    f32  LightIntensity  = 1.0f;     // 体积光强度
    bool Enabled         = true;
};

class VolumetricLighting {
public:
    static void Init(u32 width, u32 height);
    static void Shutdown();
    static void Resize(u32 width, u32 height);

    /// 执行体积光 Ray-marching (半分辨率)
    /// 需要: G-Buffer 深度、CSM 阴影贴图、摄像机参数、方向光
    static void Generate(const float* viewMat, const float* projMat,
                         const float* invViewProjMat,
                         const glm::vec3& lightDir,
                         const glm::vec3& lightColor,
                         u32 depthTexture);

    /// 将体积光结果混合到 HDR FBO
    static void Composite(u32 hdrTexture);

    /// 获取体积光纹理 ID
    static u32 GetVolumetricTexture();

    static bool IsEnabled();
    static VolumetricConfig& GetConfig();

private:
    static Scope<Framebuffer> s_HalfResFBO;
    static Ref<Shader> s_RayMarchShader;
    static Ref<Shader> s_CompositeShader;
    static u32 s_HalfWidth, s_HalfHeight;
    static VolumetricConfig s_Config;
};

} // namespace Engine
