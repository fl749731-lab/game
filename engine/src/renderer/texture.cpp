#include "engine/renderer/texture.h"
#include "engine/core/log.h"

#include <glad/glad.h>

#include "stb_image.h"

namespace Engine {

// ── 纹理参数辅助 ─────────────────────────────────────────────

/// 设置默认采样参数（LINEAR_MIPMAP_LINEAR + REPEAT）
static void SetupDefaultParams() {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

/// 启用各向异性过滤（如果硬件支持）
static void ApplyAnisotropy() {
    #ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
    #define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
    #endif
    #ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
    #define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
    #endif
    float maxAniso = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
    if (maxAniso > 1.0f)
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);
}

/// 根据通道数确定格式。成功返回 true。
static bool ResolveFormat(u32 channels, u32& internalFmt, u32& dataFmt) {
    if (channels == 4)      { internalFmt = GL_RGBA8; dataFmt = GL_RGBA; }
    else if (channels == 3) { internalFmt = GL_RGB8;  dataFmt = GL_RGB;  }
    else if (channels == 1) { internalFmt = GL_R8;    dataFmt = GL_RED;  }
    else { LOG_WARN("不支持的通道数: %u", channels); return false; }
    return true;
}

// ── 构造 (从文件) ────────────────────────────────────────────

Texture2D::Texture2D(const std::string& filepath) {
    LOG_DEBUG("加载纹理: %s", filepath.c_str());
    stbi_set_flip_vertically_on_load(1);

    int width, height, channels;
    unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &channels, 0);
    if (!data) {
        LOG_ERROR("纹理加载失败: %s", filepath.c_str());
        return;
    }

    m_Width = width;
    m_Height = height;
    m_Channels = channels;

    if (!ResolveFormat(channels, m_InternalFormat, m_DataFormat)) {
        stbi_image_free(data);
        return;
    }

    glGenTextures(1, &m_ID);
    glBindTexture(GL_TEXTURE_2D, m_ID);
    SetupDefaultParams();

    glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0,
                 m_DataFormat, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    ApplyAnisotropy();

    stbi_image_free(data);
    LOG_INFO("纹理已加载: %s (%ux%u, %d通道)", filepath.c_str(), m_Width, m_Height, m_Channels);
}

// ── 构造 (RGBA 数据) ─────────────────────────────────────────

Texture2D::Texture2D(u32 width, u32 height, const void* data)
    : m_Width(width), m_Height(height), m_Channels(4)
{
    m_InternalFormat = GL_RGBA8;
    m_DataFormat = GL_RGBA;

    glGenTextures(1, &m_ID);
    glBindTexture(GL_TEXTURE_2D, m_ID);
    SetupDefaultParams();

    glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0,
                 m_DataFormat, GL_UNSIGNED_BYTE, data);

    if (data) {
        glGenerateMipmap(GL_TEXTURE_2D);
        ApplyAnisotropy();
    }

    LOG_DEBUG("纹理已创建: %ux%u", m_Width, m_Height);
}

// ── 构造 (指定通道数) ────────────────────────────────────────

Texture2D::Texture2D(u32 width, u32 height, u32 channels, const void* data)
    : m_Width(width), m_Height(height), m_Channels(channels)
{
    if (!ResolveFormat(channels, m_InternalFormat, m_DataFormat))
        return;

    glGenTextures(1, &m_ID);
    glBindTexture(GL_TEXTURE_2D, m_ID);
    SetupDefaultParams();

    glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0,
                 m_DataFormat, GL_UNSIGNED_BYTE, data);

    if (data) {
        glGenerateMipmap(GL_TEXTURE_2D);
        ApplyAnisotropy();
    }

    LOG_DEBUG("纹理已创建: %ux%u (%u通道)", m_Width, m_Height, channels);
}

// ── 析构 / 绑定 ──────────────────────────────────────────────

Texture2D::~Texture2D() {
    if (m_ID) glDeleteTextures(1, &m_ID);
}

void Texture2D::Bind(u32 slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_ID);
}

void Texture2D::Unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::SetData(const void* data, u32 size) {
    // size 参数保留用于未来校验
    glBindTexture(GL_TEXTURE_2D, m_ID);
    glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0,
                 m_DataFormat, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
}

} // namespace Engine
