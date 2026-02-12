// ── Game Engine Sandbox v1.0 ──────────────────────────────
// 新增: Bloom 后处理

#include "engine/engine.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <vector>

// ── 着色器源码 ──────────────────────────────────────────────

static const char* litVertSrc = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

out vec3 vFragPos;
out vec3 vNormal;
out vec2 vTexCoord;

uniform mat4 uVP;
uniform mat4 uModel;

void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    vFragPos = wp.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vTexCoord = aTexCoord;
    gl_Position = uVP * wp;
}
)";

static const char* litFragSrc = R"(
#version 450 core
in vec3 vFragPos;
in vec3 vNormal;
in vec2 vTexCoord;
out vec4 FragColor;

uniform vec3 uMatDiffuse;
uniform vec3 uMatSpecular;
uniform float uShininess;
uniform vec3 uDirLightDir;
uniform vec3 uDirLightColor;

#define MAX_PL 4
uniform int uPLCount;
uniform vec3 uPLPos[MAX_PL];
uniform vec3 uPLColor[MAX_PL];
uniform float uPLIntensity[MAX_PL];

uniform vec3 uViewPos;
uniform int uUseTex;
uniform sampler2D uTex;

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uViewPos - vFragPos);
    vec3 base = uMatDiffuse;
    if (uUseTex == 1) base = texture(uTex, vTexCoord).rgb;

    vec3 L = normalize(-uDirLightDir);
    float diff = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), uShininess);
    vec3 result = (0.15 * base + diff * base + spec * uMatSpecular) * uDirLightColor * 0.6;

    for (int i = 0; i < uPLCount; i++) {
        vec3 pL = normalize(uPLPos[i] - vFragPos);
        float d = length(uPLPos[i] - vFragPos);
        float att = 1.0 / (1.0 + 0.09*d + 0.032*d*d);
        float pDiff = max(dot(N, pL), 0.0);
        vec3 pH = normalize(pL + V);
        float pSpec = pow(max(dot(N, pH), 0.0), uShininess);
        result += (pDiff * base + pSpec * uMatSpecular) * uPLColor[i] * uPLIntensity[i] * att;
    }

    FragColor = vec4(result, 1.0);
}
)";

static const char* emitVertSrc = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
uniform mat4 uVP;
uniform mat4 uModel;
void main() { gl_Position = uVP * uModel * vec4(aPos, 1.0); }
)";

static const char* emitFragSrc = R"(
#version 450 core
out vec4 FragColor;
uniform vec3 uColor;
void main() { FragColor = vec4(uColor, 1.0); }
)";

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

// ── 搭建场景 ────────────────────────────────────────────────

