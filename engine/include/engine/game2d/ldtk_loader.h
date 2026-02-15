#pragma once

#include "engine/core/types.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>

namespace Engine {

// ══════════════════════════════════════════════════════════════
//  LDtk 数据结构 — 轻量级, 只保留渲染相关数据
// ══════════════════════════════════════════════════════════════

/// 单个 Tile 实例 (来自 autoLayerTiles 或 gridTiles)
struct LdtkTile {
    i32 px_x = 0, px_y = 0;     // 在本层中的像素位置
    i32 src_x = 0, src_y = 0;   // 在 tileset PNG 中的像素坐标 (UV 来源)
    u8  flip = 0;               // 0=无 1=水平翻转 2=垂直翻转 3=两者
};

/// Entity 自定义字段值 (支持多类型)
using LdtkFieldValue = std::variant<i32, f32, bool, std::string>;

/// 单个 Entity 实体 (来自 Entities 层)
struct LdtkEntity {
    std::string identifier;     // 实体类型名 ("PlayerSpawn", "Enemy" 等)
    i32 px_x = 0, px_y = 0;    // 像素位置
    i32 width = 0, height = 0;  // 实体尺寸
    f32 pivotX = 0, pivotY = 0; // 锚点 (0~1)
    std::unordered_map<std::string, LdtkFieldValue> fields;  // 自定义字段
};

/// IntGrid 值 (用于碰撞检测等)
struct LdtkIntGridValue {
    i32 id;
    std::string name;
    glm::vec4 color;
};

/// 一个 Layer 实例
struct LdtkLayer {
    std::string identifier;     // 层名称 (如 "Ground", "Walls")
    std::string type;           // "AutoLayer", "Tiles", "IntGrid", "Entities"
    i32 gridSize;               // 格子大小 (像素)
    i32 gridW, gridH;           // 格子数
    i32 pxOffsetX, pxOffsetY;   // 层偏移

    // Tileset 信息
    std::string tilesetRelPath; // tileset 相对路径
    i32 tilesetW, tilesetH;     // tileset 尺寸 (像素)

    // Tile 数据 (autoLayerTiles + gridTiles 合并)
    std::vector<LdtkTile> tiles;

    // Entity 数据 (如果有)
    std::vector<LdtkEntity> entities;

    // IntGrid 数据 (如果有)
    std::vector<i32> intGrid;   // gridW * gridH, 值=-1 表示空
};

/// 一个 Level (关卡)
struct LdtkLevel {
    std::string identifier;     // 关卡名
    i32 uid;
    i32 worldX, worldY;         // 世界坐标
    i32 pxWid, pxHei;           // 关卡尺寸 (像素)
    std::vector<LdtkLayer> layers;
};

/// LDtk 项目
struct LdtkProject {
    std::string basePath;       // .ldtk 文件所在目录
    i32 defaultGridSize;
    std::vector<LdtkLevel> levels;
};

// ══════════════════════════════════════════════════════════════
//  LDtk 加载器
// ══════════════════════════════════════════════════════════════

class LdtkLoader {
public:
    /// 加载 .ldtk 文件 (JSON 格式)
    /// @param path .ldtk 文件路径
    /// @param project 输出项目数据
    /// @return 是否成功
    static bool Load(const std::string& path, LdtkProject& project);
};

} // namespace Engine
