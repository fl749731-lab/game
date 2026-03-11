#pragma once

// ── ECS 基础类型 ────────────────────────────────────────────
// Entity、Component 基类等基础定义，独立于 ECSWorld
// 供 components.h 和 ecs.h 同时引用，避免循环包含

#include "engine/core/types.h"

namespace Engine {

// ── Entity —— 只是一个 ID ───────────────────────────────────

using Entity = u32;
constexpr Entity INVALID_ENTITY = ~Entity(0);  // UINT32_MAX

// ── Component 基类 ──────────────────────────────────────────

struct Component {
    virtual ~Component() = default;
};

} // namespace Engine
