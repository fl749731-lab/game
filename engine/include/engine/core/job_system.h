#pragma once

#include "engine/core/types.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>
#include <vector>

namespace Engine {

// ── Job System (线程池) ─────────────────────────────────────
//
// 基于 std::thread 的轻量级任务系统。
// 适用于将 CPU 密集型工作分发到工作线程。
//
// 用法:
//   JobSystem::Init();
//   JobSystem::ParallelFor(0, count, [](u32 i) { ... });
//   JobSystem::WaitIdle();
//   JobSystem::Shutdown();
//
// 线程安全: 所有公开方法均线程安全。

class JobSystem {
public:
    /// 初始化线程池 (默认 = CPU 核心数 - 1, 至少 1)
    static void Init(u32 numThreads = 0);

    /// 安全关闭所有工作线程
    static void Shutdown();

    /// 提交单个任务
    static void Submit(std::function<void()> job);

    /// 并行循环: 将 [begin, end) 按块分配到工作线程
    /// fn 签名: void(u32 index)
    template<typename Func>
    static void ParallelFor(u32 begin, u32 end, Func&& fn);

    /// 阻塞等待所有已提交任务完成
    static void WaitIdle();

    /// 查询线程数
    static u32 GetWorkerCount() { return s_ThreadCount; }

    /// 是否已初始化
    static bool IsActive() { return s_Running; }

private:
    static void WorkerThread(u32 threadIndex);

    static std::vector<std::thread>            s_Workers;
    static std::queue<std::function<void()>>   s_JobQueue;
    static std::mutex                          s_QueueMutex;
    static std::condition_variable             s_QueueCV;
    static std::condition_variable             s_IdleCV;
    static std::atomic<bool>                   s_Running;
    static std::atomic<u32>                    s_ActiveJobs;
    static u32                                 s_ThreadCount;
};

// ── ParallelFor 模板实现 ────────────────────────────────────

template<typename Func>
void JobSystem::ParallelFor(u32 begin, u32 end, Func&& fn) {
    if (begin >= end) return;

    u32 total = end - begin;

    // 太少的工作量不值得分发
    if (total <= 64 || !s_Running || s_ThreadCount == 0) {
        for (u32 i = begin; i < end; i++) fn(i);
        return;
    }

    u32 numChunks = std::min(s_ThreadCount, total);
    u32 chunkSize = total / numChunks;
    u32 remainder = total % numChunks;

    for (u32 c = 0; c < numChunks; c++) {
        u32 chunkBegin = begin + c * chunkSize + std::min(c, remainder);
        u32 chunkEnd   = chunkBegin + chunkSize + (c < remainder ? 1 : 0);

        Submit([chunkBegin, chunkEnd, fn]() {
            for (u32 i = chunkBegin; i < chunkEnd; i++) {
                fn(i);
            }
        });
    }

    WaitIdle();
}

} // namespace Engine
