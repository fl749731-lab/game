#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_shader.h"
#include "engine/renderer/vulkan/vulkan_descriptor.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace Engine {

// ── 延迟光照 Pass ───────────────────────────────────────────
// 全屏四边形采样 G-Buffer → 光照计算 → HDR 输出
//
// 支持:
//   - 方向光 (含阴影采样)
//   - 最多 16 个点光源
//   - 最多 8 个聚光灯

struct VulkanDirectionalLight {
    glm::vec3 Direction = {-0.3f, -1.0f, -0.3f};
    f32       _pad0     = 0.0f;
    glm::vec3 Color     = {1.0f, 1.0f, 0.95f};
    f32       Intensity = 1.0f;
};

struct VulkanPointLight {
    glm::vec3 Position  = {0.0f, 0.0f, 0.0f};
    f32       Radius    = 10.0f;
    glm::vec3 Color     = {1.0f, 1.0f, 1.0f};
    f32       Intensity = 1.0f;
};

struct VulkanSpotLight {
    glm::vec3 Position  = {0.0f, 0.0f, 0.0f};
    f32       InnerCone = 0.9f;   // cos(angle)
    glm::vec3 Direction = {0.0f, -1.0f, 0.0f};
    f32       OuterCone = 0.8f;   // cos(angle)
    glm::vec3 Color     = {1.0f, 1.0f, 1.0f};
    f32       Intensity = 1.0f;
};

// 光照 UBO (对齐到 GPU)
struct VulkanLightingUBO {
    glm::vec3 CameraPos;
    f32       AmbientIntensity = 0.03f;
    glm::vec3 AmbientColor;
    u32       DirLightCount    = 0;
    u32       PointLightCount  = 0;
    u32       SpotLightCount   = 0;
    f32       _pad[2];
    VulkanDirectionalLight DirLights[4];
    VulkanPointLight       PointLights[16];
    VulkanSpotLight        SpotLights[8];
};

class VulkanLightingPass {
public:
    /// 初始化 (创建 RenderPass, Pipeline, Descriptor Layout)
    static bool Init(u32 width, u32 height);
    static void Shutdown();
    static void Resize(u32 width, u32 height);

    /// 执行光照计算
    /// @param cmd        当前 CommandBuffer
    /// @param lightData  光照 UBO 数据
    static void Execute(VkCommandBuffer cmd, const VulkanLightingUBO& lightData);

    /// 获取 HDR 颜色附件 (用于后处理)
    static VkImageView GetHDRColorView()   { return s_HDRColorView; }
    static VkSampler   GetHDRSampler()     { return s_HDRSampler; }

    /// 获取 HDR 颜色的 DescriptorImageInfo
    static VkDescriptorImageInfo GetHDRDescriptorInfo();

    static VkRenderPass GetRenderPass() { return s_RenderPass; }

private:
    static bool CreateRenderPass();
    static bool CreateHDRTarget(u32 width, u32 height);
    static void DestroyHDRTarget();

    // HDR 输出 framebuffer (R16G16B16A16_SFLOAT)
    static VkImage        s_HDRColorImage;
    static VkDeviceMemory s_HDRColorMemory;
    static VkImageView    s_HDRColorView;
    static VkSampler      s_HDRSampler;
    static VkFramebuffer  s_HDRFramebuffer;
    static VkRenderPass   s_RenderPass;

    // 光照 UBO
    static VkBuffer       s_LightUBO;
    static VkDeviceMemory s_LightUBOMemory;
    static void*          s_LightUBOMapped;

    // Descriptor Set (G-Buffer 输入 + Light UBO)
    static VkDescriptorSetLayout s_DescLayout;
    static VkDescriptorSet       s_DescSet;

    // Pipeline
    static VkPipeline       s_Pipeline;
    static VkPipelineLayout s_PipelineLayout;

    static u32 s_Width;
    static u32 s_Height;
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
