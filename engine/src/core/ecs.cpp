#include "engine/core/ecs.h"
#include "engine/core/components.h"

namespace Engine {

// ── ECSWorld 非模板方法实现 ──────────────────────────────────

Entity ECSWorld::CreateEntity(const std::string& name) {
    Entity e;

    if (!m_FreeList.empty()) {
        // 回收已销毁实体的 ID
        e = m_FreeList.back();
        m_FreeList.pop_back();
        // Generation 已在 DestroyEntity 中递增过 (偶→奇)
        m_Generation[e]++;  // 偶数→奇数 = 存活
    } else {
        // 分配全新 ID
        e = m_NextEntity++;
        if (e >= m_Generation.size()) {
            m_Generation.resize((size_t)e + 1, 0);
        }
        m_Generation[e] = 1;  // Generation 1 = 第一次存活
    }

    AddComponent<TagComponent>(e).Name = name;
    m_Entities.push_back(e);
    return e;
}

void ECSWorld::DestroyEntity(Entity e) {
    // 移除所有组件
    for (auto& [type, pool] : m_Pools) {
        pool->Remove(e);
    }

    // 从存活列表移除
    std::erase(m_Entities, e);

    // 递增 Generation (奇→偶 = 已销毁)
    if (e < m_Generation.size()) {
        m_Generation[e]++;  // 奇数→偶数 = 已销毁
    }

    // 放入回收列表
    m_FreeList.push_back(e);
}

void ECSWorld::SetParent(Entity child, Entity parent) {
    auto* childTr = GetComponent<TransformComponent>(child);
    if (!childTr) return;

    // 先从旧父节点移除
    if (childTr->Parent != INVALID_ENTITY) {
        auto* oldParentTr = GetComponent<TransformComponent>(childTr->Parent);
        if (oldParentTr) {
            std::erase(oldParentTr->Children, child);
        }
    }

    childTr->Parent = parent;
    childTr->WorldMatrixDirty = true;

    // 添加到新父节点
    if (parent != INVALID_ENTITY) {
        auto* parentTr = GetComponent<TransformComponent>(parent);
        if (parentTr) {
            parentTr->Children.push_back(child);
        }
    }
}

std::vector<Entity> ECSWorld::GetRootEntities() {
    std::vector<Entity> roots;
    for (auto e : m_Entities) {
        auto* tr = GetComponent<TransformComponent>(e);
        if (!tr || tr->Parent == INVALID_ENTITY) {
            roots.push_back(e);
        }
    }
    return roots;
}

} // namespace Engine
