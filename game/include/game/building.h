#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Engine {

// ── 建筑类型 ──────────────────────────────────────────────

enum class BuildingType : u8 {
    WoodWall = 0,   // 木墙 — 基础防御
    StoneWall,      // 石墙 — 高耐久
    WoodDoor,       // 木门 — 玩家可通过, 丧尸需打破
    Spike,          // 地刺 — 丧尸踩上扣血
    Barricade,      // 路障 — 减速丧尸
    Campfire,       // 营火 — 照亮范围, 回复体力
    Workbench,      // 工作台 — 制造
    COUNT
};

// ── 建筑预设 ──────────────────────────────────────────────

struct BuildingPreset {
    BuildingType Type;
    const char*  Name;
    f32  MaxHP;
    glm::vec2 Size;         // 碰撞尺寸 (Tile 单位)
    bool BlocksMovement;    // 阻挡移动
    bool DamagesZombies;    // 对丧尸造成伤害
    f32  DamageAmount;      // 每秒伤害
    f32  SlowFactor;        // 减速因子 (1.0=无减速, 0.5=减半)
    f32  LightRadius;       // 照明范围 (0=不发光)
};

inline BuildingPreset GetBuildingPreset(BuildingType type) {
    switch (type) {
        case BuildingType::WoodWall:
            return {type, "木墙",    80.0f, {1,1}, true,  false, 0,    1.0f, 0};
        case BuildingType::StoneWall:
            return {type, "石墙",    200.0f,{1,1}, true,  false, 0,    1.0f, 0};
        case BuildingType::WoodDoor:
            return {type, "木门",    60.0f, {1,1}, false, false, 0,    1.0f, 0};
        case BuildingType::Spike:
            return {type, "地刺",    40.0f, {1,1}, false, true,  5.0f, 1.0f, 0};
        case BuildingType::Barricade:
            return {type, "路障",    50.0f, {1,1}, false, false, 0,    0.4f, 0};
        case BuildingType::Campfire:
            return {type, "营火",    30.0f, {0.8f,0.8f}, false, false, 0, 1.0f, 5.0f};
        case BuildingType::Workbench:
            return {type, "工作台",  100.0f,{1.5f,1}, true, false, 0,    1.0f, 0};
        default:
            return {BuildingType::WoodWall, "?", 50, {1,1}, true, false, 0, 1.0f, 0};
    }
}

// ── 建筑组件 ──────────────────────────────────────────────

struct BuildableComponent : public Component {
    BuildingType Type = BuildingType::WoodWall;
    f32 MaxHP = 80.0f;
    f32 HP    = 80.0f;
    glm::vec2 Size = {1, 1};       // 碰撞尺寸
    bool BlocksMovement = true;
    bool DamagesZombies = false;
    f32  DamageAmount   = 0.0f;
    f32  SlowFactor     = 1.0f;
    f32  LightRadius    = 0.0f;
};

// ── 制作配方 ──────────────────────────────────────────────

struct CraftCost {
    u32 ItemID;
    u32 Count;
};

struct BuildRecipe {
    BuildingType Result;
    std::vector<CraftCost> Costs;
};

// ── 建造系统 ──────────────────────────────────────────────

class NavGrid;  // 前向声明

class BuildingSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override;
    const char* GetName() const override { return "BuildingSystem"; }

    /// 设置寻路网格 (放置建筑时同步更新)
    void SetNavGrid(NavGrid* grid) { m_NavGrid = grid; }

    /// 进入/退出建造模式
    void EnterBuildMode(BuildingType type);
    void ExitBuildMode();
    bool IsInBuildMode() const { return m_BuildMode; }
    BuildingType GetBuildType() const { return m_BuildType; }

    /// 放置建筑 (世界坐标, 自由放置)
    Entity PlaceBuilding(ECSWorld& world, const glm::vec2& worldPos);

    /// 丧尸攻击建筑
    void DamageBuilding(ECSWorld& world, Entity building, f32 damage);

    /// 检查位置是否可放置 (碰撞检测)
    bool CanPlace(ECSWorld& world, const glm::vec2& pos, const glm::vec2& size) const;

    /// 获取预览位置 (用于渲染半透明预览)
    void SetPreviewPosition(const glm::vec2& pos) { m_PreviewPos = pos; }
    glm::vec2 GetPreviewPosition() const { return m_PreviewPos; }

    /// 注册配方
    void RegisterRecipe(const BuildRecipe& recipe);
    const BuildRecipe* GetRecipe(BuildingType type) const;
    const std::vector<BuildRecipe>& GetAllRecipes() const { return m_Recipes; }

private:
    bool          m_BuildMode = false;
    BuildingType  m_BuildType = BuildingType::WoodWall;
    glm::vec2     m_PreviewPos = {0, 0};
    NavGrid*      m_NavGrid = nullptr;
    std::vector<BuildRecipe> m_Recipes;
};

} // namespace Engine
