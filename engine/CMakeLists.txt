# ── Engine 静态库 ───────────────────────────────────────────

file(GLOB_RECURSE ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_library(Engine STATIC ${ENGINE_SOURCES})

# 查找 Python
find_package(Python3 COMPONENTS Development QUIET)

if(Python3_FOUND)
    message(STATUS "Found Python ${Python3_VERSION}: ${Python3_INCLUDE_DIRS}")
    target_compile_definitions(Engine PUBLIC ENGINE_HAS_PYTHON)
else()
    message(WARNING "Python3 not found, AI layer disabled")
endif()

target_include_directories(Engine
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/third_party/stb"
        $<$<BOOL:${Python3_FOUND}>:${Python3_INCLUDE_DIRS}>
)

target_link_libraries(Engine
    PUBLIC
        glfw
        glad
        glm::glm
    PRIVATE
        opengl32  # Windows OpenGL
        $<$<BOOL:${Python3_FOUND}>:${Python3_LIBRARIES}>
)

# 预处理器宏
target_compile_definitions(Engine
    PUBLIC
        $<$<CONFIG:Debug>:ENGINE_DEBUG>
        ENGINE_PLATFORM_WINDOWS
)
