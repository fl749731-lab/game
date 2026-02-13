#pragma once

#include "engine/core/types.h"
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <functional>
#include <string>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Engine {

// ── 前向声明 ────────────────────────────────────────────────
class ECSWorld;

// ── Entity —— 只是一个 ID ───────────────────────────────────

using Entity = u32;
constexpr Entity INVALID_ENTITY = 0;

// ── Component 基类 ──────────────────────────────────────────

struct Component {
    virtual ~Component() = default;
};

// ── 常用组件 ────────────────────────────────────────────────

struct TransformComponent : public Component {
    // ── 局部变换（相对于父节点）──────────────────────────────
    f32 X = 0, Y = 0, Z = 0;
    f32 RotX = 0, RotY = 0, RotZ = 0;
    f32 ScaleX = 1, ScaleY = 1, ScaleZ = 1;

    // ── 层级关系 ─────────────────────────────────────────────
    Entity Parent = INVALID_ENTITY;
    std::vector<Entity> Children;

    // ── 世界矩阵缓存（由 TransformSystem 每帧更新）──────────
    glm::mat4 WorldMatrix = glm::mat4(1.0f);
    bool WorldMatrixDirty = true;

    // ── 局部矩阵构建 (TRS) ──────────────────────────────────
    glm::mat4 GetLocalMatrix() const {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), {X, Y, Z});
        m = glm::rotate(m, glm::radians(RotY), {0, 1, 0});
        m = glm::rotate(m, glm::radians(RotX), {1, 0, 0});
        m = glm::rotate(m, glm::radians(RotZ), {0, 0, 1});
        m = glm::scale(m, {ScaleX, ScaleY, ScaleZ});
        return m;
    }

    // ── 便捷方法 ─────────────────────────────────────────────
    glm::vec3 GetPosition() const { return {X, Y, Z}; }
    void SetPosition(const glm::vec3& p) { X = p.x; Y = p.y; Z = p.z; }
    glm::vec3 GetScale() const { return {ScaleX, ScaleY, ScaleZ}; }
    void SetScale(const glm::vec3& s) { ScaleX = s.x; ScaleY = s.y; ScaleZ = s.z; }
    void SetScale(f32 uniform) { ScaleX = ScaleY = ScaleZ = uniform; }

    /// 获取世界位置（从缓存的世界矩阵提取）
    glm::vec3 GetWorldPosition() const { return glm::vec3(WorldMatrix[3]); }
};

struct TagComponent : public Component {
    std::string Name = "Entity";
};

struct HealthComponent : public Component {
    f32 Current = 100.0f;
    f32 Max = 100.0f;
};

struct VelocityComponent : public Component {
    f32 VX = 0, VY = 0, VZ = 0;
};

struct AIComponent : public Component {
    std::string ScriptModule = "default_ai";
    std::string State = "Idle";
    f32 DetectRange = 10.0f;
    f32 AttackRange = 2.0f;
};

// ── 小队组件 ────────────────────────────────────────────────

struct SquadComponent : public Component {
    u32 SquadID = 0;                    // 所属小队 ID（0 = 无小队）
    u32 LeaderEntity = INVALID_ENTITY;  // 队长实体 ID
    u32 CommanderEntity = INVALID_ENTITY; // 指挥官实体 ID
    std::string Role = "soldier";       // "commander" | "leader" | "soldier"
    std::string CurrentOrder;           // 当前接收到的命令（JSON字符串）
    std::string OrderStatus = "idle";   // "idle" | "executing" | "completed" | "failed"
};

struct ScriptComponent : public Component {
    std::string ScriptModule;                                  // Python 模块名
    bool Initialized = false;                                  // on_create 是否已调用
    bool Enabled = true;                                       // 是否启用
    std::unordered_map<std::string, f32> FloatVars;           // 脚本自定义浮点变量
    std::unordered_map<std::string, std::string> StringVars;  // 脚本自定义字符串变量
};

struct RenderComponent : public Component {
    std::string MeshType = "cube";  // cube, sphere, plane, obj
    std::string ObjPath;
    // 兼容 — 新代码请使用 MaterialComponent
    f32 ColorR = 1, ColorG = 1, ColorB = 1;
    f32 Shininess = 32.0f;
};

