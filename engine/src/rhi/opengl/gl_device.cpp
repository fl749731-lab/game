#include "engine/rhi/opengl/gl_device.h"
#include "engine/core/log.h"

#include <glad/glad.h>

namespace Engine {

// ── OpenGL 管线状态实现 ─────────────────────────────────────

class GLPipelineState : public RHIPipelineState {
public:
    GLPipelineState(const RHIPipelineStateDesc& desc) : m_Desc(desc) {}

    void Bind() const override {
        // 深度测试
        if (m_Desc.DepthTest) {
            glEnable(GL_DEPTH_TEST);
            glDepthMask(m_Desc.DepthWrite ? GL_TRUE : GL_FALSE);
            glDepthFunc(ToGLDepthFunc(m_Desc.DepthFunc));
        } else {
            glDisable(GL_DEPTH_TEST);
        }

        // 混合
        if (m_Desc.Blending) {
            glEnable(GL_BLEND);
            glBlendFunc(ToGLBlendFactor(m_Desc.SrcFactor),
                        ToGLBlendFactor(m_Desc.DstFactor));
        } else {
            glDisable(GL_BLEND);
        }

        // 面剔除
        if (m_Desc.CullMode != RHICullMode::None) {
            glEnable(GL_CULL_FACE);
            glCullFace(m_Desc.CullMode == RHICullMode::Front ? GL_FRONT : GL_BACK);
        } else {
            glDisable(GL_CULL_FACE);
        }

        // 线框
        glPolygonMode(GL_FRONT_AND_BACK, m_Desc.Wireframe ? GL_LINE : GL_FILL);
    }

    const RHIPipelineStateDesc& GetDesc() const override { return m_Desc; }

private:
    RHIPipelineStateDesc m_Desc;

    static GLenum ToGLDepthFunc(RHIDepthFunc f) {
        switch (f) {
            case RHIDepthFunc::Less:         return GL_LESS;
            case RHIDepthFunc::LessEqual:    return GL_LEQUAL;
            case RHIDepthFunc::Greater:      return GL_GREATER;
            case RHIDepthFunc::GreaterEqual: return GL_GEQUAL;
            case RHIDepthFunc::Equal:        return GL_EQUAL;
            case RHIDepthFunc::NotEqual:     return GL_NOTEQUAL;
            case RHIDepthFunc::Always:       return GL_ALWAYS;
            case RHIDepthFunc::Never:        return GL_NEVER;
        }
        return GL_LESS;
    }

    static GLenum ToGLBlendFactor(RHIBlendFactor f) {
        switch (f) {
            case RHIBlendFactor::Zero:              return GL_ZERO;
            case RHIBlendFactor::One:               return GL_ONE;
            case RHIBlendFactor::SrcAlpha:           return GL_SRC_ALPHA;
            case RHIBlendFactor::OneMinusSrcAlpha:   return GL_ONE_MINUS_SRC_ALPHA;
            case RHIBlendFactor::DstAlpha:           return GL_DST_ALPHA;
            case RHIBlendFactor::OneMinusDstAlpha:   return GL_ONE_MINUS_DST_ALPHA;
            case RHIBlendFactor::SrcColor:           return GL_SRC_COLOR;
            case RHIBlendFactor::OneMinusSrcColor:   return GL_ONE_MINUS_SRC_COLOR;
        }
        return GL_ONE;
    }
};

// ── GLDevice 资源创建 ───────────────────────────────────────

Scope<RHIVertexBuffer> GLDevice::CreateVertexBuffer(
    const void* data, u32 size, RHIBufferUsage usage)
{
    return CreateScope<GLVertexBuffer>(data, size, usage);
}

Scope<RHIIndexBuffer> GLDevice::CreateIndexBuffer(const u32* indices, u32 count) {
    return CreateScope<GLIndexBuffer>(indices, count);
}

Scope<RHIVertexArray> GLDevice::CreateVertexArray() {
    return CreateScope<GLVertexArray>();
}

Scope<RHIShader> GLDevice::CreateShader(
    const std::string& vertexSrc, const std::string& fragmentSrc)
{
    return CreateScope<GLShader>(vertexSrc, fragmentSrc);
}

Scope<RHITexture2D> GLDevice::CreateTexture2DFromFile(const std::string& filepath) {
    return CreateScope<GLTexture2D>(filepath);
}

Scope<RHITexture2D> GLDevice::CreateTexture2D(
    u32 width, u32 height, const void* data)
{
    return CreateScope<GLTexture2D>(width, height, data);
}

Scope<RHIFramebuffer> GLDevice::CreateFramebuffer(const RHIFramebufferSpec& spec) {
    return CreateScope<GLFramebuffer>(spec);
}

Scope<RHIPipelineState> GLDevice::CreatePipelineState(const RHIPipelineStateDesc& desc) {
    return CreateScope<GLPipelineState>(desc);
}

// ── GLDevice 渲染命令 ───────────────────────────────────────

void GLDevice::SetViewport(u32 x, u32 y, u32 width, u32 height) {
    glViewport(x, y, width, height);
}

void GLDevice::SetClearColor(f32 r, f32 g, f32 b, f32 a) {
    glClearColor(r, g, b, a);
}

void GLDevice::Clear() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLDevice::DrawArrays(u32 vertexCount) {
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
}

void GLDevice::DrawElements(u32 indexCount) {
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
}

} // namespace Engine
