#pragma once

// ── 引擎核心头文件 ──────────────────────────────────────────

// Core
#include "engine/core/types.h"
#include "engine/core/log.h"
#include "engine/core/assert.h"
#include "engine/core/gl_debug.h"
#include "engine/core/time.h"
#include "engine/core/event.h"
#include "engine/core/ecs.h"
#include "engine/core/resource_manager.h"
#include "engine/core/scene.h"
#include "engine/core/scene_serializer.h"
#include "engine/core/script_system.h"
#include "engine/core/prefab.h"
#include "engine/core/job_system.h"
#include "engine/core/async_loader.h"
#include "engine/core/application.h"
#include "engine/core/allocator.h"

// Platform
#include "engine/platform/window.h"
#include "engine/platform/input.h"

// RHI (Rendering Hardware Interface)
#include "engine/rhi/rhi.h"

// Renderer
#include "engine/renderer/renderer.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/buffer.h"
#include "engine/renderer/texture.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/light.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/material.h"
#include "engine/renderer/framebuffer.h"
#include "engine/renderer/post_process.h"
#include "engine/renderer/skybox.h"
#include "engine/renderer/particle.h"
#include "engine/renderer/bloom.h"
#include "engine/renderer/scene_renderer.h"
#include "engine/renderer/shaders.h"
#include "engine/renderer/shader_library.h"
#include "engine/renderer/render_queue.h"
#include "engine/renderer/fps_camera_controller.h"
#include "engine/renderer/shadow_map.h"
#include "engine/renderer/frustum.h"
#include "engine/renderer/screen_quad.h"
#include "engine/renderer/sprite_batch.h"
#include "engine/renderer/font.h"
#include "engine/renderer/gltf_loader.h"
#include "engine/renderer/viewport_modes.h"
#include "engine/renderer/ssao.h"
#include "engine/renderer/ssr.h"
#include "engine/renderer/cascaded_shadow_map.h"
#include "engine/renderer/ibl.h"
#include "engine/renderer/g_buffer.h"
#include "engine/renderer/batch_renderer.h"
#include "engine/renderer/instance_renderer.h"
#include "engine/renderer/animation.h"
#include "engine/renderer/animation_blend.h"
#include "engine/renderer/animation_event.h"
#include "engine/renderer/animation_ik.h"
#include "engine/renderer/animation_layer.h"
#include "engine/renderer/animation_root_motion.h"
#include "engine/renderer/animation_state_machine.h"
#include "engine/renderer/skinning_utils.h"
#include "engine/renderer/volumetric.h"

// Debug
#include "engine/debug/debug_draw.h"
#include "engine/debug/debug_ui.h"
#include "engine/debug/profiler.h"
#include "engine/debug/engine_diagnostics.h"
#include "engine/debug/charts.h"
#include "engine/debug/stat_system.h"
#include "engine/debug/console.h"
#include "engine/debug/gpu_profiler.h"

// Renderer (Visualization)
#include "engine/renderer/overdraw.h"

// Editor
#include "engine/editor/editor.h"
#include "engine/editor/gizmo.h"
#include "engine/editor/node_graph.h"
#include "engine/editor/scene_view.h"
#include "engine/editor/asset_browser.h"
#include "engine/editor/curve_editor.h"
#include "engine/editor/hierarchy_panel.h"
#include "engine/editor/inspector_panel.h"
#include "engine/editor/material_editor.h"
#include "engine/editor/drag_drop.h"
#include "engine/editor/docking_layout.h"
#include "engine/editor/hot_reload.h"
#include "engine/editor/screen_capture.h"
#include "engine/editor/prefab_system.h"
#include "engine/editor/timeline_editor.h"
#include "engine/editor/particle_editor.h"
#include "engine/editor/color_picker.h"

// Physics
#include "engine/physics/collision.h"
#include "engine/physics/physics_world.h"
#include "engine/physics/bvh.h"

// Audio
#include "engine/audio/audio_engine.h"
#include "engine/audio/audio_mixer.h"

// Game2D (引擎级通用 2D 工具)
#include "engine/game2d/sprite2d.h"
#include "engine/game2d/tilemap.h"
#include "engine/game2d/camera2d_controller.h"

// AI (optional)
#ifdef ENGINE_HAS_PYTHON
#include "engine/ai/python_engine.h"
#endif
