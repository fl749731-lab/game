#pragma once

#include "engine/core/types.h"

#include <string>

namespace Engine {

// ── Viewport 可视化模式 ─────────────────────────────────────
// 类 UE Buffer Visualization: 8 种渲染模式切换
// Alt+1~8 快捷键

enum class ViewportMode : u8 {
    Lit = 0,              // 默认完整光照
    Unlit,                // 仅 Albedo，无光照
    Wireframe,            // 线框叠加
    Normals,              // 世界法线 → RGB
    UV,                   // UV 坐标 → RG
    Overdraw,             // 绘制次数热图
    Depth,                // 深度缓冲灰度
    LightComplexity,      // 每像素灯光数热图
    Count
};

class ViewportModes {
public:
    static void Init();
    static void Shutdown();

    /// 设置/读取当前模式
    static void SetMode(ViewportMode mode);
    static ViewportMode GetMode();
    static const char* GetModeName(ViewportMode mode);

    /// 快捷键处理 (Alt+1~8)
    static bool HandleKeyInput(int key, int action, bool altDown);

    /// 在渲染管线中应用当前模式
    /// 返回值: 需要修改的 GBuffer debug mode 编号 (-1 不修改)
    static i32 GetGBufferOverride();

    /// 是否需要线框覆盖
    static bool NeedsWireframeOverlay();

    /// 是否需要 Overdraw/LightComplexity 专用 Pass
    static bool NeedsSpecialPass();

    /// 渲染 ImGui Viewport 模式选择器 (工具栏)
    static void RenderToolbar();

    /// Overdraw Pass: 开始/结束
    static void BeginOverdrawPass();
    static void EndOverdrawPass();

    /// 读取 Overdraw 累计纹理 ID
    static u32 GetOverdrawTextureID();

private:
    inline static ViewportMode s_CurrentMode = ViewportMode::Lit;
    inline static u32 s_OverdrawFBO = 0;
    inline static u32 s_OverdrawTexture = 0;
};

} // namespace Engine
