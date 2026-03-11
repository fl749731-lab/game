// ══════════════════════════════════════════════════════════════
//  GameLayer — 渲染模块
//  从 game_layer.cpp 拆分: 地图渲染 / 实体渲染 / HUD / 建造预览
// ══════════════════════════════════════════════════════════════

#include "game_layer.h"
#include "engine/core/application.h"
#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/renderer/sprite_batch.h"

#include <glad/glad.h>

#include <cmath>
#include <algorithm>

namespace Engine {

// ── 着色回退 ────────────────────────────────────────────────

glm::vec4 GameLayer::GetTileColor(u16 tileID) {
    switch (tileID) {
        case 0:  return {0.1f, 0.1f, 0.1f, 1.0f};   // 空 — 黑
        case 1:  return {0.29f, 0.49f, 0.25f, 1.0f};  // 草地 — 深绿
        case 2:  return {0.55f, 0.41f, 0.08f, 1.0f};  // 泥土 — 棕
        case 3:  return {0.53f, 0.53f, 0.53f, 1.0f};  // 石板 — 灰
        case 4:  return {0.2f, 0.4f, 0.67f, 1.0f};    // 水 — 蓝
        case 5:  return {0.85f, 0.78f, 0.55f, 1.0f};   // 沙地 — 米色
        case 10: return {0.18f, 0.35f, 0.15f, 1.0f};  // 树木 — 暗绿
        case 11: return {0.4f, 0.4f, 0.4f, 1.0f};     // 石头 — 灰
        case 12: return {0.55f, 0.27f, 0.07f, 1.0f};  // 栅栏 — 深棕
        case 13: return {0.63f, 0.32f, 0.18f, 1.0f};  // 墙壁 — 砖色
        default: return {0.5f, 0.5f, 0.5f, 1.0f};
    }
}

// ── 主渲染入口 ──────────────────────────────────────────────

void GameLayer::OnRender() {
    if (Application::Get().GetBackend() == GraphicsBackend::Vulkan) {
#ifdef ENGINE_ENABLE_VULKAN
        // 最小 Vulkan 可视化路径：先以动态清屏色确认渲染循环稳定
        bool isNight = (m_TimeSys && m_TimeSys->IsNight());
        if (isNight) {
            VulkanRenderer::SetClearColor(0.03f, 0.04f, 0.08f, 1.0f);
        } else {
            VulkanRenderer::SetClearColor(0.08f, 0.12f, 0.08f, 1.0f);
        }

        static bool s_LoggedVulkanMinimal = false;
        if (!s_LoggedVulkanMinimal) {
            LOG_WARN("[GameLayer] Vulkan 当前为最小可视化模式 (清屏)，2D Sprite 渲染待迁移");
            s_LoggedVulkanMinimal = true;
        }
#endif
        return;
    }

    auto& window = Application::Get().GetWindow();
    u32 screenW = window.GetWidth();
    u32 screenH = window.GetHeight();

    // ── 确保绘制到默认帧缓冲 + 清屏 ─────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screenW, screenH);
    glClearColor(0.05f, 0.08f, 0.05f, 1.0f);  // 深绿色背景
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    SpriteBatch::Begin(screenW, screenH);

    RenderTilemap();
    RenderEntities();
    if (m_BuildingSys->IsInBuildMode()) RenderBuildPreview();
    if (m_TimeSys && m_TimeSys->IsNight()) RenderNightOverlay();
    RenderHUD();

    SpriteBatch::End();
}

// ── 程序化地图渲染 ──────────────────────────────────────────

void GameLayer::RenderTilemap() {
    // LDtk 地图优先
    if (m_UseLdtk) {
        RenderLdtkMap();
        return;
    }

    auto& tilemap = m_GameMap.GetTilemap();
    auto vp = GetViewport();

    // 可见范围 (裁剪)
    i32 startX = std::max(0, (i32)std::floor(vp.CamPos.x));
    i32 startY = std::max(0, (i32)std::floor(vp.CamPos.y));
    i32 endX = std::min((i32)tilemap.GetWidth(), (i32)std::ceil(vp.CamPos.x + vp.ViewW) + 1);
    i32 endY = std::min((i32)tilemap.GetHeight(), (i32)std::ceil(vp.CamPos.y + vp.ViewH) + 1);

    f32 tileScreenW = vp.TileScreenW;
    f32 tileScreenH = vp.TileScreenH;
    f32 screenH = vp.ScreenH;
    glm::vec2 camPos = vp.CamPos;

    // ── 地面层 — Autotile 渲染 ──────────────────────────────
    i32 mapW = (i32)tilemap.GetWidth();
    i32 mapH = (i32)tilemap.GetHeight();
    for (i32 y = startY; y < endY; y++) {
        for (i32 x = startX; x < endX; x++) {
            auto& tile = tilemap.GetTile(0, x, y);
            if (tile.TileID == 0) continue;

            // 像素完美对齐
            f32 sx = std::round((x - camPos.x) * tileScreenW);
            f32 sy = std::round(screenH - (y - camPos.y + 1) * tileScreenH);
            f32 tw = std::round((x + 1 - camPos.x) * tileScreenW) - sx;
            f32 th = std::round(screenH - (y - camPos.y) * tileScreenH) - sy;
            glm::vec2 drawPos = {sx, sy};
            glm::vec2 drawSize = {tw, th};

            // 4-bit Bitmask: 上1 右2 下4 左8
            u16 nUp    = (y > 0)        ? tilemap.GetTile(0, x, y - 1).TileID : 0;
            u16 nRight = (x < mapW - 1) ? tilemap.GetTile(0, x + 1, y).TileID : 0;
            u16 nDown  = (y < mapH - 1) ? tilemap.GetTile(0, x, y + 1).TileID : 0;
            u16 nLeft  = (x > 0)        ? tilemap.GetTile(0, x - 1, y).TileID : 0;
            u8 mask = AutotileSet::CalcBitmask(tile.TileID, nUp, nRight, nDown, nLeft);

            // 选择纹理和 AutotileSet
            Ref<Texture2D> tex = nullptr;
            AutotileSet* autoSet = nullptr;

            switch (tile.TileID) {
                case 1:  tex = m_TexGrass;     autoSet = &m_AutoGrass;  break;
                case 2:  tex = m_TexDirt;      break;  // 泥土: 无 autotile
                case 3:  tex = m_TexRockWall;  autoSet = &m_AutoRock;   break;
                case 4:  tex = m_TexWater;     autoSet = &m_AutoWater;  break;
                case 5:  tex = m_TexSand;      autoSet = &m_AutoSand;   break;
                default: break;
            }

            if (tex && tex->IsValid() && autoSet) {
                glm::vec4 uv = autoSet->GetUV(mask);
                SpriteBatch::Draw(tex, drawPos, drawSize, uv, 0.0f);
            } else if (tex && tex->IsValid()) {
                SpriteBatch::Draw(tex, drawPos, drawSize,
                                  {0.0f, 0.0f, 1.0f, 1.0f}, 0.0f);
            } else {
                glm::vec4 color = GetTileColor(tile.TileID);
                SpriteBatch::DrawRect(drawPos, drawSize, color);
            }
        }
    }

    // ── 装饰层 (layer 1) ────────────────────────────────────
    for (i32 y = startY; y < endY; y++) {
        for (i32 x = startX; x < endX; x++) {
            auto& tile = tilemap.GetTile(1, x, y);
            if (tile.TileID == 0) continue;

            f32 sx = (x - camPos.x) * tileScreenW;
            f32 sy = screenH - (y - camPos.y + 1) * tileScreenH;

            glm::vec4 decorColor;
            switch (tile.TileID) {
                case 20: decorColor = {0.35f, 0.55f, 0.28f, 0.6f}; break;
                case 21: decorColor = {0.85f, 0.45f, 0.55f, 0.7f}; break;
                case 22: decorColor = {0.50f, 0.48f, 0.45f, 0.5f}; break;
                default: decorColor = {0.5f, 0.5f, 0.5f, 0.4f}; break;
            }
            f32 decorScale = 0.4f;
            f32 dw = tileScreenW * decorScale;
            f32 dh = tileScreenH * decorScale;
            f32 ox = (tileScreenW - dw) * 0.5f;
            f32 oy = (tileScreenH - dh) * 0.5f;
            SpriteBatch::DrawRect({sx + ox, sy + oy}, {dw, dh}, decorColor);
        }
    }

    // ── 障碍物层 (layer 2) — Y 轴排序 ──────────────────────
    for (i32 y = startY; y < endY; y++) {
        for (i32 x = startX; x < endX; x++) {
            auto& tile = tilemap.GetTile(2, x, y);
            if (tile.TileID == 0) continue;

            glm::vec4 color = GetTileColor(tile.TileID);
            f32 sx = (x - camPos.x) * tileScreenW;
            f32 sy = screenH - (y - camPos.y + 1) * tileScreenH;

            Ref<Texture2D> tex = nullptr;
            glm::vec4 objUv = {0.0f, 0.0f, 1.0f, 1.0f};
            switch (tile.TileID) {
                case 12:
                    tex = m_TexFence;
                    objUv = {0.0f, 0.0f, 0.25f, 0.75f};
                    break;
                case 13:
                    tex = m_TexRockWall;
                    objUv = {16.0f/176.0f, 16.0f/80.0f, 32.0f/176.0f, 32.0f/80.0f};
                    break;
                default: break;
            }

            if (tex && tex->IsValid()) {
                SpriteBatch::Draw(tex, {sx, sy}, {tileScreenW + 1, tileScreenH + 1}, objUv, 0.0f);
            } else {
                SpriteBatch::DrawRect({sx, sy}, {tileScreenW + 1, tileScreenH + 1}, color);
            }
        }
    }
}

// ── 实体渲染 ────────────────────────────────────────────────

void GameLayer::RenderEntities() {
    auto& world = m_Scene->GetWorld();
    auto vp = GetViewport();
    glm::vec2 camPos = vp.CamPos;
    f32 screenW = vp.ScreenW;
    f32 screenH = vp.ScreenH;
    f32 tileScreenW = vp.TileScreenW;
    f32 tileScreenH = vp.TileScreenH;

    // 渲染建筑
    world.ForEach<BuildableComponent>([&](Entity e, BuildableComponent& bld) {
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) return;

        f32 sx = (tr->X - camPos.x - bld.Size.x * 0.5f) * tileScreenW;
        f32 sy = screenH - (tr->Y - camPos.y + bld.Size.y * 0.5f) * tileScreenH;
        f32 w = bld.Size.x * tileScreenW;
        f32 h = bld.Size.y * tileScreenH;

        Ref<Texture2D> tex = nullptr;
        switch (bld.Type) {
            case BuildingType::WoodWall:
            case BuildingType::StoneWall:
            case BuildingType::WoodDoor:
                tex = m_TexRockWall;
                break;
            case BuildingType::Spike:
            case BuildingType::Barricade:
                tex = m_TexFence;
                break;
            case BuildingType::Campfire:
                tex = m_TexFireWall;
                break;
            default: break;
        }

        glm::vec4 color;
        switch (bld.Type) {
            case BuildingType::WoodWall:  color = {0.55f, 0.35f, 0.15f, 1.0f}; break;
            case BuildingType::StoneWall: color = {0.6f, 0.6f, 0.6f, 1.0f}; break;
            case BuildingType::WoodDoor:  color = {0.72f, 0.53f, 0.04f, 1.0f}; break;
            case BuildingType::Spike:     color = {0.7f, 0.7f, 0.75f, 1.0f}; break;
            case BuildingType::Barricade: color = {0.5f, 0.3f, 0.1f, 1.0f}; break;
            case BuildingType::Campfire:  color = {1.0f, 0.5f, 0.0f, 1.0f}; break;
            case BuildingType::Workbench: color = {0.4f, 0.3f, 0.2f, 1.0f}; break;
            default: color = {0.5f, 0.5f, 0.5f, 1.0f};
        }

        f32 hpRatio = bld.HP / bld.MaxHP;
        color *= (0.5f + 0.5f * hpRatio);
        color.a = 1.0f;

        if (tex && tex->IsValid()) {
            if (bld.Type == BuildingType::Campfire) {
                i32 fireFrame = (i32)(m_AnimTimer * 6.0f) % 5;
                f32 fW = 1.0f / 5.0f;
                glm::vec4 fireUv = {fireFrame * fW, 0.0f, (fireFrame + 1) * fW, 1.0f};
                SpriteBatch::Draw(tex, {sx, sy}, {w, h}, fireUv, 0.0f, color);
            } else {
                SpriteBatch::Draw(tex, {sx, sy}, {w, h}, 0.0f, color);
            }
        } else {
            SpriteBatch::DrawRect({sx, sy}, {w, h}, color);
        }
    });

