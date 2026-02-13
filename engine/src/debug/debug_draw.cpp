#include "engine/debug/debug_draw.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>

namespace Engine {

std::vector<DebugDraw::LineVertex> DebugDraw::s_Vertices;
u32 DebugDraw::s_VAO = 0;
u32 DebugDraw::s_VBO = 0;
Ref<Shader> DebugDraw::s_Shader = nullptr;
bool DebugDraw::s_Enabled = true;
f32 DebugDraw::s_LineWidth = 2.0f;

static const char* dbgVert = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
out vec3 vColor;
uniform mat4 uVP;
void main() {
    vColor = aColor;
    gl_Position = uVP * vec4(aPos, 1.0);
}
)";

static const char* dbgFrag = R"(
#version 450 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

void DebugDraw::Init() {
    glGenVertexArrays(1, &s_VAO);
    glGenBuffers(1, &s_VBO);
    glBindVertexArray(s_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_LINES * 2 * sizeof(LineVertex), nullptr, GL_DYNAMIC_DRAW);
    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)0);
    // Color
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)(3*sizeof(float)));
    glBindVertexArray(0);

    s_Shader = std::make_shared<Shader>(dbgVert, dbgFrag);
    s_Vertices.reserve(MAX_LINES * 2);
    LOG_INFO("[调试渲染] 初始化完成 (最大 %u 线段)", MAX_LINES);
}

void DebugDraw::Shutdown() {
    if (s_VAO) { glDeleteVertexArrays(1, &s_VAO); s_VAO = 0; }
    if (s_VBO) { glDeleteBuffers(1, &s_VBO); s_VBO = 0; }
    s_Shader.reset();
    s_Vertices.clear();
}

void DebugDraw::Line(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color) {
    if (!s_Enabled) return;
    if (s_Vertices.size() >= MAX_LINES * 2) return;
    s_Vertices.push_back({from, color});
    s_Vertices.push_back({to, color});
}

void DebugDraw::AABB(const glm::vec3& mn, const glm::vec3& mx, const glm::vec3& color) {
    // 12 条边
    glm::vec3 v[8] = {
        {mn.x,mn.y,mn.z}, {mx.x,mn.y,mn.z}, {mx.x,mn.y,mx.z}, {mn.x,mn.y,mx.z},
        {mn.x,mx.y,mn.z}, {mx.x,mx.y,mn.z}, {mx.x,mx.y,mx.z}, {mn.x,mx.y,mx.z},
    };
    // 底面
    Line(v[0],v[1],color); Line(v[1],v[2],color);
    Line(v[2],v[3],color); Line(v[3],v[0],color);
    // 顶面
    Line(v[4],v[5],color); Line(v[5],v[6],color);
    Line(v[6],v[7],color); Line(v[7],v[4],color);
    // 竖直
    Line(v[0],v[4],color); Line(v[1],v[5],color);
    Line(v[2],v[6],color); Line(v[3],v[7],color);
}

void DebugDraw::Sphere(const glm::vec3& center, f32 radius,
                       const glm::vec3& color, u32 segments) {
    const float step = 6.283185f / (float)segments;
    // XY 圈
    for (u32 i = 0; i < segments; i++) {
        float a0 = i * step, a1 = (i+1) * step;
        Line(center + glm::vec3(cosf(a0), sinf(a0), 0)*radius,
             center + glm::vec3(cosf(a1), sinf(a1), 0)*radius, color);
    }
    // XZ 圈
    for (u32 i = 0; i < segments; i++) {
        float a0 = i * step, a1 = (i+1) * step;
        Line(center + glm::vec3(cosf(a0), 0, sinf(a0))*radius,
             center + glm::vec3(cosf(a1), 0, sinf(a1))*radius, color);
    }
    // YZ 圈
    for (u32 i = 0; i < segments; i++) {
        float a0 = i * step, a1 = (i+1) * step;
        Line(center + glm::vec3(0, cosf(a0), sinf(a0))*radius,
             center + glm::vec3(0, cosf(a1), sinf(a1))*radius, color);
    }
}

