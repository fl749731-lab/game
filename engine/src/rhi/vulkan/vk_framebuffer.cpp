#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/rhi/vulkan/vk_framebuffer.h"
#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/renderer/vulkan/vulkan_texture.h"
#include "engine/core/log.h"

#include <array>

namespace Engine {

// ── 辅助: RHI 格式 → VkFormat ───────────────────────────────

static VkFormat ToVkFormat(RHITextureFormat fmt) {
    switch (fmt) {
        case RHITextureFormat::RGBA8:    return VK_FORMAT_R8G8B8A8_UNORM;
        case RHITextureFormat::RGBA16F:  return VK_FORMAT_R16G16B16A16_SFLOAT;
        case RHITextureFormat::RGB16F:   return VK_FORMAT_R16G16B16_SFLOAT;
        case RHITextureFormat::RG16F:    return VK_FORMAT_R16G16_SFLOAT;
        case RHITextureFormat::R32F:     return VK_FORMAT_R32_SFLOAT;
        case RHITextureFormat::Depth24:  return VK_FORMAT_D24_UNORM_S8_UINT;
        default:                         return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

static bool IsDepthFormat(VkFormat fmt) {
    return fmt == VK_FORMAT_D16_UNORM ||
           fmt == VK_FORMAT_D24_UNORM_S8_UINT ||
           fmt == VK_FORMAT_D32_SFLOAT ||
           fmt == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

// ── 构造 ────────────────────────────────────────────────────

VKFramebuffer::VKFramebuffer(const RHIFramebufferSpec& spec)
    : m_Width(spec.Width), m_Height(spec.Height), m_Spec(spec)
{
    m_ColorAttachmentCount = (u32)spec.ColorFormats.size();
    if (m_ColorAttachmentCount == 0) m_ColorAttachmentCount = 1;
    Invalidate();
}

VKFramebuffer::~VKFramebuffer() {
    Cleanup();
}

void VKFramebuffer::Bind() const {
    // Vulkan 帧缓冲通过 vkCmdBeginRenderPass 绑定
    auto cmd = VulkanRenderer::GetCurrentCommandBuffer();
    if (!cmd || m_Framebuffer == VK_NULL_HANDLE) return;

    // 动态构建 clearValues: 每个颜色附件 + 可选深度附件
    std::vector<VkClearValue> clearValues(m_ColorAttachmentCount);
    for (u32 i = 0; i < m_ColorAttachmentCount; i++) {
        clearValues[i].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    }
    if (m_Spec.DepthAttachment) {
        VkClearValue depthClear = {};
        depthClear.depthStencil = {1.0f, 0};
        clearValues.push_back(depthClear);
    }

    VkRenderPassBeginInfo rpInfo = {};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = m_RenderPass;
    rpInfo.framebuffer       = m_Framebuffer;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = {m_Width, m_Height};
    rpInfo.clearValueCount   = (u32)clearValues.size();
    rpInfo.pClearValues      = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    // 设置 Viewport 和 Scissor
    VkViewport viewport = {};
    viewport.width    = (f32)m_Width;
    viewport.height   = (f32)m_Height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {{0, 0}, {m_Width, m_Height}};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void VKFramebuffer::Unbind() const {
    auto cmd = VulkanRenderer::GetCurrentCommandBuffer();
    if (cmd) {
        vkCmdEndRenderPass(cmd);
    }
}

void VKFramebuffer::Resize(u32 width, u32 height) {
    if (width == 0 || height == 0) return;
    m_Width  = width;
    m_Height = height;
    Invalidate();
}

// ── 内部: 创建/重建 Vulkan 资源 ─────────────────────────────

void VKFramebuffer::Invalidate() {
    Cleanup();

    VkDevice device = VulkanContext::GetDevice();

    // ── 1. 创建颜色附件 ──────────────────────────────────────
    m_ColorImages.resize(m_ColorAttachmentCount);
    m_ColorMemory.resize(m_ColorAttachmentCount);
    m_ColorViews.resize(m_ColorAttachmentCount);

    for (u32 i = 0; i < m_ColorAttachmentCount; i++) {
        VkFormat fmt = (i < m_Spec.ColorFormats.size())
            ? ToVkFormat(m_Spec.ColorFormats[i])
            : VK_FORMAT_R8G8B8A8_UNORM;

        VulkanImage::CreateImage(m_Width, m_Height, fmt,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_ColorImages[i], m_ColorMemory[i]);

        m_ColorViews[i] = VulkanImage::CreateImageView(
            m_ColorImages[i], fmt, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    // ── 2. 创建深度附件 ──────────────────────────────────────
    if (m_Spec.DepthAttachment) {
        VkFormat depthFmt = VulkanContext::FindDepthFormat();
        VulkanImage::CreateImage(m_Width, m_Height, depthFmt,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_DepthImage, m_DepthMemory);

        m_DepthView = VulkanImage::CreateImageView(
            m_DepthImage, depthFmt, VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    // ── 3. 创建 RenderPass ───────────────────────────────────
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> colorRefs;

    for (u32 i = 0; i < m_ColorAttachmentCount; i++) {
        VkFormat fmt = (i < m_Spec.ColorFormats.size())
            ? ToVkFormat(m_Spec.ColorFormats[i])
            : VK_FORMAT_R8G8B8A8_UNORM;

        VkAttachmentDescription att = {};
        att.format         = fmt;
        att.samples        = VK_SAMPLE_COUNT_1_BIT;
        att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        att.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachments.push_back(att);

        colorRefs.push_back({i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    }

    VkAttachmentReference depthRef = {};
    if (m_Spec.DepthAttachment) {
        VkAttachmentDescription depthAtt = {};
        depthAtt.format         = VulkanContext::FindDepthFormat();
        depthAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
        depthAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAtt.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAtt.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments.push_back(depthAtt);

        depthRef = {m_ColorAttachmentCount, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    }

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = (u32)colorRefs.size();
    subpass.pColorAttachments       = colorRefs.data();
    subpass.pDepthStencilAttachment = m_Spec.DepthAttachment ? &depthRef : nullptr;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                             | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                             | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                             | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = (u32)attachments.size();
    rpInfo.pAttachments    = attachments.data();
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies   = &dependency;

    if (vkCreateRenderPass(device, &rpInfo, nullptr, &m_RenderPass) != VK_SUCCESS) {
        LOG_ERROR("[VKFramebuffer] 创建 RenderPass 失败");
        return;
    }

    // ── 4. 创建 Framebuffer ──────────────────────────────────
    std::vector<VkImageView> fbAttachments;
    for (auto& v : m_ColorViews) fbAttachments.push_back(v);
    if (m_Spec.DepthAttachment) fbAttachments.push_back(m_DepthView);

    VkFramebufferCreateInfo fbInfo = {};
    fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass      = m_RenderPass;
    fbInfo.attachmentCount = (u32)fbAttachments.size();
    fbInfo.pAttachments    = fbAttachments.data();
    fbInfo.width           = m_Width;
    fbInfo.height          = m_Height;
    fbInfo.layers          = 1;

    if (vkCreateFramebuffer(device, &fbInfo, nullptr, &m_Framebuffer) != VK_SUCCESS) {
        LOG_ERROR("[VKFramebuffer] 创建 Framebuffer 失败");
        return;
    }

    LOG_DEBUG("[VKFramebuffer] 创建完成: %ux%u (%u 颜色附件, %s深度)",
              m_Width, m_Height, m_ColorAttachmentCount,
              m_Spec.DepthAttachment ? "有" : "无");
}

void VKFramebuffer::Cleanup() {
    VkDevice device = VulkanContext::GetDevice();
    if (!device) return;

    vkDeviceWaitIdle(device);

    if (m_Framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, m_Framebuffer, nullptr);
        m_Framebuffer = VK_NULL_HANDLE;
    }
    if (m_RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_RenderPass, nullptr);
        m_RenderPass = VK_NULL_HANDLE;
    }

    // 清理深度
    if (m_DepthView != VK_NULL_HANDLE || m_DepthImage != VK_NULL_HANDLE) {
        VulkanImage::DestroyImage(m_DepthImage, m_DepthMemory, m_DepthView);
        m_DepthImage  = VK_NULL_HANDLE;
        m_DepthMemory = VK_NULL_HANDLE;
        m_DepthView   = VK_NULL_HANDLE;
    }

    // 清理颜色附件
    for (u32 i = 0; i < m_ColorImages.size(); i++) {
        VulkanImage::DestroyImage(m_ColorImages[i], m_ColorMemory[i], m_ColorViews[i]);
    }
    m_ColorImages.clear();
    m_ColorMemory.clear();
    m_ColorViews.clear();
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
