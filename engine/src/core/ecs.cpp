#include "engine/core/ecs.h"
#include "engine/core/components.h"

namespace Engine {

// ── ECSWorld 非模板方法实现 ──────────────────────────────────

Entity ECSWorld::CreateEntity(const std::string& name) {
    Entity e = m_NextEntity++;
    AddComponent<TagComponent>(e).Name = name;
    m_Entities.push_back(e);
    return e;
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
