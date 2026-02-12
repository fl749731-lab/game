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

// ── 事件分发器 ──────────────────────────────────────────────

class EventDispatcher {
public:
    using HandlerFn = std::function<void(Event&)>;

    /// 订阅某类型事件
    template<typename T>
    void Subscribe(std::function<void(T&)> handler) {
        auto typeIdx = std::type_index(typeid(T));
        m_Handlers[typeIdx].push_back([handler](Event& e) {
            handler(static_cast<T&>(e));
        });
    }

    /// 发布事件
    template<typename T>
    void Dispatch(T& event) {
        auto typeIdx = std::type_index(typeid(T));
        auto it = m_Handlers.find(typeIdx);
        if (it != m_Handlers.end()) {
            for (auto& fn : it->second) {
                fn(event);
                if (event.Handled) break;
            }
        }
    }

    /// 清除所有监听
    void Clear() { m_Handlers.clear(); }

private:
    std::unordered_map<std::type_index, std::vector<HandlerFn>> m_Handlers;
};

} // namespace Engine
