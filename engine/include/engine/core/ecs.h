#pragma once

#include "engine/core/types.h"
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
constexpr Entity INVALID_ENTITY = 0;

// ── Component 基类 ──────────────────────────────────────────

struct Component {
    virtual ~Component() = default;
};

// ── 常用组件 ────────────────────────────────────────────────

struct TransformComponent : public Component {
    f32 X = 0, Y = 0, Z = 0;
    f32 RotX = 0, RotY = 0, RotZ = 0;
    f32 ScaleX = 1, ScaleY = 1, ScaleZ = 1;
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

struct RenderComponent : public Component {
    std::string MeshType = "cube";  // cube, sphere, plane, obj
    std::string ObjPath;
    f32 ColorR = 1, ColorG = 1, ColorB = 1;
    f32 Shininess = 32.0f;
};

// ── ComponentPool —— 类型擦除的组件存储 ─────────────────────

class ComponentPool {
public:
    template<typename T>
    T* Get(Entity e) {
        auto it = m_Data.find(e);
        if (it == m_Data.end()) return nullptr;
        return static_cast<T*>(it->second.get());
    }

    template<typename T, typename... Args>
    T* Add(Entity e, Args&&... args) {
        auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = ptr.get();
        m_Data[e] = std::move(ptr);
        return raw;
    }

    void Remove(Entity e) { m_Data.erase(e); }
    bool Has(Entity e) const { return m_Data.count(e) > 0; }

    auto begin() { return m_Data.begin(); }
    auto end() { return m_Data.end(); }

private:
    std::unordered_map<Entity, Scope<Component>> m_Data;
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
            pool.Remove(e);
        }
        std::erase(m_Entities, e);
    }

    /// 添加组件
    template<typename T, typename... Args>
    T& AddComponent(Entity e, Args&&... args) {
        auto& pool = GetPool<T>();
        return *pool.template Add<T>(e, std::forward<Args>(args)...);
    }

    /// 获取组件（可能为 nullptr）
    template<typename T>
    T* GetComponent(Entity e) {
        auto& pool = GetPool<T>();
        return pool.template Get<T>(e);
    }

    /// 是否拥有某组件
    template<typename T>
    bool HasComponent(Entity e) {
        return GetPool<T>().Has(e);
    }

    /// 遍历所有拥有指定组件的实体
    template<typename T>
    void ForEach(std::function<void(Entity, T&)> fn) {
        auto& pool = GetPool<T>();
        for (auto& [e, comp] : pool) {
            fn(e, *static_cast<T*>(comp.get()));
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

private:
    template<typename T>
    ComponentPool& GetPool() {
        auto typeIdx = std::type_index(typeid(T));
        return m_Pools[typeIdx];
    }

    Entity m_NextEntity = 1;
    std::vector<Entity> m_Entities;
    std::unordered_map<std::type_index, ComponentPool> m_Pools;
    std::vector<Scope<System>> m_Systems;
};

// ── 内置系统 ────────────────────────────────────────────────

/// 运动系统：根据速度更新位置
class MovementSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override {
        for (auto e : world.GetEntities()) {
            auto* transform = world.GetComponent<TransformComponent>(e);
            auto* velocity = world.GetComponent<VelocityComponent>(e);
            if (transform && velocity) {
                transform->X += velocity->VX * dt;
                transform->Y += velocity->VY * dt;
                transform->Z += velocity->VZ * dt;
            }
        }
    }
    const char* GetName() const override { return "MovementSystem"; }
};

} // namespace Engine
