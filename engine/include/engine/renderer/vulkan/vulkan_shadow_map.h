#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_texture.h"
#include "engine/renderer/vulkan/vulkan_descriptor.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace Engine {

// ── Vulkan 阴影映射 ────────────────────────────────────────
// 方向光深度渲染 + PCF 柔软阴影
//
// 流程: 阴影 Pass → 深度纹理 → 光照 Pass 中采样

struct VulkanShadowConfig {
    u32 Resolution  = 2048;
    f32 OrthoSize   = 20.0f;
    f32 NearPlane   = 0.1f;
    f32 FarPlane    = 100.0f;
    f32 Bias        = 0.005f;
    i32 PCFSamples  = 2;        // PCF 采样半径
};

class VulkanShadowMap {
public:
    static bool Init(const VulkanShadowConfig& config = {});
    static void Shutdown();

    /// 开始阴影 Pass (深度渲染)
    static void BeginPass(VkCommandBuffer cmd);
    static void EndPass(VkCommandBuffer cmd);

    /// 设置光源方向 (计算 Light Space Matrix)
    static void SetLightDirection(const glm::vec3& direction);

    /// 获取 Light-View-Projection 矩阵
    static const glm::mat4& GetLightSpaceMatrix() { return s_LightSpaceMatrix; }

    /// 获取深度纹理 (用于光照 Pass PCF 采样)
    static VkImageView GetDepthView()   { return s_DepthView; }
    static VkSampler   GetShadowSampler() { return s_ShadowSampler; }

    /// DescriptorImageInfo (深度比较采样器)
    static VkDescriptorImageInfo GetDepthDescriptorInfo();

    static VkRenderPass GetRenderPass() { return s_RenderPass; }
    static const VulkanShadowConfig& GetConfig() { return s_Config; }

private:
    static bool CreateDepthResources();
    static bool CreateRenderPass();
    static bool CreateFramebuffer();
    static void DestroyResources();

    static VkImage        s_DepthImage;
    static VkDeviceMemory s_DepthMemory;
    static VkImageView    s_DepthView;
    static VkSampler      s_ShadowSampler;  // 深度比较采样器
    static VkRenderPass   s_RenderPass;
    static VkFramebuffer  s_Framebuffer;

    static glm::mat4 s_LightSpaceMatrix;
    static VulkanShadowConfig s_Config;
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
