#include "engine/editor/screen_capture.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <imgui.h>

#ifdef _WIN32
#include <windows.h>
#endif

// glReadPixels 是 GL 1.0 函数但项目自定义 GLAD profile 未包含
// 手动从系统 opengl32 导入
typedef void (APIENTRY *PFN_glReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
static PFN_glReadPixels s_glReadPixels = nullptr;

static void EnsureGlReadPixels() {
    if (s_glReadPixels) return;
#ifdef _WIN32
    // GL 1.0 函数从 opengl32.dll 获取
    HMODULE hGL = GetModuleHandleA("opengl32.dll");
    if (hGL) s_glReadPixels = (PFN_glReadPixels)GetProcAddress(hGL, "glReadPixels");
#endif
    if (!s_glReadPixels) {
        LOG_ERROR("[ScreenCapture] 无法获取 glReadPixels");
    }
}

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cstring>
#include <cstdio>
#include <chrono>

namespace Engine {

void ScreenCapture::Init() {
    s_Recording = false;
    s_FramesCaptured = 0;
    s_FrameCount = 0;
    EnsureGlReadPixels();
    LOG_INFO("[ScreenCapture] 初始化");
}

void ScreenCapture::Shutdown() {
    if (s_Recording) StopRecording();
    LOG_INFO("[ScreenCapture] 关闭");
}

bool ScreenCapture::CaptureFrame(const std::string& filename, u32 width, u32 height, u32 fbo) {
    EnsureGlReadPixels();
    if (!s_glReadPixels) return false;

    std::vector<u8> pixels(width * height * 3);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    s_glReadPixels(0, 0, (GLsizei)width, (GLsizei)height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 垂直翻转 (OpenGL Y 轴向上)
    u32 rowLen = width * 3;
    std::vector<u8> row(rowLen);
    for (u32 y = 0; y < height / 2; y++) {
        u32 topIdx = y * rowLen;
        u32 botIdx = (height - 1 - y) * rowLen;
        memcpy(row.data(), &pixels[topIdx], rowLen);
        memcpy(&pixels[topIdx], &pixels[botIdx], rowLen);
        memcpy(&pixels[botIdx], row.data(), rowLen);
    }

    if (!stbi_write_png(filename.c_str(), (int)width, (int)height, 3, pixels.data(), (int)(width * 3))) {
        LOG_ERROR("[ScreenCapture] 保存失败: %s", filename.c_str());
        return false;
    }

    s_LastCapturePath = filename;
    if (s_Callback) s_Callback(filename);

    LOG_INFO("[ScreenCapture] 已保存: %s", filename.c_str());
    return true;
}

std::string ScreenCapture::CaptureAutoNamed(u32 width, u32 height, u32 fbo) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t);
#else
    localtime_r(&time_t, &tm_buf);
#endif
    char buf[256];
    snprintf(buf, sizeof(buf), "screenshots/screenshot_%04d%02d%02d_%02d%02d%02d.png",
             tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
             tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);

    std::string path(buf);
    CaptureFrame(path, width, height, fbo);
    return path;
}

void ScreenCapture::StartRecording(const std::string& folder, u32 width, u32 height,
                                    u32 captureInterval, u32 fbo) {
    s_RecordFolder = folder;
    s_RecordWidth = width;
    s_RecordHeight = height;
    s_CaptureInterval = captureInterval > 0 ? captureInterval : 1;
    s_RecordFBO = fbo;
    s_Recording = true;
    s_FrameCount = 0;
    s_FramesCaptured = 0;
    LOG_INFO("[ScreenCapture] 开始录制 → %s (间隔 %u 帧)", folder.c_str(), captureInterval);
}

void ScreenCapture::StopRecording() {
    s_Recording = false;
    LOG_INFO("[ScreenCapture] 停止录制 (%u 帧)", s_FramesCaptured);
}

void ScreenCapture::Update() {
    if (!s_Recording) return;

    s_FrameCount++;
    if (s_FrameCount % s_CaptureInterval == 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s/frame_%06u.png", s_RecordFolder.c_str(), s_FramesCaptured);
        CaptureFrame(std::string(buf), s_RecordWidth, s_RecordHeight, s_RecordFBO);
        s_FramesCaptured++;
    }
}

void ScreenCapture::RenderPanel() {
    if (!ImGui::Begin("截图工具##ScreenCapture")) { ImGui::End(); return; }

    ImGui::Text("截图工具");
    ImGui::Separator();

    if (ImGui::Button("📷 截图 (需外部调用)")) {
        LOG_INFO("[ScreenCapture] 请通过代码调用 CaptureAutoNamed(w, h)");
    }

    ImGui::Separator();

    if (!s_Recording) {
        if (ImGui::Button("🔴 开始录制")) {
            LOG_INFO("[ScreenCapture] 请通过代码调用 StartRecording(folder, w, h)");
        }
    } else {
        if (ImGui::Button("⬛ 停止录制")) StopRecording();
        ImGui::SameLine();
        ImGui::Text("帧: %u", s_FramesCaptured);
    }

    ImGui::Separator();
    if (!s_LastCapturePath.empty())
        ImGui::Text("最后截图: %s", s_LastCapturePath.c_str());

    ImGui::End();
}

} // namespace Engine
