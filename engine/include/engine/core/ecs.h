#pragma once

// ── ECS 核心 ────────────────────────────────────────────────
// 纯 ECS 容器: Entity、Component、ComponentArray<T>、System、ECSWorld
// 组件定义见 components.h，内置系统见 systems.h

#include "engine/core/types.h"
#include "engine/core/job_system.h"
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <functional>
#include <string>
#include <algorithm>

namespace Engine {

// ── 前向声明 ────────────────────────────────────────────────
class ECSWorld;

// ── Entity —— 只是一个 ID ───────────────────────────────────

using Entity = u32;
constexpr Entity INVALID_ENTITY = ~Entity(0);  // UINT32_MAX

// ── Component 基类 ──────────────────────────────────────────

struct Component {
    virtual ~Component() = default;
};

// ── IComponentPool —— 类型擦除的组件池基类 ──────────────────

class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual void Remove(Entity e) = 0;
    virtual bool Has(Entity e) const = 0;
};

// ── ComponentArray<T> —— Sparse Set SoA 组件存储 ────────────
//
// 数据布局:
//   m_Dense   : [T, T, T, ...]       — 组件数据紧密排列（缓存友好）
//   m_Entities: [e3, e1, e7, ...]     — 与 m_Dense 对应的 Entity ID
//   m_Sparse  : [-, 1, -, 0, -, -, -, 2]  — Entity → Dense 索引映射
//
// 删除操作使用 swap-and-pop 保持紧密排列。

template<typename T>
class ComponentArray : public IComponentPool {
public:
    /// 获取组件指针（O(1) 数组下标）
    T* Get(Entity e) {
        if (e >= m_Sparse.size() || m_Sparse[e] == INVALID_INDEX)
            return nullptr;
        return &m_Dense[m_Sparse[e]];
    }

    /// 添加组件（如已存在则覆盖）
    template<typename... Args>
    T* Add(Entity e, Args&&... args) {
        if (e >= m_Sparse.size())
            m_Sparse.resize((size_t)e + 1, INVALID_INDEX);

        if (m_Sparse[e] != INVALID_INDEX) {
            // 已存在 — 原地重构
            m_Dense[m_Sparse[e]] = T(std::forward<Args>(args)...);
            return &m_Dense[m_Sparse[e]];
        }

        u32 idx = (u32)m_Dense.size();
        m_Dense.emplace_back(std::forward<Args>(args)...);
        m_Entities.push_back(e);
        m_Sparse[e] = idx;
        return &m_Dense[idx];
    }

    /// 删除组件（swap-and-pop 保持紧密）
    void Remove(Entity e) override {
        if (e >= m_Sparse.size() || m_Sparse[e] == INVALID_INDEX)
            return;

        u32 idx = m_Sparse[e];
        u32 lastIdx = (u32)m_Dense.size() - 1;

        if (idx != lastIdx) {
            Entity lastEntity = m_Entities[lastIdx];
            m_Dense[idx] = std::move(m_Dense[lastIdx]);
            m_Entities[idx] = lastEntity;
            m_Sparse[lastEntity] = idx;
        }

        m_Dense.pop_back();
        m_Entities.pop_back();
        m_Sparse[e] = INVALID_INDEX;
    }

    bool Has(Entity e) const override {
        return e < m_Sparse.size() && m_Sparse[e] != INVALID_INDEX;
    }

    /// SoA 遍历支持
    u32  Size()  const { return (u32)m_Dense.size(); }
    T&   Data(u32 i)   { return m_Dense[i]; }
    Entity GetEntity(u32 i) const { return m_Entities[i]; }

    /// 直接访问底层数组（高性能批处理）
    T*       RawData()     { return m_Dense.data(); }
    Entity*  RawEntities() { return m_Entities.data(); }

private:
    static constexpr u32 INVALID_INDEX = ~0u;

    std::vector<T>      m_Dense;      // 组件数据 — 紧密排列（SoA 核心）
    std::vector<Entity> m_Entities;   // 与 m_Dense 对应的实体 ID
    std::vector<u32>    m_Sparse;     // Entity → Dense 索引 （稀疏数组）
};

// ── System 基类 ─────────────────────────────────────────────

class System {
public:
    virtual ~System() = default;
    virtual void Update(ECSWorld& world, f32 dt) = 0;
    virtual const char* GetName() const = 0;
};

// ── ECS World ───────────────────────────────────────────────

class ECSWorld {
public:
    ECSWorld() = default;

    /// 创建新实体 (自动附加 TagComponent — 需要 components.h 已包含)
    Entity CreateEntity(const std::string& name = "Entity");

    /// 销毁实体 (Generation 递增，ID 回收到 free list)
    void DestroyEntity(Entity e);

    /// 检查实体是否仍然存活 (防止悬垂引用)
    bool IsAlive(Entity e) const {
        return e < m_Generation.size() && m_Generation[e] > 0
            && std::find(m_Entities.begin(), m_Entities.end(), e) != m_Entities.end();
    }

    /// 获取实体当前 Generation (0 = 从未使用或已销毁)
    u32 GetGeneration(Entity e) const {
        return (e < m_Generation.size()) ? m_Generation[e] : 0;
    }

    /// 添加组件
    template<typename T, typename... Args>
    T& AddComponent(Entity e, Args&&... args) {
        auto& pool = GetPool<T>();
        return *pool.Add(e, std::forward<Args>(args)...);
    }

    /// 获取组件（可能为 nullptr）
    template<typename T>
    T* GetComponent(Entity e) {
        auto& pool = GetPool<T>();
        return pool.Get(e);
    }

