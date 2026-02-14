#include "engine/renderer/vulkan/vulkan_bloom.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/renderer/vulkan/vulkan_texture.h"
#include "engine/renderer/vulkan/vulkan_screen_quad.h"
#include "engine/renderer/vulkan/vulkan_descriptor.h"
#include "engine/core/log.h"

#include <array>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

VulkanBloomConfig VulkanBloom::s_Config = {};

VkImage        VulkanBloom::s_BloomImages[2]   = {};
VkDeviceMemory VulkanBloom::s_BloomMemories[2] = {};
VkImageView    VulkanBloom::s_BloomViews[2]    = {};
VkFramebuffer  VulkanBloom::s_BloomFBOs[2]     = {};
VkSampler      VulkanBloom::s_BloomSampler     = VK_NULL_HANDLE;

VkRenderPass   VulkanBloom::s_RenderPass = VK_NULL_HANDLE;

VkPipeline       VulkanBloom::s_ExtractPipeline = VK_NULL_HANDLE;
VkPipelineLayout VulkanBloom::s_ExtractLayout   = VK_NULL_HANDLE;

VkPipeline       VulkanBloom::s_BlurPipeline = VK_NULL_HANDLE;
VkPipelineLayout VulkanBloom::s_BlurLayout   = VK_NULL_HANDLE;

VkDescriptorSetLayout VulkanBloom::s_DescLayout      = VK_NULL_HANDLE;
VkDescriptorSet       VulkanBloom::s_ExtractDescSet   = VK_NULL_HANDLE;
VkDescriptorSet       VulkanBloom::s_BlurDescSets[2]  = {};

u32 VulkanBloom::s_Width  = 0;
u32 VulkanBloom::s_Height = 0;

// ── 初始化 ──────────────────────────────────────────────────

bool VulkanBloom::Init(u32 width, u32 height) {
    s_Width  = width / 2;  // 半分辨率
    s_Height = height / 2;

    if (!CreateRenderPasses()) return false;
    if (!CreatePingPongTargets(s_Width, s_Height)) return false;

    // Descriptor Layout (单纹理输入)
    VulkanDescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                             VK_SHADER_STAGE_FRAGMENT_BIT);
    s_DescLayout = layoutBuilder.Build();

    // 采样器
    s_BloomSampler = VulkanSampler::Create(VulkanFilterMode::Linear, 1.0f, 0.0f);

    LOG_INFO("[Vulkan] Bloom 初始化完成 (%ux%u, 半分辨率)", s_Width, s_Height);
    return true;
}

