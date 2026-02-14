#include "engine/editor/gizmo.h"
#include "engine/debug/debug_draw.h"
#include "engine/core/log.h"

#include <imgui.h>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Engine {

void Gizmo::Init() {
    s_Mode = GizmoMode::Translate;
    s_Space = GizmoSpace::World;
    s_ActiveAxis = GizmoAxis::None;
    s_HoveredAxis = GizmoAxis::None;
    s_Dragging = false;
    LOG_INFO("[Gizmo] 初始化 | UE 级 3D 操控器");
}

void Gizmo::Shutdown() { LOG_INFO("[Gizmo] 关闭"); }

void Gizmo::SetMode(GizmoMode mode) { s_Mode = mode; }
GizmoMode Gizmo::GetMode() { return s_Mode; }
const char* Gizmo::GetModeName() {
    switch(s_Mode) {
        case GizmoMode::Translate: return "平移 (W)";
        case GizmoMode::Rotate: return "旋转 (E)";
        case GizmoMode::Scale: return "缩放 (R)";
        default: return "无";
    }
}

void Gizmo::SetSpace(GizmoSpace space) { s_Space = space; }
GizmoSpace Gizmo::GetSpace() { return s_Space; }
void Gizmo::ToggleSpace() {
    s_Space = (s_Space == GizmoSpace::World) ? GizmoSpace::Local : GizmoSpace::World;
}

void Gizmo::SetSnapEnabled(bool enabled) { s_Snap.Enabled = enabled; }
bool Gizmo::IsSnapEnabled() { return s_Snap.Enabled; }
GizmoSnap& Gizmo::GetSnapConfig() { return s_Snap; }

void Gizmo::Begin(const glm::vec3& position,
                   const glm::vec3& rotation,
                   const glm::vec3& scale) {
    s_Position = position; s_Rotation = rotation; s_Scale = scale;
    s_InitPosition = position; s_InitRotation = rotation; s_InitScale = scale;
}

void Gizmo::End() { s_Dragging = false; s_ActiveAxis = GizmoAxis::None; }

bool Gizmo::Update(const glm::mat4& viewMatrix, const glm::mat4& projMatrix,
                    f32 viewportWidth, f32 viewportHeight,
                    f32 mouseX, f32 mouseY, bool mouseDown, bool ctrlDown) {
    Ray ray = ScreenToRay(viewMatrix, projMatrix, viewportWidth, viewportHeight, mouseX, mouseY);
    f32 size = CalculateScreenScale(s_Position, viewMatrix, projMatrix, viewportHeight);

    // 悬停检测
    if (!s_Dragging) {
        s_HoveredAxis = HitTest(ray, s_Position, size);
    }

    // 吸附: Ctrl 按住时启用
    s_Snap.Enabled = ctrlDown;

    if (!s_Dragging && mouseDown) {
        GizmoAxis hit = s_HoveredAxis;
        if (hit != GizmoAxis::None) {
            s_ActiveAxis = hit;
            s_Dragging = true;
            s_DragStart = ray.At(1.0f);
            s_InitPosition = s_Position;
            s_InitRotation = s_Rotation;
            s_InitScale = s_Scale;
        }
    }

    if (s_Dragging) {
        if (!mouseDown) {
            s_Dragging = false;
            s_ActiveAxis = GizmoAxis::None;
            return false;
        }

        glm::vec3 dragCurrent = ray.At(1.0f);
        glm::vec3 delta = dragCurrent - s_DragStart;

        // 约束到活动轴
        glm::vec3 axisMask(0);
        switch(s_ActiveAxis) {
            case GizmoAxis::X: axisMask = {1,0,0}; break;
            case GizmoAxis::Y: axisMask = {0,1,0}; break;
            case GizmoAxis::Z: axisMask = {0,0,1}; break;
            case GizmoAxis::XY: axisMask = {1,1,0}; break;
            case GizmoAxis::XZ: axisMask = {1,0,1}; break;
            case GizmoAxis::YZ: axisMask = {0,1,1}; break;
            case GizmoAxis::All: axisMask = {1,1,1}; break;
            default: break;
        }
        delta *= axisMask;

        switch (s_Mode) {
            case GizmoMode::Translate: {
                glm::vec3 newPos = s_InitPosition + delta;
                if (s_Snap.Enabled) {
                    newPos.x = Snap(newPos.x, s_Snap.TranslateSnap);
                    newPos.y = Snap(newPos.y, s_Snap.TranslateSnap);
                    newPos.z = Snap(newPos.z, s_Snap.TranslateSnap);
                }
                s_Position = newPos;
                break;
            }
            case GizmoMode::Rotate: {
                glm::vec3 newRot = s_InitRotation + delta * 90.0f;
                if (s_Snap.Enabled) {
                    newRot.x = Snap(newRot.x, s_Snap.RotateSnapDeg);
                    newRot.y = Snap(newRot.y, s_Snap.RotateSnapDeg);
                    newRot.z = Snap(newRot.z, s_Snap.RotateSnapDeg);
                }
                s_Rotation = newRot;
                break;
            }
            case GizmoMode::Scale: {
                glm::vec3 newScale = s_InitScale + delta;
                if (s_Snap.Enabled) {
                    newScale.x = Snap(newScale.x, s_Snap.ScaleSnap);
                    newScale.y = Snap(newScale.y, s_Snap.ScaleSnap);
                    newScale.z = Snap(newScale.z, s_Snap.ScaleSnap);
                }
                newScale = glm::max(newScale, glm::vec3(0.01f));
                s_Scale = newScale;
                break;
            }
            default: break;
        }
        return true;
    }
    return false;
}

