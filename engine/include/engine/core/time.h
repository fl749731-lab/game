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

private:
    static f32 s_DeltaTime;
    static f32 s_Elapsed;
    static f32 s_LastTime;
    static f32 s_FPS;
    static f32 s_FPSAccumulator;
    static u32 s_FPSCounter;
    static u64 s_FrameCount;
};

} // namespace Engine
