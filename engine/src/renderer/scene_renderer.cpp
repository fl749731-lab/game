#include "engine/renderer/scene_renderer.h"
#include "engine/renderer/shaders.h"
#include "engine/renderer/renderer.h"
#include "engine/renderer/post_process.h"
#include "engine/renderer/bloom.h"
#include "engine/renderer/skybox.h"
#include "engine/renderer/particle.h"
#include "engine/renderer/texture.h"
#include "engine/renderer/shadow_map.h"
#include "engine/renderer/frustum.h"
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
    std::string Constant, Linear, Quadratic;
};
static std::array<PLUniformNames, Engine::MAX_POINT_LIGHTS> s_PLUniforms;

// ── 预缓存聚光灯 Uniform 名称 ─────────────────────
struct SLUniformNames {
    std::string Pos, Dir, Color, Intensity;
    std::string InnerCut, OuterCut;
    std::string Constant, Linear, Quadratic;
};
static std::array<SLUniformNames, Engine::MAX_SPOT_LIGHTS> s_SLUniforms;

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
        s_PLUniforms[i].Constant  = "uPLConstant[" + idx + "]";
        s_PLUniforms[i].Linear    = "uPLLinear[" + idx + "]";
        s_PLUniforms[i].Quadratic = "uPLQuadratic[" + idx + "]";
    }

    // 预缓存聚光灯 Uniform 名称
    for (int i = 0; i < MAX_SPOT_LIGHTS; i++) {
        std::string idx = std::to_string(i);
        s_SLUniforms[i].Pos       = "uSLPos[" + idx + "]";
        s_SLUniforms[i].Dir       = "uSLDir[" + idx + "]";
        s_SLUniforms[i].Color     = "uSLColor[" + idx + "]";
        s_SLUniforms[i].Intensity = "uSLIntensity[" + idx + "]";
        s_SLUniforms[i].InnerCut  = "uSLInnerCut[" + idx + "]";
        s_SLUniforms[i].OuterCut  = "uSLOuterCut[" + idx + "]";
        s_SLUniforms[i].Constant  = "uSLConstant[" + idx + "]";
        s_SLUniforms[i].Linear    = "uSLLinear[" + idx + "]";
        s_SLUniforms[i].Quadratic = "uSLQuadratic[" + idx + "]";
    }

    // 后处理 + Bloom
    PostProcess::Init();
    PostProcess::SetExposure(s_Exposure);
    Bloom::Init(config.Width, config.Height);

    // 阴影
    ShadowMap::Init();

    LOG_INFO("[SceneRenderer] 初始化完成 (%ux%u)", s_Width, s_Height);
}

