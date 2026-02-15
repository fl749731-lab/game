#include "game/interaction_system.h"
#include "game/player_controller.h"
#include "engine/game2d/sprite2d.h"
#include "engine/platform/input.h"

namespace Engine {

void InteractionSystem::Update(ECSWorld& world, f32 dt) {
    (void)dt;
    if (!Input::IsKeyJustPressed(Key::E)) return;

    world.ForEach<PlayerComponent>([&](Entity playerE, PlayerComponent& player) {
        auto* playerTr = world.GetComponent<TransformComponent>(playerE);
        if (!playerTr) return;

        glm::vec2 playerPos = {playerTr->X, playerTr->Y};
        glm::ivec2 facingOffset = player.GetFacingOffset();
        glm::vec2 targetPos = playerPos + glm::vec2(facingOffset);

        world.ForEach<InteractableComponent>([&](Entity ie, InteractableComponent& interactable) {
            auto* iTr = world.GetComponent<TransformComponent>(ie);
            if (!iTr) return;
            f32 dist = glm::length(glm::vec2(iTr->X, iTr->Y) - targetPos);
            if (dist <= interactable.Range) {
                InteractionEvent event;
                event.Source = playerE;
                event.TargetTile = glm::ivec2((i32)iTr->X, (i32)iTr->Y);
                event.InteractType = interactable.Type;
                event.TargetEntity = ie;
                for (auto& cb : m_Callbacks) cb(event);
            }
        });

        world.ForEach<ScenePortalComponent>([&](Entity pe, ScenePortalComponent& portal) {
            (void)portal;
            auto* pTr = world.GetComponent<TransformComponent>(pe);
            if (!pTr) return;
            f32 dist = glm::length(glm::vec2(pTr->X, pTr->Y) - playerPos);
            if (dist <= 0.8f) {
                InteractionEvent event;
                event.Source = playerE;
                event.TargetTile = glm::ivec2((i32)pTr->X, (i32)pTr->Y);
                event.InteractType = 0xFF;
                event.TargetEntity = pe;
                for (auto& cb : m_Callbacks) cb(event);
            }
        });
    });
}

} // namespace Engine
