#include "engine/ai/behavior_tree.h"
#include "engine/core/log.h"

#include <algorithm>
#include <queue>
#include <cmath>
#include <unordered_set>

namespace Engine {

// ── NavGrid 实现 ────────────────────────────────────────────

NavGrid::NavGrid(u32 width, u32 height, f32 cellSize)
    : m_Width(width), m_Height(height), m_CellSize(cellSize)
{
    m_Nodes.resize(width * height);
    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            NavNode& node = m_Nodes[y * width + x];
            node.X = (i32)x;
            node.Y = (i32)y;
            node.Walkable = true;
        }
    }
    LOG_INFO("[NavGrid] 创建 %ux%u 导航网格 (格子大小: %.1f)", width, height, cellSize);
}

void NavGrid::SetWalkable(i32 x, i32 y, bool walkable) {
    if (x < 0 || x >= (i32)m_Width || y < 0 || y >= (i32)m_Height) return;
    m_Nodes[y * m_Width + x].Walkable = walkable;
}

bool NavGrid::IsWalkable(i32 x, i32 y) const {
    if (x < 0 || x >= (i32)m_Width || y < 0 || y >= (i32)m_Height) return false;
    return m_Nodes[y * m_Width + x].Walkable;
}

NavNode* NavGrid::GetNode(i32 x, i32 y) {
    if (x < 0 || x >= (i32)m_Width || y < 0 || y >= (i32)m_Height) return nullptr;
    return &m_Nodes[y * m_Width + x];
}

std::vector<NavNode*> NavGrid::GetNeighbors(NavNode* node) {
    std::vector<NavNode*> neighbors;
    static const i32 dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    static const i32 dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int i = 0; i < 8; i++) {
        NavNode* n = GetNode(node->X + dx[i], node->Y + dy[i]);
        if (n && n->Walkable) {
            // 对角线移动检查：两个相邻正交格都必须可走
            if (dx[i] != 0 && dy[i] != 0) {
                if (!IsWalkable(node->X + dx[i], node->Y) ||
                    !IsWalkable(node->X, node->Y + dy[i]))
                    continue;
            }
            neighbors.push_back(n);
        }
    }
    return neighbors;
}

f32 NavGrid::Heuristic(NavNode* a, NavNode* b) {
    // 八方向距离（对角线代价 √2）
    f32 dx = std::abs((f32)(a->X - b->X));
    f32 dy = std::abs((f32)(a->Y - b->Y));
    return (dx + dy) + (1.414f - 2.0f) * std::min(dx, dy);
}

std::vector<glm::vec3> NavGrid::FindPath(const glm::vec3& start, const glm::vec3& end) {
    // 世界坐标 → 网格坐标
    i32 sx = (i32)(start.x / m_CellSize);
    i32 sy = (i32)(start.z / m_CellSize);
    i32 ex = (i32)(end.x / m_CellSize);
    i32 ey = (i32)(end.z / m_CellSize);

    NavNode* startNode = GetNode(sx, sy);
    NavNode* endNode = GetNode(ex, ey);
    if (!startNode || !endNode || !startNode->Walkable || !endNode->Walkable) {
        return {};
    }

    // 重置所有节点
    for (auto& node : m_Nodes) {
        node.GCost = 1e9f;
        node.HCost = 0;
        node.Parent = nullptr;
    }

    // A* 开放列表 (优先队列)
    auto cmp = [](NavNode* a, NavNode* b) { return a->FCost() > b->FCost(); };
    std::priority_queue<NavNode*, std::vector<NavNode*>, decltype(cmp)> openSet(cmp);

    struct PairHash {
        size_t operator()(const std::pair<i32,i32>& p) const {
            return std::hash<i32>()(p.first) ^ (std::hash<i32>()(p.second) << 16);
        }
    };
    std::unordered_set<std::pair<i32,i32>, PairHash> closedSet;

    startNode->GCost = 0;
    startNode->HCost = Heuristic(startNode, endNode);
    openSet.push(startNode);

    while (!openSet.empty()) {
        NavNode* current = openSet.top();
        openSet.pop();

        if (current == endNode) {
            // 回溯路径
            std::vector<glm::vec3> path;
            NavNode* node = endNode;
            while (node) {
                f32 wx = (node->X + 0.5f) * m_CellSize;
                f32 wz = (node->Y + 0.5f) * m_CellSize;
                path.push_back({wx, 0.0f, wz});
                node = node->Parent;
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        closedSet.insert({current->X, current->Y});

        for (NavNode* neighbor : GetNeighbors(current)) {
            if (closedSet.count({neighbor->X, neighbor->Y})) continue;

            f32 dx = std::abs((f32)(neighbor->X - current->X));
            f32 dy = std::abs((f32)(neighbor->Y - current->Y));
            f32 moveCost = (dx + dy > 1.5f) ? 1.414f : 1.0f;  // 对角 vs 正交
            f32 newG = current->GCost + moveCost;

            if (newG < neighbor->GCost) {
                neighbor->GCost = newG;
                neighbor->HCost = Heuristic(neighbor, endNode);
                neighbor->Parent = current;
                openSet.push(neighbor);
            }
        }
    }

    return {};  // 无可达路径
}

} // namespace Engine
