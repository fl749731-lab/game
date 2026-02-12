#pragma once

#include "engine/core/types.h"

struct GLFWwindow;

namespace Engine {

// ── 帧时间 ──────────────────────────────────────────────────

class Time {
public:
    /// 在主循环顶部调用
    static void Update();

    /// 帧间隔（秒）
    static f32 DeltaTime() { return s_DeltaTime; }

    /// 从启动起的总时间（秒）
    static f32 Elapsed() { return s_Elapsed; }

    /// 帧率
    static f32 FPS() { return s_FPS; }

    /// 帧数
    static u64 FrameCount() { return s_FrameCount; }

    /// 固定步长 (默认 1/60)
    static f32 FixedDeltaTime() { return s_FixedDeltaTime; }
    static void SetFixedDeltaTime(f32 dt) { s_FixedDeltaTime = dt; }

    /// 固定步长累加器（用于物理循环）
    static f32 FixedAccumulator() { return s_FixedAccumulator; }
    static bool ConsumeFixedStep();

    /// 帧率限制（0 = 不限制）
    static void SetTargetFPS(u32 fps) { s_TargetFPS = fps; }
    static u32 GetTargetFPS() { return s_TargetFPS; }

private:
    static f32 s_DeltaTime;
    static f32 s_Elapsed;
    static f32 s_LastTime;
    static f32 s_FPS;
    static f32 s_FPSAccumulator;
    static u32 s_FPSCounter;
    static u64 s_FrameCount;
    static f32 s_FixedDeltaTime;
    static f32 s_FixedAccumulator;
    static u32 s_TargetFPS;
};

} // namespace Engine
