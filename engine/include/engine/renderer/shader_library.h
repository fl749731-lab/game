#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <functional>

namespace Engine {

// ── ShaderLibrary ───────────────────────────────────────────
// 功能:
//   1. 从 .glsl 文件加载 Shader (替代 shaders.h 内联字符串)
//   2. #include "xxx.glsl" 预处理
//   3. 文件监视 + 热重载 (Debug 模式)
//   4. 缓存编译后的 Shader 程序
//
// 目录结构:
//   assets/shaders/
//     common.glsl       (共享工具函数)
//     lit.vert           (顶点着色器)
//     lit.frag           (片段着色器)
//     deferred_gbuffer.vert/frag
//     deferred_lighting.vert/frag
//     ...

class ShaderLibrary {
public:
    static void Init(const std::string& shaderDir = "assets/shaders");
    static void Shutdown();

    /// 加载 Shader (自动拼接 shaderDir 路径)
    /// 例: Load("lit", "lit.vert", "lit.frag")
    static Ref<Shader> Load(const std::string& name,
                             const std::string& vertFile,
                             const std::string& fragFile);

    /// 获取已缓存的 Shader
    static Ref<Shader> Get(const std::string& name);

    /// 检查文件改动并热重载 (每帧调用, Debug 用)
    static void CheckHotReload();

    /// 注册热重载回调 (Shader 重新编译后通知)
    using ReloadCallback = std::function<void(const std::string& name, Ref<Shader>)>;
    static void SetReloadCallback(ReloadCallback cb);

    /// #include 预处理器 (递归处理 #include "xxx")
    static std::string Preprocess(const std::string& source,
                                   const std::string& baseDir,
                                   std::unordered_set<std::string>& included);

    /// 统计
    static u32 GetCount();

private:
    struct ShaderEntry {
        std::string Name;
        std::string VertFile;   // 相对路径
        std::string FragFile;
        Ref<Shader> Program;
        std::chrono::file_clock::time_point LastModified;
    };

    static std::string ReadFile(const std::string& filepath);
    static std::chrono::file_clock::time_point GetFileTime(const std::string& filepath);

    inline static std::string s_ShaderDir;
    inline static std::unordered_map<std::string, ShaderEntry> s_Shaders;
    inline static ReloadCallback s_ReloadCallback;
    inline static f32 s_CheckInterval = 1.0f;  // 每秒检查一次
    inline static f32 s_Timer = 0;
};

} // namespace Engine
