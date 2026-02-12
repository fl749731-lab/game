#include "engine/renderer/scene_renderer.h"
#include "engine/renderer/shaders.h"
#include "engine/renderer/renderer.h"
#include "engine/renderer/post_process.h"
#include "engine/renderer/bloom.h"
#include "engine/renderer/skybox.h"
#include "engine/renderer/particle.h"
#include "engine/renderer/texture.h"
#include "engine/core/resource_manager.h"
#include "engine/core/log.h"
#include "engine/core/time.h"
#include "engine/debug/debug_draw.h"
#include "engine/debug/debug_ui.h"
#include "engine/debug/profiler.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <array>

// ── 预缓存点光 Uniform 名称（避免每帧 string 分配）───────
struct PLUniformNames {
    std::string Pos, Color, Intensity;
};
static std::array<PLUniformNames, Engine::MAX_POINT_LIGHTS> s_PLUniforms;

namespace Engine {

// ── 静态成员定义 ────────────────────────────────────────────

Scope<Framebuffer> SceneRenderer::s_HDR_FBO = nullptr;
Ref<Shader>   SceneRenderer::s_LitShader = nullptr;
Ref<Shader>   SceneRenderer::s_EmissiveShader = nullptr;
u32  SceneRenderer::s_CheckerTexID = 0;
u32  SceneRenderer::s_Width = 0;
u32  SceneRenderer::s_Height = 0;
f32  SceneRenderer::s_Exposure = 1.2f;
bool SceneRenderer::s_BloomEnabled = true;

// ── 初始化 ──────────────────────────────────────────────────

void SceneRenderer::Init(const SceneRendererConfig& config) {
    s_Width  = config.Width;
    s_Height = config.Height;
    s_Exposure = config.Exposure;
    s_BloomEnabled = config.BloomEnabled;

    // HDR FBO
    FramebufferSpec fboSpec;
    fboSpec.Width  = config.Width;
    fboSpec.Height = config.Height;
    fboSpec.HDR = true;
    s_HDR_FBO = std::make_unique<Framebuffer>(fboSpec);

    // 内置 Shader
    s_LitShader = ResourceManager::LoadShader("lit",
        Shaders::LitVertex, Shaders::LitFragment);
    s_EmissiveShader = ResourceManager::LoadShader("emissive",
        Shaders::EmissiveVertex, Shaders::EmissiveFragment);

    // 基础网格（如果还没创建）
    if (!ResourceManager::GetMesh("cube"))
        ResourceManager::StoreMesh("cube", Mesh::CreateCube());
    if (!ResourceManager::GetMesh("plane"))
        ResourceManager::StoreMesh("plane", Mesh::CreatePlane(24.0f, 12.0f));
    if (!ResourceManager::GetMesh("sphere"))
        ResourceManager::StoreMesh("sphere", Mesh::CreateSphere(32, 32));

    // 棋盘纹理
    const int ts = 256;
    std::vector<u8> ck(ts * ts * 4);
    for (int y = 0; y < ts; y++) {
        for (int x = 0; x < ts; x++) {
            int i = (y * ts + x) * 4;
            bool w = ((x / 32) + (y / 32)) % 2 == 0;
            u8 v = w ? 160 : 40;
            ck[i] = v; ck[i+1] = v + 10; ck[i+2] = v; ck[i+3] = 255;
        }
    }
    glGenTextures(1, &s_CheckerTexID);
    glBindTexture(GL_TEXTURE_2D, s_CheckerTexID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ts, ts, 0, GL_RGBA, GL_UNSIGNED_BYTE, ck.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);

    // 预缓存点光 Uniform 名称
    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        std::string idx = std::to_string(i);
        s_PLUniforms[i].Pos       = "uPLPos[" + idx + "]";
        s_PLUniforms[i].Color     = "uPLColor[" + idx + "]";
        s_PLUniforms[i].Intensity = "uPLIntensity[" + idx + "]";
    }

    // 后处理 + Bloom
    PostProcess::Init();
    PostProcess::SetExposure(s_Exposure);
    Bloom::Init(config.Width, config.Height);

    LOG_INFO("[SceneRenderer] 初始化完成 (%ux%u)", s_Width, s_Height);
}