void DebugDraw::Ray(const glm::vec3& origin, const glm::vec3& direction,
                    f32 length, const glm::vec3& color) {
    glm::vec3 end = origin + glm::normalize(direction) * length;
    Line(origin, end, color);
    // 小箭头
    glm::vec3 dir = glm::normalize(direction);
    glm::vec3 perp = glm::abs(dir.y) < 0.99f
        ? glm::normalize(glm::cross(dir, glm::vec3(0,1,0)))
        : glm::normalize(glm::cross(dir, glm::vec3(1,0,0)));
    float arrowLen = length * 0.08f;
    Line(end, end - dir*arrowLen + perp*arrowLen*0.5f, color);
    Line(end, end - dir*arrowLen - perp*arrowLen*0.5f, color);
}

void DebugDraw::Axes(const glm::vec3& origin, f32 length) {
    Line(origin, origin + glm::vec3(length, 0, 0), {1, 0, 0}); // X 红
    Line(origin, origin + glm::vec3(0, length, 0), {0, 1, 0}); // Y 绿
    Line(origin, origin + glm::vec3(0, 0, length), {0, 0, 1}); // Z 蓝
}

void DebugDraw::Circle(const glm::vec3& center, f32 radius,
                       const glm::vec3& normal, const glm::vec3& color, u32 segments) {
    glm::vec3 n = glm::normalize(normal);
    glm::vec3 tangent = glm::abs(n.y) < 0.99f
        ? glm::normalize(glm::cross(n, glm::vec3(0,1,0)))
        : glm::normalize(glm::cross(n, glm::vec3(1,0,0)));
    glm::vec3 bitangent = glm::cross(n, tangent);

    float step = 6.283185f / (float)segments;
    for (u32 i = 0; i < segments; i++) {
        float a0 = i * step, a1 = (i+1) * step;
        glm::vec3 p0 = center + (tangent * cosf(a0) + bitangent * sinf(a0)) * radius;
        glm::vec3 p1 = center + (tangent * cosf(a1) + bitangent * sinf(a1)) * radius;
        Line(p0, p1, color);
    }
}

void DebugDraw::Cross(const glm::vec3& pos, f32 size, const glm::vec3& color) {
    Line(pos - glm::vec3(size, 0, 0), pos + glm::vec3(size, 0, 0), color);
    Line(pos - glm::vec3(0, size, 0), pos + glm::vec3(0, size, 0), color);
    Line(pos - glm::vec3(0, 0, size), pos + glm::vec3(0, 0, size), color);
}

void DebugDraw::Grid(f32 size, f32 step, const glm::vec3& color) {
    float half = size * 0.5f;
    for (float i = -half; i <= half; i += step) {
        Line({i, 0, -half}, {i, 0,  half}, color);
        Line({-half, 0, i}, { half, 0, i}, color);
    }
}

void DebugDraw::Flush(const f32* viewProjectionMatrix) {
    if (!s_Enabled || s_Vertices.empty()) {
        s_Vertices.clear();
        return;
    }

    // 上传顶点数据
    glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
    size_t vertCount = s_Vertices.size();
    size_t maxVerts = MAX_LINES * 2;
    if (vertCount > maxVerts) vertCount = maxVerts;
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(vertCount * sizeof(LineVertex)), s_Vertices.data());

    s_Shader->Bind();
    s_Shader->SetMat4("uVP", viewProjectionMatrix);

    // 注意：OpenGL 4.5 Core Profile 中 glLineWidth > 1.0 已废弃
    // 会产生 API_ID_LINE_WIDTH 警告，此处不再调用
    glBindVertexArray(s_VAO);
    glDrawArrays(GL_LINES, 0, (GLsizei)vertCount);
    glBindVertexArray(0);

    s_Vertices.clear();
}

u32 DebugDraw::GetLineCount() { return (u32)(s_Vertices.size() / 2); }
void DebugDraw::SetEnabled(bool enabled) { s_Enabled = enabled; }
bool DebugDraw::IsEnabled() { return s_Enabled; }
void DebugDraw::SetLineWidth(f32 width) { s_LineWidth = width; }
f32 DebugDraw::GetLineWidth() { return s_LineWidth; }

} // namespace Engine
