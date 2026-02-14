#include "engine/debug/gpu_profiler.h"
#include "engine/core/log.h"

#include <glad/glad.h>

// ── GL Timer Query 函数指针 (手动加载) ──────────────────────
// GLAD 自定义构建可能不含 Timer Query 扩展, 手动加载

#ifdef _WIN32
  #include <windows.h>
  extern "C" { typedef void* (*PFNWGLGETPROCADDRESSPROC)(const char*); }
  static void* GetGLProc(const char* name) {
      static HMODULE oglLib = GetModuleHandleA("opengl32.dll");
      void* p = (void*)wglGetProcAddress(name);
      if (!p && oglLib) p = (void*)GetProcAddress(oglLib, name);
      return p;
  }
#else
  static void* GetGLProc(const char* name) { return nullptr; }
#endif

// GL Timer Query 类型和函数指针
typedef void (APIENTRY *PFNGLGENQUERIESPROC)(GLsizei n, GLuint *ids);
typedef void (APIENTRY *PFNGLDELETEQUERIESPROC)(GLsizei n, const GLuint *ids);
typedef void (APIENTRY *PFNGLQUERYCOUNTERPROC)(GLuint id, GLenum target);
typedef void (APIENTRY *PFNGLGETQUERYOBJECTUI64VPROC)(GLuint id, GLenum pname, GLuint64 *params);

#ifndef GL_TIMESTAMP
#define GL_TIMESTAMP 0x8E28
#endif
#ifndef GL_QUERY_RESULT
#define GL_QUERY_RESULT 0x8866
#endif
#ifndef GL_QUERY_RESULT_AVAILABLE
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#endif

static PFNGLGENQUERIESPROC gpuGenQueries = nullptr;
static PFNGLDELETEQUERIESPROC gpuDeleteQueries = nullptr;
static PFNGLQUERYCOUNTERPROC gpuQueryCounter = nullptr;
static PFNGLGETQUERYOBJECTUI64VPROC gpuGetQueryObjectui64v = nullptr;
static bool s_TimerQueryAvailable = false;

static void LoadTimerQueryFunctions() {
    gpuGenQueries = (PFNGLGENQUERIESPROC)GetGLProc("glGenQueries");
    gpuDeleteQueries = (PFNGLDELETEQUERIESPROC)GetGLProc("glDeleteQueries");
    gpuQueryCounter = (PFNGLQUERYCOUNTERPROC)GetGLProc("glQueryCounter");
    gpuGetQueryObjectui64v = (PFNGLGETQUERYOBJECTUI64VPROC)GetGLProc("glGetQueryObjectui64v");

    s_TimerQueryAvailable = gpuGenQueries && gpuDeleteQueries &&
                             gpuQueryCounter && gpuGetQueryObjectui64v;
}

namespace Engine {

void GPUProfiler::Init(u32 maxQueries) {
    LoadTimerQueryFunctions();

    if (!s_TimerQueryAvailable) {
        LOG_WARN("[GPUProfiler] Timer Query 不可用, GPU 计时已禁用");
        s_Enabled = false;
        return;
    }

    s_MaxQueries = maxQueries;
    s_CurrentBuffer = 0;
    s_CurrentDepth = 0;
    s_LastGPUTime = 0;

    for (u32 b = 0; b < BUFFER_COUNT; b++) {
        s_QueryIDs[b].resize(maxQueries * 2);
        gpuGenQueries((GLsizei)(maxQueries * 2), s_QueryIDs[b].data());
        s_NextQueryIdx[b] = 0;
        s_CurrentPasses[b].clear();
    }

    s_Enabled = true;
    LOG_INFO("[GPUProfiler] 初始化 | %u 个查询 × 2 缓冲", maxQueries);
}

void GPUProfiler::Shutdown() {
    if (!s_TimerQueryAvailable) return;

    for (u32 b = 0; b < BUFFER_COUNT; b++) {
        if (!s_QueryIDs[b].empty()) {
            gpuDeleteQueries((GLsizei)s_QueryIDs[b].size(), s_QueryIDs[b].data());
            s_QueryIDs[b].clear();
        }
        s_CurrentPasses[b].clear();
    }
    s_LastResults.clear();
    LOG_INFO("[GPUProfiler] 关闭");
}

void GPUProfiler::BeginFrame() {
    if (!s_Enabled || !s_TimerQueryAvailable) return;

    u32 readBuffer = 1 - s_CurrentBuffer;
    s_LastResults.clear();
    s_LastGPUTime = 0;

    for (auto& pass : s_CurrentPasses[readBuffer]) {
        GLuint64 beginTime = 0, endTime = 0;
        gpuGetQueryObjectui64v(pass.BeginQuery, GL_QUERY_RESULT, &beginTime);
        gpuGetQueryObjectui64v(pass.EndQuery, GL_QUERY_RESULT, &endTime);

        f32 timeMs = (f32)(endTime - beginTime) / 1000000.0f;

        PassResult result;
        result.Name = pass.Name;
        result.TimeMs = timeMs;
        result.Depth = pass.Depth;
        s_LastResults.push_back(result);

        if (pass.Depth == 0) s_LastGPUTime += timeMs;
    }

    s_NextQueryIdx[s_CurrentBuffer] = 0;
    s_CurrentPasses[s_CurrentBuffer].clear();
    s_CurrentDepth = 0;
}

void GPUProfiler::EndFrame() {
    if (!s_Enabled || !s_TimerQueryAvailable) return;
    s_CurrentBuffer = 1 - s_CurrentBuffer;
}

void GPUProfiler::BeginPass(const std::string& name) {
    if (!s_Enabled || !s_TimerQueryAvailable) return;

    u32 buf = s_CurrentBuffer;
    u32 idx = s_NextQueryIdx[buf];

    if (idx + 1 >= s_MaxQueries * 2) return;

    u32 beginQuery = s_QueryIDs[buf][idx];
    u32 endQuery = s_QueryIDs[buf][idx + 1];

    gpuQueryCounter(beginQuery, GL_TIMESTAMP);

    QueryPair pair;
    pair.BeginQuery = beginQuery;
    pair.EndQuery = endQuery;
    pair.Name = name;
    pair.Depth = s_CurrentDepth;
    s_CurrentPasses[buf].push_back(pair);

    s_NextQueryIdx[buf] = idx + 2;
    s_CurrentDepth++;
}

void GPUProfiler::EndPass() {
    if (!s_Enabled || !s_TimerQueryAvailable) return;

    u32 buf = s_CurrentBuffer;
    if (s_CurrentPasses[buf].empty()) return;

    if (s_CurrentDepth > 0) s_CurrentDepth--;

    for (int i = (int)s_CurrentPasses[buf].size() - 1; i >= 0; i--) {
        auto& p = s_CurrentPasses[buf][i];
        if (p.Depth == s_CurrentDepth) {
            gpuQueryCounter(p.EndQuery, GL_TIMESTAMP);
            break;
        }
    }
}

const std::vector<GPUProfiler::PassResult>& GPUProfiler::GetLastFrameResults() {
    return s_LastResults;
}

f32 GPUProfiler::GetLastFrameGPUTime() { return s_LastGPUTime; }

void GPUProfiler::SetEnabled(bool enabled) {
    s_Enabled = enabled && s_TimerQueryAvailable;
}
bool GPUProfiler::IsEnabled() { return s_Enabled; }

} // namespace Engine
