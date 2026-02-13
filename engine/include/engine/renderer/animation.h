#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <vector>
#include <unordered_map>

namespace Engine {

// ── 最大骨骼数 ──────────────────────────────────────────────
constexpr u32 MAX_BONES = 128;

// ── 骨骼 ────────────────────────────────────────────────────

struct Bone {
    std::string Name;
    i32 ParentIndex = -1;              // -1 = 根骨骼
    glm::mat4 InverseBindMatrix = glm::mat4(1.0f);  // 绑定姿势的逆矩阵
    glm::mat4 LocalTransform = glm::mat4(1.0f);     // 当前局部变换
};

// ── 关键帧 ──────────────────────────────────────────────────

struct PositionKey {
    f32 Time;
    glm::vec3 Value;
};

struct RotationKey {
    f32 Time;
    glm::quat Value;
};

struct ScaleKey {
    f32 Time;
    glm::vec3 Value;
};

// ── 动画通道 (一条骨骼的动画曲线) ────────────────────────────

struct AnimationChannel {
    i32 BoneIndex = -1;
    std::vector<PositionKey> PositionKeys;
    std::vector<RotationKey> RotationKeys;
    std::vector<ScaleKey>    ScaleKeys;
};

// ── 动画剪辑 ────────────────────────────────────────────────

struct AnimationClip {
    std::string Name;
    f32 Duration = 0.0f;       // 总时长 (秒)
    f32 TicksPerSecond = 25.0f;
    std::vector<AnimationChannel> Channels;
};

// ── 骨架 ────────────────────────────────────────────────────

class Skeleton {
public:
    /// 添加骨骼
    i32 AddBone(const Bone& bone);

    /// 通过名称查找骨骼索引 (-1 = 未找到)
    i32 FindBone(const std::string& name) const;

    /// 获取骨骼数量
    u32 GetBoneCount() const { return (u32)m_Bones.size(); }

    /// 获取骨骼 (只读)
    const Bone& GetBone(i32 index) const { return m_Bones[index]; }
    Bone& GetBone(i32 index) { return m_Bones[index]; }

    /// 获取全部骨骼
    const std::vector<Bone>& GetBones() const { return m_Bones; }

    /// 计算最终骨骼矩阵 (用于传递给 shader)
    /// 输出: FinalMatrices[i] = GlobalTransform[i] * InverseBindMatrix[i]
    void ComputeBoneMatrices(std::vector<glm::mat4>& outMatrices) const;

private:
    std::vector<Bone> m_Bones;
    std::unordered_map<std::string, i32> m_BoneNameMap;  // 名称→索引
};

// ── 动画采样器 ──────────────────────────────────────────────

class AnimationSampler {
public:
    /// 采样指定时间点的变换 (插值关键帧)
    static void Sample(const AnimationClip& clip, f32 time,
                       Skeleton& skeleton);

private:
    /// 线性插值位置
    static glm::vec3 InterpolatePosition(const std::vector<PositionKey>& keys, f32 time);
    /// 球面线性插值旋转
    static glm::quat InterpolateRotation(const std::vector<RotationKey>& keys, f32 time);
    /// 线性插值缩放
    static glm::vec3 InterpolateScale(const std::vector<ScaleKey>& keys, f32 time);
};

// ── 动画组件 ────────────────────────────────────────────────

struct AnimatorComponent : public Component {
    Ref<Skeleton> SkeletonRef;
    std::vector<AnimationClip> Clips;            // 可用动画列表
    std::string CurrentClip;                      // 当前播放的动画名
    f32 CurrentTime = 0.0f;
    f32 PlaybackSpeed = 1.0f;
    bool Loop = true;
    bool Playing = true;

    /// 最终骨骼矩阵 (由 AnimationSystem 每帧更新)
    std::vector<glm::mat4> BoneMatrices;
};

// ── 动画系统 ────────────────────────────────────────────────

class AnimationSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override;
    const char* GetName() const override { return "AnimationSystem"; }
};

} // namespace Engine