void SceneRenderer::Shutdown() {
    if (s_CheckerTexID) { glDeleteTextures(1, &s_CheckerTexID); s_CheckerTexID = 0; }
    s_HDR_FBO.reset();
    s_LitShader.reset();
    s_EmissiveShader.reset();
    Bloom::Shutdown();
    PostProcess::Shutdown();
    LOG_DEBUG("[SceneRenderer] 已清理");
}

void SceneRenderer::Resize(u32 width, u32 height) {
    s_Width  = width;
    s_Height = height;
    if (s_HDR_FBO) s_HDR_FBO->Resize(width, height);
    Bloom::Resize(width, height);
}

// ── 核心渲染方法 ────────────────────────────────────────────

void SceneRenderer::RenderScene(Scene& scene, PerspectiveCamera& camera) {
    Profiler::BeginTimer("Render");

    // ── Pass 1: 离屏 HDR FBO ────────────────────────────────
    s_HDR_FBO->Bind();
    Renderer::SetClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    Renderer::Clear();

    // 天空盒
    glm::mat4 skyVP = camera.GetProjectionMatrix() *
        glm::mat4(glm::mat3(camera.GetViewMatrix()));
    Skybox::Draw(glm::value_ptr(skyVP));

    // 场景实体
    RenderEntities(scene, camera);

    // 光源可视化
    RenderLightGizmos(scene, camera);

    // 粒子
    ParticleSystem::Draw(
        glm::value_ptr(camera.GetViewProjectionMatrix()),
        camera.GetRight(), camera.GetUp());

    // 调试线框 (3D 空间)
    DebugDraw::Flush(glm::value_ptr(camera.GetViewProjectionMatrix()));

    Profiler::EndTimer("Render");

    // ── Pass 2: Bloom + 后处理 ──────────────────────────────
    s_HDR_FBO->Unbind();
    Renderer::SetViewport(0, 0, s_Width, s_Height);
    Renderer::Clear();

    if (s_BloomEnabled) {
        u32 bloomTex = Bloom::Process(s_HDR_FBO->GetColorAttachmentID());
        PostProcess::Draw(s_HDR_FBO->GetColorAttachmentID(),
                          bloomTex, Bloom::GetIntensity());
    } else {
        PostProcess::Draw(s_HDR_FBO->GetColorAttachmentID());
    }
}

// ── 实体渲染 ────────────────────────────────────────────────

