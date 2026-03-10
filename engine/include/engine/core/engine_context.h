#pragma once

// ── EngineContext —— 引擎子系统注册表 ───────────────────────
//
// 目的: 统一管理所有子系统的生命周期和初始化/销毁顺序。
//
// 设计原则:
//   1. EngineContext 拥有所有子系统实例
//   2. 提供 Get<T>() 类型安全访问
//   3. 现有代码可通过 EngineContext::Instance() 全局访问 (迁移过渡期)
//   4. 未来可改为显式传递 EngineContext& 实现完全去单例化
//
// 用法:
//   // 初始化 (Application::OnInit 中)
//   EngineContext::Init();
//   EngineContext::Instance().Register<ResourceManager>();
//   EngineContext::Instance().Register<SceneManager>();
//
//   // 获取子系统
//   auto& resources = EngineContext::Instance().Get<ResourceManager>();
//
//   // 清理 (Application::OnShutdown 中, 反序销毁)
//   EngineContext::Shutdown();

#include "engine/core/types.h"
#include "engine/core/log.h"
#include <typeindex>
#include <unordered_map>
#include <memory>
#include <vector>
#include <cassert>

namespace Engine {

/// 子系统基类 — 所有通过 EngineContext 管理的子系统需继承此类
class ISubsystem {
public:
    virtual ~ISubsystem() = default;

    /// 子系统名称（用于日志和调试）
    virtual const char* GetName() const = 0;

    /// 可选: 初始化后回调（所有子系统注册完毕后调用）
    virtual void OnInit() {}

    /// 可选: 每帧更新
    virtual void OnUpdate(f32 dt) { (void)dt; }

    /// 可选: 关闭前回调（反序销毁前调用）
    virtual void OnShutdown() {}
};

/// 引擎上下文 — 子系统注册表 + 生命周期管理
class EngineContext {
public:
    // ── 全局访问（迁移过渡期使用，未来可移除）─────────────

    static void Init() {
        assert(!s_Instance && "EngineContext already initialized!");
        s_Instance = new EngineContext();
        LOG_INFO("[EngineContext] 初始化");
    }

    static void Shutdown() {
        if (s_Instance) {
            s_Instance->ShutdownAll();
            delete s_Instance;
            s_Instance = nullptr;
            LOG_INFO("[EngineContext] 已销毁");
        }
    }

    static EngineContext& Instance() {
        assert(s_Instance && "EngineContext not initialized! Call EngineContext::Init() first.");
        return *s_Instance;
    }

    static bool IsInitialized() { return s_Instance != nullptr; }

    // ── 子系统注册 ──────────────────────────────────────────

    /// 注册并构造子系统
    template<typename T, typename... Args>
    T& Register(Args&&... args) {
        static_assert(std::is_base_of_v<ISubsystem, T>,
            "T must derive from ISubsystem");

        auto typeIdx = std::type_index(typeid(T));
        assert(m_Subsystems.find(typeIdx) == m_Subsystems.end() &&
            "Subsystem already registered!");

        auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *ptr;
        m_InitOrder.push_back(typeIdx);
        m_Subsystems[typeIdx] = std::move(ptr);

        LOG_INFO("[EngineContext] 注册子系统: %s", ref.GetName());
        return ref;
    }

    /// 获取已注册的子系统
    template<typename T>
    T& Get() {
        auto typeIdx = std::type_index(typeid(T));
        auto it = m_Subsystems.find(typeIdx);
        assert(it != m_Subsystems.end() && "Subsystem not registered!");
        return *static_cast<T*>(it->second.get());
    }

    /// 检查子系统是否已注册
    template<typename T>
    bool Has() const {
        auto typeIdx = std::type_index(typeid(T));
        return m_Subsystems.find(typeIdx) != m_Subsystems.end();
    }

    /// 初始化所有子系统（注册完毕后调用一次）
    void InitAll() {
        for (auto& typeIdx : m_InitOrder) {
            m_Subsystems[typeIdx]->OnInit();
        }
    }

    /// 更新所有子系统
    void UpdateAll(f32 dt) {
        for (auto& typeIdx : m_InitOrder) {
            m_Subsystems[typeIdx]->OnUpdate(dt);
        }
    }

    /// 反序关闭所有子系统
    void ShutdownAll() {
        for (auto it = m_InitOrder.rbegin(); it != m_InitOrder.rend(); ++it) {
            auto& sub = m_Subsystems[*it];
            LOG_INFO("[EngineContext] 关闭子系统: %s", sub->GetName());
            sub->OnShutdown();
        }
        m_Subsystems.clear();
        m_InitOrder.clear();
    }

private:
    EngineContext() = default;

    static EngineContext* s_Instance;

    std::unordered_map<std::type_index, Scope<ISubsystem>> m_Subsystems;
    std::vector<std::type_index> m_InitOrder;  // 保持注册顺序
};

} // namespace Engine
