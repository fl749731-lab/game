#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace Engine {

// ── Vulkan 程序化天空盒 ─────────────────────────────────────
// 使用顶部/地平线/底部三色渐变 + 太阳光方向
// 在 G-Buffer Pass 前渲染，写入最远深度

struct VulkanSkyboxColors {
    glm::vec3 Top     = {0.2f, 0.3f, 0.8f};
    f32       _pad0   = 0.0f;
    glm::vec3 Horizon = {0.6f, 0.7f, 0.9f};
    f32       _pad1   = 0.0f;
    glm::vec3 Bottom  = {0.9f, 0.8f, 0.7f};
    f32       _pad2   = 0.0f;
    glm::vec3 SunDir  = {0.3f, 0.7f, 0.5f};
    f32       SunSize = 0.01f;
};

class VulkanSkybox {
public:
    static bool Init();
    static void Shutdown();

    /// 渲染天空盒 (在 G-Buffer Pass 前或单独 Pass)
    /// @param cmd   当前 CommandBuffer
    /// @param view  相机 View 矩阵 (去掉平移部分)
    /// @param proj  投影矩阵
    static void Render(VkCommandBuffer cmd,
                       const glm::mat4& view, const glm::mat4& proj);

    static VulkanSkyboxColors& GetColors() { return s_Colors; }

private:
    static VulkanSkyboxColors s_Colors;

    // Push Constants: VP 矩阵 + 天空参数
    struct SkyboxPushConstants {
        glm::mat4 InvVP;
        VulkanSkyboxColors Colors;
    };

    static VkPipeline       s_Pipeline;
    static VkPipelineLayout s_PipelineLayout;
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
