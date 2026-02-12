#pragma once

#include "engine/core/types.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>

namespace Engine {

// ── 性能分析器 ──────────────────────────────────────────────
// 作用域计时 + 帧级汇总 + 直方图

class Profiler {
public:
    struct TimerResult {
        std::string Name;
        f64 DurationMs = 0;   // 毫秒
    };

    struct FrameStats {
        f64 TotalFrameMs = 0;
        std::vector<TimerResult> Timers;
    };

    /// 开始计时（返回计时器 ID）
    static void BeginTimer(const std::string& name);

    /// 结束计时
    static void EndTimer(const std::string& name);

    /// 每帧结束调用 — 汇总并存储帧数据
    static void EndFrame();

    /// 获取上一帧统计
    static const FrameStats& GetLastFrameStats();

    /// 获取最近 N 帧的平均时间
    static f64 GetAverageMs(const std::string& name, u32 frames = 60);

    /// 打印帧报告到日志
    static void PrintReport();

    /// 启用/禁用
    static void SetEnabled(bool enabled);
    static bool IsEnabled();

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    struct ActiveTimer {
        TimePoint Start;
    };

    static std::unordered_map<std::string, ActiveTimer> s_ActiveTimers;
    static FrameStats s_CurrentFrame;
    static FrameStats s_LastFrame;
    static std::unordered_map<std::string, std::vector<f64>> s_History;
    static bool s_Enabled;
    static constexpr u32 HISTORY_SIZE = 120;
};

// ── 作用域计时器宏 ──────────────────────────────────────────

class ScopedTimer {
public:
    ScopedTimer(const std::string& name) : m_Name(name) {
        Profiler::BeginTimer(m_Name);
    }
    ~ScopedTimer() { Profiler::EndTimer(m_Name); }
private:
    std::string m_Name;
};

#ifdef ENGINE_DEBUG
    #define PROFILE_SCOPE(name) ::Engine::ScopedTimer _timer_##__LINE__(name)
    #define PROFILE_FUNCTION()  PROFILE_SCOPE(__FUNCTION__)
#else
    #define PROFILE_SCOPE(name)
    #define PROFILE_FUNCTION()
#endif

} // namespace Engine
