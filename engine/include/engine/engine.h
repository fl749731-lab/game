#pragma once

// ── 引擎核心头文件 ──────────────────────────────────────────

#include "engine/core/types.h"
#include "engine/core/log.h"
#include "engine/core/assert.h"
#include "engine/core/time.h"
#include "engine/core/ecs.h"
#include "engine/platform/window.h"
#include "engine/platform/input.h"
#include "engine/renderer/renderer.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/buffer.h"
#include "engine/renderer/texture.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/light.h"
#include "engine/renderer/mesh.h"

#ifdef ENGINE_HAS_PYTHON
#include "engine/ai/python_engine.h"
#endif
