#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include <vulkan/vulkan.h>

namespace Engine {

// ── Vulkan 全屏四边形 ───────────────────────────────────────
// 用于延迟光照 Pass、后处理、Bloom 等全屏效果
// 使用无顶点属性的全屏三角形 (vertex shader 内生成坐标)

class VulkanScreenQuad {
public:
    static void Init();
    static void Shutdown();

    /// 绘制全屏三角形 (只需绑定好管线后调用)
    static void Draw(VkCommandBuffer cmd);
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
