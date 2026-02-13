#pragma once

#include "engine/core/types.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/shader.h"

#include <glm/glm.hpp>
#include <vector>

namespace Engine {

// ── 实例化渲染 (Instanced Rendering) ────────────────────────
//
// 批量绘制相同网格的多个实例，使用 GPU 实例化减少 Draw Call。
// 每个实例有独立的 Model Matrix 和可选颜色。
//
// 用法:
//   1. InstanceRenderer::Begin(mesh, shader)
//   2. InstanceRenderer::Submit(modelMatrix) 或 Submit(modelMatrix, color)
//   3. InstanceRenderer::End() — 一次性绘制所有实例
//   4. InstanceRenderer::Flush() — 清除实例数据

struct InstanceData {
    glm::mat4 Model;
    glm::vec4 Color = {1, 1, 1, 1};
};

class InstanceRenderer {
public:
    static void Init(u32 maxInstances = 10000);
    static void Shutdown();

    /// 开始实例化批次
    static void Begin(Mesh* mesh, Shader* shader);

    /// 提交一个实例
    static void Submit(const glm::mat4& model);
    static void Submit(const glm::mat4& model, const glm::vec4& color);

    /// 刷新并绘制所有已提交实例（GPU instanced draw call）
    static void End();

    /// 重置计数（不释放内存）
    static void Flush();

    /// 获取统计
    static u32 GetInstanceCount();
    static u32 GetDrawCallCount();
    static void ResetStats();

private:
    static void SetupInstanceVBO();

    static u32 s_InstanceVBO;
    static u32 s_MaxInstances;
    static std::vector<InstanceData> s_Instances;
    static Mesh* s_CurrentMesh;
    static Shader* s_CurrentShader;
    static u32 s_DrawCalls;
};

} // namespace Engine
