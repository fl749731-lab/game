#include "engine/core/time.h"

#include <GLFW/glfw3.h>
#include <thread>
#include <chrono>

namespace Engine {

f32 Time::s_DeltaTime = 0.0f;
f32 Time::s_Elapsed = 0.0f;
f32 Time::s_LastTime = 0.0f;
f32 Time::s_FPS = 0.0f;
f32 Time::s_FPSAccumulator = 0.0f;
u32 Time::s_FPSCounter = 0;
u64 Time::s_FrameCount = 0;
f32 Time::s_FixedDeltaTime = 1.0f / 60.0f;
f32 Time::s_FixedAccumulator = 0.0f;
u32 Time::s_TargetFPS = 0;

void Time::Update() {
    f32 currentTime = (f32)glfwGetTime();
    s_DeltaTime = currentTime - s_LastTime;

    // 帧率限制
    if (s_TargetFPS > 0) {
        f32 targetDt = 1.0f / (f32)s_TargetFPS;
        while (s_DeltaTime < targetDt) {
            f32 sleepMs = (targetDt - s_DeltaTime) * 1000.0f;
            if (sleepMs > 1.0f) {
                std::this_thread::sleep_for(std::chrono::microseconds((int)(sleepMs * 800)));
            }
            currentTime = (f32)glfwGetTime();
            s_DeltaTime = currentTime - s_LastTime;
        }
    }

    // 防止过大的 delta（例如断点后恢复）
    if (s_DeltaTime > 0.25f) s_DeltaTime = 0.25f;

    s_LastTime = currentTime;
    s_Elapsed = currentTime;
    s_FrameCount++;

    // 固定步长累加
    s_FixedAccumulator += s_DeltaTime;

    // FPS 统计（每秒更新一次）
    s_FPSCounter++;
    s_FPSAccumulator += s_DeltaTime;
    if (s_FPSAccumulator >= 1.0f) {
        s_FPS = (f32)s_FPSCounter / s_FPSAccumulator;
        s_FPSCounter = 0;
        s_FPSAccumulator = 0.0f;
    }
}

bool Time::ConsumeFixedStep() {
    if (s_FixedAccumulator >= s_FixedDeltaTime) {
        s_FixedAccumulator -= s_FixedDeltaTime;
        return true;
    }
    return false;
}

} // namespace Engine
