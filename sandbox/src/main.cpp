// ── Game Engine Sandbox v2.0 ──────────────────────────────
// 数据层 / 渲染层 彻底分离
// 场景搭建 + 输入逻辑 → Sandbox
// 渲染管线 → SceneRenderer (引擎内部)

#include "engine/engine.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

// ── AI 状态颜色 ─────────────────────────────────────────────

static glm::vec3 AIStateColor(const std::string& state) {
    if (state == "Idle")    return {0.5f, 0.5f, 0.5f};
    if (state == "Patrol")  return {0.3f, 0.8f, 0.3f};
    if (state == "Chase")   return {0.9f, 0.7f, 0.1f};
    if (state == "Attack")  return {1.0f, 0.2f, 0.2f};
    if (state == "Flee")    return {0.2f, 0.5f, 1.0f};
    if (state == "Dead")    return {0.1f, 0.1f, 0.1f};
    return {1.0f, 1.0f, 1.0f};
}

// ── 搭建场景 (纯数据) ───────────────────────────────────────

static void BuildDemoScene(Engine::Scene& scene) {
    auto& world = scene.GetWorld();
    world.AddSystem<Engine::TransformSystem>();  // 必须最先注册
    world.AddSystem<Engine::MovementSystem>();
    world.AddSystem<Engine::LifetimeSystem>();

    // 地面
    {
        auto e = scene.CreateEntity("Ground");
        auto& t = world.AddComponent<Engine::TransformComponent>(e);
        t.Y = -0.01f;
        auto& r = world.AddComponent<Engine::RenderComponent>(e);
        r.MeshType = "plane"; r.Shininess = 16;
    }

    // 中央立方体
    Engine::Entity centerCube;
    {
        centerCube = scene.CreateEntity("CenterCube");
        auto& t = world.AddComponent<Engine::TransformComponent>(centerCube);
        t.Y = 0.8f;
        auto& r = world.AddComponent<Engine::RenderComponent>(centerCube);
        r.MeshType = "cube";
        r.ColorR = 0.9f; r.ColorG = 0.35f; r.ColorB = 0.25f;
        r.Shininess = 64;
    }

    // 子实体：环绕 CenterCube 的小球（层级演示）
    {
        auto child = scene.CreateEntity("OrbitChild");
        auto& t = world.AddComponent<Engine::TransformComponent>(child);
        t.X = 2.0f; t.Y = 0.0f; t.Z = 0.0f;
        t.SetScale(0.3f);
        auto& r = world.AddComponent<Engine::RenderComponent>(child);
        r.MeshType = "sphere";
        r.ColorR = 0.3f; r.ColorG = 0.9f; r.ColorB = 0.4f;
        r.Shininess = 64;
        world.SetParent(child, centerCube);
    }

    // 金属球
    {
        auto e = scene.CreateEntity("MetalSphere");
        auto& t = world.AddComponent<Engine::TransformComponent>(e);
        t.X = 3; t.Y = 0.6f; t.Z = -1;
        auto& r = world.AddComponent<Engine::RenderComponent>(e);
        r.MeshType = "sphere";
        r.ColorR = 0.75f; r.ColorG = 0.75f; r.ColorB = 0.8f;
        r.Shininess = 128;
    }

    // AI 机器人
    for (int i = 0; i < 5; i++) {
        float angle = (float)i / 5.0f * 6.283f;
        auto e = scene.CreateEntity("AIBot_" + std::to_string(i));
        auto& t = world.AddComponent<Engine::TransformComponent>(e);
        t.X = 4.0f * cosf(angle); t.Y = 0.4f; t.Z = 4.0f * sinf(angle);
        t.ScaleX = t.ScaleY = t.ScaleZ = 0.5f;
        auto& r = world.AddComponent<Engine::RenderComponent>(e);
        r.MeshType = "cube"; r.Shininess = 32;
        auto& ai = world.AddComponent<Engine::AIComponent>(e);
        ai.ScriptModule = "default_ai";
        auto& h = world.AddComponent<Engine::HealthComponent>(e);
        h.Current = 80.0f + (float)(i * 10);
        world.AddComponent<Engine::VelocityComponent>(e);
    }

    // 柱体
    for (int i = 0; i < 6; i++) {
        float angle = (float)i / 6.0f * 6.283f + 0.5f;
        auto e = scene.CreateEntity("Pillar_" + std::to_string(i));
        auto& t = world.AddComponent<Engine::TransformComponent>(e);
        t.X = 7.0f * cosf(angle); t.Y = 1.2f; t.Z = 7.0f * sinf(angle);
        t.ScaleX = 0.35f; t.ScaleY = 2.4f; t.ScaleZ = 0.35f;
        auto& r = world.AddComponent<Engine::RenderComponent>(e);
        r.MeshType = "cube";
        r.ColorR = 0.55f; r.ColorG = 0.5f; r.ColorB = 0.45f;
        r.Shininess = 16;
    }

    // 方向光 (夜晚月光)
    auto& dirLight = scene.GetDirLight();
    dirLight.Direction = {-0.3f, -1.0f, -0.5f};
    dirLight.Color = {0.4f, 0.35f, 0.5f};
    dirLight.Intensity = 0.6f;

    // 点光源 (纯数据)
    auto& pl0 = scene.AddPointLight();
    pl0.Position = {2, 1.5f, 2}; pl0.Color = {1, 0.3f, 0.3f}; pl0.Intensity = 2.5f;
    auto& pl1 = scene.AddPointLight();
    pl1.Position = {-2, 1.5f, -1}; pl1.Color = {0.3f, 1, 0.3f}; pl1.Intensity = 2.5f;
    auto& pl2 = scene.AddPointLight();
    pl2.Position = {0, 3.0f, 0}; pl2.Color = {0.4f, 0.4f, 1}; pl2.Intensity = 3.0f;

    // 聚光灯 (纯数据)
    auto& sl0 = scene.AddSpotLight();
    sl0.Position  = {3, 6, 3};
    sl0.Direction  = {-0.3f, -1.0f, -0.3f};
    sl0.Color      = {1.0f, 0.95f, 0.8f};
    sl0.Intensity  = 5.0f;
    sl0.InnerCutoff = 10.0f;
    sl0.OuterCutoff = 18.0f;
}

