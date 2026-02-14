#pragma once

#include "engine/core/types.h"
#include "engine/physics/collision.h"

#include <glm/glm.hpp>
#include <vector>
#include <functional>
#include <algorithm>
#include <limits>

namespace Engine {

// ── BVH 节点 ────────────────────────────────────────────────

struct BVHNode {
    AABB Bounds;
    i32 Left  = -1;    // 子节点索引 (< 0 = 叶节点)
    i32 Right = -1;
    i32 ObjectIndex = -1;   // 叶节点存储的对象索引
    i32 ObjectCount = 0;    // 叶节点包含的对象数

    bool IsLeaf() const { return ObjectCount > 0; }
};

// ── BVH 树 ──────────────────────────────────────────────────
// Top-Down 构建, SAH (Surface Area Heuristic) 分割
// 用途:
//   1. 碰撞宽相 (O(n log n) vs O(n²))
//   2. 视锥剔除 (快速排除不可见对象)
//   3. 射线拾取 (鼠标点选)

class BVH {
public:
    struct ObjectInfo {
        AABB Bounds;
        u32 UserData = 0;   // 用户数据 (实体 ID 等)
    };

    /// 从对象列表构建 BVH
    void Build(const std::vector<ObjectInfo>& objects);

    /// 清空
    void Clear();

    /// AABB 查询 (返回所有与 queryBox 相交的对象索引)
    void QueryAABB(const AABB& queryBox,
                   std::vector<u32>& results) const;

    /// 射线查询 (返回所有与射线相交的对象)
    void QueryRay(const glm::vec3& origin, const glm::vec3& direction,
                  std::vector<u32>& results) const;

    /// 视锥查询 (6 个平面)
    void QueryFrustum(const glm::vec4 planes[6],
                      std::vector<u32>& results) const;

    /// 统计
    u32 GetNodeCount() const { return (u32)m_Nodes.size(); }
    u32 GetObjectCount() const { return (u32)m_Objects.size(); }
    u32 GetDepth() const { return m_Depth; }
    const std::vector<BVHNode>& GetNodes() const { return m_Nodes; }

private:
    i32 BuildRecursive(std::vector<u32>& indices, u32 start, u32 end, u32 depth);

    /// SAH 分割 — 找到最佳分割轴和位置
    struct SplitResult { i32 Axis; float Cost; u32 SplitIndex; };
    SplitResult FindBestSplit(const std::vector<u32>& indices, u32 start, u32 end,
                              const AABB& bounds);

    /// 视锥-AABB 检测
    static bool FrustumIntersectsAABB(const glm::vec4 planes[6], const AABB& box);

    std::vector<BVHNode> m_Nodes;
    std::vector<ObjectInfo> m_Objects;
    u32 m_Depth = 0;

    static constexpr u32 MAX_LEAF_SIZE = 4;
};

} // namespace Engine
