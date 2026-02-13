#pragma once

#include "engine/core/types.h"

#include <vector>

namespace Engine {

// ── 纹理格式 ────────────────────────────────────────────────

enum class TextureFormat {
    RGBA8,       // 标准 LDR
    RGBA16F,     // HDR 浮点
    RGB16F,      // HDR 浮点 (无 Alpha)
    RG16F,       // 2 通道浮点 (Velocity Buffer 等)
    R32F,        // 单通道浮点
    Depth24,     // 深度 24 位
};

// ── 帧缓冲规格 ──────────────────────────────────────────────

struct FramebufferSpec {
    u32 Width = 1280;
    u32 Height = 720;

    /// 颜色附件格式列表 (支持 MRT)
    /// 为空则创建一个默认 RGBA8 / RGBA16F (取决于 HDR 标志)
    std::vector<TextureFormat> ColorFormats;

    bool DepthAttachment = true;
    bool HDR = false;  // 快捷标志: true = 单附件 RGBA16F
};

// ── 帧缓冲 ──────────────────────────────────────────────────

class Framebuffer {
public:
    Framebuffer(const FramebufferSpec& spec);
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    void Bind() const;
    void Unbind() const;
    void Resize(u32 width, u32 height);

    /// 获取第 index 个颜色附件的纹理 ID
    u32 GetColorAttachmentID(u32 index = 0) const;
    u32 GetDepthAttachmentID() const { return m_DepthAttachment; }

    u32 GetColorAttachmentCount() const { return (u32)m_ColorAttachments.size(); }
    u32 GetWidth() const { return m_Spec.Width; }
    u32 GetHeight() const { return m_Spec.Height; }
    u32 GetFBO() const { return m_FBO; }
    bool IsValid() const { return m_FBO != 0; }

private:
    void Invalidate();
    void Cleanup();

    u32 m_FBO = 0;
    std::vector<u32> m_ColorAttachments;
    u32 m_DepthAttachment = 0;
    FramebufferSpec m_Spec;
};

} // namespace Engine
