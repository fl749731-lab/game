#pragma once

#include "engine/core/types.h"
#include <functional>
#include <vector>
#include <unordered_map>
#include <string>
#include <typeindex>
#include <memory>

namespace Engine {

// ── 事件基类 ────────────────────────────────────────────────

struct Event {
    virtual ~Event() = default;
    virtual const char* GetName() const = 0;
    bool Handled = false;
};

// ── 窗口事件 ────────────────────────────────────────────────

struct WindowResizeEvent : public Event {
    u32 Width, Height;
    WindowResizeEvent(u32 w, u32 h) : Width(w), Height(h) {}
    const char* GetName() const override { return "WindowResize"; }
};

struct WindowCloseEvent : public Event {
    const char* GetName() const override { return "WindowClose"; }
};

// ── 键盘事件 ────────────────────────────────────────────────

struct KeyEvent : public Event {
    i32 KeyCode;
    KeyEvent(i32 key) : KeyCode(key) {}
};

struct KeyPressedEvent : public KeyEvent {
    i32 RepeatCount;
    KeyPressedEvent(i32 key, i32 repeat = 0) : KeyEvent(key), RepeatCount(repeat) {}
    const char* GetName() const override { return "KeyPressed"; }
};

struct KeyReleasedEvent : public KeyEvent {
    KeyReleasedEvent(i32 key) : KeyEvent(key) {}
    const char* GetName() const override { return "KeyReleased"; }
};

// ── 鼠标事件 ────────────────────────────────────────────────

struct MouseMovedEvent : public Event {
    f32 X, Y;
    MouseMovedEvent(f32 x, f32 y) : X(x), Y(y) {}
    const char* GetName() const override { return "MouseMoved"; }
};

struct MouseScrolledEvent : public Event {
    f32 OffsetX, OffsetY;
    MouseScrolledEvent(f32 ox, f32 oy) : OffsetX(ox), OffsetY(oy) {}
    const char* GetName() const override { return "MouseScrolled"; }
};

struct MouseButtonEvent : public Event {
    i32 Button;
    MouseButtonEvent(i32 btn) : Button(btn) {}
};

struct MouseButtonPressedEvent : public MouseButtonEvent {
    MouseButtonPressedEvent(i32 btn) : MouseButtonEvent(btn) {}
    const char* GetName() const override { return "MouseButtonPressed"; }
};

struct MouseButtonReleasedEvent : public MouseButtonEvent {
    MouseButtonReleasedEvent(i32 btn) : MouseButtonEvent(btn) {}
    const char* GetName() const override { return "MouseButtonReleased"; }
};

// ── 碰撞事件 ────────────────────────────────────────────────

struct CollisionEvent : public Event {
    u32 EntityA = 0;
    u32 EntityB = 0;
    f32 PenetrationX = 0, PenetrationY = 0, PenetrationZ = 0;
    f32 NormalX = 0, NormalY = 0, NormalZ = 0;
    CollisionEvent(u32 a, u32 b, f32 nx, f32 ny, f32 nz, f32 pen)
        : EntityA(a), EntityB(b), NormalX(nx), NormalY(ny), NormalZ(nz),
          PenetrationX(pen * nx), PenetrationY(pen * ny), PenetrationZ(pen * nz) {}
    const char* GetName() const override { return "Collision"; }
};

// ── 实体生命周期事件 ─────────────────────────────────────────

struct EntityCreatedEvent : public Event {
    u32 EntityID = 0;
    std::string EntityName;
    EntityCreatedEvent(u32 id, const std::string& name) : EntityID(id), EntityName(name) {}
    const char* GetName() const override { return "EntityCreated"; }
};

struct EntityDestroyedEvent : public Event {
    u32 EntityID = 0;
    EntityDestroyedEvent(u32 id) : EntityID(id) {}
    const char* GetName() const override { return "EntityDestroyed"; }
};

// ── 场景事件 ─────────────────────────────────────────────────

struct SceneChangedEvent : public Event {
    std::string OldSceneName;
    std::string NewSceneName;
    SceneChangedEvent(const std::string& oldName, const std::string& newName)
        : OldSceneName(oldName), NewSceneName(newName) {}
    const char* GetName() const override { return "SceneChanged"; }
};

// ── 订阅 ID ─────────────────────────────────────────────────

using SubscriptionID = u64;

// ── 事件分发器 ──────────────────────────────────────────────

class EventDispatcher {
public:
    using HandlerFn = std::function<void(Event&)>;