void SceneRenderer::Shutdown() {
    if (s_CheckerTexID) { glDeleteTextures(1, &s_CheckerTexID); s_CheckerTexID = 0; }
    s_HDR_FBO.reset();
    s_LitShader.reset();
    s_EmissiveShader.reset();
    Bloom::Shutdown();
    ShadowMap::Shutdown();
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

    // ── Pass 0: 阴影深度 ──────────────────────────────────
    {
        auto& dirLight = scene.GetDirLight();
        ShadowMap::BeginShadowPass(dirLight);
        auto depthShader = ShadowMap::GetDepthShader();
        auto& world = scene.GetWorld();

        for (auto e : world.GetEntities()) {
            auto* tr = world.GetComponent<TransformComponent>(e);
            auto* rc = world.GetComponent<RenderComponent>(e);
            if (!tr || !rc) continue;

            glm::mat4 model = glm::translate(glm::mat4(1.0f), {tr->X, tr->Y, tr->Z});
            model = glm::rotate(model, glm::radians(tr->RotY), {0, 1, 0});
            model = glm::rotate(model, glm::radians(tr->RotX), {1, 0, 0});
            model = glm::rotate(model, glm::radians(tr->RotZ), {0, 0, 1});
            model = glm::scale(model, {tr->ScaleX, tr->ScaleY, tr->ScaleZ});

            depthShader->SetMat4("uModel", glm::value_ptr(model));
            auto* mesh = ResourceManager::GetMesh(rc->MeshType);
            if (mesh) mesh->Draw();
        }
        ShadowMap::EndShadowPass();
    }

    // ── Pass 1: 离屏 HDR FBO ────────────────────────────────
    s_HDR_FBO->Bind();
    Renderer::SetViewport(0, 0, s_Width, s_Height);  // 恢复 viewport（Shadow Pass 改了）
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
    s_LitShader->SetFloat("uAmbientStrength", 0.15f);

    // 阴影 Uniform
    s_LitShader->SetMat4("uLightSpaceMat", glm::value_ptr(ShadowMap::GetLightSpaceMatrix()));
    s_LitShader->SetInt("uShadowEnabled", 1);
    s_LitShader->SetInt("uShadowMap", 5);  // 绽定到纹理单元 5
    glActiveTexture(GL_TEXTURE0 + 5);
    glBindTexture(GL_TEXTURE_2D, ShadowMap::GetShadowTextureID());

    // 点光源 Uniform
    int plCount = std::min((int)pls.size(), (int)MAX_POINT_LIGHTS);
    s_LitShader->SetInt("uPLCount", plCount);
    for (int i = 0; i < (int)pls.size() && i < MAX_POINT_LIGHTS; i++) {
        s_LitShader->SetVec3(s_PLUniforms[i].Pos, pls[i].Position.x, pls[i].Position.y, pls[i].Position.z);
        s_LitShader->SetVec3(s_PLUniforms[i].Color, pls[i].Color.x, pls[i].Color.y, pls[i].Color.z);
        s_LitShader->SetFloat(s_PLUniforms[i].Intensity, pls[i].Intensity);
        s_LitShader->SetFloat(s_PLUniforms[i].Constant, pls[i].Constant);
        s_LitShader->SetFloat(s_PLUniforms[i].Linear, pls[i].Linear);
        s_LitShader->SetFloat(s_PLUniforms[i].Quadratic, pls[i].Quadratic);
    }

    // 聚光灯 Uniform
    auto& sls = scene.GetSpotLights();
    int slCount = std::min((int)sls.size(), (int)MAX_SPOT_LIGHTS);
    s_LitShader->SetInt("uSLCount", slCount);
    for (int i = 0; i < (int)sls.size() && i < MAX_SPOT_LIGHTS; i++) {
        s_LitShader->SetVec3(s_SLUniforms[i].Pos, sls[i].Position.x, sls[i].Position.y, sls[i].Position.z);
        s_LitShader->SetVec3(s_SLUniforms[i].Dir, sls[i].Direction.x, sls[i].Direction.y, sls[i].Direction.z);
        s_LitShader->SetVec3(s_SLUniforms[i].Color, sls[i].Color.x, sls[i].Color.y, sls[i].Color.z);
        s_LitShader->SetFloat(s_SLUniforms[i].Intensity, sls[i].Intensity);
        s_LitShader->SetFloat(s_SLUniforms[i].InnerCut, cosf(glm::radians(sls[i].InnerCutoff)));
        s_LitShader->SetFloat(s_SLUniforms[i].OuterCut, cosf(glm::radians(sls[i].OuterCutoff)));
        s_LitShader->SetFloat(s_SLUniforms[i].Constant, sls[i].Constant);
        s_LitShader->SetFloat(s_SLUniforms[i].Linear, sls[i].Linear);
        s_LitShader->SetFloat(s_SLUniforms[i].Quadratic, sls[i].Quadratic);
    }

    // 视锥体剔除准备
    Frustum frustum;
    frustum.ExtractFromVP(camera.GetViewProjectionMatrix());

    // 遍历实体
    for (auto e : world.GetEntities()) {
        auto* tr = world.GetComponent<TransformComponent>(e);
        auto* rc = world.GetComponent<RenderComponent>(e);
        if (!tr || !rc) continue;

        // 视锥体剪裁 — 跳过不可见实体
        {
            AABB worldAABB;
            worldAABB.Min = glm::vec3(tr->X - tr->ScaleX * 0.5f,
                                      tr->Y - tr->ScaleY * 0.5f,
                                      tr->Z - tr->ScaleZ * 0.5f);
            worldAABB.Max = glm::vec3(tr->X + tr->ScaleX * 0.5f,
                                      tr->Y + tr->ScaleY * 0.5f,
                                      tr->Z + tr->ScaleZ * 0.5f);
            // plane 很大，不剪裁
            if (rc->MeshType != "plane" && !frustum.IsAABBVisible(worldAABB))
                continue;
        }

        // Model 矩阵
        glm::mat4 model = glm::translate(glm::mat4(1.0f), {tr->X, tr->Y, tr->Z});
        model = glm::rotate(model, glm::radians(tr->RotY), {0, 1, 0});
        model = glm::rotate(model, glm::radians(tr->RotX), {1, 0, 0});
        model = glm::rotate(model, glm::radians(tr->RotZ), {0, 0, 1});
        model = glm::scale(model, {tr->ScaleX, tr->ScaleY, tr->ScaleZ});

        // 旋转动画（如果 TagComponent 指定）
        auto* tag = world.GetComponent<TagComponent>(e);
        if (tag && tag->Name == "CenterCube") {
            model = glm::translate(glm::mat4(1.0f), {tr->X, tr->Y, tr->Z});
            model = glm::rotate(model, t * 0.6f, {0, 1, 0});
            model = glm::rotate(model, t * 0.2f, {1, 0, 0});
        }

        s_LitShader->SetMat4("uModel", glm::value_ptr(model));

        // CPU 预计算法线矩阵（避免 GPU 每顶点 inverse）
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));
        s_LitShader->SetMat3("uNormalMat", glm::value_ptr(normalMat));

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

            // 法线贴图
            if (!mat->NormalMapName.empty()) {
                s_LitShader->SetInt("uUseNormalMap", 1);
                auto nmap = ResourceManager::GetTexture(mat->NormalMapName);
                if (nmap) { nmap->Bind(2); s_LitShader->SetInt("uNormalMap", 2); }
            } else {
                s_LitShader->SetInt("uUseNormalMap", 0);
            }

            // 自发光
            if (mat->Emissive) {
                s_LitShader->SetInt("uIsEmissive", 1);
                s_LitShader->SetVec3("uEmissiveColor", mat->EmissiveR, mat->EmissiveG, mat->EmissiveB);
                s_LitShader->SetFloat("uEmissiveIntensity", mat->EmissiveIntensity);
            } else {
                s_LitShader->SetInt("uIsEmissive", 0);
            }
        } else {
            // 旧兼容路径
            s_LitShader->SetVec3("uMatDiffuse", rc->ColorR, rc->ColorG, rc->ColorB);
            s_LitShader->SetVec3("uMatSpecular", 0.8f, 0.8f, 0.8f);
            s_LitShader->SetFloat("uShininess", rc->Shininess);
            s_LitShader->SetInt("uUseNormalMap", 0);
            s_LitShader->SetInt("uIsEmissive", 0);

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

    // 聚光灯 Gizmo
    auto& sls = scene.GetSpotLights();
    for (auto& sl : sls) {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), sl.Position);
        m = glm::scale(m, glm::vec3(0.15f));
        s_EmissiveShader->SetMat4("uModel", glm::value_ptr(m));
        s_EmissiveShader->SetVec3("uColor",
            sl.Color.x * sl.Intensity,
            sl.Color.y * sl.Intensity,
            sl.Color.z * sl.Intensity);
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
