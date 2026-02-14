#include "engine/renderer/vulkan/vulkan_g_buffer.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/renderer/vulkan/vulkan_texture.h"
#include "engine/core/log.h"

#include <array>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

VkImage        VulkanGBuffer::s_ColorImages[4]   = {};
VkDeviceMemory VulkanGBuffer::s_ColorMemories[4] = {};
VkImageView    VulkanGBuffer::s_ColorViews[4]    = {};

VkImage        VulkanGBuffer::s_DepthImage  = VK_NULL_HANDLE;
VkDeviceMemory VulkanGBuffer::s_DepthMemory = VK_NULL_HANDLE;
VkImageView    VulkanGBuffer::s_DepthView   = VK_NULL_HANDLE;

VkRenderPass   VulkanGBuffer::s_RenderPass  = VK_NULL_HANDLE;
VkFramebuffer  VulkanGBuffer::s_Framebuffer = VK_NULL_HANDLE;
VkSampler      VulkanGBuffer::s_Sampler     = VK_NULL_HANDLE;

u32 VulkanGBuffer::s_Width  = 0;
u32 VulkanGBuffer::s_Height = 0;

VkFormat VulkanGBuffer::s_ColorFormats[4] = {
    VK_FORMAT_R16G16B16A16_SFLOAT, // Position
    VK_FORMAT_R16G16B16A16_SFLOAT, // Normal
    VK_FORMAT_R8G8B8A8_UNORM,      // Albedo + Specular
    VK_FORMAT_R8G8B8A8_UNORM       // Emissive
};

// ── 初始化 ──────────────────────────────────────────────────

bool VulkanGBuffer::Init(u32 width, u32 height) {
    s_Width  = width;
    s_Height = height;

    if (!CreateRenderPass()) return false;
    if (!CreateImages(width, height)) return false;
    if (!CreateFramebuffer(width, height)) return false;

    // 创建采样器 (光照 Pass 采样 G-Buffer 纹理)
    s_Sampler = VulkanSampler::Create(VulkanFilterMode::Nearest, 1.0f, 0.0f);

    LOG_INFO("[Vulkan] G-Buffer 初始化完成 (%ux%u)", width, height);
    return true;
}

void VulkanGBuffer::Shutdown() {
    DestroyResources();
    VkDevice device = VulkanContext::GetDevice();

    VulkanSampler::Destroy(s_Sampler);
    s_Sampler = VK_NULL_HANDLE;

    if (s_RenderPass) {
        vkDestroyRenderPass(device, s_RenderPass, nullptr);
        s_RenderPass = VK_NULL_HANDLE;
    }
}

void VulkanGBuffer::Resize(u32 width, u32 height) {
    if (width == 0 || height == 0) return;

    vkDeviceWaitIdle(VulkanContext::GetDevice());
    DestroyResources();

    s_Width  = width;
    s_Height = height;
    CreateImages(width, height);
    CreateFramebuffer(width, height);

    LOG_DEBUG("[Vulkan] G-Buffer 已 Resize (%ux%u)", width, height);
}

// ── 渲染操作 ────────────────────────────────────────────────

void VulkanGBuffer::BeginPass(VkCommandBuffer cmd) {
    std::array<VkClearValue, 5> clearValues = {};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}}; // Position
    clearValues[1].color = {{0.0f, 0.0f, 0.0f, 0.0f}}; // Normal
    clearValues[2].color = {{0.0f, 0.0f, 0.0f, 0.0f}}; // Albedo
    clearValues[3].color = {{0.0f, 0.0f, 0.0f, 0.0f}}; // Emissive
    clearValues[4].depthStencil = {1.0f, 0};             // Depth

    VkRenderPassBeginInfo rpInfo = {};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = s_RenderPass;
    rpInfo.framebuffer       = s_Framebuffer;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = {s_Width, s_Height};
    rpInfo.clearValueCount   = (u32)clearValues.size();
    rpInfo.pClearValues      = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    // 设置 viewport 和 scissor
    VkViewport viewport = {};
    viewport.x = 0.0f; viewport.y = 0.0f;
    viewport.width  = (f32)s_Width;
    viewport.height = (f32)s_Height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {{0, 0}, {s_Width, s_Height}};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void VulkanGBuffer::EndPass(VkCommandBuffer cmd) {
    vkCmdEndRenderPass(cmd);
}

void VulkanGBuffer::GetDescriptorInfos(VkDescriptorImageInfo outInfos[4]) {
    for (u32 i = 0; i < COLOR_ATTACHMENT_COUNT; i++) {
        outInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        outInfos[i].imageView   = s_ColorViews[i];
        outInfos[i].sampler     = s_Sampler;
    }
}

// ── 内部: 创建 RenderPass ───────────────────────────────────

