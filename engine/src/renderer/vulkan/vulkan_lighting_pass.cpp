#include "engine/renderer/vulkan/vulkan_lighting_pass.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/renderer/vulkan/vulkan_g_buffer.h"
#include "engine/renderer/vulkan/vulkan_screen_quad.h"
#include "engine/renderer/vulkan/vulkan_texture.h"
#include "engine/core/log.h"

#include <array>
#include <cstring>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

VkImage        VulkanLightingPass::s_HDRColorImage   = VK_NULL_HANDLE;
VkDeviceMemory VulkanLightingPass::s_HDRColorMemory  = VK_NULL_HANDLE;
VkImageView    VulkanLightingPass::s_HDRColorView    = VK_NULL_HANDLE;
VkSampler      VulkanLightingPass::s_HDRSampler      = VK_NULL_HANDLE;
VkFramebuffer  VulkanLightingPass::s_HDRFramebuffer  = VK_NULL_HANDLE;
VkRenderPass   VulkanLightingPass::s_RenderPass      = VK_NULL_HANDLE;

VkBuffer       VulkanLightingPass::s_LightUBO        = VK_NULL_HANDLE;
VkDeviceMemory VulkanLightingPass::s_LightUBOMemory  = VK_NULL_HANDLE;
void*          VulkanLightingPass::s_LightUBOMapped  = nullptr;

VkDescriptorSetLayout VulkanLightingPass::s_DescLayout    = VK_NULL_HANDLE;
VkDescriptorSet       VulkanLightingPass::s_DescSet       = VK_NULL_HANDLE;

VkPipeline       VulkanLightingPass::s_Pipeline       = VK_NULL_HANDLE;
VkPipelineLayout VulkanLightingPass::s_PipelineLayout = VK_NULL_HANDLE;

u32 VulkanLightingPass::s_Width  = 0;
u32 VulkanLightingPass::s_Height = 0;

// ── 初始化 ──────────────────────────────────────────────────

bool VulkanLightingPass::Init(u32 width, u32 height) {
    s_Width  = width;
    s_Height = height;

    if (!CreateRenderPass()) return false;
    if (!CreateHDRTarget(width, height)) return false;

    // 创建光照 UBO (持久映射)
    VulkanBuffer::CreateBuffer(sizeof(VulkanLightingUBO),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        s_LightUBO, s_LightUBOMemory);
    vkMapMemory(VulkanContext::GetDevice(), s_LightUBOMemory,
                0, sizeof(VulkanLightingUBO), 0, &s_LightUBOMapped);

    // 创建 Descriptor Layout
    // binding 0-3 = G-Buffer 纹理 (Position, Normal, Albedo, Emissive)
    // binding 4   = 光照 UBO
    // binding 5   = 深度纹理 (for SSAO/shadow 采样)
    VulkanDescriptorSetLayoutBuilder layoutBuilder;
    for (u32 i = 0; i < 4; i++) {
        layoutBuilder.AddBinding(i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                 VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    layoutBuilder.AddBinding(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                             VK_SHADER_STAGE_FRAGMENT_BIT);
    layoutBuilder.AddBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                             VK_SHADER_STAGE_FRAGMENT_BIT);

    s_DescLayout = layoutBuilder.Build();
    if (s_DescLayout == VK_NULL_HANDLE) {
        LOG_ERROR("[Vulkan] LightingPass Descriptor Layout 创建失败");
        return false;
    }

    // 创建 HDR Sampler
    s_HDRSampler = VulkanSampler::Create(VulkanFilterMode::Linear, 1.0f, 0.0f);

    LOG_INFO("[Vulkan] LightingPass 初始化完成 (%ux%u)", width, height);
    return true;
}

void VulkanLightingPass::Shutdown() {
    VkDevice device = VulkanContext::GetDevice();

    if (s_LightUBOMapped) {
        vkUnmapMemory(device, s_LightUBOMemory);
        s_LightUBOMapped = nullptr;
    }
    VulkanBuffer::DestroyBuffer(s_LightUBO, s_LightUBOMemory);

    DestroyHDRTarget();
    VulkanSampler::Destroy(s_HDRSampler);
    s_HDRSampler = VK_NULL_HANDLE;

    if (s_Pipeline) {
        vkDestroyPipeline(device, s_Pipeline, nullptr);
        s_Pipeline = VK_NULL_HANDLE;
    }
    if (s_PipelineLayout) {
        vkDestroyPipelineLayout(device, s_PipelineLayout, nullptr);
        s_PipelineLayout = VK_NULL_HANDLE;
    }
    if (s_DescLayout) {
        vkDestroyDescriptorSetLayout(device, s_DescLayout, nullptr);
        s_DescLayout = VK_NULL_HANDLE;
    }
    if (s_RenderPass) {
        vkDestroyRenderPass(device, s_RenderPass, nullptr);
        s_RenderPass = VK_NULL_HANDLE;
    }
}

void VulkanLightingPass::Resize(u32 width, u32 height) {
    if (width == 0 || height == 0) return;

    vkDeviceWaitIdle(VulkanContext::GetDevice());
    DestroyHDRTarget();

    s_Width  = width;
    s_Height = height;
    CreateHDRTarget(width, height);
}

// ── 执行光照 Pass ───────────────────────────────────────────

void VulkanLightingPass::Execute(VkCommandBuffer cmd, const VulkanLightingUBO& lightData) {
    // 更新光照 UBO
    if (s_LightUBOMapped) {
        std::memcpy(s_LightUBOMapped, &lightData, sizeof(VulkanLightingUBO));
    }

    // 开始 HDR RenderPass
    VkClearValue clearValue = {};
    clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderPassBeginInfo rpInfo = {};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = s_RenderPass;
    rpInfo.framebuffer       = s_HDRFramebuffer;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = {s_Width, s_Height};
    rpInfo.clearValueCount   = 1;
    rpInfo.pClearValues      = &clearValue;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    // 设置 viewport 和 scissor
    VkViewport viewport = {};
    viewport.width  = (f32)s_Width;
    viewport.height = (f32)s_Height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {{0, 0}, {s_Width, s_Height}};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // 绑定 Pipeline 和 Descriptor Set
    if (s_Pipeline) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_Pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                s_PipelineLayout, 0, 1, &s_DescSet, 0, nullptr);
        // 绘制全屏四边形
        VulkanScreenQuad::Draw(cmd);
    }

    vkCmdEndRenderPass(cmd);
}

