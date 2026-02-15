#include "game/player_controller.h"
#include "engine/game2d/sprite2d.h"
#include "engine/platform/input.h"

#include <cmath>

namespace Engine {

void PlayerControlSystem::Update(ECSWorld& world, f32 dt) {
    world.ForEach<PlayerComponent>([&](Entity e, PlayerComponent& player) {
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) return;

        if (player.IsUsingTool) {
            player.ToolTimer -= dt;
            if (player.ToolTimer <= 0.0f) player.IsUsingTool = false;
            return;
        }

        glm::vec2 moveDir = {0, 0};
        if (Input::IsKeyDown(Key::W) || Input::IsKeyDown(Key::Up))    moveDir.y += 1;
        if (Input::IsKeyDown(Key::S) || Input::IsKeyDown(Key::Down))  moveDir.y -= 1;
        if (Input::IsKeyDown(Key::A) || Input::IsKeyDown(Key::Left))  moveDir.x -= 1;
        if (Input::IsKeyDown(Key::D) || Input::IsKeyDown(Key::Right)) moveDir.x += 1;

        player.IsMoving = (moveDir.x != 0.0f || moveDir.y != 0.0f);
        if (player.IsMoving) {
            f32 len = std::sqrt(moveDir.x * moveDir.x + moveDir.y * moveDir.y);
            moveDir /= len;
            tr->X += moveDir.x * player.MoveSpeed * dt;
            tr->Y += moveDir.y * player.MoveSpeed * dt;
            if (std::abs(moveDir.y) >= std::abs(moveDir.x))
                player.Facing = moveDir.y > 0 ? Direction::Up : Direction::Down;
            else
                player.Facing = moveDir.x > 0 ? Direction::Right : Direction::Left;
        }

        auto* anim = world.GetComponent<SpriteAnimatorComponent>(e);
        if (anim) {
            if (player.IsMoving) {
                switch (player.Facing) {
                    case Direction::Down:  anim->Play("walk_down");  break;
                    case Direction::Up:    anim->Play("walk_up");    break;
                    case Direction::Left:  anim->Play("walk_left");  break;
                    case Direction::Right: anim->Play("walk_right"); break;
                }
            } else {
                switch (player.Facing) {
                    case Direction::Down:  anim->Play("idle_down");  break;
                    case Direction::Up:    anim->Play("idle_up");    break;
                    case Direction::Left:  anim->Play("idle_left");  break;
                    case Direction::Right: anim->Play("idle_right"); break;
                }
            }
        }

        if (Input::IsKeyJustPressed(Key::Space) && player.CurrentTool != ToolType::None) {
            player.IsUsingTool = true;
            player.ToolTimer = player.ToolCooldown;
            player.Stamina -= 2.0f;
            if (player.Stamina < 0) player.Stamina = 0;
        }
    });
}

} // namespace Engine