glm::vec3 Gizmo::GetResultPosition() { return s_Position; }
glm::vec3 Gizmo::GetResultRotation() { return s_Rotation; }
glm::vec3 Gizmo::GetResultScale() { return s_Scale; }

// ── 3D 渲染 ────────────────────────────────────────────────

void Gizmo::Render(const glm::vec3& position,
                    const glm::mat4& viewMatrix,
                    const glm::mat4& projMatrix,
                    f32 viewportWidth, f32 viewportHeight) {
    f32 size = CalculateScreenScale(position, viewMatrix, projMatrix, viewportHeight);

    switch (s_Mode) {
        case GizmoMode::Translate: RenderTranslateHandles(position, size); break;
        case GizmoMode::Rotate:    RenderRotateHandles(position, size); break;
        case GizmoMode::Scale:     RenderScaleHandles(position, size); break;
        default: break;
    }
}

void Gizmo::RenderTranslateHandles(const glm::vec3& pos, f32 size) {
    // 轴颜色 (悬停/活跃时高亮黄色)
    auto GetColor = [](GizmoAxis axis, const glm::vec3& baseColor) -> glm::vec3 {
        if (s_ActiveAxis == axis || s_HoveredAxis == axis)
            return {1.0f, 1.0f, 0.2f};
        return baseColor;
    };

    // X 轴 — 锥体箭头
    DrawConeArrow(pos, pos + glm::vec3(size, 0, 0), GetColor(GizmoAxis::X, {1,0.2f,0.2f}), size*0.06f, 8);
    // Y 轴
    DrawConeArrow(pos, pos + glm::vec3(0, size, 0), GetColor(GizmoAxis::Y, {0.2f,1,0.2f}), size*0.06f, 8);
    // Z 轴
    DrawConeArrow(pos, pos + glm::vec3(0, 0, size), GetColor(GizmoAxis::Z, {0.2f,0.2f,1}), size*0.06f, 8);

    // 双轴平面手柄
    f32 ps = size * 0.25f;

    // XY 平面
    glm::vec3 xyColor = GetColor(GizmoAxis::XY, {0.8f, 0.8f, 0.0f});
    DebugDraw::Line(pos + glm::vec3(ps, 0, 0), pos + glm::vec3(ps, ps, 0), xyColor);
    DebugDraw::Line(pos + glm::vec3(0, ps, 0), pos + glm::vec3(ps, ps, 0), xyColor);

    // XZ 平面
    glm::vec3 xzColor = GetColor(GizmoAxis::XZ, {0.8f, 0.0f, 0.8f});
    DebugDraw::Line(pos + glm::vec3(ps, 0, 0), pos + glm::vec3(ps, 0, ps), xzColor);
    DebugDraw::Line(pos + glm::vec3(0, 0, ps), pos + glm::vec3(ps, 0, ps), xzColor);

    // YZ 平面
    glm::vec3 yzColor = GetColor(GizmoAxis::YZ, {0.0f, 0.8f, 0.8f});
    DebugDraw::Line(pos + glm::vec3(0, ps, 0), pos + glm::vec3(0, ps, ps), yzColor);
    DebugDraw::Line(pos + glm::vec3(0, 0, ps), pos + glm::vec3(0, ps, ps), yzColor);

    // 中心白色小十字 (All 轴)
    f32 cs = size * 0.08f;
    glm::vec3 allColor = GetColor(GizmoAxis::All, {0.9f, 0.9f, 0.9f});
    DebugDraw::Line(pos - glm::vec3(cs, 0, 0), pos + glm::vec3(cs, 0, 0), allColor);
    DebugDraw::Line(pos - glm::vec3(0, cs, 0), pos + glm::vec3(0, cs, 0), allColor);
}

