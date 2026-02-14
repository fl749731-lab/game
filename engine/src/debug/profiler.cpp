#include "engine/debug/profiler.h"
#include "engine/core/log.h"

#include <algorithm>
#include <numeric>

namespace Engine {

std::vector<Profiler::ActiveTimer> Profiler::s_TimerStack;
Profiler::FrameStats Profiler::s_CurrentFrame;
Profiler::FrameStats Profiler::s_LastFrame;
std::unordered_map<std::string, std::vector<f64>> Profiler::s_History;
static std::unordered_map<std::string, u32> s_HistoryIndex;
bool Profiler::s_Enabled = true;

void Profiler::BeginTimer(const std::string& name) {
    if (!s_Enabled) return;
    ActiveTimer timer;
    timer.Start = Clock::now();
    timer.Name = name;
    timer.Depth = (u32)s_TimerStack.size();
    s_TimerStack.push_back(timer);
}

void Profiler::EndTimer(const std::string& name) {
    if (!s_Enabled) return;

    // 从栈顶向下查找匹配的计时器
    for (int i = (int)s_TimerStack.size() - 1; i >= 0; i--) {
        if (s_TimerStack[i].Name == name) {
            auto end = Clock::now();
            f64 ms = std::chrono::duration<f64, std::milli>(end - s_TimerStack[i].Start).count();

            TimerResult result;
            result.Name = name;
            result.DurationMs = ms;
            result.Depth = s_TimerStack[i].Depth;
            s_CurrentFrame.Timers.push_back(result);

            s_TimerStack.erase(s_TimerStack.begin() + i);
            break;
        }
    }
}

void Profiler::EndFrame() {
    if (!s_Enabled) return;

    // 帧总时间 = 顶层计时器之和
    f64 total = 0;
    for (auto& t : s_CurrentFrame.Timers) {
        if (t.Depth == 0) total += t.DurationMs;

        // 存入历史（环形缓冲区）
        auto& hist = s_History[t.Name];
        auto& idx = s_HistoryIndex[t.Name];
        if (hist.size() < HISTORY_SIZE) {
            hist.push_back(t.DurationMs);
        } else {
            hist[idx % HISTORY_SIZE] = t.DurationMs;
        }
        idx++;
    }
    s_CurrentFrame.TotalFrameMs = total;

    s_LastFrame = s_CurrentFrame;
    s_CurrentFrame.Timers.clear();
    s_TimerStack.clear();  // 防止跨帧泄漏
}

const Profiler::FrameStats& Profiler::GetLastFrameStats() {
    return s_LastFrame;
}

f64 Profiler::GetAverageMs(const std::string& name, u32 frames) {
    auto it = s_History.find(name);
    if (it == s_History.end() || it->second.empty()) return 0;

    auto& hist = it->second;
    u32 count = std::min((u32)hist.size(), frames);
    f64 sum = 0;

    if (hist.size() < HISTORY_SIZE) {
        for (u32 i = (u32)hist.size() - count; i < (u32)hist.size(); i++)
            sum += hist[i];
    } else {
        u32 writePos = s_HistoryIndex[name];
        for (u32 i = 0; i < count; i++) {
            u32 idx = (writePos - 1 - i + HISTORY_SIZE) % HISTORY_SIZE;
            sum += hist[idx];
        }
    }
    return sum / count;
}

void Profiler::PrintReport() {
    if (s_LastFrame.Timers.empty()) return;
    LOG_DEBUG("=== Profiler 帧报告 (%.2f ms 总计) ===", s_LastFrame.TotalFrameMs);
    for (auto& t : s_LastFrame.Timers) {
        f64 avg = GetAverageMs(t.Name, 60);
        // 缩进显示层级
        std::string indent(t.Depth * 2, ' ');
        LOG_DEBUG("  %s%-20s %.3f ms (avg: %.3f ms)", indent.c_str(), t.Name.c_str(), t.DurationMs, avg);
    }
}

void Profiler::SetEnabled(bool enabled) { s_Enabled = enabled; }
bool Profiler::IsEnabled() { return s_Enabled; }

} // namespace Engine
