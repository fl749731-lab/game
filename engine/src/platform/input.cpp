#include "engine/platform/input.h"

#include <GLFW/glfw3.h>

namespace Engine {

GLFWwindow* Input::s_Window = nullptr;
float Input::s_LastMouseX = 0;
float Input::s_LastMouseY = 0;
float Input::s_DeltaX = 0;
float Input::s_DeltaY = 0;
float Input::s_ScrollOffset = 0;
bool Input::s_FirstMouse = true;
CursorMode Input::s_CursorMode = CursorMode::Normal;
std::unordered_map<int, bool> Input::s_KeyStatePrevFrame;
std::unordered_map<int, bool> Input::s_KeyStateCurrFrame;

void Input::Init(GLFWwindow* window) {
    s_Window = window;
    glfwSetScrollCallback(window, ScrollCallback);
}

void Input::Update() {
    // ── 按键状态双缓冲：prev ← curr, 然后重新轮询 curr ──
    // 此时 glfwGetKey 反映的是上一帧末尾 glfwPollEvents 后的状态
    s_KeyStatePrevFrame = s_KeyStateCurrFrame;
    for (auto& [key, state] : s_KeyStateCurrFrame) {
        state = (glfwGetKey(s_Window, key) == GLFW_PRESS);
    }

    // 计算鼠标 Delta
    double mx, my;
    glfwGetCursorPos(s_Window, &mx, &my);
    float x = (float)mx, y = (float)my;

    if (s_FirstMouse) {
        s_LastMouseX = x;
        s_LastMouseY = y;
        s_FirstMouse = false;
    }

    s_DeltaX = x - s_LastMouseX;
    s_DeltaY = s_LastMouseY - y;  // Y 反转（屏幕坐标向下为正）
    s_LastMouseX = x;
    s_LastMouseY = y;
}

void Input::EndFrame() {
    s_ScrollOffset = 0;
    // 按键状态更新已移至 Update()，此处不再处理
}

void Input::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    s_ScrollOffset += (float)yoffset;  // 累加而非覆盖，防止一帧内多次滚动
}

void Input::SetCursorMode(CursorMode mode) {
    s_CursorMode = mode;
    switch (mode) {
        case CursorMode::Normal:
            glfwSetInputMode(s_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            break;
        case CursorMode::Hidden:
            glfwSetInputMode(s_Window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            break;
        case CursorMode::Captured:
            glfwSetInputMode(s_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            s_FirstMouse = true;  // 重新校准，避免跳变
            break;
    }
}

CursorMode Input::GetCursorMode() {
    return s_CursorMode;
}

bool Input::IsKeyPressed(Key key) {
    int state = glfwGetKey(s_Window, static_cast<int>(key));
    return state == GLFW_PRESS;
}

bool Input::IsKeyDown(Key key) {
    int state = glfwGetKey(s_Window, static_cast<int>(key));
    return state == GLFW_PRESS || state == GLFW_REPEAT;
}

bool Input::IsKeyJustPressed(Key key) {
    int k = static_cast<int>(key);
    // 如果是首次查询此键，注册到追踪列表
    if (s_KeyStateCurrFrame.find(k) == s_KeyStateCurrFrame.end()) {
        bool nowPressed = (glfwGetKey(s_Window, k) == GLFW_PRESS);
        s_KeyStateCurrFrame[k] = nowPressed;
        s_KeyStatePrevFrame[k] = false;
        return nowPressed;
    }
    return s_KeyStateCurrFrame[k] && !s_KeyStatePrevFrame[k];
}

bool Input::IsKeyJustReleased(Key key) {
    int k = static_cast<int>(key);
    if (s_KeyStateCurrFrame.find(k) == s_KeyStateCurrFrame.end()) {
        bool nowPressed = (glfwGetKey(s_Window, k) == GLFW_PRESS);
        s_KeyStateCurrFrame[k] = nowPressed;
        s_KeyStatePrevFrame[k] = nowPressed;
        return false;
    }
    return !s_KeyStateCurrFrame[k] && s_KeyStatePrevFrame[k];
}

bool Input::IsMouseButtonPressed(MouseButton button) {
    int state = glfwGetMouseButton(s_Window, static_cast<int>(button));
    return state == GLFW_PRESS;
}

float Input::GetMouseX() {
    double x, y;
    glfwGetCursorPos(s_Window, &x, &y);
    return static_cast<float>(x);
}

float Input::GetMouseY() {
    double x, y;
    glfwGetCursorPos(s_Window, &x, &y);
    return static_cast<float>(y);
}

float Input::GetMouseDeltaX() { return s_DeltaX; }
float Input::GetMouseDeltaY() { return s_DeltaY; }
float Input::GetScrollOffset() { return s_ScrollOffset; }

} // namespace Engine
