#include "engine/renderer/screen_quad.h"
#include "engine/core/log.h"

#include <glad/glad.h>

namespace Engine {

u32 ScreenQuad::s_VAO = 0;
u32 ScreenQuad::s_VBO = 0;
bool ScreenQuad::s_Initialized = false;

static const float quadVertices[] = {
    // position (2)  texcoord (2)
    -1.0f, -1.0f,    0.0f, 0.0f,
     1.0f, -1.0f,    1.0f, 0.0f,
     1.0f,  1.0f,    1.0f, 1.0f,
    -1.0f, -1.0f,    0.0f, 0.0f,
     1.0f,  1.0f,    1.0f, 1.0f,
    -1.0f,  1.0f,    0.0f, 1.0f,
};

void ScreenQuad::Init() {
    if (s_Initialized) return;

    glGenVertexArrays(1, &s_VAO);
    glGenBuffers(1, &s_VBO);
    glBindVertexArray(s_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // position (2 floats)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    // texcoord (2 floats)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
    s_Initialized = true;
    LOG_DEBUG("[ScreenQuad] 全屏四边形共享 VAO/VBO 初始化完成");
}

void ScreenQuad::Shutdown() {
    if (!s_Initialized) return;
    if (s_VAO) { glDeleteVertexArrays(1, &s_VAO); s_VAO = 0; }
    if (s_VBO) { glDeleteBuffers(1, &s_VBO); s_VBO = 0; }
    s_Initialized = false;
}

void ScreenQuad::Draw() {
    glBindVertexArray(s_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

} // namespace Engine
