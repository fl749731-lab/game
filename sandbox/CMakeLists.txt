# ── Sandbox 测试应用 ────────────────────────────────────────

add_executable(Sandbox
    src/main.cpp
    src/game_layer.cpp
    src/game_layer_render.cpp
    src/game_layer_ldtk.cpp
)

target_link_libraries(Sandbox PRIVATE Game)
