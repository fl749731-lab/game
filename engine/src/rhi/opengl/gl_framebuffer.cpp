#include "engine/rhi/opengl/gl_framebuffer.h"
#include "engine/core/log.h"

// GL_RG 可能在某些 GLAD 配置中缺失
#ifndef GL_RG
#define GL_RG 0x8227
#endif

namespace Engine {

// ── 工具: RHITextureFormat → OpenGL 参数 ────────────────────

static GLenum FormatToInternalGL(RHITextureFormat fmt) {
    switch (fmt) {
        case RHITextureFormat::RGBA8:    return GL_RGBA8;
        case RHITextureFormat::RGBA16F:  return GL_RGBA16F;
        case RHITextureFormat::RGB16F:   return GL_RGB16F;
        case RHITextureFormat::RG16F:    return GL_RG16F;
        case RHITextureFormat::R32F:     return GL_R32F;
        case RHITextureFormat::Depth24:  return GL_DEPTH_COMPONENT24;
    }
    return GL_RGBA8;
}

static GLenum FormatToBaseGL(RHITextureFormat fmt) {
    switch (fmt) {
        case RHITextureFormat::RGBA8:
        case RHITextureFormat::RGBA16F:  return GL_RGBA;
        case RHITextureFormat::RGB16F:   return GL_RGB;
        case RHITextureFormat::RG16F:    return GL_RG;
        case RHITextureFormat::R32F:     return GL_RED;
        case RHITextureFormat::Depth24:  return GL_DEPTH_COMPONENT;
    }
    return GL_RGBA;
}

static GLenum FormatToDataTypeGL(RHITextureFormat fmt) {
    switch (fmt) {
        case RHITextureFormat::RGBA8:   return GL_UNSIGNED_BYTE;
        case RHITextureFormat::RGBA16F:
        case RHITextureFormat::RGB16F:
        case RHITextureFormat::RG16F:
        case RHITextureFormat::R32F:
        case RHITextureFormat::Depth24: return GL_FLOAT;
    }
    return GL_UNSIGNED_BYTE;
}

// ── GLFramebuffer ───────────────────────────────────────────

GLFramebuffer::GLFramebuffer(const RHIFramebufferSpec& spec)
    : m_Spec(spec)
{
    Invalidate();
}

GLFramebuffer::~GLFramebuffer() {
    Cleanup();
}

void GLFramebuffer::Bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_Spec.Width, m_Spec.Height);
}

void GLFramebuffer::Unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLFramebuffer::Resize(u32 width, u32 height) {
    if (width == 0 || height == 0 || width > 8192 || height > 8192) {
        LOG_WARN("[GLFramebuffer] 无效尺寸: %ux%u", width, height);
        return;
    }
    m_Spec.Width  = width;
    m_Spec.Height = height;
    Invalidate();
}

u32 GLFramebuffer::GetColorAttachmentID(u32 index) const {
    if (index < m_ColorAttachments.size())
        return m_ColorAttachments[index];
    return 0;
}

void GLFramebuffer::Invalidate() {
    if (m_FBO) Cleanup();

    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

    // ── 确定颜色附件格式列表
    std::vector<RHITextureFormat> formats = m_Spec.ColorFormats;
    if (formats.empty()) {
        formats.push_back(m_Spec.HDR ? RHITextureFormat::RGBA16F : RHITextureFormat::RGBA8);
    }

    // ── 创建颜色附件
    m_ColorAttachments.resize(formats.size());
    glGenTextures((GLsizei)formats.size(), m_ColorAttachments.data());

    std::vector<GLenum> drawBuffers;
    for (u32 i = 0; i < formats.size(); i++) {
        glBindTexture(GL_TEXTURE_2D, m_ColorAttachments[i]);
        glTexImage2D(GL_TEXTURE_2D, 0,
                     FormatToInternalGL(formats[i]),
                     m_Spec.Width, m_Spec.Height, 0,
                     FormatToBaseGL(formats[i]),
                     FormatToDataTypeGL(formats[i]),
                     nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                               GL_TEXTURE_2D, m_ColorAttachments[i], 0);
        drawBuffers.push_back(GL_COLOR_ATTACHMENT0 + i);
    }

    if (drawBuffers.size() > 1) {
        glDrawBuffers((GLsizei)drawBuffers.size(), drawBuffers.data());
    }

    // ── 深度附件
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

    // ── 验证
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR("[GLFramebuffer] 帧缓冲不完整! 状态: 0x%x", status);
    } else {
        LOG_DEBUG("[GLFramebuffer] 创建成功 %ux%u (ID=%u, %u 附件)",
                  m_Spec.Width, m_Spec.Height, m_FBO, (u32)formats.size());
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLFramebuffer::Cleanup() {
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
