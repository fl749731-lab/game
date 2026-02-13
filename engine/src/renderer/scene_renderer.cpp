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
#include "engine/renderer/g_buffer.h"
#include "engine/renderer/screen_quad.h"
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

// ── 预缓存 Uniform 名称 ─────────────────────────────────────
struct PLUniformNames {
    std::string Pos, Color, Intensity;
    std::string Constant, Linear, Quadratic;
};
static std::array<PLUniformNames, Engine::MAX_POINT_LIGHTS> s_PLUniforms;

struct SLUniformNames {
    std::string Pos, Dir, Color, Intensity;
    std::string InnerCut, OuterCut;
    std::string Constant, Linear, Quadratic;
};
static std::array<SLUniformNames, Engine::MAX_SPOT_LIGHTS> s_SLUniforms;

namespace Engine {

// ── 静态成员定义 ────────────────────────────────────────────

Scope<Framebuffer> SceneRenderer::s_HDR_FBO = nullptr;
Ref<Shader> SceneRenderer::s_GBufferShader  = nullptr;
Ref<Shader> SceneRenderer::s_DeferredShader = nullptr;
Ref<Shader> SceneRenderer::s_EmissiveShader = nullptr;
Ref<Shader> SceneRenderer::s_GBufDebugShader = nullptr;
Ref<Shader> SceneRenderer::s_LitShader     = nullptr;
u32  SceneRenderer::s_CheckerTexID = 0;
u32  SceneRenderer::s_Width = 0;
u32  SceneRenderer::s_Height = 0;
f32  SceneRenderer::s_Exposure = 1.2f;
bool SceneRenderer::s_BloomEnabled = true;
int  SceneRenderer::s_GBufDebugMode = 0;

// ── 初始化 ──────────────────────────────────────────────────

void SceneRenderer::Init(const SceneRendererConfig& config) {
    s_Width  = config.Width;
    s_Height = config.Height;
    s_Exposure = config.Exposure;
    s_BloomEnabled = config.BloomEnabled;

    // HDR FBO (用于光照 Pass 输出)
    FramebufferSpec fboSpec;
    fboSpec.Width  = config.Width;
    fboSpec.Height = config.Height;
    fboSpec.HDR = true;
    s_HDR_FBO = std::make_unique<Framebuffer>(fboSpec);

    // G-Buffer (MRT)
    GBuffer::Init(config.Width, config.Height);

    // Shader
    s_GBufferShader = ResourceManager::LoadShader("gbuffer",
        Shaders::GBufferVertex, Shaders::GBufferFragment);
    s_DeferredShader = ResourceManager::LoadShader("deferred_light",
        Shaders::DeferredLightVertex, Shaders::DeferredLightFragment);
    s_GBufDebugShader = ResourceManager::LoadShader("gbuf_debug",
        Shaders::GBufferDebugVertex, Shaders::GBufferDebugFragment);
    s_EmissiveShader = ResourceManager::LoadShader("emissive",
        Shaders::EmissiveVertex, Shaders::EmissiveFragment);
    s_LitShader = ResourceManager::LoadShader("lit",
        Shaders::LitVertex, Shaders::LitFragment);

    // 基础网格
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

    // Uniform 名称缓存
    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        std::string idx = std::to_string(i);
        s_PLUniforms[i].Pos       = "uPLPos[" + idx + "]";
        s_PLUniforms[i].Color     = "uPLColor[" + idx + "]";
        s_PLUniforms[i].Intensity = "uPLIntensity[" + idx + "]";
        s_PLUniforms[i].Constant  = "uPLConstant[" + idx + "]";
        s_PLUniforms[i].Linear    = "uPLLinear[" + idx + "]";
        s_PLUniforms[i].Quadratic = "uPLQuadratic[" + idx + "]";
    }
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

    // 后处理 + Bloom + 阴影
    PostProcess::Init();
    PostProcess::SetExposure(s_Exposure);
    Bloom::Init(config.Width, config.Height);
    ShadowMap::Init();

    LOG_INFO("[SceneRenderer] 初始化完成 (%ux%u) — 延迟渲染管线", s_Width, s_Height);
}

void SceneRenderer::Shutdown() {
    if (s_CheckerTexID) { glDeleteTextures(1, &s_CheckerTexID); s_CheckerTexID = 0; }
    s_HDR_FBO.reset();
    GBuffer::Shutdown();
    s_GBufferShader.reset();
    s_DeferredShader.reset();
    s_EmissiveShader.reset();
    s_GBufDebugShader.reset();
    s_LitShader.reset();
    Bloom::Shutdown();
    ShadowMap::Shutdown();
    PostProcess::Shutdown();
    LOG_DEBUG("[SceneRenderer] 已清理");
}

