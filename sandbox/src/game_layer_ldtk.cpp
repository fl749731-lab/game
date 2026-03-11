// ══════════════════════════════════════════════════════════════
//  GameLayer — LDtk 地图加载与渲染
//  从 game_layer.cpp 拆分
// ══════════════════════════════════════════════════════════════

#include "game_layer.h"
#include "engine/core/application.h"
#include "engine/renderer/sprite_batch.h"
#include "engine/game2d/ldtk_loader.h"

#include <cmath>
#include <algorithm>

namespace Engine {

bool GameLayer::LoadLdtkMap(const std::string& path) {
    if (!LdtkLoader::Load(path, m_LdtkProject)) {
        m_UseLdtk = false;
        return false;
    }

    // 加载所有引用的 tileset 纹理
    for (auto& level : m_LdtkProject.levels) {
        for (auto& layer : level.layers) {
            if (layer.tilesetRelPath.empty()) continue;

            // 统一 tileset 路径分隔符为 /
            std::replace(layer.tilesetRelPath.begin(),
                         layer.tilesetRelPath.end(), '\\', '/');

            if (m_LdtkTilesets.count(layer.tilesetRelPath)) continue;

            // 构建完整路径
            std::string fullPath = m_LdtkProject.basePath + layer.tilesetRelPath;
            auto tex = std::make_shared<Texture2D>(fullPath);
            if (tex->IsValid()) {
                tex->SetFilterNearest();  // Pixel Art
                LOG_INFO("[LDtk] 加载 tileset: %s (%ux%u)",
                         layer.tilesetRelPath.c_str(),
                         tex->GetWidth(), tex->GetHeight());
            } else {
                LOG_WARN("[LDtk] tileset 加载失败: %s", fullPath.c_str());
            }
            m_LdtkTilesets[layer.tilesetRelPath] = tex;
        }
    }

    // ── IntGrid → NavGrid 碰撞同步 ───────────────────
    if (!m_LdtkProject.levels.empty()) {
        auto& level = m_LdtkProject.levels[0];
        for (auto& layer : level.layers) {
            if (layer.type != "IntGrid") continue;
            if (layer.identifier.find("Collision") == std::string::npos &&
                layer.identifier.find("collision") == std::string::npos) continue;

            if (layer.intGrid.empty()) continue;

            auto& nav = m_GameMap.GetNavGrid();
            u32 navW = (u32)layer.gridW;
            u32 navH = (u32)layer.gridH;
            nav = NavGrid(navW, navH, 1.0f);

            auto& tilemap = m_GameMap.GetTilemap();
            tilemap = Tilemap(navW, navH, 16);
            tilemap.AddLayer("collisions", 0);

            i32 count = 0;
            for (i32 y = 0; y < layer.gridH; y++) {
                for (i32 x = 0; x < layer.gridW; x++) {
                    i32 idx = y * layer.gridW + x;
                    if (idx >= (i32)layer.intGrid.size()) break;
                    bool blocked = (layer.intGrid[idx] > 0);
                    i32 flippedY = navH - 1 - y;
                    if (blocked) {
                        nav.SetWalkable(x, flippedY, false);
                        tilemap.SetTile(0, x, flippedY, {1});
                        count++;
                    } else {
                        nav.SetWalkable(x, flippedY, true);
                        tilemap.SetTile(0, x, flippedY, {0});
                    }
                }
            }
            LOG_INFO("[LDtk] 碰撞层 '%s': %dx%d, %d 个不可行走格子",
                     layer.identifier.c_str(), navW, navH, count);
            break;
        }

        // ── Entity 层 → Spawn Points ───────────────────
        m_GameMap.ClearSpawns();

        for (auto& layer : level.layers) {
            if (layer.type != "Entities") continue;
            i32 gs = layer.gridSize > 0 ? layer.gridSize : m_LdtkProject.defaultGridSize;
            f32 levelH = (f32)level.pxHei / (f32)gs;

            for (auto& entity : layer.entities) {
                f32 worldX = (f32)entity.px_x / (f32)gs;
                f32 entityH = (f32)entity.height / (f32)gs;
                f32 worldY = levelH - ((f32)entity.px_y / (f32)gs) - entityH;

                if (entity.identifier == "PlayerSpawn") {
                    m_GameMap.SetPlayerSpawn({worldX, worldY});
                    LOG_INFO("[LDtk] 玩家出生点: (%.1f, %.1f)", worldX, worldY);
                } else if (entity.identifier == "EnemySpawn" ||
                           entity.identifier == "ZombieSpawn") {
                    m_GameMap.AddZombieSpawn({worldX, worldY});
                    LOG_INFO("[LDtk] 敌人刷新点: (%.1f, %.1f)", worldX, worldY);
                } else if (entity.identifier == "LootPoint") {
                    m_GameMap.AddLootPoint({worldX, worldY});
                    LOG_INFO("[LDtk] 物资点: (%.1f, %.1f)", worldX, worldY);
                } else {
                    LOG_INFO("[LDtk] Entity '%s' at (%.1f, %.1f)",
                             entity.identifier.c_str(), worldX, worldY);
                }
            }
        }

        // ── Level 边界 → 相机世界约束 ─────────────────
        i32 gs = m_LdtkProject.defaultGridSize;
        f32 levelW = (f32)level.pxWid / (f32)gs;
        f32 levelH = (f32)level.pxHei / (f32)gs;
        if (levelW > 0 && levelH > 0) {
            m_CamCtrl.SetWorldBounds({0, 0}, {levelW, levelH});
            LOG_INFO("[LDtk] 相机边界设为: %.0fx%.0f tiles", levelW, levelH);
        }
    }

    m_UseLdtk = true;
    return true;
}

void GameLayer::RenderLdtkMap() {
    if (m_LdtkProject.levels.empty()) return;

    auto& level = m_LdtkProject.levels[0];

    glm::vec2 camCenter = m_CamCtrl.GetPosition();
    f32 zoom = m_CamCtrl.GetZoom();
    f32 viewW = BASE_VIEW_WIDTH / zoom;
    f32 viewH = BASE_VIEW_HEIGHT / zoom;
    glm::vec2 camPos = camCenter - glm::vec2(viewW * 0.5f, viewH * 0.5f);

    auto& window = Application::Get().GetWindow();
    f32 screenW = (f32)window.GetWidth();
    f32 screenH = (f32)window.GetHeight();
    f32 ppu = screenW / viewW;

    i32 gridSize = m_LdtkProject.defaultGridSize;
    f32 tileWorldSize = 1.0f;

    // 从后往前渲染层
    for (i32 li = (i32)level.layers.size() - 1; li >= 0; li--) {
        auto& layer = level.layers[li];
        if (layer.tiles.empty()) continue;
        if (layer.tilesetRelPath.empty()) continue;

        auto it = m_LdtkTilesets.find(layer.tilesetRelPath);
        if (it == m_LdtkTilesets.end() || !it->second || !it->second->IsValid())
            continue;

        auto& tex = it->second;
        f32 texW = (f32)tex->GetWidth();
        f32 texH = (f32)tex->GetHeight();
        f32 gs = (f32)layer.gridSize;

        f32 layerCamX = camPos.x * layer.parallaxFactorX;
        f32 layerCamY = camPos.y * layer.parallaxFactorY;
        f32 layerViewW = viewW;
        f32 layerViewH = viewH;

        glm::vec4 layerTint = glm::vec4(1.0f, 1.0f, 1.0f, layer.opacity);

        f32 levelH = (f32)level.pxHei / (f32)gs;

        for (auto& tile : layer.tiles) {
            f32 worldX = (f32)tile.px_x / gs;
            f32 worldY = levelH - ((f32)tile.px_y / gs) - tileWorldSize;

            if (worldX + tileWorldSize < layerCamX || worldX > layerCamX + layerViewW) continue;
            if (worldY + tileWorldSize < layerCamY || worldY > layerCamY + layerViewH) continue;

            f32 sx = std::round((worldX - layerCamX) * ppu);
            f32 sy = std::round(screenH - (worldY - layerCamY + tileWorldSize) * ppu);
            f32 tw = std::round((worldX + tileWorldSize - layerCamX) * ppu) - sx;
            f32 th = std::round(screenH - (worldY - layerCamY) * ppu) - sy;

            f32 halfPxU = 0.5f / texW;
            f32 halfPxV = 0.5f / texH;
            f32 u0 = tile.src_x / texW + halfPxU;
            f32 u1 = (tile.src_x + gs) / texW - halfPxU;
            f32 v0 = 1.0f - (tile.src_y + gs) / texH + halfPxV;
            f32 v1 = 1.0f - tile.src_y / texH - halfPxV;

            if (tile.flip & 1) std::swap(u0, u1);
            if (tile.flip & 2) std::swap(v0, v1);

            SpriteBatch::Draw(tex, {sx, sy}, {tw, th}, {u0, v0, u1, v1}, 0.0f, layerTint);
        }
    }
}

} // namespace Engine