    // 渲染可搜刮点
    world.ForEach<LootableComponent>([&](Entity e, LootableComponent& loot) {
        if (loot.Looted) return;
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) return;

        f32 sx = (tr->X - camPos.x - 0.4f) * tileScreenW;
        f32 sy = screenH - (tr->Y - camPos.y + 0.4f) * tileScreenH;
        f32 w = 0.8f * tileScreenW;
        f32 h = 0.8f * tileScreenH;

        if (m_TexItems && m_TexItems->IsValid()) {
            SpriteBatch::Draw(m_TexItems, {sx, sy}, {w, h});
        } else {
            SpriteBatch::DrawRect({sx, sy}, {w, h}, {0.9f, 0.8f, 0.2f, 1.0f});
        }
    });

    // 渲染掉落物
    world.ForEach<LootDropComponent>([&](Entity e, LootDropComponent& loot) {
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) return;

        f32 sx = (tr->X - camPos.x - 0.2f) * tileScreenW;
        f32 sy = screenH - (tr->Y - camPos.y + 0.2f) * tileScreenH;
        f32 w = 0.4f * tileScreenW;
        f32 h = 0.4f * tileScreenH;

        SpriteBatch::DrawRect({sx, sy}, {w, h}, {1.0f, 1.0f, 0.0f, 0.8f});
    });

    // 渲染丧尸
    world.ForEach<ZombieComponent>([&](Entity e, ZombieComponent& zombie) {
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) return;

        f32 size = 0.8f;
        if (zombie.Type == ZombieType::Tank) size = 1.3f;
        else if (zombie.Type == ZombieType::Runner) size = 0.7f;

        f32 sx = (tr->X - camPos.x - size * 0.5f) * tileScreenW;
        f32 sy = screenH - (tr->Y - camPos.y + size * 0.5f) * tileScreenH;
        f32 w = size * tileScreenW;
        f32 h = size * tileScreenH;

        glm::vec4 color;
        switch (zombie.Type) {
            case ZombieType::Walker: color = {0.29f, 0.42f, 0.23f, 1.0f}; break;
            case ZombieType::Runner: color = {0.55f, 0.27f, 0.07f, 1.0f}; break;
            case ZombieType::Tank:   color = {0.29f, 0.0f, 0.51f, 1.0f}; break;
        }

        if (m_TexSlime && m_TexSlime->IsValid()) {
            f32 row = 0.0f;
            switch (zombie.Type) {
                case ZombieType::Walker: row = 0.0f;  break;
                case ZombieType::Runner: row = 0.25f; break;
                case ZombieType::Tank:   row = 0.5f;  break;
            }
            i32 frame = (i32)(m_AnimTimer * 5.0f) % 6;
            f32 colW = 32.0f / 192.0f;
            f32 rowH = 48.0f / 192.0f;
            glm::vec4 uv = {frame * colW, row, (frame + 1) * colW, row + rowH};
            SpriteBatch::Draw(m_TexSlime, {sx, sy}, {w, h}, uv, 0.0f);
        } else {
            SpriteBatch::DrawRect({sx, sy}, {w, h}, color);
        }

        // 血条
        auto* hp = world.GetComponent<HealthComponent>(e);
        if (hp && hp->Current < hp->Max) {
            f32 barW = w;
            f32 barH = 3.0f;
            f32 ratio = hp->Current / hp->Max;
            SpriteBatch::DrawRect({sx, sy - 5}, {barW, barH}, {0.2f, 0.2f, 0.2f, 0.8f});
            SpriteBatch::DrawRect({sx, sy - 5}, {barW * ratio, barH}, {1, 0, 0, 0.9f});
        }
    });

    // 渲染玩家
    auto* ptr = world.GetComponent<TransformComponent>(m_Player);
    if (ptr) {
        f32 size = 0.8f;
        f32 sx = (ptr->X - camPos.x - size * 0.5f) * tileScreenW;
        f32 sy = screenH - (ptr->Y - camPos.y + size * 0.5f) * tileScreenH;
        f32 w = size * tileScreenW;
        f32 h = size * tileScreenH;

        if (m_TexPlayer && m_TexPlayer->IsValid()) {
            auto* player = m_Scene->GetWorld().GetComponent<PlayerComponent>(m_Player);
            i32 frame = 0;
            if (player && player->IsMoving) {
                frame = (i32)(m_AnimTimer * 8.0f) % 8;
            }
            f32 frameW = 32.0f / 384.0f;
            glm::vec4 uv = {frame * frameW, 0.0f, (frame + 1) * frameW, 1.0f};
            SpriteBatch::Draw(m_TexPlayer, {sx, sy}, {w, h}, uv, 0.0f);
        } else {
            SpriteBatch::DrawRect({sx, sy}, {w, h}, {0.9f, 0.9f, 0.95f, 1.0f});
        }

        // 攻击指示器
        auto* combat = world.GetComponent<CombatComponent>(m_Player);
        if (combat && combat->IsAttacking) {
            f32 dir = ptr->RotZ;
            f32 ax = ptr->X + std::cos(dir) * 0.8f;
            f32 ay = ptr->Y + std::sin(dir) * 0.8f;
            f32 asx = (ax - camPos.x - 0.2f) * tileScreenW;
            f32 asy = screenH - (ay - camPos.y + 0.2f) * tileScreenH;
            SpriteBatch::DrawRect({asx, asy}, {0.4f * tileScreenW, 0.4f * tileScreenH},
                                   {1.0f, 0.8f, 0.2f, 0.9f});
        }
    }
}