void VulkanBloom::Shutdown() {
    DestroyResources();
    VkDevice device = VulkanContext::GetDevice();

    VulkanSampler::Destroy(s_BloomSampler);
    s_BloomSampler = VK_NULL_HANDLE;

    if (s_ExtractPipeline) {
        vkDestroyPipeline(device, s_ExtractPipeline, nullptr);
        s_ExtractPipeline = VK_NULL_HANDLE;
    }
    if (s_ExtractLayout) {
        vkDestroyPipelineLayout(device, s_ExtractLayout, nullptr);
        s_ExtractLayout = VK_NULL_HANDLE;
    }
    if (s_BlurPipeline) {
        vkDestroyPipeline(device, s_BlurPipeline, nullptr);
        s_BlurPipeline = VK_NULL_HANDLE;
    }
    if (s_BlurLayout) {
        vkDestroyPipelineLayout(device, s_BlurLayout, nullptr);
        s_BlurLayout = VK_NULL_HANDLE;
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

void VulkanBloom::Resize(u32 width, u32 height) {
    if (width == 0 || height == 0) return;

    vkDeviceWaitIdle(VulkanContext::GetDevice());
    DestroyResources();

    s_Width  = width / 2;
    s_Height = height / 2;
    CreatePingPongTargets(s_Width, s_Height);
}

// ── 执行 Bloom ──────────────────────────────────────────────

void VulkanBloom::Execute(VkCommandBuffer cmd, VkImageView hdrView, VkSampler hdrSampler) {
    if (!s_Config.Enabled) return;

    // 阶段 1: 亮度提取 (HDR → BloomFBO[0] — 半分辨率)
    {
        VkClearValue clearValue = {};
        clearValue.color = {{0.0f, 0.0f, 0.0f, 0.0f}};

        VkRenderPassBeginInfo rpInfo = {};
        rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass        = s_RenderPass;
        rpInfo.framebuffer       = s_BloomFBOs[0];
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = {s_Width, s_Height};
        rpInfo.clearValueCount   = 1;
        rpInfo.pClearValues      = &clearValue;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp = {};
        vp.width  = (f32)s_Width;
        vp.height = (f32)s_Height;
        vp.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &vp);

        VkRect2D scissor = {{0, 0}, {s_Width, s_Height}};
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        if (s_ExtractPipeline) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_ExtractPipeline);
            if (s_ExtractDescSet) {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        s_ExtractLayout, 0, 1, &s_ExtractDescSet, 0, nullptr);
            }
            VulkanScreenQuad::Draw(cmd);
        }

        vkCmdEndRenderPass(cmd);
    }

    // 阶段 2: Gaussian Blur PingPong
    for (u32 i = 0; i < s_Config.Iterations; i++) {
        u32 srcIdx = (i % 2 == 0) ? 0 : 1;
        u32 dstIdx = (i % 2 == 0) ? 1 : 0;

        VkClearValue clearValue = {};
        clearValue.color = {{0.0f, 0.0f, 0.0f, 0.0f}};

        VkRenderPassBeginInfo rpInfo = {};
        rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass        = s_RenderPass;
        rpInfo.framebuffer       = s_BloomFBOs[dstIdx];
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = {s_Width, s_Height};
        rpInfo.clearValueCount   = 1;
        rpInfo.pClearValues      = &clearValue;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp = {};
        vp.width  = (f32)s_Width;
        vp.height = (f32)s_Height;
        vp.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &vp);

        VkRect2D scissor = {{0, 0}, {s_Width, s_Height}};
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        if (s_BlurPipeline && s_BlurDescSets[srcIdx]) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_BlurPipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    s_BlurLayout, 0, 1, &s_BlurDescSets[srcIdx], 0, nullptr);

            // Push constant: 水平/垂直方向
            u32 horizontal = (i % 2 == 0) ? 1 : 0;
            vkCmdPushConstants(cmd, s_BlurLayout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(u32), &horizontal);

            VulkanScreenQuad::Draw(cmd);
        }

        vkCmdEndRenderPass(cmd);
    }
}

// ── 内部: RenderPass ────────────────────────────────────────

bool VulkanBloom::CreateRenderPasses() {
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
    dep.srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
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
        LOG_ERROR("[Vulkan] Bloom RenderPass 创建失败");
        return false;
    }
    return true;
}

// ── 内部: PingPong 目标 ─────────────────────────────────────

bool VulkanBloom::CreatePingPongTargets(u32 width, u32 height) {
    VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

    for (u32 i = 0; i < 2; i++) {
        VulkanImage::CreateImage(width, height, format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            s_BloomImages[i], s_BloomMemories[i]);

        s_BloomViews[i] = VulkanImage::CreateImageView(
            s_BloomImages[i], format, VK_IMAGE_ASPECT_COLOR_BIT);

        // Framebuffer
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = s_RenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments    = &s_BloomViews[i];
        fbInfo.width           = width;
        fbInfo.height          = height;
        fbInfo.layers          = 1;

        if (vkCreateFramebuffer(VulkanContext::GetDevice(), &fbInfo, nullptr, &s_BloomFBOs[i]) != VK_SUCCESS) {
            LOG_ERROR("[Vulkan] Bloom FBO %u 创建失败", i);
            return false;
        }
    }
    return true;
}

void VulkanBloom::DestroyResources() {
    VkDevice device = VulkanContext::GetDevice();

    for (u32 i = 0; i < 2; i++) {
        if (s_BloomFBOs[i]) {
            vkDestroyFramebuffer(device, s_BloomFBOs[i], nullptr);
            s_BloomFBOs[i] = VK_NULL_HANDLE;
        }
        VulkanImage::DestroyImage(s_BloomImages[i], s_BloomMemories[i], s_BloomViews[i]);
        s_BloomImages[i]   = VK_NULL_HANDLE;
        s_BloomMemories[i] = VK_NULL_HANDLE;
        s_BloomViews[i]    = VK_NULL_HANDLE;
    }
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
