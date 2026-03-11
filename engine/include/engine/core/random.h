#pragma once

// ── 随机数工具 ──────────────────────────────────────────────
// 替代 std::rand()，线程安全、无 modulo bias

#include "engine/core/types.h"
#include <random>

namespace Engine {

class Random {
public:
    /// 返回 [min, max] 范围内的均匀整数
    static u32 UInt(u32 min, u32 max) {
        std::uniform_int_distribution<u32> dist(min, max);
        return dist(GetEngine());
    }

    /// 返回 [0, max) 范围内的均匀整数 (常用于 % 替代)
    static u32 UInt(u32 max) {
        if (max == 0) return 0;
        std::uniform_int_distribution<u32> dist(0, max - 1);
        return dist(GetEngine());
    }

    /// 返回 [min, max] 范围内的均匀浮点数
    static f32 Float(f32 min, f32 max) {
        std::uniform_real_distribution<f32> dist(min, max);
        return dist(GetEngine());
    }

    /// 返回 [0.0, 1.0) 范围内的均匀浮点数
    static f32 Float01() {
        return Float(0.0f, 1.0f);
    }

    /// 设置种子
    static void Seed(u32 seed) {
        GetEngine().seed(seed);
    }

private:
    static std::mt19937& GetEngine() {
        static std::mt19937 s_Engine{std::random_device{}()};
        return s_Engine;
    }
};

} // namespace Engine
