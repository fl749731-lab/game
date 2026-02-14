#include "engine/core/application.h"
#include "engine/core/allocator.h"
#include "engine/core/log.h"
#include "engine/renderer/shader_library.h"
#include "engine/core/time.h"
#include "engine/core/job_system.h"
#include "engine/core/async_loader.h"
#include "engine/core/resource_manager.h"
#include "engine/core/scene.h"
#include "engine/platform/input.h"
#include "engine/renderer/renderer.h"
#include "engine/renderer/skybox.h"
#include "engine/renderer/particle.h"
#include "engine/renderer/sprite_batch.h"
#include "engine/renderer/scene_renderer.h"
#include "engine/audio/audio_engine.h"
#include "engine/debug/debug_draw.h"
#include "engine/debug/debug_ui.h"
#include "engine/debug/profiler.h"

namespace Engine {

Application* Application::s_Instance = nullptr;

// ── 构造 ────────────────────────────────────────────────────

Application::Application(const ApplicationConfig& config)
    : m_Window(WindowConfig{config.Title, config.Width, config.Height, config.VSync})
{
    if (s_Instance) {
        LOG_ERROR("[Application] 重复创建！只允许一个实例");
        return;
    }
    s_Instance = this;

    Logger::Init();
    LOG_INFO("=== 引擎 Application 初始化 ===");

    InitSubsystems();

    LOG_INFO("[Application] 初始化完成");
}

// ── 析构 ────────────────────────────────────────────────────

Application::~Application() {
    // 反序 Detach 所有 Layer
    for (auto it = m_Layers.rbegin(); it != m_Layers.rend(); ++it) {
        LOG_INFO("[Application] Detach Layer: %s", (*it)->GetName());
        (*it)->OnDetach();
    }
    m_Layers.clear();

    ShutdownSubsystems();

    LOG_INFO("[Application] 已关闭");
    s_Instance = nullptr;
}

// ── 子系统初始化 / 关闭 ─────────────────────────────────────

void Application::InitSubsystems() {
    FrameAllocator::Init();  // 4MB 帧分配器
    Input::Init(m_Window.GetNativeWindow());
    Renderer::Init();
    Skybox::Init();
    ParticleSystem::Init();
    AudioEngine::Init();
    SpriteBatch::Init();

    JobSystem::Init();
    AsyncLoader::Init();

    // SceneRenderer (延迟渲染管线)
    SceneRendererConfig renderCfg;
    renderCfg.Width  = m_Window.GetWidth();
    renderCfg.Height = m_Window.GetHeight();
    SceneRenderer::Init(renderCfg);

    // 调试工具
    DebugDraw::Init();
    DebugUI::Init();

    // Shader 库 (Debug 模式支持热重载)
    ShaderLibrary::Init();

    LOG_INFO("[Application] 所有子系统已初始化");
}

void Application::ShutdownSubsystems() {
#ifdef ENGINE_HAS_PYTHON
    // PythonEngine 由 Layer 自行管理
#endif
    ShaderLibrary::Shutdown();
    DebugUI::Shutdown();
    DebugDraw::Shutdown();
    SpriteBatch::Shutdown();
    ParticleSystem::Shutdown();
    AudioEngine::Shutdown();
    Skybox::Shutdown();
    SceneRenderer::Shutdown();
    AsyncLoader::Shutdown();
    JobSystem::Shutdown();
    SceneManager::Clear();
    ResourceManager::Clear();
    Renderer::Shutdown();
    FrameAllocator::Shutdown();
}

// ── 主循环 ──────────────────────────────────────────────────

void Application::Run() {
    LOG_INFO("[Application] 进入主循环");

    while (m_Running && !m_Window.ShouldClose()) {
        Time::Update();
        Input::Update();
        Renderer::ResetStats();
        Profiler::BeginTimer("Frame");
        FrameAllocator::Reset();
        ShaderLibrary::CheckHotReload();  // Shader 热重载检查

        f32 dt = Time::DeltaTime();

        // 异步资源上传
        AsyncLoader::FlushUploads(4);

        // 窗口 Resize 检测
        // (由各 Layer 通过 OnEvent 处理)

        // Layer 更新
        for (auto& layer : m_Layers) {
            layer->OnUpdate(dt);
        }

        // Layer 渲染
        for (auto& layer : m_Layers) {
            layer->OnRender();
        }

        // Layer ImGui
        for (auto& layer : m_Layers) {
            layer->OnImGui();
        }

        Profiler::EndFrame();
        m_Window.Update();
        Input::EndFrame();
    }

    LOG_INFO("[Application] 退出主循环 | 总帧数: %llu", Time::FrameCount());
}

// ── Layer 管理 ──────────────────────────────────────────────

void Application::PushLayer(Scope<Layer> layer) {
    LOG_INFO("[Application] Push Layer: %s", layer->GetName());
    layer->OnAttach();
    m_Layers.push_back(std::move(layer));
}

void Application::PopLayer() {
    if (m_Layers.empty()) return;
    auto& back = m_Layers.back();
    LOG_INFO("[Application] Pop Layer: %s", back->GetName());
    back->OnDetach();
    m_Layers.pop_back();
}

void Application::Close() {
    m_Running = false;
}

Application& Application::Get() {
    return *s_Instance;
}

} // namespace Engine
