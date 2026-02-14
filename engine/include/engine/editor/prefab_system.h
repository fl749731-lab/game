#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace Engine {

// ── 预制件系统 ──────────────────────────────────────────────
// 功能:
//   1. 从实体保存预制件模板
//   2. 实例化预制件 (保持组件数据)
//   3. 覆盖管理 (实例 vs 模板)
//   4. 嵌套预制件
//   5. 序列化/反序列化 (JSON)

struct PrefabComponentData {
    std::string TypeName;    // "TransformComponent", "RenderComponent" 等
    std::unordered_map<std::string, std::string> Properties;  // key=value 字符串表示
};

struct PrefabTemplate {
    std::string Name;
    std::string FilePath;    // .prefab 文件路径
    std::vector<PrefabComponentData> Components;
    std::vector<PrefabTemplate> Children;  // 嵌套子预制件
    u32 ID = 0;
};

class PrefabSystem {
public:
    static void Init();
    static void Shutdown();

    /// 从实体创建预制件模板
    static PrefabTemplate CreateFromEntity(ECSWorld& world, Entity entity);

    /// 实例化预制件 — 返回根实体
    static Entity Instantiate(ECSWorld& world, const PrefabTemplate& prefab,
                               const glm::vec3& position = {0,0,0});

    /// 保存预制件到文件
    static bool SavePrefab(const PrefabTemplate& prefab, const std::string& path);

    /// 从文件加载预制件
    static PrefabTemplate LoadPrefab(const std::string& path);

    /// 注册预制件到库
    static void Register(const PrefabTemplate& prefab);

    /// 获取所有注册的预制件
    static const std::vector<PrefabTemplate>& GetLibrary() { return s_Library; }

    /// 渲染预制件库面板
    static void RenderLibraryPanel(ECSWorld& world);

private:
    static void SerializeComponent(const ECSWorld& world, Entity entity,
                                    const std::string& typeName, PrefabComponentData& data);

    inline static std::vector<PrefabTemplate> s_Library;
    inline static u32 s_NextPrefabID = 1;
};

} // namespace Engine