bool VulkanGBuffer::CreateRenderPass() {
    // 4 颜色附件 + 1 深度附件
    std::array<VkAttachmentDescription, 5> attachments = {};

    for (u32 i = 0; i < COLOR_ATTACHMENT_COUNT; i++) {
        attachments[i].format         = s_ColorFormats[i];
        attachments[i].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[i].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[i].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[i].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[i].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[i].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // 深度附件
    attachments[4].format         = VulkanContext::FindDepthFormat();
    attachments[4].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[4].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[4].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[4].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[4].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[4].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 颜色引用
    std::array<VkAttachmentReference, 4> colorRefs;
    for (u32 i = 0; i < COLOR_ATTACHMENT_COUNT; i++) {
        colorRefs[i] = {i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    }

    // 深度引用
    VkAttachmentReference depthRef = {4, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = COLOR_ATTACHMENT_COUNT;
    subpass.pColorAttachments       = colorRefs.data();
    subpass.pDepthStencilAttachment = &depthRef;

    // 子通道依赖: 确保颜色输出完成后才被采样
    std::array<VkSubpassDependency, 2> deps = {};

    deps[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass    = 0;
    deps[0].srcStageMask  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    deps[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    deps[1].srcSubpass    = 0;
    deps[1].dstSubpass    = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    deps[1].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = (u32)attachments.size();
    rpInfo.pAttachments    = attachments.data();
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = (u32)deps.size();
    rpInfo.pDependencies   = deps.data();

    if (vkCreateRenderPass(VulkanContext::GetDevice(), &rpInfo, nullptr, &s_RenderPass) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] G-Buffer RenderPass 创建失败");
        return false;
    }
    return true;
}

// ── 内部: 创建 Images ───────────────────────────────────────

bool VulkanGBuffer::CreateImages(u32 width, u32 height) {
    VkDevice device = VulkanContext::GetDevice();

    // 创建 4 个颜色附件
    for (u32 i = 0; i < COLOR_ATTACHMENT_COUNT; i++) {
        VulkanImage::CreateImage(width, height, s_ColorFormats[i],
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            s_ColorImages[i], s_ColorMemories[i]);

        s_ColorViews[i] = VulkanImage::CreateImageView(
            s_ColorImages[i], s_ColorFormats[i], VK_IMAGE_ASPECT_COLOR_BIT);

        if (s_ColorViews[i] == VK_NULL_HANDLE) {
            LOG_ERROR("[Vulkan] G-Buffer 颜色附件 %u 创建失败", i);
            return false;
        }
    }

    // 创建深度附件
    VkFormat depthFormat = VulkanContext::FindDepthFormat();
    VulkanImage::CreateImage(width, height, depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        s_DepthImage, s_DepthMemory);

    s_DepthView = VulkanImage::CreateImageView(
        s_DepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    return s_DepthView != VK_NULL_HANDLE;
}

// ── 内部: 创建 Framebuffer ──────────────────────────────────

bool VulkanGBuffer::CreateFramebuffer(u32 width, u32 height) {
    std::array<VkImageView, 5> attachments = {
        s_ColorViews[0], s_ColorViews[1], s_ColorViews[2], s_ColorViews[3],
        s_DepthView
    };

    VkFramebufferCreateInfo fbInfo = {};
    fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass      = s_RenderPass;
    fbInfo.attachmentCount = (u32)attachments.size();
    fbInfo.pAttachments    = attachments.data();
    fbInfo.width           = width;
    fbInfo.height          = height;
    fbInfo.layers          = 1;

    if (vkCreateFramebuffer(VulkanContext::GetDevice(), &fbInfo, nullptr, &s_Framebuffer) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] G-Buffer Framebuffer 创建失败");
        return false;
    }
    return true;
}

// ── 内部: 销毁资源 ──────────────────────────────────────────

void VulkanGBuffer::DestroyResources() {
    VkDevice device = VulkanContext::GetDevice();

    if (s_Framebuffer) {
        vkDestroyFramebuffer(device, s_Framebuffer, nullptr);
        s_Framebuffer = VK_NULL_HANDLE;
    }

    for (u32 i = 0; i < COLOR_ATTACHMENT_COUNT; i++) {
        VulkanImage::DestroyImage(s_ColorImages[i], s_ColorMemories[i], s_ColorViews[i]);
        s_ColorImages[i]   = VK_NULL_HANDLE;
        s_ColorMemories[i] = VK_NULL_HANDLE;
        s_ColorViews[i]    = VK_NULL_HANDLE;
    }

    VulkanImage::DestroyImage(s_DepthImage, s_DepthMemory, s_DepthView);
    s_DepthImage  = VK_NULL_HANDLE;
    s_DepthMemory = VK_NULL_HANDLE;
    s_DepthView   = VK_NULL_HANDLE;
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
