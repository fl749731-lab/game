#include "engine/renderer/animation_blend.h"
#include "engine/core/log.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

namespace Engine {

// ═══════════════════════════════════════════════════════════
//  PoseBlender
// ═══════════════════════════════════════════════════════════

void PoseBlender::Blend(const AnimPose& a, const AnimPose& b, f32 weight, AnimPose& out) {
    u32 count = (u32)std::min(a.BonePoses.size(), b.BonePoses.size());
    out.Resize(count);

    f32 w = glm::clamp(weight, 0.0f, 1.0f);
    for (u32 i = 0; i < count; i++) {
        out.BonePoses[i].Position = glm::mix(a.BonePoses[i].Position, b.BonePoses[i].Position, w);
        out.BonePoses[i].Rotation = glm::slerp(a.BonePoses[i].Rotation, b.BonePoses[i].Rotation, w);
        out.BonePoses[i].Scale    = glm::mix(a.BonePoses[i].Scale, b.BonePoses[i].Scale, w);
    }
}

void PoseBlender::BlendAdditive(const AnimPose& base, const AnimPose& additive,
                                 const AnimPose& reference, f32 weight, AnimPose& out) {
    u32 count = (u32)std::min({base.BonePoses.size(), additive.BonePoses.size(), reference.BonePoses.size()});
    out.Resize(count);

    for (u32 i = 0; i < count; i++) {
        glm::vec3 deltaPos = additive.BonePoses[i].Position - reference.BonePoses[i].Position;
        glm::quat deltaRot = additive.BonePoses[i].Rotation * glm::inverse(reference.BonePoses[i].Rotation);
        glm::vec3 deltaScl = additive.BonePoses[i].Scale / (reference.BonePoses[i].Scale + glm::vec3(0.001f));

        out.BonePoses[i].Position = base.BonePoses[i].Position + deltaPos * weight;
        out.BonePoses[i].Rotation = glm::slerp(glm::quat(1,0,0,0), deltaRot, weight) * base.BonePoses[i].Rotation;
        out.BonePoses[i].Scale    = base.BonePoses[i].Scale * glm::mix(glm::vec3(1.0f), deltaScl, weight);
    }
}

// ═══════════════════════════════════════════════════════════
//  Crossfade
// ═══════════════════════════════════════════════════════════

void Crossfade::Start(const std::string& fromClip, const std::string& toClip,
                       f32 transitionDuration) {
    m_FromClip = fromClip;
    m_ToClip   = toClip;
    m_Duration = glm::max(transitionDuration, 0.001f);
    m_Elapsed  = 0.0f;
    m_Weight   = 0.0f;
    m_Active   = true;
}

void Crossfade::Update(f32 dt) {
    if (!m_Active) return;
    m_Elapsed += dt;
    m_Weight = glm::clamp(m_Elapsed / m_Duration, 0.0f, 1.0f);
    if (m_Elapsed >= m_Duration) {
        m_Active = false;
        m_Weight = 1.0f;
    }
}

// ═══════════════════════════════════════════════════════════
//  PoseSampler
// ═══════════════════════════════════════════════════════════

static glm::vec3 SamplePosition(const std::vector<PositionKey>& keys, f32 time) {
    if (keys.empty()) return {0, 0, 0};
    if (keys.size() == 1 || time <= keys.front().Time) return keys.front().Value;
    if (time >= keys.back().Time) return keys.back().Value;

    for (size_t i = 0; i + 1 < keys.size(); i++) {
        if (time < keys[i + 1].Time) {
            f32 t = (time - keys[i].Time) / (keys[i + 1].Time - keys[i].Time);
            return glm::mix(keys[i].Value, keys[i + 1].Value, t);
        }
    }
    return keys.back().Value;
}

static glm::quat SampleRotation(const std::vector<RotationKey>& keys, f32 time) {
    if (keys.empty()) return glm::quat(1, 0, 0, 0);
    if (keys.size() == 1 || time <= keys.front().Time) return keys.front().Value;
    if (time >= keys.back().Time) return keys.back().Value;

    for (size_t i = 0; i + 1 < keys.size(); i++) {
        if (time < keys[i + 1].Time) {
            f32 t = (time - keys[i].Time) / (keys[i + 1].Time - keys[i].Time);
            return glm::slerp(keys[i].Value, keys[i + 1].Value, t);
        }
    }
    return keys.back().Value;
}

static glm::vec3 SampleScale(const std::vector<ScaleKey>& keys, f32 time) {
    if (keys.empty()) return {1, 1, 1};
    if (keys.size() == 1 || time <= keys.front().Time) return keys.front().Value;
    if (time >= keys.back().Time) return keys.back().Value;

    for (size_t i = 0; i + 1 < keys.size(); i++) {
        if (time < keys[i + 1].Time) {
            f32 t = (time - keys[i].Time) / (keys[i + 1].Time - keys[i].Time);
            return glm::mix(keys[i].Value, keys[i + 1].Value, t);
        }
    }
    return keys.back().Value;
}

void PoseSampler::SampleClip(const AnimationClip& clip, f32 time,
                              const Skeleton& skeleton, AnimPose& outPose) {
    u32 boneCount = skeleton.GetBoneCount();
    outPose.Resize(boneCount);

    // 默认值：绑定姿势
    for (u32 i = 0; i < boneCount; i++) {
        outPose.BonePoses[i].Position = {0, 0, 0};
        outPose.BonePoses[i].Rotation = glm::quat(1, 0, 0, 0);
        outPose.BonePoses[i].Scale    = {1, 1, 1};
    }

    // 从各通道采样
    for (auto& channel : clip.Channels) {
        if (channel.BoneIndex < 0 || channel.BoneIndex >= (i32)boneCount) continue;

        auto& pose = outPose.BonePoses[channel.BoneIndex];
        pose.Position = SamplePosition(channel.PositionKeys, time);
        pose.Rotation = SampleRotation(channel.RotationKeys, time);
        pose.Scale    = SampleScale(channel.ScaleKeys, time);
    }
}

} // namespace Engine
