#pragma once

#include "engine/core/types.h"
#include "engine/core/scene.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/framebuffer.h"

namespace Engine {

// ── 场景渲染器 (延迟渲染管线) ────────────────────────────────
//
// 渲染流程:
//   Pass 0: Shadow Map (深度)
//   Pass 1: G-Buffer 几何 (MRT 写入 Position/Normal/Albedo/Emissive)
//   Pass 2: 延迟光照 (全屏四边形采样 G-Buffer + 阴影 → HDR FBO)
//   Pass 3: 前向叠加 (天空盒 / 透明物 / 自发光 / 粒子 / 调试线)
//   Pass 4: Bloom + 后处理 (色调映射 → 屏幕)
//
// 使用方法: SceneRenderer::Init() → 每帧 RenderScene() → Shutdown()

struct SceneRendererConfig {
    u32 Width  = 1280;
    u32 Height = 720;
    f32 Exposure = 1.2f;
    bool BloomEnabled = true;
};

class SceneRenderer {
public:
    static void Init(const SceneRendererConfig& config);
    static void Shutdown();

    /// 单次调用完成全部渲染
    static void RenderScene(Scene& scene, PerspectiveCamera& camera);

    /// 窗口大小变化
    static void Resize(u32 width, u32 height);

    // ── 渲染参数控制 ────────────────────────────────────────
    static void SetExposure(f32 exposure);
    static f32  GetExposure();
    static void SetBloomEnabled(bool enabled);
    static bool GetBloomEnabled();
    static void SetWireframe(bool enabled);

    /// G-Buffer 调试模式 (0=关, 1=Position, 2=Normal, 3=Albedo, 4=Specular, 5=Emissive)
    static void SetGBufferDebugMode(int mode);
    static int  GetGBufferDebugMode();

    /// 获取 HDR FBO 的颜色纹理 ID
    static u32 GetHDRColorAttachment();

private:
    // 各 Pass 函数
    static void ShadowPass(Scene& scene, PerspectiveCamera& camera);
    static void GeometryPass(Scene& scene, PerspectiveCamera& camera);
    static void LightingPass(Scene& scene, PerspectiveCamera& camera);
    static void ForwardPass(Scene& scene, PerspectiveCamera& camera);
    static void PostProcessPass();

    // 辅助
    static void RenderEntitiesDeferred(Scene& scene, PerspectiveCamera& camera);
    static void SetupLightUniforms(Scene& scene, Shader* shader, PerspectiveCamera& camera);

    static Scope<Framebuffer> s_HDR_FBO;
    static Ref<Shader> s_GBufferShader;    // 几何 Pass
    static Ref<Shader> s_DeferredShader;   // 光照 Pass
    static Ref<Shader> s_EmissiveShader;   // 前向叠加
    static Ref<Shader> s_GBufDebugShader;  // 调试可视化
    static Ref<Shader> s_LitShader;        // 保留 (前向 fallback)
    static Ref<Shader> s_BlitShader;       // 全屏纹理 blit (SSR 混合等)

    static u32 s_CheckerTexID;

    static u32 s_Width;
    static u32 s_Height;
    static f32 s_Exposure;
    static bool s_BloomEnabled;
    static int  s_GBufDebugMode;
};

} // namespace Engine
