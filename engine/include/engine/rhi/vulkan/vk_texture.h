#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/rhi/rhi_texture.h"
#include <vulkan/vulkan.h>
#include <string>

namespace Engine {

// ── Vulkan 2D 纹理 ─────────────────────────────────────────
// 直接管理 VkImage/VkImageView/VkSampler 资源

class VKTexture2D : public RHITexture2D {
public:
    VKTexture2D(const std::string& filepath);
    VKTexture2D(u32 width, u32 height, const void* data = nullptr);
    ~VKTexture2D() override;

    void Bind(u32 slot = 0) const override;
    void Unbind() const override;
    void SetData(const void* data, u32 size) override;

    u32 GetWidth()  const override { return m_Width; }
    u32 GetHeight() const override { return m_Height; }
    bool IsValid()  const override { return m_Image != VK_NULL_HANDLE; }

    /// Vulkan 专用: 获取 Descriptor 写入信息
    VkImageView GetImageView() const { return m_ImageView; }
    VkSampler   GetSampler()   const { return m_Sampler; }
    VkDescriptorImageInfo GetDescriptorInfo() const;

private:
    void CreateFromPixels(const void* pixels, u32 width, u32 height, u32 channels);

    VkImage        m_Image     = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory    = VK_NULL_HANDLE;
    VkImageView    m_ImageView = VK_NULL_HANDLE;
    VkSampler      m_Sampler   = VK_NULL_HANDLE;
    u32 m_Width  = 0;
    u32 m_Height = 0;
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
