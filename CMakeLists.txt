cmake_minimum_required(VERSION 3.20)
project(GameEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MinGW 静态链接运行时，避免 DLL 依赖
if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
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

# ── 引擎库 ──────────────────────────────────────────────────

add_subdirectory(engine)

# ── 沙盒测试应用 ────────────────────────────────────────────

add_subdirectory(sandbox)
