#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"

#include <glm/glm.hpp>
#include <vector>

namespace Engine {

// ── 调试线框渲染 ────────────────────────────────────────────
// 绘制调试用线框图形：线段、AABB、球体、射线、坐标轴等
// 所有图形每帧提交、每帧清除（立即模式风格）

class DebugDraw {
public:
    static void Init();
    static void Shutdown();

    /// 提交调试图形（在 Update 阶段调用）
    static void Line(const glm::vec3& from, const glm::vec3& to,
                     const glm::vec3& color = {0, 1, 0});

    static void AABB(const glm::vec3& min, const glm::vec3& max,
                     const glm::vec3& color = {0, 1, 0});

    static void Sphere(const glm::vec3& center, f32 radius,
                       const glm::vec3& color = {0, 1, 1}, u32 segments = 16);

    static void Ray(const glm::vec3& origin, const glm::vec3& direction, f32 length = 5.0f,
                    const glm::vec3& color = {1, 1, 0});

    static void Axes(const glm::vec3& origin, f32 length = 1.0f);

    static void Circle(const glm::vec3& center, f32 radius,
                       const glm::vec3& normal = {0, 1, 0},
                       const glm::vec3& color = {1, 0, 1}, u32 segments = 32);

    static void Cross(const glm::vec3& position, f32 size = 0.3f,
                      const glm::vec3& color = {1, 1, 1});

    static void Grid(f32 size = 20.0f, f32 step = 1.0f,
                     const glm::vec3& color = {0.3f, 0.3f, 0.3f});

    /// 渲染并清空所有排队的线段
    static void Flush(const f32* viewProjectionMatrix);

    /// 线段计数
    static u32 GetLineCount();

    static void SetEnabled(bool enabled);
    static bool IsEnabled();

    /// 线宽控制
    static void SetLineWidth(f32 width);
    static f32 GetLineWidth();

private:
    struct LineVertex {
        glm::vec3 Position;
        glm::vec3 Color;
    };

    static std::vector<LineVertex> s_Vertices;
    static u32 s_VAO;
    static u32 s_VBO;
    static Ref<Shader> s_Shader;
    static bool s_Enabled;
    static f32 s_LineWidth;
    static constexpr u32 MAX_LINES = 10000;
};

} // namespace Engine
