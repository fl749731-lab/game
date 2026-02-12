#include "engine/renderer/font.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <fstream>
#include <vector>

// stb_truetype + stb_rect_pack 实现
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace Engine {

Font::Font(const std::string& filepath, f32 pixelHeight) {
    // 读取字体文件
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR("[Font] 无法打开字体文件: %s", filepath.c_str());
        return;
    }

    auto fileSize = file.tellg();
    file.seekg(0);
    std::vector<u8> fontBuffer(fileSize);
    file.read(reinterpret_cast<char*>(fontBuffer.data()), fileSize);

    // 烘焙字形图集
    std::vector<u8> atlasData(ATLAS_SIZE * ATLAS_SIZE);
    stbtt_bakedchar charData[CHAR_COUNT];

    int result = stbtt_BakeFontBitmap(
        fontBuffer.data(), 0,
        pixelHeight,
        atlasData.data(), ATLAS_SIZE, ATLAS_SIZE,
        FIRST_CHAR, CHAR_COUNT,
        charData
    );

    if (result <= 0) {
        LOG_ERROR("[Font] 字形烘焙失败: %s", filepath.c_str());
        return;
    }

    // 填充字形信息
    f32 invW = 1.0f / (f32)ATLAS_SIZE;
    f32 invH = 1.0f / (f32)ATLAS_SIZE;

    for (u32 i = 0; i < CHAR_COUNT; i++) {
        auto& bc = charData[i];
        auto& g = m_Glyphs[i];

        g.U0 = bc.x0 * invW;
        g.V0 = bc.y0 * invH;
        g.U1 = bc.x1 * invW;
        g.V1 = bc.y1 * invH;
        g.Width  = (f32)(bc.x1 - bc.x0);
        g.Height = (f32)(bc.y1 - bc.y0);
        g.OffsetX = bc.xoff;
        g.OffsetY = bc.yoff;
        g.Advance = bc.xadvance;
    }

    m_LineHeight = pixelHeight;

    // 将单通道 atlas 转为 RGBA (白色 + alpha)
    // 这样就能直接用 SpriteBatch 的标准 shader
    std::vector<u8> rgbaData(ATLAS_SIZE * ATLAS_SIZE * 4);
    for (u32 i = 0; i < ATLAS_SIZE * ATLAS_SIZE; i++) {
        rgbaData[i * 4 + 0] = 255;           // R
        rgbaData[i * 4 + 1] = 255;           // G
        rgbaData[i * 4 + 2] = 255;           // B
        rgbaData[i * 4 + 3] = atlasData[i];  // A = 字形亮度
    }

    // 创建 OpenGL 纹理 (RGBA8)
    glGenTextures(1, &m_TextureID);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 ATLAS_SIZE, ATLAS_SIZE, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgbaData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    m_Valid = true;
    LOG_INFO("[Font] 已加载: %s (%.0fpx, %d 字形)", 
             filepath.c_str(), pixelHeight, result);
}

Font::~Font() {
    if (m_TextureID) {
        glDeleteTextures(1, &m_TextureID);
    }
}

const GlyphInfo& Font::GetGlyph(char c) const {
    u32 idx = (u32)c - FIRST_CHAR;
    if (idx >= CHAR_COUNT) idx = 0; // 回退到空格
    return m_Glyphs[idx];
}

f32 Font::MeasureText(const std::string& text, f32 scale) const {
    f32 width = 0;
    for (char c : text) {
        width += GetGlyph(c).Advance * scale;
    }
    return width;
}

} // namespace Engine
