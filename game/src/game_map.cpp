#include "game/game_map.h"

#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <cmath>

namespace Engine {

// Tile ID 约定
static constexpr u16 TILE_GRASS  = 1;
static constexpr u16 TILE_DIRT   = 2;
static constexpr u16 TILE_STONE  = 3;
static constexpr u16 TILE_WATER  = 4;
static constexpr u16 TILE_SAND   = 5;
static constexpr u16 TILE_TREE   = 10;
static constexpr u16 TILE_ROCK   = 11;
static constexpr u16 TILE_WALL   = 13;
static constexpr u16 TILE_FENCE  = 12;

// ── 简易值噪声 (Value Noise) ─────────────────────────────

static f32 Hash2D(i32 x, i32 y) {
    i32 n = x * 374761393 + y * 668265263;
    n = (n ^ (n >> 13)) * 1274126177;
    return (f32)(n & 0x7FFFFFFF) / (f32)0x7FFFFFFF;
}

static f32 SmoothNoise(f32 x, f32 y) {
    i32 ix = (i32)std::floor(x);
    i32 iy = (i32)std::floor(y);
    f32 fx = x - ix;
    f32 fy = y - iy;

    // 平滑插值 (smoothstep)
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);

    f32 v00 = Hash2D(ix,     iy);
    f32 v10 = Hash2D(ix + 1, iy);
    f32 v01 = Hash2D(ix,     iy + 1);
    f32 v11 = Hash2D(ix + 1, iy + 1);

    return v00 * (1 - fx) * (1 - fy) +
           v10 * fx       * (1 - fy) +
           v01 * (1 - fx) * fy       +
           v11 * fx       * fy;
}

