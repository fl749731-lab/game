#include "engine/core/async_loader.h"
#include "engine/core/job_system.h"
#include "engine/core/resource_manager.h"
#include "engine/core/log.h"

#include "stb_image.h"

#include <algorithm>

namespace Engine {

// ── 静态成员定义 ────────────────────────────────────────────

std::queue<TextureLoadResult>  AsyncLoader::s_TextureQueue;
std::queue<MeshLoadResult>     AsyncLoader::s_MeshQueue;
std::mutex                     AsyncLoader::s_TextureMutex;
std::mutex                     AsyncLoader::s_MeshMutex;
std::atomic<u32>               AsyncLoader::s_InFlight{0};
bool                           AsyncLoader::s_Initialized = false;

// ── 初始化 / 关闭 ───────────────────────────────────────────

void AsyncLoader::Init() {
    if (s_Initialized) return;
    s_Initialized = true;
    LOG_INFO("[AsyncLoader] 初始化完成");
}

void AsyncLoader::Shutdown() {
    if (!s_Initialized) return;

    // 等待所有后台任务完成
    JobSystem::WaitIdle();

    // 清理队列中未上传的数据
    {
        std::lock_guard<std::mutex> lock(s_TextureMutex);
        while (!s_TextureQueue.empty()) {
            auto& item = s_TextureQueue.front();
            if (item.PixelData) {
                stbi_image_free(item.PixelData);
            }
            s_TextureQueue.pop();
        }
    }
    {
        std::lock_guard<std::mutex> lock(s_MeshMutex);
        while (!s_MeshQueue.empty()) {
            s_MeshQueue.pop();
        }
    }

    s_InFlight = 0;
    s_Initialized = false;
    LOG_INFO("[AsyncLoader] 已关闭");
}

// ── 异步纹理加载 ────────────────────────────────────────────

void AsyncLoader::LoadTextureAsync(const std::string& name,
                                    const std::string& filepath,
                                    std::function<void(Ref<Texture2D>)> callback) {
    if (!s_Initialized) {
        LOG_WARN("[AsyncLoader] 未初始化，回退到同步加载: %s", name.c_str());
        auto tex = ResourceManager::LoadTexture(name, filepath);
        if (callback) callback(tex);
        return;
    }

    // 检查缓存
    auto cached = ResourceManager::GetTexture(name);
    if (cached) {
        if (callback) callback(cached);
        return;
    }

    s_InFlight++;

    // 捕获到 lambda 中
    std::string capName = name;
    std::string capPath = filepath;
    auto capCallback = std::move(callback);

    JobSystem::Submit([capName, capPath, capCallback]() {
        // ── 工作线程：纯 CPU 操作 ──────────────────────────────
        stbi_set_flip_vertically_on_load(1);

        int w, h, ch;
        unsigned char* data = stbi_load(capPath.c_str(), &w, &h, &ch, 0);

        if (!data) {
            LOG_ERROR("[AsyncLoader] 纹理解码失败: %s", capPath.c_str());
            s_InFlight--;
            return;
        }

        LOG_DEBUG("[AsyncLoader] 纹理解码完成: %s (%dx%d, %d通道)", 
                  capPath.c_str(), w, h, ch);

        // 推入完成队列
        TextureLoadResult result;
        result.Name = capName;
        result.FilePath = capPath;
        result.PixelData = data;
        result.Width = w;
        result.Height = h;
        result.Channels = ch;
        result.Callback = capCallback;

        {
            std::lock_guard<std::mutex> lock(s_TextureMutex);
            s_TextureQueue.push(std::move(result));
        }
    });
}

// ── 异步模型加载 ────────────────────────────────────────────

void AsyncLoader::LoadModelAsync(const std::string& filepath,
                                  std::function<void(std::vector<std::string>)> callback) {
    if (!s_Initialized) {
        LOG_WARN("[AsyncLoader] 未初始化，回退到同步加载: %s", filepath.c_str());
        auto names = ResourceManager::LoadModel(filepath);
        if (callback) callback(names);
        return;
    }

    s_InFlight++;

    std::string capPath = filepath;
    auto capCallback = std::move(callback);

    JobSystem::Submit([capPath, capCallback]() {
        // ── 工作线程：OBJ 文件解析 ─────────────────────────────
        // 注意: glTF 加载涉及纹理，更复杂，目前回退到同步
        // OBJ 解析可以在工作线程做顶点数据处理

        // 目前首版简单处理：将模型加载推入完成队列
        // 后续可以拆分为更细粒度的 CPU/GPU 分离
        MeshLoadResult result;
        result.FilePath = capPath;
        result.Callback = capCallback;

        {
            std::lock_guard<std::mutex> lock(s_MeshMutex);
            s_MeshQueue.push(std::move(result));
        }
    });
}

// ── 主线程刷新（GPU 上传）──────────────────────────────────

void AsyncLoader::FlushUploads(u32 budget) {
    if (!s_Initialized) return;

    u32 uploaded = 0;

    // ── 纹理上传 ────────────────────────────────────────────
    while (budget == 0 || uploaded < budget) {
        TextureLoadResult item;
        {
            std::lock_guard<std::mutex> lock(s_TextureMutex);
            if (s_TextureQueue.empty()) break;
            item = std::move(s_TextureQueue.front());
            s_TextureQueue.pop();
        }

        // 主线程: 创建 GL 纹理
        auto tex = std::make_shared<Texture2D>(
            (u32)item.Width, (u32)item.Height, (u32)item.Channels, item.PixelData);

        // 释放 CPU 像素数据
        stbi_image_free(item.PixelData);
        item.PixelData = nullptr;

        if (tex->IsValid()) {
            // 存入全局缓存
            ResourceManager::CacheTexture(item.Name, tex);
            LOG_INFO("[AsyncLoader] 纹理上传完成: %s (%dx%d)", 
                     item.Name.c_str(), item.Width, item.Height);
        } else {
            LOG_ERROR("[AsyncLoader] 纹理 GPU 上传失败: %s", item.Name.c_str());
        }

        // 回调
        if (item.Callback) {
            item.Callback(tex);
        }

        s_InFlight--;
        uploaded++;
    }

    // ── 模型上传 ────────────────────────────────────────────
    while (budget == 0 || uploaded < budget) {
        MeshLoadResult item;
        {
            std::lock_guard<std::mutex> lock(s_MeshMutex);
            if (s_MeshQueue.empty()) break;
            item = std::move(s_MeshQueue.front());
            s_MeshQueue.pop();
        }

        // 首版: 在主线程做完整的模型加载（含 GL 调用）
        auto names = ResourceManager::LoadModel(item.FilePath);

        if (item.Callback) {
            item.Callback(names);
        }

        s_InFlight--;
        uploaded++;
    }
}

// ── 状态查询 ────────────────────────────────────────────────

bool AsyncLoader::IsIdle() {
    return s_InFlight == 0;
}

u32 AsyncLoader::GetPendingUploadCount() {
    u32 count = 0;
    {
        std::lock_guard<std::mutex> lock(s_TextureMutex);
        count += (u32)s_TextureQueue.size();
    }
    {
        std::lock_guard<std::mutex> lock(s_MeshMutex);
        count += (u32)s_MeshQueue.size();
    }
    return count;
}

u32 AsyncLoader::GetInFlightCount() {
    return s_InFlight.load();
}

} // namespace Engine
