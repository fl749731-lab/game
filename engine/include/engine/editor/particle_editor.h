#pragma once

#include "engine/core/types.h"
#include "engine/renderer/particle.h"

#include <imgui.h>
#include <string>
#include <vector>

namespace Engine {

// ── 粒子编辑器 ──────────────────────────────────────────────
// 功能:
//   1. 实时粒子预览
//   2. 发射器参数调节
//   3. 颜色/大小曲线
//   4. 预设库

struct ParticleEditorConfig {
    // 发射参数
    f32 EmitRate = 50.0f;         // 每秒发射粒子数
    f32 LifetimeMin = 1.0f, LifetimeMax = 3.0f;
    f32 SpeedMin = 1.0f, SpeedMax = 5.0f;

    // 形状
    enum Shape : u8 { Point, Sphere, Cone, Box };
    Shape EmitShape = Point;
    f32 ShapeRadius = 1.0f;
    f32 ConeAngle = 30.0f;

    // 方向
    f32 DirX = 0, DirY = 1, DirZ = 0;
    f32 Spread = 0.3f;  // 随机散布

    // 大小
    f32 StartSize = 0.2f, EndSize = 0.0f;

    // 颜色
    f32 StartR = 1, StartG = 0.8f, StartB = 0.3f, StartA = 1;
    f32 EndR = 1, EndG = 0.2f, EndB = 0.0f, EndA = 0;

    // 力
    f32 GravityY = -9.8f;
    f32 DragCoeff = 0.1f;

    // 渲染
    enum RenderMode : u8 { Billboard, Stretched, Trail };
    RenderMode Mode = Billboard;
    std::string TexturePath;
    bool Additive = true;

    u32 MaxParticles = 1000;
};

class ParticleEditor {
public:
    ParticleEditor();
    ~ParticleEditor();

    void Render(const char* title = "粒子编辑器");

    ParticleEditorConfig& GetConfig() { return m_Config; }
    void SetConfig(const ParticleEditorConfig& cfg) { m_Config = cfg; }

    // 预设
    static const std::vector<std::pair<std::string, ParticleEditorConfig>>& GetPresets();

private:
    void RenderEmitSection();
    void RenderShapeSection();
    void RenderSizeColorSection();
    void RenderForceSection();
    void RenderRenderSection();
    void RenderPresetSection();
    void RenderPreview();

    ParticleEditorConfig m_Config;
    f32 m_PreviewTime = 0;
};

} // namespace Engine

