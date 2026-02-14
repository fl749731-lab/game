#pragma once

#include "engine/core/types.h"

#include <imgui.h>
#include <string>

namespace Engine {

// ── 场景视口面板 ────────────────────────────────────────────
// 将 SceneRenderer 的 HDR FBO 输出嵌入 ImGui 窗口
// 支持:
//   1. 实时预览（纹理 blit 到 ImGui Image）
//   2. 视口内 Gizmo 操作
//   3. 拖拽旋转相机 (右键)
//   4. 工具栏 (Viewport Mode / Gizmo Mode / Grid)
//   5. 视口大小自适应

class SceneView {
public:
    static void Init();
    static void Shutdown();

    /// 渲染场景视口面板 (传入 HDR FBO color attachment ID)
    static void Render(u32 hdrColorTextureID, f32 viewportAspect = 16.0f/9.0f);

    /// 获取视口尺寸 (用于相机投影矩阵)
    static u32 GetViewportWidth()  { return s_ViewportWidth; }
    static u32 GetViewportHeight() { return s_ViewportHeight; }
    static bool IsViewportHovered() { return s_Hovered; }
    static bool IsViewportFocused() { return s_Focused; }

    /// 网格显示
    static void SetGridVisible(bool v) { s_ShowGrid = v; }
    static bool IsGridVisible() { return s_ShowGrid; }
    static void ToggleGrid() { s_ShowGrid = !s_ShowGrid; }

private:
    static void RenderToolbar();
    static void RenderViewportInfo();

    inline static u32  s_ViewportWidth  = 1280;
    inline static u32  s_ViewportHeight = 720;
    inline static bool s_Hovered = false;
    inline static bool s_Focused = false;
    inline static bool s_ShowGrid = true;
    inline static bool s_ShowStats = true;
};

} // namespace Engine
