#include "engine/physics/bvh.h"
#include "engine/core/log.h"

#include <numeric>

namespace Engine {

// ── 构建 ────────────────────────────────────────────────────

void BVH::Build(const std::vector<ObjectInfo>& objects) {
    Clear();
    if (objects.empty()) return;

    m_Objects = objects;
    m_Nodes.reserve(objects.size() * 2);  // 预估节点数

    std::vector<u32> indices(objects.size());
    std::iota(indices.begin(), indices.end(), 0);

    m_Depth = 0;
    BuildRecursive(indices, 0, (u32)indices.size(), 0);

    LOG_INFO("[BVH] 构建完成: %u 对象, %u 节点, 深度 %u",
             (u32)m_Objects.size(), (u32)m_Nodes.size(), m_Depth);
}

void BVH::Clear() {
    m_Nodes.clear();
    m_Objects.clear();
    m_Depth = 0;
}

i32 BVH::BuildRecursive(std::vector<u32>& indices, u32 start, u32 end, u32 depth) {
    if (depth > m_Depth) m_Depth = depth;

    i32 nodeIndex = (i32)m_Nodes.size();
    m_Nodes.push_back({});
    BVHNode& node = m_Nodes.back();

    // 计算包围盒
    AABB bounds;
    for (u32 i = start; i < end; i++) {
        bounds.Expand(m_Objects[indices[i]].Bounds);
    }
    node.Bounds = bounds;

    u32 count = end - start;

    // 叶节点
    if (count <= MAX_LEAF_SIZE) {
        node.ObjectIndex = (i32)start;
        node.ObjectCount = (i32)count;
        // 重排对象索引以便连续存储被引用
        return nodeIndex;
    }

    // SAH 分割
    auto split = FindBestSplit(indices, start, end, bounds);

    if (split.SplitIndex == start || split.SplitIndex == end) {
        // SAH 未找到好的分割，中点分割
        u32 mid = (start + end) / 2;
        i32 axis = 0;
        glm::vec3 sz = bounds.Size();
        if (sz.y > sz.x) axis = 1;
        if (sz.z > sz[axis]) axis = 2;

        std::nth_element(
            indices.begin() + start,
            indices.begin() + mid,
            indices.begin() + end,
            [&](u32 a, u32 b) {
                return m_Objects[a].Bounds.Center()[axis] < m_Objects[b].Bounds.Center()[axis];
            });

        node.Left = BuildRecursive(indices, start, mid, depth + 1);
        // 需要重新获取 node 引用（可能因 push_back 失效）
        m_Nodes[nodeIndex].Right = BuildRecursive(indices, mid, end, depth + 1);
    } else {
        node.Left = BuildRecursive(indices, start, split.SplitIndex, depth + 1);
        m_Nodes[nodeIndex].Right = BuildRecursive(indices, split.SplitIndex, end, depth + 1);
    }

    return nodeIndex;
}

BVH::SplitResult BVH::FindBestSplit(const std::vector<u32>& indices,
                                      u32 start, u32 end,
                                      const AABB& bounds) {
    constexpr u32 NUM_BINS = 12;
    float bestCost = std::numeric_limits<float>::max();
    i32 bestAxis = -1;
    u32 bestSplit = start;

    float parentArea = bounds.SurfaceArea();
    if (parentArea < 1e-8f) return {-1, bestCost, start};

    for (i32 axis = 0; axis < 3; axis++) {
        float axisMin = bounds.Min[axis];
        float axisMax = bounds.Max[axis];
        float axisRange = axisMax - axisMin;
        if (axisRange < 1e-6f) continue;

        // 按轴排序
        std::vector<u32> sorted(indices.begin() + start, indices.begin() + end);
        std::sort(sorted.begin(), sorted.end(), [&](u32 a, u32 b) {
            return m_Objects[a].Bounds.Center()[axis] < m_Objects[b].Bounds.Center()[axis];
        });

        // 从左扫描累积包围盒
        u32 count = end - start;
        std::vector<float> leftAreas(count);
        AABB leftBox;
        for (u32 i = 0; i < count; i++) {
            leftBox.Expand(m_Objects[sorted[i]].Bounds);
            leftAreas[i] = leftBox.SurfaceArea();
        }

        // 从右扫描累积包围盒
        AABB rightBox;
        for (u32 i = count - 1; i > 0; i--) {
            rightBox.Expand(m_Objects[sorted[i]].Bounds);
            float rightArea = rightBox.SurfaceArea();
            float cost = (leftAreas[i-1] * (float)i + rightArea * (float)(count - i)) / parentArea;
            if (cost < bestCost) {
                bestCost = cost;
                bestAxis = axis;
                bestSplit = start + i;
            }
        }
    }

    if (bestAxis >= 0) {
        // 按最佳轴重排
        // (简化: 使用 nth_element 在最佳分割点)
        // 注意这里需要修改原始 indices
        auto* idxPtr = const_cast<u32*>(indices.data());
        std::nth_element(
            idxPtr + start, idxPtr + bestSplit, idxPtr + end,
            [&](u32 a, u32 b) {
                return m_Objects[a].Bounds.Center()[bestAxis] <
                       m_Objects[b].Bounds.Center()[bestAxis];
            });
    }

    return {bestAxis, bestCost, bestSplit};
}

// ── 查询 ────────────────────────────────────────────────────

void BVH::QueryAABB(const AABB& queryBox, std::vector<u32>& results) const {
    if (m_Nodes.empty()) return;

    // 迭代栈式遍历
    std::vector<i32> stack;
    stack.reserve(64);
    stack.push_back(0);

    while (!stack.empty()) {
        i32 idx = stack.back();
        stack.pop_back();
        if (idx < 0 || idx >= (i32)m_Nodes.size()) continue;

        const auto& node = m_Nodes[idx];
        if (!node.Bounds.Intersects(queryBox)) continue;

        if (node.IsLeaf()) {
            for (i32 i = 0; i < node.ObjectCount; i++) {
                u32 objIdx = (u32)(node.ObjectIndex + i);
                if (objIdx < m_Objects.size() && m_Objects[objIdx].Bounds.Intersects(queryBox)) {
                    results.push_back(m_Objects[objIdx].UserData);
                }
            }
        } else {
            stack.push_back(node.Left);
            stack.push_back(node.Right);
        }
    }
}

void BVH::QueryRay(const glm::vec3& origin, const glm::vec3& direction,
                    std::vector<u32>& results) const {
    if (m_Nodes.empty()) return;

    glm::vec3 invDir = 1.0f / glm::max(glm::abs(direction), glm::vec3(1e-8f));
    invDir *= glm::sign(direction + glm::vec3(1e-8f));

    std::vector<i32> stack;
    stack.reserve(64);
    stack.push_back(0);

    while (!stack.empty()) {
        i32 idx = stack.back();
        stack.pop_back();
        if (idx < 0 || idx >= (i32)m_Nodes.size()) continue;

        const auto& node = m_Nodes[idx];
        if (!node.Bounds.RayIntersect(origin, invDir)) continue;

        if (node.IsLeaf()) {
            for (i32 i = 0; i < node.ObjectCount; i++) {
                u32 objIdx = (u32)(node.ObjectIndex + i);
                if (objIdx < m_Objects.size() && m_Objects[objIdx].Bounds.RayIntersect(origin, invDir)) {
                    results.push_back(m_Objects[objIdx].UserData);
                }
            }
        } else {
            stack.push_back(node.Left);
            stack.push_back(node.Right);
        }
    }
}

void BVH::QueryFrustum(const glm::vec4 planes[6],
                         std::vector<u32>& results) const {
    if (m_Nodes.empty()) return;

    std::vector<i32> stack;
    stack.reserve(64);
    stack.push_back(0);

    while (!stack.empty()) {
        i32 idx = stack.back();
        stack.pop_back();
        if (idx < 0 || idx >= (i32)m_Nodes.size()) continue;

        const auto& node = m_Nodes[idx];
        if (!FrustumIntersectsAABB(planes, node.Bounds)) continue;

        if (node.IsLeaf()) {
            for (i32 i = 0; i < node.ObjectCount; i++) {
                u32 objIdx = (u32)(node.ObjectIndex + i);
                if (objIdx < m_Objects.size()) {
                    results.push_back(m_Objects[objIdx].UserData);
                }
            }
        } else {
            stack.push_back(node.Left);
            stack.push_back(node.Right);
        }
    }
}

bool BVH::FrustumIntersectsAABB(const glm::vec4 planes[6], const AABB& box) {
    for (int i = 0; i < 6; i++) {
        glm::vec3 pVertex;
        pVertex.x = (planes[i].x > 0) ? box.Max.x : box.Min.x;
        pVertex.y = (planes[i].y > 0) ? box.Max.y : box.Min.y;
        pVertex.z = (planes[i].z > 0) ? box.Max.z : box.Min.z;
        if (glm::dot(glm::vec3(planes[i]), pVertex) + planes[i].w < 0) {
            return false;
        }
    }
    return true;
}

} // namespace Engine
