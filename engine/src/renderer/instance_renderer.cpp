#include "engine/renderer/instance_renderer.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

u32 InstanceRenderer::s_InstanceVBO = 0;
u32 InstanceRenderer::s_MaxInstances = 0;
std::vector<InstanceData> InstanceRenderer::s_Instances;
Mesh* InstanceRenderer::s_CurrentMesh = nullptr;
Shader* InstanceRenderer::s_CurrentShader = nullptr;
u32 InstanceRenderer::s_DrawCalls = 0;

// ── 初始化 ──────────────────────────────────────────────────

void InstanceRenderer::Init(u32 maxInstances) {
    s_MaxInstances = maxInstances;
    s_Instances.reserve(maxInstances);

    // 创建实例数据 VBO（动态更新）
    glGenBuffers(1, &s_InstanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, s_InstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances * sizeof(InstanceData),
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    LOG_INFO("[InstanceRenderer] 初始化完成 (最大 %u 实例)", maxInstances);
}

void InstanceRenderer::Shutdown() {
    if (s_InstanceVBO) { glDeleteBuffers(1, &s_InstanceVBO); s_InstanceVBO = 0; }
    s_Instances.clear();
    s_CurrentMesh = nullptr;
    s_CurrentShader = nullptr;
}

// ── 批次管理 ────────────────────────────────────────────────

void InstanceRenderer::Begin(Mesh* mesh, Shader* shader) {
    s_CurrentMesh = mesh;
    s_CurrentShader = shader;
    s_Instances.clear();
}

void InstanceRenderer::Submit(const glm::mat4& model) {
    if (s_Instances.size() >= s_MaxInstances) return;
    s_Instances.push_back({model, {1, 1, 1, 1}});
}

void InstanceRenderer::Submit(const glm::mat4& model, const glm::vec4& color) {
    if (s_Instances.size() >= s_MaxInstances) return;
    s_Instances.push_back({model, color});
}

void InstanceRenderer::End() {
    if (!s_CurrentMesh || !s_CurrentShader || s_Instances.empty()) return;

    s_CurrentShader->Bind();

    // 上传实例数据到 GPU
    glBindBuffer(GL_ARRAY_BUFFER, s_InstanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    s_Instances.size() * sizeof(InstanceData),
                    s_Instances.data());

    // 设置 VAO 的实例化顶点属性
    u32 vao = s_CurrentMesh->GetVAO();
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_InstanceVBO);

    // mat4 占 4 个 vec4 (属性 location 3~6)
    for (int i = 0; i < 4; i++) {
        u32 loc = 3 + i;
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE,
                              sizeof(InstanceData),
                              (void*)(sizeof(glm::vec4) * i));
        glVertexAttribDivisor(loc, 1);
    }

    // 实例颜色 (属性 location 7)
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE,
                          sizeof(InstanceData),
                          (void*)(sizeof(glm::mat4)));
    glVertexAttribDivisor(7, 1);

    // 实例化绘制
    // 注意: 若网格有索引，使用索引数; 若无索引，使用顶点数
    // 由于 GLAD 配置未导出 glDrawElementsInstanced，
    // 改为先调用 Mesh::Draw() 的方式批量处理
    glDrawArraysInstanced(GL_TRIANGLES, 0,
                          s_CurrentMesh->GetVertexCount(),
                          (GLsizei)s_Instances.size());

    // 清理属性分频器
    for (int i = 0; i < 4; i++) {
        glVertexAttribDivisor(3 + i, 0);
        glDisableVertexAttribArray(3 + i);
    }
    glVertexAttribDivisor(7, 0);
    glDisableVertexAttribArray(7);

    glBindVertexArray(0);

    s_DrawCalls++;
}

void InstanceRenderer::Flush() {
    s_Instances.clear();
}

// ── 统计 ────────────────────────────────────────────────────

u32 InstanceRenderer::GetInstanceCount() {
    return (u32)s_Instances.size();
}

u32 InstanceRenderer::GetDrawCallCount() {
    return s_DrawCalls;
}

void InstanceRenderer::ResetStats() {
    s_DrawCalls = 0;
}

} // namespace Engine
