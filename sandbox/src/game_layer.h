#pragma once

#include "engine/engine.h"
#include "game/game_map.h"
#include "game/combat.h"
#include "game/zombie.h"
#include "game/building.h"
#include "game/player_controller.h"
#include "game/inventory.h"
#include "game/time_system.h"

namespace Engine {

class GameLayer : public Layer {
public:
    const char* GetName() const override { return "ZombieSurvival"; }

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(f32 dt) override;
    void OnRender() override;

private:
    // ── 初始化 ──────────────────────────────────────────
    void RegisterItems();
    void RegisterRecipes();
    void SetupPlayer();
    void SpawnInitialZombies();
    void SetupLootPoints();

    // ── 输入处理 ────────────────────────────────────────
    void HandleInput(f32 dt);
    void HandleBuildInput(f32 dt);

    // ── 渲染 ────────────────────────────────────────────
    void RenderTilemap();
    void RenderEntities();
    void RenderBuildPreview();
    void RenderHUD();
    void RenderNightOverlay();

    // ── 游戏逻辑 ────────────────────────────────────────
    void UpdateSurvival(f32 dt);
    void UpdateZombieSpawning(f32 dt);
    void CleanupDeadZombies();

    // ── 场景 ────────────────────────────────────────────
    Ref<Scene> m_Scene;

    // ── 地图 ────────────────────────────────────────────
    GameMap m_GameMap;

    // ── 系统 (指针，便于调用非 Update 方法) ────────────
    CombatSystem*       m_CombatSys   = nullptr;
    ZombieSystem*       m_ZombieSys   = nullptr;
    BuildingSystem*     m_BuildingSys = nullptr;
    GameTimeSystem*     m_TimeSys     = nullptr;

    // ── 刷新器 ──────────────────────────────────────────
    ZombieSpawner m_Spawner;

    // ── 相机 ────────────────────────────────────────────
    Camera2DController  m_CamCtrl;

    // ── 玩家实体 ────────────────────────────────────────
    Entity m_Player = INVALID_ENTITY;

    // ── 状态 ────────────────────────────────────────────
    u32 m_DayCount     = 1;
    u32 m_KillCount    = 0;
    u32 m_BuildSlot    = 0;        // 当前选中的建筑类型
    bool m_GameOver    = false;
    f32  m_AnimTimer   = 0.0f;    // 全局动画计时器

    // ── 纹理资源 ────────────────────────────────────────
    Ref<Texture2D> m_TexGrass;
    Ref<Texture2D> m_TexDirt;
    Ref<Texture2D> m_TexSand;
    Ref<Texture2D> m_TexRockWall;
    Ref<Texture2D> m_TexFence;
    Ref<Texture2D> m_TexPlayer;
    Ref<Texture2D> m_TexSlime;
    Ref<Texture2D> m_TexItems;
    Ref<Texture2D> m_TexFireWall;

    // Tile 颜色映射 (TileID → 颜色) — 纹理加载失败时回退
    static glm::vec4 GetTileColor(u16 tileID);
};

} // namespace Engine
