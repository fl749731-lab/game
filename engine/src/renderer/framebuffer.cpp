#include "engine/renderer/framebuffer.h"
#include "engine/core/log.h"

#include <glad/glad.h>

// GL_RG 可能在某些 GLAD 配置中缺失
#ifndef GL_RG
#define GL_RG 0x8227
#endif


namespace Engine {

// ── 工具: TextureFormat → OpenGL 参数 ───────────────────────

static GLenum FormatToInternalFormat(TextureFormat fmt) {
    switch (fmt) {
        case TextureFormat::RGBA8:    return GL_RGBA8;
        case TextureFormat::RGBA16F:  return GL_RGBA16F;
        case TextureFormat::RGB16F:   return GL_RGB16F;
        case TextureFormat::RG16F:    return GL_RG16F;
        case TextureFormat::R32F:     return GL_R32F;
        case TextureFormat::Depth24:  return GL_DEPTH_COMPONENT24;
    }
    return GL_RGBA8;
}

static GLenum FormatToBaseFormat(TextureFormat fmt) {
    switch (fmt) {
        case TextureFormat::RGBA8:
        case TextureFormat::RGBA16F:  return GL_RGBA;
        case TextureFormat::RGB16F:   return GL_RGB;
        case TextureFormat::RG16F:    return GL_RG;
        case TextureFormat::R32F:     return GL_RED;
        case TextureFormat::Depth24:  return GL_DEPTH_COMPONENT;
    }
    return GL_RGBA;
}

static GLenum FormatToDataType(TextureFormat fmt) {
    switch (fmt) {
        case TextureFormat::RGBA8:    return GL_UNSIGNED_BYTE;
        case TextureFormat::RGBA16F:
        case TextureFormat::RGB16F:
        case TextureFormat::RG16F:
        case TextureFormat::R32F:
        case TextureFormat::Depth24:  return GL_FLOAT;
    }
    return GL_UNSIGNED_BYTE;
}

// ── Framebuffer ─────────────────────────────────────────────

Framebuffer::Framebuffer(const FramebufferSpec& spec)
    : m_Spec(spec)
{
    Invalidate();
}

Framebuffer::~Framebuffer() {
    Cleanup();
}

void Framebuffer::Bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_Spec.Width, m_Spec.Height);
}

void Framebuffer::Unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Resize(u32 width, u32 height) {
    if (width == 0 || height == 0 || width > 8192 || height > 8192) {
        LOG_WARN("[FBO] 无效尺寸: %ux%u", width, height);
        return;
    }
    m_Spec.Width = width;
    m_Spec.Height = height;
    Invalidate();
}

u32 Framebuffer::GetColorAttachmentID(u32 index) const {
    if (index < m_ColorAttachments.size())
        return m_ColorAttachments[index];
    return 0;
}

void Framebuffer::Invalidate() {
    if (m_FBO) Cleanup();

    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

    // ── 确定颜色附件格式列表 ──────────────────────────────
    std::vector<TextureFormat> formats = m_Spec.ColorFormats;
    if (formats.empty()) {
        // 向后兼容: 使用 HDR 标志创建单附件
        formats.push_back(m_Spec.HDR ? TextureFormat::RGBA16F : TextureFormat::RGBA8);
    }

    // ── 创建颜色附件 ─────────────────────────────────────
    m_ColorAttachments.resize(formats.size());
    glGenTextures((GLsizei)formats.size(), m_ColorAttachments.data());

    std::vector<GLenum> drawBuffers;
    for (u32 i = 0; i < formats.size(); i++) {
        glBindTexture(GL_TEXTURE_2D, m_ColorAttachments[i]);
        glTexImage2D(GL_TEXTURE_2D, 0,
                     FormatToInternalFormat(formats[i]),
                     m_Spec.Width, m_Spec.Height, 0,
                     FormatToBaseFormat(formats[i]),
                     FormatToDataType(formats[i]),
                     nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                               GL_TEXTURE_2D, m_ColorAttachments[i], 0);
        drawBuffers.push_back(GL_COLOR_ATTACHMENT0 + i);
    }

    // MRT: 告知 OpenGL 我们要写入哪些颜色附件
    if (drawBuffers.size() > 1) {
        glDrawBuffers((GLsizei)drawBuffers.size(), drawBuffers.data());
    }

    // ── 深度附件 ─────────────────────────────────────────
    if (m_Spec.DepthAttachment) {
        glGenTextures(1, &m_DepthAttachment);
        glBindTexture(GL_TEXTURE_2D, m_DepthAttachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                     m_Spec.Width, m_Spec.Height, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D, m_DepthAttachment, 0);
    }

    // ── 验证 FBO 完整性 ──────────────────────────────────
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR("[FBO] 帧缓冲不完整! 状态: 0x%x  附件数: %u", status, (u32)formats.size());
    } else {
        LOG_DEBUG("[FBO] 创建成功 %ux%u (ID=%u, %u 个颜色附件)",
                  m_Spec.Width, m_Spec.Height, m_FBO, (u32)formats.size());
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Cleanup() {
    if (m_FBO) {
        glDeleteFramebuffers(1, &m_FBO);
        m_FBO = 0;
    }
    if (!m_ColorAttachments.empty()) {
        glDeleteTextures((GLsizei)m_ColorAttachments.size(), m_ColorAttachments.data());
        m_ColorAttachments.clear();
    }
    if (m_DepthAttachment) {
        glDeleteTextures(1, &m_DepthAttachment);
        m_DepthAttachment = 0;
    }
}

} // namespace Engine
