# ── Game 静态库 (星露谷风格游戏专属逻辑) ────────────────────
# 这个库包含游戏层系统，不属于引擎核心。
# 引擎 (Engine) 是通用的, Game 库是特定游戏的框架。

file(GLOB_RECURSE GAME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(Game STATIC ${GAME_SOURCES})

target_include_directories(Game PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# Game 依赖 Engine
target_link_libraries(Game PUBLIC Engine)
