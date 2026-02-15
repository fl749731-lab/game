#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"
#include "engine/renderer/texture.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace Engine {

// ── Tile 定义 ──────────────────────────────────────────────

enum class TileCollision : u8 {
    None = 0,       // 可通行
    Solid,          // 完全阻挡
    Water,          // 水域 (需要道具)
    Interact,       // 可交互 (如邮箱/告示牌)
};

struct TileData {
    u16  TileID = 0;                      // 图集中的 Tile 索引
    TileCollision Collision = TileCollision::None;
    u8   InteractType = 0;                // 自定义交互类型 (0=无)
};

// ── Tilemap 层 ─────────────────────────────────────────────

struct TilemapLayer {
    std::string Name;
    std::vector<TileData> Tiles;          // width * height 大小
    bool Visible = true;
    i32  ZOrder = 0;
};

// ── Tilemap ────────────────────────────────────────────────

class Tilemap {
public:
    Tilemap() = default;
    Tilemap(u32 width, u32 height, u32 tileSize);

    /// 从 JSON 文件加载 (兼容 Tiled 简化格式)
    bool LoadFromJSON(const std::string& filepath);

    /// 添加层
    void AddLayer(const std::string& name, i32 zOrder = 0);

    /// 获取/设置 Tile
    TileData& GetTile(u32 layerIdx, u32 x, u32 y);
    const TileData& GetTile(u32 layerIdx, u32 x, u32 y) const;
    void SetTile(u32 layerIdx, u32 x, u32 y, const TileData& tile);

    /// 碰撞查询
    TileCollision GetCollision(u32 x, u32 y) const;
    bool IsSolid(u32 x, u32 y) const;

    /// 世界坐标 → Tile 坐标
    glm::ivec2 WorldToTile(const glm::vec2& worldPos) const;
    glm::vec2  TileToWorld(u32 x, u32 y) const;

    /// AABB 碰撞 (世界坐标矩形 vs Tilemap)
    bool CheckAABBCollision(const glm::vec2& pos, const glm::vec2& size) const;

    // Getter
    u32 GetWidth()    const { return m_Width; }
    u32 GetHeight()   const { return m_Height; }
    u32 GetTileSize() const { return m_TileSize; }
    u32 GetLayerCount() const { return (u32)m_Layers.size(); }
    TilemapLayer& GetLayer(u32 idx) { return m_Layers[idx]; }
    const TilemapLayer& GetLayer(u32 idx) const { return m_Layers[idx]; }

    void SetTilesetTexture(const std::string& texName) { m_TilesetTexture = texName; }
    const std::string& GetTilesetTexture() const { return m_TilesetTexture; }
    u32 GetTilesetColumns() const { return m_TilesetColumns; }
    void SetTilesetColumns(u32 cols) { m_TilesetColumns = cols; }

private:
    u32 m_Width  = 0;
    u32 m_Height = 0;
    u32 m_TileSize = 16;                  // 像素
    std::vector<TilemapLayer> m_Layers;
    std::string m_TilesetTexture;
    u32 m_TilesetColumns = 16;            // 图集每行 Tile 数

    static TileData s_EmptyTile;
};

// ── Tilemap 渲染器 ─────────────────────────────────────────

class TilemapRenderer {
public:
    /// 绘制可见区域 (相机视锥裁剪)
    static void Draw(const Tilemap& map,
                     const glm::vec2& cameraPos,
                     const glm::vec2& viewportSize,
                     f32 pixelsPerUnit = 16.0f);
};

// ── Tilemap 组件 (挂到 Entity 上) ──────────────────────────

struct TilemapComponent : public Component {
    Ref<Tilemap> Map;
    f32 PixelsPerUnit = 16.0f;   // 1 个 Tile = 多少像素
};

} // namespace Engine
