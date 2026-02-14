#include "engine/renderer/animation_root_motion.h"
#include "engine/core/log.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Engine {

RootMotionDelta RootMotionExtractor::Extract(const AnimationClip& clip,
                                              f32 prevTime, f32 currTime) const {
    RootMotionDelta delta;
    if (!m_Enabled || m_RootBone < 0) return delta;

    // 找到根骨骼通道
    const AnimationChannel* rootChannel = nullptr;
    for (auto& ch : clip.Channels) {
        if (ch.BoneIndex == m_RootBone) {
            rootChannel = &ch;
            break;
        }
    }
    if (!rootChannel) return delta;

    // 采样两个时间点的位置
    glm::vec3 prevPos = {0, 0, 0};
    glm::vec3 currPos = {0, 0, 0};
    glm::quat prevRot = glm::quat(1, 0, 0, 0);
    glm::quat currRot = glm::quat(1, 0, 0, 0);

    // 位置
    if (!rootChannel->PositionKeys.empty()) {
        auto& keys = rootChannel->PositionKeys;

        auto samplePos = [&](f32 t) -> glm::vec3 {
            if (t <= keys.front().Time) return keys.front().Value;
            if (t >= keys.back().Time) return keys.back().Value;
            for (size_t i = 0; i + 1 < keys.size(); i++) {
                if (t < keys[i + 1].Time) {
                    f32 f = (t - keys[i].Time) / (keys[i + 1].Time - keys[i].Time);
                    return glm::mix(keys[i].Value, keys[i + 1].Value, f);
                }
            }
            return keys.back().Value;
        };

        prevPos = samplePos(prevTime);
        currPos = samplePos(currTime);
    }

    // 旋转
    if (!rootChannel->RotationKeys.empty()) {
        auto& keys = rootChannel->RotationKeys;

        auto sampleRot = [&](f32 t) -> glm::quat {
            if (t <= keys.front().Time) return keys.front().Value;
            if (t >= keys.back().Time) return keys.back().Value;
            for (size_t i = 0; i + 1 < keys.size(); i++) {
                if (t < keys[i + 1].Time) {
                    f32 f = (t - keys[i].Time) / (keys[i + 1].Time - keys[i].Time);
                    return glm::slerp(keys[i].Value, keys[i + 1].Value, f);
                }
            }
            return keys.back().Value;
        };

        prevRot = sampleRot(prevTime);
        currRot = sampleRot(currTime);
    }

    // 计算增量
    glm::vec3 rawDelta = currPos - prevPos;

    switch (m_Mode) {
        case Mode::XZ:
            delta.DeltaPosition = {rawDelta.x, 0.0f, rawDelta.z};
            break;
        case Mode::XYZ:
            delta.DeltaPosition = rawDelta;
            break;
        case Mode::RotationOnly:
            delta.DeltaPosition = {0, 0, 0};
            break;
    }

    delta.DeltaRotation = currRot * glm::inverse(prevRot);
    return delta;
}

} // namespace Engine
