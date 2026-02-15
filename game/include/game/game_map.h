#pragma once

#include "engine/core/types.h"
#include "engine/game2d/tilemap.h"
#include "engine/ai/behavior_tree.h"

#include <glm/glm.hpp>
#include <vector>

namespace Engine {

// ── 生存组件 ──────────────────────────────────────────────

struct SurvivalComponent : public Component {
    f32 Hunger    = 100.0f;
    f32 Thirst    = 100.0f;
    f32 MaxHunger = 100.0f;
    f32 MaxThirst = 100.0f;
    f32 HungerRate = 0.3f;  // 每游戏分钟消耗
    f32 ThirstRate = 0.5f;
};

// ── 可搜刮组件 ────────────────────────────────────────────

struct LootableComponent : public Component {
    bool Looted = false;
    std::vector<std::pair<u32,u32>> LootTable;  // {ItemID, Count}
    std::string PromptText = "搜刮 [E]";
};

// ── 游戏地图 ──────────────────────────────────────────────
// 管理 Tilemap + NavGrid + 区域信息

class GameMap {
public:
    /// 程序化生成 (width × height Tile)
    void Generate(u32 width, u32 height);

    /// Tilemap ↔ NavGrid 同步
    void SyncNavGrid();

    // ── Getter ──────────────────────────────────────────
    Tilemap&       GetTilemap()  { return m_Tilemap; }
    NavGrid&       GetNavGrid()  { return m_NavGrid; }
    const Tilemap& GetTilemap()  const { return m_Tilemap; }
    const NavGrid& GetNavGrid()  const { return m_NavGrid; }

    glm::vec2 GetPlayerSpawn() const { return m_PlayerSpawn; }
    const std::vector<glm::vec2>& GetZombieSpawnPoints() const { return m_ZombieSpawns; }
    const std::vector<glm::vec2>& GetLootPoints() const { return m_LootPoints; }

    u32 GetWidth()  const { return m_Tilemap.GetWidth(); }
    u32 GetHeight() const { return m_Tilemap.GetHeight(); }

private:
    /// 内部: 放置一个矩形房间
    void PlaceRoom(u32 x, u32 y, u32 w, u32 h);
    /// 内部: 散布装饰
    void ScatterObjects();

    Tilemap m_Tilemap;
    NavGrid m_NavGrid;

    glm::vec2 m_PlayerSpawn = {0, 0};
    std::vector<glm::vec2> m_ZombieSpawns;
    std::vector<glm::vec2> m_LootPoints;
};

} // namespace Engine
