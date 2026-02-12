#include "engine/core/scene.h"
#include "engine/core/log.h"

namespace Engine {

// ── Scene ───────────────────────────────────────────────────

Scene::Scene(const std::string& name)
    : m_Name(name)
{
    LOG_INFO("[场景] 创建: '%s'", m_Name.c_str());
}

void Scene::Update(f32 dt) {
    m_World.Update(dt);
}

Entity Scene::CreateEntity(const std::string& name) {
    return m_World.CreateEntity(name);
}

void Scene::DestroyEntity(Entity e) {
    m_World.DestroyEntity(e);
}

// ── SceneManager ────────────────────────────────────────────

std::vector<Ref<Scene>> SceneManager::s_SceneStack;

void SceneManager::PushScene(Ref<Scene> scene) {
    LOG_INFO("[场景管理] Push: '%s'", scene->GetName().c_str());
    s_SceneStack.push_back(scene);
}

void SceneManager::PopScene() {
    if (!s_SceneStack.empty()) {
        LOG_INFO("[场景管理] Pop: '%s'", s_SceneStack.back()->GetName().c_str());
        s_SceneStack.pop_back();
    }
}

Ref<Scene> SceneManager::GetActiveScene() {
    if (s_SceneStack.empty()) return nullptr;
    return s_SceneStack.back();
}

void SceneManager::Update(f32 dt) {
    if (!s_SceneStack.empty()) {
        s_SceneStack.back()->Update(dt);
    }
}

void SceneManager::Clear() {
    LOG_INFO("[场景管理] 清除 %zu 个场景", s_SceneStack.size());
    s_SceneStack.clear();
}

} // namespace Engine
