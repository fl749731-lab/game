#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>

namespace Engine {

// ── Vulkan IBL (基于图像的光照) ─────────────────────────────
// 预计算组件:
//   1. Irradiance Map (漫反射) — cubemap 卷积
//   2. Prefiltered Environment Map (镜面反射) — 不同粗糙度 mip
//   3. BRDF LUT — 2D 积分查找表
//
// 目前使用程序化环境 (后续可加载 HDR 环境贴图)

struct VulkanIBLConfig {
    u32  IrradianceSize     = 32;    // Irradiance cubemap 每个面的分辨率
    u32  PrefilteredSize    = 128;   // Prefiltered map 每个面的分辨率
    u32  PrefilteredMipLevels = 5;   // 粗糙度 mip 级别
    u32  BRDFLUTSize        = 512;   // BRDF LUT 分辨率
    f32  Intensity          = 1.0f;
    bool Enabled            = true;
};

class VulkanIBL {
public:
    static bool Init();
    static void Shutdown();

    /// 从 HDR 环境贴图生成 IBL 数据 (可选)
    static bool LoadEnvironmentMap(const std::string& hdrPath);

    /// 使用程序化天空生成 IBL 数据
    static bool GenerateFromSky(const glm::vec3& topColor,
                                const glm::vec3& horizonColor,
                                const glm::vec3& bottomColor);

    /// 绑定 IBL 纹理到 Descriptor Set
    static VkImageView GetIrradianceView()    { return s_IrradianceView; }
    static VkImageView GetPrefilteredView()   { return s_PrefilteredView; }
    static VkImageView GetBRDFLUTView()       { return s_BRDFLUTView; }
    static VkSampler   GetIBLSampler()        { return s_IBLSampler; }
    static VkSampler   GetBRDFSampler()       { return s_BRDFSampler; }

    static VulkanIBLConfig& GetConfig() { return s_Config; }
    static bool IsEnabled() { return s_Config.Enabled; }

private:
    static bool PrecomputeBRDFLUT();
    static void DestroyResources();

    static VulkanIBLConfig s_Config;

    // Irradiance Map (Cubemap, 32x32 * 6 面)
    static VkImage        s_IrradianceImage;
    static VkDeviceMemory s_IrradianceMemory;
    static VkImageView    s_IrradianceView;

    // Prefiltered Environment Map (Cubemap, 128x128 * 6 面, 多 mip)
    static VkImage        s_PrefilteredImage;
    static VkDeviceMemory s_PrefilteredMemory;
    static VkImageView    s_PrefilteredView;

    // BRDF LUT (2D, 512x512)
    static VkImage        s_BRDFLUTImage;
    static VkDeviceMemory s_BRDFLUTMemory;
    static VkImageView    s_BRDFLUTView;

    // 采样器
    static VkSampler s_IBLSampler;    // cubemap 采样 (trilinear)
    static VkSampler s_BRDFSampler;   // BRDF LUT 采样 (clamp)

    // Compute Pipeline (BRDF LUT 预计算)
    static VkPipeline       s_BRDFPipeline;
    static VkPipelineLayout s_BRDFPipelineLayout;
    static VkDescriptorSetLayout s_BRDFDescLayout;
    static VkDescriptorSet       s_BRDFDescSet;
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