void SceneRenderer::Resize(u32 width, u32 height) {
    s_Width  = width;
    s_Height = height;
    if (s_HDR_FBO) s_HDR_FBO->Resize(width, height);
    GBuffer::Resize(width, height);
    Bloom::Resize(width, height);
}

// ── 核心渲染方法 ────────────────────────────────────────────

void SceneRenderer::RenderScene(Scene& scene, PerspectiveCamera& camera) {
    Profiler::BeginTimer("Render");

    ShadowPass(scene, camera);
    GeometryPass(scene, camera);

    // 调试模式: 直接显示 G-Buffer
    if (s_GBufDebugMode > 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        Renderer::SetViewport(0, 0, s_Width, s_Height);
        Renderer::Clear();

        s_GBufDebugShader->Bind();
        GBuffer::BindTextures(0);
        s_GBufDebugShader->SetInt("gPosition", 0);
        s_GBufDebugShader->SetInt("gNormal", 1);
        s_GBufDebugShader->SetInt("gAlbedoSpec", 2);
        s_GBufDebugShader->SetInt("gEmissive", 3);
        s_GBufDebugShader->SetInt("uDebugMode", s_GBufDebugMode - 1);
        ScreenQuad::Draw();

        Profiler::EndTimer("Render");
        return;
    }

    LightingPass(scene, camera);
    ForwardPass(scene, camera);

    Profiler::EndTimer("Render");

    PostProcessPass();
}

// ── Pass 0: 阴影深度 ───────────────────────────────────────

