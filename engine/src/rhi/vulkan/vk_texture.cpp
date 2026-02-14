#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/rhi/vulkan/vk_texture.h"
#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/renderer/vulkan/vulkan_texture.h"
#include "engine/core/log.h"

#include <stb_image.h>
#include <cstring>

namespace Engine {

// ── 从文件加载 ──────────────────────────────────────────────

VKTexture2D::VKTexture2D(const std::string& filepath) {
    int w, h, channels;
    stbi_set_flip_vertically_on_load(false); // Vulkan Y 轴向下
    unsigned char* pixels = stbi_load(filepath.c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels) {
        LOG_ERROR("[VKTexture2D] 加载失败: %s", filepath.c_str());
        return;
    }

    m_Width  = (u32)w;
    m_Height = (u32)h;
    CreateFromPixels(pixels, m_Width, m_Height, 4);
    stbi_image_free(pixels);

    LOG_DEBUG("[VKTexture2D] 从文件加载: %s (%ux%u)", filepath.c_str(), m_Width, m_Height);
}

// ── 从像素数据创建 ──────────────────────────────────────────

VKTexture2D::VKTexture2D(u32 width, u32 height, const void* data)
    : m_Width(width), m_Height(height)
{
    if (data) {
        CreateFromPixels(data, width, height, 4);
    } else {
        // 创建空白纹理 (1x1 白色)
        u32 white = 0xFFFFFFFF;
        CreateFromPixels(&white, 1, 1, 4);
        m_Width  = width;
        m_Height = height;
    }
    LOG_DEBUG("[VKTexture2D] 创建: %ux%u", width, height);
}

VKTexture2D::~VKTexture2D() {
    if (m_Image != VK_NULL_HANDLE) {
        VulkanImage::DestroyImage(m_Image, m_Memory, m_ImageView);
        m_Image     = VK_NULL_HANDLE;
        m_Memory    = VK_NULL_HANDLE;
        m_ImageView = VK_NULL_HANDLE;
    }
    if (m_Sampler != VK_NULL_HANDLE) {
        VulkanSampler::Destroy(m_Sampler);
        m_Sampler = VK_NULL_HANDLE;
    }
}

void VKTexture2D::Bind(u32 slot) const {
    // Vulkan 纹理绑定通过 Descriptor Set 实现
}

void VKTexture2D::Unbind() const {}

void VKTexture2D::SetData(const void* data, u32 size) {
    if (!data || size == 0 || m_Image == VK_NULL_HANDLE) {
        LOG_WARN("[VKTexture2D] SetData: 无效参数");
        return;
    }

    // 通过 Staging Buffer 更新纹理数据
    VkDeviceSize bufferSize = (VkDeviceSize)size;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VulkanBuffer::CreateBuffer(bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory);

    void* mapped;
    vkMapMemory(VulkanContext::GetDevice(), stagingMemory, 0, bufferSize, 0, &mapped);
    std::memcpy(mapped, data, size);
    vkUnmapMemory(VulkanContext::GetDevice(), stagingMemory);

    // 转换 Image Layout → TRANSFER_DST → 拷贝 → SHADER_READ_ONLY
    VulkanImage::TransitionLayout(m_Image, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VulkanImage::CopyBufferToImage(stagingBuffer, m_Image, m_Width, m_Height);

    VulkanImage::TransitionLayout(m_Image, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VulkanBuffer::DestroyBuffer(stagingBuffer, stagingMemory);

    LOG_DEBUG("[VKTexture2D] SetData 完成 (%u bytes)", size);
}

// ── 内部: 从像素数据创建 Image ──────────────────────────────

void VKTexture2D::CreateFromPixels(const void* pixels, u32 width, u32 height, u32 channels) {
    VkDeviceSize imageSize = (VkDeviceSize)(width * height * channels);
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    u32 mipLevels = VulkanImage::CalculateMipLevels(width, height);

    // Staging Buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VulkanBuffer::CreateBuffer(imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory);

    void* mapped;
    vkMapMemory(VulkanContext::GetDevice(), stagingMemory, 0, imageSize, 0, &mapped);
    std::memcpy(mapped, pixels, imageSize);
    vkUnmapMemory(VulkanContext::GetDevice(), stagingMemory);

    // 创建带 mipmap 的 Image
    VulkanImage::CreateImage(width, height, format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_Image, m_Memory, mipLevels);

    // 全部 mip 转为 TRANSFER_DST
    VulkanImage::TransitionLayout(m_Image, format,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);

    // 拷贝 staging → mip 0
    VulkanImage::CopyBufferToImage(stagingBuffer, m_Image, width, height);

    // 生成 mipmap (同时将所有 mip 转为 SHADER_READ_ONLY)
    VulkanImage::GenerateMipmaps(m_Image, format, width, height, mipLevels);

    VulkanBuffer::DestroyBuffer(stagingBuffer, stagingMemory);

    // ImageView (含所有 mip 级别) + Sampler (含 maxLod)
    m_ImageView = VulkanImage::CreateImageView(m_Image, format,
                                               VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
    m_Sampler = VulkanSampler::Create(VulkanFilterMode::Linear, 16.0f, (f32)mipLevels);
}

VkDescriptorImageInfo VKTexture2D::GetDescriptorInfo() const {
    VkDescriptorImageInfo info = {};
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.imageView   = m_ImageView;
    info.sampler     = m_Sampler;
    return info;
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
