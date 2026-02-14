#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/rhi/rhi_device.h"
#include <glm/glm.hpp>

namespace Engine {

// ── Vulkan 渲染设备 ────────────────────────────────────────

class VKDevice : public RHIDevice {
public:
    VKDevice() = default;
    ~VKDevice() override = default;

    GraphicsBackend GetBackend() const override { return GraphicsBackend::Vulkan; }

    Scope<RHIVertexBuffer> CreateVertexBuffer(
        const void* data, u32 size,
        RHIBufferUsage usage = RHIBufferUsage::Static) override;

    Scope<RHIIndexBuffer> CreateIndexBuffer(
        const u32* indices, u32 count) override;

    Scope<RHIVertexArray> CreateVertexArray() override;

    Scope<RHIShader> CreateShader(
        const std::string& vertexSrc,
        const std::string& fragmentSrc) override;

    Scope<RHITexture2D> CreateTexture2DFromFile(
        const std::string& filepath) override;

    Scope<RHITexture2D> CreateTexture2D(
        u32 width, u32 height, const void* data = nullptr) override;

    Scope<RHIFramebuffer> CreateFramebuffer(
        const RHIFramebufferSpec& spec) override;

    Scope<RHIPipelineState> CreatePipelineState(
        const RHIPipelineStateDesc& desc) override;

    void SetViewport(u32 x, u32 y, u32 width, u32 height) override;
    void SetClearColor(f32 r, f32 g, f32 b, f32 a = 1.0f) override;
    void Clear() override;
    void DrawArrays(u32 vertexCount) override;
    void DrawElements(u32 indexCount) override;

private:
    glm::vec4 m_ClearColor = {0.01f, 0.01f, 0.02f, 1.0f};
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