void SceneRenderer::ShadowPass(Scene& scene, PerspectiveCamera& camera) {
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

// ── Pass 1: G-Buffer 几何 ──────────────────────────────────

void SceneRenderer::GeometryPass(Scene& scene, PerspectiveCamera& camera) {
    GBuffer::Bind();
    Renderer::SetClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    Renderer::Clear();

    RenderEntitiesDeferred(scene, camera);

    GBuffer::Unbind();
}

// ── Pass 2: 延迟光照 ───────────────────────────────────────

void SceneRenderer::LightingPass(Scene& scene, PerspectiveCamera& camera) {
    s_HDR_FBO->Bind();
    Renderer::SetClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    Renderer::Clear();

    s_DeferredShader->Bind();

    // 绑定 G-Buffer 纹理到纹理单元 0~3
    GBuffer::BindTextures(0);
    s_DeferredShader->SetInt("gPosition", 0);
    s_DeferredShader->SetInt("gNormal", 1);
    s_DeferredShader->SetInt("gAlbedoSpec", 2);
    s_DeferredShader->SetInt("gEmissive", 3);

    // 设置光照 Uniform
    SetupLightUniforms(scene, s_DeferredShader.get(), camera);

    // 绘制全屏四边形
    ScreenQuad::Draw();

    // 不要 Unbind HDR FBO — 前向 Pass 会继续写入
}

// ── Pass 3: 前向叠加 (天空盒/透明物/自发光/粒子/调试) ────

void SceneRenderer::ForwardPass(Scene& scene, PerspectiveCamera& camera) {
    // 直接在 HDR FBO 上继续绘制（光照 Pass 已经绑定了 HDR FBO）
    // 前向叠加 Pass 不需要深度测试（天空盒在最远处，粒子/调试线也不需要严格深度遮挡）
    Renderer::SetViewport(0, 0, s_Width, s_Height);


    // 天空盒 (深度测试读取但不写入)
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glm::mat4 skyVP = camera.GetProjectionMatrix() *
        glm::mat4(glm::mat3(camera.GetViewMatrix()));
    Skybox::Draw(glm::value_ptr(skyVP));
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    // 光源可视化 (前向自发光 Shader)
    {
        auto& pls = scene.GetPointLights();
        auto& sls = scene.GetSpotLights();
        s_EmissiveShader->Bind();
        s_EmissiveShader->SetMat4("uVP", glm::value_ptr(camera.GetViewProjectionMatrix()));
        auto* cubeMesh = ResourceManager::GetMesh("cube");

        if (cubeMesh) {
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
    }

    // 粒子
    ParticleSystem::Draw(
        glm::value_ptr(camera.GetViewProjectionMatrix()),
        camera.GetRight(), camera.GetUp());

    // 调试线框
    DebugDraw::Flush(glm::value_ptr(camera.GetViewProjectionMatrix()));

    s_HDR_FBO->Unbind();
}

// ── Pass 4: 后处理 ──────────────────────────────────────────

void SceneRenderer::PostProcessPass() {
    Renderer::SetViewport(0, 0, s_Width, s_Height);
    Renderer::Clear();

    if (s_BloomEnabled) {
        Profiler::BeginTimer("Bloom");
        u32 bloomTex = Bloom::Process(s_HDR_FBO->GetColorAttachmentID());
        Profiler::EndTimer("Bloom");
        PostProcess::Draw(s_HDR_FBO->GetColorAttachmentID(),
                          bloomTex, Bloom::GetIntensity());
    } else {
        PostProcess::Draw(s_HDR_FBO->GetColorAttachmentID());
    }
}

// ── 延迟几何实体渲染 ────────────────────────────────────────

void SceneRenderer::RenderEntitiesDeferred(Scene& scene, PerspectiveCamera& camera) {
    auto& world = scene.GetWorld();
    float t = Time::Elapsed();

    s_GBufferShader->Bind();
    s_GBufferShader->SetMat4("uVP", glm::value_ptr(camera.GetViewProjectionMatrix()));

    // 视锥体剔除准备
    Frustum frustum;
    frustum.ExtractFromVP(camera.GetViewProjectionMatrix());

    for (auto e : world.GetEntities()) {
        auto* tr = world.GetComponent<TransformComponent>(e);
        auto* rc = world.GetComponent<RenderComponent>(e);
        if (!tr || !rc) continue;

        // 视锥体剪裁
        {
            AABB worldAABB;
            worldAABB.Min = glm::vec3(tr->X - tr->ScaleX * 0.5f,
                                      tr->Y - tr->ScaleY * 0.5f,
                                      tr->Z - tr->ScaleZ * 0.5f);
            worldAABB.Max = glm::vec3(tr->X + tr->ScaleX * 0.5f,
                                      tr->Y + tr->ScaleY * 0.5f,
                                      tr->Z + tr->ScaleZ * 0.5f);
            if (rc->MeshType != "plane" && !frustum.IsAABBVisible(worldAABB))
                continue;
        }

        // Model 矩阵
        glm::mat4 model = glm::translate(glm::mat4(1.0f), {tr->X, tr->Y, tr->Z});
        model = glm::rotate(model, glm::radians(tr->RotY), {0, 1, 0});
        model = glm::rotate(model, glm::radians(tr->RotX), {1, 0, 0});
        model = glm::rotate(model, glm::radians(tr->RotZ), {0, 0, 1});
        model = glm::scale(model, {tr->ScaleX, tr->ScaleY, tr->ScaleZ});

        // 旋转动画
        auto* tag = world.GetComponent<TagComponent>(e);
        if (tag && tag->Name == "CenterCube") {
            model = glm::translate(glm::mat4(1.0f), {tr->X, tr->Y, tr->Z});
            model = glm::rotate(model, t * 0.6f, {0, 1, 0});
            model = glm::rotate(model, t * 0.2f, {1, 0, 0});
        }

        s_GBufferShader->SetMat4("uModel", glm::value_ptr(model));
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));
        s_GBufferShader->SetMat3("uNormalMat", glm::value_ptr(normalMat));

        // 材质 (PBR)
        auto* mat = world.GetComponent<MaterialComponent>(e);
        if (mat) {
            s_GBufferShader->SetVec3("uAlbedo", mat->DiffuseR, mat->DiffuseG, mat->DiffuseB);
            s_GBufferShader->SetFloat("uMetallic", mat->Metallic);
            s_GBufferShader->SetFloat("uRoughness", mat->Roughness);

            if (!mat->TextureName.empty()) {
                s_GBufferShader->SetInt("uUseTex", 1);
                auto tex = ResourceManager::GetTexture(mat->TextureName);
                if (tex) { tex->Bind(0); s_GBufferShader->SetInt("uTex", 0); }
            } else {
                s_GBufferShader->SetInt("uUseTex", 0);
            }

            if (!mat->NormalMapName.empty()) {
                s_GBufferShader->SetInt("uUseNormalMap", 1);
                auto nmap = ResourceManager::GetTexture(mat->NormalMapName);
                if (nmap) { nmap->Bind(2); s_GBufferShader->SetInt("uNormalMap", 2); }
            } else {
                s_GBufferShader->SetInt("uUseNormalMap", 0);
            }

            if (mat->Emissive) {
                s_GBufferShader->SetInt("uIsEmissive", 1);
                s_GBufferShader->SetVec3("uEmissiveColor", mat->EmissiveR, mat->EmissiveG, mat->EmissiveB);
                s_GBufferShader->SetFloat("uEmissiveIntensity", mat->EmissiveIntensity);
            } else {
                s_GBufferShader->SetInt("uIsEmissive", 0);
            }
        } else {
            // 旧兼容路径 → 默认 PBR 参数
            s_GBufferShader->SetVec3("uAlbedo", rc->ColorR, rc->ColorG, rc->ColorB);
            s_GBufferShader->SetFloat("uMetallic", 0.0f);   // 非金属
            s_GBufferShader->SetFloat("uRoughness", 0.5f);  // 中等粗糙
            s_GBufferShader->SetInt("uUseNormalMap", 0);
            s_GBufferShader->SetInt("uIsEmissive", 0);

            if (rc->MeshType == "plane") {
                s_GBufferShader->SetInt("uUseTex", 1);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, s_CheckerTexID);
                s_GBufferShader->SetInt("uTex", 0);
            } else {
                s_GBufferShader->SetInt("uUseTex", 0);
            }
        }

        auto* mesh = ResourceManager::GetMesh(rc->MeshType);
        if (mesh) mesh->Draw();
    }
}

