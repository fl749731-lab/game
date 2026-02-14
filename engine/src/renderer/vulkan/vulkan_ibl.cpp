#include "engine/renderer/vulkan/vulkan_ibl.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/renderer/vulkan/vulkan_texture.h"
#include "engine/renderer/vulkan/vulkan_descriptor.h"
#include "engine/core/log.h"

#include <cstring>
#include <cmath>
#include <vector>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

VulkanIBLConfig VulkanIBL::s_Config = {};

VkImage        VulkanIBL::s_IrradianceImage   = VK_NULL_HANDLE;
VkDeviceMemory VulkanIBL::s_IrradianceMemory  = VK_NULL_HANDLE;
VkImageView    VulkanIBL::s_IrradianceView    = VK_NULL_HANDLE;

VkImage        VulkanIBL::s_PrefilteredImage   = VK_NULL_HANDLE;
VkDeviceMemory VulkanIBL::s_PrefilteredMemory  = VK_NULL_HANDLE;
VkImageView    VulkanIBL::s_PrefilteredView    = VK_NULL_HANDLE;

VkImage        VulkanIBL::s_BRDFLUTImage   = VK_NULL_HANDLE;
VkDeviceMemory VulkanIBL::s_BRDFLUTMemory  = VK_NULL_HANDLE;
VkImageView    VulkanIBL::s_BRDFLUTView    = VK_NULL_HANDLE;

VkSampler VulkanIBL::s_IBLSampler  = VK_NULL_HANDLE;
VkSampler VulkanIBL::s_BRDFSampler = VK_NULL_HANDLE;

VkPipeline             VulkanIBL::s_BRDFPipeline       = VK_NULL_HANDLE;
VkPipelineLayout       VulkanIBL::s_BRDFPipelineLayout = VK_NULL_HANDLE;
VkDescriptorSetLayout  VulkanIBL::s_BRDFDescLayout     = VK_NULL_HANDLE;
VkDescriptorSet        VulkanIBL::s_BRDFDescSet        = VK_NULL_HANDLE;

// ── 初始化 ──────────────────────────────────────────────────

bool VulkanIBL::Init() {
    // 创建 IBL 采样器 (cubemap, trilinear, clamp)
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter    = VK_FILTER_LINEAR;
    samplerInfo.minFilter    = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.maxLod       = (f32)s_Config.PrefilteredMipLevels;
    samplerInfo.anisotropyEnable = VK_FALSE;

    if (vkCreateSampler(VulkanContext::GetDevice(), &samplerInfo, nullptr, &s_IBLSampler) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] IBL 采样器创建失败");
        return false;
    }

    // BRDF LUT 采样器 (2D, clamp, no mip)
    s_BRDFSampler = VulkanSampler::Create(VulkanFilterMode::Linear, 1.0f, 0.0f);

    // 预计算 BRDF LUT
    PrecomputeBRDFLUT();

    LOG_INFO("[Vulkan] IBL 初始化完成");
    return true;
}

void VulkanIBL::Shutdown() {
    DestroyResources();
    VkDevice device = VulkanContext::GetDevice();

    if (s_IBLSampler) {
        vkDestroySampler(device, s_IBLSampler, nullptr);
        s_IBLSampler = VK_NULL_HANDLE;
    }
    VulkanSampler::Destroy(s_BRDFSampler);
    s_BRDFSampler = VK_NULL_HANDLE;

    if (s_BRDFPipeline) {
        vkDestroyPipeline(device, s_BRDFPipeline, nullptr);
        s_BRDFPipeline = VK_NULL_HANDLE;
    }
    if (s_BRDFPipelineLayout) {
        vkDestroyPipelineLayout(device, s_BRDFPipelineLayout, nullptr);
        s_BRDFPipelineLayout = VK_NULL_HANDLE;
    }
    if (s_BRDFDescLayout) {
        vkDestroyDescriptorSetLayout(device, s_BRDFDescLayout, nullptr);
        s_BRDFDescLayout = VK_NULL_HANDLE;
    }
}

bool VulkanIBL::LoadEnvironmentMap(const std::string& hdrPath) {
    // TODO: 加载 HDR 环境贴图 → equirectangular → cubemap → 卷积
    // 将在 compute shader 编译后实现
    LOG_WARN("[Vulkan] IBL::LoadEnvironmentMap 未实现 — 请使用 GenerateFromSky()");
    return false;
}

bool VulkanIBL::GenerateFromSky(const glm::vec3& topColor,
                                const glm::vec3& horizonColor,
                                const glm::vec3& bottomColor) {
    // TODO: 使用程序化天空颜色生成 cubemap → irradiance + prefiltered
    // 将在 compute shader 框架完善后实现
    LOG_INFO("[Vulkan] IBL::GenerateFromSky 已标记待实现");
    return true;
}

// ── BRDF LUT 预计算 ────────────────────────────────────────

bool VulkanIBL::PrecomputeBRDFLUT() {
    u32 size = s_Config.BRDFLUTSize;

    // 创建 BRDF LUT 纹理 (R16G16_SFLOAT)
    VkFormat format = VK_FORMAT_R16G16_SFLOAT;

    VulkanImage::CreateImage(size, size, format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        s_BRDFLUTImage, s_BRDFLUTMemory);

    s_BRDFLUTView = VulkanImage::CreateImageView(
        s_BRDFLUTImage, format, VK_IMAGE_ASPECT_COLOR_BIT);

    // 转换 layout 为 SHADER_READ_ONLY (compute shader 完善后改为 GENERAL → 计算 → READ_ONLY)
    VulkanImage::TransitionLayout(s_BRDFLUTImage, format,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    LOG_INFO("[Vulkan] BRDF LUT 已创建 (%ux%u)", size, size);
    return s_BRDFLUTView != VK_NULL_HANDLE;
}

void VulkanIBL::DestroyResources() {
    VulkanImage::DestroyImage(s_IrradianceImage, s_IrradianceMemory, s_IrradianceView);
    s_IrradianceImage  = VK_NULL_HANDLE;
    s_IrradianceMemory = VK_NULL_HANDLE;
    s_IrradianceView   = VK_NULL_HANDLE;

    VulkanImage::DestroyImage(s_PrefilteredImage, s_PrefilteredMemory, s_PrefilteredView);
    s_PrefilteredImage  = VK_NULL_HANDLE;
    s_PrefilteredMemory = VK_NULL_HANDLE;
    s_PrefilteredView   = VK_NULL_HANDLE;

    VulkanImage::DestroyImage(s_BRDFLUTImage, s_BRDFLUTMemory, s_BRDFLUTView);
    s_BRDFLUTImage  = VK_NULL_HANDLE;
    s_BRDFLUTMemory = VK_NULL_HANDLE;
    s_BRDFLUTView   = VK_NULL_HANDLE;
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
