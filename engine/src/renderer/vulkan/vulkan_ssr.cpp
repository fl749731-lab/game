#include "engine/renderer/vulkan/vulkan_ssr.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/renderer/vulkan/vulkan_texture.h"
#include "engine/renderer/vulkan/vulkan_screen_quad.h"
#include "engine/renderer/vulkan/vulkan_descriptor.h"
#include "engine/core/log.h"

#include <glm/glm.hpp>
#include <cstring>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

VulkanSSRConfig VulkanSSR::s_Config = {};

VkImage        VulkanSSR::s_ReflectionImage   = VK_NULL_HANDLE;
VkDeviceMemory VulkanSSR::s_ReflectionMemory  = VK_NULL_HANDLE;
VkImageView    VulkanSSR::s_ReflectionView    = VK_NULL_HANDLE;
VkSampler      VulkanSSR::s_ReflectionSampler = VK_NULL_HANDLE;
VkFramebuffer  VulkanSSR::s_ReflectionFBO     = VK_NULL_HANDLE;

VkRenderPass     VulkanSSR::s_RenderPass     = VK_NULL_HANDLE;
VkPipeline       VulkanSSR::s_Pipeline       = VK_NULL_HANDLE;
VkPipelineLayout VulkanSSR::s_PipelineLayout = VK_NULL_HANDLE;

VkDescriptorSetLayout VulkanSSR::s_DescLayout = VK_NULL_HANDLE;
VkDescriptorSet       VulkanSSR::s_DescSet    = VK_NULL_HANDLE;

VkBuffer       VulkanSSR::s_ConfigUBO       = VK_NULL_HANDLE;
VkDeviceMemory VulkanSSR::s_ConfigUBOMemory = VK_NULL_HANDLE;
void*          VulkanSSR::s_ConfigUBOMapped = nullptr;

u32 VulkanSSR::s_Width  = 0;
u32 VulkanSSR::s_Height = 0;

// ── 初始化 ──────────────────────────────────────────────────

bool VulkanSSR::Init(u32 width, u32 height) {
    s_Width  = width / 2;  // 半分辨率
    s_Height = height / 2;

    if (!CreateResources(s_Width, s_Height)) return false;

    // 创建 Config UBO
    VkDeviceSize uboSize = sizeof(VulkanSSRConfig);
    VulkanBuffer::CreateBuffer(uboSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        s_ConfigUBO, s_ConfigUBOMemory);
    vkMapMemory(VulkanContext::GetDevice(), s_ConfigUBOMemory, 0, uboSize, 0, &s_ConfigUBOMapped);

    // 采样器
    s_ReflectionSampler = VulkanSampler::Create(VulkanFilterMode::Linear, 1.0f, 0.0f);

    LOG_INFO("[Vulkan] SSR 初始化完成 (%ux%u, 半分辨率)", s_Width, s_Height);
    return true;
}

void VulkanSSR::Shutdown() {
    VkDevice device = VulkanContext::GetDevice();

    DestroyResources();

    if (s_ConfigUBOMapped) {
        vkUnmapMemory(device, s_ConfigUBOMemory);
        s_ConfigUBOMapped = nullptr;
    }
    VulkanBuffer::DestroyBuffer(s_ConfigUBO, s_ConfigUBOMemory);

    VulkanSampler::Destroy(s_ReflectionSampler);
    s_ReflectionSampler = VK_NULL_HANDLE;

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

void VulkanSSR::Resize(u32 width, u32 height) {
    if (width == 0 || height == 0) return;

    vkDeviceWaitIdle(VulkanContext::GetDevice());
    DestroyResources();

    s_Width  = width / 2;
    s_Height = height / 2;
    CreateResources(s_Width, s_Height);
}

void VulkanSSR::Execute(VkCommandBuffer cmd,
                        const glm::mat4& view, const glm::mat4& proj) {
    if (!s_Config.Enabled || !s_Pipeline) return;

    // 更新 Config UBO
    if (s_ConfigUBOMapped) {
        std::memcpy(s_ConfigUBOMapped, &s_Config, sizeof(VulkanSSRConfig));
    }

    // TODO: SSR Pass 的执行逻辑 — 射线步进
    // 将在 shader 编译系统完善后实现
}

VkDescriptorImageInfo VulkanSSR::GetReflectionDescriptorInfo() {
    VkDescriptorImageInfo info = {};
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.imageView   = s_ReflectionView;
    info.sampler     = s_ReflectionSampler;
    return info;
}

// ── 内部资源 ────────────────────────────────────────────────

bool VulkanSSR::CreateResources(u32 width, u32 height) {
    VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

    VulkanImage::CreateImage(width, height, format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        s_ReflectionImage, s_ReflectionMemory);

    s_ReflectionView = VulkanImage::CreateImageView(
        s_ReflectionImage, format, VK_IMAGE_ASPECT_COLOR_BIT);

    return s_ReflectionView != VK_NULL_HANDLE;
}

void VulkanSSR::DestroyResources() {
    VkDevice device = VulkanContext::GetDevice();

    if (s_ReflectionFBO) {
        vkDestroyFramebuffer(device, s_ReflectionFBO, nullptr);
        s_ReflectionFBO = VK_NULL_HANDLE;
    }
    VulkanImage::DestroyImage(s_ReflectionImage, s_ReflectionMemory, s_ReflectionView);
    s_ReflectionImage  = VK_NULL_HANDLE;
    s_ReflectionMemory = VK_NULL_HANDLE;
    s_ReflectionView   = VK_NULL_HANDLE;
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
