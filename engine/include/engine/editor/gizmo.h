#pragma once

#include "engine/core/types.h"
#include "engine/physics/collision.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Engine {

// ── Gizmo 操作模式 ──────────────────────────────────────────

enum class GizmoMode : u8 {
    None      = 0,
    Translate = 1,
    Rotate    = 2,
    Scale     = 3,
};

// ── Gizmo 活动轴 ───────────────────────────────────────────

enum class GizmoAxis : u8 {
    None = 0,
    X    = 1,
    Y    = 2,
    Z    = 3,
    XY   = 4,
    XZ   = 5,
    YZ   = 6,
    All  = 7,
};

// ── 坐标系空间 ─────────────────────────────────────────────

enum class GizmoSpace : u8 {
    World = 0,
    Local = 1,
};

// ── 吸附配置 ────────────────────────────────────────────────

struct GizmoSnap {
    bool Enabled = false;
    f32 TranslateSnap = 0.25f;    // 平移吸附步长
    f32 RotateSnapDeg = 15.0f;    // 旋转吸附角度
    f32 ScaleSnap = 0.1f;         // 缩放吸附步长
};

// ── Gizmo (UE 级场景变换操控器) ─────────────────────────────
//
// 增强特性:
//   1. 3D 锥体箭头 / 旋转环 / 缩放立方手柄
//   2. 网格吸附 (Ctrl 按住)
//   3. 世界/本地空间切换
//   4. 视点自适应缩放 (屏幕上始终等大)
//   5. 双轴平面手柄 (XY/XZ/YZ 半透明正方形)
//   6. 高亮悬停轴 + 活跃轴黄色

class Gizmo {
public:
    static void Init();
    static void Shutdown();

    /// 模式
    static void SetMode(GizmoMode mode);
    static GizmoMode GetMode();
    static const char* GetModeName();

    /// 坐标空间
    static void SetSpace(GizmoSpace space);
    static GizmoSpace GetSpace();
    static void ToggleSpace();

    /// 吸附
    static void SetSnapEnabled(bool enabled);
    static bool IsSnapEnabled();
    static GizmoSnap& GetSnapConfig();

    /// 开始/结束操控
    static void Begin(const glm::vec3& position,
                      const glm::vec3& rotation,
                      const glm::vec3& scale);
    static void End();

    /// 更新 — 返回是否正在拖拽
    static bool Update(const glm::mat4& viewMatrix,
                       const glm::mat4& projMatrix,
                       f32 viewportWidth, f32 viewportHeight,
                       f32 mouseX, f32 mouseY,
                       bool mouseDown, bool ctrlDown);

    /// 结果
    static glm::vec3 GetResultPosition();
    static glm::vec3 GetResultRotation();
    static glm::vec3 GetResultScale();

    /// 3D 渲染 (DebugDraw —— 锥体箭头 / 旋转环 / 缩放立方)
    static void Render(const glm::vec3& position,
                       const glm::mat4& viewMatrix,
                       const glm::mat4& projMatrix,
                       f32 viewportWidth, f32 viewportHeight);

    /// 渲染 ImGui 工具栏 (模式/空间/吸附 按钮)
    static void RenderToolbar();

    /// 快捷键: W=平移 E=旋转 R=缩放 Q=空间切换
    static bool HandleKeyInput(int key, int action);

    /// 射线拾取
    static Ray ScreenToRay(const glm::mat4& viewMatrix,
                           const glm::mat4& projMatrix,
                           f32 viewportWidth, f32 viewportHeight,
                           f32 mouseX, f32 mouseY);

    static GizmoAxis HitTest(const Ray& ray, const glm::vec3& gizmoPos, f32 size);

    static bool IsDragging();
    static GizmoAxis GetActiveAxis();
    static GizmoAxis GetHoveredAxis();

private:
    /// 渲染各模式手柄
    static void RenderTranslateHandles(const glm::vec3& pos, f32 size);
    static void RenderRotateHandles(const glm::vec3& pos, f32 size);
    static void RenderScaleHandles(const glm::vec3& pos, f32 size);

    /// 绘制锥体箭头 (用线段模拟)
    static void DrawConeArrow(const glm::vec3& from, const glm::vec3& to,
                               const glm::vec3& color, f32 coneRadius, u32 segments);

    /// 绘制立方体手柄
    static void DrawCube(const glm::vec3& center, f32 halfSize, const glm::vec3& color);

    /// 计算视点自适应缩放
    static f32 CalculateScreenScale(const glm::vec3& position,
                                     const glm::mat4& viewMatrix,
                                     const glm::mat4& projMatrix,
                                     f32 viewportHeight);

    /// 吸附值
    static f32 Snap(f32 value, f32 step);

    inline static GizmoMode s_Mode = GizmoMode::Translate;
    inline static GizmoSpace s_Space = GizmoSpace::World;
    inline static GizmoSnap s_Snap;
    inline static GizmoAxis s_ActiveAxis = GizmoAxis::None;
    inline static GizmoAxis s_HoveredAxis = GizmoAxis::None;
    inline static bool s_Dragging = false;

    inline static glm::vec3 s_Position = {};
    inline static glm::vec3 s_Rotation = {};
    inline static glm::vec3 s_Scale = {1,1,1};

    inline static glm::vec3 s_DragStart = {};
    inline static glm::vec3 s_InitPosition = {};
    inline static glm::vec3 s_InitRotation = {};
    inline static glm::vec3 s_InitScale = {};

    static constexpr f32 BASE_GIZMO_SIZE = 1.5f;
    static constexpr f32 AXIS_HIT_RADIUS = 0.15f;
    static constexpr f32 SCREEN_SCALE_FACTOR = 0.1f; // 视点缩放系数
};

} // namespace Engine
