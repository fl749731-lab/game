#include "engine/renderer/animation_ik.h"
#include "engine/renderer/animation.h"
#include "engine/core/log.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <cmath>

namespace Engine {

// ═══════════════════════════════════════════════════════════
//  TwoBoneIK
// ═══════════════════════════════════════════════════════════

TwoBoneIK::Result TwoBoneIK::Solve(const glm::vec3& rootPos,
                                    const glm::vec3& middlePos,
                                    const glm::vec3& endPos,
                                    const glm::vec3& targetPos,
                                    const glm::vec3& poleVector) {
    Result result;
    result.RootPos = rootPos;

    f32 upperLen = glm::length(middlePos - rootPos);
    f32 lowerLen = glm::length(endPos - middlePos);
    f32 totalLen = upperLen + lowerLen;

    glm::vec3 toTarget = targetPos - rootPos;
    f32 targetDist = glm::length(toTarget);

    // 目标不可达
    if (targetDist >= totalLen - 0.001f) {
        glm::vec3 dir = glm::normalize(toTarget);
        result.MiddlePos = rootPos + dir * upperLen;
        result.EndPos    = rootPos + dir * totalLen;
        result.Solved = true;
        return result;
    }

    // 目标太近
    if (targetDist < std::abs(upperLen - lowerLen) + 0.001f) {
        result.MiddlePos = middlePos;
        result.EndPos    = endPos;
        result.Solved = false;
        return result;
    }

    // 余弦定理求中间关节角度
    // cos(A) = (b² + c² - a²) / (2bc)
    // A = root-middle 和 target 方向的夹角
    f32 cosAngle = (upperLen * upperLen + targetDist * targetDist - lowerLen * lowerLen)
                    / (2.0f * upperLen * targetDist);
    cosAngle = glm::clamp(cosAngle, -1.0f, 1.0f);
    f32 angle = std::acos(cosAngle);

    // 构建基坐标系
    glm::vec3 targetDir = glm::normalize(toTarget);

    // 极向量投影（去掉沿 target 方向的分量）
    glm::vec3 poleDir = poleVector - rootPos;
    poleDir = poleDir - glm::dot(poleDir, targetDir) * targetDir;
    if (glm::length(poleDir) < 0.001f) {
        // 极向量退化，使用默认
        glm::vec3 up = glm::abs(targetDir.y) < 0.999f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        poleDir = glm::cross(targetDir, up);
    }
    poleDir = glm::normalize(poleDir);

    // 中间关节位置 = root + 旋转后的上臂方向 * upperLen
    glm::vec3 upVec = glm::cross(targetDir, poleDir);

    result.MiddlePos = rootPos
        + targetDir * (upperLen * cosAngle)
        + poleDir * (upperLen * std::sin(angle));

    // 末端 = target
    result.EndPos = targetPos;
    result.Solved = true;
    return result;
}

void TwoBoneIK::Apply(Skeleton& skeleton,
                       const IKConstraint& constraint,
                       const std::vector<glm::mat4>& worldTransforms) {
    if (constraint.RootBone < 0 || constraint.MiddleBone < 0 || constraint.EndEffectorBone < 0)
        return;
    if (constraint.Weight < 0.001f) return;

    glm::vec3 rootPos   = glm::vec3(worldTransforms[constraint.RootBone][3]);
    glm::vec3 middlePos = glm::vec3(worldTransforms[constraint.MiddleBone][3]);
    glm::vec3 endPos    = glm::vec3(worldTransforms[constraint.EndEffectorBone][3]);

    // 混合目标位置
    glm::vec3 target = glm::mix(endPos, constraint.TargetPos, constraint.Weight);

    Result result = Solve(rootPos, middlePos, endPos, target, constraint.PoleVector);

    if (!result.Solved) return;

    // 更新中间骨骼的局部变换 (简化：直接设置位置偏移)
    // 完整实现应逆推局部旋转
    auto& middleBone = skeleton.GetBone(constraint.MiddleBone);
    glm::vec3 localOffset = result.MiddlePos - middlePos;
    middleBone.LocalTransform[3] += glm::vec4(localOffset * constraint.Weight, 0.0f);
}

// ═══════════════════════════════════════════════════════════
//  FABRIK
// ═══════════════════════════════════════════════════════════

void FABRIK::Solve(const FABRIKChain& chain, std::vector<glm::vec3>& positions) {
    u32 n = (u32)positions.size();
    if (n < 2) return;

    // 预计算骨骼长度
    std::vector<f32> lengths(n - 1);
    f32 totalLen = 0.0f;
    for (u32 i = 0; i < n - 1; i++) {
        lengths[i] = glm::length(positions[i + 1] - positions[i]);
        totalLen += lengths[i];
    }

    glm::vec3 rootPos = positions[0];
    f32 targetDist = glm::length(chain.TargetPos - rootPos);

    // 不可达
    if (targetDist >= totalLen) {
        glm::vec3 dir = glm::normalize(chain.TargetPos - rootPos);
        for (u32 i = 1; i < n; i++) {
            positions[i] = positions[i - 1] + dir * lengths[i - 1];
        }
        return;
    }

    // 迭代求解
    for (u32 iter = 0; iter < chain.MaxIterations; iter++) {
        // 检查收敛
        f32 endDist = glm::length(positions[n - 1] - chain.TargetPos);
        if (endDist < chain.Tolerance) break;

        // Forward pass: 从末端到根
        positions[n - 1] = chain.TargetPos;
        for (i32 i = (i32)n - 2; i >= 0; i--) {
            glm::vec3 dir = glm::normalize(positions[i] - positions[i + 1]);
            positions[i] = positions[i + 1] + dir * lengths[i];
        }

        // Backward pass: 从根到末端
        positions[0] = rootPos;
        for (u32 i = 0; i < n - 1; i++) {
            glm::vec3 dir = glm::normalize(positions[i + 1] - positions[i]);
            positions[i + 1] = positions[i] + dir * lengths[i];
        }
    }
}

} // namespace Engine