struct MaterialComponent : public Component {
    f32 DiffuseR = 0.8f, DiffuseG = 0.8f, DiffuseB = 0.8f;
    f32 SpecularR = 0.8f, SpecularG = 0.8f, SpecularB = 0.8f;
    f32 Shininess = 32.0f;
    f32 Roughness = 0.5f;       // PBR 粗糙度 (0=光滑 1=粗糙)
    f32 Metallic  = 0.0f;       // PBR 金属度 (0=非金属 1=金属)
    std::string TextureName;    // 空 = 无纹理
    std::string NormalMapName;  // 空 = 无法线贴图
    bool Emissive = false;      // 自发光物体 (跳过光照计算)
    f32 EmissiveR = 1.0f, EmissiveG = 1.0f, EmissiveB = 1.0f;
    f32 EmissiveIntensity = 1.0f;
};

/// 旋转动画组件 — 支持多轴自动旋转（替代硬编码的 CenterCube 逻辑）
struct RotationAnimComponent : public Component {
    f32 SpeedY = 0.6f;  // 绕 Y 轴旋转速度 (rad/s)
    f32 SpeedX = 0.2f;  // 绕 X 轴旋转速度 (rad/s)
    f32 SpeedZ = 0.0f;  // 绕 Z 轴旋转速度 (rad/s)
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

    /// 创建新实体
    Entity CreateEntity(const std::string& name = "Entity") {
        Entity e = m_NextEntity++;
        AddComponent<TagComponent>(e).Name = name;
        m_Entities.push_back(e);
        return e;
    }

    /// 销毁实体
    void DestroyEntity(Entity e) {
        for (auto& [type, pool] : m_Pools) {
            pool->Remove(e);
        }
        std::erase(m_Entities, e);
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

    /// 设置实体的父节点（自动维护双向引用）
    void SetParent(Entity child, Entity parent) {
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

    /// 获取所有根实体（无父节点的实体）
    std::vector<Entity> GetRootEntities() {
        std::vector<Entity> roots;
        for (auto e : m_Entities) {
            auto* tr = GetComponent<TransformComponent>(e);
            if (!tr || tr->Parent == INVALID_ENTITY) {
                roots.push_back(e);
            }
        }
        return roots;
    }

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
    std::vector<Entity> m_Entities;
    std::unordered_map<std::type_index, Scope<IComponentPool>> m_Pools;
    std::vector<Scope<System>> m_Systems;
};

// ── 内置系统 ────────────────────────────────────────────────

/// 运动系统：根据速度更新位置（SoA 直接遍历）
class MovementSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override {
        auto& pool = world.GetComponentArray<VelocityComponent>();
        u32 count = pool.Size();
        for (u32 i = 0; i < count; i++) {
            VelocityComponent& vel = pool.Data(i);
            auto* tr = world.GetComponent<TransformComponent>(pool.GetEntity(i));
            if (!tr) continue;
            tr->X += vel.VX * dt;
            tr->Y += vel.VY * dt;
            tr->Z += vel.VZ * dt;
        }
    }
    const char* GetName() const override { return "MovementSystem"; }
};

/// 生命周期组件 — 倒计时后自动销毁
struct LifetimeComponent : public Component {
    f32 TimeRemaining = 5.0f;
};

/// 生命周期系统 — 每帧更新倒计时，到期则标记删除
class LifetimeSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override {
        m_ToDestroy.clear();
        auto& pool = world.GetComponentArray<LifetimeComponent>();
        u32 count = pool.Size();
        for (u32 i = 0; i < count; i++) {
            LifetimeComponent& lc = pool.Data(i);
            lc.TimeRemaining -= dt;
            if (lc.TimeRemaining <= 0)
                m_ToDestroy.push_back(pool.GetEntity(i));
        }
        for (auto e : m_ToDestroy) {
            world.DestroyEntity(e);
        }
    }
    const char* GetName() const override { return "LifetimeSystem"; }
private:
    std::vector<Entity> m_ToDestroy;
};

/// Transform 层级系统 — 递归计算世界矩阵
class TransformSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override {
        (void)dt;
        // 只处理根实体（无父节点），递归向下计算
        for (auto e : world.GetEntities()) {
            auto* tr = world.GetComponent<TransformComponent>(e);
            if (tr && tr->Parent == INVALID_ENTITY) {
                UpdateWorldMatrix(world, e, glm::mat4(1.0f));
            }
        }
    }
    const char* GetName() const override { return "TransformSystem"; }

private:
    void UpdateWorldMatrix(ECSWorld& world, Entity e, const glm::mat4& parentWorld) {
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) return;

        tr->WorldMatrix = parentWorld * tr->GetLocalMatrix();
        tr->WorldMatrixDirty = false;

        for (Entity child : tr->Children) {
            UpdateWorldMatrix(world, child, tr->WorldMatrix);
        }
    }
};

} // namespace Engine
