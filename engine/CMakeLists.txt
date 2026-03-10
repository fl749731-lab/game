# ── Engine 静态库 ───────────────────────────────────────────

# ── 显式源文件列表 (按子模块分组) ────────────────────────────
# 新增 .cpp 文件时请手动添加到对应分组。

set(ENGINE_SOURCES
    # ── Core ──────────────────────────────────────────────────
    src/core/application.cpp
    src/core/async_loader.cpp
    src/core/ecs.cpp
    src/core/job_system.cpp
    src/core/log.cpp
    src/core/prefab.cpp
    src/core/resource_manager.cpp
    src/core/scene.cpp
    src/core/scene_serializer.cpp
    src/core/script_system.cpp
    src/core/time.cpp

    # ── Platform ──────────────────────────────────────────────
    src/platform/input.cpp
    src/platform/window.cpp

    # ── Physics ───────────────────────────────────────────────
    src/physics/bvh.cpp
    src/physics/collision.cpp
    src/physics/obb.cpp
    src/physics/physics_world.cpp

    # ── Audio ─────────────────────────────────────────────────
    src/audio/audio_engine.cpp
    src/audio/audio_mixer.cpp

    # ── Debug ─────────────────────────────────────────────────
    src/debug/charts.cpp
    src/debug/console.cpp
    src/debug/debug_draw.cpp
    src/debug/debug_ui.cpp
    src/debug/engine_diagnostics.cpp
    src/debug/gpu_profiler.cpp
    src/debug/profiler.cpp
    src/debug/stat_system.cpp

    # ── Editor ────────────────────────────────────────────────
    src/editor/asset_browser.cpp
    src/editor/color_picker.cpp
    src/editor/curve_editor.cpp
    src/editor/docking_layout.cpp
    src/editor/drag_drop.cpp
    src/editor/editor.cpp
    src/editor/engine_inspector.cpp
    src/editor/gizmo.cpp
    src/editor/hierarchy_panel.cpp
    src/editor/hot_reload.cpp
    src/editor/inspector_panel.cpp
    src/editor/material_editor.cpp
    src/editor/node_graph.cpp
    src/editor/particle_editor.cpp
    src/editor/prefab_system.cpp
    src/editor/scene_view.cpp
    src/editor/screen_capture.cpp
    src/editor/timeline_editor.cpp

    # ── Game2D ────────────────────────────────────────────────
    src/game2d/camera2d_controller.cpp
    src/game2d/ldtk_loader.cpp
    src/game2d/sprite2d.cpp
    src/game2d/tilemap.cpp

    # ── Renderer ──────────────────────────────────────────────
    src/renderer/animation.cpp
    src/renderer/animation_blend.cpp
    src/renderer/animation_event.cpp
    src/renderer/animation_ik.cpp
    src/renderer/animation_layer.cpp
    src/renderer/animation_root_motion.cpp
    src/renderer/animation_state_machine.cpp
    src/renderer/batch_renderer.cpp
    src/renderer/bloom.cpp
    src/renderer/buffer.cpp
    src/renderer/camera.cpp
    src/renderer/cascaded_shadow_map.cpp
    src/renderer/font.cpp
    src/renderer/fps_camera_controller.cpp
    src/renderer/framebuffer.cpp
    src/renderer/frustum.cpp
    src/renderer/g_buffer.cpp
    src/renderer/gltf_loader.cpp
    src/renderer/ibl.cpp
    src/renderer/instance_renderer.cpp
    src/renderer/material.cpp
    src/renderer/mesh.cpp
    src/renderer/overdraw.cpp
    src/renderer/particle.cpp
    src/renderer/post_process.cpp
    src/renderer/render_queue.cpp
    src/renderer/renderer.cpp
    src/renderer/scene_renderer.cpp
    src/renderer/screen_quad.cpp
    src/renderer/shader.cpp
    src/renderer/shader_library.cpp
    src/renderer/shadow_map.cpp
    src/renderer/skinning_utils.cpp
    src/renderer/skybox.cpp
    src/renderer/sprite_batch.cpp
    src/renderer/ssao.cpp
    src/renderer/ssr.cpp
    src/renderer/stb_image_impl.cpp
    src/renderer/texture.cpp
    src/renderer/viewport_modes.cpp
    src/renderer/volumetric.cpp

    # ── RHI ───────────────────────────────────────────────────
    src/rhi/rhi_device.cpp
    src/rhi/opengl/gl_buffer.cpp
    src/rhi/opengl/gl_device.cpp
    src/rhi/opengl/gl_framebuffer.cpp
    src/rhi/opengl/gl_shader.cpp
    src/rhi/opengl/gl_texture.cpp

    # ── AI (optional, guarded by ENGINE_ENABLE_PYTHON) ────────
    src/ai/behavior_tree.cpp
    src/ai/python_bridge.cpp
    src/ai/python_engine.cpp

    # ── Data (optional, JNI) ──────────────────────────────────
    src/data/jni_bridge.cpp
)

