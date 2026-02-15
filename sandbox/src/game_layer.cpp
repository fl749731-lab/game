#include "game_layer.h"
#include "engine/renderer/sprite_batch.h"
#include "engine/platform/input.h"
#include "engine/game2d/collision2d.h"

#include <glad/glad.h>

#include <cmath>
#include <sstream>
#include <algorithm>

namespace Engine {

// ══════════════════════════════════════════════════════════════
//  Tile 颜色映射
// ══════════════════════════════════════════════════════════════

glm::vec4 GameLayer::GetTileColor(u16 tileID) {
    switch (tileID) {
        case 0:  return {0.1f, 0.1f, 0.1f, 1.0f};   // 空 — 黑
        case 1:  return {0.29f, 0.49f, 0.25f, 1.0f};  // 草地 — 深绿
        case 2:  return {0.55f, 0.41f, 0.08f, 1.0f};  // 泥土 — 棕
        case 3:  return {0.53f, 0.53f, 0.53f, 1.0f};  // 石板 — 灰
        case 4:  return {0.2f, 0.4f, 0.67f, 1.0f};    // 水 — 蓝
        case 10: return {0.18f, 0.35f, 0.15f, 1.0f};  // 树木 — 暗绿
        case 11: return {0.4f, 0.4f, 0.4f, 1.0f};     // 石头 — 灰
        case 12: return {0.55f, 0.27f, 0.07f, 1.0f};  // 栅栏 — 深棕
        case 13: return {0.63f, 0.32f, 0.18f, 1.0f};  // 墙壁 — 砖色
        default: return {0.5f, 0.5f, 0.5f, 1.0f};
    }
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

    // ── 创建玩家 ─────────────────────────────────────────
    SetupPlayer();

    // ── 初始丧尸 ─────────────────────────────────────────
    SpawnInitialZombies();

    // ── 可搜刮物资点 ────────────────────────────────────
    SetupLootPoints();

    // ── 相机 ─────────────────────────────────────────────
    m_CamCtrl = Camera2DController(20.0f, 15.0f);
    m_CamCtrl.SetSmoothness(6.0f);
    m_CamCtrl.SetWorldBounds({0,0},
        {(f32)m_GameMap.GetWidth(), (f32)m_GameMap.GetHeight()});

    // 刷新器配置
    m_Spawner.SetWaveInterval(25.0f);

    // ── 加载贴图 ──────────────────────────────────────────
    auto loadTex = [](const std::string& path) -> Ref<Texture2D> {
        auto tex = std::make_shared<Texture2D>(path);
        if (tex->IsValid()) {
            LOG_INFO("[GameLayer] 加载贴图: %s (%ux%u)", path.c_str(), tex->GetWidth(), tex->GetHeight());
        } else {
            LOG_WARN("[GameLayer] 贴图加载失败: %s", path.c_str());
        }
        return tex;
    };
    m_TexGrass    = loadTex("assets/textures/tiles/GrassTile.png");
    m_TexDirt     = loadTex("assets/textures/tiles/DirtTile.png");
    m_TexSand     = loadTex("assets/textures/tiles/SandTile.png");
    m_TexRockWall = loadTex("assets/textures/tiles/RockWall.png");
    m_TexFence    = loadTex("assets/textures/tiles/fence.png");
    m_TexPlayer   = loadTex("assets/textures/characters/Player Sprite-export.png");
    m_TexSlime    = loadTex("assets/textures/characters/Slime.png");
    m_TexItems    = loadTex("assets/textures/objects/Items.png");
    m_TexFireWall = loadTex("assets/textures/objects/FireWall.png");

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
        LOG_INFO("[GameLayer] 游戏结束! 存活 {} 天, 击杀 {} 只丧尸",
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

    // ── 搜刮 / 交互 (E) ─────────────────────────────────
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

        // 使用食物 (如果手持食物)
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
        auto& window = Application::Get().GetWindow();
        f32 mx = Input::GetMouseX();
        f32 my = Input::GetMouseY();
        f32 screenW = (f32)window.GetWidth();
        f32 screenH = (f32)window.GetHeight();

        glm::vec2 camPos = m_CamCtrl.GetPosition();
        f32 zoom = m_CamCtrl.GetZoom();
        f32 viewW = 20.0f / zoom;
        f32 viewH = 15.0f / zoom;

        f32 worldX = camPos.x + (mx / screenW) * viewW;
        f32 worldY = camPos.y + (1.0f - my / screenH) * viewH;

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
            u32 roll = std::rand() % 100;
            if (roll < 10 && m_DayCount >= 3) type = ZombieType::Tank;
            else if (roll < 35) type = ZombieType::Runner;

            // 在刷新点附近随机偏移
            glm::vec2 spawnPos = pos;
            spawnPos.x += (std::rand() % 40 - 20) * 0.1f;
            spawnPos.y += (std::rand() % 40 - 20) * 0.1f;

            m_ZombieSys->SpawnZombie(world, spawnPos, type);
        }

        LOG_INFO("[GameLayer] 第 {} 波丧尸! 数量: {}", m_Spawner.GetWaveNumber(), count);
    }

