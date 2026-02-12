#include "engine/core/time.h"

#include <GLFW/glfw3.h>

namespace Engine {

f32 Time::s_DeltaTime = 0.0f;
f32 Time::s_Elapsed = 0.0f;
f32 Time::s_LastTime = 0.0f;
f32 Time::s_FPS = 0.0f;
f32 Time::s_FPSAccumulator = 0.0f;
u32 Time::s_FPSCounter = 0;
u64 Time::s_FrameCount = 0;

void Time::Update() {
    f32 currentTime = (f32)glfwGetTime();
    s_DeltaTime = currentTime - s_LastTime;
    s_LastTime = currentTime;
    s_Elapsed = currentTime;
    s_FrameCount++;

    // FPS 统计（每秒更新一次）
    s_FPSCounter++;
    s_FPSAccumulator += s_DeltaTime;
    if (s_FPSAccumulator >= 1.0f) {
        s_FPS = (f32)s_FPSCounter / s_FPSAccumulator;
        s_FPSCounter = 0;
        s_FPSAccumulator = 0.0f;
    }
}

} // namespace Engine
