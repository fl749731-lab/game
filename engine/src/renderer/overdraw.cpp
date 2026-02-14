#include "engine/renderer/overdraw.h"
#include "engine/core/log.h"

#include <glad/glad.h>

namespace Engine {

// ── 着色器源码 ──────────────────────────────────────────────

const char* OverdrawVisualization::GetVertexShaderSource() {
    return R"(
#version 450 core
layout(location = 0) in vec3 aPos;

uniform mat4 uMVP;

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";
}

const char* OverdrawVisualization::GetFragmentShaderSource() {
    return R"(
#version 450 core
layout(location = 0) out vec4 FragColor;

void main() {
    // 每个片元写入固定增量 (用于叠加混合)
    FragColor = vec4(1.0 / 255.0, 0.0, 0.0, 1.0);
}
)";
}

const char* OverdrawVisualization::GetHeatmapFragSource() {
    return R"(
#version 450 core
layout(location = 0) out vec4 FragColor;

in vec2 vTexCoord;
uniform sampler2D uCountTexture;

// 热力图: 黑→蓝→绿→黄→红→白
vec3 heatmap(float t) {
    if (t < 0.2) return mix(vec3(0,0,0), vec3(0,0,1), t / 0.2);
    if (t < 0.4) return mix(vec3(0,0,1), vec3(0,1,0), (t - 0.2) / 0.2);
    if (t < 0.6) return mix(vec3(0,1,0), vec3(1,1,0), (t - 0.4) / 0.2);
    if (t < 0.8) return mix(vec3(1,1,0), vec3(1,0,0), (t - 0.6) / 0.2);
    return mix(vec3(1,0,0), vec3(1,1,1), (t - 0.8) / 0.2);
}

void main() {
    float count = texture(uCountTexture, vTexCoord).r;
    // 归一化: 假定最大 overdraw 为 10 次
    float t = clamp(count * 255.0 / 10.0, 0.0, 1.0);
    FragColor = vec4(heatmap(t), 0.7);
}
)";
}

// ── 辅助: 编译着色器 ────────────────────────────────────────

static u32 CompileShader(GLenum type, const char* src) {
    u32 shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    int ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        LOG_ERROR("[Overdraw] 着色器编译失败: %s", log);
    }
    return shader;
}

static u32 LinkProgram(u32 vs, u32 fs) {
    u32 prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    int ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        LOG_ERROR("[Overdraw] 程序链接失败: %s", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ── 初始化/关闭 ─────────────────────────────────────────────

void OverdrawVisualization::Init(u32 width, u32 height) {
    s_Width = width;
    s_Height = height;

    // 创建 overdraw 累积纹理 (R8)
    glGenTextures(1, &s_CountTexture);
    glBindTexture(GL_TEXTURE_2D, s_CountTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // 创建 FBO
    glGenFramebuffers(1, &s_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, s_FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_CountTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR("[Overdraw] FBO 不完整");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 编译着色器
    {
        u32 vs = CompileShader(GL_VERTEX_SHADER, GetVertexShaderSource());
        u32 fs = CompileShader(GL_FRAGMENT_SHADER, GetFragmentShaderSource());
        s_OverdrawProgram = LinkProgram(vs, fs);
    }

    // 热力图着色器
    {
        const char* quadVS = R"(
#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPos, 0, 1);
}
)";
        u32 vs = CompileShader(GL_VERTEX_SHADER, quadVS);
        u32 fs = CompileShader(GL_FRAGMENT_SHADER, GetHeatmapFragSource());
        s_HeatmapProgram = LinkProgram(vs, fs);
    }

    LOG_INFO("[Overdraw] 初始化完成 (%ux%u)", width, height);
}

void OverdrawVisualization::Shutdown() {
    if (s_FBO) glDeleteFramebuffers(1, &s_FBO);
    if (s_CountTexture) glDeleteTextures(1, &s_CountTexture);
    if (s_OverdrawProgram) glDeleteProgram(s_OverdrawProgram);
    if (s_HeatmapProgram) glDeleteProgram(s_HeatmapProgram);
    s_FBO = s_CountTexture = s_OverdrawProgram = s_HeatmapProgram = 0;
    LOG_INFO("[Overdraw] 关闭");
}

void OverdrawVisualization::Begin() {
    if (!s_Enabled) return;

    glBindFramebuffer(GL_FRAMEBUFFER, s_FBO);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    // 启用叠加混合: dst += src
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    // GL_FUNC_ADD 是默认混合方程，无需显式设置

    // 禁用深度测试 (让所有片元都写入)
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glUseProgram(s_OverdrawProgram);
}

void OverdrawVisualization::End() {
    if (!s_Enabled) return;

    glUseProgram(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OverdrawVisualization::RenderOverlay(u32 screenWidth, u32 screenHeight) {
    if (!s_Enabled) return;

    // 全屏四边形 — 用热力图着色器显示
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(s_HeatmapProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_CountTexture);
    glUniform1i(glGetUniformLocation(s_HeatmapProgram, "uCountTexture"), 0);

    // 使用简化的即时模式绘制 (TODO: 使用 ScreenQuad)
    // 这里只设置着色器状态，实际绘制由外部 ScreenQuad 完成

    glUseProgram(0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

} // namespace Engine
