#pragma once

#include "engine/core/types.h"

#include <imgui.h>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <functional>

namespace Engine {

// ── Stat 类别 ───────────────────────────────────────────────
// 类似 UE: stat fps, stat unit, stat gpu, stat memory, ...

enum class StatCategory : u8 {
    None       = 0,
    FPS        = 1 << 0,   // 帧率 + 帧时间折线
    Unit       = 1 << 1,   // Game/Render/GPU/Total 分时条
    GPU        = 1 << 2,   // 各 Pass GPU 耗时
    Memory     = 1 << 3,   // VRAM/RAM 占用明细
    Rendering  = 1 << 4,   // DrawCalls/三角形/批次
    Physics    = 1 << 5,   // 碰撞对/BVH/宽相
    Audio      = 1 << 6,   // 音频通道/事件
    SceneInfo  = 1 << 7,   // 实体/灯光/粒子
};

inline StatCategory operator|(StatCategory a, StatCategory b) {
    return (StatCategory)((u8)a | (u8)b);
}
inline StatCategory operator&(StatCategory a, StatCategory b) {
    return (StatCategory)((u8)a & (u8)b);
}

// ── 环形缓冲区 ──────────────────────────────────────────────

template<typename T, u32 N>
class RingBuffer {
public:
    void Push(T value) {
        m_Data[m_WritePos] = value;
        m_WritePos = (m_WritePos + 1) % N;
        if (m_Count < N) m_Count++;
    }

    T Get(u32 index) const {
        if (index >= m_Count) return T{};
        u32 startIdx = (m_Count < N) ? 0 : m_WritePos;
        return m_Data[(startIdx + index) % N];
    }

    u32 Count() const { return m_Count; }
    static constexpr u32 Capacity() { return N; }

    T Latest() const {
        if (m_Count == 0) return T{};
        return m_Data[(m_WritePos + N - 1) % N];
    }

    T Average() const {
        if (m_Count == 0) return T{};
        T sum = T{};
        for (u32 i = 0; i < m_Count; i++) sum += Get(i);
        return sum / (T)m_Count;
    }

    T Max() const {
        if (m_Count == 0) return T{};
        T mx = Get(0);
        for (u32 i = 1; i < m_Count; i++) {
            T v = Get(i);
            if (v > mx) mx = v;
        }
        return mx;
    }

    T Min() const {
        if (m_Count == 0) return T{};
        T mn = Get(0);
        for (u32 i = 1; i < m_Count; i++) {
            T v = Get(i);
            if (v < mn) mn = v;
        }
        return mn;
    }

    // 用于绘图: 填充到 float 数组
    void CopyToArray(float* out, u32 maxCount) const {
        u32 count = (maxCount < m_Count) ? maxCount : m_Count;
        for (u32 i = 0; i < count; i++) {
            out[i] = (float)Get(i);
        }
    }

private:
    std::array<T, N> m_Data{};
    u32 m_WritePos = 0;
    u32 m_Count = 0;
};

// ── Stat Overlay ────────────────────────────────────────────
// UE 风格屏幕统计覆盖层
// 用法:
//   StatOverlay::Toggle(StatCategory::FPS);
//   StatOverlay::Toggle(StatCategory::Unit);
//   每帧: StatOverlay::Update(dt) → StatOverlay::Render()

class StatOverlay {
public:
    static void Init();
    static void Shutdown();

    /// 切换某类别的显示
    static void Toggle(StatCategory cat);
    static void Enable(StatCategory cat);
    static void Disable(StatCategory cat);
    static bool IsEnabled(StatCategory cat);

    /// 从命令字符串切换: "fps", "unit", "gpu", "memory", ...
    static void ToggleByName(const std::string& name);

    /// 每帧更新数据 (传入帧增量和各子系统时间)
    static void Update(f32 deltaTime, f32 gameTimeMs, f32 renderTimeMs, f32 gpuTimeMs);

    /// 录入 GPU Pass 数据
    static void RecordGPUPass(const std::string& name, f32 timeMs);

    /// 录入内存数据
    static void RecordMemory(const std::string& label, size_t bytes, size_t totalCapacity);

    /// 录入渲染统计
    static void RecordRendering(u32 drawCalls, u32 triangles, u32 batches,
                                 u32 stateChanges, u32 culledObjects);

    /// 录入物理统计
    static void RecordPhysics(u32 collisionPairs, u32 bvhNodes, f32 broadPhaseMs);

    /// 录入场景信息
    static void RecordSceneInfo(u32 entities, u32 activeLights, u32 particleEmitters);

    /// ImGui 渲染 (每帧调用)
    static void Render();

private:
    static void RenderFPS();
    static void RenderUnit();
    static void RenderGPU();
    static void RenderMemory();
    static void RenderRendering();
    static void RenderPhysics();
    static void RenderSceneInfo();

    /// 绘制迷你折线图 (嵌入在统计文字旁)
    static void DrawMiniGraph(const char* label, const float* data, u32 count,
                               f32 minVal, f32 maxVal, ImVec2 size,
                               ImU32 color, bool showSpikes = true);

    // ── 数据存储 ──────────────────────────────────────
    static constexpr u32 HISTORY_SIZE = 240;

    inline static StatCategory s_ActiveCategories = StatCategory::None;

    // FPS
    inline static RingBuffer<f32, HISTORY_SIZE> s_FrameTimeHistory;
    inline static f32 s_FPS = 0;
    inline static f32 s_FrameTimeMs = 0;
    inline static f32 s_FrameTimeAvg = 0;

    // Unit (分时)
    inline static RingBuffer<f32, HISTORY_SIZE> s_GameTimeHistory;
    inline static RingBuffer<f32, HISTORY_SIZE> s_RenderTimeHistory;
    inline static RingBuffer<f32, HISTORY_SIZE> s_GPUTimeHistory;
    inline static f32 s_GameTimeMs = 0;
    inline static f32 s_RenderTimeMs = 0;
    inline static f32 s_GPUTimeMs = 0;

    // GPU Pass
    struct GPUPassInfo {
        std::string Name;
        f32 TimeMs = 0;
        RingBuffer<f32, 120> History;
    };
    inline static std::vector<GPUPassInfo> s_GPUPasses;

    // Memory
    struct MemoryInfo {
        std::string Label;
        size_t Bytes = 0;
        size_t Capacity = 0;
    };
    inline static std::vector<MemoryInfo> s_MemoryEntries;

    // Rendering
    inline static u32 s_DrawCalls = 0;
    inline static u32 s_Triangles = 0;
    inline static u32 s_Batches = 0;
    inline static u32 s_StateChanges = 0;
    inline static u32 s_CulledObjects = 0;

    // Physics
    inline static u32 s_CollisionPairs = 0;
    inline static u32 s_BVHNodes = 0;
    inline static f32 s_BroadPhaseMs = 0;

    // Scene
    inline static u32 s_Entities = 0;
    inline static u32 s_ActiveLights = 0;
    inline static u32 s_ParticleEmitters = 0;
};

} // namespace Engine
