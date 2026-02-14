#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/rhi/vulkan/vk_device.h"
#include "engine/rhi/vulkan/vk_buffer.h"
#include "engine/rhi/vulkan/vk_shader.h"
#include "engine/rhi/vulkan/vk_texture.h"
#include "engine/rhi/vulkan/vk_framebuffer.h"
#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/core/log.h"

namespace Engine {

// ── Vulkan 管线状态 ─────────────────────────────────────────

class VKPipelineState : public RHIPipelineState {
public:
    VKPipelineState(const RHIPipelineStateDesc& desc) : m_Desc(desc) {}

    void Bind() const override {
        // Vulkan 管线状态在 Pipeline 创建时就已固化
    }

    const RHIPipelineStateDesc& GetDesc() const override { return m_Desc; }

private:
    RHIPipelineStateDesc m_Desc;
};

// ── VKDevice 资源创建 ───────────────────────────────────────

Scope<RHIVertexBuffer> VKDevice::CreateVertexBuffer(
    const void* data, u32 size, RHIBufferUsage usage)
{
    return CreateScope<VKVertexBuffer>(data, size, usage);
}

Scope<RHIIndexBuffer> VKDevice::CreateIndexBuffer(const u32* indices, u32 count) {
    return CreateScope<VKIndexBuffer>(indices, count);
}

Scope<RHIVertexArray> VKDevice::CreateVertexArray() {
    return CreateScope<VKVertexArray>();
}

Scope<RHIShader> VKDevice::CreateShader(
    const std::string& vertexSrc, const std::string& fragmentSrc)
{
    return CreateScope<VKShader>(vertexSrc, fragmentSrc);
}

Scope<RHITexture2D> VKDevice::CreateTexture2DFromFile(const std::string& filepath) {
    return CreateScope<VKTexture2D>(filepath);
}

Scope<RHITexture2D> VKDevice::CreateTexture2D(
    u32 width, u32 height, const void* data)
{
    return CreateScope<VKTexture2D>(width, height, data);
}

Scope<RHIFramebuffer> VKDevice::CreateFramebuffer(const RHIFramebufferSpec& spec) {
    return CreateScope<VKFramebuffer>(spec);
}

Scope<RHIPipelineState> VKDevice::CreatePipelineState(const RHIPipelineStateDesc& desc) {
    return CreateScope<VKPipelineState>(desc);
}

// ── VKDevice 渲染命令 ───────────────────────────────────────

void VKDevice::SetViewport(u32 x, u32 y, u32 width, u32 height) {
    auto cmd = VulkanRenderer::GetCurrentCommandBuffer();
    if (!cmd) return;

    VkViewport viewport = {};
    viewport.x        = (f32)x;
    viewport.y        = (f32)y;
    viewport.width    = (f32)width;
    viewport.height   = (f32)height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {(i32)x, (i32)y};
    scissor.extent = {width, height};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void VKDevice::SetClearColor(f32 r, f32 g, f32 b, f32 a) {
    // 存储到成员变量
    m_ClearColor = {r, g, b, a};
    // 同步到 VulkanRenderer
    VulkanRenderer::SetClearColor(r, g, b, a);
}

void VKDevice::Clear() {
    // Vulkan 中 clear 在 vkCmdBeginRenderPass 时通过 VK_ATTACHMENT_LOAD_OP_CLEAR 执行
    // 此处无需额外操作
}

void VKDevice::DrawArrays(u32 vertexCount) {
    auto cmd = VulkanRenderer::GetCurrentCommandBuffer();
    if (cmd) {
        vkCmdDraw(cmd, vertexCount, 1, 0, 0);
    }
}

void VKDevice::DrawElements(u32 indexCount) {
    auto cmd = VulkanRenderer::GetCurrentCommandBuffer();
    if (cmd) {
        vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
    }
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
