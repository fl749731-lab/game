#include "engine/debug/profiler.h"
#include "engine/core/log.h"

#include <algorithm>
#include <numeric>

namespace Engine {

std::unordered_map<std::string, Profiler::ActiveTimer> Profiler::s_ActiveTimers;
Profiler::FrameStats Profiler::s_CurrentFrame;
Profiler::FrameStats Profiler::s_LastFrame;
std::unordered_map<std::string, std::vector<f64>> Profiler::s_History;
static std::unordered_map<std::string, u32> s_HistoryIndex;  // 循环缓冲区写入位置
bool Profiler::s_Enabled = true;

void Profiler::BeginTimer(const std::string& name) {
    if (!s_Enabled) return;
    s_ActiveTimers[name] = { Clock::now() };
}

void Profiler::EndTimer(const std::string& name) {
    if (!s_Enabled) return;
    auto it = s_ActiveTimers.find(name);
    if (it == s_ActiveTimers.end()) return;

    auto end = Clock::now();
    f64 ms = std::chrono::duration<f64, std::milli>(end - it->second.Start).count();
    s_CurrentFrame.Timers.push_back({name, ms});
    s_ActiveTimers.erase(it);
}

void Profiler::EndFrame() {
    if (!s_Enabled) return;

    // 计算帧总时间
    f64 total = 0;
    for (auto& t : s_CurrentFrame.Timers) {
        total += t.DurationMs;
        // 存入历史（循环缓冲区，避免 O(N) 头部删除）
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
        // 缓冲区未满：尾部即最新数据
        for (u32 i = (u32)hist.size() - count; i < (u32)hist.size(); i++)
            sum += hist[i];
    } else {
        // 缓冲区已满：从写入位置倒序读取最新 count 个
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
        LOG_DEBUG("  %-24s %.3f ms (平均: %.3f ms)", t.Name.c_str(), t.DurationMs, avg);
    }
}

void Profiler::SetEnabled(bool enabled) { s_Enabled = enabled; }
bool Profiler::IsEnabled() { return s_Enabled; }

} // namespace Engine
