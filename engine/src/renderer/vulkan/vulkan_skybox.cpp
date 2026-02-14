#include "engine/renderer/vulkan/vulkan_skybox.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/renderer/vulkan/vulkan_screen_quad.h"
#include "engine/core/log.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

VulkanSkyboxColors VulkanSkybox::s_Colors = {};

VkPipeline       VulkanSkybox::s_Pipeline       = VK_NULL_HANDLE;
VkPipelineLayout VulkanSkybox::s_PipelineLayout = VK_NULL_HANDLE;

// ── 初始化 ──────────────────────────────────────────────────

bool VulkanSkybox::Init() {
    // Pipeline 和 PipelineLayout 将在 shader 编译系统完善后创建
    // 目前仅初始化参数
    LOG_INFO("[Vulkan] Skybox 初始化完成 (程序化渐变)");
    return true;
}

void VulkanSkybox::Shutdown() {
    VkDevice device = VulkanContext::GetDevice();

    if (s_Pipeline) {
        vkDestroyPipeline(device, s_Pipeline, nullptr);
        s_Pipeline = VK_NULL_HANDLE;
    }
    if (s_PipelineLayout) {
        vkDestroyPipelineLayout(device, s_PipelineLayout, nullptr);
        s_PipelineLayout = VK_NULL_HANDLE;
    }
}

void VulkanSkybox::Render(VkCommandBuffer cmd,
                          const glm::mat4& view, const glm::mat4& proj) {
    if (!s_Pipeline) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_Pipeline);

    // 去掉 View 矩阵的平移部分 (只保留旋转)
    glm::mat4 viewNoTranslate = glm::mat4(glm::mat3(view));
    glm::mat4 VP = proj * viewNoTranslate;

    SkyboxPushConstants pc;
    pc.InvVP  = glm::inverse(VP);
    pc.Colors = s_Colors;

    vkCmdPushConstants(cmd, s_PipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(SkyboxPushConstants), &pc);

    VulkanScreenQuad::Draw(cmd);
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
