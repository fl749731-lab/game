#pragma once

#include "engine/core/types.h"

#include <string>
#include <vector>
#include <stack>
#include <unordered_map>

namespace Engine {

// ── GPU 计时查询 (OpenGL Timer Query) ───────────────────────
// 双缓冲 Query Pool — 避免 pipeline stall
// 自动包装各 Pass: Shadow / GBuffer / Lighting / Forward / Post

class GPUProfiler {
public:
    static void Init(u32 maxQueries = 64);
    static void Shutdown();

    /// 帧开始/结束
    static void BeginFrame();
    static void EndFrame();

    /// 包围一个 GPU Pass
    static void BeginPass(const std::string& name);
    static void EndPass();

    /// 上一帧查询结果
    struct PassResult {
        std::string Name;
        f32 TimeMs = 0;
        u32 Depth = 0;   // 嵌套深度
    };

    static const std::vector<PassResult>& GetLastFrameResults();
    static f32 GetLastFrameGPUTime();

    /// 是否启用
    static void SetEnabled(bool enabled);
    static bool IsEnabled();

private:
    struct QueryPair {
        u32 BeginQuery = 0;
        u32 EndQuery = 0;
        std::string Name;
        u32 Depth = 0;
    };

    static constexpr u32 BUFFER_COUNT = 2;  // 双缓冲

    inline static bool s_Enabled = true;
    inline static u32 s_CurrentBuffer = 0;  // 当前写入缓冲
    inline static u32 s_MaxQueries = 64;

    // 双缓冲 query IDs
    inline static std::vector<u32> s_QueryIDs[BUFFER_COUNT];
    inline static u32 s_NextQueryIdx[BUFFER_COUNT] = {};

    // 当前帧的 Pass 记录
    inline static std::vector<QueryPair> s_CurrentPasses[BUFFER_COUNT];
    inline static u32 s_CurrentDepth = 0;

    // 上一帧结果
    inline static std::vector<PassResult> s_LastResults;
    inline static f32 s_LastGPUTime = 0;
};

// ── GPU 计时作用域宏 ────────────────────────────────────────

class ScopedGPUTimer {
public:
    ScopedGPUTimer(const std::string& name) { GPUProfiler::BeginPass(name); }
    ~ScopedGPUTimer() { GPUProfiler::EndPass(); }
};

#ifdef ENGINE_DEBUG
    #define GPU_PROFILE_SCOPE(name) ::Engine::ScopedGPUTimer _gpu_timer_##__LINE__(name)
#else
    #define GPU_PROFILE_SCOPE(name)
#endif

} // namespace Engine