// ── 光照 Uniform 设置 ──────────────────────────────────────

void SceneRenderer::SetupLightUniforms(Scene& scene, Shader* shader, PerspectiveCamera& camera) {
    auto& dirLight = scene.GetDirLight();
    auto& pls = scene.GetPointLights();
    auto& sls = scene.GetSpotLights();
    glm::vec3 cp = camera.GetPosition();

    shader->SetVec3("uDirLightDir", dirLight.Direction.x, dirLight.Direction.y, dirLight.Direction.z);
    shader->SetVec3("uDirLightColor",
        dirLight.Color.x * dirLight.Intensity,
        dirLight.Color.y * dirLight.Intensity,
        dirLight.Color.z * dirLight.Intensity);
    shader->SetVec3("uViewPos", cp.x, cp.y, cp.z);
    shader->SetFloat("uAmbientStrength", 0.15f);

    // 阴影
    shader->SetMat4("uLightSpaceMat", glm::value_ptr(ShadowMap::GetLightSpaceMatrix()));
    shader->SetInt("uShadowEnabled", 1);
    shader->SetInt("uShadowMap", 4);  // 纹理单元 4
    glActiveTexture(GL_TEXTURE0 + 4);
    glBindTexture(GL_TEXTURE_2D, ShadowMap::GetShadowTextureID());

    // 点光源
    int plCount = std::min((int)pls.size(), (int)MAX_POINT_LIGHTS);
    shader->SetInt("uPLCount", plCount);
    for (int i = 0; i < plCount; i++) {
        shader->SetVec3(s_PLUniforms[i].Pos, pls[i].Position.x, pls[i].Position.y, pls[i].Position.z);
        shader->SetVec3(s_PLUniforms[i].Color, pls[i].Color.x, pls[i].Color.y, pls[i].Color.z);
        shader->SetFloat(s_PLUniforms[i].Intensity, pls[i].Intensity);
        shader->SetFloat(s_PLUniforms[i].Constant, pls[i].Constant);
        shader->SetFloat(s_PLUniforms[i].Linear, pls[i].Linear);
        shader->SetFloat(s_PLUniforms[i].Quadratic, pls[i].Quadratic);
    }

    // 聚光灯
    int slCount = std::min((int)sls.size(), (int)MAX_SPOT_LIGHTS);
    shader->SetInt("uSLCount", slCount);
    for (int i = 0; i < slCount; i++) {
        shader->SetVec3(s_SLUniforms[i].Pos, sls[i].Position.x, sls[i].Position.y, sls[i].Position.z);
        shader->SetVec3(s_SLUniforms[i].Dir, sls[i].Direction.x, sls[i].Direction.y, sls[i].Direction.z);
        shader->SetVec3(s_SLUniforms[i].Color, sls[i].Color.x, sls[i].Color.y, sls[i].Color.z);
        shader->SetFloat(s_SLUniforms[i].Intensity, sls[i].Intensity);
        shader->SetFloat(s_SLUniforms[i].InnerCut, cosf(glm::radians(sls[i].InnerCutoff)));
        shader->SetFloat(s_SLUniforms[i].OuterCut, cosf(glm::radians(sls[i].OuterCutoff)));
        shader->SetFloat(s_SLUniforms[i].Constant, sls[i].Constant);
        shader->SetFloat(s_SLUniforms[i].Linear, sls[i].Linear);
        shader->SetFloat(s_SLUniforms[i].Quadratic, sls[i].Quadratic);
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

void SceneRenderer::SetGBufferDebugMode(int mode) {
    s_GBufDebugMode = mode;
    if (mode > 0)
        LOG_INFO("[SceneRenderer] G-Buffer 调试模式: %d", mode);
    else
        LOG_INFO("[SceneRenderer] G-Buffer 调试关闭");
}

int SceneRenderer::GetGBufferDebugMode() { return s_GBufDebugMode; }

u32 SceneRenderer::GetHDRColorAttachment() {
    return s_HDR_FBO ? s_HDR_FBO->GetColorAttachmentID() : 0;
}

} // namespace Engine
