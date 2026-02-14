#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include <vulkan/vulkan.h>
#include <string>

namespace Engine {

// ── Vulkan Image 辅助 ──────────────────────────────────────

class VulkanImage {
public:
    /// 创建 2D 图像 (支持 mipLevels)
    static void CreateImage(u32 width, u32 height, VkFormat format,
                            VkImageTiling tiling, VkImageUsageFlags usage,
                            VkMemoryPropertyFlags memProps,
                            VkImage& image, VkDeviceMemory& memory,
                            u32 mipLevels = 1);

    /// 创建 ImageView (支持 mipLevels)
    static VkImageView CreateImageView(VkImage image, VkFormat format,
                                       VkImageAspectFlags aspectFlags,
                                       u32 mipLevels = 1);

    /// 布局转换 (支持多 mip 级别)
    static void TransitionLayout(VkImage image, VkFormat format,
                                 VkImageLayout oldLayout, VkImageLayout newLayout,
                                 u32 mipLevels = 1);

    /// 拷贝 Buffer → Image (mip 0)
    static void CopyBufferToImage(VkBuffer buffer, VkImage image,
                                  u32 width, u32 height);

    /// 自动生成 Mipmap (使用 vkCmdBlitImage)
    static void GenerateMipmaps(VkImage image, VkFormat format,
                                u32 width, u32 height, u32 mipLevels);

    static void DestroyImage(VkImage image, VkDeviceMemory memory,
                             VkImageView view = VK_NULL_HANDLE);

    /// 计算 mip 级别数
    static u32 CalculateMipLevels(u32 width, u32 height);
};

// ── Vulkan Sampler ─────────────────────────────────────────

enum class VulkanFilterMode : u8 { Nearest, Linear };

class VulkanSampler {
public:
    static VkSampler Create(VulkanFilterMode filter = VulkanFilterMode::Linear,
                            f32 maxAnisotropy = 16.0f,
                            f32 maxLod = 0.0f);
    static void Destroy(VkSampler sampler);
};

// ── Vulkan Texture2D ───────────────────────────────────────

class VulkanTexture2D {
public:
    VulkanTexture2D(const std::string& filepath);
    VulkanTexture2D(u32 width, u32 height, const void* pixels, u32 channels = 4);
    ~VulkanTexture2D();

    // 禁止拷贝
    VulkanTexture2D(const VulkanTexture2D&) = delete;
    VulkanTexture2D& operator=(const VulkanTexture2D&) = delete;

    VkImageView GetImageView() const { return m_ImageView; }
    VkSampler   GetSampler()   const { return m_Sampler; }
    u32 GetWidth()  const { return m_Width; }
    u32 GetHeight() const { return m_Height; }
    u32 GetMipLevels() const { return m_MipLevels; }
    bool IsValid()  const { return m_Image != VK_NULL_HANDLE; }

    /// 返回适合写入 DescriptorSet 的 ImageInfo
    VkDescriptorImageInfo GetDescriptorInfo() const;

private:
    void CreateFromPixels(const void* pixels, u32 width, u32 height, u32 channels);

    VkImage        m_Image     = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory    = VK_NULL_HANDLE;
    VkImageView    m_ImageView = VK_NULL_HANDLE;
    VkSampler      m_Sampler   = VK_NULL_HANDLE;
    u32 m_Width     = 0;
    u32 m_Height    = 0;
    u32 m_MipLevels = 1;
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
