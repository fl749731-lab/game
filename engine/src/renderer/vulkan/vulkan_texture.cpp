#include "engine/renderer/vulkan/vulkan_texture.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/core/log.h"

#include <stb_image.h>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace Engine {

// ═══════════════════════════════════════════════════════════
//  VulkanImage
// ═══════════════════════════════════════════════════════════

u32 VulkanImage::CalculateMipLevels(u32 width, u32 height) {
    return (u32)std::floor(std::log2(std::max(width, height))) + 1;
}

void VulkanImage::CreateImage(u32 width, u32 height, VkFormat format,
                              VkImageTiling tiling, VkImageUsageFlags usage,
                              VkMemoryPropertyFlags memProps,
                              VkImage& image, VkDeviceMemory& memory,
                              u32 mipLevels) {
    VkDevice device = VulkanContext::GetDevice();

    VkImageCreateInfo info = {};
    info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType     = VK_IMAGE_TYPE_2D;
    info.extent.width  = width;
    info.extent.height = height;
    info.extent.depth  = 1;
    info.mipLevels     = mipLevels;
    info.arrayLayers   = 1;
    info.format        = format;
    info.tiling        = tiling;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.usage         = usage;
    info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    info.samples       = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device, &info, nullptr, &image) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] vkCreateImage 失败");
        return;
    }

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(device, image, &memReq);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = VulkanContext::FindMemoryType(memReq.memoryTypeBits, memProps);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] vkAllocateMemory (Image) 失败");
        return;
    }
    vkBindImageMemory(device, image, memory, 0);
}

VkImageView VulkanImage::CreateImageView(VkImage image, VkFormat format,
                                         VkImageAspectFlags aspectFlags,
                                         u32 mipLevels) {
    VkImageViewCreateInfo info = {};
    info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image    = image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format   = format;
    info.subresourceRange.aspectMask     = aspectFlags;
    info.subresourceRange.baseMipLevel   = 0;
    info.subresourceRange.levelCount     = mipLevels;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount     = 1;

    VkImageView view;
    if (vkCreateImageView(VulkanContext::GetDevice(), &info, nullptr, &view) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] vkCreateImageView 失败");
        return VK_NULL_HANDLE;
    }
    return view;
}

void VulkanImage::TransitionLayout(VkImage image, VkFormat format,
                                   VkImageLayout oldLayout, VkImageLayout newLayout,
                                   u32 mipLevels) {
    VkCommandBuffer cmd = VulkanCommand::BeginSingleTime();

    VkImageMemoryBarrier barrier = {};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;

    // 根据格式选择 aspect
    if (format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D24_UNORM_S8_UINT ||
        format == VK_FORMAT_D16_UNORM) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (format == VK_FORMAT_D24_UNORM_S8_UINT)
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    } else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    VkPipelineStageFlags srcStage, dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        // 通用 fallback — 仍记录警告
        LOG_WARN("[Vulkan] TransitionLayout: 未优化的 layout 转换 (%d → %d)，使用全流水线屏障",
                 (int)oldLayout, (int)newLayout);
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);

    VulkanCommand::EndSingleTime(cmd);
}

void VulkanImage::GenerateMipmaps(VkImage image, VkFormat format,
                                  u32 width, u32 height, u32 mipLevels) {
    // 检查 format 是否支持线性滤波 blit
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(VulkanContext::GetPhysicalDevice(),
                                        format, &formatProps);
    if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        LOG_WARN("[Vulkan] 纹理格式不支持线性 blit，跳过 mipmap 生成");
        return;
    }

    VkCommandBuffer cmd = VulkanCommand::BeginSingleTime();

    VkImageMemoryBarrier barrier = {};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image               = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.subresourceRange.levelCount     = 1;

    i32 mipW = (i32)width;
    i32 mipH = (i32)height;

    for (u32 i = 1; i < mipLevels; i++) {
        // 将上一级 mip 从 TRANSFER_DST 转为 TRANSFER_SRC
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr, 0, nullptr, 1, &barrier);

        // Blit 上一级 → 当前级
        VkImageBlit blit = {};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipW, mipH, 1};
        blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel       = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount     = 1;

        i32 nextW = (mipW > 1) ? mipW / 2 : 1;
        i32 nextH = (mipH > 1) ? mipH / 2 : 1;

        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {nextW, nextH, 1};
        blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel       = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount     = 1;

        vkCmdBlitImage(cmd,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit, VK_FILTER_LINEAR);

        // 将上一级转为 SHADER_READ
        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr, 0, nullptr, 1, &barrier);

        mipW = nextW;
        mipH = nextH;
    }

    // 将最后一级从 TRANSFER_DST 转为 SHADER_READ
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr, 0, nullptr, 1, &barrier);

    VulkanCommand::EndSingleTime(cmd);
}