// 多层叠加噪声 (FBM)
static f32 FBMNoise(f32 x, f32 y, u32 octaves = 3) {
    f32 val = 0.0f;
    f32 amp = 1.0f;
    f32 freq = 1.0f;
    f32 total = 0.0f;
    for (u32 i = 0; i < octaves; i++) {
        val += SmoothNoise(x * freq, y * freq) * amp;
        total += amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return val / total;
}

// ══════════════════════════════════════════════════════════════

void GameMap::Generate(u32 width, u32 height) {
    std::srand((unsigned)std::time(nullptr));

    m_Tilemap = Tilemap(width, height, 16);
    m_Tilemap.AddLayer("ground", 0);
    m_Tilemap.AddLayer("decor", 1);    // 装饰层 (小草/花/碎石)
    m_Tilemap.AddLayer("objects", 2);  // 障碍物层 (树木/建筑)

    f32 cx = (f32)width  * 0.5f;
    f32 cy = (f32)height * 0.5f;
    f32 maxR = std::min(cx, cy) * 0.95f;  // 最大有效半径

    // ── 1) 距离场 + 噪声 → 多层地形 ────────────────────
    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            f32 dx = (f32)x - cx;
            f32 dy = (f32)y - cy;
            f32 dist = std::sqrt(dx * dx + dy * dy);

            // 噪声扰动 (让边界不规则)
            f32 noise = FBMNoise((f32)x * 0.08f, (f32)y * 0.08f, 3);
            f32 normalizedDist = (dist / maxR) + (noise - 0.5f) * 0.35f;

            // 地形分层: 中心草地 → 泥土 → 沙地 → 石墙(外围)
            u16 terrainID;
            if (normalizedDist < 0.4f) {
                terrainID = TILE_GRASS;
            } else if (normalizedDist < 0.6f) {
                terrainID = TILE_DIRT;
            } else if (normalizedDist < 0.8f) {
                terrainID = TILE_SAND;
            } else {
                terrainID = TILE_STONE;
            }

            TileData tile;
            tile.TileID = terrainID;
            tile.Collision = TileCollision::None;

            // 最外围设为不可通行
            if (normalizedDist > 1.0f) {
                tile.Collision = TileCollision::Solid;
            }

            m_Tilemap.SetTile(0, x, y, tile);
        }
    }

    // ── 2) 边界围墙 ─────────────────────────────────────
    for (u32 x = 0; x < width; x++) {
        TileData wall;
        wall.TileID = TILE_STONE;
        wall.Collision = TileCollision::Solid;
        m_Tilemap.SetTile(0, x, 0, wall);
        m_Tilemap.SetTile(0, x, height - 1, wall);
    }
    for (u32 y = 0; y < height; y++) {
        TileData wall;
        wall.TileID = TILE_STONE;
        wall.Collision = TileCollision::Solid;
        m_Tilemap.SetTile(0, 0, y, wall);
        m_Tilemap.SetTile(0, width - 1, y, wall);
    }

    // ── 3) 中心出生点 ───────────────────────────────────
    m_PlayerSpawn = {cx, cy};

    // ── 4) 水池 (在泥土/沙地区域随机放 1-3 个) ─────────
    u32 pondCount = 1 + std::rand() % 3;
    for (u32 i = 0; i < pondCount; i++) {
        f32 angle = (f32)(std::rand() % 360) * 3.14159f / 180.0f;
        f32 r = maxR * (0.45f + (f32)(std::rand() % 20) * 0.01f);
        i32 px = (i32)(cx + r * std::cos(angle));
        i32 py = (i32)(cy + r * std::sin(angle));
        u32 ps = 2 + std::rand() % 2;
        for (u32 dy2 = 0; dy2 < ps; dy2++) {
            for (u32 dx2 = 0; dx2 < ps; dx2++) {
                i32 tx = px + (i32)dx2;
                i32 ty = py + (i32)dy2;
                if (tx > 1 && tx < (i32)width - 2 && ty > 1 && ty < (i32)height - 2) {
                    TileData water;
                    water.TileID = TILE_WATER;
                    water.Collision = TileCollision::Water;
                    m_Tilemap.SetTile(0, (u32)tx, (u32)ty, water);
                }
            }
        }
    }

    // ── 5) 废弃房间 (泥土/沙地区域) ─────────────────────
    m_LootPoints.clear();
    u32 roomCount = 4 + std::rand() % 4;
    for (u32 i = 0; i < roomCount; i++) {
        f32 angle = (f32)(std::rand() % 360) * 3.14159f / 180.0f;
        f32 r = maxR * (0.4f + (f32)(std::rand() % 30) * 0.01f);
        u32 rx = (u32)(cx + r * std::cos(angle));
        u32 ry = (u32)(cy + r * std::sin(angle));
        u32 rw = 3 + std::rand() % 3;
        u32 rh = 3 + std::rand() % 3;

        if (rx < 3 || ry < 3 || rx + rw >= width - 3 || ry + rh >= height - 3)
            continue;

        PlaceRoom(rx, ry, rw, rh);

        glm::vec2 loot = {(f32)(rx + rw / 2), (f32)(ry + rh / 2)};
        m_LootPoints.push_back(loot);
    }

    // ── 6) 散布装饰物 ───────────────────────────────────
    ScatterObjects();

    // ── 7) 丧尸刷新点 (外围石墙区) ─────────────────────
    m_ZombieSpawns.clear();
    for (i32 i = 0; i < 8; i++) {
        f32 angle = (f32)i * 3.14159f * 2.0f / 8.0f;
        f32 r = maxR * 0.85f;
        f32 sx = cx + r * std::cos(angle);
        f32 sy = cy + r * std::sin(angle);
        m_ZombieSpawns.push_back({sx, sy});
    }

    // 同步 NavGrid
    m_NavGrid = NavGrid(width, height, 1.0f);
    SyncNavGrid();
}

