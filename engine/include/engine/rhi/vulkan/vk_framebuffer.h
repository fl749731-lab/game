#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/rhi/rhi_framebuffer.h"
#include "engine/rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace Engine {

// ── Vulkan 帧缓冲 (RHI 封装) ───────────────────────────────
// 离屏渲染: 自建 RenderPass + Color/Depth Image + VkFramebuffer

class VKFramebuffer : public RHIFramebuffer {
public:
    VKFramebuffer(const RHIFramebufferSpec& spec);
    ~VKFramebuffer() override;

    void Bind() const override;
    void Unbind() const override;
    void Resize(u32 width, u32 height) override;

    u32  GetColorAttachmentCount() const override { return m_ColorAttachmentCount; }
    u32  GetWidth() const override  { return m_Width; }
    u32  GetHeight() const override { return m_Height; }
    bool IsValid() const override   { return m_Framebuffer != VK_NULL_HANDLE; }

    /// Vulkan 专用
    VkFramebuffer  GetVkFramebuffer() const { return m_Framebuffer; }
    VkRenderPass   GetRenderPass()    const { return m_RenderPass; }
    VkImageView    GetColorImageView(u32 index) const {
        return (index < m_ColorViews.size()) ? m_ColorViews[index] : VK_NULL_HANDLE;
    }

private:
    void Invalidate();
    void Cleanup();

    RHIFramebufferSpec m_Spec;
    u32 m_Width  = 0;
    u32 m_Height = 0;
    u32 m_ColorAttachmentCount = 0;

    VkRenderPass  m_RenderPass  = VK_NULL_HANDLE;
    VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;

    // Color 附件
    std::vector<VkImage>        m_ColorImages;
    std::vector<VkDeviceMemory> m_ColorMemory;
    std::vector<VkImageView>    m_ColorViews;

    // Depth 附件
    VkImage        m_DepthImage  = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthMemory = VK_NULL_HANDLE;
    VkImageView    m_DepthView   = VK_NULL_HANDLE;
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
