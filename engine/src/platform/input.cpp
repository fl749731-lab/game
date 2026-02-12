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

void Input::Init(GLFWwindow* window) {
    s_Window = window;
    glfwSetScrollCallback(window, ScrollCallback);
}

void Input::Update() {
    // 鼠标 Delta
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

    // 每帧消耗滚轮值
    s_ScrollOffset = 0;
}

void Input::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    s_ScrollOffset = (float)yoffset;
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
