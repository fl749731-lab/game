/**
 * @file test_ecs.cpp
 * @brief ECS 系统单元测试
 *
 * 测试 Entity 创建/销毁、Component 添加/获取/移除、System 注册与更新。
 */

#include <gtest/gtest.h>
#include "engine/core/ecs.h"

using namespace Engine;

// ── Entity 生命周期 ─────────────────────────────────────────

TEST(ECSTest, CreateEntity) {
    ECSWorld world;
    Entity e = world.CreateEntity("TestEntity");
    EXPECT_NE(e, INVALID_ENTITY);
    EXPECT_EQ(world.GetEntityCount(), 1);
}

TEST(ECSTest, CreateMultipleEntities) {
    ECSWorld world;
    Entity e1 = world.CreateEntity("A");
    Entity e2 = world.CreateEntity("B");
    Entity e3 = world.CreateEntity("C");
    EXPECT_NE(e1, e2);
    EXPECT_NE(e2, e3);
    EXPECT_EQ(world.GetEntityCount(), 3);
}

TEST(ECSTest, DestroyEntity) {
    ECSWorld world;
    Entity e = world.CreateEntity("Temp");
    EXPECT_EQ(world.GetEntityCount(), 1);
    world.DestroyEntity(e);
    EXPECT_EQ(world.GetEntityCount(), 0);
}

// ── Component 操作 ──────────────────────────────────────────

TEST(ECSTest, AddAndGetComponent) {
    ECSWorld world;
    Entity e = world.CreateEntity();

    auto& transform = world.AddComponent<TransformComponent>(e);
    transform.X = 1.0f;
    transform.Y = 2.0f;
    transform.Z = 3.0f;

    auto* got = world.GetComponent<TransformComponent>(e);
    ASSERT_NE(got, nullptr);
    EXPECT_FLOAT_EQ(got->X, 1.0f);
    EXPECT_FLOAT_EQ(got->Y, 2.0f);
    EXPECT_FLOAT_EQ(got->Z, 3.0f);
}

TEST(ECSTest, HasComponent) {
    ECSWorld world;
    Entity e = world.CreateEntity();

    EXPECT_FALSE(world.HasComponent<HealthComponent>(e));
    world.AddComponent<HealthComponent>(e);
    EXPECT_TRUE(world.HasComponent<HealthComponent>(e));
}

TEST(ECSTest, GetNonexistentComponentReturnsNull) {
    ECSWorld world;
    Entity e = world.CreateEntity();

    auto* vel = world.GetComponent<VelocityComponent>(e);
    EXPECT_EQ(vel, nullptr);
}

TEST(ECSTest, TagComponentAutoCreated) {
    ECSWorld world;
    Entity e = world.CreateEntity("MyEntity");

    auto* tag = world.GetComponent<TagComponent>(e);
    ASSERT_NE(tag, nullptr);
    EXPECT_EQ(tag->Name, "MyEntity");
}

TEST(ECSTest, MultipleComponentsOnEntity) {
    ECSWorld world;
    Entity e = world.CreateEntity();

    world.AddComponent<TransformComponent>(e);
    world.AddComponent<HealthComponent>(e);
    world.AddComponent<VelocityComponent>(e);

    EXPECT_TRUE(world.HasComponent<TransformComponent>(e));
    EXPECT_TRUE(world.HasComponent<HealthComponent>(e));
    EXPECT_TRUE(world.HasComponent<VelocityComponent>(e));
}

// ── ForEach 遍历 ────────────────────────────────────────────

TEST(ECSTest, ForEachIteratesCorrectly) {
    ECSWorld world;

    Entity e1 = world.CreateEntity();
    Entity e2 = world.CreateEntity();
    world.AddComponent<HealthComponent>(e1).Current = 50.0f;
    world.AddComponent<HealthComponent>(e2).Current = 75.0f;

    int count = 0;
    f32 totalHealth = 0;
    world.ForEach<HealthComponent>([&](Entity, HealthComponent& hp) {
        count++;
        totalHealth += hp.Current;
    });

    EXPECT_EQ(count, 2);
    EXPECT_FLOAT_EQ(totalHealth, 125.0f);
}

// ── System 测试 ─────────────────────────────────────────────

TEST(ECSTest, MovementSystemUpdatesPosition) {
    ECSWorld world;
    world.AddSystem<MovementSystem>();

    Entity e = world.CreateEntity();
    auto& t = world.AddComponent<TransformComponent>(e);
    t.X = 0.0f; t.Y = 0.0f; t.Z = 0.0f;

    auto& v = world.AddComponent<VelocityComponent>(e);
    v.VX = 10.0f; v.VY = 5.0f; v.VZ = -3.0f;

    // 模拟 1 秒
    world.Update(1.0f);

    auto* result = world.GetComponent<TransformComponent>(e);
    ASSERT_NE(result, nullptr);
    EXPECT_FLOAT_EQ(result->X, 10.0f);
    EXPECT_FLOAT_EQ(result->Y, 5.0f);
    EXPECT_FLOAT_EQ(result->Z, -3.0f);
}

TEST(ECSTest, LifetimeSystemDestroysExpiredEntities) {
    ECSWorld world;
    world.AddSystem<LifetimeSystem>();

    Entity e = world.CreateEntity("Temp");
    world.AddComponent<LifetimeComponent>(e).TimeRemaining = 0.5f;

    EXPECT_EQ(world.GetEntityCount(), 1);

    // 模拟 0.3 秒 — 还没到期
    world.Update(0.3f);
    EXPECT_EQ(world.GetEntityCount(), 1);

    // 再模拟 0.3 秒 — 超过 0.5 秒，应该销毁
    world.Update(0.3f);
    EXPECT_EQ(world.GetEntityCount(), 0);
}

// ── 销毁后清理组件 ──────────────────────────────────────────

TEST(ECSTest, DestroyEntityCleansUpComponents) {
    ECSWorld world;
    Entity e = world.CreateEntity();
    world.AddComponent<TransformComponent>(e);
    world.AddComponent<HealthComponent>(e);

    world.DestroyEntity(e);

    // 销毁后组件应不可访问
    EXPECT_EQ(world.GetComponent<TransformComponent>(e), nullptr);
    EXPECT_EQ(world.GetComponent<HealthComponent>(e), nullptr);
}
