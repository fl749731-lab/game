// ── Game Engine Sandbox v3.0 ──────────────────────────────
// Application + Layer 架构
// SandboxLayer: 场景搭建 + 输入逻辑 + 渲染
// Application: 窗口管理 + 子系统生命周期 + 主循环

#include "engine/engine.h"
#include "game_layer.h"

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
    world.AddSystem<Engine::TransformSystem>();
    world.AddSystem<Engine::MovementSystem>();
    world.AddSystem<Engine::LifetimeSystem>();
    world.AddSystem<Engine::ScriptSystem>();

    // 游戏管理器
    {
        auto gm = scene.CreateEntity("GameManager");
        auto& sc = world.AddComponent<Engine::ScriptComponent>(gm);
        sc.ScriptModule = "game_manager";
    }

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

    // 子实体：环绕 CenterCube 的小球
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

    // 方向光
    auto& dirLight = scene.GetDirLight();
    dirLight.Direction = {-0.3f, -1.0f, -0.5f};
    dirLight.Color = {1.0f, 0.95f, 0.9f};
    dirLight.Intensity = 2.0f;

    // 点光源
    auto& pl0 = scene.AddPointLight();
    pl0.Position = {2, 1.5f, 2}; pl0.Color = {1, 0.3f, 0.3f}; pl0.Intensity = 2.5f;
    auto& pl1 = scene.AddPointLight();
    pl1.Position = {-2, 1.5f, -1}; pl1.Color = {0.3f, 1, 0.3f}; pl1.Intensity = 2.5f;
    auto& pl2 = scene.AddPointLight();
    pl2.Position = {0, 3.0f, 0}; pl2.Color = {0.4f, 0.4f, 1}; pl2.Intensity = 3.0f;

    // 聚光灯
    auto& sl0 = scene.AddSpotLight();
    sl0.Position  = {3, 6, 3};
    sl0.Direction  = {-0.3f, -1.0f, -0.3f};
    sl0.Color      = {1.0f, 0.95f, 0.8f};
    sl0.Intensity  = 5.0f;
    sl0.InnerCutoff = 10.0f;
    sl0.OuterCutoff = 18.0f;
}

// ════════════════════════════════════════════════════════════
//  SandboxLayer — 游戏逻辑 + 渲染
// ════════════════════════════════════════════════════════════

class SandboxLayer : public Engine::Layer {
public:
    const char* GetName() const override { return "Sandbox"; }

    // ── OnAttach ────────────────────────────────────────────
    void OnAttach() override {
        auto& window = Engine::Application::Get().GetWindow();

        // 天空盒
        Engine::Skybox::SetTopColor(0.2f, 0.4f, 0.8f);
        Engine::Skybox::SetHorizonColor(0.8f, 0.65f, 0.5f);
        Engine::Skybox::SetBottomColor(0.25f, 0.2f, 0.15f);

        // Editor
        Engine::Editor::Init(window.GetNativeWindow());
        Engine::Profiler::SetEnabled(false);

        // Python AI (可选)
#ifdef ENGINE_HAS_PYTHON
        Engine::AI::PythonEngine::Init();
        if (Engine::AI::PythonEngine::LoadModule("default_ai") &&
            Engine::AI::PythonEngine::LoadModule("game_manager")) {
            m_AIReady = true;
            LOG_INFO("[AI] Python 模块加载成功");
        }
#else
        LOG_WARN("[AI] Python 未链接，AI 层已禁用");
#endif

        // 场景
        m_Scene = Engine::CreateRef<Engine::Scene>("DemoScene");
        BuildDemoScene(*m_Scene);
        Engine::SceneManager::PushScene(m_Scene);
        Engine::ResourceManager::PrintStats();
        LOG_INFO("[ECS] %u 个实体", m_Scene->GetEntityCount());

        // 摄像机
        m_Camera.SetProjection(45, (float)window.GetWidth() / (float)window.GetHeight(), 0.1f, 100.f);
        m_Camera.SetPosition({0, 4, 14});
        m_Camera.SetRotation(-90, -12);
        auto& camCfg = m_CamCtrl.GetConfig();
        camCfg.MoveSpeed = 5; camCfg.LookSpeed = 80; camCfg.MouseSens = 0.15f;

        // 粒子发射器
        m_FireEmitter.Position = {0, 0.1f, 0};
        m_FireEmitter.Direction = {0, 1, 0};
        m_FireEmitter.SpreadAngle = 25;
        m_FireEmitter.MinSpeed = 1.0f; m_FireEmitter.MaxSpeed = 3.5f;
        m_FireEmitter.MinLife = 0.5f;  m_FireEmitter.MaxLife = 1.5f;
        m_FireEmitter.MinSize = 0.04f; m_FireEmitter.MaxSize = 0.12f;
        m_FireEmitter.ColorStart = {1, 0.7f, 0.2f};
        m_FireEmitter.ColorEnd   = {1, 0.1f, 0};
        m_FireEmitter.EmitRate = 60;

        LOG_INFO("按键: WASD 移动 | F1 线框 | F3/F4 曝光 | F5 调试线 | F6 调试UI | F7 分析器 | F8 Bloom | F9 保存场景 | F10 加载场景 | Esc 退出");
    }

