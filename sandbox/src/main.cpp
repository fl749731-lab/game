// ── Game Engine Sandbox v0.5 ────────────────────────────────
// 集成演示: ECS + Mesh + Blinn-Phong + Python AI + Time/Input

#include "engine/engine.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <vector>

// ── 着色器 ──────────────────────────────────────────────────

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

    result = result / (result + vec3(1.0));
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

// ── AI 状态 → 颜色 ─────────────────────────────────────────

static glm::vec3 AIStateColor(const std::string& state) {
    if (state == "Idle")    return {0.5f, 0.5f, 0.5f};
    if (state == "Patrol")  return {0.3f, 0.8f, 0.3f};
    if (state == "Chase")   return {0.9f, 0.7f, 0.1f};
    if (state == "Attack")  return {1.0f, 0.2f, 0.2f};
    if (state == "Flee")    return {0.2f, 0.5f, 1.0f};
    if (state == "Dead")    return {0.1f, 0.1f, 0.1f};
    return {1.0f, 1.0f, 1.0f};
}

// ── 主程序 ──────────────────────────────────────────────────

int main() {
    Engine::Logger::Init();
    LOG_INFO("=== 游戏引擎 v0.5.0 — 优化版 ===");

    Engine::WindowConfig cfg;
    cfg.Title  = "Game Engine v0.5 - Optimized";
    cfg.Width  = 1280;
    cfg.Height = 720;
    Engine::Window window(cfg);
    Engine::Input::Init(window.GetNativeWindow());
    Engine::Renderer::Init();

#ifdef ENGINE_HAS_PYTHON
    Engine::AI::PythonEngine::Init("ai/scripts");
    bool aiReady = Engine::AI::PythonEngine::IsInitialized();
#else
    bool aiReady = false;
    LOG_WARN("[AI] Python 未链接，AI 层已禁用");
#endif

    // ── 网格 ────────────────────────────────────────────────
    auto cubeMesh   = Engine::Mesh::CreateCube();
    auto planeMesh  = Engine::Mesh::CreatePlane(16.0f, 8.0f);
    auto sphereMesh = Engine::Mesh::CreateSphere(24, 24);

    // ── 棋盘纹理 ────────────────────────────────────────────
    const int ts = 256;
    std::vector<Engine::u8> ck(ts*ts*4);
    for (int y=0;y<ts;y++) for (int x=0;x<ts;x++) {
        int i=(y*ts+x)*4;
        bool w=((x/32)+(y/32))%2==0;
        Engine::u8 v=w?180:50;
        ck[i]=v; ck[i+1]=v+15; ck[i+2]=v; ck[i+3]=255;
    }
    Engine::Texture2D checkerTex(ts, ts, ck.data());

    // ── 着色器 ──────────────────────────────────────────────
    Engine::Shader litShader(litVertSrc, litFragSrc);
    Engine::Shader emitShader(emitVertSrc, emitFragSrc);

    // ── ECS ─────────────────────────────────────────────────
    Engine::ECSWorld world;
    world.AddSystem<Engine::MovementSystem>();

    // 地面
    {
        auto e = world.CreateEntity("Ground");
        auto& t = world.AddComponent<Engine::TransformComponent>(e);
        t.Y = 0;
        auto& r = world.AddComponent<Engine::RenderComponent>(e);
        r.MeshType = "plane"; r.Shininess = 16;
    }

    // 中央立方体
    {
        auto e = world.CreateEntity("CenterCube");
        auto& t = world.AddComponent<Engine::TransformComponent>(e);
        t.Y = 0.8f;
        auto& r = world.AddComponent<Engine::RenderComponent>(e);
        r.MeshType = "cube";
        r.ColorR = 0.9f; r.ColorG = 0.35f; r.ColorB = 0.25f;
        r.Shininess = 64;
    }

    // 金属球
    {
        auto e = world.CreateEntity("MetalSphere");
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
        auto e = world.CreateEntity("AIBot_" + std::to_string(i));
        auto& t = world.AddComponent<Engine::TransformComponent>(e);
        t.X = 4.0f * cos(angle); t.Y = 0.4f; t.Z = 4.0f * sin(angle);
        t.ScaleX = t.ScaleY = t.ScaleZ = 0.5f;
        auto& r = world.AddComponent<Engine::RenderComponent>(e);
        r.MeshType = "cube"; r.Shininess = 32;
        auto& ai = world.AddComponent<Engine::AIComponent>(e);
        ai.ScriptModule = "default_ai";
        auto& h = world.AddComponent<Engine::HealthComponent>(e);
        h.Current = 80.0f + (float)(i * 10);
        world.AddComponent<Engine::VelocityComponent>(e);
    }
    LOG_INFO("[ECS] %u 个实体", world.GetEntityCount());

    // ── 光源 ─────────────────────────────────────────────────
    struct PLData { glm::vec3 pos; glm::vec3 color; float intensity; };
    std::vector<PLData> pointLights = {
        {{2,1.5f,2},   {1,0.3f,0.3f}, 2.0f},
        {{-2,1.5f,-1}, {0.3f,1,0.3f}, 2.0f},
        {{0,2.5f,0},   {0.4f,0.4f,1}, 2.5f},
    };

    // ── 摄像机 ──────────────────────────────────────────────
    Engine::PerspectiveCamera camera(45, 1280.f/720.f, 0.1f, 100.f);
    camera.SetPosition({0, 4, 10});
    camera.SetRotation(-90, -20);
    float spd = 5, lspd = 80;
    float aiTimer = 0;

    LOG_INFO("主循环开始 (WASD 移动, 方向键旋转, Space/Shift 升降, 滚轮变速)");

    while (!window.ShouldClose()) {
        Engine::Time::Update();
        Engine::Input::Update();
        float dt = Engine::Time::DeltaTime();
        float t  = Engine::Time::Elapsed();

        if (Engine::Input::IsKeyPressed(Engine::Key::Escape)) break;

        // 每秒打印一次 FPS
        if (Engine::Time::FrameCount() % 120 == 0) {
            LOG_DEBUG("FPS: %.0f | DT: %.2fms | Entities: %u",
                Engine::Time::FPS(), dt * 1000.0f, world.GetEntityCount());
        }

        // 滚轮控制速度
        float scroll = Engine::Input::GetScrollOffset();
        if (scroll != 0) spd = glm::clamp(spd + scroll * 0.5f, 1.0f, 50.0f);

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
        camera.SetRotation(yaw, glm::clamp(pitch, -89.f, 89.f));

        // 点光源动画
        pointLights[0].pos = glm::vec3(4*cos(t*0.7f), 1.5f, 4*sin(t*0.7f));
        pointLights[1].pos = glm::vec3(-3*cos(t*0.5f), 1.5f+sin(t), -3*sin(t*0.5f));

        // AI 更新 (每 0.5 秒)
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
                        vel->VX = sin(t + (float)e) * 0.5f;
                        vel->VZ = cos(t + (float)e) * 0.5f;
                    } else if (ai.State == "Flee") {
                        vel->VX = sin(t*2 + (float)e) * 1.5f;
                        vel->VZ = cos(t*2 + (float)e) * 1.5f;
                    } else { vel->VX = 0; vel->VZ = 0; }
                }
            });
        }
        world.Update(dt);

        // ── 渲染 ────────────────────────────────────────────
        Engine::Renderer::SetClearColor(0.02f, 0.02f, 0.05f, 1.0f);
        Engine::Renderer::Clear();

        litShader.Bind();
        litShader.SetMat4("uVP", glm::value_ptr(camera.GetViewProjectionMatrix()));
        litShader.SetVec3("uDirLightDir", -0.3f, -1.0f, -0.5f);
        litShader.SetVec3("uDirLightColor", 1.0f, 0.95f, 0.85f);
        glm::vec3 cp = camera.GetPosition();
        litShader.SetVec3("uViewPos", cp.x, cp.y, cp.z);
        litShader.SetInt("uPLCount", (int)pointLights.size());
        for (int i=0; i<(int)pointLights.size(); i++) {
            auto& pl = pointLights[i];
            litShader.SetVec3("uPLPos["+std::to_string(i)+"]", pl.pos.x, pl.pos.y, pl.pos.z);
            litShader.SetVec3("uPLColor["+std::to_string(i)+"]", pl.color.x, pl.color.y, pl.color.z);
            litShader.SetFloat("uPLIntensity["+std::to_string(i)+"]", pl.intensity);
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

            litShader.SetMat4("uModel", glm::value_ptr(model));
            litShader.SetVec3("uMatDiffuse", rc->ColorR, rc->ColorG, rc->ColorB);
            litShader.SetVec3("uMatSpecular", 0.8f, 0.8f, 0.8f);
            litShader.SetFloat("uShininess", rc->Shininess);

            if (rc->MeshType == "plane") {
                litShader.SetInt("uUseTex", 1);
                checkerTex.Bind(0);
                litShader.SetInt("uTex", 0);
                planeMesh->Draw();
            } else {
                litShader.SetInt("uUseTex", 0);
                if (rc->MeshType == "sphere") sphereMesh->Draw();
                else cubeMesh->Draw();
            }
        }

        // 光源可视化
        emitShader.Bind();
        emitShader.SetMat4("uVP", glm::value_ptr(camera.GetViewProjectionMatrix()));
        for (auto& pl : pointLights) {
            glm::mat4 m = glm::translate(glm::mat4(1.0f), pl.pos);
            m = glm::scale(m, glm::vec3(0.12f));
            emitShader.SetMat4("uModel", glm::value_ptr(m));
            emitShader.SetVec3("uColor", pl.color.x, pl.color.y, pl.color.z);
            cubeMesh->Draw();
        }

        window.Update();
    }

#ifdef ENGINE_HAS_PYTHON
    Engine::AI::PythonEngine::Shutdown();
#endif
    Engine::Renderer::Shutdown();
    LOG_INFO("引擎正常退出 | 总帧数: %llu", Engine::Time::FrameCount());
    return 0;
}
