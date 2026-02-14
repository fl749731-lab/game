#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace Engine {

// ── Vulkan SSR (屏幕空间反射) ───────────────────────────────
// 输入: G-Buffer 位置 + 法线 + HDR 颜色
// 输出: 反射颜色纹理

struct VulkanSSRConfig {
    f32  MaxDistance  = 50.0f;    // 最大射线步进距离
    f32  StepSize    = 0.1f;     // 射线步进大小
    u32  MaxSteps    = 100;      // 最大步数
    f32  Thickness   = 0.5f;     // 相交判定厚度
    f32  Intensity   = 0.5f;     // 反射强度
    bool Enabled     = false;    // 默认关闭 (性能较重)
};

class VulkanSSR {
public:
    static bool Init(u32 width, u32 height);
    static void Shutdown();
    static void Resize(u32 width, u32 height);

    /// 执行 SSR Pass
    /// @param cmd   当前 CommandBuffer
    /// @param view  View 矩阵
    /// @param proj  Projection 矩阵
    static void Execute(VkCommandBuffer cmd,
                        const glm::mat4& view, const glm::mat4& proj);

    /// 获取反射纹理
    static VkImageView GetReflectionView()    { return s_ReflectionView; }
    static VkSampler   GetReflectionSampler() { return s_ReflectionSampler; }

    static VkDescriptorImageInfo GetReflectionDescriptorInfo();

    static VulkanSSRConfig& GetConfig() { return s_Config; }
    static bool IsEnabled() { return s_Config.Enabled; }

private:
    static bool CreateResources(u32 width, u32 height);
    static void DestroyResources();

    static VulkanSSRConfig s_Config;

    // 反射输出 (半分辨率)
    static VkImage        s_ReflectionImage;
    static VkDeviceMemory s_ReflectionMemory;
    static VkImageView    s_ReflectionView;
    static VkSampler      s_ReflectionSampler;
    static VkFramebuffer  s_ReflectionFBO;

    // RenderPass + Pipeline
    static VkRenderPass   s_RenderPass;
    static VkPipeline     s_Pipeline;
    static VkPipelineLayout s_PipelineLayout;

    // Descriptors
    static VkDescriptorSetLayout s_DescLayout;
    static VkDescriptorSet       s_DescSet;

    // UBO
    static VkBuffer       s_ConfigUBO;
    static VkDeviceMemory s_ConfigUBOMemory;
    static void*          s_ConfigUBOMapped;

    static u32 s_Width;
    static u32 s_Height;
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
