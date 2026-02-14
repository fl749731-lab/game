#include "engine/renderer/vulkan/vulkan_ssao.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/renderer/vulkan/vulkan_texture.h"
#include "engine/renderer/vulkan/vulkan_g_buffer.h"
#include "engine/renderer/vulkan/vulkan_screen_quad.h"
#include "engine/renderer/vulkan/vulkan_descriptor.h"
#include "engine/core/log.h"

#include <glm/gtc/matrix_transform.hpp>
#include <random>
#include <cstring>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

VulkanSSAOConfig VulkanSSAO::s_Config = {};

VkImage        VulkanSSAO::s_OcclusionImage   = VK_NULL_HANDLE;
VkDeviceMemory VulkanSSAO::s_OcclusionMemory  = VK_NULL_HANDLE;
VkImageView    VulkanSSAO::s_OcclusionView    = VK_NULL_HANDLE;
VkSampler      VulkanSSAO::s_OcclusionSampler = VK_NULL_HANDLE;
VkFramebuffer  VulkanSSAO::s_OcclusionFBO     = VK_NULL_HANDLE;

VkImage        VulkanSSAO::s_BlurImage   = VK_NULL_HANDLE;
VkDeviceMemory VulkanSSAO::s_BlurMemory  = VK_NULL_HANDLE;
VkImageView    VulkanSSAO::s_BlurView    = VK_NULL_HANDLE;
VkFramebuffer  VulkanSSAO::s_BlurFBO     = VK_NULL_HANDLE;

VkImage        VulkanSSAO::s_NoiseImage   = VK_NULL_HANDLE;
VkDeviceMemory VulkanSSAO::s_NoiseMemory  = VK_NULL_HANDLE;
VkImageView    VulkanSSAO::s_NoiseView    = VK_NULL_HANDLE;
VkSampler      VulkanSSAO::s_NoiseSampler = VK_NULL_HANDLE;

std::vector<glm::vec4> VulkanSSAO::s_Kernel;

VkRenderPass     VulkanSSAO::s_RenderPass    = VK_NULL_HANDLE;
VkPipeline       VulkanSSAO::s_SSAOPipeline  = VK_NULL_HANDLE;
VkPipelineLayout VulkanSSAO::s_SSAOLayout    = VK_NULL_HANDLE;
VkPipeline       VulkanSSAO::s_BlurPipeline  = VK_NULL_HANDLE;
VkPipelineLayout VulkanSSAO::s_BlurLayout    = VK_NULL_HANDLE;

VkDescriptorSetLayout VulkanSSAO::s_DescLayout   = VK_NULL_HANDLE;
VkDescriptorSet       VulkanSSAO::s_SSAODescSet  = VK_NULL_HANDLE;
VkDescriptorSet       VulkanSSAO::s_BlurDescSet  = VK_NULL_HANDLE;

VkBuffer       VulkanSSAO::s_KernelUBO       = VK_NULL_HANDLE;
VkDeviceMemory VulkanSSAO::s_KernelUBOMemory = VK_NULL_HANDLE;
void*          VulkanSSAO::s_KernelUBOMapped = nullptr;

u32 VulkanSSAO::s_Width  = 0;
u32 VulkanSSAO::s_Height = 0;

// ── 初始化 ──────────────────────────────────────────────────

bool VulkanSSAO::Init(u32 width, u32 height) {
    s_Width  = width;
    s_Height = height;

    GenerateKernel();
    if (!CreateNoiseTexture()) return false;
    if (!CreateResources(width, height)) return false;

    // 创建 Kernel UBO (持久映射)
    VkDeviceSize uboSize = sizeof(glm::vec4) * s_Config.KernelSize;
    VulkanBuffer::CreateBuffer(uboSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        s_KernelUBO, s_KernelUBOMemory);
    vkMapMemory(VulkanContext::GetDevice(), s_KernelUBOMemory, 0, uboSize, 0, &s_KernelUBOMapped);
    std::memcpy(s_KernelUBOMapped, s_Kernel.data(), uboSize);

    // 采样器
    s_OcclusionSampler = VulkanSampler::Create(VulkanFilterMode::Nearest, 1.0f, 0.0f);

    LOG_INFO("[Vulkan] SSAO 初始化完成 (%ux%u, %u 核心采样)",
             width, height, s_Config.KernelSize);
    return true;
}

