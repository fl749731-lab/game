#pragma once

#include "engine/renderer/animation.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>

namespace Engine {

// ── 骨骼姿势（一帧中所有骨骼的局部变换）─────────────────────

struct BonePose {
    glm::vec3 Position = {0, 0, 0};
    glm::quat Rotation = glm::quat(1, 0, 0, 0);
    glm::vec3 Scale    = {1, 1, 1};
};

struct AnimPose {
    std::vector<BonePose> BonePoses;  // 索引对应骨骼索引

    void Resize(u32 boneCount) {
        BonePoses.resize(boneCount);
    }
};

// ── 姿势混合 ────────────────────────────────────────────────

class PoseBlender {
public:
    /// 两个姿势按权重线性混合
    /// result = lerp(a, b, weight)  weight=0 → a, weight=1 → b
    static void Blend(const AnimPose& a, const AnimPose& b, f32 weight, AnimPose& out);

    /// 叠加混合：out = base + (additive - reference) * weight
    static void BlendAdditive(const AnimPose& base, const AnimPose& additive,
                               const AnimPose& reference, f32 weight, AnimPose& out);
};

// ── 交叉淡入淡出 ────────────────────────────────────────────

class Crossfade {
public:
    /// 开始从当前动画过渡到目标动画
    void Start(const std::string& fromClip, const std::string& toClip,
               f32 transitionDuration);

    /// 更新过渡进度
    void Update(f32 dt);

    /// 是否正在过渡
    bool IsActive() const { return m_Active; }

    /// 获取混合权重 (0=from, 1=to)
    f32 GetWeight() const { return m_Weight; }

    /// 获取来源/目标动画名
    const std::string& GetFromClip() const { return m_FromClip; }
    const std::string& GetToClip() const { return m_ToClip; }

private:
    bool m_Active = false;
    f32 m_Duration = 0.0f;
    f32 m_Elapsed  = 0.0f;
    f32 m_Weight   = 0.0f;
    std::string m_FromClip;
    std::string m_ToClip;
};

// ── 采样到 Pose ─────────────────────────────────────────────

class PoseSampler {
public:
    /// 从 AnimationClip 采样指定时间的姿势
    static void SampleClip(const AnimationClip& clip, f32 time,
                           const Skeleton& skeleton, AnimPose& outPose);
};

} // namespace Engine
