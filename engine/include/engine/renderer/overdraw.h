#pragma once

#include "engine/core/types.h"
#include <string>

namespace Engine {

// ── Overdraw 可视化 ─────────────────────────────────────────
// 功能:
//   1. Overdraw 着色器 (累积片元写入计数)
//   2. 热力图颜色映射 (低→绿, 中→黄, 高→红)
//   3. 显示统计信息

class OverdrawVisualization {
public:
    static void Init(u32 width, u32 height);
    static void Shutdown();

    /// 开始 Overdraw 渲染 Pass (绑定着色器和 FBO)
    static void Begin();

    /// 结束 Overdraw Pass
    static void End();

    /// 渲染最终热力图到屏幕
    static void RenderOverlay(u32 screenWidth, u32 screenHeight);

    /// 获取着色器源码
    static const char* GetVertexShaderSource();
    static const char* GetFragmentShaderSource();
    static const char* GetHeatmapFragSource();

    static void SetEnabled(bool enabled) { s_Enabled = enabled; }
    static bool IsEnabled() { return s_Enabled; }

    // 统计
    static f32 GetAverageOverdraw() { return s_AvgOverdraw; }
    static f32 GetMaxOverdraw() { return s_MaxOverdraw; }

private:
    inline static bool s_Enabled = false;
    inline static u32 s_FBO = 0;
    inline static u32 s_CountTexture = 0;
    inline static u32 s_OverdrawProgram = 0;
    inline static u32 s_HeatmapProgram = 0;
    inline static u32 s_Width = 0, s_Height = 0;
    inline static f32 s_AvgOverdraw = 0, s_MaxOverdraw = 0;
};

} // namespace Engine
