#include "engine/renderer/framebuffer.h"
#include "engine/core/log.h"

#include <glad/glad.h>

namespace Engine {

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

void Framebuffer::Invalidate() {
    if (m_FBO) Cleanup();

    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

    // 颜色附件 (HDR: RGBA16F, LDR: RGBA8)
    glGenTextures(1, &m_ColorAttachment);
    glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);
    if (m_Spec.HDR) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_Spec.Width, m_Spec.Height, 0,
                     GL_RGBA, GL_FLOAT, nullptr);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Spec.Width, m_Spec.Height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           m_ColorAttachment, 0);

    // 深度附件
    if (m_Spec.DepthAttachment) {
        glGenTextures(1, &m_DepthAttachment);
        glBindTexture(GL_TEXTURE_2D, m_DepthAttachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_Spec.Width, m_Spec.Height, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                               m_DepthAttachment, 0);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR("[FBO] 帧缓冲不完整! 状态: 0x%x", status);
    } else {
        LOG_DEBUG("[FBO] 创建成功 %ux%u (ID=%u)", m_Spec.Width, m_Spec.Height, m_FBO);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Cleanup() {
    if (m_FBO) {
        glDeleteFramebuffers(1, &m_FBO);
        m_FBO = 0;
    }
    if (m_ColorAttachment) {
        glDeleteTextures(1, &m_ColorAttachment);
        m_ColorAttachment = 0;
    }
    if (m_DepthAttachment) {
        glDeleteTextures(1, &m_DepthAttachment);
        m_DepthAttachment = 0;
    }
}

} // namespace Engine
