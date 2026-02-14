#include "engine/data/jni_bridge.h"
#include "engine/core/log.h"

#include <cstring>

namespace Engine {

#ifdef ENGINE_ENABLE_JAVA

// ── 静态成员 ────────────────────────────────────────────────

void* JNIBridge::s_JVM = nullptr;
void* JNIBridge::s_Env = nullptr;
bool  JNIBridge::s_Initialized = false;

// ═══════════════════════════════════════════════════════════
//  JNI 桥接桩实现
// ═══════════════════════════════════════════════════════════
//
// 真正的 JNI 初始化需要 jni.h 和 jvm.lib
// 当前为桩实现，仅含日志

bool JNIBridge::Init(const JNIBridgeConfig& config) {
    LOG_INFO("[JNI] Initializing Java bridge");
    LOG_INFO("[JNI] ClassPath: %s", config.JavaClassPath.c_str());
    LOG_INFO("[JNI] MainClass: %s", config.MainClass.c_str());

    // TODO: 真正的 JNI 初始化:
    // 1. JNI_CreateJavaVM(&jvm, &env, &vmArgs)
    // 2. FindClass(config.MainClass)
    // 3. 缓存常用方法 ID

    s_Initialized = true;
    LOG_INFO("[JNI] Java bridge ready (stub mode)");
    return true;
}

void JNIBridge::Shutdown() {
    if (!s_Initialized) return;
    LOG_INFO("[JNI] Shutting down Java bridge");
    // TODO: jvm->DestroyJavaVM()
    s_JVM = nullptr;
    s_Env = nullptr;
    s_Initialized = false;
}

bool JNIBridge::IsInitialized() { return s_Initialized; }

// ── 数据操作桩 ──────────────────────────────────────────────

std::string JNIBridge::LoadConfig(const std::string& key) {
    if (!s_Initialized) {
        LOG_WARN("[JNI] Not initialized, cannot load config: %s", key.c_str());
        return "";
    }
    LOG_DEBUG("[JNI] LoadConfig: %s", key.c_str());
    // TODO: 调用 Java DataManager.loadConfig(key)
    return "";
}

bool JNIBridge::SaveConfig(const std::string& key, const std::string& value) {
    if (!s_Initialized) {
        LOG_WARN("[JNI] Not initialized, cannot save config: %s", key.c_str());
        return false;
    }
    LOG_DEBUG("[JNI] SaveConfig: %s = %s", key.c_str(), value.c_str());
    // TODO: 调用 Java DataManager.saveConfig(key, value)
    return true;
}

std::vector<std::pair<std::string, std::string>>
JNIBridge::LoadAllConfigs(const std::string& category) {
    LOG_DEBUG("[JNI] LoadAllConfigs: %s", category.c_str());
    // TODO: 调用 Java DataManager.loadAllConfigs(category)
    return {};
}

// ── 通用 Java 方法调用桩 ────────────────────────────────────

std::string JNIBridge::CallStaticString(const char* className,
                                         const char* methodName,
                                         const char* signature, ...) {
    LOG_DEBUG("[JNI] CallStaticString: %s.%s", className, methodName);
    // TODO: FindClass → GetStaticMethodID → CallStaticObjectMethod
    return "";
}

int JNIBridge::CallStaticInt(const char* className,
                              const char* methodName,
                              const char* signature, ...) {
    LOG_DEBUG("[JNI] CallStaticInt: %s.%s", className, methodName);
    // TODO: CallStaticIntMethod
    return 0;
}

void JNIBridge::CallStaticVoid(const char* className,
                                const char* methodName,
                                const char* signature, ...) {
    LOG_DEBUG("[JNI] CallStaticVoid: %s.%s", className, methodName);
    // TODO: CallStaticVoidMethod
}

#endif // ENGINE_ENABLE_JAVA

} // namespace Engine
