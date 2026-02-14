#pragma once

#include "engine/core/types.h"
#include "engine/renderer/texture.h"

#include <string>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>

namespace Engine {

// ── 纹理 CPU 解码结果 ──────────────────────────────────────
// 工作线程产出的中间数据（不含任何 GL 调用）

struct TextureLoadResult {
    std::string Name;           // 资源名
    std::string FilePath;       // 原始路径（用于日志）
    unsigned char* PixelData;   // stbi 分配的像素数据
    i32 Width;
    i32 Height;
    i32 Channels;
    std::function<void(Ref<Texture2D>)> Callback;  // 加载完成回调（可选）
};

// ── 模型 CPU 解析结果 ──────────────────────────────────────

struct MeshLoadResult {
    std::string FilePath;
    std::vector<struct MeshCpuData> Meshes;
    std::function<void(std::vector<std::string>)> Callback;
};

struct MeshCpuData {
    std::string Name;
    std::vector<struct MeshVertex> Vertices;
    std::vector<u32> Indices;
    // 关联纹理路径
    std::string AlbedoTexPath;
    std::string NormalTexPath;
    std::string MetallicRoughnessTexPath;
};

// ── 异步资源加载器 ─────────────────────────────────────────
//
// 利用 JobSystem 工作线程做 CPU 密集型（磁盘IO+解码），
// 主线程做 GPU 上传（OpenGL 调用），避免帧卡顿。
//
// 用法:
//   AsyncLoader::Init();
//   AsyncLoader::LoadTextureAsync("grass", "textures/grass.png");
//   // 主循环中:
//   AsyncLoader::FlushUploads(4);     // 每帧上传最多 4 个
//   // 退出时:
//   AsyncLoader::Shutdown();

class AsyncLoader {
public:
    /// 初始化 (JobSystem 必须已初始化)
    static void Init();

    /// 关闭并等待所有挂起任务
    static void Shutdown();

    // ── 异步加载提交 ────────────────────────────────────────

    /// 异步加载纹理: 工作线程解码 → 主线程上传 GPU
    /// callback 在主线程 FlushUploads 时调用（可选）
    static void LoadTextureAsync(const std::string& name,
                                  const std::string& filepath,
                                  std::function<void(Ref<Texture2D>)> callback = nullptr);

    /// 异步加载模型: 工作线程解析 → 主线程上传 GPU
    static void LoadModelAsync(const std::string& filepath,
                                std::function<void(std::vector<std::string>)> callback = nullptr);

    // ── 主线程刷新 ──────────────────────────────────────────

    /// 主线程每帧调用: 从完成队列取出 CPU 数据并上传 GPU
    /// budget = 本帧最多上传数量 (0 = 全部)
    static void FlushUploads(u32 budget = 4);

    // ── 状态查询 ────────────────────────────────────────────

    /// 是否有挂起的加载任务
    static bool IsIdle();

    /// 待上传数量（已完成 CPU 解码，等待 GPU 上传）
    static u32 GetPendingUploadCount();

    /// 正在后台处理中的数量
    static u32 GetInFlightCount();

private:
    // 完成队列（工作线程写入，主线程读取）
    static std::queue<TextureLoadResult>  s_TextureQueue;
    static std::queue<MeshLoadResult>     s_MeshQueue;
    static std::mutex                     s_TextureMutex;
    static std::mutex                     s_MeshMutex;

    // 活跃计数
    static std::atomic<u32>               s_InFlight;
    static bool                           s_Initialized;
};

} // namespace Engine
