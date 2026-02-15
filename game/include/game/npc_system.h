#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"
#include "game/farming.h"  // Season

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace Engine {

// ── NPC 日程点 ────────────────────────────────────────────

struct ScheduleEntry {
    u32       Hour = 8;
    u32       Minute = 0;
    glm::vec2 TargetPos;
    std::string Animation = "idle";
};

// ── NPC 组件 ──────────────────────────────────────────────

struct NPCComponent : public Component {
    std::string Name;
    std::string PortraitTexture;

    i32  Friendship = 0;
    i32  MaxFriendship = 1000;

    std::unordered_map<u32, i32> LovedGifts;
    std::unordered_map<u32, i32> LikedGifts;
    std::unordered_map<u32, i32> DislikedGifts;
    std::unordered_map<u32, i32> HatedGifts;

    std::vector<ScheduleEntry> Schedule;
    u32 CurrentScheduleIdx = 0;

    bool TalkedToday = false;
    bool GiftedToday = false;

    f32  MoveSpeed = 2.0f;
    bool IsMoving = false;
};

// ── NPC 系统 ──────────────────────────────────────────────

class NPCSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override;
    const char* GetName() const override { return "NPCSystem"; }

    i32 GiveGift(NPCComponent& npc, u32 itemID);
    void AdvanceDay(ECSWorld& world);
    void LoadSchedules(ECSWorld& world, Season season, u32 dayOfWeek);
};

} // namespace Engine
