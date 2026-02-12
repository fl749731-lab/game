#pragma once

#include "engine/core/types.h"
#include "engine/core/scene.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/framebuffer.h"

namespace Engine {

// ── 场景渲染器 ──────────────────────────────────────────────
// 彻底分离数据层与渲染层
// 使用方法: SceneRenderer::Init() → 每帧 RenderScene() → Shutdown()
//
// 内部自动管理:
//   - HDR FBO (离屏渲染)
//   - 内置 Shader (Lit + Emissive)
//   - 基础网格 (cube, plane, sphere)
//   - 棋盘纹理
//   - ECS 遍历 → Model 矩阵 → Uniform → Draw
//   - 光源可视化
//   - 粒子渲染
//   - DebugDraw::Flush
//   - Bloom + PostProcess
//   - DebugUI::Flush

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

    /// 单次调用完成全部渲染（FBO → 遍历 → 后处理 → 调试叠加）
    static void RenderScene(Scene& scene, PerspectiveCamera& camera);

    /// 窗口大小变化
    static void Resize(u32 width, u32 height);

    // ── 渲染参数控制 ────────────────────────────────────────
    static void SetExposure(f32 exposure);
    static f32  GetExposure();
    static void SetBloomEnabled(bool enabled);
    static bool GetBloomEnabled();
    static void SetWireframe(bool enabled);

    /// 获取 HDR FBO 的颜色纹理 ID（供外部 debug 用）
    static u32 GetHDRColorAttachment();

private:
    static void RenderEntities(Scene& scene, PerspectiveCamera& camera);
    static void RenderLightGizmos(Scene& scene, PerspectiveCamera& camera);

    static Scope<Framebuffer> s_HDR_FBO;
    static Ref<Shader> s_LitShader;
    static Ref<Shader> s_EmissiveShader;

    static u32 s_CheckerTexID;   // 棋盘纹理

    static u32 s_Width;
    static u32 s_Height;
    static f32 s_Exposure;
    static bool s_BloomEnabled;
};

} // namespace Engine
