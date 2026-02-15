# ── Sandbox 测试应用 ────────────────────────────────────────

add_executable(Sandbox
    src/main.cpp
    src/game_layer.cpp
)

target_link_libraries(Sandbox PRIVATE Game)