// ── 建造预览 ────────────────────────────────────────────────

void GameLayer::RenderBuildPreview() {
    auto vp = GetViewport();
    glm::vec2 camPos = vp.CamPos;
    f32 screenW = vp.ScreenW;
    f32 screenH = vp.ScreenH;
    f32 tileScreenW = vp.TileScreenW;
    f32 tileScreenH = vp.TileScreenH;

    glm::vec2 pos = m_BuildingSys->GetPreviewPosition();
    auto preset = GetBuildingPreset(m_BuildingSys->GetBuildType());

    f32 sx = (pos.x - camPos.x - preset.Size.x * 0.5f) * tileScreenW;
    f32 sy = screenH - (pos.y - camPos.y + preset.Size.y * 0.5f) * tileScreenH;
    f32 w = preset.Size.x * tileScreenW;
    f32 h = preset.Size.y * tileScreenH;

    bool canPlace = m_BuildingSys->CanPlace(m_Scene->GetWorld(), pos, preset.Size);
    glm::vec4 color = canPlace ?  glm::vec4{0.3f, 0.8f, 0.3f, 0.5f}
                               : glm::vec4{0.8f, 0.3f, 0.3f, 0.5f};

    SpriteBatch::DrawRect({sx, sy}, {w, h}, color);
}

