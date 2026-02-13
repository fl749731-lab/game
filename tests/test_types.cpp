/**
 * @file test_types.cpp
 * @brief 类型系统单元测试
 *
 * 测试 engine/core/types.h 中的类型别名和智能指针工具。
 */

#include <gtest/gtest.h>
#include "engine/core/types.h"

using namespace Engine;

// ── 类型大小测试 ────────────────────────────────────────────

TEST(TypesTest, IntegerSizes) {
    EXPECT_EQ(sizeof(u8),  1);
    EXPECT_EQ(sizeof(u16), 2);
    EXPECT_EQ(sizeof(u32), 4);
    EXPECT_EQ(sizeof(u64), 8);

    EXPECT_EQ(sizeof(i8),  1);
    EXPECT_EQ(sizeof(i16), 2);
    EXPECT_EQ(sizeof(i32), 4);
    EXPECT_EQ(sizeof(i64), 8);
}

TEST(TypesTest, FloatSizes) {
    EXPECT_EQ(sizeof(f32), 4);
    EXPECT_EQ(sizeof(f64), 8);
}

// ── 智能指针工具测试 ────────────────────────────────────────

struct TestObj {
    int value;
    explicit TestObj(int v) : value(v) {}
};

TEST(TypesTest, CreateScopeCreatesUniquePtr) {
    auto obj = CreateScope<TestObj>(42);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->value, 42);
}

TEST(TypesTest, CreateRefCreatesSharedPtr) {
    auto obj = CreateRef<TestObj>(99);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->value, 99);
    EXPECT_EQ(obj.use_count(), 1);

    // 拷贝后引用计数增加
    Ref<TestObj> obj2 = obj;
    EXPECT_EQ(obj.use_count(), 2);
}

TEST(TypesTest, ScopeOwnershipTransfer) {
    Scope<TestObj> a = CreateScope<TestObj>(10);
    Scope<TestObj> b = std::move(a);
    EXPECT_EQ(a, nullptr);
    EXPECT_EQ(b->value, 10);
}
