#include "engine/game2d/tilemap.h"
#include "engine/renderer/sprite_batch.h"
#include "engine/core/resource_manager.h"
#include "engine/core/log.h"

#include <fstream>
#include <cmath>

namespace Engine {

TileData Tilemap::s_EmptyTile = {};

Tilemap::Tilemap(u32 width, u32 height, u32 tileSize)
    : m_Width(width), m_Height(height), m_TileSize(tileSize) {}

bool Tilemap::LoadFromJSON(const std::string& filepath) {
    // 简易 JSON 读取 — 未来可替换为 nlohmann::json
    // 目前仅支持手动构建
    LOG_WARN("[Tilemap] JSON 加载暂未实现, 请使用 API 手动构建: %s", filepath.c_str());
    return false;
}

void Tilemap::AddLayer(const std::string& name, i32 zOrder) {
    TilemapLayer layer;
    layer.Name = name;
    layer.ZOrder = zOrder;
    layer.Tiles.resize(m_Width * m_Height);
    m_Layers.push_back(std::move(layer));
}

TileData& Tilemap::GetTile(u32 layerIdx, u32 x, u32 y) {
    if (layerIdx >= m_Layers.size() || x >= m_Width || y >= m_Height)
        return s_EmptyTile;
    return m_Layers[layerIdx].Tiles[y * m_Width + x];
}

const TileData& Tilemap::GetTile(u32 layerIdx, u32 x, u32 y) const {
    if (layerIdx >= m_Layers.size() || x >= m_Width || y >= m_Height)
        return s_EmptyTile;
    return m_Layers[layerIdx].Tiles[y * m_Width + x];
}

void Tilemap::SetTile(u32 layerIdx, u32 x, u32 y, const TileData& tile) {
    if (layerIdx >= m_Layers.size() || x >= m_Width || y >= m_Height) return;
    m_Layers[layerIdx].Tiles[y * m_Width + x] = tile;
}

TileCollision Tilemap::GetCollision(u32 x, u32 y) const {
    // 在所有层中查找最高优先碰撞
    TileCollision result = TileCollision::None;
    for (auto& layer : m_Layers) {
        if (x < m_Width && y < m_Height) {
            auto& td = layer.Tiles[y * m_Width + x];
            if (td.Collision > result)
                result = td.Collision;
        }
    }
    return result;
}

bool Tilemap::IsSolid(u32 x, u32 y) const {
    return GetCollision(x, y) == TileCollision::Solid;
}

glm::ivec2 Tilemap::WorldToTile(const glm::vec2& worldPos) const {
    return glm::ivec2((i32)std::floor(worldPos.x), (i32)std::floor(worldPos.y));
}

glm::vec2 Tilemap::TileToWorld(u32 x, u32 y) const {
    return glm::vec2((f32)x + 0.5f, (f32)y + 0.5f); // 返回 Tile 中心
}

bool Tilemap::CheckAABBCollision(const glm::vec2& pos, const glm::vec2& size) const {
    // 获取 AABB 覆盖的 Tile 范围
    i32 minX = (i32)std::floor(pos.x - size.x * 0.5f);
    i32 minY = (i32)std::floor(pos.y - size.y * 0.5f);
    i32 maxX = (i32)std::floor(pos.x + size.x * 0.5f);
    i32 maxY = (i32)std::floor(pos.y + size.y * 0.5f);

    for (i32 y = minY; y <= maxY; y++) {
        for (i32 x = minX; x <= maxX; x++) {
            if (x < 0 || y < 0 || x >= (i32)m_Width || y >= (i32)m_Height) {
                return true; // 地图边界 = 碰撞
            }
            if (IsSolid((u32)x, (u32)y)) return true;
        }
    }
    return false;
}

// ── TilemapRenderer ────────────────────────────────────────

void TilemapRenderer::Draw(const Tilemap& map,
                           const glm::vec2& cameraPos,
                           const glm::vec2& viewportSize,
                           f32 pixelsPerUnit) {
    auto tex = ResourceManager::GetTexture(map.GetTilesetTexture());
    if (!tex) return;

    u32 tileSize = map.GetTileSize();
    u32 tilesetCols = map.GetTilesetColumns();
    u32 texW = tex->GetWidth();
    u32 texH = tex->GetHeight();

    // 计算可见 Tile 范围
    f32 halfW = viewportSize.x * 0.5f;
    f32 halfH = viewportSize.y * 0.5f;
    i32 minX = std::max(0, (i32)std::floor(cameraPos.x - halfW));
    i32 minY = std::max(0, (i32)std::floor(cameraPos.y - halfH));
    i32 maxX = std::min((i32)map.GetWidth() - 1,  (i32)std::ceil(cameraPos.x + halfW));
    i32 maxY = std::min((i32)map.GetHeight() - 1, (i32)std::ceil(cameraPos.y + halfH));

    // 逐层绘制
    for (u32 l = 0; l < map.GetLayerCount(); l++) {
        auto& layer = map.GetLayer(l);
        if (!layer.Visible) continue;

        for (i32 y = minY; y <= maxY; y++) {
            for (i32 x = minX; x <= maxX; x++) {
                auto& tile = map.GetTile(l, (u32)x, (u32)y);
                if (tile.TileID == 0) continue; // 0 = 空 Tile

                // 计算图集中的 UV
                u32 tileIdx = tile.TileID - 1; // TileID 从 1 开始
                u32 col = tileIdx % tilesetCols;
                u32 row = tileIdx / tilesetCols;

                glm::vec2 pos((f32)x, (f32)y);
                glm::vec2 size(1.0f, 1.0f);

                // 用 SpriteBatch 中的 Draw 直接绘制
                // 由于 SpriteBatch 使用 Texture2D Ref, 此处简化为坐标计算
                SpriteBatch::Draw(tex, pos, size, 0.0f,
                                  glm::vec4(1.0f)); // TODO: UV 子区域
            }
        }
    }
}

} // namespace Engine