void Gizmo::RenderRotateHandles(const glm::vec3& pos, f32 size) {
    auto GetColor = [](GizmoAxis axis, const glm::vec3& baseColor) -> glm::vec3 {
        if (s_ActiveAxis == axis || s_HoveredAxis == axis)
            return {1.0f, 1.0f, 0.2f};
        return baseColor;
    };

    f32 radius = size * 0.9f;

    // X 环 (YZ 平面)
    DebugDraw::Circle(pos, radius, {1, 0, 0}, GetColor(GizmoAxis::X, {1, 0.2f, 0.2f}), 48);
    // Y 环 (XZ 平面)
    DebugDraw::Circle(pos, radius, {0, 1, 0}, GetColor(GizmoAxis::Y, {0.2f, 1, 0.2f}), 48);
    // Z 环 (XY 平面)
    DebugDraw::Circle(pos, radius, {0, 0, 1}, GetColor(GizmoAxis::Z, {0.2f, 0.2f, 1}), 48);

    // 外圈 (屏幕空间旋转 — 灰色)
    DebugDraw::Circle(pos, radius * 1.1f, {0, 1, 0}, {0.5f, 0.5f, 0.5f}, 64);
}

void Gizmo::RenderScaleHandles(const glm::vec3& pos, f32 size) {
    auto GetColor = [](GizmoAxis axis, const glm::vec3& baseColor) -> glm::vec3 {
        if (s_ActiveAxis == axis || s_HoveredAxis == axis)
            return {1.0f, 1.0f, 0.2f};
        return baseColor;
    };

    f32 cubeHalf = size * 0.05f;

    // X 轴 + 立方体
    glm::vec3 xEnd = pos + glm::vec3(size, 0, 0);
    DebugDraw::Line(pos, xEnd, GetColor(GizmoAxis::X, {1, 0.2f, 0.2f}));
    DrawCube(xEnd, cubeHalf, GetColor(GizmoAxis::X, {1, 0.2f, 0.2f}));

    // Y 轴
    glm::vec3 yEnd = pos + glm::vec3(0, size, 0);
    DebugDraw::Line(pos, yEnd, GetColor(GizmoAxis::Y, {0.2f, 1, 0.2f}));
    DrawCube(yEnd, cubeHalf, GetColor(GizmoAxis::Y, {0.2f, 1, 0.2f}));

    // Z 轴
    glm::vec3 zEnd = pos + glm::vec3(0, 0, size);
    DebugDraw::Line(pos, zEnd, GetColor(GizmoAxis::Z, {0.2f, 0.2f, 1}));
    DrawCube(zEnd, cubeHalf, GetColor(GizmoAxis::Z, {0.2f, 0.2f, 1}));

    // 中心三角 (均匀缩放)
    f32 t = size * 0.15f;
    glm::vec3 allColor = GetColor(GizmoAxis::All, {0.9f, 0.9f, 0.9f});
    DebugDraw::Line(pos + glm::vec3(t,0,0), pos + glm::vec3(0,t,0), allColor);
    DebugDraw::Line(pos + glm::vec3(0,t,0), pos + glm::vec3(0,0,t), allColor);
    DebugDraw::Line(pos + glm::vec3(0,0,t), pos + glm::vec3(t,0,0), allColor);
}

