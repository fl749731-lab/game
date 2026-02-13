# ── 单元测试 ────────────────────────────────────────────────

include(FetchContent)

# 拉取 Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

# Windows: 避免 gtest 覆盖父项目的编译器/链接器设置
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ── 测试可执行文件 ──────────────────────────────────────────

add_executable(engine_tests
    test_types.cpp
    test_ecs.cpp
)

target_link_libraries(engine_tests
    PRIVATE
        Engine
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(engine_tests)
