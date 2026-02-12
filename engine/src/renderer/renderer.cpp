#include "engine/renderer/renderer.h"
#include "engine/core/log.h"

#include <glad/glad.h>

namespace Engine {

#ifdef ENGINE_DEBUG
static void APIENTRY OpenGLDebugCallback(GLenum source, GLenum type, GLuint id,
    GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

    const char* severityStr = "未知";
    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:   severityStr = "高"; break;
        case GL_DEBUG_SEVERITY_MEDIUM: severityStr = "中"; break;
        case GL_DEBUG_SEVERITY_LOW:    severityStr = "低"; break;
    }
    LOG_WARN("OpenGL 调试 [严重度: %s]: %s", severityStr, message);
}
#endif

void Renderer::Init() {
    LOG_INFO("渲染器初始化...");

#ifdef ENGINE_DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    if (glDebugMessageCallback) {
        glDebugMessageCallback(OpenGLDebugCallback, nullptr);
        LOG_DEBUG("OpenGL 调试回调已启用");
    }
#endif

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // 面剔除 — 提升渲染性能
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    LOG_INFO("渲染器初始化完成 (深度测试+面剔除+混合)");
}

void Renderer::Shutdown() {
    LOG_INFO("渲染器关闭");
}

void Renderer::SetClearColor(f32 r, f32 g, f32 b, f32 a) {
    glClearColor(r, g, b, a);
}

void Renderer::Clear() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::SetViewport(u32 x, u32 y, u32 width, u32 height) {
    glViewport(x, y, width, height);
}

void Renderer::DrawArrays(u32 vao, u32 vertexCount) {
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
}

void Renderer::DrawElements(u32 vao, u32 indexCount) {
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
}

} // namespace Engine
