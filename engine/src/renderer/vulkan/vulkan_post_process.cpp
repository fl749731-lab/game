#include "engine/renderer/vulkan/vulkan_post_process.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/renderer/vulkan/vulkan_texture.h"
#include "engine/renderer/vulkan/vulkan_screen_quad.h"
#include "engine/renderer/vulkan/vulkan_descriptor.h"
#include "engine/core/log.h"

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

VulkanPostProcessParams VulkanPostProcess::s_Params = {};

VkRenderPass     VulkanPostProcess::s_RenderPass     = VK_NULL_HANDLE;
VkPipeline       VulkanPostProcess::s_Pipeline       = VK_NULL_HANDLE;
VkPipelineLayout VulkanPostProcess::s_PipelineLayout = VK_NULL_HANDLE;

VkDescriptorSetLayout VulkanPostProcess::s_DescLayout = VK_NULL_HANDLE;
VkDescriptorSet       VulkanPostProcess::s_DescSet    = VK_NULL_HANDLE;

u32 VulkanPostProcess::s_Width  = 0;
u32 VulkanPostProcess::s_Height = 0;

// ── 初始化 ──────────────────────────────────────────────────

bool VulkanPostProcess::Init(u32 width, u32 height) {
    s_Width  = width;
    s_Height = height;

    // Descriptor Layout:
    //   binding 0 = HDR 颜色纹理
    //   binding 1 = Bloom 混合纹理 (可选)
    VulkanDescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                             VK_SHADER_STAGE_FRAGMENT_BIT);
    layoutBuilder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                             VK_SHADER_STAGE_FRAGMENT_BIT);

    s_DescLayout = layoutBuilder.Build();
    if (s_DescLayout == VK_NULL_HANDLE) {
        LOG_ERROR("[Vulkan] PostProcess Descriptor Layout 创建失败");
        return false;
    }

    LOG_INFO("[Vulkan] PostProcess 初始化完成 (%ux%u)", width, height);
    return true;
}

void VulkanPostProcess::Shutdown() {
    VkDevice device = VulkanContext::GetDevice();

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

void VulkanPostProcess::Resize(u32 width, u32 height) {
    if (width == 0 || height == 0) return;
    s_Width  = width;
    s_Height = height;
}

void VulkanPostProcess::Execute(VkCommandBuffer cmd,
                                VkImageView hdrView, VkSampler hdrSampler,
                                VkImageView bloomView, VkSampler bloomSampler) {
    // 后处理在 BeginFrame/EndFrame 的 swapchain RenderPass 中执行
    // 绑定 pipeline 并绘制全屏四边形

    if (s_Pipeline) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_Pipeline);

        // Push Constants
        PostProcessPushConstants pc;
        pc.Exposure    = s_Params.Exposure;
        pc.Gamma       = s_Params.Gamma;
        pc.VignetteStr = s_Params.VignetteStr;
        pc.VignetteRad = s_Params.VignetteRad;
        pc.HasBloom    = (bloomView != VK_NULL_HANDLE) ? 1 : 0;

        vkCmdPushConstants(cmd, s_PipelineLayout,
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PostProcessPushConstants), &pc);

        if (s_DescSet) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    s_PipelineLayout, 0, 1, &s_DescSet, 0, nullptr);
        }

        VulkanScreenQuad::Draw(cmd);
    }
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
