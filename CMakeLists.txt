cmake_minimum_required(VERSION 3.20)
project(GameEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── 平台 / 编译器 特定设置 ──────────────────────────────────
if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
elseif(MSVC)
    # MSVC: 高警告等级 + UTF-8 源码
    add_compile_options(/W4 /utf-8)
    # 禁用烦人的安全警告
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# ── 第三方依赖 ──────────────────────────────────────────────

# GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glfw)

# GLAD (OpenGL Loader) - 直接编译源码
add_library(glad STATIC
    third_party/glad/src/glad.c
)
target_include_directories(glad PUBLIC third_party/glad/include)

# GLM (Header-only math)
add_subdirectory(third_party/glm)

# stb (Header-only, 在引擎内部实现)

# ImGui (场景编辑器 UI)
file(GLOB IMGUI_SOURCES "${CMAKE_SOURCE_DIR}/third_party/imgui/*.cpp")
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC
    "${CMAKE_SOURCE_DIR}/third_party/imgui"
)
target_link_libraries(imgui PUBLIC glfw glad)

# cgltf (Header-only glTF 解析器, 在引擎内部实现)

# ── 引擎库 ──────────────────────────────────────────────────

add_subdirectory(engine)

# ── 沙盒测试应用 ────────────────────────────────────────────

add_subdirectory(sandbox)

# ── 单元测试 (可选) ─────────────────────────────────────────

option(BUILD_TESTS "构建单元测试" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
