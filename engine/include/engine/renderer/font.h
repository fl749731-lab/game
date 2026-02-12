#pragma once

#include "engine/core/types.h"
#include <string>
#include <array>

namespace Engine {

// ── 字形信息 ────────────────────────────────────────────────

struct GlyphInfo {
    f32 U0, V0, U1, V1;   // 纹理 UV 坐标
    f32 Width, Height;      // 像素尺寸
    f32 OffsetX, OffsetY;   // 基线偏移
    f32 Advance;            // 水平步进
};

// ── 字体 (stb_truetype) ────────────────────────────────────

class Font {
public:
    /// 从 TTF 文件加载字体
    Font(const std::string& filepath, f32 pixelHeight = 32.0f);
    ~Font();

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    bool IsValid() const { return m_Valid; }
    u32  GetTextureID() const { return m_TextureID; }
    f32  GetLineHeight() const { return m_LineHeight; }

    /// 获取字形信息 (ASCII 32~126)
    const GlyphInfo& GetGlyph(char c) const;

    /// 计算文本宽度
    f32 MeasureText(const std::string& text, f32 scale = 1.0f) const;

private:
    static constexpr u32 ATLAS_SIZE = 512;
    static constexpr u32 FIRST_CHAR = 32;
    static constexpr u32 CHAR_COUNT = 95; // 32~126

    u32  m_TextureID = 0;
    f32  m_LineHeight = 0;
    bool m_Valid = false;
    std::array<GlyphInfo, CHAR_COUNT> m_Glyphs{};
};

} // namespace Engine