void SceneRenderer::RenderEntities(Scene& scene, PerspectiveCamera& camera) {
    auto& world = scene.GetWorld();
    auto& pls = scene.GetPointLights();
    auto& dirLight = scene.GetDirLight();
    float t = Time::Elapsed();

    s_LitShader->Bind();
    s_LitShader->SetMat4("uVP", glm::value_ptr(camera.GetViewProjectionMatrix()));
    s_LitShader->SetVec3("uDirLightDir", dirLight.Direction.x, dirLight.Direction.y, dirLight.Direction.z);
    s_LitShader->SetVec3("uDirLightColor", dirLight.Color.x * dirLight.Intensity,
                          dirLight.Color.y * dirLight.Intensity, dirLight.Color.z * dirLight.Intensity);
    glm::vec3 cp = camera.GetPosition();
    s_LitShader->SetVec3("uViewPos", cp.x, cp.y, cp.z);

    // 点光源 Uniform
    s_LitShader->SetInt("uPLCount", (int)pls.size());
    for (int i = 0; i < (int)pls.size() && i < MAX_POINT_LIGHTS; i++) {
        s_LitShader->SetVec3(s_PLUniforms[i].Pos, pls[i].Position.x, pls[i].Position.y, pls[i].Position.z);
        s_LitShader->SetVec3(s_PLUniforms[i].Color, pls[i].Color.x, pls[i].Color.y, pls[i].Color.z);
        s_LitShader->SetFloat(s_PLUniforms[i].Intensity, pls[i].Intensity);
    }

    // 遍历实体
    for (auto e : world.GetEntities()) {
        auto* tr = world.GetComponent<TransformComponent>(e);
        auto* rc = world.GetComponent<RenderComponent>(e);
        if (!tr || !rc) continue;

        // Model 矩阵
        glm::mat4 model = glm::translate(glm::mat4(1.0f), {tr->X, tr->Y, tr->Z});
        model = glm::rotate(model, glm::radians(tr->RotY), {0, 1, 0});
        model = glm::rotate(model, glm::radians(tr->RotX), {1, 0, 0});
        model = glm::scale(model, {tr->ScaleX, tr->ScaleY, tr->ScaleZ});

        // 旋转动画（如果 TagComponent 指定）
        auto* tag = world.GetComponent<TagComponent>(e);
        if (tag && tag->Name == "CenterCube") {
            model = glm::translate(glm::mat4(1.0f), {tr->X, tr->Y, tr->Z});
            model = glm::rotate(model, t * 0.6f, {0, 1, 0});
            model = glm::rotate(model, t * 0.2f, {1, 0, 0});
        }

        s_LitShader->SetMat4("uModel", glm::value_ptr(model));

        // 材质：优先使用 MaterialComponent，回退到 RenderComponent 旧字段
        auto* mat = world.GetComponent<MaterialComponent>(e);
        if (mat) {
            s_LitShader->SetVec3("uMatDiffuse", mat->DiffuseR, mat->DiffuseG, mat->DiffuseB);
            s_LitShader->SetVec3("uMatSpecular", mat->SpecularR, mat->SpecularG, mat->SpecularB);
            s_LitShader->SetFloat("uShininess", mat->Shininess);

            // 纹理
            if (!mat->TextureName.empty()) {
                s_LitShader->SetInt("uUseTex", 1);
                auto tex = ResourceManager::GetTexture(mat->TextureName);
                if (tex) { tex->Bind(0); s_LitShader->SetInt("uTex", 0); }
            } else {
                s_LitShader->SetInt("uUseTex", 0);
            }
        } else {
            // 旧兼容路径
            s_LitShader->SetVec3("uMatDiffuse", rc->ColorR, rc->ColorG, rc->ColorB);
            s_LitShader->SetVec3("uMatSpecular", 0.8f, 0.8f, 0.8f);
            s_LitShader->SetFloat("uShininess", rc->Shininess);

            if (rc->MeshType == "plane") {
                s_LitShader->SetInt("uUseTex", 1);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, s_CheckerTexID);
                s_LitShader->SetInt("uTex", 0);
            } else {
                s_LitShader->SetInt("uUseTex", 0);
            }
        }

        auto* mesh = ResourceManager::GetMesh(rc->MeshType);
        if (mesh) mesh->Draw();
    }
}

// ── 光源可视化 ──────────────────────────────────────────────

void SceneRenderer::RenderLightGizmos(Scene& scene, PerspectiveCamera& camera) {
    auto& pls = scene.GetPointLights();
    if (pls.empty()) return;

    s_EmissiveShader->Bind();
    s_EmissiveShader->SetMat4("uVP", glm::value_ptr(camera.GetViewProjectionMatrix()));
    auto* cubeMesh = ResourceManager::GetMesh("cube");
    if (!cubeMesh) return;

    for (auto& pl : pls) {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), pl.Position);
        m = glm::scale(m, glm::vec3(0.12f));
        s_EmissiveShader->SetMat4("uModel", glm::value_ptr(m));
        s_EmissiveShader->SetVec3("uColor",
            pl.Color.x * pl.Intensity,
            pl.Color.y * pl.Intensity,
            pl.Color.z * pl.Intensity);
        cubeMesh->Draw();
    }
}

// ── 参数控制 ────────────────────────────────────────────────

void SceneRenderer::SetExposure(f32 exposure) {
    s_Exposure = exposure;
    PostProcess::SetExposure(exposure);
}

f32 SceneRenderer::GetExposure() { return s_Exposure; }

void SceneRenderer::SetBloomEnabled(bool enabled) {
    s_BloomEnabled = enabled;
    Bloom::SetEnabled(enabled);
}

bool SceneRenderer::GetBloomEnabled() { return s_BloomEnabled; }

void SceneRenderer::SetWireframe(bool enabled) {
    Renderer::SetWireframe(enabled);
}

u32 SceneRenderer::GetHDRColorAttachment() {
    return s_HDR_FBO ? s_HDR_FBO->GetColorAttachmentID() : 0;
}

} // namespace Engine
