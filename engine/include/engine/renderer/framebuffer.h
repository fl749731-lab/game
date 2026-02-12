#pragma once

#include "engine/core/types.h"

namespace Engine {

// ── 帧缓冲 (渲染到纹理) ─────────────────────────────────────

struct FramebufferSpec {
    u32 Width = 1280;
    u32 Height = 720;
    bool DepthAttachment = true;
    bool HDR = false;  // 使用 GL_RGBA16F
};

class Framebuffer {
public:
    Framebuffer(const FramebufferSpec& spec);
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    /// 绑定为渲染目标
    void Bind() const;

    /// 解绑回默认帧缓冲
    void Unbind() const;

    /// 窗口大小变化时重建
    void Resize(u32 width, u32 height);

    /// 获取颜色附件纹理 ID（可用于后处理）
    u32 GetColorAttachmentID() const { return m_ColorAttachment; }
    u32 GetDepthAttachmentID() const { return m_DepthAttachment; }

    u32 GetWidth() const { return m_Spec.Width; }
    u32 GetHeight() const { return m_Spec.Height; }

    bool IsValid() const { return m_FBO != 0; }

private:
    void Invalidate();
    void Cleanup();

    u32 m_FBO = 0;
    u32 m_ColorAttachment = 0;
    u32 m_DepthAttachment = 0;
    FramebufferSpec m_Spec;
};

} // namespace Engine
