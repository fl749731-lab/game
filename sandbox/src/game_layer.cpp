#include "game_layer.h"
#include "engine/core/application.h"
#include "engine/core/random.h"
#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/renderer/sprite_batch.h"
#include "engine/platform/input.h"
#include "engine/game2d/collision2d.h"

#include <glad/glad.h>

#include <cmath>
#include <sstream>
#include <algorithm>

namespace Engine {

// ── 视口计算辅助 — 消除所有重复的相机坐标计算 ───────────

CameraViewport GameLayer::GetViewport() const {
    CameraViewport vp;
    glm::vec2 center = m_CamCtrl.GetPosition();
    f32 zoom = m_CamCtrl.GetZoom();
    vp.ViewW = BASE_VIEW_WIDTH / zoom;
    vp.ViewH = BASE_VIEW_HEIGHT / zoom;
    vp.CamPos = center - glm::vec2(vp.ViewW * 0.5f, vp.ViewH * 0.5f);

    auto& window = Application::Get().GetWindow();
    vp.ScreenW = (f32)window.GetWidth();
    vp.ScreenH = (f32)window.GetHeight();
    vp.TileScreenW = vp.ScreenW / vp.ViewW;
    vp.TileScreenH = vp.ScreenH / vp.ViewH;
    return vp;
}

// ══════════════════════════════════════════════════════════════
//  初始化 / 清理
// ══════════════════════════════════════════════════════════════

void GameLayer::OnAttach() {
    // 切换鼠标模式为正常 (不锁定)
    Input::SetCursorMode(CursorMode::Normal);

    // ── 创建场景 ─────────────────────────────────────────
    m_Scene = std::make_shared<Scene>("ZombieSurvival");
    auto& world = m_Scene->GetWorld();

    // ── 生成地图 (60×60) ─────────────────────────────────
    m_GameMap.Generate(60, 60);

    // ── 注册物品 ─────────────────────────────────────────
    RegisterItems();

    // ── 创建系统 (通过 ECSWorld::AddSystem 模板) ─────────
    auto& combatSys   = world.AddSystem<CombatSystem>();
    auto& zombieSys   = world.AddSystem<ZombieSystem>();
    auto& buildingSys = world.AddSystem<BuildingSystem>();
    auto& timeSys     = world.AddSystem<GameTimeSystem>();
    world.AddSystem<PlayerControlSystem>();

    // 配置系统
    zombieSys.SetNavGrid(&m_GameMap.GetNavGrid());
    buildingSys.SetNavGrid(&m_GameMap.GetNavGrid());
    timeSys.SetTimeScale(10.0f);   // 加速: 10 游戏分钟/秒

    // 保存指针
    m_CombatSys   = &combatSys;
    m_ZombieSys   = &zombieSys;
    m_BuildingSys = &buildingSys;
    m_TimeSys     = &timeSys;

    // ── 注册建造配方 (必须在 m_BuildingSys 初始化之后) ────
    RegisterRecipes();

    // ── 尝试加载 LDtk 地图 ─────────────────────────
    if (LoadLdtkMap("assets/maps/world.ldtk")) {
        LOG_INFO("[GameLayer] LDtk 地图加载成功, 使用 LDtk 渲染");
    } else {
        LOG_INFO("[GameLayer] LDtk 地图未找到, 使用程序化地图");
    }

    // ── 创建玩家 ─────────────────────────────────────────
    SetupPlayer();

    // ── 初始丧尸 ─────────────────────────────────────────
    SpawnInitialZombies();

    // ── 可搜刮物资点 ────────────────────────────────────
    SetupLootPoints();

    // ── 相机 ─────────────────────────────────────────────
    m_CamCtrl = Camera2DController(BASE_VIEW_WIDTH, BASE_VIEW_HEIGHT);
    m_CamCtrl.SetSmoothness(6.0f);
    m_CamCtrl.SetWorldBounds({0,0},
        {(f32)m_GameMap.GetWidth(), (f32)m_GameMap.GetHeight()});

    // 刷新器配置
    m_Spawner.SetWaveInterval(25.0f);

    // ── 加载贴图 ──────────────────────────────────────────
    auto loadTex = [](const std::string& path) -> Ref<Texture2D> {
        auto tex = std::make_shared<Texture2D>(path);
        if (tex->IsValid()) {
            tex->SetFilterNearest();  // Pixel Art: 最近邻采样
            LOG_INFO("[GameLayer] 加载贴图: %s (%ux%u)", path.c_str(), tex->GetWidth(), tex->GetHeight());
        } else {
            LOG_WARN("[GameLayer] 贴图加载失败: %s", path.c_str());
        }
        return tex;
    };
    m_TexGrass    = loadTex("assets/textures/tiles/GrassTile.png");
    m_TexGrassWall= loadTex("assets/textures/tiles/GrassWall.png");
    m_TexDirt     = loadTex("assets/textures/tiles/DirtTile.png");
    m_TexSand     = loadTex("assets/textures/tiles/SandTile.png");
    m_TexRockWall = loadTex("assets/textures/tiles/RockWall.png");
    m_TexFence    = loadTex("assets/textures/tiles/fence.png");
    m_TexPlayer   = loadTex("assets/textures/characters/Player Sprite-export.png");
    m_TexSlime    = loadTex("assets/textures/characters/Slime.png");
    m_TexItems    = loadTex("assets/textures/objects/Items.png");
    m_TexFireWall = loadTex("assets/textures/objects/FireWall.png");

    m_TexWater    = loadTex("assets/textures/tiles/WaterTile.png");

    // ── 初始化 AutotileSet ───────────────────────────────
    m_AutoGrass     = CreateStandardAutotile(176.0f, 80.0f);
    m_AutoGrassWall = CreateStandardAutotile(176.0f, 80.0f);
    m_AutoSand      = CreateStandardAutotile(176.0f, 80.0f);
    m_AutoRock      = CreateStandardAutotile(176.0f, 80.0f);
    m_AutoWater     = CreateStandardAutotile(176.0f, 80.0f);

    LOG_INFO("[GameLayer] 丧尸生存原型启动!");
}

void GameLayer::OnDetach() {
    m_Scene.reset();
}

// ══════════════════════════════════════════════════════════════
//  物品 / 配方 注册
// ══════════════════════════════════════════════════════════════

void GameLayer::RegisterItems() {
    auto& db = ItemDatabase::Get();

    // 资源
    db.Register({1, "木材", "建造用", "", 0, ItemCategory::Resource, 99, 5, 0, 0});
    db.Register({2, "石头", "建造用", "", 0, ItemCategory::Resource, 99, 8, 0, 0});
    db.Register({3, "铁片", "稀有资源", "", 0, ItemCategory::Resource, 50, 15, 0, 0});
    db.Register({4, "布料", "制作绷带", "", 0, ItemCategory::Resource, 50, 5, 0, 0});

    // 食物
    db.Register({10, "罐头", "恢复30饥饿", "", 0, ItemCategory::Food, 10, 20, 0, 30.0f});
    db.Register({11, "水瓶", "恢复40口渴", "", 0, ItemCategory::Food, 10, 15, 0, 0});
    db.Register({12, "急救包", "恢复50血量", "", 0, ItemCategory::Food, 5, 50, 0, 0});

    // 武器
    db.Register({20, "木棒", "基础武器", "", 0, ItemCategory::Tool, 1, 0, 0, 0});
    db.Register({21, "铁管", "中等武器", "", 0, ItemCategory::Tool, 1, 0, 0, 0});
    db.Register({22, "斧头", "伤害高", "", 0, ItemCategory::Tool, 1, 0, 0, 0});
}

void GameLayer::RegisterRecipes() {
    // 木墙: 5 木材
    m_BuildingSys->RegisterRecipe({BuildingType::WoodWall, {{1, 5}}});
    // 石墙: 8 石头
    m_BuildingSys->RegisterRecipe({BuildingType::StoneWall, {{2, 8}}});
    // 木门: 3 木材
    m_BuildingSys->RegisterRecipe({BuildingType::WoodDoor, {{1, 3}}});
    // 地刺: 3 铁片 + 2 木材
    m_BuildingSys->RegisterRecipe({BuildingType::Spike, {{3, 3}, {1, 2}}});
    // 路障: 4 木材
    m_BuildingSys->RegisterRecipe({BuildingType::Barricade, {{1, 4}}});
    // 营火: 5 木材
    m_BuildingSys->RegisterRecipe({BuildingType::Campfire, {{1, 5}}});
    // 工作台: 10 木材 + 5 石头
    m_BuildingSys->RegisterRecipe({BuildingType::Workbench, {{1, 10}, {2, 5}}});
}

// ══════════════════════════════════════════════════════════════
//  场景搭建
// ══════════════════════════════════════════════════════════════

void GameLayer::SetupPlayer() {
    auto& world = m_Scene->GetWorld();
    glm::vec2 spawn = m_GameMap.GetPlayerSpawn();

    m_Player = world.CreateEntity("Player");

    auto& tr = world.AddComponent<TransformComponent>(m_Player);
    tr.X = spawn.x;
    tr.Y = spawn.y;
    tr.ScaleX = tr.ScaleY = tr.ScaleZ = 0.8f;

    auto& hp = world.AddComponent<HealthComponent>(m_Player);
    hp.Max = 100.0f;
    hp.Current = 100.0f;

    auto& player = world.AddComponent<PlayerComponent>(m_Player);
    player.MoveSpeed = 4.0f;
    player.MaxStamina = 100.0f;
    player.Stamina = 100.0f;

    auto& combat = world.AddComponent<CombatComponent>(m_Player);
    combat.AttackDamage = 15.0f;
    combat.AttackRange = 1.5f;
    combat.AttackCooldown = 0.4f;
    combat.KnockbackForce = 4.0f;

    auto& survival = world.AddComponent<SurvivalComponent>(m_Player);
    survival.Hunger = 100.0f;
    survival.Thirst = 100.0f;

    auto& inv = world.AddComponent<InventoryComponent>(m_Player);
    inv.Init(20);
    // 初始物资
    inv.AddItem(1, 20);   // 20 木材
    inv.AddItem(2, 10);   // 10 石头
    inv.AddItem(10, 3);   // 3 罐头
    inv.AddItem(11, 3);   // 3 水瓶
    inv.AddItem(20, 1);   // 1 木棒

    m_ZombieSys->SetPlayerEntity(m_Player);
}

void GameLayer::SpawnInitialZombies() {
    auto& world = m_Scene->GetWorld();
    auto& spawns = m_GameMap.GetZombieSpawnPoints();

    for (u32 i = 0; i < std::min((u32)spawns.size(), 5u); i++) {
        m_ZombieSys->SpawnZombie(world, spawns[i], ZombieType::Walker);
    }
}

void GameLayer::SetupLootPoints() {
    auto& world = m_Scene->GetWorld();
    auto& lootPts = m_GameMap.GetLootPoints();

    for (auto& pos : lootPts) {
        Entity e = world.CreateEntity("LootCrate");
        auto& tr = world.AddComponent<TransformComponent>(e);
        tr.X = pos.x;
        tr.Y = pos.y;
        tr.ScaleX = tr.ScaleY = tr.ScaleZ = 0.9f;

        auto& lootable = world.AddComponent<LootableComponent>(e);
        lootable.LootTable = {{1, 5}, {2, 3}, {10, 1}};  // 木材+石头+罐头

        auto& hp = world.AddComponent<HealthComponent>(e);
        hp.Max = 1.0f;
        hp.Current = 1.0f;  // 不可被攻击摧毁
    }
}

// ══════════════════════════════════════════════════════════════
//  更新
// ══════════════════════════════════════════════════════════════

void GameLayer::OnUpdate(f32 dt) {
    if (m_GameOver) return;

    m_AnimTimer += dt;  // 累加动画计时器

    HandleInput(dt);

    // 更新所有 ECS 系统
    m_Scene->Update(dt);

    // 生存要素
    UpdateSurvival(dt);

    // 丧尸刷新
    UpdateZombieSpawning(dt);

    // 清理死亡丧尸
    CleanupDeadZombies();

    // ── 实体碰撞推挤 ─────────────────────────────────────
    {
        auto& world = m_Scene->GetWorld();
        auto* playerTr = world.GetComponent<TransformComponent>(m_Player);
        if (playerTr) {
            const f32 playerRadius = 0.35f;
            const f32 zombieRadius = 0.35f;

            world.ForEach<ZombieComponent>([&](Entity e, ZombieComponent& zombie) {
                auto* zTr = world.GetComponent<TransformComponent>(e);
                if (!zTr) return;

                f32 zRadius = zombieRadius;
                if (zombie.Type == ZombieType::Tank) zRadius = 0.55f;

                glm::vec2 push = Collision2D::CirclePush(
                    {playerTr->X, playerTr->Y}, playerRadius,
                    {zTr->X, zTr->Y}, zRadius);

                if (push.x != 0 || push.y != 0) {
                    // 推开丧尸 70%, 推开玩家 30%
                    zTr->X += push.x * 0.7f;
                    zTr->Y += push.y * 0.7f;
                    playerTr->X -= push.x * 0.3f;
                    playerTr->Y -= push.y * 0.3f;
                }
            });
        }
    }

    // 检查玩家死亡
    if (m_CombatSys->IsDead(m_Scene->GetWorld(), m_Player)) {
        m_GameOver = true;
        LOG_INFO("[GameLayer] 游戏结束! 存活 %u 天, 击杀 %u 只丧尸",
                  m_DayCount, m_KillCount);
    }

    // 更新相机
    auto* ptr = m_Scene->GetWorld().GetComponent<TransformComponent>(m_Player);
    if (ptr) {
        m_CamCtrl.Update(dt, {ptr->X, ptr->Y});
    }
}

void GameLayer::HandleInput(f32 dt) {
    auto& world = m_Scene->GetWorld();
    auto* ptr = world.GetComponent<TransformComponent>(m_Player);
    auto* player = world.GetComponent<PlayerComponent>(m_Player);
    if (!ptr || !player) return;

    // ── 移动 (WASD) ─────────────────────────────────────
    glm::vec2 moveDir = {0, 0};
    if (Input::IsKeyDown(Key::W)) { moveDir.y += 1; player->Facing = Direction::Up; }
    if (Input::IsKeyDown(Key::S)) { moveDir.y -= 1; player->Facing = Direction::Down; }
    if (Input::IsKeyDown(Key::A)) { moveDir.x -= 1; player->Facing = Direction::Left; }
    if (Input::IsKeyDown(Key::D)) { moveDir.x += 1; player->Facing = Direction::Right; }

    if (moveDir.x != 0 || moveDir.y != 0) {
        f32 len = std::sqrt(moveDir.x * moveDir.x + moveDir.y * moveDir.y);
        moveDir /= len;

        f32 newX = ptr->X + moveDir.x * player->MoveSpeed * dt;
        f32 newY = ptr->Y + moveDir.y * player->MoveSpeed * dt;

        // 分轴 AABB 碰撞检测 (贴墙可滑动)
        auto& tilemap = m_GameMap.GetTilemap();
        const glm::vec2 halfSize = {0.3f, 0.3f};  // 玩家碰撞体半尺寸
        glm::vec2 resolved = Collision2D::MoveAndSlide(
            tilemap, {ptr->X, ptr->Y}, {newX, newY}, halfSize);
        ptr->X = resolved.x;
        ptr->Y = resolved.y;

        player->IsMoving = true;
        // 更新朝向角度 (给战斗系统用)
        ptr->RotZ = std::atan2(moveDir.y, moveDir.x);
    } else {
        player->IsMoving = false;
    }

    // ── 鼠标朝向 ─────────────────────────────────────────
    // 将鼠标屏幕坐标转换为世界方向
    auto& window = Application::Get().GetWindow();
    f32 mx = Input::GetMouseX();
    f32 my = Input::GetMouseY();
    // 屏幕中心 → 方向
    f32 cx = (f32)window.GetWidth() * 0.5f;
    f32 cy = (f32)window.GetHeight() * 0.5f;
    f32 dx = mx - cx;
    f32 dy = -(my - cy);  // 屏幕Y翻转
    if (dx != 0 || dy != 0) {
        ptr->RotZ = std::atan2(dy, dx);
    }

    // ── 攻击 (鼠标左键) ─────────────────────────────────
    if (Input::IsMouseButtonPressed(MouseButton::Left) &&
        !m_BuildingSys->IsInBuildMode()) {
        m_CombatSys->MeleeAttack(world, m_Player);
    }

    // ── 搜刮 / 拾取 (E) ─────────────────────────────────
    if (Input::IsKeyJustPressed(Key::E)) {
        glm::vec2 playerPos = {ptr->X, ptr->Y};
        world.ForEach<LootableComponent>([&](Entity e, LootableComponent& loot) {
            if (loot.Looted) return;
            auto* ltr = world.GetComponent<TransformComponent>(e);
            if (!ltr) return;

            f32 dist = std::sqrt(
                (ltr->X - playerPos.x) * (ltr->X - playerPos.x) +
                (ltr->Y - playerPos.y) * (ltr->Y - playerPos.y));
            if (dist < 1.5f) {
                auto* inv = world.GetComponent<InventoryComponent>(m_Player);
                if (inv) {
                    for (auto& [itemID, count] : loot.LootTable)
                        inv->AddItem(itemID, count);
                }
                loot.Looted = true;
                LOG_INFO("[GameLayer] 搜刮成功!");
            }
        });

        // 拾取地面掉落物
        m_CombatSys->PickupLoot(world, m_Player);
    }

    // ── 使用食物/医疗物品 (F) ─────────────────────────────
    if (Input::IsKeyJustPressed(Key::F)) {
        auto* inv = world.GetComponent<InventoryComponent>(m_Player);
        if (inv) {
            auto& selected = inv->GetSelectedItem();
            if (!selected.IsEmpty()) {
                auto* def = ItemDatabase::Get().Find(selected.ItemID);
                if (def && def->Category == ItemCategory::Food) {
                    auto* surv = world.GetComponent<SurvivalComponent>(m_Player);
                    auto* hp = world.GetComponent<HealthComponent>(m_Player);
                    if (def->StaminaRestore > 0 && surv) {
                        surv->Hunger = std::min(surv->MaxHunger,
                                                 surv->Hunger + def->StaminaRestore);
                    }
                    // 急救包恢复血量
                    if (selected.ItemID == 12 && hp) {
                        hp->Current = std::min(hp->Max, hp->Current + 50.0f);
                    }
                    // 水瓶恢复口渴
                    if (selected.ItemID == 11 && surv) {
                        surv->Thirst = std::min(surv->MaxThirst, surv->Thirst + 40.0f);
                    }
                    inv->RemoveItem(selected.ItemID, 1);
                    LOG_INFO("[GameLayer] 使用物品: %s", def->Name.c_str());
                }
            }
        }
    }

    // ── 建造模式 ─────────────────────────────────────────
    HandleBuildInput(dt);

    // ── 快捷栏 (1-5) ─────────────────────────────────────
    auto* inv = world.GetComponent<InventoryComponent>(m_Player);
    if (inv) {
        if (Input::IsKeyJustPressed(Key::Num1)) inv->SelectedSlot = 0;
        if (Input::IsKeyJustPressed(Key::Num2)) inv->SelectedSlot = 1;
        if (Input::IsKeyJustPressed(Key::Num3)) inv->SelectedSlot = 2;
        if (Input::IsKeyJustPressed(Key::Num4)) inv->SelectedSlot = 3;
        if (Input::IsKeyJustPressed(Key::Num5)) inv->SelectedSlot = 4;
    }
}

void GameLayer::HandleBuildInput(f32 dt) {
    auto& world = m_Scene->GetWorld();

    // B 键切换建造模式
    if (Input::IsKeyJustPressed(Key::B)) {
        if (m_BuildingSys->IsInBuildMode()) {
            m_BuildingSys->ExitBuildMode();
        } else {
            m_BuildingSys->EnterBuildMode((BuildingType)m_BuildSlot);
        }
    }

    // 建造模式中
    if (m_BuildingSys->IsInBuildMode()) {
        // Tab 切换建筑类型
        if (Input::IsKeyJustPressed(Key::Tab)) {
            m_BuildSlot = (m_BuildSlot + 1) % (u32)BuildingType::COUNT;
            m_BuildingSys->EnterBuildMode((BuildingType)m_BuildSlot);
        }

        // 计算鼠标对应的世界坐标
        f32 mx = Input::GetMouseX();
        f32 my = Input::GetMouseY();
        auto vp = GetViewport();
        f32 worldX = vp.CamPos.x + (mx / vp.ScreenW) * vp.ViewW;
        f32 worldY = vp.CamPos.y + (1.0f - my / vp.ScreenH) * vp.ViewH;

        // 对齐到 0.5 格
        worldX = std::floor(worldX * 2.0f) / 2.0f + 0.25f;
        worldY = std::floor(worldY * 2.0f) / 2.0f + 0.25f;

        m_BuildingSys->SetPreviewPosition({worldX, worldY});

        // 鼠标左键放置
        if (Input::IsMouseButtonPressed(MouseButton::Left)) {
            // 检查资源是否足够
            auto* inv = world.GetComponent<InventoryComponent>(m_Player);
            auto* recipe = m_BuildingSys->GetRecipe((BuildingType)m_BuildSlot);
            bool canAfford = true;
            if (recipe && inv) {
                for (auto& cost : recipe->Costs) {
                    if (!inv->HasItem(cost.ItemID, cost.Count)) {
                        canAfford = false;
                        break;
                    }
                }
                if (canAfford) {
                    Entity bld = m_BuildingSys->PlaceBuilding(world, {worldX, worldY});
                    if (bld != INVALID_ENTITY) {
                        // 扣除资源
                        for (auto& cost : recipe->Costs)
                            inv->RemoveItem(cost.ItemID, cost.Count);
                    }
                }
            }
        }

        // ESC 退出建造
        if (Input::IsKeyJustPressed(Key::Escape)) {
            m_BuildingSys->ExitBuildMode();
        }
    }

    // 缩放
    f32 scroll = Input::GetScrollOffset();
    if (scroll != 0) {
        f32 zoom = m_CamCtrl.GetZoom();
        zoom += scroll * 0.1f;
        zoom = std::clamp(zoom, 0.5f, 3.0f);
        m_CamCtrl.SetZoom(zoom);
    }
}

// ══════════════════════════════════════════════════════════════
//  生存 / 丧尸刷新
// ══════════════════════════════════════════════════════════════

void GameLayer::UpdateSurvival(f32 dt) {
    auto& world = m_Scene->GetWorld();
    auto* surv = world.GetComponent<SurvivalComponent>(m_Player);
    if (!surv) return;

    // 饥饿和口渴随时间下降
    f32 gameMinutes = m_TimeSys ? (m_TimeSys->IsPaused() ? 0 : 10.0f * dt) : dt;
    surv->Hunger -= surv->HungerRate * gameMinutes / 60.0f;
    surv->Thirst -= surv->ThirstRate * gameMinutes / 60.0f;

    surv->Hunger = std::max(0.0f, surv->Hunger);
    surv->Thirst = std::max(0.0f, surv->Thirst);

    // 持续掉血
    if (surv->Hunger <= 0 || surv->Thirst <= 0) {
        auto* hp = world.GetComponent<HealthComponent>(m_Player);
        if (hp) hp->Current -= 2.0f * dt;
    }
}

void GameLayer::UpdateZombieSpawning(f32 dt) {
    if (!m_TimeSys) return;

    bool isNight = m_TimeSys->IsNight();
    m_Spawner.Update(dt, isNight, m_DayCount);

    if (m_Spawner.ShouldSpawnWave()) {
        m_Spawner.ConsumeSpawn();
        auto& world = m_Scene->GetWorld();
        auto& spawns = m_GameMap.GetZombieSpawnPoints();

        u32 count = m_Spawner.GetSpawnCount();
        for (u32 i = 0; i < count; i++) {
            auto& pos = spawns[i % spawns.size()];
            // 随机丧尸类型
            ZombieType type = ZombieType::Walker;
            u32 roll = Random::UInt(100);
            if (roll < 10 && m_DayCount >= 3) type = ZombieType::Tank;
            else if (roll < 35) type = ZombieType::Runner;

            // 在刷新点附近随机偏移
            glm::vec2 spawnPos = pos;
            spawnPos.x += Random::Float(-2.0f, 2.0f);
            spawnPos.y += Random::Float(-2.0f, 2.0f);

            m_ZombieSys->SpawnZombie(world, spawnPos, type);
        }

        LOG_INFO("[GameLayer] 第 %u 波丧尸! 数量: %u", m_Spawner.GetWaveNumber(), count);
    }

    // 日计数
    if (m_TimeSys->GetDay() != m_LastDay) {
        m_LastDay = m_TimeSys->GetDay();
        m_DayCount = m_LastDay;
    }
}

void GameLayer::CleanupDeadZombies() {
    auto& world = m_Scene->GetWorld();
    std::vector<Entity> dead;

    world.ForEach<ZombieComponent>([&](Entity e, ZombieComponent& zombie) {
        auto* hp = world.GetComponent<HealthComponent>(e);
        if (hp && hp->Current <= 0) {
            dead.push_back(e);
        }
    });

    for (auto e : dead) {
        auto* tr = world.GetComponent<TransformComponent>(e);
        auto* zombie = world.GetComponent<ZombieComponent>(e);
        if (tr) {
            // 掉落资源
            u32 roll = Random::UInt(100);
            if (roll < 40) m_CombatSys->SpawnLoot(world, {tr->X, tr->Y}, 1, 2);  // 木材
            if (roll < 20) m_CombatSys->SpawnLoot(world, {tr->X, tr->Y}, 10, 1); // 罐头
            if (roll < 10) m_CombatSys->SpawnLoot(world, {tr->X, tr->Y}, 3, 1);  // 铁片
        }
        if (zombie) m_KillCount++;
        world.DestroyEntity(e);
    }
}

} // namespace Engine
