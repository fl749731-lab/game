#include "engine/rhi/opengl/gl_texture.h"
#include "engine/core/log.h"

// stb_image 已在 texture.cpp 中定义 STB_IMAGE_IMPLEMENTATION
// 这里只需要声明
#include "stb_image.h"

namespace Engine {

// ── 从文件加载 ──────────────────────────────────────────────

GLTexture2D::GLTexture2D(const std::string& filepath) {
    LOG_DEBUG("[GLTexture] 加载纹理: %s", filepath.c_str());
    stbi_set_flip_vertically_on_load(1);

    int width, height, channels;
    unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &channels, 0);
    if (!data) {
        LOG_ERROR("[GLTexture] 纹理加载失败: %s", filepath.c_str());
        return;
    }

    m_Width    = width;
    m_Height   = height;
    m_Channels = channels;

    if (channels == 4) {
        m_InternalFormat = GL_RGBA8;
        m_DataFormat     = GL_RGBA;
    } else if (channels == 3) {
        m_InternalFormat = GL_RGB8;
        m_DataFormat     = GL_RGB;
    } else if (channels == 1) {
        m_InternalFormat = GL_R8;
        m_DataFormat     = GL_RED;
    } else {
        LOG_WARN("[GLTexture] 不支持的通道数: %d", channels);
        stbi_image_free(data);
        return;
    }

    glGenTextures(1, &m_ID);
    glBindTexture(GL_TEXTURE_2D, m_ID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0,
                 m_DataFormat, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // 各向异性过滤
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

    stbi_image_free(data);
    LOG_INFO("[GLTexture] 纹理已加载: %s (%ux%u, %d通道)", filepath.c_str(), m_Width, m_Height, m_Channels);
}

// ── 创建空纹理 ──────────────────────────────────────────────

GLTexture2D::GLTexture2D(u32 width, u32 height, const void* data)
    : m_Width(width), m_Height(height), m_Channels(4)
{
    m_InternalFormat = GL_RGBA8;
    m_DataFormat     = GL_RGBA;

    glGenTextures(1, &m_ID);
    glBindTexture(GL_TEXTURE_2D, m_ID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0,
                 m_DataFormat, GL_UNSIGNED_BYTE, data);

    if (data) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    LOG_DEBUG("[GLTexture] 纹理已创建: %ux%u", m_Width, m_Height);
}

GLTexture2D::~GLTexture2D() {
    if (m_ID) glDeleteTextures(1, &m_ID);
}

void GLTexture2D::Bind(u32 slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_ID);
}

void GLTexture2D::Unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GLTexture2D::SetData(const void* data, u32 size) {
    glBindTexture(GL_TEXTURE_2D, m_ID);
    glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0,
                 m_DataFormat, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
}

} // namespace Engine
