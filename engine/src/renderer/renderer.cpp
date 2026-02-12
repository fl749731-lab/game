#include "engine/renderer/renderer.h"
#include "engine/core/log.h"

#include <glad/glad.h>

namespace Engine {

Renderer::Stats Renderer::s_Stats;

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
    s_Stats.DrawCalls++;
    s_Stats.TriangleCount += vertexCount / 3;
}

void Renderer::DrawElements(u32 vao, u32 indexCount) {
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    s_Stats.DrawCalls++;
    s_Stats.TriangleCount += indexCount / 3;
}

void Renderer::ResetStats() {
    s_Stats.DrawCalls = 0;
    s_Stats.TriangleCount = 0;
}

Renderer::Stats Renderer::GetStats() {
    return s_Stats;
}

void Renderer::SetCullFace(bool enabled) {
    if (enabled) glEnable(GL_CULL_FACE);
    else glDisable(GL_CULL_FACE);
}

void Renderer::SetWireframe(bool enabled) {
    if (enabled) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void Renderer::NotifyDraw(u32 triangleCount) {
    s_Stats.DrawCalls++;
    s_Stats.TriangleCount += triangleCount;
}

} // namespace Engine
