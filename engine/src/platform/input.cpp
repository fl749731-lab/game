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

void Input::Init(GLFWwindow* window) {
    s_Window = window;
    glfwSetScrollCallback(window, ScrollCallback);
}

void Input::Update() {
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
    // 帧末消耗：在下一帧开始前清零
    s_ScrollOffset = 0;
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
