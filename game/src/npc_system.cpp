#include "game/npc_system.h"
#include "engine/game2d/sprite2d.h"
#include "engine/core/log.h"

#include <cmath>

namespace Engine {

void NPCSystem::Update(ECSWorld& world, f32 dt) {
    world.ForEach<NPCComponent>([&](Entity e, NPCComponent& npc) {
        if (!npc.IsMoving || npc.Schedule.empty()) return;
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) return;
        if (npc.CurrentScheduleIdx >= npc.Schedule.size()) return;

        auto& target = npc.Schedule[npc.CurrentScheduleIdx];
        glm::vec2 pos = {tr->X, tr->Y};
        glm::vec2 dir = target.TargetPos - pos;
        f32 dist = glm::length(dir);

        if (dist < 0.1f) {
            tr->X = target.TargetPos.x;
            tr->Y = target.TargetPos.y;
            npc.IsMoving = false;
            auto* anim = world.GetComponent<SpriteAnimatorComponent>(e);
            if (anim) anim->Play(target.Animation);
        } else {
            dir /= dist;
            f32 step = std::min(npc.MoveSpeed * dt, dist);
            tr->X += dir.x * step;
            tr->Y += dir.y * step;
            auto* anim = world.GetComponent<SpriteAnimatorComponent>(e);
            if (anim) {
                if (std::abs(dir.y) >= std::abs(dir.x))
                    anim->Play(dir.y > 0 ? "walk_up" : "walk_down");
                else
                    anim->Play(dir.x > 0 ? "walk_right" : "walk_left");
            }
        }
    });
}

i32 NPCSystem::GiveGift(NPCComponent& npc, u32 itemID) {
    if (npc.GiftedToday) return 0;
    npc.GiftedToday = true;
    i32 delta = 20;
    if (npc.LovedGifts.count(itemID))       delta = 80;
    else if (npc.LikedGifts.count(itemID))  delta = 45;
    else if (npc.DislikedGifts.count(itemID)) delta = -20;
    else if (npc.HatedGifts.count(itemID))  delta = -40;
    npc.Friendship = std::max(0, std::min(npc.Friendship + delta, npc.MaxFriendship));
    LOG_INFO("[NPC] %s 好感度 %+d → %d", npc.Name.c_str(), delta, npc.Friendship);
    return delta;
}

void NPCSystem::AdvanceDay(ECSWorld& world) {
    world.ForEach<NPCComponent>([](Entity e, NPCComponent& npc) {
        (void)e;
        npc.TalkedToday = false;
        npc.GiftedToday = false;
        npc.CurrentScheduleIdx = 0;
        npc.IsMoving = false;
    });
}

void NPCSystem::LoadSchedules(ECSWorld& world, Season season, u32 dayOfWeek) {
    (void)world; (void)season; (void)dayOfWeek;
}

} // namespace Engine
