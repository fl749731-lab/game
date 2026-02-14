#pragma once

#include "engine/core/types.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/shader.h"

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <string>

namespace Engine {

// ── 批处理实例数据 ──────────────────────────────────────────
//
// 每个实例携带 Model 矩阵 + PBR 材质参数。
// GPU 属性布局: location 5~12 (避开 Mesh 顶点的 0~4)
//   layout 5~8:  Model 矩阵 (4 × vec4)
//   layout 9:    Albedo.rgb + Metallic
//   layout 10:   EmissiveColor.rgb + EmissiveIntensity
//   layout 11:   Roughness, UseTex, UseNormalMap, IsEmissive

struct BatchInstanceData {
    glm::mat4 Model;           // → location 5~8
    glm::vec4 Albedo;          // → location 9   (rgb = albedo, a = metallic)
    glm::vec4 EmissiveInfo;    // → location 10  (rgb = emissive, a = intensity)
    glm::vec4 MaterialParams;  // → location 11  (x=roughness, y=useTex, z=useNormal, w=isEmissive)
};

// ── 批处理渲染键 ────────────────────────────────────────────

struct BatchKey {
    std::string MeshType;
    std::string TextureName;
    std::string NormalMapName;

    bool operator==(const BatchKey& other) const {
        return MeshType == other.MeshType &&
               TextureName == other.TextureName &&
               NormalMapName == other.NormalMapName;
    }
};

struct BatchKeyHash {
    size_t operator()(const BatchKey& k) const {
        size_t h1 = std::hash<std::string>{}(k.MeshType);
        size_t h2 = std::hash<std::string>{}(k.TextureName);
        size_t h3 = std::hash<std::string>{}(k.NormalMapName);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

// ── 批处理渲染器 ────────────────────────────────────────────
//
// 自动将实体按 Mesh+Texture 分组，同组实体用 GPU 实例化绘制。
// 使用专用的 instanced G-Buffer shader，材质参数通过实例属性传递。
//
// 用法:
//   BatchRenderer::Begin(shader)
//   BatchRenderer::Submit(meshType, texName, normalMapName, data)
//   BatchRenderer::End()   ← 每组一次 DrawCall

class BatchRenderer {
public:
    static void Init(u32 maxInstances = 10000);
    static void Shutdown();

    /// 开始一帧的批处理收集
    static void Begin(Shader* shader);

    /// 提交一个实例
    static void Submit(const std::string& meshType,
                       const std::string& textureName,
                       const std::string& normalMapName,
                       const BatchInstanceData& data);

    /// 刷新所有批次 (每组一次 DrawCall)
    static void End();

    /// 获取统计
    static u32 GetDrawCallCount() { return s_DrawCalls; }
    static u32 GetInstanceCount() { return s_TotalInstances; }
    static void ResetStats();

private:
    // 实例属性起始 location = 5
    static constexpr u32 INSTANCE_ATTRIB_START = 5;

    struct BatchGroup {
        std::vector<BatchInstanceData> Instances;
    };

    static u32 s_InstanceVBO;
    static u32 s_MaxInstances;
    static Shader* s_CurrentShader;
    static std::unordered_map<BatchKey, BatchGroup, BatchKeyHash> s_Batches;
    static u32 s_DrawCalls;
    static u32 s_TotalInstances;
};

} // namespace Engine
