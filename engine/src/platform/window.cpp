#include "engine/platform/window.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Engine {

static bool s_GLFWInitialized = false;

static void GLFWErrorCallback(int error, const char* description) {
    LOG_ERROR("GLFW 错误 (%d): %s", error, description);
}

Window::Window(const WindowConfig& config) {
    Init(config);
}

Window::~Window() {
    Shutdown();
}

void Window::Init(const WindowConfig& config) {
    m_Title  = config.Title;
    m_Width  = config.Width;
    m_Height = config.Height;
    m_VSync  = config.VSync;

    LOG_INFO("创建窗口: %s (%u x %u)", m_Title.c_str(), m_Width, m_Height);

    if (!s_GLFWInitialized) {
        int success = glfwInit();
        if (!success) {
            LOG_FATAL("GLFW 初始化失败!");
            return;
        }
        glfwSetErrorCallback(GLFWErrorCallback);
        s_GLFWInitialized = true;
    }

    // OpenGL 版本提示
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef ENGINE_DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    m_Window = glfwCreateWindow((int)m_Width, (int)m_Height, m_Title.c_str(), nullptr, nullptr);
    if (!m_Window) {
        LOG_FATAL("窗口创建失败!");
        return;
    }

    glfwMakeContextCurrent(m_Window);

    // 加载 OpenGL 函数
    int loaded = gladLoadGL((GLADloadproc)glfwGetProcAddress);
    LOG_INFO("GLAD 加载了 %d 个 OpenGL 函数", loaded);

    LOG_INFO("OpenGL 信息:");
    LOG_INFO("  供应商:   %s", glGetString(GL_VENDOR));
    LOG_INFO("  渲染器:   %s", glGetString(GL_RENDERER));
    LOG_INFO("  版本:     %s", glGetString(GL_VERSION));
    LOG_INFO("  GLSL:     %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

    SetVSync(m_VSync);

    // 窗口大小变化回调
    glfwSetWindowUserPointer(m_Window, this);
    glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, int width, int height) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        self->m_Width = width;
        self->m_Height = height;
        glViewport(0, 0, width, height);
    });
}

void Window::Shutdown() {
    if (m_Window) {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }
    // 注意: 这里不调用 glfwTerminate()，让 main 来管理
}

void Window::Update() {
    glfwPollEvents();
    glfwSwapBuffers(m_Window);
}

bool Window::ShouldClose() const {
    return glfwWindowShouldClose(m_Window);
}

void Window::SetVSync(bool enabled) {
    glfwSwapInterval(enabled ? 1 : 0);
    m_VSync = enabled;
}

} // namespace Engine
