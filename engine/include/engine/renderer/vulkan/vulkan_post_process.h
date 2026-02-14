#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include <vulkan/vulkan.h>

namespace Engine {

// ── Vulkan 后处理 ──────────────────────────────────────────
// HDR 色调映射 + Gamma 校正 + 晕影
// 输入: HDR 颜色纹理 (R16G16B16A16_SFLOAT)
// 输出: LDR 到 Swapchain

struct VulkanPostProcessParams {
    f32 Exposure     = 1.0f;
    f32 Gamma        = 2.2f;
    f32 VignetteStr  = 0.3f;   // 晕影强度
    f32 VignetteRad  = 0.8f;   // 晕影圆角半径
};

class VulkanPostProcess {
public:
    static bool Init(u32 width, u32 height);
    static void Shutdown();
    static void Resize(u32 width, u32 height);

    /// 执行后处理 (tone mapping + gamma + vignette)
    /// @param cmd         当前 CommandBuffer
    /// @param hdrView     HDR 颜色附件 ImageView
    /// @param hdrSampler  HDR 采样器
    /// @param bloomView   Bloom 混合后的纹理 (可选, VK_NULL_HANDLE 则不叠加)
    /// @param bloomSampler Bloom 采样器
    static void Execute(VkCommandBuffer cmd,
                        VkImageView hdrView, VkSampler hdrSampler,
                        VkImageView bloomView = VK_NULL_HANDLE,
                        VkSampler bloomSampler = VK_NULL_HANDLE);

    static VulkanPostProcessParams& GetParams() { return s_Params; }

    static VkRenderPass GetRenderPass() { return s_RenderPass; }

private:
    static VulkanPostProcessParams s_Params;

    // RenderPass (写入 swapchain)
    static VkRenderPass   s_RenderPass;
    static VkPipeline     s_Pipeline;
    static VkPipelineLayout s_PipelineLayout;

    static VkDescriptorSetLayout s_DescLayout;
    static VkDescriptorSet       s_DescSet;

    // Push Constants
    struct PostProcessPushConstants {
        f32 Exposure;
        f32 Gamma;
        f32 VignetteStr;
        f32 VignetteRad;
        i32 HasBloom;
        f32 _pad[3];
    };

    static u32 s_Width;
    static u32 s_Height;
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
