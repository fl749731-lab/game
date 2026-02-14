#pragma once

#include "engine/core/types.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>

namespace Engine {

class Skeleton;

// ── IK 约束 ─────────────────────────────────────────────────

struct IKConstraint {
    i32 EndEffectorBone  = -1;     // 末端骨骼（如手、脚）
    i32 MiddleBone       = -1;     // 中间关节（如肘、膝）
    i32 RootBone         = -1;     // 根骨骼（如肩、髋）
    glm::vec3 TargetPos  = {0, 0, 0};  // 目标位置
    glm::vec3 PoleVector = {0, 0, 1};  // 极向量（控制关节弯曲方向）
    f32 Weight           = 1.0f;       // IK 权重 (0=FK, 1=完全IK)
};

// ── 两骨 IK 求解器 ──────────────────────────────────────────

class TwoBoneIK {
public:
    /// 求解两骨 IK
    /// 输入: 三个骨骼的世界位置、目标位置、极向量
    /// 输出: 中间关节和末端关节的新位置
    struct Result {
        glm::vec3 RootPos;
        glm::vec3 MiddlePos;
        glm::vec3 EndPos;
        bool Solved = false;
    };

    static Result Solve(const glm::vec3& rootPos,
                        const glm::vec3& middlePos,
                        const glm::vec3& endPos,
                        const glm::vec3& targetPos,
                        const glm::vec3& poleVector);

    /// 应用 IK 结果到骨架的局部变换
    /// 需要骨骼的世界变换来逆推局部变换
    static void Apply(Skeleton& skeleton,
                      const IKConstraint& constraint,
                      const std::vector<glm::mat4>& worldTransforms);
};

// ── FABRIK 求解器（多骨 IK）──────────────────────────────────

struct FABRIKChain {
    std::vector<i32> BoneIndices;        // 从根到末端的骨骼索引
    glm::vec3 TargetPos = {0, 0, 0};
    f32 Tolerance = 0.001f;
    u32 MaxIterations = 10;
    f32 Weight = 1.0f;
};

class FABRIK {
public:
    /// 求解 FABRIK
    static void Solve(const FABRIKChain& chain,
                      std::vector<glm::vec3>& positions);
};

} // namespace Engine
