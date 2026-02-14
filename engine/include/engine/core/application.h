#pragma once

#include "engine/core/types.h"
#include "engine/core/event.h"
#include "engine/platform/window.h"

#include <string>
#include <vector>

namespace Engine {

// ── Layer 基类 ──────────────────────────────────────────────
// 所有游戏逻辑 / 编辑器 / 调试工具继承此类

class Layer {
public:
    virtual ~Layer() = default;

    /// 附加到 Application 时调用 (初始化资源)
    virtual void OnAttach() {}

    /// 从 Application 移除时调用 (清理资源)
    virtual void OnDetach() {}

    /// 逻辑更新 (每帧)
    virtual void OnUpdate(f32 dt) {}

    /// 渲染 (每帧，在 OnUpdate 之后)
    virtual void OnRender() {}

    /// ImGui 绘制 (每帧，在 OnRender 之后)
    virtual void OnImGui() {}

    /// 事件处理 (窗口 Resize / 键盘 / 鼠标等)
    virtual void OnEvent(Event& e) {}

    /// 层名称 (调试用)
    virtual const char* GetName() const = 0;
};

// ── Application 配置 ────────────────────────────────────────

struct ApplicationConfig {
    std::string Title = "Game Engine";
    u32 Width  = 1280;
    u32 Height = 720;
    bool VSync = true;
};

// ── Application 类 ──────────────────────────────────────────
// 管理窗口、子系统初始化、主循环和 Layer 栈
//
// 生命周期:
//   构造 → PushLayer() → Run() → 析构
//
// 子类化 Layer 实现具体游戏逻辑

class Application {
public:
    Application(const ApplicationConfig& config = {});
    virtual ~Application();

    // 禁止拷贝
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /// 进入主循环 (阻塞直到窗口关闭)
    void Run();

    /// 请求退出
    void Close();

    /// Layer 管理
    void PushLayer(Scope<Layer> layer);
    void PopLayer();

    /// 窗口访问
    Window& GetWindow() { return m_Window; }
    const Window& GetWindow() const { return m_Window; }

    /// 全局单例访问
    static Application& Get();

private:
    void InitSubsystems();
    void ShutdownSubsystems();

    Window m_Window;
    std::vector<Scope<Layer>> m_Layers;
    bool m_Running = true;

    static Application* s_Instance;
};

} // namespace Engine
