#pragma once

#include "engine/core/types.h"
#include "engine/renderer/animation.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Engine {

// ── 根运动数据（每帧增量）────────────────────────────────────

struct RootMotionDelta {
    glm::vec3 DeltaPosition = {0, 0, 0};
    glm::quat DeltaRotation = glm::quat(1, 0, 0, 0);
};

// ── 根运动提取器 ────────────────────────────────────────────

class RootMotionExtractor {
public:
    /// 设置根骨骼索引
    void SetRootBoneIndex(i32 index) { m_RootBone = index; }

    /// 提取两个时间点之间的根运动增量
    RootMotionDelta Extract(const AnimationClip& clip,
                            f32 prevTime, f32 currTime) const;

    /// 是否启用根运动
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    /// 根运动模式
    enum class Mode : u8 {
        XZ,        // 仅水平移动（常用）
        XYZ,       // 完整 3D 移动
        RotationOnly, // 仅旋转
    };

    void SetMode(Mode mode) { m_Mode = mode; }

private:
    i32 m_RootBone = 0;
    bool m_Enabled = false;
    Mode m_Mode = Mode::XZ;
};

} // namespace Engine