# ── Vulkan RHI + Renderer 后端 (条件编译) ────────────────────
set(ENGINE_VULKAN_SOURCES
    src/rhi/vulkan/vk_buffer.cpp
    src/rhi/vulkan/vk_device.cpp
    src/rhi/vulkan/vk_framebuffer.cpp
    src/rhi/vulkan/vk_shader.cpp
    src/rhi/vulkan/vk_texture.cpp
    src/renderer/vulkan/vulkan_bloom.cpp
    src/renderer/vulkan/vulkan_compute.cpp
    src/renderer/vulkan/vulkan_context.cpp
    src/renderer/vulkan/vulkan_descriptor.cpp
    src/renderer/vulkan/vulkan_g_buffer.cpp
    src/renderer/vulkan/vulkan_ibl.cpp
    src/renderer/vulkan/vulkan_lighting_pass.cpp
    src/renderer/vulkan/vulkan_material.cpp
    src/renderer/vulkan/vulkan_mesh.cpp
    src/renderer/vulkan/vulkan_post_process.cpp
    src/renderer/vulkan/vulkan_scene_renderer.cpp
    src/renderer/vulkan/vulkan_screen_quad.cpp
    src/renderer/vulkan/vulkan_shader.cpp
    src/renderer/vulkan/vulkan_shadow_map.cpp
    src/renderer/vulkan/vulkan_skybox.cpp
    src/renderer/vulkan/vulkan_ssao.cpp
    src/renderer/vulkan/vulkan_ssr.cpp
    src/renderer/vulkan/vulkan_texture.cpp
)

# 使用相对路径 — CMakeLists.txt 位于 engine/ 目录
list(TRANSFORM ENGINE_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
list(TRANSFORM ENGINE_VULKAN_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

add_library(Engine STATIC ${ENGINE_SOURCES} ${ENGINE_VULKAN_SOURCES})

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

# Vulkan (必选，默认开启)
option(ENGINE_ENABLE_VULKAN "启用 Vulkan 渲染后端" ON)

if(ENGINE_ENABLE_VULKAN)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        message(STATUS "Found Vulkan: ${Vulkan_INCLUDE_DIRS}")
        target_compile_definitions(Engine PUBLIC ENGINE_ENABLE_VULKAN)
        target_include_directories(Engine PRIVATE ${Vulkan_INCLUDE_DIRS})
    elseif(EXISTS "C:/msys64/mingw64/include/vulkan/vulkan.h")
        # MSYS2 MinGW 路径回退
        message(STATUS "Found Vulkan via MSYS2 MinGW")
        set(Vulkan_FOUND TRUE)
        set(Vulkan_INCLUDE_DIRS "C:/msys64/mingw64/include")
        set(Vulkan_LIBRARIES "C:/msys64/mingw64/lib/libvulkan-1.dll.a")
        target_compile_definitions(Engine PUBLIC ENGINE_ENABLE_VULKAN)
        target_include_directories(Engine PRIVATE ${Vulkan_INCLUDE_DIRS})
    else()
        message(FATAL_ERROR "未找到 Vulkan SDK（也未命中 MSYS2 回退路径），当前配置要求启用 Vulkan 渲染后端")
    endif()
    # ── SPIR-V 着色器编译 ─────────────────────────────────
    if(Vulkan_FOUND)
        # 查找 glslc (Vulkan SDK 或 MSYS2)
        find_program(GLSLC glslc HINTS
            "$ENV{VULKAN_SDK}/Bin"
            "C:/msys64/mingw64/bin"
            "C:/VulkanSDK/*/Bin"
        )
        if(NOT GLSLC)
            find_program(GLSLC glslangValidator HINTS
                "$ENV{VULKAN_SDK}/Bin"
                "C:/msys64/mingw64/bin"
            )
        endif()

        if(GLSLC)
            message(STATUS "Found shader compiler: ${GLSLC}")

            set(VK_SHADER_DIR "${CMAKE_SOURCE_DIR}/sandbox/assets/shaders/vulkan")
            file(GLOB VK_SHADER_SOURCES
                "${VK_SHADER_DIR}/*.vert"
                "${VK_SHADER_DIR}/*.frag"
                "${VK_SHADER_DIR}/*.comp"
            )

            set(VK_SPV_OUTPUTS "")
            foreach(SHADER ${VK_SHADER_SOURCES})
                get_filename_component(SHADER_NAME ${SHADER} NAME)
                set(SPV_OUTPUT "${VK_SHADER_DIR}/${SHADER_NAME}.spv")
                add_custom_command(
                    OUTPUT ${SPV_OUTPUT}
                    COMMAND ${GLSLC} -o ${SPV_OUTPUT} ${SHADER}
                    DEPENDS ${SHADER}
                    COMMENT "Compiling SPIR-V: ${SHADER_NAME}"
                )
                list(APPEND VK_SPV_OUTPUTS ${SPV_OUTPUT})
            endforeach()

            if(VK_SPV_OUTPUTS)
                add_custom_target(VulkanShaders ALL DEPENDS ${VK_SPV_OUTPUTS})
                add_dependencies(Engine VulkanShaders)
            endif()
        else()
            message(WARNING "glslc/glslangValidator not found, Vulkan shaders will not be compiled")
        endif()
    endif()
else()
    message(FATAL_ERROR "当前项目配置要求 Vulkan 渲染后端，禁止关闭 ENGINE_ENABLE_VULKAN")
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
        "${CMAKE_SOURCE_DIR}/third_party"
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
