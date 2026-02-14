#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>

namespace Engine {

// ── Vulkan SSAO (屏幕空间环境光遮蔽) ────────────────────────
// 输入: G-Buffer 位置 + 法线 + 噪声纹理
// 输出: 遮蔽因子纹理 (R8, 0=全遮蔽, 1=无遮蔽)

struct VulkanSSAOConfig {
    f32  Radius    = 0.5f;
    f32  Bias      = 0.025f;
    f32  Intensity = 1.0f;
    u32  KernelSize = 64;
    bool Enabled   = true;
};

class VulkanSSAO {
public:
    static bool Init(u32 width, u32 height);
    static void Shutdown();
    static void Resize(u32 width, u32 height);

    /// 执行 SSAO Pass
    /// @param cmd   当前 CommandBuffer
    /// @param proj  投影矩阵 (用于重建位置)
    static void Execute(VkCommandBuffer cmd, const glm::mat4& proj);

    /// 获取遮蔽结果纹理
    static VkImageView GetOcclusionView()   { return s_OcclusionView; }
    static VkSampler   GetOcclusionSampler() { return s_OcclusionSampler; }

    /// DescriptorImageInfo (用于光照 Pass)
    static VkDescriptorImageInfo GetOcclusionDescriptorInfo();

    static VulkanSSAOConfig& GetConfig() { return s_Config; }
    static bool IsEnabled() { return s_Config.Enabled; }

private:
    static bool CreateResources(u32 width, u32 height);
    static bool CreateNoiseTexture();
    static void GenerateKernel();
    static void DestroyResources();

    static VulkanSSAOConfig s_Config;

    // SSAO 输出
    static VkImage        s_OcclusionImage;
    static VkDeviceMemory s_OcclusionMemory;
    static VkImageView    s_OcclusionView;
    static VkSampler      s_OcclusionSampler;
    static VkFramebuffer  s_OcclusionFBO;

    // 模糊后输出
    static VkImage        s_BlurImage;
    static VkDeviceMemory s_BlurMemory;
    static VkImageView    s_BlurView;
    static VkFramebuffer  s_BlurFBO;

    // 噪声纹理 (4x4)
    static VkImage        s_NoiseImage;
    static VkDeviceMemory s_NoiseMemory;
    static VkImageView    s_NoiseView;
    static VkSampler      s_NoiseSampler;

    // 采样核心
    static std::vector<glm::vec4> s_Kernel;

    // RenderPass + Pipeline
    static VkRenderPass   s_RenderPass;
    static VkPipeline     s_SSAOPipeline;
    static VkPipelineLayout s_SSAOLayout;
    static VkPipeline     s_BlurPipeline;
    static VkPipelineLayout s_BlurLayout;

    // Descriptors
    static VkDescriptorSetLayout s_DescLayout;
    static VkDescriptorSet       s_SSAODescSet;
    static VkDescriptorSet       s_BlurDescSet;

    // 核心 UBO
    static VkBuffer       s_KernelUBO;
    static VkDeviceMemory s_KernelUBOMemory;
    static void*          s_KernelUBOMapped;

    static u32 s_Width;
    static u32 s_Height;
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
