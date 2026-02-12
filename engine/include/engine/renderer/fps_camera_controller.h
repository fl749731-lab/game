#pragma once

#include "engine/core/types.h"
#include "engine/renderer/camera.h"
#include "engine/platform/input.h"

namespace Engine {

// ── FPS 摄像机控制器 ────────────────────────────────────────
// 封装 WASD + 鼠标 + 滚轮 FOV 的标准 FPS 摄像机控制
// Sandbox 只需每帧调用 Update() 即可

struct FPSCameraConfig {
    f32 MoveSpeed     = 5.0f;     // 移动速度 (m/s)
    f32 SprintMulti   = 2.0f;     // Shift 加速倍率
    f32 LookSpeed     = 80.0f;    // 键盘旋转速度 (度/s)
    f32 MouseSens     = 0.15f;    // 鼠标灵敏度
    f32 ZoomSpeed     = 30.0f;    // FOV 缩放速度 (度/s)
    f32 ScrollZoomStep = 1.0f;    // 每次滚轮 FOV 变化
    f32 MinPitch      = -89.0f;
    f32 MaxPitch      =  89.0f;
};

class FPSCameraController {
public:
    FPSCameraController() = default;
    explicit FPSCameraController(PerspectiveCamera* camera, const FPSCameraConfig& config = {});

    /// 每帧调用 — 处理输入并更新摄像机
    void Update(f32 dt);

    /// 设置目标摄像机
    void SetCamera(PerspectiveCamera* camera) { m_Camera = camera; }

    /// 鼠标捕获控制
    bool IsCaptured() const { return m_Captured; }
    void SetCaptured(bool captured);

    /// 配置
    FPSCameraConfig& GetConfig() { return m_Config; }
    const FPSCameraConfig& GetConfig() const { return m_Config; }

private:
    PerspectiveCamera* m_Camera = nullptr;
    FPSCameraConfig m_Config;
    bool m_Captured = false;
};

} // namespace Engine