VkDescriptorImageInfo VulkanLightingPass::GetHDRDescriptorInfo() {
    VkDescriptorImageInfo info = {};
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.imageView   = s_HDRColorView;
    info.sampler     = s_HDRSampler;
    return info;
}

// ── 内部: 创建 RenderPass ───────────────────────────────────

bool VulkanLightingPass::CreateRenderPass() {
    // HDR 颜色附件
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format         = VK_FORMAT_R16G16B16A16_SFLOAT;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency dep = {};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments    = &colorAttachment;
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies   = &dep;

    if (vkCreateRenderPass(VulkanContext::GetDevice(), &rpInfo, nullptr, &s_RenderPass) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] LightingPass RenderPass 创建失败");
        return false;
    }
    return true;
}

// ── 内部: HDR 目标 ──────────────────────────────────────────

bool VulkanLightingPass::CreateHDRTarget(u32 width, u32 height) {
    VkFormat hdrFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

    VulkanImage::CreateImage(width, height, hdrFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        s_HDRColorImage, s_HDRColorMemory);

    s_HDRColorView = VulkanImage::CreateImageView(
        s_HDRColorImage, hdrFormat, VK_IMAGE_ASPECT_COLOR_BIT);

    // Framebuffer
    VkFramebufferCreateInfo fbInfo = {};
    fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass      = s_RenderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments    = &s_HDRColorView;
    fbInfo.width           = width;
    fbInfo.height          = height;
    fbInfo.layers          = 1;

    if (vkCreateFramebuffer(VulkanContext::GetDevice(), &fbInfo, nullptr, &s_HDRFramebuffer) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] LightingPass HDR Framebuffer 创建失败");
        return false;
    }
    return true;
}

void VulkanLightingPass::DestroyHDRTarget() {
    VkDevice device = VulkanContext::GetDevice();

    if (s_HDRFramebuffer) {
        vkDestroyFramebuffer(device, s_HDRFramebuffer, nullptr);
        s_HDRFramebuffer = VK_NULL_HANDLE;
    }
    VulkanImage::DestroyImage(s_HDRColorImage, s_HDRColorMemory, s_HDRColorView);
    s_HDRColorImage  = VK_NULL_HANDLE;
    s_HDRColorMemory = VK_NULL_HANDLE;
    s_HDRColorView   = VK_NULL_HANDLE;
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