// ── 主程序 ──────────────────────────────────────────────────

int main() {
    Engine::Logger::Init();
    LOG_INFO("=== 游戏引擎 v2.0 — 渲染层分离 ===");

    // ── 窗口 + 核心初始化 ────────────────────────────────────
    Engine::WindowConfig cfg;
    cfg.Title  = "Game Engine v2.0";
    cfg.Width  = 1280;
    cfg.Height = 720;
    Engine::Window window(cfg);
    Engine::Input::Init(window.GetNativeWindow());
    Engine::Renderer::Init();
    Engine::Skybox::Init();
    Engine::ParticleSystem::Init();
    Engine::AudioEngine::Init();
    Engine::SpriteBatch::Init();

    // 天空盒 (夜晚配色)
    Engine::Skybox::SetTopColor(0.02f, 0.02f, 0.12f);
    Engine::Skybox::SetHorizonColor(0.15f, 0.08f, 0.2f);
    Engine::Skybox::SetBottomColor(0.05f, 0.03f, 0.03f);
    Engine::Skybox::SetSunDirection(-0.3f, 0.15f, -0.5f);

    // ── SceneRenderer 初始化（管理 FBO + Shader + 网格 + 纹理 + 后处理 + Bloom）
    Engine::SceneRendererConfig renderCfg;
    renderCfg.Width = cfg.Width;
    renderCfg.Height = cfg.Height;
    Engine::SceneRenderer::Init(renderCfg);

    // ── 调试工具 ────────────────────────────────────────────
    Engine::DebugDraw::Init();
    Engine::DebugUI::Init();

#ifdef ENGINE_HAS_PYTHON
    Engine::AI::PythonEngine::Init("ai/scripts");
    bool aiReady = Engine::AI::PythonEngine::IsInitialized();
#else
    bool aiReady = false;
    LOG_WARN("[AI] Python 未链接，AI 层已禁用");
#endif

    // ── 场景 (纯数据搭建) ───────────────────────────────────
    auto scene = Engine::CreateRef<Engine::Scene>("DemoScene");
    BuildDemoScene(*scene);
    Engine::SceneManager::PushScene(scene);
    Engine::ResourceManager::PrintStats();
    LOG_INFO("[ECS] %u 个实体", scene->GetEntityCount());

    // ── 摄像机 ──────────────────────────────────────────────
    Engine::PerspectiveCamera camera(45, 1280.f/720.f, 0.1f, 100.f);
    camera.SetPosition({0, 4, 14});
    camera.SetRotation(-90, -12);

    Engine::FPSCameraController camCtrl(&camera);
    auto& camCfg = camCtrl.GetConfig();
    camCfg.MoveSpeed = 5; camCfg.LookSpeed = 80; camCfg.MouseSens = 0.15f;

    float aiTimer = 0;
    bool wireframe = false;
    float fpsTimer = 0;
    bool showProfiler = false;

    // ── 事件 ────────────────────────────────────────────────
    Engine::EventDispatcher dispatcher;
    dispatcher.Subscribe<Engine::WindowResizeEvent>([&](Engine::WindowResizeEvent& ev) {
        Engine::Renderer::SetViewport(0, 0, ev.Width, ev.Height);
        Engine::SceneRenderer::Resize(ev.Width, ev.Height);
        camera.SetProjection(camera.GetFOV(), (float)ev.Width/(float)ev.Height, 0.1f, 100.f);
    });

    // ── 粒子发射器配置 ──────────────────────────────────────
    Engine::ParticleEmitterConfig fireEmitter;
    fireEmitter.Position = {0, 0.1f, 0};
    fireEmitter.Direction = {0, 1, 0};
    fireEmitter.SpreadAngle = 25;
    fireEmitter.MinSpeed = 1.0f; fireEmitter.MaxSpeed = 3.5f;
    fireEmitter.MinLife = 0.5f;  fireEmitter.MaxLife = 1.5f;
    fireEmitter.MinSize = 0.04f; fireEmitter.MaxSize = 0.12f;
    fireEmitter.ColorStart = {1, 0.7f, 0.2f};
    fireEmitter.ColorEnd   = {1, 0.1f, 0};
    fireEmitter.EmitRate = 60;

    LOG_INFO("按键: WASD 移动 | F1 线框 | F3/F4 曝光 | F5 调试线 | F6 调试UI | F7 分析器 | F8 Bloom | F9 保存场景 | F10 加载场景 | Esc 退出");

    // 用于检测窗口大小变化
    Engine::u32 lastW = window.GetWidth(), lastH = window.GetHeight();

    // ═══════════════════════════════════════════════════════
    //  主循环：只有 输入 + 逻辑 + SceneRenderer::RenderScene
    // ═══════════════════════════════════════════════════════

    while (!window.ShouldClose()) {
        Engine::Time::Update();
        Engine::Input::Update();
        Engine::Renderer::ResetStats();
        float dt = Engine::Time::DeltaTime();
        float t  = Engine::Time::Elapsed();

        // ── 窗口 Resize 检测 ─────────────────────────────────
        {
            Engine::u32 curW = window.GetWidth(), curH = window.GetHeight();
            if (curW != lastW || curH != lastH) {
                if (curW > 0 && curH > 0) {
                    Engine::SceneRenderer::Resize(curW, curH);
                    camera.SetProjection(camera.GetFOV(), (float)curW / (float)curH,
                                         camera.GetNearClip(), camera.GetFarClip());
                    LOG_INFO("[窗口] 尺寸变更: %ux%u → %ux%u", lastW, lastH, curW, curH);
                }
                lastW = curW;
                lastH = curH;
            }
        }

        // ── 输入处理 ────────────────────────────────────────
        if (Engine::Input::IsKeyJustPressed(Engine::Key::Escape)) {
            if (camCtrl.IsCaptured()) {
                camCtrl.SetCaptured(false);
            } else break;
        }

        // F1 线框
        if (Engine::Input::IsKeyJustPressed(Engine::Key::F1)) {
            wireframe = !wireframe;
            Engine::SceneRenderer::SetWireframe(wireframe);
        }

        // F3/F4 曝光
        if (Engine::Input::IsKeyDown(Engine::Key::F3)) {
            float exp = Engine::SceneRenderer::GetExposure();
            Engine::SceneRenderer::SetExposure(glm::max(exp - dt, 0.1f));
        }
        if (Engine::Input::IsKeyDown(Engine::Key::F4)) {
            float exp = Engine::SceneRenderer::GetExposure();
            Engine::SceneRenderer::SetExposure(glm::min(exp + dt, 5.0f));
        }

        // F5 调试渲染
        if (Engine::Input::IsKeyJustPressed(Engine::Key::F5))
            Engine::DebugDraw::SetEnabled(!Engine::DebugDraw::IsEnabled());

        // F6 调试 UI
        if (Engine::Input::IsKeyJustPressed(Engine::Key::F6))
            Engine::DebugUI::SetEnabled(!Engine::DebugUI::IsEnabled());

        // F7 性能分析器
        if (Engine::Input::IsKeyJustPressed(Engine::Key::F7)) {
            showProfiler = !showProfiler;
            Engine::Profiler::SetEnabled(showProfiler);
        }

        // F8 Bloom
        if (Engine::Input::IsKeyJustPressed(Engine::Key::F8)) {
            Engine::SceneRenderer::SetBloomEnabled(!Engine::SceneRenderer::GetBloomEnabled());
            LOG_INFO("[Bloom] %s", Engine::SceneRenderer::GetBloomEnabled() ? "开启" : "关闭");
        }

        // F9 保存场景
        if (Engine::Input::IsKeyJustPressed(Engine::Key::F9)) {
            if (Engine::SceneSerializer::Save(*scene, "scene.json"))
                LOG_INFO("[Scene] 场景已保存到 scene.json");
        }

        // F10 加载场景
        if (Engine::Input::IsKeyJustPressed(Engine::Key::F10)) {
            auto loaded = Engine::SceneSerializer::Load("scene.json");
            if (loaded) {
                Engine::SceneManager::PopScene();
                scene = loaded;
                Engine::SceneManager::PushScene(scene);
                LOG_INFO("[Scene] 场景已从 scene.json 加载 (%u 个实体)", scene->GetEntityCount());
            }
        }

        // F12 G-Buffer 调试可视化 (循环: 关→位置→法线→漫反射→高光→自发光→关)
        if (Engine::Input::IsKeyJustPressed(Engine::Key::F12)) {
            int mode = Engine::SceneRenderer::GetGBufferDebugMode();
            mode = (mode + 1) % 6;  // 0~5 循环
            Engine::SceneRenderer::SetGBufferDebugMode(mode);
        }


        // ── 摄像机（全部由控制器处理）───────────────────────
        camCtrl.Update(dt);

        // ── 游戏逻辑 ────────────────────────────────────────
        // 点光动画
        auto& pls = scene->GetPointLights();
        if (pls.size() >= 2) {
            pls[0].Position = glm::vec3(5*cosf(t*0.5f), 1.5f, 5*sinf(t*0.5f));
            pls[1].Position = glm::vec3(-4*cosf(t*0.4f), 1.5f+sinf(t*0.8f), -4*sinf(t*0.4f));
        }

        // AI 更新
        auto& world = scene->GetWorld();
        aiTimer += dt;
        if (aiTimer >= 0.5f) {
            aiTimer = 0;
            world.ForEach<Engine::AIComponent>([&](Engine::Entity e, Engine::AIComponent& ai) {
                auto* health = world.GetComponent<Engine::HealthComponent>(e);
                float hp = health ? health->Current : 100.0f;
#ifdef ENGINE_HAS_PYTHON
                if (aiReady) {
                    auto result = Engine::AI::PythonEngine::CallFunction(
                        ai.ScriptModule, "update_ai",
                        {std::to_string(e), ai.State, std::to_string(hp), "0.5"});
                    if (!result.empty()) ai.State = result;
                }
#else
                if (hp <= 0) ai.State = "Dead";
                else if (hp < 20) ai.State = "Flee";
                else if (ai.State == "Idle") ai.State = "Patrol";
#endif
                auto* render = world.GetComponent<Engine::RenderComponent>(e);
                if (render) {
                    glm::vec3 c = AIStateColor(ai.State);
                    render->ColorR = c.r; render->ColorG = c.g; render->ColorB = c.b;
                }
                auto* vel = world.GetComponent<Engine::VelocityComponent>(e);
                if (vel) {
                    if (ai.State == "Patrol") {
                        vel->VX = sinf(t + (float)e) * 0.5f;
                        vel->VZ = cosf(t + (float)e) * 0.5f;
                    } else if (ai.State == "Flee") {
                        vel->VX = sinf(t*2 + (float)e) * 1.5f;
                        vel->VZ = cosf(t*2 + (float)e) * 1.5f;
                    } else { vel->VX = 0; vel->VZ = 0; }
                }
            });
        }

        // 天空盒动画
        Engine::Skybox::SetSunDirection(cosf(t*0.03f)*0.3f, 0.15f+sinf(t*0.02f)*0.1f, -0.5f);

        // ── 物理 + 粒子 ────────────────────────────────────
        Engine::Profiler::BeginTimer("Frame");

        Engine::Profiler::BeginTimer("Physics");
        while (Engine::Time::ConsumeFixedStep()) {
            scene->Update(Engine::Time::FixedDeltaTime());
        }
        Engine::Profiler::EndTimer("Physics");

        Engine::Profiler::BeginTimer("Particles");
        fireEmitter.Position = glm::vec3(2*cosf(t*0.3f), 0.1f, 2*sinf(t*0.3f));
        Engine::ParticleSystem::Emit(fireEmitter, dt);
        Engine::ParticleSystem::Update(dt);
        Engine::Profiler::EndTimer("Particles");

        // ── 调试图形提交 (纯数据) ───────────────────────────
        Engine::DebugDraw::Grid(20.0f, 2.0f, {0.2f, 0.2f, 0.3f});
        Engine::DebugDraw::Axes({0,0,0}, 3.0f);
        Engine::DebugDraw::AABB({-1, 0.5f, -1}, {1, 2.5f, 1}, {1, 0.5f, 0});
        Engine::DebugDraw::Sphere(fireEmitter.Position, 0.3f, {1, 0.6f, 0}, 12);
        for (size_t i = 0; i < pls.size(); i++) {
            Engine::DebugDraw::Cross(pls[i].Position, 0.3f, pls[i].Color);
            Engine::DebugDraw::Circle(pls[i].Position, pls[i].Intensity * 0.5f,
                                     {0,1,0}, pls[i].Color * 0.5f, 16);
        }

        // 碰撞包围盒可视化 (绿色 AABB)
        world.ForEach<Engine::TransformComponent>([&](Engine::Entity e, Engine::TransformComponent& tr) {
            auto* rc = world.GetComponent<Engine::RenderComponent>(e);
            if (!rc || rc->MeshType == "plane") return;
            glm::vec3 mn(tr.X - tr.ScaleX*0.5f, tr.Y - tr.ScaleY*0.5f, tr.Z - tr.ScaleZ*0.5f);
            glm::vec3 mx(tr.X + tr.ScaleX*0.5f, tr.Y + tr.ScaleY*0.5f, tr.Z + tr.ScaleZ*0.5f);
            Engine::DebugDraw::AABB(mn, mx, {0.2f, 0.8f, 0.2f});
        });

        // ═════════════════════════════════════════════════════
        //  ★ 一行渲染：SceneRenderer 处理一切
        // ═════════════════════════════════════════════════════
        Engine::SceneRenderer::RenderScene(*scene, camera);

        // ── 2D SpriteBatch 渲染 (在 3D 之上) ────────────────
        Engine::SpriteBatch::Begin(window.GetWidth(), window.GetHeight());
        // 半透明 HUD 背景面板
        Engine::SpriteBatch::DrawRect({10, 10}, {220, 50}, {0.0f, 0.0f, 0.0f, 0.5f});
        // 小彩色方块作为指示灯
        Engine::SpriteBatch::DrawRect({20, 20}, {12, 12}, {0.2f, 1.0f, 0.4f, 1.0f});
        Engine::SpriteBatch::DrawRect({40, 20}, {12, 12}, {1.0f, 0.8f, 0.2f, 1.0f});
        Engine::SpriteBatch::DrawRect({60, 20}, {12, 12}, {1.0f, 0.3f, 0.3f, 1.0f});
        // 进度条背景 + 前景
        Engine::SpriteBatch::DrawRect({20, 40}, {190, 10}, {0.3f, 0.3f, 0.3f, 0.8f});
        float hpPct = 0.75f; // 示例血条 75%
        Engine::SpriteBatch::DrawRect({20, 40}, {190 * hpPct, 10}, {0.2f, 0.9f, 0.3f, 0.9f});
        Engine::SpriteBatch::End();

        // ── 调试 UI 叠加 (2D，在 SceneRenderer 之后) ────────
        {
            auto stats = Engine::Renderer::GetStats();
            Engine::DebugUI::DrawStatsPanel(
                Engine::Time::FPS(), stats.DrawCalls, stats.TriangleCount,
                Engine::ParticleSystem::GetAliveCount(),
                scene->GetEntityCount(),
                Engine::DebugDraw::GetLineCount(),
                Engine::SceneRenderer::GetExposure(), camera.GetFOV());

            if (showProfiler) {
                auto& pf = Engine::Profiler::GetLastFrameStats();
                float py = 170;
                Engine::DebugUI::Printf(10, py, {0.6f,1,0.6f}, "=== Profiler ===");
                py += 16;
                for (auto& t : pf.Timers) {
                    Engine::DebugUI::Printf(10, py, {0.8f,0.9f,1},
                        "%-12s %.2f ms (avg: %.2f)",
                        t.Name.c_str(), t.DurationMs,
                        Engine::Profiler::GetAverageMs(t.Name, 60));
                    py += 16;
                }
            }

            Engine::DebugUI::Flush(window.GetWidth(), window.GetHeight());
        }

        // ── 窗口标题 FPS / 渲染统计 (在渲染之后更新) ─────────
        fpsTimer += dt;
        if (fpsTimer >= 0.5f) {
            fpsTimer = 0;
            auto stats = Engine::Renderer::GetStats();
            char title[128];
            snprintf(title, sizeof(title),
                "Engine v2.1 | FPS: %.0f | Draw: %u | Tri: %u | Part: %u | Exp: %.1f%s",
                Engine::Time::FPS(), stats.DrawCalls, stats.TriangleCount,
                Engine::ParticleSystem::GetAliveCount(),
                Engine::SceneRenderer::GetExposure(),
                Engine::SceneRenderer::GetBloomEnabled() ? " | Bloom:ON" : "");
            window.SetTitle(title);
        }
        Engine::Profiler::EndFrame();

        window.Update();
        Engine::Input::EndFrame();
    }

    // ── 清理 ────────────────────────────────────────────────
#ifdef ENGINE_HAS_PYTHON
    Engine::AI::PythonEngine::Shutdown();
#endif
    Engine::DebugUI::Shutdown();
    Engine::DebugDraw::Shutdown();
    Engine::SpriteBatch::Shutdown();
    Engine::ParticleSystem::Shutdown();
    Engine::AudioEngine::Shutdown();
    Engine::Skybox::Shutdown();
    Engine::SceneRenderer::Shutdown();   // 内部清理 FBO + Shader + Bloom + PostProcess
    Engine::SceneManager::Clear();
    Engine::ResourceManager::Clear();
    Engine::Renderer::Shutdown();
    LOG_INFO("引擎正常退出 | 总帧数: %llu", Engine::Time::FrameCount());
    return 0;
}
