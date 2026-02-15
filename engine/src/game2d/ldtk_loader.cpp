#include "engine/game2d/ldtk_loader.h"
#include "engine/core/log.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;

namespace Engine {

bool LdtkLoader::Load(const std::string& path, LdtkProject& project) {
    // ── 读取 JSON 文件 ─────────────────────────────
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("[LDtk] 无法打开文件: %s", path.c_str());
        return false;
    }

    json root;
    try {
        root = json::parse(file);
    } catch (const json::parse_error& e) {
        LOG_ERROR("[LDtk] JSON 解析失败: %s", e.what());
        return false;
    }
    file.close();

    // ── 基路径 ─────────────────────────────────────
    std::filesystem::path fsPath(path);
    project.basePath = fsPath.parent_path().string();
    if (!project.basePath.empty() && project.basePath.back() != '/' && project.basePath.back() != '\\') {
        project.basePath += '/';
    }

    project.defaultGridSize = root.value("defaultGridSize", 16);

    LOG_INFO("[LDtk] 加载项目: %s (网格 %d px)", path.c_str(), project.defaultGridSize);

    // ── 解析 Tileset 定义 (uid → relPath 映射) ────
    std::unordered_map<i32, std::string> tilesetPaths;
    std::unordered_map<i32, std::pair<i32,i32>> tilesetSizes;

    if (root.contains("defs") && root["defs"].contains("tilesets")) {
        for (auto& ts : root["defs"]["tilesets"]) {
            i32 uid = ts.value("uid", -1);
            std::string relPath = ts.value("relPath", "");
            i32 pxW = ts.value("pxWid", 0);
            i32 pxH = ts.value("pxHei", 0);
            tilesetPaths[uid] = relPath;
            tilesetSizes[uid] = {pxW, pxH};
        }
    }

    // ── 解析 Levels ────────────────────────────────
    const auto& levels = root.contains("levels") ? root["levels"] : root["levels"];

    for (auto& lvl : levels) {
        LdtkLevel level;
        level.identifier = lvl.value("identifier", "unnamed");
        level.uid = lvl.value("uid", 0);
        level.worldX = lvl.value("worldX", 0);
        level.worldY = lvl.value("worldY", 0);
        level.pxWid = lvl.value("pxWid", 0);
        level.pxHei = lvl.value("pxHei", 0);

        LOG_INFO("[LDtk]   Level: %s (%dx%d px)", level.identifier.c_str(),
                 level.pxWid, level.pxHei);

        // ── 解析 layerInstances ────────────────────
        if (!lvl.contains("layerInstances")) continue;
        const auto& layerArr = lvl["layerInstances"];

        for (auto& li : layerArr) {
            LdtkLayer layer;
            layer.identifier = li.value("__identifier", "");
            layer.type = li.value("__type", "");
            layer.gridSize = li.value("__gridSize", 16);
            layer.gridW = li.value("__cWid", 0);
            layer.gridH = li.value("__cHei", 0);
            layer.pxOffsetX = li.value("__pxTotalOffsetX", 0);
            layer.pxOffsetY = li.value("__pxTotalOffsetY", 0);

            // Tileset 信息
            i32 tsUid = li.value("__tilesetDefUid", -1);
            if (tsUid >= 0 && tilesetPaths.count(tsUid)) {
                layer.tilesetRelPath = tilesetPaths[tsUid];
                layer.tilesetW = tilesetSizes[tsUid].first;
                layer.tilesetH = tilesetSizes[tsUid].second;
            }

            // ── 解析 Tiles ──────────────────────────
            // autoLayerTiles (Auto-Layer 生成的) 和
            // gridTiles (手动放置的) 合并

            auto parseTiles = [&](const json& arr) {
                for (auto& t : arr) {
                    LdtkTile tile;
                    if (t.contains("px") && t["px"].size() >= 2) {
                        tile.px_x = t["px"][0].get<i32>();
                        tile.px_y = t["px"][1].get<i32>();
                    }
                    if (t.contains("src") && t["src"].size() >= 2) {
                        tile.src_x = t["src"][0].get<i32>();
                        tile.src_y = t["src"][1].get<i32>();
                    }
                    tile.flip = t.value("f", 0);
                    layer.tiles.push_back(tile);
                }
            };

            if (li.contains("autoLayerTiles")) {
                parseTiles(li["autoLayerTiles"]);
            }
            if (li.contains("gridTiles")) {
                parseTiles(li["gridTiles"]);
            }

            // ── IntGrid (碰撞数据等) ─────────────────
            if (li.contains("intGridCsv")) {
                layer.intGrid.clear();
                for (auto& v : li["intGridCsv"]) {
                    layer.intGrid.push_back(v.get<i32>());
                }
            }

            LOG_INFO("[LDtk]     Layer: %s (%s) %dx%d, %zu tiles",
                     layer.identifier.c_str(), layer.type.c_str(),
                     layer.gridW, layer.gridH, layer.tiles.size());

            level.layers.push_back(std::move(layer));
        }

        project.levels.push_back(std::move(level));
    }

    LOG_INFO("[LDtk] 加载完成: %zu levels", project.levels.size());
    return true;
}

} // namespace Engine