void VulkanImage::CopyBufferToImage(VkBuffer buffer, VkImage image,
                                    u32 width, u32 height) {
    VkCommandBuffer cmd = VulkanCommand::BeginSingleTime();

    VkBufferImageCopy region = {};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(cmd, buffer, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VulkanCommand::EndSingleTime(cmd);
}

void VulkanImage::DestroyImage(VkImage image, VkDeviceMemory memory,
                               VkImageView view) {
    VkDevice device = VulkanContext::GetDevice();
    if (view != VK_NULL_HANDLE)  vkDestroyImageView(device, view, nullptr);
    if (image != VK_NULL_HANDLE) vkDestroyImage(device, image, nullptr);
    if (memory != VK_NULL_HANDLE) vkFreeMemory(device, memory, nullptr);
}

// ═══════════════════════════════════════════════════════════
//  VulkanSampler
// ═══════════════════════════════════════════════════════════

VkSampler VulkanSampler::Create(VulkanFilterMode filter, f32 maxAnisotropy, f32 maxLod) {
    VkFilter vkFilter = (filter == VulkanFilterMode::Nearest)
                        ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;

    VkSamplerCreateInfo info = {};
    info.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter    = vkFilter;
    info.minFilter    = vkFilter;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    info.anisotropyEnable = (maxAnisotropy > 1.0f) ? VK_TRUE : VK_FALSE;
    info.maxAnisotropy    = maxAnisotropy;

    info.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;
    info.compareEnable           = VK_FALSE;
    info.compareOp               = VK_COMPARE_OP_ALWAYS;
    info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.minLod                  = 0.0f;
    info.maxLod                  = maxLod;
    info.mipLodBias              = 0.0f;

    VkSampler sampler;
    if (vkCreateSampler(VulkanContext::GetDevice(), &info, nullptr, &sampler) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] vkCreateSampler 失败");
        return VK_NULL_HANDLE;
    }
    return sampler;
}

void VulkanSampler::Destroy(VkSampler sampler) {
    if (sampler != VK_NULL_HANDLE)
        vkDestroySampler(VulkanContext::GetDevice(), sampler, nullptr);
}

// ═══════════════════════════════════════════════════════════
//  VulkanTexture2D
// ═══════════════════════════════════════════════════════════

VulkanTexture2D::VulkanTexture2D(const std::string& filepath) {
    int w, h, ch;
    stbi_set_flip_vertically_on_load(false); // Vulkan Y 轴不反转
    stbi_uc* pixels = stbi_load(filepath.c_str(), &w, &h, &ch, STBI_rgb_alpha);
    if (!pixels) {
        LOG_ERROR("[Vulkan] 纹理加载失败: %s", filepath.c_str());
        return;
    }
    CreateFromPixels(pixels, (u32)w, (u32)h, 4);
    stbi_image_free(pixels);
    LOG_INFO("[Vulkan] 纹理已加载: %s (%dx%d, %u mips)", filepath.c_str(), w, h, m_MipLevels);
}

VulkanTexture2D::VulkanTexture2D(u32 width, u32 height, const void* pixels, u32 channels) {
    if (pixels) {
        CreateFromPixels(pixels, width, height, channels);
    }
}

VulkanTexture2D::~VulkanTexture2D() {
    VulkanSampler::Destroy(m_Sampler);
    VulkanImage::DestroyImage(m_Image, m_Memory, m_ImageView);
}

VkDescriptorImageInfo VulkanTexture2D::GetDescriptorInfo() const {
    VkDescriptorImageInfo info = {};
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.imageView   = m_ImageView;
    info.sampler     = m_Sampler;
    return info;
}

void VulkanTexture2D::CreateFromPixels(const void* pixels, u32 width, u32 height, u32 channels) {
    m_Width  = width;
    m_Height = height;
    m_MipLevels = VulkanImage::CalculateMipLevels(width, height);

    VkDeviceSize imageSize = (VkDeviceSize)width * height * 4; // 始终 RGBA

    // Staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VulkanBuffer::CreateBuffer(imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory);

    // 拷贝像素到 staging
    void* mapped;
    vkMapMemory(VulkanContext::GetDevice(), stagingMemory, 0, imageSize, 0, &mapped);
    if (channels == 4) {
        std::memcpy(mapped, pixels, imageSize);
    } else {
        // 将 RGB → RGBA
        const u8* src = (const u8*)pixels;
        u8* dst = (u8*)mapped;
        for (u32 i = 0; i < width * height; i++) {
            dst[i*4+0] = src[i*channels+0];
            dst[i*4+1] = (channels > 1) ? src[i*channels+1] : src[i*channels+0];
            dst[i*4+2] = (channels > 2) ? src[i*channels+2] : src[i*channels+0];
            dst[i*4+3] = 255;
        }
    }
    vkUnmapMemory(VulkanContext::GetDevice(), stagingMemory);

    // 创建带 mipmap 的 Image
    // usage 需要 TRANSFER_SRC (mipmap blit 源) + TRANSFER_DST (拷贝/blit 目标) + SAMPLED
    VulkanImage::CreateImage(width, height,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_Image, m_Memory, m_MipLevels);

    // 全部 mip 转为 TRANSFER_DST
    VulkanImage::TransitionLayout(m_Image, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_MipLevels);

    // 拷贝 staging → mip 0
    VulkanImage::CopyBufferToImage(stagingBuffer, m_Image, width, height);

    // 生成 mipmap (同时将所有 mip 转为 SHADER_READ_ONLY)
    VulkanImage::GenerateMipmaps(m_Image, VK_FORMAT_R8G8B8A8_SRGB,
                                 width, height, m_MipLevels);

    // 清理 staging
    VulkanBuffer::DestroyBuffer(stagingBuffer, stagingMemory);

    // ImageView (含所有 mip 级别) + Sampler (含 maxLod)
    m_ImageView = VulkanImage::CreateImageView(m_Image, VK_FORMAT_R8G8B8A8_SRGB,
                                               VK_IMAGE_ASPECT_COLOR_BIT, m_MipLevels);
    m_Sampler = VulkanSampler::Create(VulkanFilterMode::Linear, 16.0f, (f32)m_MipLevels);
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
