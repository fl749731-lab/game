// ── Game Engine — Zombie Survival ─────────────────────────
// 入口点: 创建 Application 并推送 GameLayer

#include "engine/engine.h"
#include "game_layer.h"

int main() {
    Engine::Application app({
        .Title = "Zombie Survival",
        .Width = 1280,
        .Height = 720
    });
    app.PushLayer(Engine::CreateScope<Engine::GameLayer>());
    app.Run();
    return 0;
}

