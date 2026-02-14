#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/rhi/vulkan/vk_shader.h"
#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/renderer/vulkan/vulkan_shader.h"
#include "engine/core/log.h"

#include <cstring>

namespace Engine {

VKShader::VKShader(const std::string& vertexPath, const std::string& fragmentPath) {
    VulkanShaderConfig config;
    config.VertexPath   = vertexPath;
    config.FragmentPath = fragmentPath;
    config.VertexInput  = VulkanShader::GetMeshVertexInput();
    config.RenderPass   = VulkanSwapchain::GetRenderPass();

    // 预留 128 字节 Push Constants (2个 mat4)
    config.PushConstantSize   = 128;
    config.PushConstantStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    m_VulkanShader = CreateScope<VulkanShader>(config);
    if (m_VulkanShader && m_VulkanShader->IsValid()) {
        m_Pipeline       = m_VulkanShader->GetPipeline();
        m_PipelineLayout = m_VulkanShader->GetLayout();
        LOG_DEBUG("[VKShader] Pipeline 创建成功");
    } else {
        LOG_ERROR("[VKShader] Pipeline 创建失败: %s + %s", vertexPath.c_str(), fragmentPath.c_str());
    }
}

VKShader::~VKShader() {
    m_VulkanShader.reset();
    m_Pipeline       = VK_NULL_HANDLE;
    m_PipelineLayout = VK_NULL_HANDLE;
}

void VKShader::Bind() const {
    auto cmd = VulkanRenderer::GetCurrentCommandBuffer();
    if (cmd && m_Pipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
    }
    m_PushConstantOffset = 0; // 每次 Bind 重置偏移
}

void VKShader::Unbind() const {}

// ── Uniform 设置 (Push Constants) ───────────────────────────

void VKShader::SetInt(const std::string& name, i32 value) {
    PushData(name, &value, sizeof(i32));
}

void VKShader::SetFloat(const std::string& name, f32 value) {
    PushData(name, &value, sizeof(f32));
}

void VKShader::SetVec2(const std::string& name, f32 x, f32 y) {
    f32 data[2] = {x, y};
    PushData(name, data, sizeof(data));
}

void VKShader::SetVec3(const std::string& name, f32 x, f32 y, f32 z) {
    f32 data[3] = {x, y, z};
    PushData(name, data, sizeof(data));
}

void VKShader::SetVec4(const std::string& name, f32 x, f32 y, f32 z, f32 w) {
    f32 data[4] = {x, y, z, w};
    PushData(name, data, sizeof(data));
}

void VKShader::SetMat3(const std::string& name, const f32* value) {
    PushData(name, value, sizeof(f32) * 9);
}

void VKShader::SetMat4(const std::string& name, const f32* value) {
    PushData(name, value, sizeof(f32) * 16);
}

void VKShader::PushData(const std::string& name, const void* data, u32 size) {
    auto cmd = VulkanRenderer::GetCurrentCommandBuffer();
    if (!cmd || m_PipelineLayout == VK_NULL_HANDLE) return;

    if (m_PushConstantOffset + size > 128) {
        LOG_WARN("[VKShader] Push Constants 溢出 (%u + %u > 128), 跳过: %s",
                 m_PushConstantOffset, size, name.c_str());
        return;
    }

    vkCmdPushConstants(cmd, m_PipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        m_PushConstantOffset, size, data);

    m_PushConstantOffset += size;
}

void VKShader::ResetPushConstants() {
    m_PushConstantOffset = 0;
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
