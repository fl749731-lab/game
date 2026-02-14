#pragma once

#include "engine/core/types.h"
#include "engine/renderer/material.h"

#include <glm/glm.hpp>
#include <vector>
#include <algorithm>

namespace Engine {

// ── 渲染命令 ────────────────────────────────────────────────
// 一个 draw call 的完整信息，用于排序后批量提交

struct RenderCommand {
    u64 SortKey = 0;          // 用于排序 (Shader ID << 32 | Material ID)
    Ref<Material> Mat;         // 材质
    glm::mat4 Transform;       // 模型变换矩阵
    u32 MeshID = 0;           // Mesh 资源 ID
    f32 DistToCamera = 0;     // 到摄像机距离 (透明物反向排序用)
    bool Transparent = false; // 是否透明
};

// ── RenderQueue ─────────────────────────────────────────────
// 收集渲染命令 → 排序 → 批量执行 → 减少 GPU state change
//
// 排序策略:
//   不透明: Shader → Material → Mesh → Front-to-Back
//   透明: Back-to-Front (远→近)

class RenderQueue {
public:
    /// 提交渲染命令
    void Submit(const RenderCommand& cmd);

    /// 排序 (不透明 + 透明分开)
    void Sort();

    /// 获取排序后的命令列表 (用于执行渲染)
    const std::vector<RenderCommand>& GetOpaqueCommands() const { return m_OpaqueCommands; }
    const std::vector<RenderCommand>& GetTransparentCommands() const { return m_TransparentCommands; }

    /// 每帧重置
    void Clear();

    /// 统计
    u32 GetOpaqueCount() const { return (u32)m_OpaqueCommands.size(); }
    u32 GetTransparentCount() const { return (u32)m_TransparentCommands.size(); }
    u32 GetTotalCount() const { return GetOpaqueCount() + GetTransparentCount(); }

    /// 批次统计 (排序后相邻相同 Shader 的命令为一个批次)
    u32 CountBatches() const;

private:
    std::vector<RenderCommand> m_OpaqueCommands;
    std::vector<RenderCommand> m_TransparentCommands;
};

} // namespace Engine
