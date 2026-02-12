#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Engine {

// ── 调试叠加 UI ─────────────────────────────────────────────
// 在屏幕上绘制调试文字（位图字体渲染）
// 使用固定宽度位图字体，支持多行文本

class DebugUI {
public:
    static void Init();
    static void Shutdown();

    /// 添加一行文字（屏幕空间坐标，左上角为原点）
    static void Text(f32 x, f32 y, const std::string& text,
                     const glm::vec3& color = {1, 1, 1}, f32 scale = 1.0f);

    /// 添加一行带格式的文字
    static void Printf(f32 x, f32 y, const glm::vec3& color, const char* fmt, ...);

    /// 绘制调试信息面板（自动显示 FPS/Draw/Tri/Mem 等）
    static void DrawStatsPanel(f32 fps, u32 drawCalls, u32 triangles, u32 particles,
                               u32 entities, u32 debugLines, f32 exposure, f32 fov);

    /// 渲染并清空所有排队的文字
    static void Flush(u32 screenWidth, u32 screenHeight);

    static void SetEnabled(bool enabled);
    static bool IsEnabled();

private:
    struct TextEntry {
        f32 X, Y;
        std::string Content;
        glm::vec3 Color;
        f32 Scale;
    };

    static void BuildFontTexture();

    static std::vector<TextEntry> s_TextQueue;
    static u32 s_FontTexture;
    static u32 s_VAO;
    static u32 s_VBO;
    static Ref<Shader> s_Shader;
    static bool s_Enabled;

    static constexpr u32 CHAR_W = 8;   // 字符宽度 (像素)
    static constexpr u32 CHAR_H = 14;  // 字符高度 (像素)
    static constexpr u32 COLS = 16;    // 字体图集列数
    static constexpr u32 ROWS = 6;     // 字体图集行数
};

} // namespace Engine
