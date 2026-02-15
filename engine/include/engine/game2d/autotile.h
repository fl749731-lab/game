#pragma once

#include "engine/core/types.h"
#include <glm/glm.hpp>

namespace Engine {

// ══════════════════════════════════════════════════════════════
//  4-bit Autotile — 基于 Godot/Tiled 规范的位掩码系统
// ══════════════════════════════════════════════════════════════
//
//  位掩码编码:  上=1, 右=2, 下=4, 左=8
//  位=1 表示该方向邻居是同一地形 (不需要泥土边缘)
//  位=0 表示该方向是不同地形 (需要显示泥土过渡边缘)
//
//  Valley Ruin 176×80 autotile 图集 (像素级扫描确认):
//
//  col:  0          1          2          3
//  r0:  [TL角]     [T边]      [TR角]     [上左右泥]
//  r1:  [L边]      [C填充]    [R边]      [左右泥]
//  r2:  [BL角]     [B边]      [BR角]     [下左右泥]
//  r3:  [上下左泥] [上下泥]   [上下右泥] [孤岛]
//  r4:  [空]       [空]       [空]       [空]
//
//  stbi 已翻转 Y 轴: PNG row0 → v ≈ 1.0, row4 → v ≈ 0.0
//

struct AutotileSet {
    f32 TexW = 176.0f;
    f32 TexH = 80.0f;
    f32 TileW = 16.0f;
    f32 TileH = 16.0f;

    struct TilePos { f32 px, py; };
    TilePos Tiles[16];

    /// 计算归一化 UV rect {u0, v0, u1, v1}
    glm::vec4 GetUV(u8 bitmask) const {
        bitmask &= 0x0F;
        auto& t = Tiles[bitmask];

        f32 u0 = t.px / TexW;
        f32 u1 = (t.px + TileW) / TexW;

        // stbi Y 翻转: PNG py=0 → v=1.0
        f32 v0 = 1.0f - (t.py + TileH) / TexH;
        f32 v1 = 1.0f - t.py / TexH;

        return {u0, v0, u1, v1};
    }

    /// 计算 4 方向位掩码
    static u8 CalcBitmask(u16 terrainID, u16 nUp, u16 nRight,
                          u16 nDown, u16 nLeft) {
        u8 mask = 0;
        if (nUp    == terrainID) mask |= 1;
        if (nRight == terrainID) mask |= 2;
        if (nDown  == terrainID) mask |= 4;
        if (nLeft  == terrainID) mask |= 8;
        return mask;
    }
};

// ══════════════════════════════════════════════════════════════
//  工厂 — Valley Ruin 4×4 独立 autotile (像素级扫描验证)
// ══════════════════════════════════════════════════════════════
//
//  每个 bitmask (0-15) 都有唯一对应的 tile, 不复用:
//
//  mask | 二进制 | 同类邻居   | 泥土边在…    | tile
//  -----|--------|-----------|-------------|--------
//    0  | 0000   | 无        | 上右下左     | 孤岛   [3,3]
//    1  | 0001   | 上        | 右下左       | 上下左泥 → 下半 [0,3]? → B+L+R? 不对
//
//  正确逻辑(泥土边 = bitmask 为0 的方向):
//    mask=0  → 泥上 泥右 泥下 泥左 → ISLAND    [3,3]
//    mask=1  → 草上 泥右 泥下 泥左 → 下左右泥  [3,2]
//    mask=2  → 泥上 草右 泥下 泥左 → 上下左泥  [0,3]
//    mask=3  → 草上 草右 泥下 泥左 → BL角      [0,2]
//    mask=4  → 泥上 泥右 草下 泥左 → 上左右泥  [3,0]
//    mask=5  → 草上 泥右 草下 泥左 → 左右泥    [3,1]
//    mask=6  → 泥上 草右 草下 泥左 → TL角      [0,0]
//    mask=7  → 草上 草右 草下 泥左 → L边       [0,1]
//    mask=8  → 泥上 泥右 泥下 草左 → 上下右泥  [2,3]
//    mask=9  → 草上 泥右 泥下 草左 → 下右泥    [2,2] = BR
//    mask=10 → 泥上 草右 泥下 草左 → 上下泥    [1,3]
//    mask=11 → 草上 草右 泥下 草左 → B边       [1,2]
//    mask=12 → 泥上 泥右 草下 草左 → 上右泥    [2,0] = TR
//    mask=13 → 草上 泥右 草下 草左 → R边       [2,1]
//    mask=14 → 泥上 草右 草下 草左 → T边       [1,0]
//    mask=15 → 草上 草右 草下 草左 → 填充      [1,1]

inline AutotileSet CreateStandardAutotile(f32 texW = 176.0f, f32 texH = 80.0f) {
    AutotileSet set;
    set.TexW = texW;
    set.TexH = texH;

    constexpr f32 S = 16.0f;

    // 按 [col, row] 像素坐标定义所有 16 种 tile
    // 坐标 = (col * 16, row * 16) 在原始 PNG 坐标系中

    //          位掩码    泥边方向          tile位置
    set.Tiles[0]  = {S*3, S*3};  // 0000  上右下左泥  = 孤岛   [3,3]
    set.Tiles[1]  = {S*3, S*2};  // 0001  右下左泥    = 下左右泥 [3,2]
    set.Tiles[2]  = {S*0, S*3};  // 0010  上下左泥    = 上下左泥 [0,3]
    set.Tiles[3]  = {S*0, S*2};  // 0011  下左泥      = BL角   [0,2]
    set.Tiles[4]  = {S*3, S*0};  // 0100  上左右泥    = 上左右泥 [3,0]
    set.Tiles[5]  = {S*3, S*1};  // 0101  左右泥      = 左右泥  [3,1]
    set.Tiles[6]  = {S*0, S*0};  // 0110  上左泥      = TL角   [0,0]
    set.Tiles[7]  = {S*0, S*1};  // 0111  左泥        = L边    [0,1]
    set.Tiles[8]  = {S*2, S*3};  // 1000  上下右泥    = 上下右泥 [2,3]
    set.Tiles[9]  = {S*2, S*2};  // 1001  下右泥      = BR角   [2,2]
    set.Tiles[10] = {S*1, S*3};  // 1010  上下泥      = 上下泥  [1,3]
    set.Tiles[11] = {S*1, S*2};  // 1011  下泥        = B边    [1,2]
    set.Tiles[12] = {S*2, S*0};  // 1100  上右泥      = TR角   [2,0]
    set.Tiles[13] = {S*2, S*1};  // 1101  右泥        = R边    [2,1]
    set.Tiles[14] = {S*1, S*0};  // 1110  上泥        = T边    [1,0]
    set.Tiles[15] = {S*1, S*1};  // 1111  无泥        = 填充   [1,1]

    return set;
}

} // namespace Engine

