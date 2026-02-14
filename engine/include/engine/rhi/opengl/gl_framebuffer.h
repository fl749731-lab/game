#pragma once

#include "engine/rhi/rhi_framebuffer.h"

#include <glad/glad.h>
#include <vector>

namespace Engine {

// ── OpenGL 帧缓冲 ──────────────────────────────────────────

class GLFramebuffer : public RHIFramebuffer {
public:
    GLFramebuffer(const RHIFramebufferSpec& spec);
    ~GLFramebuffer() override;

    GLFramebuffer(const GLFramebuffer&) = delete;
    GLFramebuffer& operator=(const GLFramebuffer&) = delete;

    void Bind() const override;
    void Unbind() const override;
    void Resize(u32 width, u32 height) override;

    u32  GetColorAttachmentCount() const override { return (u32)m_ColorAttachments.size(); }
    u32  GetWidth() const override  { return m_Spec.Width; }
    u32  GetHeight() const override { return m_Spec.Height; }
    bool IsValid() const override   { return m_FBO != 0; }

    /// OpenGL 特有: 获取颜色/深度附件纹理 ID
    u32 GetColorAttachmentID(u32 index = 0) const;
    u32 GetDepthAttachmentID() const { return m_DepthAttachment; }
    u32 GetFBO() const { return m_FBO; }

private:
    void Invalidate();
    void Cleanup();

    u32 m_FBO = 0;
    std::vector<u32> m_ColorAttachments;
    u32 m_DepthAttachment = 0;
    RHIFramebufferSpec m_Spec;
};

} // namespace Engine
