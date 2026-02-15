#include "game/farming.h"
#include "engine/core/log.h"

namespace Engine {

void FarmingSystem::Update(ECSWorld& world, f32 dt) {
    (void)world; (void)dt;
}

void FarmingSystem::RegisterCrop(const CropDef& def) {
    m_CropDefs[def.ID] = def;
    LOG_INFO("[农场] 注册作物: %s (%u天成熟)", def.Name.c_str(), def.GrowthDays);
}

const CropDef* FarmingSystem::GetCropDef(const std::string& id) const {
    auto it = m_CropDefs.find(id);
    return it != m_CropDefs.end() ? &it->second : nullptr;
}

void FarmingSystem::AdvanceDay(ECSWorld& world, Season currentSeason) {
    world.ForEach<FarmComponent>([&](Entity e, FarmComponent& farm) {
        (void)e;
        for (u32 y = 0; y < farm.Height; y++) {
            for (u32 x = 0; x < farm.Width; x++) {
                auto& tile = farm.At(x, y);
                if (!tile.CropID.empty() && tile.WateredToday) {
                    auto* cropDef = GetCropDef(tile.CropID);
                    if (cropDef) {
                        if (!cropDef->CanGrowIn(currentSeason)) {
                            tile.CropID.clear();
                            tile.GrowthDay = 0;
                            tile.State = SoilState::Tilled;
                            continue;
                        }
                        tile.GrowthDay++;
                        if (tile.Fertilized) tile.GrowthDay++;
                    }
                }
                tile.WateredToday = false;
                if (tile.State == SoilState::Watered || tile.State == SoilState::Planted) {
                    if (tile.CropID.empty()) tile.State = SoilState::Tilled;
                }
            }
        }
    });
}

bool FarmingSystem::TillSoil(FarmComponent& farm, u32 x, u32 y) {
    if (!farm.InBounds(x, y)) return false;
    auto& tile = farm.At(x, y);
    if (tile.State != SoilState::Untilled) return false;
    tile.State = SoilState::Tilled;
    return true;
}

bool FarmingSystem::WaterSoil(FarmComponent& farm, u32 x, u32 y) {
    if (!farm.InBounds(x, y)) return false;
    auto& tile = farm.At(x, y);
    if (tile.State == SoilState::Untilled) return false;
    tile.WateredToday = true;
    if (tile.State == SoilState::Tilled) tile.State = SoilState::Watered;
    return true;
}

bool FarmingSystem::PlantSeed(FarmComponent& farm, u32 x, u32 y,
                               const std::string& cropID, Season season) {
    if (!farm.InBounds(x, y)) return false;
    auto& tile = farm.At(x, y);
    if (tile.State != SoilState::Tilled && tile.State != SoilState::Watered) return false;
    if (!tile.CropID.empty()) return false;
    auto* def = GetCropDef(cropID);
    if (!def || !def->CanGrowIn(season)) return false;
    tile.CropID = cropID;
    tile.GrowthDay = 0;
    tile.State = SoilState::Planted;
    return true;
}

u32 FarmingSystem::Harvest(FarmComponent& farm, u32 x, u32 y) {
    if (!farm.InBounds(x, y)) return 0;
    auto& tile = farm.At(x, y);
    if (tile.CropID.empty()) return 0;
    auto* def = GetCropDef(tile.CropID);
    if (!def || tile.GrowthDay < def->GrowthDays) return 0;
    u32 harvestItem = def->HarvestItemID;
    if (def->Regrows) {
        tile.GrowthDay = def->GrowthDays - def->RegrowDays;
    } else {
        tile.CropID.clear();
        tile.GrowthDay = 0;
        tile.State = SoilState::Tilled;
    }
    return harvestItem;
}

} // namespace Engine
