# ── Engine 静态库 ───────────────────────────────────────────

file(GLOB_RECURSE ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_library(Engine STATIC ${ENGINE_SOURCES})

# 查找 Python (可选，默认关闭)
option(ENGINE_ENABLE_PYTHON "启用 Python AI 层" OFF)

if(ENGINE_ENABLE_PYTHON)
    find_package(Python3 COMPONENTS Development QUIET)
    if(Python3_FOUND)
        message(STATUS "Found Python ${Python3_VERSION}: ${Python3_INCLUDE_DIRS}")
        target_compile_definitions(Engine PUBLIC ENGINE_HAS_PYTHON)
    else()
        message(WARNING "Python3 not found, AI layer disabled")
    endif()
else()
    message(STATUS "Python AI layer disabled (use -DENGINE_ENABLE_PYTHON=ON to enable)")
endif()

# Vulkan (可选，默认关闭)
option(ENGINE_ENABLE_VULKAN "启用 Vulkan 渲染后端" OFF)

if(ENGINE_ENABLE_VULKAN)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        message(STATUS "Found Vulkan: ${Vulkan_INCLUDE_DIRS}")
        target_compile_definitions(Engine PUBLIC ENGINE_ENABLE_VULKAN)
        target_include_directories(Engine PRIVATE ${Vulkan_INCLUDE_DIRS})
    else()
        message(WARNING "Vulkan SDK not found, Vulkan backend disabled")
    endif()
else()
    message(STATUS "Vulkan backend disabled (use -DENGINE_ENABLE_VULKAN=ON to enable)")
endif()

# Java/JNI (可选，默认关闭)
option(ENGINE_ENABLE_JAVA "启用 Java 数据层 (JNI)" OFF)

if(ENGINE_ENABLE_JAVA)
    find_package(JNI QUIET)
    if(JNI_FOUND)
        message(STATUS "Found JNI: ${JNI_INCLUDE_DIRS}")
        target_compile_definitions(Engine PUBLIC ENGINE_ENABLE_JAVA)
        target_include_directories(Engine PRIVATE ${JNI_INCLUDE_DIRS})
    else()
        message(WARNING "JNI not found, Java data layer disabled")
    endif()
else()
    message(STATUS "Java data layer disabled (use -DENGINE_ENABLE_JAVA=ON to enable)")
endif()

target_include_directories(Engine
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/third_party/stb"
        "${CMAKE_SOURCE_DIR}/third_party/miniaudio"
        "${CMAKE_SOURCE_DIR}/third_party/cgltf"
        $<$<BOOL:${Python3_FOUND}>:${Python3_INCLUDE_DIRS}>
)

# ── 跨平台 OpenGL 链接 ─────────────────────────────────────
if(WIN32)
    set(OPENGL_LIBS opengl32)
elseif(APPLE)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(IOKIT_LIBRARY IOKit REQUIRED)
    find_library(OPENGL_LIBRARY OpenGL REQUIRED)
    set(OPENGL_LIBS ${OPENGL_LIBRARY} ${COCOA_LIBRARY} ${IOKIT_LIBRARY})
else()
    # Linux
    find_package(OpenGL REQUIRED)
    set(OPENGL_LIBS OpenGL::GL)
endif()

target_link_libraries(Engine
    PUBLIC
        glfw
        glad
        imgui
        glm::glm
    PRIVATE
        ${OPENGL_LIBS}
        $<$<BOOL:${Python3_FOUND}>:${Python3_LIBRARIES}>
        $<$<BOOL:${Vulkan_FOUND}>:${Vulkan_LIBRARIES}>
        $<$<BOOL:${JNI_FOUND}>:${JNI_LIBRARIES}>
)

# 预处理器宏
target_compile_definitions(Engine
    PUBLIC
        $<$<CONFIG:Debug>:ENGINE_DEBUG>
        $<$<PLATFORM_ID:Windows>:ENGINE_PLATFORM_WINDOWS>
        $<$<PLATFORM_ID:Linux>:ENGINE_PLATFORM_LINUX>
        $<$<PLATFORM_ID:Darwin>:ENGINE_PLATFORM_MACOS>
)
