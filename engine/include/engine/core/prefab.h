#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"

#include <string>
#include <vector>
#include <functional>

namespace Engine {

// ── 组件快照 (序列化用) ──────────────────────────────────────

struct ComponentSnapshot {
    std::string TypeName;    // 组件类型名 (如 "Transform", "Render")
    // 使用 float map 简单存储组件数据
    std::unordered_map<std::string, f32> FloatValues;
    std::unordered_map<std::string, std::string> StringValues;
};

// ── 实体蓝图 ────────────────────────────────────────────────

struct EntityBlueprint {
    std::string Name = "Entity";
    std::vector<ComponentSnapshot> Components;
    std::vector<EntityBlueprint> Children; // 子实体
};

// ── 预制体 ──────────────────────────────────────────────────

class Prefab {
public:
    Prefab() = default;
    Prefab(const std::string& name) : m_Name(name) {}

    /// 名称
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    /// 蓝图数据
    EntityBlueprint& GetRoot() { return m_Root; }
    const EntityBlueprint& GetRoot() const { return m_Root; }

    /// 从现有实体捕获快照
    static Ref<Prefab> CaptureFromEntity(ECSWorld& world, Entity e,
                                          const std::string& prefabName = "Prefab");

    /// 实例化到场景
    Entity Instantiate(ECSWorld& world, Entity parent = INVALID_ENTITY) const;

    /// 序列化为 JSON 字符串
    std::string Serialize() const;

    /// 从 JSON 字符串反序列化
    static Ref<Prefab> Deserialize(const std::string& json);

    /// 保存到文件
    bool SaveToFile(const std::string& path) const;

    /// 从文件加载
    static Ref<Prefab> LoadFromFile(const std::string& path);

private:
    std::string m_Name;
    EntityBlueprint m_Root;

    /// 递归捕获实体及子实体
    static void CaptureEntity(ECSWorld& world, Entity e, EntityBlueprint& blueprint);

    /// 递归实例化蓝图
    Entity InstantiateBlueprint(ECSWorld& world, const EntityBlueprint& bp,
                                 Entity parent) const;

    /// 应用组件快照到实体
    static void ApplySnapshot(ECSWorld& world, Entity e,
                               const ComponentSnapshot& snapshot);

    /// 从组件捕获快照
    static void CaptureSnapshot(ECSWorld& world, Entity e,
                                 ComponentSnapshot& snapshot,
                                 const std::string& typeName);
};

// ── 预制体管理器 ─────────────────────────────────────────────

class PrefabManager {
public:
    static void Register(const std::string& name, Ref<Prefab> prefab);
    static Ref<Prefab> Get(const std::string& name);
    static bool Has(const std::string& name);
    static void Clear();

    /// 从目录加载所有 .prefab 文件
    static void LoadFromDirectory(const std::string& dirPath);

private:
    static std::unordered_map<std::string, Ref<Prefab>> s_Prefabs;
};

} // namespace Engine