    /// 订阅某类型事件，返回可用于取消订阅的 ID
    template<typename T>
    SubscriptionID Subscribe(std::function<void(T&)> handler) {
        auto typeIdx = std::type_index(typeid(T));
        SubscriptionID id = m_NextID++;
        m_Handlers[typeIdx].push_back({id, [handler](Event& e) {
            handler(static_cast<T&>(e));
        }});
        return id;
    }

    /// 取消订阅 (按 ID)
    void Unsubscribe(SubscriptionID id) {
        for (auto& [type, entries] : m_Handlers) {
            std::erase_if(entries, [id](const HandlerEntry& e) {
                return e.ID == id;
            });
        }
    }

    /// 发布事件
    template<typename T>
    void Dispatch(T& event) {
        auto typeIdx = std::type_index(typeid(T));
        auto it = m_Handlers.find(typeIdx);
        if (it != m_Handlers.end()) {
            for (auto& entry : it->second) {
                entry.Fn(event);
                if (event.Handled) break;
            }
        }
    }

    /// 清除所有监听
    void Clear() { m_Handlers.clear(); m_NextID = 1; }

private:
    struct HandlerEntry {
        SubscriptionID ID;
        HandlerFn      Fn;
    };

    SubscriptionID m_NextID = 1;
    std::unordered_map<std::type_index, std::vector<HandlerEntry>> m_Handlers;
};

// ── RAII 订阅守卫 ───────────────────────────────────────────
// 析构时自动取消订阅，防止悬垂回调

class ScopedSubscription {
public:
    ScopedSubscription() = default;
    ScopedSubscription(EventDispatcher& d, SubscriptionID id)
        : m_Dispatcher(&d), m_ID(id) {}

    ~ScopedSubscription() { Unsubscribe(); }

    // 禁止拷贝
    ScopedSubscription(const ScopedSubscription&) = delete;
    ScopedSubscription& operator=(const ScopedSubscription&) = delete;

    // 允许移动
    ScopedSubscription(ScopedSubscription&& o) noexcept
        : m_Dispatcher(o.m_Dispatcher), m_ID(o.m_ID) {
        o.m_Dispatcher = nullptr;
        o.m_ID = 0;
    }
    ScopedSubscription& operator=(ScopedSubscription&& o) noexcept {
        Unsubscribe();
        m_Dispatcher = o.m_Dispatcher;
        m_ID = o.m_ID;
        o.m_Dispatcher = nullptr;
        o.m_ID = 0;
        return *this;
    }

    void Unsubscribe() {
        if (m_Dispatcher && m_ID != 0) {
            m_Dispatcher->Unsubscribe(m_ID);
            m_ID = 0;
        }
    }

    SubscriptionID GetID() const { return m_ID; }

private:
    EventDispatcher* m_Dispatcher = nullptr;
    SubscriptionID   m_ID = 0;
};

// ── 全局事件总线 ─────────────────────────────────────────────
// 单例模式，方便跨模块发布/订阅

class EventBus {
public:
    static EventDispatcher& Get() {
        static EventDispatcher s_Instance;
        return s_Instance;
    }

    /// 订阅 (返回 ID，需手动或通过 ScopedSubscription 管理)
    template<typename T>
    static SubscriptionID Subscribe(std::function<void(T&)> handler) {
        return Get().Subscribe<T>(handler);
    }

    /// 取消订阅
    static void Unsubscribe(SubscriptionID id) {
        Get().Unsubscribe(id);
    }

    /// 订阅并返回 RAII 守卫 (推荐用法)
    template<typename T>
    static ScopedSubscription ScopedSubscribe(std::function<void(T&)> handler) {
        SubscriptionID id = Get().Subscribe<T>(handler);
        return ScopedSubscription(Get(), id);
    }

    template<typename T>
    static void Dispatch(T& event) {
        Get().Dispatch<T>(event);
    }
};

} // namespace Engine
