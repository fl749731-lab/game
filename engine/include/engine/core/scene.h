#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"
#include "engine/core/event.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/light.h"

#include <string>
#include <vector>
#include <functional>

namespace Engine {

// ── 场景 ────────────────────────────────────────────────────
// 封装一个完整的 ECS 世界 + 光照 + 摄像机

class Scene {
public:
    Scene(const std::string& name = "Untitled");
    ~Scene() = default;

    /// 名称
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    /// ECS World 访问
    ECSWorld& GetWorld() { return m_World; }
    const ECSWorld& GetWorld() const { return m_World; }

    /// 光照
    DirectionalLight& GetDirLight() { return m_DirLight; }
    std::vector<PointLight>& GetPointLights() { return m_PointLights; }
    PointLight& AddPointLight() { m_PointLights.emplace_back(); return m_PointLights.back(); }

    /// 更新 (调用所有 System)
    void Update(f32 dt);

    /// 实体快捷创建
    Entity CreateEntity(const std::string& name = "Entity");
    void DestroyEntity(Entity e);
    u32 GetEntityCount() const { return m_World.GetEntityCount(); }

private:
    std::string m_Name;
    ECSWorld m_World;
    DirectionalLight m_DirLight;
    std::vector<PointLight> m_PointLights;
};

// ── 场景管理器 ──────────────────────────────────────────────
// 支持多场景 → 切换 → 栈式管理

class SceneManager {
public:
    static void PushScene(Ref<Scene> scene);
    static void PopScene();
    static Ref<Scene> GetActiveScene();
    static void Update(f32 dt);
    static void Clear();

private:
    static std::vector<Ref<Scene>> s_SceneStack;
};

} // namespace Engine
