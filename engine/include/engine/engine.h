#pragma once

// ── 引擎核心头文件 ──────────────────────────────────────────

// Core
#include "engine/core/types.h"
#include "engine/core/log.h"
#include "engine/core/assert.h"
#include "engine/core/time.h"
#include "engine/core/event.h"
#include "engine/core/ecs.h"
#include "engine/core/resource_manager.h"
#include "engine/core/scene.h"

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

// Debug
#include "engine/debug/debug_draw.h"
#include "engine/debug/debug_ui.h"
#include "engine/debug/profiler.h"

// AI (optional)
#ifdef ENGINE_HAS_PYTHON
#include "engine/ai/python_engine.h"
#endif
