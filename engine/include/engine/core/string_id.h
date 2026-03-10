#pragma once

// ── StringID —— 编译期 / 运行时字符串哈希 ───────────────────
//
// 用途: 替代 std::string 做字典 key，O(1) 整数比较
// 算法: FNV-1a 32-bit
//
// 用法:
//   constexpr StringID id = SID("player_texture");
//   auto* tex = resources.Get(id);
//
//   if (sid == SID("cube")) { ... }

#include "engine/core/types.h"
#include <string>
#include <string_view>

namespace Engine {

/// 字符串哈希 ID — 轻量整数类型，用于替代 std::string key
using StringID = u32;

/// 无效 / 空 StringID
constexpr StringID INVALID_SID = 0;

// ── FNV-1a 32-bit 常量 ─────────────────────────────────────
namespace detail {
    constexpr u32 FNV_OFFSET = 2166136261u;
    constexpr u32 FNV_PRIME  = 16777619u;

    /// 编译期 FNV-1a 递归
    constexpr u32 fnv1a(const char* str, u32 hash = FNV_OFFSET) {
        return (*str == '\0') ? hash
               : fnv1a(str + 1, (hash ^ static_cast<u32>(*str)) * FNV_PRIME);
    }
} // namespace detail

// ── 编译期字符串哈希 ────────────────────────────────────────

/// 编译期 constexpr 哈希 (字面量)
constexpr StringID SID(const char* str) {
    return detail::fnv1a(str);
}

/// 运行时哈希 (std::string / std::string_view)
inline StringID SID(std::string_view str) {
    u32 hash = detail::FNV_OFFSET;
    for (char c : str) {
        hash ^= static_cast<u32>(c);
        hash *= detail::FNV_PRIME;
    }
    return hash;
}

// ── 用户自定义字面量 ────────────────────────────────────────
// 用法: auto id = "player"_sid;

constexpr StringID operator""_sid(const char* str, std::size_t) {
    return detail::fnv1a(str);
}

// ── Debug: StringID → 原始字符串反查 ────────────────────────
// 仅在 Debug 模式下使用，用于日志和编辑器

#ifndef NDEBUG
#include <unordered_map>

class StringIDRegistry {
public:
    static StringIDRegistry& Get() {
        static StringIDRegistry s_Instance;
        return s_Instance;
    }

    /// 注册 StringID ↔ 字符串 映射
    void Register(StringID id, std::string_view str) {
        m_Map[id] = std::string(str);
    }

    /// 反查原始字符串 (未注册时返回 "<unknown:0xHEX>")
    const std::string& Resolve(StringID id) const {
        static const std::string s_Unknown = "<unknown>";
        auto it = m_Map.find(id);
        return (it != m_Map.end()) ? it->second : s_Unknown;
    }

private:
    std::unordered_map<StringID, std::string> m_Map;
};

/// 注册并返回 StringID (Debug 模式 — 同时记录原始字符串)
inline StringID SID_DEBUG(const char* str) {
    StringID id = SID(str);
    StringIDRegistry::Get().Register(id, str);
    return id;
}

#else
// Release 模式: SID_DEBUG 等同于 SID
inline StringID SID_DEBUG(const char* str) { return SID(str); }
#endif

} // namespace Engine
