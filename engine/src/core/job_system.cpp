#include "engine/core/job_system.h"
#include "engine/core/log.h"

#include <algorithm>

namespace Engine {

// ── 静态成员定义 ────────────────────────────────────────────

std::vector<std::thread>            JobSystem::s_Workers;
std::queue<std::function<void()>>   JobSystem::s_JobQueue;
std::mutex                          JobSystem::s_QueueMutex;
std::condition_variable             JobSystem::s_QueueCV;
std::condition_variable             JobSystem::s_IdleCV;
std::atomic<bool>                   JobSystem::s_Running{false};
std::atomic<u32>                    JobSystem::s_ActiveJobs{0};
u32                                 JobSystem::s_ThreadCount = 0;

// ── 初始化 ──────────────────────────────────────────────────

void JobSystem::Init(u32 numThreads) {
    if (s_Running) return;

    if (numThreads == 0) {
        u32 hw = std::thread::hardware_concurrency();
        numThreads = (hw > 1) ? (hw - 1) : 1;
    }

    s_ThreadCount = numThreads;
    s_Running = true;
    s_ActiveJobs = 0;

    s_Workers.reserve(numThreads);
    for (u32 i = 0; i < numThreads; i++) {
        s_Workers.emplace_back(WorkerThread, i);
    }

    LOG_INFO("[JobSystem] 初始化完成: %u 工作线程 (CPU: %u 核心)",
             numThreads, std::thread::hardware_concurrency());
}

// ── 关闭 ────────────────────────────────────────────────────

void JobSystem::Shutdown() {
    if (!s_Running) return;

    // 等待所有正在进行的工作完成
    WaitIdle();

    // 通知所有线程退出
    s_Running = false;
    s_QueueCV.notify_all();

    // 等待所有线程结束
    for (auto& w : s_Workers) {
        if (w.joinable()) w.join();
    }
    s_Workers.clear();
    s_ThreadCount = 0;

    LOG_INFO("[JobSystem] 已关闭");
}

// ── 提交任务 ────────────────────────────────────────────────

void JobSystem::Submit(std::function<void()> job) {
    {
        std::lock_guard<std::mutex> lock(s_QueueMutex);
        s_ActiveJobs++;
        s_JobQueue.push(std::move(job));
    }
    s_QueueCV.notify_one();
}

// ── 等待所有任务完成 ────────────────────────────────────────

void JobSystem::WaitIdle() {
    std::unique_lock<std::mutex> lock(s_QueueMutex);
    s_IdleCV.wait(lock, [] {
        return s_ActiveJobs == 0 && s_JobQueue.empty();
    });
}

// ── 工作线程入口 ────────────────────────────────────────────

void JobSystem::WorkerThread(u32 threadIndex) {
    (void)threadIndex;

    while (true) {
        std::function<void()> job;

        {
            std::unique_lock<std::mutex> lock(s_QueueMutex);
            s_QueueCV.wait(lock, [] {
                return !s_JobQueue.empty() || !s_Running;
            });

            if (!s_Running && s_JobQueue.empty()) return;

            job = std::move(s_JobQueue.front());
            s_JobQueue.pop();
        }

        // 执行任务
        job();

        // 更新活跃计数并通知
        if (--s_ActiveJobs == 0) {
            s_IdleCV.notify_all();
        }
    }
}

} // namespace Engine
