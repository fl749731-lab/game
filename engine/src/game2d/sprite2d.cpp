#include "engine/game2d/sprite2d.h"

namespace Engine {

void SpriteAnimationSystem::Update(ECSWorld& world, f32 dt) {
    world.ForEach2<SpriteAnimatorComponent, Sprite2DComponent>(
        [dt](Entity e, SpriteAnimatorComponent& anim, Sprite2DComponent& sprite) {
            if (!anim.Playing || anim.CurrentAnim.empty()) return;

            auto it = anim.Animations.find(anim.CurrentAnim);
            if (it == anim.Animations.end()) return;

            auto& animation = it->second;
            if (animation.Frames.empty()) return;

            // 推进计时器
            anim.Timer += dt;

            const auto& frame = animation.Frames[anim.CurrentFrame];
            if (anim.Timer >= frame.Duration) {
                anim.Timer -= frame.Duration;
                anim.CurrentFrame++;

                if (anim.CurrentFrame >= animation.Frames.size()) {
                    if (animation.Loop) {
                        anim.CurrentFrame = 0;
                    } else {
                        anim.CurrentFrame = (u32)animation.Frames.size() - 1;
                        anim.Playing = false;
                    }
                }
            }

            // 应用当前帧到 Sprite
            sprite.Region = animation.Frames[anim.CurrentFrame].Region;
        }
    );
}

} // namespace Engine
