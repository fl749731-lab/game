#include "engine/renderer/skybox.h"
#include "engine/renderer/renderer.h"
#include "engine/core/log.h"

#include <glad/glad.h>

namespace Engine {

u32 Skybox::s_CubeVAO = 0;
u32 Skybox::s_CubeVBO = 0;
Ref<Shader> Skybox::s_Shader = nullptr;
f32 Skybox::s_TopColor[3] = {0.1f, 0.15f, 0.5f};
f32 Skybox::s_HorizonColor[3] = {0.7f, 0.5f, 0.3f};
f32 Skybox::s_BottomColor[3] = {0.15f, 0.12f, 0.1f};
f32 Skybox::s_SunDir[3] = {0.3f, 0.6f, -0.5f};

static const char* skyVert = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
out vec3 vPos;
uniform mat4 uVP;
void main() {
    vPos = aPos;
    vec4 p = uVP * vec4(aPos, 0.0);
    gl_Position = p.xyww;
}
)";

static const char* skyFrag = R"(
#version 450 core
in vec3 vPos;
out vec4 FragColor;

uniform vec3 uTopColor;
uniform vec3 uHorizonColor;
uniform vec3 uBottomColor;
uniform vec3 uSunDir;

void main() {
    vec3 dir = normalize(vPos);
    float y = dir.y;

    // 天空渐变
    vec3 color;
    if (y > 0.0) {
        float t = pow(y, 0.4);
        color = mix(uHorizonColor, uTopColor, t);
    } else {
        float t = pow(-y, 0.6);
        color = mix(uHorizonColor, uBottomColor, t);
    }

    // 太阳光晕
    vec3 sunDir = normalize(uSunDir);
    float sunDot = max(dot(dir, sunDir), 0.0);
    float sunGlow = pow(sunDot, 64.0);
    float sunHalo = pow(sunDot, 8.0) * 0.3;
    color += vec3(1.0, 0.95, 0.8) * sunGlow * 1.5;
    color += vec3(1.0, 0.7, 0.4) * sunHalo;

    FragColor = vec4(color, 1.0);
}
)";

static const float cubeVerts[] = {
    -1, 1,-1, -1,-1,-1, 1,-1,-1,  1,-1,-1,  1, 1,-1, -1, 1,-1,
    -1,-1, 1, -1,-1,-1, -1, 1,-1, -1, 1,-1, -1, 1, 1, -1,-1, 1,
     1,-1,-1,  1,-1, 1,  1, 1, 1,  1, 1, 1,  1, 1,-1,  1,-1,-1,
    -1,-1, 1, -1, 1, 1,  1, 1, 1,  1, 1, 1,  1,-1, 1, -1,-1, 1,
    -1, 1,-1,  1, 1,-1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1, 1,-1,
    -1,-1,-1, -1,-1, 1,  1,-1,-1,  1,-1,-1, -1,-1, 1,  1,-1, 1,
};

void Skybox::Init() {
    glGenVertexArrays(1, &s_CubeVAO);
    glGenBuffers(1, &s_CubeVBO);
    glBindVertexArray(s_CubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, s_CubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glBindVertexArray(0);

    s_Shader = std::make_shared<Shader>(skyVert, skyFrag);
    LOG_INFO("[天空盒] 初始化完成");
}

void Skybox::Shutdown() {
    if (s_CubeVAO) { glDeleteVertexArrays(1, &s_CubeVAO); s_CubeVAO = 0; }
    if (s_CubeVBO) { glDeleteBuffers(1, &s_CubeVBO); s_CubeVBO = 0; }
    s_Shader.reset();
}

void Skybox::Draw(const f32* viewProjectionMatrix) {
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);

    s_Shader->Bind();
    s_Shader->SetMat4("uVP", viewProjectionMatrix);
    s_Shader->SetVec3("uTopColor", s_TopColor[0], s_TopColor[1], s_TopColor[2]);
    s_Shader->SetVec3("uHorizonColor", s_HorizonColor[0], s_HorizonColor[1], s_HorizonColor[2]);
    s_Shader->SetVec3("uBottomColor", s_BottomColor[0], s_BottomColor[1], s_BottomColor[2]);
    s_Shader->SetVec3("uSunDir", s_SunDir[0], s_SunDir[1], s_SunDir[2]);

    glBindVertexArray(s_CubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    Renderer::NotifyDraw(12);  // 36 顶点 = 12 个三角形

    glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);
}

void Skybox::SetTopColor(f32 r, f32 g, f32 b)     { s_TopColor[0]=r; s_TopColor[1]=g; s_TopColor[2]=b; }
void Skybox::SetHorizonColor(f32 r, f32 g, f32 b) { s_HorizonColor[0]=r; s_HorizonColor[1]=g; s_HorizonColor[2]=b; }
void Skybox::SetBottomColor(f32 r, f32 g, f32 b)  { s_BottomColor[0]=r; s_BottomColor[1]=g; s_BottomColor[2]=b; }
void Skybox::SetSunDirection(f32 x, f32 y, f32 z) { s_SunDir[0]=x; s_SunDir[1]=y; s_SunDir[2]=z; }

} // namespace Engine
