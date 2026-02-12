#pragma once

#include "engine/core/types.h"

struct GLFWwindow;

namespace Engine {

// ── 键盘键码 ────────────────────────────────────────────────

enum class Key {
    Space   = 32,
    Escape  = 256,
    Enter   = 257,
    Tab     = 258,
    Right   = 262,
    Left    = 263,
    Down    = 264,
    Up      = 265,
    F1 = 290, F2 = 291, F3 = 292, F4 = 293, F5 = 294,
    F6 = 295, F7 = 296, F8 = 297, F9 = 298, F10 = 299,
    F11 = 300, F12 = 301,

    A = 65, B = 66, C = 67, D = 68, E = 69, F = 70,
    G = 71, H = 72, I = 73, J = 74, K = 75, L = 76,
    M = 77, N = 78, O = 79, P = 80, Q = 81, R = 82,
    S = 83, T = 84, U = 85, V = 86, W = 87, X = 88,
    Y = 89, Z = 90,

    Num0 = 48, Num1 = 49, Num2 = 50, Num3 = 51, Num4 = 52,
    Num5 = 53, Num6 = 54, Num7 = 55, Num8 = 56, Num9 = 57,

    LeftShift  = 340,
    LeftCtrl   = 341,
    LeftAlt    = 342,
    RightShift = 344,
    RightCtrl  = 345,
    RightAlt   = 346,
};

// ── 鼠标按钮 ────────────────────────────────────────────────

enum class MouseButton {
    Left   = 0,
    Right  = 1,
    Middle = 2,
};

// ── 输入系统 ────────────────────────────────────────────────

class Input {
public:
    static void Init(GLFWwindow* window);

    /// 键盘
    static bool IsKeyPressed(Key key);
    static bool IsKeyDown(Key key);

    /// 鼠标
    static bool IsMouseButtonPressed(MouseButton button);
    static float GetMouseX();
    static float GetMouseY();
    static float GetMouseDeltaX();
    static float GetMouseDeltaY();
    static float GetScrollOffset();

    /// 每帧更新（在 Window::Update 之前调用）
    static void Update();

private:
    static GLFWwindow* s_Window;
    static float s_LastMouseX, s_LastMouseY;
    static float s_DeltaX, s_DeltaY;
    static float s_ScrollOffset;
    static bool s_FirstMouse;

    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
};

} // namespace Engine