    /// 是否拥有某组件
    template<typename T>
    bool HasComponent(Entity e) {
        return GetPool<T>().Has(e);
    }

    /// 遍历所有拥有指定组件的实体（SoA 线性扫描，极致缓存命中）
    template<typename T, typename Func>
    void ForEach(Func&& fn) {
        auto& pool = GetPool<T>();
        u32 count = pool.Size();
        for (u32 i = 0; i < count; i++) {
            fn(pool.GetEntity(i), pool.Data(i));
        }
    }

    /// 遍历同时拥有 T1 和 T2 两个组件的实体
    /// 以较小的组件池为驱动端，减少无效查找
    template<typename T1, typename T2, typename Func>
    void ForEach2(Func&& fn) {
        auto& pool1 = GetPool<T1>();
        auto& pool2 = GetPool<T2>();

        // 选择较小的池作为驱动端
        if (pool1.Size() <= pool2.Size()) {
            u32 count = pool1.Size();
            for (u32 i = 0; i < count; i++) {
                Entity e = pool1.GetEntity(i);
                T2* c2 = pool2.Get(e);
                if (c2) fn(e, pool1.Data(i), *c2);
            }
        } else {
            u32 count = pool2.Size();
            for (u32 i = 0; i < count; i++) {
                Entity e = pool2.GetEntity(i);
                T1* c1 = pool1.Get(e);
                if (c1) fn(e, *c1, pool2.Data(i));
            }
        }
    }

    /// 遍历同时拥有 T1, T2, T3 三个组件的实体
    template<typename T1, typename T2, typename T3, typename Func>
    void ForEach3(Func&& fn) {
        auto& pool1 = GetPool<T1>();
        auto& pool2 = GetPool<T2>();
        auto& pool3 = GetPool<T3>();

        // 以最小池驱动
        u32 minSize = std::min({pool1.Size(), pool2.Size(), pool3.Size()});
        if (minSize == pool1.Size()) {
            u32 count = pool1.Size();
            for (u32 i = 0; i < count; i++) {
                Entity e = pool1.GetEntity(i);
                T2* c2 = pool2.Get(e);
                T3* c3 = pool3.Get(e);
                if (c2 && c3) fn(e, pool1.Data(i), *c2, *c3);
            }
        } else if (minSize == pool2.Size()) {
            u32 count = pool2.Size();
            for (u32 i = 0; i < count; i++) {
                Entity e = pool2.GetEntity(i);
                T1* c1 = pool1.Get(e);
                T3* c3 = pool3.Get(e);
                if (c1 && c3) fn(e, *c1, pool2.Data(i), *c3);
            }
        } else {
            u32 count = pool3.Size();
            for (u32 i = 0; i < count; i++) {
                Entity e = pool3.GetEntity(i);
                T1* c1 = pool1.Get(e);
                T2* c2 = pool2.Get(e);
                if (c1 && c2) fn(e, *c1, *c2, pool3.Data(i));
            }
        }
    }

    /// 并行遍历所有拥有指定组件的实体（多线程分块）
    /// 注意: fn 内部不可创建/销毁实体、不可添加/删除组件
    template<typename T, typename Func>
    void ParallelForEach(Func&& fn) {
        auto& pool = GetPool<T>();
        u32 count = pool.Size();
        JobSystem::ParallelFor(0u, count, [&](u32 i) {
            fn(pool.GetEntity(i), pool.Data(i));
        });
    }

    /// 注册系统
    template<typename T, typename... Args>
    T& AddSystem(Args&&... args) {
        auto sys = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *sys;
        m_Systems.push_back(std::move(sys));
        return ref;
    }

    /// 更新所有系统
    void Update(f32 dt) {
        for (auto& sys : m_Systems) {
            sys->Update(*this, dt);
        }
    }

    /// 获取所有实体
    const std::vector<Entity>& GetEntities() const { return m_Entities; }
    u32 GetEntityCount() const { return (u32)m_Entities.size(); }

    /// 设置实体的父节点（自动维护双向引用）— 需要 TransformComponent
    void SetParent(Entity child, Entity parent);

    /// 获取所有根实体（无父节点的实体）
    std::vector<Entity> GetRootEntities();

    /// 获取某类型的组件数组（高级用法，直接访问 SoA 数据）
    template<typename T>
    ComponentArray<T>& GetComponentArray() { return GetPool<T>(); }

private:
    template<typename T>
    ComponentArray<T>& GetPool() {
        auto typeIdx = std::type_index(typeid(T));
        auto it = m_Pools.find(typeIdx);
        if (it != m_Pools.end()) {
            return *static_cast<ComponentArray<T>*>(it->second.get());
        }
        auto pool = std::make_unique<ComponentArray<T>>();
        auto& ref = *pool;
        m_Pools[typeIdx] = std::move(pool);
        return ref;
    }

    Entity m_NextEntity = 1;
    std::vector<Entity>  m_Entities;     // 当前存活实体列表
    std::vector<u32>     m_Generation;   // 每个 index 的代数 (奇数=存活，偶数=已销毁)
    std::vector<Entity>  m_FreeList;     // 可复用的已销毁实体 ID
    std::unordered_map<std::type_index, Scope<IComponentPool>> m_Pools;
    std::vector<Scope<System>> m_Systems;
};

} // namespace Engine

// ── 向后兼容 — 包含拆分出的组件和系统定义 ───────────────────
// 现有代码只 #include "ecs.h" 即可获得完整功能
#include "engine/core/components.h"
#include "engine/core/systems.h"

