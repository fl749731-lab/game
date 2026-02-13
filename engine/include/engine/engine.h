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

// Platform
#include "engine/platform/window.h"
#include "engine/platform/input.h"

// Renderer
#include "engine/renderer/renderer.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/buffer.h"
#include "engine/renderer/texture.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/light.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/framebuffer.h"
#include "engine/renderer/post_process.h"
#include "engine/renderer/skybox.h"
#include "engine/renderer/particle.h"
#include "engine/renderer/bloom.h"
#include "engine/renderer/scene_renderer.h"
#include "engine/renderer/shaders.h"
#include "engine/renderer/fps_camera_controller.h"
#include "engine/renderer/shadow_map.h"
#include "engine/renderer/frustum.h"
#include "engine/renderer/screen_quad.h"
#include "engine/renderer/sprite_batch.h"
#include "engine/renderer/font.h"
#include "engine/renderer/gltf_loader.h"

// Debug
#include "engine/debug/debug_draw.h"
#include "engine/debug/debug_ui.h"
#include "engine/debug/profiler.h"

// Editor
#include "engine/editor/editor.h"

// Physics
#include "engine/physics/collision.h"
#include "engine/physics/physics_world.h"

// Audio
#include "engine/audio/audio_engine.h"

// AI (optional)
#ifdef ENGINE_HAS_PYTHON
#include "engine/ai/python_engine.h"
#endif