    // 日计数
    static u32 lastDay = 0;
    if (m_TimeSys->GetDay() != lastDay) {
        lastDay = m_TimeSys->GetDay();
        m_DayCount = lastDay;
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
            u32 roll = std::rand() % 100;
            if (roll < 40) m_CombatSys->SpawnLoot(world, {tr->X, tr->Y}, 1, 2);  // 木材
            if (roll < 20) m_CombatSys->SpawnLoot(world, {tr->X, tr->Y}, 10, 1); // 罐头
            if (roll < 10) m_CombatSys->SpawnLoot(world, {tr->X, tr->Y}, 3, 1);  // 铁片
        }
        if (zombie) m_KillCount++;
        world.DestroyEntity(e);
    }
}

// ══════════════════════════════════════════════════════════════
//  渲染
// ══════════════════════════════════════════════════════════════

void GameLayer::OnRender() {
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

void GameLayer::RenderTilemap() {
    auto& tilemap = m_GameMap.GetTilemap();
    glm::vec2 camCenter = m_CamCtrl.GetPosition();
    f32 zoom = m_CamCtrl.GetZoom();
    f32 viewW = 20.0f / zoom;
    f32 viewH = 15.0f / zoom;

    // 将中心坐标转换为左下角坐标
    glm::vec2 camPos = camCenter - glm::vec2(viewW * 0.5f, viewH * 0.5f);

    // 可见范围 (裁剪)
    i32 startX = std::max(0, (i32)std::floor(camPos.x));
    i32 startY = std::max(0, (i32)std::floor(camPos.y));
    i32 endX = std::min((i32)tilemap.GetWidth(), (i32)std::ceil(camPos.x + viewW) + 1);
    i32 endY = std::min((i32)tilemap.GetHeight(), (i32)std::ceil(camPos.y + viewH) + 1);

    auto& window = Application::Get().GetWindow();
    f32 screenW = (f32)window.GetWidth();
    f32 screenH = (f32)window.GetHeight();
    f32 tileScreenW = screenW / viewW;
    f32 tileScreenH = screenH / viewH;

    // 绘制地面层
    for (i32 y = startY; y < endY; y++) {
        for (i32 x = startX; x < endX; x++) {
            auto& tile = tilemap.GetTile(0, x, y);

            f32 sx = (x - camPos.x) * tileScreenW;
            f32 sy = screenH - (y - camPos.y + 1) * tileScreenH; // Y翻转
            glm::vec2 drawPos = {sx, sy};
            glm::vec2 drawSize = {tileScreenW + 1, tileScreenH + 1};

            // 根据 TileID 选择贴图
            Ref<Texture2D> tex = nullptr;
            switch (tile.TileID) {
                case 1:  tex = m_TexGrass; break;    // 草地
                case 2:  tex = m_TexDirt;  break;    // 泥土
                case 3:  tex = m_TexRockWall; break; // 石板
                case 4:  break;                       // 水 (保持纯色)
                default: break;
            }

            if (tex && tex->IsValid()) {
                // 图集类纹理: 取 autotile 中心纯色填充块
                glm::vec4 uv = {0.0f, 0.0f, 1.0f, 1.0f}; // 默认全图 (DirtTile)
                if (tile.TileID == 1) {
                    // GrassTile 176x80 autotile: 左上48x48块的中心16x16是纯草地
                    uv = {16.0f/176.0f, 16.0f/80.0f, 32.0f/176.0f, 32.0f/80.0f};
                } else if (tile.TileID == 3) {
                    // RockWall autotile: 同理取中心纯石墙块
                    uv = {16.0f/176.0f, 16.0f/80.0f, 32.0f/176.0f, 32.0f/80.0f};
                }
                SpriteBatch::Draw(tex, drawPos, drawSize, uv, 0.0f);
            } else {
                glm::vec4 color = GetTileColor(tile.TileID);
                SpriteBatch::DrawRect(drawPos, drawSize, color);
            }
        }
    }

    // 绘制物件层
    for (i32 y = startY; y < endY; y++) {
        for (i32 x = startX; x < endX; x++) {
            auto& tile = tilemap.GetTile(1, x, y);
            if (tile.TileID == 0) continue;

            glm::vec4 color = GetTileColor(tile.TileID);
            f32 sx = (x - camPos.x) * tileScreenW;
            f32 sy = screenH - (y - camPos.y + 1) * tileScreenH;

            // 物件层贴图 (树木/石头/栏杆/墙壁)
            Ref<Texture2D> tex = nullptr;
            glm::vec4 objUv = {0.0f, 0.0f, 1.0f, 1.0f};
            switch (tile.TileID) {
                case 12:
                    tex = m_TexFence;
                    objUv = {0.0f, 0.0f, 0.25f, 0.75f}; // fence 64x64: 左侧完整栅栏
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

void GameLayer::RenderEntities() {
    auto& world = m_Scene->GetWorld();
    glm::vec2 camCenter = m_CamCtrl.GetPosition();
    f32 zoom = m_CamCtrl.GetZoom();
    f32 viewW = 20.0f / zoom;
    f32 viewH = 15.0f / zoom;
    // 将中心坐标转换为左下角坐标
    glm::vec2 camPos = camCenter - glm::vec2(viewW * 0.5f, viewH * 0.5f);
    auto& window = Application::Get().GetWindow();
    f32 screenW = (f32)window.GetWidth();
    f32 screenH = (f32)window.GetHeight();
    f32 tileScreenW = screenW / viewW;
    f32 tileScreenH = screenH / viewH;

    // 渲染建筑
    world.ForEach<BuildableComponent>([&](Entity e, BuildableComponent& bld) {
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) return;

        f32 sx = (tr->X - camPos.x - bld.Size.x * 0.5f) * tileScreenW;
        f32 sy = screenH - (tr->Y - camPos.y + bld.Size.y * 0.5f) * tileScreenH;
        f32 w = bld.Size.x * tileScreenW;
        f32 h = bld.Size.y * tileScreenH;

        // 建筑纹理映射
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

        // 耐久影响颜色
        f32 hpRatio = bld.HP / bld.MaxHP;
        color *= (0.5f + 0.5f * hpRatio);
        color.a = 1.0f;

        if (tex && tex->IsValid()) {
            if (bld.Type == BuildingType::Campfire) {
                // FireWall ~80x16: 5帧火焰动画, 6fps
                i32 fireFrame = (i32)(m_AnimTimer * 6.0f) % 5;
                f32 fW = 1.0f / 5.0f;  // 每帧 1/5 宽度
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

        // Slime 贴图 192x192: 6列×4行, 蓝/绿/红/粉
        // 每帧约 32x48, 动画循环 6 帧
        if (m_TexSlime && m_TexSlime->IsValid()) {
            f32 row = 0.0f;
            switch (zombie.Type) {
                case ZombieType::Walker: row = 0.0f;  break;
                case ZombieType::Runner: row = 0.25f; break;
                case ZombieType::Tank:   row = 0.5f;  break;
            }
            // 6 帧动画, 5fps
            i32 frame = (i32)(m_AnimTimer * 5.0f) % 6;
            f32 colW = 32.0f / 192.0f;  // 每列宽度 ~0.167
            f32 rowH = 48.0f / 192.0f;  // 每行高度 = 0.25
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
            // Player Sprite 384x32: 每帧 32x32
            auto* player = m_Scene->GetWorld().GetComponent<PlayerComponent>(m_Player);
            i32 frame = 0;
            if (player && player->IsMoving) {
                // 走路动画 8fps, 循环前 8 帧
                frame = (i32)(m_AnimTimer * 8.0f) % 8;
            }
            f32 frameW = 32.0f / 384.0f;  // ~0.0833
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

void GameLayer::RenderBuildPreview() {
    glm::vec2 camCenter = m_CamCtrl.GetPosition();
    f32 zoom = m_CamCtrl.GetZoom();
    f32 viewW = 20.0f / zoom;
    f32 viewH = 15.0f / zoom;
    // 将中心坐标转换为左下角坐标
    glm::vec2 camPos = camCenter - glm::vec2(viewW * 0.5f, viewH * 0.5f);
    auto& window = Application::Get().GetWindow();
    f32 screenW = (f32)window.GetWidth();
    f32 screenH = (f32)window.GetHeight();
    f32 tileScreenW = screenW / viewW;
    f32 tileScreenH = screenH / viewH;

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

void GameLayer::RenderNightOverlay() {
    auto& window = Application::Get().GetWindow();
    f32 screenW = (f32)window.GetWidth();
    f32 screenH = (f32)window.GetHeight();

    f32 darkness = 0.0f;
    if (m_TimeSys) {
        // 18:00-20:00 渐暗, 20:00-4:00 最暗, 4:00-6:00 渐明
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
        // 饥饿
        f32 hungerRatio = surv->Hunger / surv->MaxHunger;
        SpriteBatch::DrawRect({10, 32}, {barW, barH}, {0.2f, 0.2f, 0.2f, 0.7f});
        SpriteBatch::DrawRect({10, 32}, {barW * hungerRatio, barH},
                               {0.85f, 0.55f, 0.1f, 0.9f});
        // 口渴
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

            // 背景
            glm::vec4 bg = selected ? glm::vec4{0.4f, 0.4f, 0.5f, 0.9f}
                                    : glm::vec4{0.2f, 0.2f, 0.25f, 0.7f};
            SpriteBatch::DrawRect({sx, startY}, {slotSize, slotSize}, bg);

            // 物品颜色指示
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

            // 选中边框
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
