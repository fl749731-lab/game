#include "engine/renderer/vulkan/vulkan_scene_renderer.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/renderer/vulkan/vulkan_shader.h"
#include "engine/renderer/vulkan/vulkan_mesh.h"
#include "engine/renderer/vulkan/vulkan_texture.h"
#include "engine/renderer/vulkan/vulkan_descriptor.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/camera.h"
#include "engine/core/scene.h"
#include "engine/core/ecs.h"
#include "engine/core/log.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <unordered_map>

namespace Engine {

// ── GPU UBO 数据结构 ───────────────────────────────────────

struct GlobalUBOData {
    glm::mat4 View;
    glm::mat4 Projection;
    glm::vec4 CameraPos;
    glm::vec4 LightDir;     // xyz=direction, w=intensity
    glm::vec4 LightColor;   // xyz=color,     w=ambient
};

struct PushConstantData {
    glm::mat4 Model;
};

// ── 静态资源 ───────────────────────────────────────────────

static std::unique_ptr<VulkanShader>    s_BasicShader;
static VkDescriptorSetLayout            s_GlobalSetLayout = VK_NULL_HANDLE;
static VkDescriptorSet                  s_GlobalSet       = VK_NULL_HANDLE;
static VulkanUBO                        s_GlobalUBO       = {};
static std::unique_ptr<VulkanTexture2D> s_WhiteTexture;
static std::string                      s_ShaderDir;

// Mesh 缓存 (类型名 → VulkanMesh)
static std::unordered_map<std::string, std::unique_ptr<VulkanMesh>> s_MeshCache;

// ── 白色默认纹理 (1x1) ────────────────────────────────────

static void CreateWhiteTexture() {
    u32 white = 0xFFFFFFFF;
    s_WhiteTexture = std::make_unique<VulkanTexture2D>(1, 1, &white, 4);
}

// ── 获取或创建 Mesh ────────────────────────────────────────

static VulkanMesh* GetOrCreateMesh(const std::string& type) {
    auto it = s_MeshCache.find(type);
    if (it != s_MeshCache.end()) return it->second.get();

    std::unique_ptr<VulkanMesh> mesh;
    if (type == "cube")        mesh = VulkanMesh::CreateCube();
    else if (type == "plane")  mesh = VulkanMesh::CreatePlane();
    else if (type == "sphere") mesh = VulkanMesh::CreateSphere();
    else                       mesh = VulkanMesh::CreateCube(); // fallback

    auto* ptr = mesh.get();
    s_MeshCache[type] = std::move(mesh);
    return ptr;
}

// ═══════════════════════════════════════════════════════════
//  Init / Shutdown
// ═══════════════════════════════════════════════════════════

bool VulkanSceneRenderer::Init(const VulkanSceneConfig& config) {
    s_ShaderDir = config.ShaderDir;

    // Descriptor Pool
    if (!VulkanDescriptorPool::Init(100)) return false;

    // Global UBO DescriptorSetLayout
    //   binding 0: UBO (GlobalUBOData)
    //   binding 1: Combined Image Sampler (default texture)
    s_GlobalSetLayout = VulkanDescriptorSetLayoutBuilder()
        .AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build();

    if (s_GlobalSetLayout == VK_NULL_HANDLE) return false;

    // UBO
    s_GlobalUBO = VulkanUniformManager::CreateUBO(sizeof(GlobalUBOData));

    // 白色纹理
    CreateWhiteTexture();

    // Descriptor Set
    s_GlobalSet = VulkanDescriptorPool::Allocate(s_GlobalSetLayout);
    VulkanDescriptorWriter(s_GlobalSet)
        .WriteBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                     s_GlobalUBO.Buffer, sizeof(GlobalUBOData))
        .WriteImage(1, s_WhiteTexture->GetImageView(), s_WhiteTexture->GetSampler())
        .Flush();

