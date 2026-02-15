#include "game/game_map.h"

#include <cstdlib>
#include <ctime>
#include <algorithm>

namespace Engine {

// Tile ID 约定 (与渲染颜色对应)
static constexpr u16 TILE_GRASS  = 1;
static constexpr u16 TILE_DIRT   = 2;
static constexpr u16 TILE_STONE  = 3;
static constexpr u16 TILE_WATER  = 4;
static constexpr u16 TILE_TREE   = 10;
static constexpr u16 TILE_ROCK   = 11;
static constexpr u16 TILE_WALL   = 13;
static constexpr u16 TILE_FENCE  = 12;

void GameMap::Generate(u32 width, u32 height) {
    std::srand((unsigned)std::time(nullptr));

    // 初始化 Tilemap
    m_Tilemap = Tilemap(width, height, 16);
    m_Tilemap.AddLayer("ground", 0);
    m_Tilemap.AddLayer("objects", 1);

    // 1) 地面全部填草地
    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++) {
            TileData grass;
            grass.TileID = TILE_GRASS;
            grass.Collision = TileCollision::None;
            m_Tilemap.SetTile(0, x, y, grass);
        }
    }

    // 2) 边界围墙
    for (u32 x = 0; x < width; x++) {
        TileData wall;
        wall.TileID = TILE_FENCE;
        wall.Collision = TileCollision::Solid;
        m_Tilemap.SetTile(1, x, 0, wall);
        m_Tilemap.SetTile(1, x, height - 1, wall);
    }
    for (u32 y = 0; y < height; y++) {
        TileData wall;
        wall.TileID = TILE_FENCE;
        wall.Collision = TileCollision::Solid;
        m_Tilemap.SetTile(1, 0, y, wall);
        m_Tilemap.SetTile(1, width - 1, y, wall);
    }

    // 3) 中心空地 (玩家出生区)
    m_PlayerSpawn = {(f32)width * 0.5f, (f32)height * 0.5f};

    // 中心附近铺泥土路
    u32 cx = width / 2, cy = height / 2;
    for (i32 dy = -2; dy <= 2; dy++) {
        for (i32 dx = -2; dx <= 2; dx++) {
            u32 tx = cx + dx, ty = cy + dy;
            if (tx < width && ty < height) {
                TileData dirt;
                dirt.TileID = TILE_DIRT;
                dirt.Collision = TileCollision::None;
                m_Tilemap.SetTile(0, tx, ty, dirt);
            }
        }
    }

    // 4) 随机散布废弃房间 (5-8 个)
    u32 roomCount = 5 + std::rand() % 4;
    for (u32 i = 0; i < roomCount; i++) {
        u32 rw = 3 + std::rand() % 3;
        u32 rh = 3 + std::rand() % 3;
        u32 rx = 3 + std::rand() % (width - rw - 6);
        u32 ry = 3 + std::rand() % (height - rh - 6);

        // 跳过与中心重叠的房间
        if (std::abs((i32)rx - (i32)cx) < 5 && std::abs((i32)ry - (i32)cy) < 5)
            continue;

        PlaceRoom(rx, ry, rw, rh);

        // 房间中心作为搜刮点
        glm::vec2 loot = {(f32)(rx + rw / 2), (f32)(ry + rh / 2)};
        m_LootPoints.push_back(loot);
    }

    // 5) 散布装饰物 (树/石头)
    ScatterObjects();

    // 6) 水池 (随机 1-2 个)
    u32 pondCount = 1 + std::rand() % 2;
    for (u32 i = 0; i < pondCount; i++) {
        u32 px = 5 + std::rand() % (width - 10);
        u32 py = 5 + std::rand() % (height - 10);
        u32 ps = 2 + std::rand() % 2;
        for (u32 dy = 0; dy < ps; dy++) {
            for (u32 dx = 0; dx < ps; dx++) {
                u32 tx = px + dx, ty = py + dy;
                if (tx < width && ty < height) {
                    TileData water;
                    water.TileID = TILE_WATER;
                    water.Collision = TileCollision::Water;
                    m_Tilemap.SetTile(0, tx, ty, water);
                }
            }
        }
    }

    // 7) 丧尸刷新点 (地图四角 + 边缘)
    m_ZombieSpawns.clear();
    m_ZombieSpawns.push_back({3.0f, 3.0f});
    m_ZombieSpawns.push_back({(f32)width - 4.0f, 3.0f});
    m_ZombieSpawns.push_back({3.0f, (f32)height - 4.0f});
    m_ZombieSpawns.push_back({(f32)width - 4.0f, (f32)height - 4.0f});
    // 边缘中点
    m_ZombieSpawns.push_back({(f32)(width / 2), 3.0f});
    m_ZombieSpawns.push_back({(f32)(width / 2), (f32)height - 4.0f});
    m_ZombieSpawns.push_back({3.0f, (f32)(height / 2)});
    m_ZombieSpawns.push_back({(f32)width - 4.0f, (f32)(height / 2)});

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
        m_Tilemap.SetTile(1, x + dx, y, wall);
        m_Tilemap.SetTile(1, x + dx, y + h - 1, wall);
    }
    for (u32 dy = 0; dy < h; dy++) {
        TileData wall;
        wall.TileID = TILE_WALL;
        wall.Collision = TileCollision::Solid;
        m_Tilemap.SetTile(1, x, y + dy, wall);
        m_Tilemap.SetTile(1, x + w - 1, y + dy, wall);
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
    m_Tilemap.SetTile(1, doorX, y, door);
}

void GameMap::ScatterObjects() {
    u32 w = m_Tilemap.GetWidth();
    u32 h = m_Tilemap.GetHeight();

    for (u32 i = 0; i < w * h / 12; i++) {
        u32 x = 2 + std::rand() % (w - 4);
        u32 y = 2 + std::rand() % (h - 4);

        // 不要覆盖已有物件
        auto& existing = m_Tilemap.GetTile(1, x, y);
        if (existing.TileID != 0) continue;

        // 不要挡住中心出生点
        u32 cx = w / 2, cy = h / 2;
        if (std::abs((i32)x - (i32)cx) < 4 && std::abs((i32)y - (i32)cy) < 4)
            continue;

        TileData obj;
        obj.TileID = (std::rand() % 3 == 0) ? TILE_ROCK : TILE_TREE;
        obj.Collision = TileCollision::Solid;
        m_Tilemap.SetTile(1, x, y, obj);
    }
}

void GameMap::SyncNavGrid() {
    u32 w = m_Tilemap.GetWidth();
    u32 h = m_Tilemap.GetHeight();

    for (u32 y = 0; y < h; y++) {
        for (u32 x = 0; x < w; x++) {
            // 检查所有层的碰撞
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