// ── 夜晚覆盖 ────────────────────────────────────────────────

void GameLayer::RenderNightOverlay() {
    auto& window = Application::Get().GetWindow();
    f32 screenW = (f32)window.GetWidth();
    f32 screenH = (f32)window.GetHeight();

    f32 darkness = 0.0f;
    if (m_TimeSys) {
        u32 hour = m_TimeSys->GetHour();
        if (hour >= 18 && hour < 20) darkness = (hour - 18) / 2.0f * 0.5f;
        else if (hour >= 20 || hour < 4) darkness = 0.5f;
        else if (hour >= 4 && hour < 6) darkness = (6 - hour) / 2.0f * 0.5f;
    }

    if (darkness > 0) {
        SpriteBatch::DrawRect({0, 0}, {screenW, screenH},
                               {0.05f, 0.02f, 0.1f, darkness});
    }
}

// ── HUD ─────────────────────────────────────────────────────

void GameLayer::RenderHUD() {
    auto& world = m_Scene->GetWorld();
    auto& window = Application::Get().GetWindow();
    f32 screenW = (f32)window.GetWidth();
    f32 screenH = (f32)window.GetHeight();

    // ── 血量条 (左上) ─────────────────────────────────────
    auto* hp = world.GetComponent<HealthComponent>(m_Player);
    if (hp) {
        f32 barW = 200.0f, barH = 16.0f;
        f32 bx = 10, by = 10;
        f32 ratio = hp->Current / hp->Max;
        SpriteBatch::DrawRect({bx, by}, {barW, barH}, {0.2f, 0.2f, 0.2f, 0.8f});
        SpriteBatch::DrawRect({bx, by}, {barW * ratio, barH}, {0.8f, 0.1f, 0.1f, 0.9f});
    }

    // ── 饥饿/口渴 ─────────────────────────────────────────
    auto* surv = world.GetComponent<SurvivalComponent>(m_Player);
    if (surv) {
        f32 barW = 150.0f, barH = 10.0f;
        f32 hungerRatio = surv->Hunger / surv->MaxHunger;
        SpriteBatch::DrawRect({10, 32}, {barW, barH}, {0.2f, 0.2f, 0.2f, 0.7f});
        SpriteBatch::DrawRect({10, 32}, {barW * hungerRatio, barH},
                               {0.85f, 0.55f, 0.1f, 0.9f});
        f32 thirstRatio = surv->Thirst / surv->MaxThirst;
        SpriteBatch::DrawRect({10, 46}, {barW, barH}, {0.2f, 0.2f, 0.2f, 0.7f});
        SpriteBatch::DrawRect({10, 46}, {barW * thirstRatio, barH},
                               {0.2f, 0.5f, 0.9f, 0.9f});
    }

    // ── 快捷栏 (底部中间) ────────────────────────────────
    auto* inv = world.GetComponent<InventoryComponent>(m_Player);
    if (inv) {
        f32 slotSize = 48.0f;
        f32 gap = 4.0f;
        u32 slotCount = std::min(inv->HotbarSize, 5u);
        f32 totalW = slotCount * (slotSize + gap) - gap;
        f32 startX = (screenW - totalW) * 0.5f;
        f32 startY = screenH - slotSize - 10;

        for (u32 i = 0; i < slotCount; i++) {
            f32 sx = startX + i * (slotSize + gap);
            bool selected = (i == inv->SelectedSlot);

            glm::vec4 bg = selected ? glm::vec4{0.4f, 0.4f, 0.5f, 0.9f}
                                    : glm::vec4{0.2f, 0.2f, 0.25f, 0.7f};
            SpriteBatch::DrawRect({sx, startY}, {slotSize, slotSize}, bg);

            if (i < inv->Slots.size() && !inv->Slots[i].IsEmpty()) {
                glm::vec4 itemColor = {0.7f, 0.7f, 0.7f, 1.0f};
                auto* def = ItemDatabase::Get().Find(inv->Slots[i].ItemID);
                if (def) {
                    if (def->Category == ItemCategory::Resource)
                        itemColor = {0.55f, 0.35f, 0.15f, 1.0f};
                    else if (def->Category == ItemCategory::Food)
                        itemColor = {0.2f, 0.8f, 0.3f, 1.0f};
                    else if (def->Category == ItemCategory::Tool)
                        itemColor = {0.7f, 0.7f, 0.8f, 1.0f};
                }
                SpriteBatch::DrawRect({sx + 6, startY + 6},
                                       {slotSize - 12, slotSize - 12}, itemColor);
            }

            if (selected) {
                SpriteBatch::DrawRect({sx - 2, startY - 2},
                                       {slotSize + 4, 2}, {1, 1, 1, 0.8f});
                SpriteBatch::DrawRect({sx - 2, startY + slotSize},
                                       {slotSize + 4, 2}, {1, 1, 1, 0.8f});
            }
        }
    }

    // ── 时间/天数 (右上) ──────────────────────────────────
    if (m_TimeSys) {
        f32 bx = screenW - 120;
        bool isNight = m_TimeSys->IsNight();
        glm::vec4 timeBg = isNight ? glm::vec4{0.1f, 0.1f, 0.3f, 0.8f}
                                   : glm::vec4{0.3f, 0.3f, 0.1f, 0.8f};
        SpriteBatch::DrawRect({bx, 10}, {110, 40}, timeBg);
    }

    // ── 击杀数 + 波数 ─────────────────────────────────────
    SpriteBatch::DrawRect({screenW - 120, 56}, {110, 20},
                           {0.2f, 0.2f, 0.2f, 0.7f});

    // ── 建造模式提示 ─────────────────────────────────────
    if (m_BuildingSys->IsInBuildMode()) {
        SpriteBatch::DrawRect({screenW * 0.5f - 100, 10}, {200, 30},
                               {0.2f, 0.5f, 0.2f, 0.8f});
    }

    // ── 游戏结束 ─────────────────────────────────────────
    if (m_GameOver) {
        SpriteBatch::DrawRect({screenW * 0.5f - 150, screenH * 0.5f - 30},
                               {300, 60}, {0.8f, 0.1f, 0.1f, 0.9f});
    }
}

} // namespace Engine