void GameMap::PlaceRoom(u32 x, u32 y, u32 w, u32 h) {
    // 围墙
    for (u32 dx = 0; dx < w; dx++) {
        TileData wall;
        wall.TileID = TILE_WALL;
        wall.Collision = TileCollision::Solid;
        m_Tilemap.SetTile(2, x + dx, y, wall);
        m_Tilemap.SetTile(2, x + dx, y + h - 1, wall);
    }
    for (u32 dy = 0; dy < h; dy++) {
        TileData wall;
        wall.TileID = TILE_WALL;
        wall.Collision = TileCollision::Solid;
        m_Tilemap.SetTile(2, x, y + dy, wall);
        m_Tilemap.SetTile(2, x + w - 1, y + dy, wall);
    }

    // 内部铺石板
    for (u32 dy = 1; dy < h - 1; dy++) {
        for (u32 dx = 1; dx < w - 1; dx++) {
            TileData floor;
            floor.TileID = TILE_STONE;
            floor.Collision = TileCollision::None;
            m_Tilemap.SetTile(0, x + dx, y + dy, floor);
        }
    }

    // 开一个门 (底边中间)
    u32 doorX = x + w / 2;
    TileData door;
    door.TileID = TILE_DIRT;
    door.Collision = TileCollision::None;
    m_Tilemap.SetTile(2, doorX, y, door);
}

void GameMap::ScatterObjects() {
    u32 w = m_Tilemap.GetWidth();
    u32 h = m_Tilemap.GetHeight();
    f32 cx = (f32)w * 0.5f;
    f32 cy = (f32)h * 0.5f;

    // ── 装饰层 (layer 1): 小草/花/碎石 ──────────────
    for (u32 y = 1; y < h - 1; y++) {
        for (u32 x = 1; x < w - 1; x++) {
            auto& ground = m_Tilemap.GetTile(0, x, y);
            // 只在草地/泥土上放装饰
            if (ground.TileID != TILE_GRASS && ground.TileID != TILE_DIRT)
                continue;

            // 15% 几率生成装饰
            if (std::rand() % 100 < 15) {
                TileData decor;
                i32 roll = std::rand() % 10;
                if (roll < 5)      decor.TileID = 20;  // 小草
                else if (roll < 8) decor.TileID = 21;  // 花
                else               decor.TileID = 22;  // 碎石
                decor.Collision = TileCollision::None;
                m_Tilemap.SetTile(1, x, y, decor);
            }
        }
    }

    // ── 障碍物层 (layer 2): 树木/石头 ───────────────
    for (u32 i = 0; i < w * h / 10; i++) {
        u32 x = 2 + std::rand() % (w - 4);
        u32 y = 2 + std::rand() % (h - 4);

        // 不覆盖已有物件
        auto& existing = m_Tilemap.GetTile(2, x, y);
        if (existing.TileID != 0) continue;

        // 不挡中心出生点
        if (std::abs((i32)x - (i32)cx) < 5 && std::abs((i32)y - (i32)cy) < 5)
            continue;

        // 只在草地/泥土区域放装饰
        auto& ground = m_Tilemap.GetTile(0, x, y);
        if (ground.TileID != TILE_GRASS && ground.TileID != TILE_DIRT)
            continue;

        TileData obj;
        i32 roll = std::rand() % 10;
        if (roll < 4) {
            obj.TileID = TILE_TREE;
            obj.Collision = TileCollision::Solid;
        } else if (roll < 6) {
            obj.TileID = TILE_ROCK;
            obj.Collision = TileCollision::Solid;
        } else {
            continue;
        }
        m_Tilemap.SetTile(2, x, y, obj);
    }
}

void GameMap::SyncNavGrid() {
    u32 w = m_Tilemap.GetWidth();
    u32 h = m_Tilemap.GetHeight();

    for (u32 y = 0; y < h; y++) {
        for (u32 x = 0; x < w; x++) {
            bool walkable = true;
            for (u32 layer = 0; layer < m_Tilemap.GetLayerCount(); layer++) {
                auto& tile = m_Tilemap.GetTile(layer, x, y);
                if (tile.Collision == TileCollision::Solid ||
                    tile.Collision == TileCollision::Water) {
                    walkable = false;
                    break;
                }
            }
            m_NavGrid.SetWalkable(x, y, walkable);
        }
    }
}

} // namespace Engine
