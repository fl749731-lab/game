#pragma once

#include "engine/core/types.h"
#include "engine/core/log.h"

#include <string>
#include <vector>
#include <functional>

namespace Engine {

// ── Java 数据层 JNI 桥接 ────────────────────────────────────
//
// 通过 JNI 连接 C++ 引擎与 Java 数据层
// Java 端负责: 数据模型、配置管理、持久化
// 需要 cmake -DENGINE_ENABLE_JAVA=ON 开启

#ifdef ENGINE_ENABLE_JAVA

// ── JNI 配置 ───────────────────────────────────────────────

struct JNIBridgeConfig {
    std::string JavaClassPath = "data/";        // Java 类路径
    std::string MainClass     = "engine.Data";  // Java 主类
    bool        CreateJVM     = true;
    std::vector<std::string> JVMArgs;            // 额外 JVM 参数
};

// ── JNI 桥接 ───────────────────────────────────────────────

class JNIBridge {
public:
    /// 初始化 JVM 并加载 Java 类
    static bool Init(const JNIBridgeConfig& config = {});

    /// 关闭 JVM
    static void Shutdown();

    /// JVM 是否就绪
    static bool IsInitialized();

    // ── 数据操作 ────────────────────────────────────────────

    /// 加载配置 (调用 Java 端的 loadConfig 方法)
    static std::string LoadConfig(const std::string& key);

    /// 保存配置
    static bool SaveConfig(const std::string& key, const std::string& value);

    /// 批量加载配置
    static std::vector<std::pair<std::string, std::string>>
        LoadAllConfigs(const std::string& category);

    // ── 通用 Java 方法调用 ──────────────────────────────────

    /// 调用静态 Java 方法 (返回字符串)
    static std::string CallStaticString(const char* className,
                                         const char* methodName,
                                         const char* signature,
                                         ...);

    /// 调用静态 Java 方法 (返回 int)
    static int CallStaticInt(const char* className,
                              const char* methodName,
                              const char* signature,
                              ...);

    /// 调用静态 Java 方法 (返回 void)
    static void CallStaticVoid(const char* className,
                                const char* methodName,
                                const char* signature,
                                ...);

private:
    static void* s_JVM;     // JavaVM*
    static void* s_Env;     // JNIEnv*
    static bool  s_Initialized;
};

#else // !ENGINE_ENABLE_JAVA

class JNIBridge {
public:
    static bool Init(const void* = nullptr) {
        LOG_WARN("[JNI] Java bridge disabled. Rebuild with -DENGINE_ENABLE_JAVA=ON");
        return false;
    }
    static void Shutdown() {}
    static bool IsInitialized() { return false; }
    static std::string LoadConfig(const std::string&) { return ""; }
    static bool SaveConfig(const std::string&, const std::string&) { return false; }
};

#endif // ENGINE_ENABLE_JAVA

} // namespace Engine
