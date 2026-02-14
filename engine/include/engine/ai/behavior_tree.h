#pragma once

#include "engine/core/types.h"

#include <glm/glm.hpp>

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>

namespace Engine {

// ── 行为树节点状态 ──────────────────────────────────────────

enum class BTStatus { Success, Failure, Running };

// ── 行为树节点基类 ──────────────────────────────────────────

class BTNode {
public:
    virtual ~BTNode() = default;
    virtual BTStatus Tick(f32 dt) = 0;
    virtual void Reset() {}
    
    void SetName(const std::string& name) { m_Name = name; }
    const std::string& GetName() const { return m_Name; }

protected:
    std::string m_Name;
};

// ── 组合节点 ────────────────────────────────────────────────

class BTSequence : public BTNode {
public:
    void AddChild(Ref<BTNode> child) { m_Children.push_back(child); }

    BTStatus Tick(f32 dt) override {
        for (auto& child : m_Children) {
            BTStatus status = child->Tick(dt);
            if (status != BTStatus::Success) return status;
        }
        return BTStatus::Success;
    }

    void Reset() override {
        for (auto& child : m_Children) child->Reset();
    }

private:
    std::vector<Ref<BTNode>> m_Children;
};

class BTSelector : public BTNode {
public:
    void AddChild(Ref<BTNode> child) { m_Children.push_back(child); }

    BTStatus Tick(f32 dt) override {
        for (auto& child : m_Children) {
            BTStatus status = child->Tick(dt);
            if (status != BTStatus::Failure) return status;
        }
        return BTStatus::Failure;
    }

    void Reset() override {
        for (auto& child : m_Children) child->Reset();
    }

private:
    std::vector<Ref<BTNode>> m_Children;
};

// ── 装饰器 ──────────────────────────────────────────────────

class BTInverter : public BTNode {
public:
    BTInverter(Ref<BTNode> child) : m_Child(child) {}

    BTStatus Tick(f32 dt) override {
        BTStatus s = m_Child->Tick(dt);
        if (s == BTStatus::Success) return BTStatus::Failure;
        if (s == BTStatus::Failure) return BTStatus::Success;
        return BTStatus::Running;
    }

private:
    Ref<BTNode> m_Child;
};

class BTRepeater : public BTNode {
public:
    BTRepeater(Ref<BTNode> child, i32 maxRepeats = -1)
        : m_Child(child), m_MaxRepeats(maxRepeats) {}

    BTStatus Tick(f32 dt) override {
        if (m_MaxRepeats > 0 && m_Count >= m_MaxRepeats)
            return BTStatus::Success;

        BTStatus s = m_Child->Tick(dt);
        if (s == BTStatus::Running) return BTStatus::Running;
        m_Count++;
        m_Child->Reset();
        return (m_MaxRepeats > 0 && m_Count >= m_MaxRepeats) 
            ? BTStatus::Success : BTStatus::Running;
    }

    void Reset() override { m_Count = 0; m_Child->Reset(); }

private:
    Ref<BTNode> m_Child;
    i32 m_MaxRepeats;
    i32 m_Count = 0;
};

// ── 动作/条件（Lambda 包装）────────────────────────────────

class BTAction : public BTNode {
public:
    using ActionFunc = std::function<BTStatus(f32)>;
    BTAction(const std::string& name, ActionFunc func) 
        : m_Func(func) { m_Name = name; }

    BTStatus Tick(f32 dt) override { return m_Func(dt); }

private:
    ActionFunc m_Func;
};

class BTCondition : public BTNode {
public:
    using CondFunc = std::function<bool()>;
    BTCondition(const std::string& name, CondFunc func)
        : m_Func(func) { m_Name = name; }

    BTStatus Tick(f32 dt) override {
        return m_Func() ? BTStatus::Success : BTStatus::Failure;
    }

private:
    CondFunc m_Func;
};

// ── 行为树 ──────────────────────────────────────────────────

class BehaviorTree {
public:
    void SetRoot(Ref<BTNode> root) { m_Root = root; }
    BTStatus Tick(f32 dt) { return m_Root ? m_Root->Tick(dt) : BTStatus::Failure; }
    void Reset() { if (m_Root) m_Root->Reset(); }

private:
    Ref<BTNode> m_Root;
};

// ── A* 寻路 ─────────────────────────────────────────────────

struct NavNode {
    i32 X, Y;
    f32 GCost = 0, HCost = 0;
    f32 FCost() const { return GCost + HCost; }
    NavNode* Parent = nullptr;
    bool Walkable = true;
};

class NavGrid {
public:
    NavGrid() = default;
    NavGrid(u32 width, u32 height, f32 cellSize = 1.0f);

    void SetWalkable(i32 x, i32 y, bool walkable);
    bool IsWalkable(i32 x, i32 y) const;

    /// A* 寻路 → 返回路径点列表 (世界坐标)
    std::vector<glm::vec3> FindPath(const glm::vec3& start, const glm::vec3& end);

    u32 GetWidth() const { return m_Width; }
    u32 GetHeight() const { return m_Height; }
    f32 GetCellSize() const { return m_CellSize; }

private:
    u32 m_Width = 0, m_Height = 0;
    f32 m_CellSize = 1.0f;
    std::vector<NavNode> m_Nodes;
    
    NavNode* GetNode(i32 x, i32 y);
    std::vector<NavNode*> GetNeighbors(NavNode* node);
    f32 Heuristic(NavNode* a, NavNode* b);
};

} // namespace Engine