    // Shader + Pipeline
    VulkanShaderConfig shaderConfig;
    shaderConfig.VertexPath   = s_ShaderDir + "basic.vert.spv";
    shaderConfig.FragmentPath = s_ShaderDir + "basic.frag.spv";
    shaderConfig.VertexInput  = VulkanShader::GetMeshVertexInput();
    shaderConfig.SetLayout    = s_GlobalSetLayout;
    shaderConfig.PushConstantSize   = sizeof(PushConstantData);
    shaderConfig.PushConstantStages = VK_SHADER_STAGE_VERTEX_BIT;

    s_BasicShader = std::make_unique<VulkanShader>(shaderConfig);
    if (!s_BasicShader->IsValid()) {
        LOG_WARN("[Vulkan] 基础着色器创建失败 (SPIR-V 可能未编译), 场景渲染器仍可用于清屏");
    }

    LOG_INFO("[Vulkan] 场景渲染器已初始化");
    return true;
}

void VulkanSceneRenderer::Shutdown() {
    vkDeviceWaitIdle(VulkanContext::GetDevice());

    s_MeshCache.clear();
    s_BasicShader.reset();
    s_WhiteTexture.reset();
    VulkanUniformManager::DestroyUBO(s_GlobalUBO);

    if (s_GlobalSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(VulkanContext::GetDevice(), s_GlobalSetLayout, nullptr);
        s_GlobalSetLayout = VK_NULL_HANDLE;
    }

    VulkanDescriptorPool::Shutdown();
    LOG_INFO("[Vulkan] 场景渲染器已关闭");
}

// ═══════════════════════════════════════════════════════════
//  RenderScene
// ═══════════════════════════════════════════════════════════

void VulkanSceneRenderer::RenderScene(Scene& scene, PerspectiveCamera& camera) {
    VkCommandBuffer cmd = VulkanRenderer::GetCurrentCommandBuffer();
    if (!cmd) return;

    // 更新 Global UBO
    GlobalUBOData uboData;
    uboData.View       = camera.GetViewMatrix();
    uboData.Projection = camera.GetProjectionMatrix();
    uboData.CameraPos  = glm::vec4(camera.GetPosition(), 1.0f);

    // 从 Scene 获取方向光
    auto& dirLight = scene.GetDirLight();
    uboData.LightDir   = glm::vec4(dirLight.Direction, dirLight.Intensity);
    uboData.LightColor = glm::vec4(dirLight.Color, 0.15f); // w = ambient

    VulkanUniformManager::UpdateUBO(s_GlobalUBO, &uboData, sizeof(uboData));

    if (!s_BasicShader || !s_BasicShader->IsValid()) return;

    // 设置 Dynamic Viewport/Scissor
    VkExtent2D extent = VulkanSwapchain::GetExtent();
    VkViewport viewport = {};
    viewport.width    = (f32)extent.width;
    viewport.height   = (f32)extent.height;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {{0, 0}, extent};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // 绑定 Shader Pipeline
    s_BasicShader->Bind(cmd);

    // 绑定 Global Descriptor Set
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            s_BasicShader->GetLayout(), 0, 1, &s_GlobalSet, 0, nullptr);

    // 遍历 ECS 中同时拥有 TransformComponent + RenderComponent 的实体
    auto& world = scene.GetWorld();
    world.ForEach2<RenderComponent, TransformComponent>(
        [&](Entity e, RenderComponent& rc, TransformComponent& tc) {
            VulkanMesh* mesh = GetOrCreateMesh(rc.MeshType);
            if (!mesh || !mesh->IsValid()) return;

            // Push Constants: Model 矩阵
            PushConstantData pushData;
            pushData.Model = tc.GetLocalMatrix();
            s_BasicShader->PushConstants(cmd, &pushData, sizeof(pushData));

            // 绑定并绘制
            mesh->Bind(cmd);
            mesh->Draw(cmd);
        });
}

void VulkanSceneRenderer::OnResize(u32 width, u32 height) {
    (void)width;
    (void)height;
}

void VulkanSceneRenderer::CreateResources() {}
void VulkanSceneRenderer::DestroyResources() {}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