    // ── OnDetach ────────────────────────────────────────────
    void OnDetach() override {
        Engine::Editor::Shutdown();
#ifdef ENGINE_HAS_PYTHON
        Engine::AI::PythonEngine::Shutdown();
#endif
    }

    // ── OnUpdate ────────────────────────────────────────────
    void OnUpdate(Engine::f32 dt) override {
        float t = Engine::Time::Elapsed();
        auto& window = Engine::Application::Get().GetWindow();

        // 窗口 Resize 检测
        {
            Engine::u32 curW = window.GetWidth(), curH = window.GetHeight();
            if (curW != m_LastW || curH != m_LastH) {
                if (curW > 0 && curH > 0) {
                    Engine::SceneRenderer::Resize(curW, curH);
                    m_Camera.SetProjection(m_Camera.GetFOV(), (float)curW / (float)curH,
                                           m_Camera.GetNearClip(), m_Camera.GetFarClip());
                    LOG_INFO("[窗口] 尺寸变更: %ux%u → %ux%u", m_LastW, m_LastH, curW, curH);
                }
                m_LastW = curW;
                m_LastH = curH;
            }
        }

        // 输入处理
        if (Engine::Input::IsKeyJustPressed(Engine::Key::Escape)) {
            if (m_CamCtrl.IsCaptured()) {
                m_CamCtrl.SetCaptured(false);
            } else {
                Engine::Application::Get().Close();
            }
        }

        if (Engine::Input::IsKeyJustPressed(Engine::Key::F1)) {
            m_Wireframe = !m_Wireframe;
            Engine::SceneRenderer::SetWireframe(m_Wireframe);
            LOG_INFO("[Input] F1 pressed -> wireframe=%d", m_Wireframe);
        }
        if (Engine::Input::IsKeyDown(Engine::Key::F3)) {
            float exp = Engine::SceneRenderer::GetExposure();
            Engine::SceneRenderer::SetExposure(glm::max(exp - dt, 0.1f));
        }
        if (Engine::Input::IsKeyDown(Engine::Key::F4)) {
            float exp = Engine::SceneRenderer::GetExposure();
            Engine::SceneRenderer::SetExposure(glm::min(exp + dt, 5.0f));
        }
        if (Engine::Input::IsKeyJustPressed(Engine::Key::F5)) {
            Engine::DebugDraw::SetEnabled(!Engine::DebugDraw::IsEnabled());
            LOG_INFO("[Input] F5 pressed -> DebugDraw=%d", Engine::DebugDraw::IsEnabled());
        }
        if (Engine::Input::IsKeyJustPressed(Engine::Key::F6)) {
            Engine::DebugUI::SetEnabled(!Engine::DebugUI::IsEnabled());
            LOG_INFO("[Input] F6 pressed -> DebugUI=%d", Engine::DebugUI::IsEnabled());
        }
        if (Engine::Input::IsKeyJustPressed(Engine::Key::F7)) {
            m_ShowProfiler = !m_ShowProfiler;
            Engine::Profiler::SetEnabled(m_ShowProfiler);
            LOG_INFO("[Input] F7 pressed -> Profiler=%d", m_ShowProfiler);
        }
        if (Engine::Input::IsKeyJustPressed(Engine::Key::F8)) {
            Engine::SceneRenderer::SetBloomEnabled(!Engine::SceneRenderer::GetBloomEnabled());
            LOG_INFO("[Bloom] %s", Engine::SceneRenderer::GetBloomEnabled() ? "开启" : "关闭");
        }
        if (Engine::Input::IsKeyJustPressed(Engine::Key::F9)) {
            if (Engine::SceneSerializer::Save(*m_Scene, "scene.json"))
                LOG_INFO("[Scene] 场景已保存到 scene.json");
        }
        if (Engine::Input::IsKeyJustPressed(Engine::Key::F10)) {
            auto loaded = Engine::SceneSerializer::Load("scene.json");
            if (loaded) {
                Engine::SceneManager::PopScene();
                m_Scene = loaded;
                Engine::SceneManager::PushScene(m_Scene);
                LOG_INFO("[Scene] 场景已从 scene.json 加载 (%u 个实体)", m_Scene->GetEntityCount());
            }
        }
        if (Engine::Input::IsKeyJustPressed(Engine::Key::F12)) {
            int mode = Engine::SceneRenderer::GetGBufferDebugMode();
            mode = (mode + 1) % 6;
            Engine::SceneRenderer::SetGBufferDebugMode(mode);
            LOG_INFO("[Input] F12 pressed -> GBufDebug=%d", mode);
        }
        if (Engine::Input::IsKeyJustPressed(Engine::Key::F2)) {
            Engine::Editor::Toggle();
            LOG_INFO("[Input] F2 pressed -> Editor=%d", Engine::Editor::IsEnabled());
        }
        if (Engine::Input::IsKeyJustPressed(Engine::Key::F11)) {
            Engine::Editor::Toggle();
            LOG_INFO("[Input] F11 pressed -> Editor=%d", Engine::Editor::IsEnabled());
        }

        // 摄像机
        m_CamCtrl.Update(dt);

        // 游戏逻辑
        auto& pls = m_Scene->GetPointLights();
        if (pls.size() >= 2) {
            pls[0].Position = glm::vec3(5*cosf(t*0.5f), 1.5f, 5*sinf(t*0.5f));
            pls[1].Position = glm::vec3(-4*cosf(t*0.4f), 1.5f+sinf(t*0.8f), -4*sinf(t*0.4f));
        }

        // AI 更新
        auto& world = m_Scene->GetWorld();
        m_AITimer += dt;
        if (m_AITimer >= 0.5f) {
            m_AITimer = 0;
            world.ForEach<Engine::AIComponent>([&](Engine::Entity e, Engine::AIComponent& ai) {
                auto* health = world.GetComponent<Engine::HealthComponent>(e);
                float hp = health ? health->Current : 100.0f;
#ifdef ENGINE_HAS_PYTHON
                if (m_AIReady) {
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

        // 物理 + 粒子
        Engine::Profiler::BeginTimer("Physics");
        while (Engine::Time::ConsumeFixedStep()) {
            m_Scene->Update(Engine::Time::FixedDeltaTime());
        }
        Engine::Profiler::EndTimer("Physics");

        Engine::Profiler::BeginTimer("Particles");
        m_FireEmitter.Position = glm::vec3(2*cosf(t*0.3f), 0.1f, 2*sinf(t*0.3f));
        Engine::ParticleSystem::Emit(m_FireEmitter, dt);
        Engine::ParticleSystem::Update(dt);
        Engine::Profiler::EndTimer("Particles");

        // 调试图形
        Engine::DebugDraw::Grid(20.0f, 2.0f, {0.2f, 0.2f, 0.3f});
        Engine::DebugDraw::Axes({0,0,0}, 3.0f);
        Engine::DebugDraw::AABB({-1, 0.5f, -1}, {1, 2.5f, 1}, {1, 0.5f, 0});
        Engine::DebugDraw::Sphere(m_FireEmitter.Position, 0.3f, {1, 0.6f, 0}, 12);
        for (size_t i = 0; i < pls.size(); i++) {
            Engine::DebugDraw::Cross(pls[i].Position, 0.3f, pls[i].Color);
            Engine::DebugDraw::Circle(pls[i].Position, pls[i].Intensity * 0.5f,
                                     {0,1,0}, pls[i].Color * 0.5f, 16);
        }

        // 碰撞包围盒可视化
        world.ForEach<Engine::TransformComponent>([&](Engine::Entity e, Engine::TransformComponent& tr) {
            auto* rc = world.GetComponent<Engine::RenderComponent>(e);
            if (!rc || rc->MeshType == "plane") return;
            glm::vec3 mn(tr.X - tr.ScaleX*0.5f, tr.Y - tr.ScaleY*0.5f, tr.Z - tr.ScaleZ*0.5f);
            glm::vec3 mx(tr.X + tr.ScaleX*0.5f, tr.Y + tr.ScaleY*0.5f, tr.Z + tr.ScaleZ*0.5f);
            Engine::DebugDraw::AABB(mn, mx, {0.2f, 0.8f, 0.2f});
        });
    }

    // ── OnRender ────────────────────────────────────────────
    void OnRender() override {
        auto& window = Engine::Application::Get().GetWindow();

        // 3D 场景渲染
        Engine::SceneRenderer::RenderScene(*m_Scene, m_Camera);

        // 2D SpriteBatch
        Engine::SpriteBatch::Begin(window.GetWidth(), window.GetHeight());
        Engine::SpriteBatch::DrawRect({10, 10}, {220, 50}, {0.0f, 0.0f, 0.0f, 0.5f});
        Engine::SpriteBatch::DrawRect({20, 20}, {12, 12}, {0.2f, 1.0f, 0.4f, 1.0f});
        Engine::SpriteBatch::DrawRect({40, 20}, {12, 12}, {1.0f, 0.8f, 0.2f, 1.0f});
        Engine::SpriteBatch::DrawRect({60, 20}, {12, 12}, {1.0f, 0.3f, 0.3f, 1.0f});
        Engine::SpriteBatch::DrawRect({20, 40}, {190, 10}, {0.3f, 0.3f, 0.3f, 0.8f});
        float hpPct = 0.75f;
        Engine::SpriteBatch::DrawRect({20, 40}, {190 * hpPct, 10}, {0.2f, 0.9f, 0.3f, 0.9f});
        Engine::SpriteBatch::End();

        // 调试 UI 叠加
        {
            auto stats = Engine::Renderer::GetStats();
            Engine::DebugUI::DrawStatsPanel(
                Engine::Time::FPS(), stats.DrawCalls, stats.TriangleCount,
                Engine::ParticleSystem::GetAliveCount(),
                m_Scene->GetEntityCount(),
                Engine::DebugDraw::GetLineCount(),
                Engine::SceneRenderer::GetExposure(), m_Camera.GetFOV());

            if (m_ShowProfiler) {
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

        // 窗口标题
        m_FPSTimer += Engine::Time::DeltaTime();
        if (m_FPSTimer >= 0.5f) {
            m_FPSTimer = 0;
            auto stats = Engine::Renderer::GetStats();
            char title[128];
            snprintf(title, sizeof(title),
                "Engine v3.0 | FPS: %.0f | Draw: %u | Tri: %u | Part: %u | Exp: %.1f%s",
                Engine::Time::FPS(), stats.DrawCalls, stats.TriangleCount,
                Engine::ParticleSystem::GetAliveCount(),
                Engine::SceneRenderer::GetExposure(),
                Engine::SceneRenderer::GetBloomEnabled() ? " | Bloom:ON" : "");
            window.SetTitle(title);
        }
    }

    // ── OnImGui ─────────────────────────────────────────────
    void OnImGui() override {
        Engine::Editor::BeginFrame();
        Engine::Editor::Render(*m_Scene, m_SelectedEntity);
        Engine::Editor::EndFrame();
    }

private:
    // 场景
    Engine::Ref<Engine::Scene> m_Scene;
    Engine::Entity m_SelectedEntity = Engine::INVALID_ENTITY;

    // 摄像机
    Engine::PerspectiveCamera m_Camera{45, 1280.f/720.f, 0.1f, 100.f};
    Engine::FPSCameraController m_CamCtrl{&m_Camera};

    // 粒子
    Engine::ParticleEmitterConfig m_FireEmitter;

    // 状态
    bool m_Wireframe = false;
    bool m_ShowProfiler = false;
    bool m_AIReady = false;
    float m_AITimer = 0;
    float m_FPSTimer = 0;
    Engine::u32 m_LastW = 1280, m_LastH = 720;
};

// ════════════════════════════════════════════════════════════
//  入口点 — 只要 5 行
// ════════════════════════════════════════════════════════════

int main() {
    Engine::Application app({
        .Title = "Zombie Survival",
        .Width = 1280,
        .Height = 720
    });
    // 默认启动丧尸生存原型
    app.PushLayer(Engine::CreateScope<Engine::GameLayer>());
    app.Run();
    return 0;
}
