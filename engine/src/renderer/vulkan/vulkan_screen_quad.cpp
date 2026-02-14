#include "engine/renderer/vulkan/vulkan_screen_quad.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/core/log.h"

namespace Engine {

// ── 全屏四边形 ──────────────────────────────────────────────
// 使用 "full-screen triangle" 技术:
// vertex shader 只需 gl_VertexIndex，生成覆盖全屏的超大三角形
// 不需要任何顶点缓冲

void VulkanScreenQuad::Init() {
    // 无顶点缓冲方案，无需初始化
    LOG_DEBUG("[Vulkan] ScreenQuad 初始化 (无顶点缓冲模式)");
}

void VulkanScreenQuad::Shutdown() {
    // 无资源需要释放
}

void VulkanScreenQuad::Draw(VkCommandBuffer cmd) {
    // 绘制 3 个顶点 (vertex shader 内部生成全屏三角形)
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