static void BuildDemoScene(Engine::Scene& scene) {
    auto& world = scene.GetWorld();
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
    {
        auto e = scene.CreateEntity("CenterCube");
        auto& t = world.AddComponent<Engine::TransformComponent>(e);
        t.Y = 0.8f;
        auto& r = world.AddComponent<Engine::RenderComponent>(e);
        r.MeshType = "cube";
        r.ColorR = 0.9f; r.ColorG = 0.35f; r.ColorB = 0.25f;
        r.Shininess = 64;
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

    // 光源
    auto& pl0 = scene.AddPointLight();
    pl0.Position = {2, 1.5f, 2}; pl0.Color = {1, 0.3f, 0.3f}; pl0.Intensity = 2.5f;
    auto& pl1 = scene.AddPointLight();
    pl1.Position = {-2, 1.5f, -1}; pl1.Color = {0.3f, 1, 0.3f}; pl1.Intensity = 2.5f;
    auto& pl2 = scene.AddPointLight();
    pl2.Position = {0, 3.0f, 0}; pl2.Color = {0.4f, 0.4f, 1}; pl2.Intensity = 3.0f;
}

// ── 主程序 ──────────────────────────────────────────────────

int main() {
    Engine::Logger::Init();
    LOG_INFO("=== 游戏引擎 v1.0 — HDR + Bloom ===");

    // ── 窗口 ────────────────────────────────────────────────
    Engine::WindowConfig cfg;
    cfg.Title  = "Game Engine v0.9 - HDR + Particles";
    cfg.Width  = 1280;
    cfg.Height = 720;
    Engine::Window window(cfg);
    Engine::Input::Init(window.GetNativeWindow());
    Engine::Renderer::Init();
    Engine::PostProcess::Init();
    Engine::Skybox::Init();
    Engine::ParticleSystem::Init();
    Engine::Bloom::Init(cfg.Width, cfg.Height);

    // 天空盒 (夜晚配色)
    Engine::Skybox::SetTopColor(0.02f, 0.02f, 0.12f);
    Engine::Skybox::SetHorizonColor(0.15f, 0.08f, 0.2f);
    Engine::Skybox::SetBottomColor(0.05f, 0.03f, 0.03f);
    Engine::Skybox::SetSunDirection(-0.3f, 0.15f, -0.5f);

    // ── 调试工具初始化 ────────────────────────────────────────
    Engine::DebugDraw::Init();
    Engine::DebugUI::Init();

#ifdef ENGINE_HAS_PYTHON
    Engine::AI::PythonEngine::Init("ai/scripts");
    bool aiReady = Engine::AI::PythonEngine::IsInitialized();
#else
    bool aiReady = false;
    LOG_WARN("[AI] Python 未链接，AI 层已禁用");
#endif

    // ── 资源加载 ────────────────────────────────────────────
    auto litShader  = Engine::ResourceManager::LoadShader("lit", litVertSrc, litFragSrc);
    auto emitShader = Engine::ResourceManager::LoadShader("emissive", emitVertSrc, emitFragSrc);

    Engine::ResourceManager::StoreMesh("cube", Engine::Mesh::CreateCube());
    Engine::ResourceManager::StoreMesh("plane", Engine::Mesh::CreatePlane(24.0f, 12.0f));
    Engine::ResourceManager::StoreMesh("sphere", Engine::Mesh::CreateSphere(32, 32));

    // 棋盘纹理
    const int ts = 256;
    std::vector<Engine::u8> ck(ts*ts*4);
    for (int y=0;y<ts;y++) for (int x=0;x<ts;x++) {
        int i=(y*ts+x)*4;
        bool w=((x/32)+(y/32))%2==0;
        Engine::u8 v=w?160:40;
        ck[i]=v; ck[i+1]=v+10; ck[i+2]=v; ck[i+3]=255;
    }
    Engine::Texture2D checkerTex(ts, ts, ck.data());

    // ── HDR FBO 离屏渲染 ────────────────────────────────────
    Engine::FramebufferSpec fboSpec;
    fboSpec.Width = cfg.Width;
    fboSpec.Height = cfg.Height;
    fboSpec.HDR = true;  // v0.9: HDR 渲染
    Engine::Framebuffer sceneFBO(fboSpec);

    // ── 场景 ────────────────────────────────────────────────
    auto scene = Engine::CreateRef<Engine::Scene>("DemoScene");
    BuildDemoScene(*scene);
    Engine::SceneManager::PushScene(scene);
    Engine::ResourceManager::PrintStats();
    LOG_INFO("[ECS] %u 个实体", scene->GetEntityCount());

    // ── 摄像机 ──────────────────────────────────────────────
    Engine::PerspectiveCamera camera(45, 1280.f/720.f, 0.1f, 100.f);
    camera.SetPosition({0, 4, 14});
    camera.SetRotation(-90, -12);
    float spd = 5, lspd = 80, mouseSens = 0.15f;
    float aiTimer = 0;
    bool wireframe = false;
    bool captured = false;
    float exposure = 1.2f;
    float fpsTimer = 0;
    bool showDebugDraw = true;
    bool showDebugUI  = true;
    bool showProfiler = false;
    bool showBloom = true;

    // ── 事件 ────────────────────────────────────────────────
    Engine::EventDispatcher dispatcher;
    dispatcher.Subscribe<Engine::WindowResizeEvent>([&](Engine::WindowResizeEvent& ev) {
        Engine::Renderer::SetViewport(0, 0, ev.Width, ev.Height);
        sceneFBO.Resize(ev.Width, ev.Height);
        Engine::Bloom::Resize(ev.Width, ev.Height);
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

    Engine::PostProcess::SetExposure(exposure);

    LOG_INFO("按键: WASD 移动 | F1 线框 | F2 捕获 | F3/F4 曝光 | F5 调试线 | F6 调试UI | F7 分析器 | F8 Bloom | Z/X Zoom | Esc 退出");

    while (!window.ShouldClose()) {
        Engine::Time::Update();
        Engine::Input::Update();
        Engine::Renderer::ResetStats();
        float dt = Engine::Time::DeltaTime();
        float t  = Engine::Time::Elapsed();

        if (Engine::Input::IsKeyPressed(Engine::Key::Escape)) {
            if (captured) {
                Engine::Input::SetCursorMode(Engine::CursorMode::Normal);
                captured = false;
            } else break;
        }

        // F1 线框
        static bool f1Last = false;
        bool f1Now = Engine::Input::IsKeyPressed(Engine::Key::F1);
        if (f1Now && !f1Last) {
            wireframe = !wireframe;
            Engine::Renderer::SetWireframe(wireframe);
        }
        f1Last = f1Now;

        // F2 鼠标捕获
        static bool f2Last = false;
        bool f2Now = Engine::Input::IsKeyPressed(Engine::Key::F2);
        if (f2Now && !f2Last) {
            captured = !captured;
            Engine::Input::SetCursorMode(captured ? Engine::CursorMode::Captured : Engine::CursorMode::Normal);
        }
        f2Last = f2Now;

        // F3/F4 曝光
        if (Engine::Input::IsKeyDown(Engine::Key::F3)) {
            exposure = glm::max(exposure - dt, 0.1f);
            Engine::PostProcess::SetExposure(exposure);
        }
        if (Engine::Input::IsKeyDown(Engine::Key::F4)) {
            exposure = glm::min(exposure + dt, 5.0f);
            Engine::PostProcess::SetExposure(exposure);
        }

        // F5 调试渲染
        static bool f5Last = false;
        bool f5Now = Engine::Input::IsKeyPressed(Engine::Key::F5);
        if (f5Now && !f5Last) {
            showDebugDraw = !showDebugDraw;
            Engine::DebugDraw::SetEnabled(showDebugDraw);
            LOG_INFO("[调试渲染] %s", showDebugDraw ? "开启" : "关闭");
        }
        f5Last = f5Now;

        // F6 调试 UI
        static bool f6Last = false;
        bool f6Now = Engine::Input::IsKeyPressed(Engine::Key::F6);
        if (f6Now && !f6Last) {
            showDebugUI = !showDebugUI;
            Engine::DebugUI::SetEnabled(showDebugUI);
            LOG_INFO("[调试UI] %s", showDebugUI ? "开启" : "关闭");
        }
        f6Last = f6Now;

        // F7 性能分析器日志报告
        static bool f7Last = false;
        bool f7Now = Engine::Input::IsKeyPressed(Engine::Key::F7);
        if (f7Now && !f7Last) {
            showProfiler = !showProfiler;
            Engine::Profiler::SetEnabled(showProfiler);
            LOG_INFO("[分析器] %s", showProfiler ? "开启" : "关闭");
        }
        f7Last = f7Now;

        // Z/X FOV Zoom
        if (Engine::Input::IsKeyDown(Engine::Key::Z)) camera.Zoom(dt * 30);
        if (Engine::Input::IsKeyDown(Engine::Key::X)) camera.Zoom(-dt * 30);

        // 滚轮 FOV
        float scroll = Engine::Input::GetScrollOffset();
        if (scroll != 0) camera.Zoom(scroll);

        // 窗口标题 FPS
        fpsTimer += dt;
        if (fpsTimer >= 0.5f) {
            fpsTimer = 0;
            auto stats = Engine::Renderer::GetStats();
            char title[128];
            snprintf(title, sizeof(title),
                "Engine v1.0 HDR+Bloom | FPS: %.0f | Draw: %u | Tri: %u | Part: %u | Exp: %.1f%s",
                Engine::Time::FPS(), stats.DrawCalls, stats.TriangleCount,
                Engine::ParticleSystem::GetAliveCount(), exposure,
                showBloom ? " | Bloom:ON" : "");
            window.SetTitle(title);
        }

        // 摄像机
        glm::vec3 p = camera.GetPosition();
        if (Engine::Input::IsKeyDown(Engine::Key::W)) p += camera.GetForward()*spd*dt;
        if (Engine::Input::IsKeyDown(Engine::Key::S)) p -= camera.GetForward()*spd*dt;
        if (Engine::Input::IsKeyDown(Engine::Key::A)) p -= camera.GetRight()*spd*dt;
        if (Engine::Input::IsKeyDown(Engine::Key::D)) p += camera.GetRight()*spd*dt;
        if (Engine::Input::IsKeyDown(Engine::Key::Space)) p.y += spd*dt;
        if (Engine::Input::IsKeyDown(Engine::Key::LeftShift)) p.y -= spd*dt;
        camera.SetPosition(p);

        float yaw = camera.GetYaw(), pitch = camera.GetPitch();
        if (Engine::Input::IsKeyDown(Engine::Key::Left))  yaw -= lspd*dt;
        if (Engine::Input::IsKeyDown(Engine::Key::Right)) yaw += lspd*dt;
        if (Engine::Input::IsKeyDown(Engine::Key::Up))    pitch += lspd*dt;
        if (Engine::Input::IsKeyDown(Engine::Key::Down))  pitch -= lspd*dt;
        if (captured) {
            yaw   += Engine::Input::GetMouseDeltaX() * mouseSens;
            pitch += Engine::Input::GetMouseDeltaY() * mouseSens;
        }
        camera.SetRotation(yaw, glm::clamp(pitch, -89.f, 89.f));

        // 点光动画
        auto& pls = scene->GetPointLights();
        if (pls.size() >= 2) {
            pls[0].Position = glm::vec3(5*cosf(t*0.5f), 1.5f, 5*sinf(t*0.5f));
            pls[1].Position = glm::vec3(-4*cosf(t*0.4f), 1.5f+sinf(t*0.8f), -4*sinf(t*0.4f));
        }

        // AI
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

        // 帧性能计时开始
        Engine::Profiler::BeginTimer("Frame");

        // 固定步长物理
        Engine::Profiler::BeginTimer("Physics");
        while (Engine::Time::ConsumeFixedStep()) {
            scene->Update(Engine::Time::FixedDeltaTime());
        }
        Engine::Profiler::EndTimer("Physics");

        // 粒子: 火焰效果绕中心旋转
        Engine::Profiler::BeginTimer("Particles");
        fireEmitter.Position = glm::vec3(2*cosf(t*0.3f), 0.1f, 2*sinf(t*0.3f));
        Engine::ParticleSystem::Emit(fireEmitter, dt);
        Engine::ParticleSystem::Update(dt);
        Engine::Profiler::EndTimer("Particles");

        // ── 调试图形提交 ─────────────────────────────────────
        // 地面参考网格
        Engine::DebugDraw::Grid(20.0f, 2.0f, {0.2f, 0.2f, 0.3f});
        // 世界坐标轴
        Engine::DebugDraw::Axes({0,0,0}, 3.0f);
        // 中心立方体包围盒
        Engine::DebugDraw::AABB({-1, 0.5f, -1}, {1, 2.5f, 1}, {1, 0.5f, 0});
        // 粒子发射器球
        Engine::DebugDraw::Sphere(fireEmitter.Position, 0.3f, {1, 0.6f, 0}, 12);
        // 点光源标记
        auto& plsDbg = scene->GetPointLights();
        for (size_t i = 0; i < plsDbg.size(); i++) {
            Engine::DebugDraw::Cross(plsDbg[i].Position, 0.3f, plsDbg[i].Color);
            Engine::DebugDraw::Circle(plsDbg[i].Position, plsDbg[i].Intensity * 0.5f,
                                     {0,1,0}, plsDbg[i].Color * 0.5f, 16);
        }

        // ── 渲染 Pass 1: 离屏 HDR FBO ──────────────────────
        Engine::Profiler::BeginTimer("Render");
        sceneFBO.Bind();
        Engine::Renderer::SetClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        Engine::Renderer::Clear();

        // 天空盒
        glm::mat4 skyVP = camera.GetProjectionMatrix() *
            glm::mat4(glm::mat3(camera.GetViewMatrix()));
        Engine::Skybox::Draw(glm::value_ptr(skyVP));
        Engine::Skybox::SetSunDirection(cosf(t*0.03f)*0.3f, 0.15f+sinf(t*0.02f)*0.1f, -0.5f);

        // 场景渲染
        litShader->Bind();
        litShader->SetMat4("uVP", glm::value_ptr(camera.GetViewProjectionMatrix()));
        litShader->SetVec3("uDirLightDir", -0.3f, -1.0f, -0.5f);
        litShader->SetVec3("uDirLightColor", 0.4f, 0.35f, 0.5f);  // 夜晚月光
        glm::vec3 cp = camera.GetPosition();
        litShader->SetVec3("uViewPos", cp.x, cp.y, cp.z);
        litShader->SetInt("uPLCount", (int)pls.size());
        for (int i=0; i<(int)pls.size(); i++) {
            auto& pl = pls[i];
            litShader->SetVec3("uPLPos["+std::to_string(i)+"]", pl.Position.x, pl.Position.y, pl.Position.z);
            litShader->SetVec3("uPLColor["+std::to_string(i)+"]", pl.Color.x, pl.Color.y, pl.Color.z);
            litShader->SetFloat("uPLIntensity["+std::to_string(i)+"]", pl.Intensity);
        }

        for (auto e : world.GetEntities()) {
            auto* tr = world.GetComponent<Engine::TransformComponent>(e);
            auto* rc = world.GetComponent<Engine::RenderComponent>(e);
            if (!tr || !rc) continue;

            glm::mat4 model = glm::translate(glm::mat4(1.0f), {tr->X, tr->Y, tr->Z});
            model = glm::rotate(model, glm::radians(tr->RotY), {0,1,0});
            model = glm::rotate(model, glm::radians(tr->RotX), {1,0,0});
            model = glm::scale(model, {tr->ScaleX, tr->ScaleY, tr->ScaleZ});

            auto* tag = world.GetComponent<Engine::TagComponent>(e);
            if (tag && tag->Name == "CenterCube") {
                model = glm::translate(glm::mat4(1.0f), {tr->X, tr->Y, tr->Z});
                model = glm::rotate(model, t*0.6f, {0,1,0});
                model = glm::rotate(model, t*0.2f, {1,0,0});
            }

            litShader->SetMat4("uModel", glm::value_ptr(model));
            litShader->SetVec3("uMatDiffuse", rc->ColorR, rc->ColorG, rc->ColorB);
            litShader->SetVec3("uMatSpecular", 0.8f, 0.8f, 0.8f);
            litShader->SetFloat("uShininess", rc->Shininess);

            if (rc->MeshType == "plane") {
                litShader->SetInt("uUseTex", 1);
                checkerTex.Bind(0);
                litShader->SetInt("uTex", 0);
            } else {
                litShader->SetInt("uUseTex", 0);
            }

            auto* mesh = Engine::ResourceManager::GetMesh(rc->MeshType);
            if (mesh) mesh->Draw();
        }

        // 光源方块
        emitShader->Bind();
        emitShader->SetMat4("uVP", glm::value_ptr(camera.GetViewProjectionMatrix()));
        auto* cubeMesh = Engine::ResourceManager::GetMesh("cube");
        for (auto& pl : pls) {
            glm::mat4 m = glm::translate(glm::mat4(1.0f), pl.Position);
            m = glm::scale(m, glm::vec3(0.12f));
            emitShader->SetMat4("uModel", glm::value_ptr(m));
            emitShader->SetVec3("uColor", pl.Color.x * pl.Intensity, pl.Color.y * pl.Intensity, pl.Color.z * pl.Intensity);
            if (cubeMesh) cubeMesh->Draw();
        }

        // 粒子渲染
        Engine::ParticleSystem::Draw(
            glm::value_ptr(camera.GetViewProjectionMatrix()),
            camera.GetRight(), camera.GetUp());

        // 调试线框渲染 (3D 空间，在 FBO 内)
        Engine::DebugDraw::Flush(glm::value_ptr(camera.GetViewProjectionMatrix()));

        Engine::Profiler::EndTimer("Render");

        // ── 渲染 Pass 2: 后处理 ─────────────────────────────
        sceneFBO.Unbind();
        Engine::Renderer::SetViewport(0, 0, window.GetWidth(), window.GetHeight());
        Engine::Renderer::Clear();
        Engine::PostProcess::Draw(sceneFBO.GetColorAttachmentID());

        // ── 渲染 Pass 3: 调试 UI 叠加 ───────────────────────
        {
            auto stats = Engine::Renderer::GetStats();
            Engine::DebugUI::DrawStatsPanel(
                Engine::Time::FPS(), stats.DrawCalls, stats.TriangleCount,
                Engine::ParticleSystem::GetAliveCount(),
                scene->GetEntityCount(),
                Engine::DebugDraw::GetLineCount(),
                exposure, camera.GetFOV());

            // Profiler 信息
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

        Engine::Profiler::EndTimer("Frame");
        Engine::Profiler::EndFrame();

        window.Update();
        Engine::Input::EndFrame();
    }

#ifdef ENGINE_HAS_PYTHON
    Engine::AI::PythonEngine::Shutdown();
#endif
    Engine::DebugUI::Shutdown();
    Engine::DebugDraw::Shutdown();
    Engine::Bloom::Shutdown();
    Engine::ParticleSystem::Shutdown();
    Engine::Skybox::Shutdown();
    Engine::PostProcess::Shutdown();
    Engine::SceneManager::Clear();
    Engine::ResourceManager::Clear();
    Engine::Renderer::Shutdown();
    LOG_INFO("引擎正常退出 | 总帧数: %llu", Engine::Time::FrameCount());
    return 0;
}
