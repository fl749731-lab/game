#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace Engine {

// ── 季节 ──────────────────────────────────────────────────

enum class Season : u8 {
    Spring = 0,
    Summer,
    Autumn,
    Winter,
    COUNT
};

inline const char* SeasonName(Season s) {
    switch (s) {
        case Season::Spring: return "春";
        case Season::Summer: return "夏";
        case Season::Autumn: return "秋";
        case Season::Winter: return "冬";
        default: return "?";
    }
}

// ── 土壤状态 ──────────────────────────────────────────────

enum class SoilState : u8 {
    Untilled = 0,   // 未开垦
    Tilled,         // 已翻耕
    Watered,        // 已浇水
    Planted,        // 已种植 (含浇水)
};

// ── 作物定义 ──────────────────────────────────────────────

struct CropDef {
    std::string ID;
    std::string Name;
    u32         SeedItemID = 0;
    u32         HarvestItemID = 0;
    u32         GrowthDays = 4;
    u32         Stages = 4;
    bool        Regrows = false;
    u32         RegrowDays = 3;
    u32         HarvestMin = 1;
    u32         HarvestMax = 1;
    Season      AllowedSeasons[4] = {Season::Spring};
    u32         AllowedSeasonCount = 1;

    bool CanGrowIn(Season s) const {
        for (u32 i = 0; i < AllowedSeasonCount; i++)
            if (AllowedSeasons[i] == s) return true;
        return false;
    }
};

// ── 农田格子 ──────────────────────────────────────────────

struct FarmTile {
    SoilState State = SoilState::Untilled;
    std::string CropID;
    u32  GrowthDay = 0;
    bool WateredToday = false;
    bool Fertilized = false;
};

// ── 农场组件 ──────────────────────────────────────────────

struct FarmComponent : public Component {
    std::vector<FarmTile>  Tiles;
    u32 Width  = 0;
    u32 Height = 0;
    glm::ivec2 Offset = {0, 0};

    void Init(u32 w, u32 h, const glm::ivec2& offset) {
        Width = w; Height = h; Offset = offset;
        Tiles.resize(w * h);
    }

    FarmTile& At(u32 x, u32 y) { return Tiles[y * Width + x]; }
    const FarmTile& At(u32 x, u32 y) const { return Tiles[y * Width + x]; }
    bool InBounds(u32 x, u32 y) const { return x < Width && y < Height; }

    glm::ivec2 WorldToFarm(const glm::ivec2& world) const {
        return world - Offset;
    }
};

// ── 农场系统 ──────────────────────────────────────────────

class FarmingSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override;
    const char* GetName() const override { return "FarmingSystem"; }

    void RegisterCrop(const CropDef& def);
    const CropDef* GetCropDef(const std::string& id) const;

    void AdvanceDay(ECSWorld& world, Season currentSeason);

    bool TillSoil(FarmComponent& farm, u32 x, u32 y);
    bool WaterSoil(FarmComponent& farm, u32 x, u32 y);
    bool PlantSeed(FarmComponent& farm, u32 x, u32 y, const std::string& cropID, Season season);
    u32  Harvest(FarmComponent& farm, u32 x, u32 y);

private:
    std::unordered_map<std::string, CropDef> m_CropDefs;
};

} // namespace Engine
