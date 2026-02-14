#pragma once

#include "engine/rhi/rhi_types.h"
#include "engine/rhi/rhi_buffer.h"
#include "engine/rhi/rhi_shader.h"
#include "engine/rhi/rhi_texture.h"
#include "engine/rhi/rhi_framebuffer.h"
#include "engine/rhi/rhi_pipeline_state.h"

#include <string>

namespace Engine {

// ── 抽象渲染设备 (工厂) ─────────────────────────────────────
// 每个图形后端提供一个 RHIDevice 实现，负责创建所有 RHI 资源。

class RHIDevice {
public:
    virtual ~RHIDevice() = default;

    /// 工厂方法：根据后端类型创建设备
    static Scope<RHIDevice> Create(GraphicsBackend backend);

    /// 获取当前后端类型
    virtual GraphicsBackend GetBackend() const = 0;

    // ── 资源创建 ────────────────────────────────────────────

    /// 创建顶点缓冲
    virtual Scope<RHIVertexBuffer> CreateVertexBuffer(
        const void* data, u32 size,
        RHIBufferUsage usage = RHIBufferUsage::Static) = 0;

    /// 创建索引缓冲
    virtual Scope<RHIIndexBuffer> CreateIndexBuffer(
        const u32* indices, u32 count) = 0;

    /// 创建顶点数组
    virtual Scope<RHIVertexArray> CreateVertexArray() = 0;

    /// 从源码创建着色器 (OpenGL: GLSL, Vulkan: SPIR-V 路径)
    virtual Scope<RHIShader> CreateShader(
        const std::string& vertexSrc,
        const std::string& fragmentSrc) = 0;

    /// 从文件加载纹理
    virtual Scope<RHITexture2D> CreateTexture2DFromFile(
        const std::string& filepath) = 0;

    /// 创建空纹理
    virtual Scope<RHITexture2D> CreateTexture2D(
        u32 width, u32 height, const void* data = nullptr) = 0;

    /// 创建帧缓冲
    virtual Scope<RHIFramebuffer> CreateFramebuffer(
        const RHIFramebufferSpec& spec) = 0;

    /// 创建管线状态
    virtual Scope<RHIPipelineState> CreatePipelineState(
        const RHIPipelineStateDesc& desc) = 0;

    // ── 渲染命令 ────────────────────────────────────────────

    virtual void SetViewport(u32 x, u32 y, u32 width, u32 height) = 0;
    virtual void SetClearColor(f32 r, f32 g, f32 b, f32 a = 1.0f) = 0;
    virtual void Clear() = 0;
    virtual void DrawArrays(u32 vertexCount) = 0;
    virtual void DrawElements(u32 indexCount) = 0;
};

} // namespace Engine
