#include "game/player_controller.h"
#include "engine/game2d/sprite2d.h"
#include "engine/platform/input.h"

#include <cmath>

namespace Engine {

void PlayerControlSystem::Update(ECSWorld& world, f32 dt) {
    world.ForEach<PlayerComponent>([&](Entity e, PlayerComponent& player) {
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) return;

        // 工具使用中，仅更新计时器
        if (player.m_isUsingTool) {
            player.m_toolTimer -= dt;
            if (player.m_toolTimer <= 0.0f) {
                player.m_isUsingTool = false;
            }
            return;
        }

        // 处理移动输入
        glm::vec2 moveDir = {0, 0};
        if (Input::IsKeyDown(Key::W) || Input::IsKeyDown(Key::Up))    moveDir.y += 1;
        if (Input::IsKeyDown(Key::S) || Input::IsKeyDown(Key::Down))  moveDir.y -= 1;
        if (Input::IsKeyDown(Key::A) || Input::IsKeyDown(Key::Left))  moveDir.x -= 1;
        if (Input::IsKeyDown(Key::D) || Input::IsKeyDown(Key::Right)) moveDir.x += 1;

        // 标准化移动向量
        player.m_isMoving = (moveDir.x != 0.0f || moveDir.y != 0.0f);
        
        if (player.m_isMoving) {
            const f32 len = std::sqrt(moveDir.x * moveDir.x + moveDir.y * moveDir.y);
            moveDir /= len;
            
            tr->X += moveDir.x * player.m_moveSpeed * dt;
            tr->Y += moveDir.y * player.m_moveSpeed * dt;
            
            // 更新朝向：Y轴移动优先于X轴
            if (std::abs(moveDir.y) >= std::abs(moveDir.x)) {
                player.m_facing = moveDir.y > 0 ? Direction::Up : Direction::Down;
            } else {
                player.m_facing = moveDir.x > 0 ? Direction::Right : Direction::Left;
            }
        }

        // 更新动画
        auto* anim = world.GetComponent<SpriteAnimatorComponent>(e);
        if (anim) {
            const size_t dirIdx = GetDirectionIndex(player.m_facing);
            const size_t moveIdx = player.m_isMoving ? 1 : 0;
            anim->Play(s_animationNames[dirIdx][moveIdx]);
        }

        // 处理工具使用
        if (Input::IsKeyJustPressed(Key::Space) && player.m_currentTool != ToolType::None) {
            player.m_isUsingTool = true;
            player.m_toolTimer = player.m_toolCooldown;
            player.m_stamina = std::max(0.0f, player.m_stamina - 2.0f);
        }
    });
}

} // namespace Engine
