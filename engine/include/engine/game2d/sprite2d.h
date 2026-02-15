#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"
#include "engine/renderer/texture.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace Engine {

// ── Sprite 子区域 (图集中的一帧) ───────────────────────────

struct SpriteRect {
    f32 U0 = 0.0f, V0 = 0.0f;   // 左下角 UV
    f32 U1 = 1.0f, V1 = 1.0f;   // 右上角 UV

    /// 从像素坐标构建 UV (需要图集尺寸)
    static SpriteRect FromPixels(u32 x, u32 y, u32 w, u32 h,
                                  u32 atlasW, u32 atlasH) {
        SpriteRect r;
        r.U0 = (f32)x / atlasW;
        r.V0 = (f32)y / atlasH;
        r.U1 = (f32)(x + w) / atlasW;
        r.V1 = (f32)(y + h) / atlasH;
        return r;
    }
};

// ── Sprite2D 组件 ──────────────────────────────────────────

struct Sprite2DComponent : public Component {
    std::string TextureName;         // ResourceManager 中的纹理名
    SpriteRect  Region;              // 图集子区域 (默认=整张纹理)
    glm::vec4   Tint = {1,1,1,1};   // 色调/透明度
    glm::vec2   Size = {1.0f, 1.0f}; // 世界空间尺寸 (Tile 单位)
    glm::vec2   Pivot = {0.5f, 0.5f}; // 锚点 (0,0=左下 1,1=右上)
    i32         ZOrder = 0;          // 绘制排序层
    bool        FlipX = false;
    bool        FlipY = false;
    bool        Visible = true;
};

// ── 帧动画 ─────────────────────────────────────────────────

struct AnimationFrame {
    SpriteRect Region;
    f32 Duration = 0.1f;   // 秒
};

struct SpriteAnimation {
    std::string Name;
    std::vector<AnimationFrame> Frames;
    bool Loop = true;
};

// ── Sprite 动画器组件 ──────────────────────────────────────

struct SpriteAnimatorComponent : public Component {
    std::unordered_map<std::string, SpriteAnimation> Animations;
    std::string CurrentAnim;
    u32  CurrentFrame = 0;
    f32  Timer = 0.0f;
    bool Playing = true;

    void Play(const std::string& animName) {
        if (CurrentAnim == animName && Playing) return;
        CurrentAnim = animName;
        CurrentFrame = 0;
        Timer = 0.0f;
        Playing = true;
    }

    void Stop() { Playing = false; }

    void AddAnimation(const std::string& name, const SpriteAnimation& anim) {
        Animations[name] = anim;
    }

    /// 便捷: 从 SpriteSheet 切片创建动画
    /// row/col 是起始帧, count 是帧数, 每帧宽高 frameW*frameH
    void AddFromSheet(const std::string& name,
                      u32 startCol, u32 startRow,
                      u32 frameCount, u32 frameW, u32 frameH,
                      u32 atlasW, u32 atlasH,
                      f32 frameDuration = 0.1f, bool loop = true) {
        SpriteAnimation anim;
        anim.Name = name;
        anim.Loop = loop;
        for (u32 i = 0; i < frameCount; i++) {
            u32 col = startCol + i;
            u32 row = startRow;
            // 自动换行
            while (col * frameW >= atlasW) {
                col -= atlasW / frameW;
                row++;
            }
            AnimationFrame f;
            f.Region = SpriteRect::FromPixels(col * frameW, row * frameH,
                                               frameW, frameH, atlasW, atlasH);
            f.Duration = frameDuration;
            anim.Frames.push_back(f);
        }
        Animations[name] = anim;
    }
};

// ── Sprite 动画系统 ────────────────────────────────────────

class SpriteAnimationSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override;
    const char* GetName() const override { return "SpriteAnimationSystem"; }
};

} // namespace Engine
