#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include <vulkan/vulkan.h>

namespace Engine {

// ── Vulkan Bloom ─────────────────────────────────────────
// 多步骤:
//   1. 亮度提取 (Threshold)
//   2. 高斯模糊 (PingPong 双 Pass)
//   3. 合成 (在后处理 Pass 中叠加)

struct VulkanBloomConfig {
    f32  Threshold  = 1.0f;
    f32  Intensity  = 0.5f;
    u32  Iterations = 10;    // 模糊迭代次数
    bool Enabled    = true;
};

class VulkanBloom {
public:
    static bool Init(u32 width, u32 height);
    static void Shutdown();
    static void Resize(u32 width, u32 height);

    /// 执行 Bloom 效果
    /// @param cmd       当前 CommandBuffer
    /// @param hdrView   HDR 颜色附件 (光照 Pass 输出)
    /// @param hdrSampler HDR 采样器
    static void Execute(VkCommandBuffer cmd, VkImageView hdrView, VkSampler hdrSampler);

    /// 获取最终 Bloom 纹理
    static VkImageView GetBloomView()      { return s_BloomViews[0]; }
    static VkSampler   GetBloomSampler()   { return s_BloomSampler; }

    static VulkanBloomConfig& GetConfig()  { return s_Config; }
    static bool IsEnabled()                { return s_Config.Enabled; }

private:
    static bool CreatePingPongTargets(u32 width, u32 height);
    static bool CreateRenderPasses();
    static void DestroyResources();

    static VulkanBloomConfig s_Config;

    // 2 个 Ping-Pong FBO (半分辨率)
    static VkImage        s_BloomImages[2];
    static VkDeviceMemory s_BloomMemories[2];
    static VkImageView    s_BloomViews[2];
    static VkFramebuffer  s_BloomFBOs[2];
    static VkSampler      s_BloomSampler;

    // Render Pass (亮度提取 + 模糊共用)
    static VkRenderPass   s_RenderPass;

    // Pipeline: 亮度提取
    static VkPipeline       s_ExtractPipeline;
    static VkPipelineLayout s_ExtractLayout;

    // Pipeline: 高斯模糊
    static VkPipeline       s_BlurPipeline;
    static VkPipelineLayout s_BlurLayout;

    // Descriptor Layout
    static VkDescriptorSetLayout s_DescLayout;
    static VkDescriptorSet       s_ExtractDescSet;
    static VkDescriptorSet       s_BlurDescSets[2];

    static u32 s_Width;
    static u32 s_Height;
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
