#include "game/building.h"
#include "engine/ai/behavior_tree.h"  // NavGrid

#include <cmath>

namespace Engine {

void BuildingSystem::Update(ECSWorld& world, f32 dt) {
    // 地刺伤害 + 路障减速 由 ZombieSystem 那边检测处理
    // 这里处理建筑耐久归零 → 销毁
    std::vector<Entity> toDestroy;

    world.ForEach<BuildableComponent>([&](Entity e, BuildableComponent& bld) {
        if (bld.HP <= 0) {
            toDestroy.push_back(e);
        }
    });

    for (auto e : toDestroy) {
        // 销毁建筑时，恢复 NavGrid 可行走
        auto* tr = world.GetComponent<TransformComponent>(e);
        auto* bld = world.GetComponent<BuildableComponent>(e);
        if (tr && bld && bld->BlocksMovement && m_NavGrid) {
            i32 gx = (i32)std::floor(tr->X);
            i32 gy = (i32)std::floor(tr->Y);
            m_NavGrid->SetWalkable(gx, gy, true);
        }
        world.DestroyEntity(e);
    }
}

void BuildingSystem::EnterBuildMode(BuildingType type) {
    m_BuildMode = true;
    m_BuildType = type;
}

void BuildingSystem::ExitBuildMode() {
    m_BuildMode = false;
}

Entity BuildingSystem::PlaceBuilding(ECSWorld& world, const glm::vec2& worldPos) {
    auto preset = GetBuildingPreset(m_BuildType);

    if (!CanPlace(world, worldPos, preset.Size)) {
        return INVALID_ENTITY;
    }

    Entity e = world.CreateEntity(preset.Name);

    auto& tr = world.AddComponent<TransformComponent>(e);
    tr.X = worldPos.x;
    tr.Y = worldPos.y;
    tr.ScaleX = preset.Size.x;
    tr.ScaleY = preset.Size.y;
    tr.ScaleZ = 1.0f;

    auto& bld = world.AddComponent<BuildableComponent>(e);
    bld.Type           = preset.Type;
    bld.MaxHP          = preset.MaxHP;
    bld.HP             = preset.MaxHP;
    bld.Size           = preset.Size;
    bld.BlocksMovement = preset.BlocksMovement;
    bld.DamagesZombies = preset.DamagesZombies;
    bld.DamageAmount   = preset.DamageAmount;
    bld.SlowFactor     = preset.SlowFactor;
    bld.LightRadius    = preset.LightRadius;

    // 阻挡型建筑 → 更新 NavGrid
    if (bld.BlocksMovement && m_NavGrid) {
        i32 gx = (i32)std::floor(worldPos.x);
        i32 gy = (i32)std::floor(worldPos.y);
        m_NavGrid->SetWalkable(gx, gy, false);
    }

    return e;
}

void BuildingSystem::DamageBuilding(ECSWorld& world, Entity building, f32 damage) {
    auto* bld = world.GetComponent<BuildableComponent>(building);
    if (!bld) return;
    bld->HP -= damage;
    if (bld->HP < 0) bld->HP = 0;
}

bool BuildingSystem::CanPlace(ECSWorld& world, const glm::vec2& pos,
                               const glm::vec2& size) const {
    // AABB 碰撞检测: 不能和已有建筑重叠
    f32 halfW = size.x * 0.5f;
    f32 halfH = size.y * 0.5f;

    bool blocked = false;
    world.ForEach<BuildableComponent>([&](Entity e, BuildableComponent& other) {
        if (blocked) return;
        auto* otr = world.GetComponent<TransformComponent>(e);
        if (!otr) return;

        glm::vec2 opos = {otr->X, otr->Y};
        f32 oHalfW = other.Size.x * 0.5f;
        f32 oHalfH = other.Size.y * 0.5f;

        bool overlap = (pos.x - halfW < opos.x + oHalfW) &&
                        (pos.x + halfW > opos.x - oHalfW) &&
                        (pos.y - halfH < opos.y + oHalfH) &&
                        (pos.y + halfH > opos.y - oHalfH);
        if (overlap) blocked = true;
    });

    return !blocked;
}

void BuildingSystem::RegisterRecipe(const BuildRecipe& recipe) {
    m_Recipes.push_back(recipe);
}

const BuildRecipe* BuildingSystem::GetRecipe(BuildingType type) const {
    for (auto& r : m_Recipes) {
        if (r.Result == type) return &r;
    }
    return nullptr;
}

} // namespace Engine
