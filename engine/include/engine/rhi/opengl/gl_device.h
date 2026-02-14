#pragma once

#include "engine/rhi/rhi_device.h"
#include "engine/rhi/opengl/gl_buffer.h"
#include "engine/rhi/opengl/gl_shader.h"
#include "engine/rhi/opengl/gl_texture.h"
#include "engine/rhi/opengl/gl_framebuffer.h"

namespace Engine {

// ── OpenGL 渲染设备 ────────────────────────────────────────

class GLDevice : public RHIDevice {
public:
    GLDevice() = default;
    ~GLDevice() override = default;

    GraphicsBackend GetBackend() const override { return GraphicsBackend::OpenGL; }

    // ── 资源创建 ────────────────────────────────────────────

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

    // ── 渲染命令 ────────────────────────────────────────────

    void SetViewport(u32 x, u32 y, u32 width, u32 height) override;
    void SetClearColor(f32 r, f32 g, f32 b, f32 a = 1.0f) override;
    void Clear() override;
    void DrawArrays(u32 vertexCount) override;
    void DrawElements(u32 indexCount) override;
};

} // namespace Engine