// ── 几何绘制辅助 ────────────────────────────────────────────

void Gizmo::DrawConeArrow(const glm::vec3& from, const glm::vec3& to,
                            const glm::vec3& color, f32 coneRadius, u32 segments) {
    glm::vec3 dir = glm::normalize(to - from);
    f32 length = glm::length(to - from);
    f32 coneLength = length * 0.15f;
    glm::vec3 coneBase = from + dir * (length - coneLength);

    // 轴线
    DebugDraw::Line(from, coneBase, color);

    // 锥体 (线段模拟)
    glm::vec3 up = (std::abs(dir.y) < 0.99f) ? glm::vec3(0,1,0) : glm::vec3(1,0,0);
    glm::vec3 right = glm::normalize(glm::cross(dir, up));
    glm::vec3 forward = glm::normalize(glm::cross(right, dir));

    for (u32 i = 0; i < segments; i++) {
        f32 a0 = (f32)i / (f32)segments * 2.0f * (f32)M_PI;
        f32 a1 = (f32)(i+1) / (f32)segments * 2.0f * (f32)M_PI;

        glm::vec3 p0 = coneBase + (right * std::cos(a0) + forward * std::sin(a0)) * coneRadius;
        glm::vec3 p1 = coneBase + (right * std::cos(a1) + forward * std::sin(a1)) * coneRadius;

        DebugDraw::Line(to, p0, color);   // 锥顶→底边
        DebugDraw::Line(p0, p1, color);   // 底边环
    }
}

void Gizmo::DrawCube(const glm::vec3& center, f32 h, const glm::vec3& color) {
    glm::vec3 min = center - glm::vec3(h);
    glm::vec3 max = center + glm::vec3(h);
    DebugDraw::AABB(min, max, color);
}

f32 Gizmo::CalculateScreenScale(const glm::vec3& position,
                                  const glm::mat4& viewMatrix,
                                  const glm::mat4& projMatrix,
                                  f32 viewportHeight) {
    // 在视空间中计算距离
    glm::vec4 viewPos = viewMatrix * glm::vec4(position, 1.0f);
    f32 distance = -viewPos.z;
    if (distance < 0.1f) distance = 0.1f;

    return distance * SCREEN_SCALE_FACTOR;
}

f32 Gizmo::Snap(f32 value, f32 step) {
    return std::round(value / step) * step;
}

// ── 射线拾取 ────────────────────────────────────────────────

Ray Gizmo::ScreenToRay(const glm::mat4& viewMatrix, const glm::mat4& projMatrix,
                        f32 viewportWidth, f32 viewportHeight,
                        f32 mouseX, f32 mouseY) {
    f32 ndcX = (2.0f * mouseX / viewportWidth) - 1.0f;
    f32 ndcY = 1.0f - (2.0f * mouseY / viewportHeight);
    glm::mat4 invVP = glm::inverse(projMatrix * viewMatrix);
    glm::vec4 nearPt = invVP * glm::vec4(ndcX, ndcY, -1, 1); nearPt /= nearPt.w;
    glm::vec4 farPt = invVP * glm::vec4(ndcX, ndcY, 1, 1); farPt /= farPt.w;
    Ray ray;
    ray.Origin = glm::vec3(nearPt);
    ray.Direction = glm::normalize(glm::vec3(farPt) - glm::vec3(nearPt));
    return ray;
}

