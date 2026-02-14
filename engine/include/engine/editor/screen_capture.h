#pragma once

#include "engine/core/types.h"

#include <string>
#include <vector>
#include <functional>

namespace Engine {

// ── 截图系统 ────────────────────────────────────────────────
// 功能:
//   1. 视口截图 (读取帧缓冲)
//   2. 保存为 PNG (通过 stb_image_write)
//   3. 序列帧录制 (每 N 帧截一张)
//   4. 截图通知

class ScreenCapture {
public:
    static void Init();
    static void Shutdown();

    /// 截取当前帧缓冲（在帧结束时调用）
    static bool CaptureFrame(const std::string& filename,
                              u32 width, u32 height, u32 fbo = 0);

    /// 截图并自动命名 (screenshots/screenshot_YYYYMMDD_HHMMSS.png)
    static std::string CaptureAutoNamed(u32 width, u32 height, u32 fbo = 0);

    /// 序列帧录制
    static void StartRecording(const std::string& folder, u32 width, u32 height,
                                u32 captureInterval = 1, u32 fbo = 0);
    static void StopRecording();
    static bool IsRecording() { return s_Recording; }
    static u32  GetFramesCaptured() { return s_FramesCaptured; }

    /// 每帧调用（录制模式下自动截图）
    static void Update();

    /// 截图完成回调
    using CaptureCallback = std::function<void(const std::string& path)>;
    static void SetCallback(CaptureCallback cb) { s_Callback = cb; }

    /// 渲染截图工具面板
    static void RenderPanel();

private:
    inline static bool s_Recording = false;
    inline static std::string s_RecordFolder;
    inline static u32 s_RecordWidth = 0, s_RecordHeight = 0;
    inline static u32 s_RecordFBO = 0;
    inline static u32 s_CaptureInterval = 1;
    inline static u32 s_FrameCount = 0;
    inline static u32 s_FramesCaptured = 0;
    inline static CaptureCallback s_Callback;
    inline static std::string s_LastCapturePath;
};

} // namespace Engine
