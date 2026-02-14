#pragma once

#include "engine/core/types.h"
#include <imgui.h>
#include <vector>
#include <string>

namespace Engine {

// ── 增强颜色拾取器 ──────────────────────────────────────────
// 功能:
//   1. HSV 色轮 + 亮度条
//   2. HDR 支持 (值域 > 1.0)
//   3. 调色板 (最近使用 + 自定义收藏)
//   4. 十六进制输入
//   5. 吸色器 (eyedropper)
//   6. 渐变编辑 (Gradient)

struct GradientStop {
    f32 Position = 0;  // 0..1
    f32 R = 1, G = 1, B = 1, A = 1;
};

class ColorPickerEx {
public:
    static void Init();
    static void Shutdown();

    /// 颜色拾取弹出窗口 (替代 ImGui 内置)
    /// 返回是否修改
    static bool ColorEdit(const char* label, f32* rgba, bool hdr = false);

    /// 单独颜色轮盘
    static bool ColorWheel(const char* label, f32* hsv);

    /// 渐变编辑器
    static bool GradientEditor(const char* label, std::vector<GradientStop>& stops);

    /// 采样渐变颜色
    static void SampleGradient(const std::vector<GradientStop>& stops, f32 t,
                                f32& r, f32& g, f32& b, f32& a);

    /// 添加颜色到收藏
    static void AddFavorite(f32 r, f32 g, f32 b, f32 a = 1.0f);

    /// 渲染调色板面板
    static void RenderPalettePanel();

private:
    static void RenderColorPreview(ImVec4 color, ImVec2 size);

    struct SavedColor { f32 R, G, B, A; };
    inline static std::vector<SavedColor> s_RecentColors;
    inline static std::vector<SavedColor> s_FavoriteColors;
    static constexpr u32 MAX_RECENT = 32;
};

} // namespace Engine