GizmoAxis Gizmo::HitTest(const Ray& ray, const glm::vec3& gizmoPos, f32 size) {
    f32 r = AXIS_HIT_RADIUS * size / BASE_GIZMO_SIZE;

    AABB xBox; xBox.Min = gizmoPos + glm::vec3(0,-r,-r); xBox.Max = gizmoPos + glm::vec3(size,r,r);
    AABB yBox; yBox.Min = gizmoPos + glm::vec3(-r,0,-r); yBox.Max = gizmoPos + glm::vec3(r,size,r);
    AABB zBox; zBox.Min = gizmoPos + glm::vec3(-r,-r,0); zBox.Max = gizmoPos + glm::vec3(r,r,size);

    glm::vec3 invDir = 1.0f / glm::max(glm::abs(ray.Direction), glm::vec3(1e-8f));
    invDir *= glm::sign(ray.Direction + glm::vec3(1e-8f));

    if (xBox.RayIntersect(ray.Origin, invDir)) return GizmoAxis::X;
    if (yBox.RayIntersect(ray.Origin, invDir)) return GizmoAxis::Y;
    if (zBox.RayIntersect(ray.Origin, invDir)) return GizmoAxis::Z;

    return GizmoAxis::None;
}

bool Gizmo::IsDragging() { return s_Dragging; }
GizmoAxis Gizmo::GetActiveAxis() { return s_ActiveAxis; }
GizmoAxis Gizmo::GetHoveredAxis() { return s_HoveredAxis; }

// ── 快捷键 ──────────────────────────────────────────────────

bool Gizmo::HandleKeyInput(int key, int action) {
    if (action != 1) return false;  // 只处理按下
    // GLFW: W=87 E=69 R=82 Q=81
    switch (key) {
        case 87: SetMode(GizmoMode::Translate); return true; // W
        case 69: SetMode(GizmoMode::Rotate); return true;    // E
        case 82: SetMode(GizmoMode::Scale); return true;     // R
        case 81: ToggleSpace(); return true;                  // Q
        default: return false;
    }
}

// ── ImGui 工具栏 ────────────────────────────────────────────

void Gizmo::RenderToolbar() {
    auto ButtonActive = [](const char* label, bool active) -> bool {
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.2f, 1));
        }
        bool clicked = ImGui::SmallButton(label);
        ImGui::PopStyleColor();
        return clicked;
    };

    if (ButtonActive("W 平移", s_Mode == GizmoMode::Translate)) SetMode(GizmoMode::Translate);
    ImGui::SameLine();
    if (ButtonActive("E 旋转", s_Mode == GizmoMode::Rotate)) SetMode(GizmoMode::Rotate);
    ImGui::SameLine();
    if (ButtonActive("R 缩放", s_Mode == GizmoMode::Scale)) SetMode(GizmoMode::Scale);
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();
    if (ButtonActive(s_Space == GizmoSpace::World ? "世界" : "本地", true)) ToggleSpace();
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();
    ImGui::Text("吸附: %s", s_Snap.Enabled ? "ON" : "OFF (Ctrl)");
}

// ── 多选变换 ────────────────────────────────────────────────

void Gizmo::BeginMulti(const std::vector<glm::vec3>& positions) {
    s_MultiPositions = positions;
    s_MultiInitPositions = positions;

    // 计算质心
    s_Centroid = glm::vec3(0);
    for (auto& p : positions) s_Centroid += p;
    if (!positions.empty()) s_Centroid /= (f32)positions.size();

    // 用质心作为 Gizmo 位置
    Begin(s_Centroid, {0,0,0}, {1,1,1});
}

bool Gizmo::UpdateMulti(const glm::mat4& viewMatrix, const glm::mat4& projMatrix,
                         f32 viewportW, f32 viewportH,
                         f32 mouseX, f32 mouseY, bool mouseDown, bool ctrlDown) {
    bool dragging = Update(viewMatrix, projMatrix, viewportW, viewportH,
                           mouseX, mouseY, mouseDown, ctrlDown);

    if (dragging) {
        // 计算质心偏移量，应用到所有实体
        glm::vec3 delta = s_Position - s_InitPosition;
        for (size_t i = 0; i < s_MultiPositions.size(); i++) {
            s_MultiPositions[i] = s_MultiInitPositions[i] + delta;
        }
        s_Centroid = s_Position;
    }

    return dragging;
}

std::vector<glm::vec3> Gizmo::GetResultPositions() {
    return s_MultiPositions;
}

} // namespace Engine
