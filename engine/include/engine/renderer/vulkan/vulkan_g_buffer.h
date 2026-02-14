#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include <vulkan/vulkan.h>
#include <vector>

namespace Engine {

// ── Vulkan G-Buffer (延迟渲染几何缓冲) ──────────────────────
//
// 附件布局 (对齐 OpenGL 端):
//   RT0 (R16G16B16A16_SFLOAT) — 世界空间位置 (Position.xyz)
//   RT1 (R16G16B16A16_SFLOAT) — 世界空间法线 (Normal.xyz)
//   RT2 (R8G8B8A8_UNORM)      — 漫反射 + 高光 (Albedo.rgb, Specular)
//   RT3 (R8G8B8A8_UNORM)      — 自发光 (Emissive.rgb, Reserved)
//   Depth (D32_SFLOAT)        — 深度 (前向叠加 Pass 复用)

class VulkanGBuffer {
public:
    static bool Init(u32 width, u32 height);
    static void Shutdown();
    static void Resize(u32 width, u32 height);

    /// 绑定 G-Buffer (开始 RenderPass)
    static void BeginPass(VkCommandBuffer cmd);
    /// 结束 G-Buffer (结束 RenderPass)
    static void EndPass(VkCommandBuffer cmd);

    // ── 纹理采样 (光照 Pass 使用) ──────────────────────────
    static VkImageView GetPositionView()  { return s_ColorViews[0]; }
    static VkImageView GetNormalView()    { return s_ColorViews[1]; }
    static VkImageView GetAlbedoView()    { return s_ColorViews[2]; }
    static VkImageView GetEmissiveView()  { return s_ColorViews[3]; }
    static VkImageView GetDepthView()     { return s_DepthView; }
    static VkSampler   GetSampler()       { return s_Sampler; }

    static VkRenderPass GetRenderPass()   { return s_RenderPass; }
    static u32 GetWidth()  { return s_Width; }
    static u32 GetHeight() { return s_Height; }

    /// 返回 Descriptor Image Info 数组 (用于光照 Pass)
    static void GetDescriptorInfos(VkDescriptorImageInfo outInfos[4]);

    static constexpr u32 COLOR_ATTACHMENT_COUNT = 4;

private:
    static bool CreateImages(u32 width, u32 height);
    static bool CreateRenderPass();
    static bool CreateFramebuffer(u32 width, u32 height);
    static void DestroyResources();

    // 4 个颜色附件
    static VkImage        s_ColorImages[4];
    static VkDeviceMemory s_ColorMemories[4];
    static VkImageView    s_ColorViews[4];

    // 深度附件
    static VkImage        s_DepthImage;
    static VkDeviceMemory s_DepthMemory;
    static VkImageView    s_DepthView;

    static VkRenderPass   s_RenderPass;
    static VkFramebuffer  s_Framebuffer;
    static VkSampler      s_Sampler;

    static u32 s_Width;
    static u32 s_Height;

    static VkFormat s_ColorFormats[4];
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
