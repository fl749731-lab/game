#include "engine/renderer/animation.h"
#include "engine/core/log.h"

#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Engine {

// ── Skeleton ────────────────────────────────────────────────

i32 Skeleton::AddBone(const Bone& bone) {
    i32 idx = (i32)m_Bones.size();
    m_BoneNameMap[bone.Name] = idx;
    m_Bones.push_back(bone);
    return idx;
}

i32 Skeleton::FindBone(const std::string& name) const {
    auto it = m_BoneNameMap.find(name);
    if (it != m_BoneNameMap.end()) return it->second;
    return -1;
}

void Skeleton::ComputeBoneMatrices(std::vector<glm::mat4>& outMatrices) const {
    u32 count = GetBoneCount();
    outMatrices.resize(count);

    // 临时存储全局变换
    std::vector<glm::mat4> globalTransforms(count);

    for (u32 i = 0; i < count; i++) {
        const Bone& bone = m_Bones[i];
        if (bone.ParentIndex < 0) {
            // 根骨骼
            globalTransforms[i] = bone.LocalTransform;
        } else {
            // 子骨骼 = 父全局 * 自身局部
            globalTransforms[i] = globalTransforms[bone.ParentIndex] * bone.LocalTransform;
        }
        // Final = GlobalTransform * InverseBindMatrix
        outMatrices[i] = globalTransforms[i] * bone.InverseBindMatrix;
    }
}

// ── AnimationSampler ────────────────────────────────────────

glm::vec3 AnimationSampler::InterpolatePosition(
    const std::vector<PositionKey>& keys, f32 time) {
    if (keys.empty()) return glm::vec3(0.0f);
    if (keys.size() == 1) return keys[0].Value;

    // 找到当前时间所在的两个关键帧
    for (size_t i = 0; i < keys.size() - 1; i++) {
        if (time < keys[i + 1].Time) {
            f32 dt = keys[i + 1].Time - keys[i].Time;
            f32 factor = (dt > 0.0f) ? (time - keys[i].Time) / dt : 0.0f;
            return glm::mix(keys[i].Value, keys[i + 1].Value, factor);
        }
    }
    return keys.back().Value;
}

glm::quat AnimationSampler::InterpolateRotation(
    const std::vector<RotationKey>& keys, f32 time) {
    if (keys.empty()) return glm::quat(1, 0, 0, 0);
    if (keys.size() == 1) return keys[0].Value;

    for (size_t i = 0; i < keys.size() - 1; i++) {
        if (time < keys[i + 1].Time) {
            f32 dt = keys[i + 1].Time - keys[i].Time;
            f32 factor = (dt > 0.0f) ? (time - keys[i].Time) / dt : 0.0f;
            return glm::slerp(keys[i].Value, keys[i + 1].Value, factor);
        }
    }
    return keys.back().Value;
}

glm::vec3 AnimationSampler::InterpolateScale(
    const std::vector<ScaleKey>& keys, f32 time) {
    if (keys.empty()) return glm::vec3(1.0f);
    if (keys.size() == 1) return keys[0].Value;

    for (size_t i = 0; i < keys.size() - 1; i++) {
        if (time < keys[i + 1].Time) {
            f32 dt = keys[i + 1].Time - keys[i].Time;
            f32 factor = (dt > 0.0f) ? (time - keys[i].Time) / dt : 0.0f;
            return glm::mix(keys[i].Value, keys[i + 1].Value, factor);
        }
    }
    return keys.back().Value;
}

void AnimationSampler::Sample(const AnimationClip& clip, f32 time,
                              Skeleton& skeleton) {
    for (const auto& channel : clip.Channels) {
        if (channel.BoneIndex < 0 ||
            channel.BoneIndex >= (i32)skeleton.GetBoneCount()) continue;

        glm::vec3 pos   = InterpolatePosition(channel.PositionKeys, time);
        glm::quat rot   = InterpolateRotation(channel.RotationKeys, time);
        glm::vec3 scale = InterpolateScale(channel.ScaleKeys, time);

        // 构建局部变换 TRS
        glm::mat4 T = glm::translate(glm::mat4(1.0f), pos);
        glm::mat4 R = glm::toMat4(rot);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);

        skeleton.GetBone(channel.BoneIndex).LocalTransform = T * R * S;
    }
}

// ── AnimationSystem ─────────────────────────────────────────

void AnimationSystem::Update(ECSWorld& world, f32 dt) {
    world.ForEach<AnimatorComponent>([&](Entity e, AnimatorComponent& anim) {
        if (!anim.Playing || !anim.SkeletonRef) return;

        // 查找当前动画
        AnimationClip* clip = nullptr;
        for (auto& c : anim.Clips) {
            if (c.Name == anim.CurrentClip) {
                clip = &c;
                break;
            }
        }
        if (!clip || clip->Duration <= 0.0f) return;

        // 时间推进
        anim.CurrentTime += dt * anim.PlaybackSpeed;
        if (anim.Loop) {
            anim.CurrentTime = fmodf(anim.CurrentTime, clip->Duration);
            if (anim.CurrentTime < 0) anim.CurrentTime += clip->Duration;
        } else {
            if (anim.CurrentTime >= clip->Duration) {
                anim.CurrentTime = clip->Duration;
                anim.Playing = false;
            }
        }

        // 采样关键帧 → 更新骨骼局部变换
        AnimationSampler::Sample(*clip, anim.CurrentTime, *anim.SkeletonRef);

        // 计算最终骨骼矩阵
        anim.SkeletonRef->ComputeBoneMatrices(anim.BoneMatrices);
    });
}

} // namespace Engine
