#pragma once

#include "engine/core/types.h"
#include <string>

struct GLFWwindow;

namespace Engine {

// ── 窗口配置 ────────────────────────────────────────────────

struct WindowConfig {
    std::string Title = "Game Engine";
    u32 Width  = 1280;
    u32 Height = 720;
    bool VSync = true;
};

// ── 窗口类 ──────────────────────────────────────────────────

class Window {
public:
    Window(const WindowConfig& config = {});
    ~Window();

    // 禁止拷贝
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void Update();
    bool ShouldClose() const;

    u32 GetWidth() const { return m_Width; }
    u32 GetHeight() const { return m_Height; }
    GLFWwindow* GetNativeWindow() const { return m_Window; }

    void SetVSync(bool enabled);
    bool IsVSync() const { return m_VSync; }

private:
    GLFWwindow* m_Window = nullptr;
    std::string m_Title;
    u32 m_Width = 0;
    u32 m_Height = 0;
    bool m_VSync = true;

    void Init(const WindowConfig& config);
    void Shutdown();
};

} // namespace Engine