void VulkanSSAO::Shutdown() {
    VkDevice device = VulkanContext::GetDevice();

    DestroyResources();

    if (s_KernelUBOMapped) {
        vkUnmapMemory(device, s_KernelUBOMemory);
        s_KernelUBOMapped = nullptr;
    }
    VulkanBuffer::DestroyBuffer(s_KernelUBO, s_KernelUBOMemory);

    VulkanSampler::Destroy(s_OcclusionSampler);
    s_OcclusionSampler = VK_NULL_HANDLE;

    VulkanImage::DestroyImage(s_NoiseImage, s_NoiseMemory, s_NoiseView);
    s_NoiseImage  = VK_NULL_HANDLE;
    s_NoiseMemory = VK_NULL_HANDLE;
    s_NoiseView   = VK_NULL_HANDLE;
    VulkanSampler::Destroy(s_NoiseSampler);
    s_NoiseSampler = VK_NULL_HANDLE;

    if (s_SSAOPipeline) {
        vkDestroyPipeline(device, s_SSAOPipeline, nullptr);
        s_SSAOPipeline = VK_NULL_HANDLE;
    }
    if (s_SSAOLayout) {
        vkDestroyPipelineLayout(device, s_SSAOLayout, nullptr);
        s_SSAOLayout = VK_NULL_HANDLE;
    }
    if (s_BlurPipeline) {
        vkDestroyPipeline(device, s_BlurPipeline, nullptr);
        s_BlurPipeline = VK_NULL_HANDLE;
    }
    if (s_BlurLayout) {
        vkDestroyPipelineLayout(device, s_BlurLayout, nullptr);
        s_BlurLayout = VK_NULL_HANDLE;
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

void VulkanSSAO::Resize(u32 width, u32 height) {
    if (width == 0 || height == 0) return;

    vkDeviceWaitIdle(VulkanContext::GetDevice());
    DestroyResources();

    s_Width  = width;
    s_Height = height;
    CreateResources(width, height);
}

void VulkanSSAO::Execute(VkCommandBuffer cmd, const glm::mat4& proj) {
    if (!s_Config.Enabled || !s_SSAOPipeline) return;

    // TODO: SSAO Pass + 模糊 Pass 的执行逻辑
    // 将在 shader 编译系统完善后实现
}

VkDescriptorImageInfo VulkanSSAO::GetOcclusionDescriptorInfo() {
    VkDescriptorImageInfo info = {};
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    // 使用模糊后的结果
    info.imageView   = (s_BlurView != VK_NULL_HANDLE) ? s_BlurView : s_OcclusionView;
    info.sampler     = s_OcclusionSampler;
    return info;
}

// ── 内部: 采样核心生成 ──────────────────────────────────────

void VulkanSSAO::GenerateKernel() {
    std::default_random_engine gen;
    std::uniform_real_distribution<f32> rng(0.0f, 1.0f);

    s_Kernel.resize(s_Config.KernelSize);
    for (u32 i = 0; i < s_Config.KernelSize; i++) {
        // 半球采样
        glm::vec3 sample(
            rng(gen) * 2.0f - 1.0f,
            rng(gen) * 2.0f - 1.0f,
            rng(gen)  // 仅正方向 (法线方向)
        );
        sample = glm::normalize(sample);
        sample *= rng(gen);

        // 加速衰减 (更多采样点靠近中心)
        f32 scale = (f32)i / (f32)s_Config.KernelSize;
        scale = 0.1f + scale * scale * 0.9f; // lerp(0.1, 1.0, scale^2)
        sample *= scale;

        s_Kernel[i] = glm::vec4(sample, 0.0f);
    }
}

// ── 内部: 噪声纹理 (4x4) ───────────────────────────────────

bool VulkanSSAO::CreateNoiseTexture() {
    std::default_random_engine gen;
    std::uniform_real_distribution<f32> rng(0.0f, 1.0f);

    // 4x4 噪声 (随机旋转向量)
    std::vector<glm::vec4> noise(16);
    for (u32 i = 0; i < 16; i++) {
        noise[i] = glm::vec4(
            rng(gen) * 2.0f - 1.0f,
            rng(gen) * 2.0f - 1.0f,
            0.0f, 0.0f
        );
    }

    // 创建 4x4 R32G32B32A32_SFLOAT 纹理
    VulkanImage::CreateImage(4, 4, VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        s_NoiseImage, s_NoiseMemory);

    // 通过 staging buffer 上传
    VkDeviceSize dataSize = sizeof(glm::vec4) * 16;
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VulkanBuffer::CreateBuffer(dataSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory);

    void* mapped;
    vkMapMemory(VulkanContext::GetDevice(), stagingMemory, 0, dataSize, 0, &mapped);
    std::memcpy(mapped, noise.data(), dataSize);
    vkUnmapMemory(VulkanContext::GetDevice(), stagingMemory);

    VulkanImage::TransitionLayout(s_NoiseImage, VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VulkanImage::CopyBufferToImage(stagingBuffer, s_NoiseImage, 4, 4);
    VulkanImage::TransitionLayout(s_NoiseImage, VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VulkanBuffer::DestroyBuffer(stagingBuffer, stagingMemory);

    s_NoiseView = VulkanImage::CreateImageView(
        s_NoiseImage, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);

    // 噪声采样器 (Repeat 寻址)
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter    = VK_FILTER_NEAREST;
    samplerInfo.minFilter    = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    if (vkCreateSampler(VulkanContext::GetDevice(), &samplerInfo, nullptr, &s_NoiseSampler) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] SSAO 噪声采样器创建失败");
        return false;
    }

    return s_NoiseView != VK_NULL_HANDLE;
}

// ── 内部: 创建资源 ──────────────────────────────────────────

bool VulkanSSAO::CreateResources(u32 width, u32 height) {
    VkFormat format = VK_FORMAT_R8_UNORM;

    // SSAO 输出
    VulkanImage::CreateImage(width, height, format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        s_OcclusionImage, s_OcclusionMemory);
    s_OcclusionView = VulkanImage::CreateImageView(
        s_OcclusionImage, format, VK_IMAGE_ASPECT_COLOR_BIT);

    // 模糊输出
    VulkanImage::CreateImage(width, height, format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        s_BlurImage, s_BlurMemory);
    s_BlurView = VulkanImage::CreateImageView(
        s_BlurImage, format, VK_IMAGE_ASPECT_COLOR_BIT);

    return s_OcclusionView != VK_NULL_HANDLE && s_BlurView != VK_NULL_HANDLE;
}

void VulkanSSAO::DestroyResources() {
    VkDevice device = VulkanContext::GetDevice();

    if (s_OcclusionFBO) {
        vkDestroyFramebuffer(device, s_OcclusionFBO, nullptr);
        s_OcclusionFBO = VK_NULL_HANDLE;
    }
    if (s_BlurFBO) {
        vkDestroyFramebuffer(device, s_BlurFBO, nullptr);
        s_BlurFBO = VK_NULL_HANDLE;
    }

    VulkanImage::DestroyImage(s_OcclusionImage, s_OcclusionMemory, s_OcclusionView);
    s_OcclusionImage  = VK_NULL_HANDLE;
    s_OcclusionMemory = VK_NULL_HANDLE;
    s_OcclusionView   = VK_NULL_HANDLE;

    VulkanImage::DestroyImage(s_BlurImage, s_BlurMemory, s_BlurView);
    s_BlurImage  = VK_NULL_HANDLE;
    s_BlurMemory = VK_NULL_HANDLE;
    s_BlurView   = VK_NULL_HANDLE;
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
