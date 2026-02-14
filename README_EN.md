# 🎮 Game Engine

[![Build Engine](https://github.com/fl749731-lab/game/actions/workflows/build.yml/badge.svg)](https://github.com/fl749731-lab/game/actions/workflows/build.yml)
**English | [中文](README.md)**

A general-purpose 2D/3D game engine built from scratch in C/C++.

> 🚧 **Under Active Development** — Personal learning project, continuously iterating.

## ✨ Features

### Rendering

| Feature | Status | Description |
| --- | :---: | --- |
| Deferred Rendering | ✅ | G-Buffer MRT + Fullscreen Deferred Lighting Pass |
| PBR Materials | ✅ | Cook-Torrance GGX BRDF (Metallic/Roughness workflow) |
| IBL Lighting | ✅ | HDR→Cubemap + Irradiance Map + Prefiltered Env Map + BRDF LUT |
| SSAO | ✅ | 32-sample hemisphere kernel + 4×4 noise rotation + blur |
| SSR | ✅ | View-space Ray Marching (64 steps) + HDR sampling |
| Instanced Rendering | ✅ | Dynamic VBO + glDrawArraysInstanced (10k+ instances) |
| HDR + Bloom | ✅ | Reinhard tone mapping + Gaussian blur bloom |
| Shadow Mapping | ✅ | Directional Shadow Map + CSM Cascade + PCF soft shadows |
| GPU Skeletal Skinning | ✅ | 128 bones × 4 weights, integer bone ID attributes |
| Normal Mapping | ✅ | TBN matrix, CPU precomputed |
| Particle System | ✅ | GPU Instancing |
| Procedural Skybox | ✅ | 3-layer gradient + sun halo |
| Overdraw Visualization | ✅ | Fragment overlap counting + heatmap (black→blue→green→yellow→red→white) |
| Vignette / Frustum Culling | ✅ | — |
| Vulkan Backend | ✅ | Full feature parity: Mipmap / PBR Materials / G-Buffer MRT / Deferred Lighting / Shadow Map (PCF) / Bloom / Post-processing (HDR+Gamma+Vignette) / SSAO / SSR / Procedural Skybox / IBL (BRDF LUT) / Compute Pipeline |
| RHI Abstraction Layer | ✅ | Backend-agnostic interfaces: Buffer / Shader / Texture / Framebuffer / PipelineState / Device — OpenGL & Vulkan implementations |

### Editor Tools

| Feature | Status | Description |
| --- | :---: | --- |
| Scene Editor | ✅ | ImGui integrated, multi-panel layout |
| Hierarchy Panel | ✅ | Scene tree + drag-and-drop sorting + search filter |
| Inspector Panel | ✅ | 8-component property Inspector (Transform/Light/Material...) |
| Node Graph Editor | ✅ | Generic visual node graph (drag/connect/undo/categorized menu) |
| Material Editor | ✅ | Node-based visual material editing + preview |
| Particle Editor | ✅ | Real-time particle preview + 4 presets + parameter tuning |
| Timeline Editor | ✅ | Multi-track timeline + diamond keyframe markers + zoom/pan |
| Curve Editor | ✅ | Hermite spline curves + keyframe editing |
| Color Picker | ✅ | HSV wheel + HDR + gradient editor + eyedropper + favorites |
| Asset Browser | ✅ | Filesystem browsing + icon grid/list view |
| Gizmo | ✅ | Translate/Rotate/Scale 3D Gizmo |
| Drag & Drop | ✅ | Cross-panel drag & drop (entities/assets/colors) |
| Hot Reload | ✅ | File monitoring + Shader/Script auto-reload |
| Docking Layout | ✅ | Multi-panel layout management |
| Screenshot System | ✅ | Framebuffer screenshot + sequence frame recording |
| Prefab System | ✅ | Prefab save & instantiation |

### Core Engine

| Feature | Status | Description |
| --- | :---: | --- |
| ECS Architecture | ✅ | Entity-Component-System |
| AABB / OBB Physics | ✅ | Collision + Raycast + BVH acceleration + OBB/Sphere/Capsule |
| Skeletal Animation | ✅ | Sampling/Blending/Crossfade/State Machine/Layer Masking/IK/Root Motion/Events |
| Scene Serialization | ✅ | JSON Save/Load, 16+ components fully covered |
| Script Logic Layer | ✅ | ScriptSystem + EngineAPI (30+ Python bindings) |
| Python AI | ✅ | pybind11 bridge (patrol/chase/defend) |
| Hierarchical Command AI | ✅ | Commander→Squad Leader→Soldier 3-tier decision + player intent memory |
| Audio System | ✅ | miniaudio (3D spatial audio + mixer) |
| glTF / OBJ Loading | ✅ | cgltf + skinning/skeleton/animation parsing + custom OBJ (with tangent calculation) |
| Multi-threaded JobSystem | ✅ | Thread pool + ParallelFor (physics/ECS parallel) |
| Async Resource Loading | ✅ | AsyncLoader: background decode → main thread GPU upload |

### Debug & Performance

| Feature | Status | Description |
| --- | :---: | --- |
| DebugDraw | ✅ | Lines/AABB/Sphere/Ray/Grid/Axes |
| GPU Profiler | ✅ | Timer Query timing + ImGui visualization |
| Performance Charts | ✅ | Real-time FPS/memory/system curve graphs |
| Stats System | ✅ | Draw Call/Triangle/Entity counting |
| Console | ✅ | Command-line console + log level filtering |
| Engine Diagnostics | ✅ | GL context / memory / system info |

## Architecture

```text
engine/       ← C++ engine static library (core rendering/physics/ECS/audio/editor)
sandbox/      ← Test sandbox application
ai/           ← Script logic layer (generic scripts + hierarchical AI behavior, optional)
data/         ← Java data layer (JNI bridge, optional)
third_party/  ← Third-party dependencies (glfw, glad, glm, stb, imgui, cgltf, miniaudio, pybind11)
tests/        ← Unit tests (Google Test)
docs/         ← Documentation & benchmarks
```

### ECS Design

- **Storage**: `ComponentArray<T>` — Sparse Set **SoA** layout (dense data tightly packed, cache-friendly; sparse mapping O(1) lookup; swap-and-pop deletion)
- **Entity ID**: 32-bit incrementing ID, `CreateEntity()` auto-attaches `TagComponent`
- **Iteration**: Templated `ForEach<T>()` linear scan + `ForEach2<T1,T2>()` dual-component joint query (driven by smaller pool)
- **Parallel**: `ParallelForEach<T>()` multi-threaded chunked iteration (based on `JobSystem` thread pool)
- **Hierarchy**: Parent-child relationships (`SetParent` / `GetRootEntities`) + `TransformSystem` recursive world matrix computation
- **Systems**: Inherit from `System` base class, serial update (built-in `MovementSystem`, `LifetimeSystem`, `TransformSystem`, `ScriptSystem`)
- **Components**: Transform, Tag, Health, Velocity, AI, Squad, Script, Render, Material, RotationAnim, Collider, RigidBody, Animator, etc. (16+ components)
- **Direct Access**: `RawData()` / `RawEntities()` expose underlying array pointers for high-performance batch processing

### Deferred Rendering Pipeline

```text
Pass 0: Shadow Map        → Directional light depth map (CSM Cascade + PCF soft shadows)
Pass 1: G-Buffer Geometry  → MRT (Position / Normal / Albedo+Metallic / Emissive+Roughness)
Pass 2: SSAO              → 32-sample hemisphere kernel + 4×4 rotation noise → Blur
Pass 3: Deferred PBR Light → G-Buffer + Shadow + AO + IBL → HDR FBO
Pass 4: SSR               → View-space Ray Marching → Reflection blending
Pass 5: Forward Overlay    → Skybox / Transparent / Emissive / Particles / Debug lines
Pass 6: Bloom + Post       → Brightness extract → Gaussian blur → Reinhard tone mapping → Screen
Pass *: Overdraw Viz       → Fragment overlap count → Heatmap overlay (optional)
```

### Skeletal Animation Pipeline

```text
glTF Load → Skeleton + AnimationClip → AnimationSampler (CPU keyframe interpolation)
                                     → PoseBlender (pose blending/crossfade)
                                     → AnimStateMachine (FSM state driving)
                                     → AnimLayerStack (base layer + masked overlay layers)
                                     → TwoBoneIK / FABRIK (inverse kinematics)
                                     → RootMotionExtractor (XZ/XYZ delta)
                                     → AnimEventDispatcher (frame event callbacks)
                                     → SkinningUtils → skinning.glsl (GPU skinning)
```

### Resource Management

- Global `ResourceManager` for unified Shader / Texture / Mesh caching
- **Async Loading**: `AsyncLoader` uses `JobSystem` thread pool for CPU decoding on worker threads (stbi_load), main thread `FlushUploads()` rate-limited GPU upload per frame
- Supports both synchronous (`LoadTexture`) and asynchronous (`LoadTextureAsync`) APIs, 100% backward compatible

### Script Logic Layer

- **Generic Scripts**: `ScriptComponent` + `ScriptSystem`, any entity can attach Python scripts
- **Lifecycle**: `on_create` / `on_update` / `on_event` / `on_destroy`
- **EngineAPI**: 30+ Python bindings to operate the engine (Transform/Physics/Entity/Event/Audio)
- **AI Behavior**: `AIComponent` + `AIManager` dedicated to NPC behavior decisions
- **Hierarchical Command Chain**: Commander→Squad Leader→Soldier 3-tier decisions, commands cascade down
- **Player Intent Recognition**: `PlayerTracker` captures player behavior → Commander AI analyzes patterns and memorizes
- **Formation System**: Triangle/Line/Scatter/Wedge 4 formations, dynamic position calculation
- **Optional**: Controlled via `ENGINE_ENABLE_PYTHON` compile flag (default OFF)

## Tech Stack

| Layer | Technology |
| ------ | ------ |
| Languages | C17 / C++20 |
| Graphics | OpenGL 4.5 (GLSL 450), Vulkan 1.3 (optional) |
| Window/Input | GLFW 3.x |
| Math | GLM |
| Image/Font | stb_image, stb_image_write, stb_truetype |
| GL Loading | Custom GLAD |
| Models | cgltf (glTF 2.0 + skinning), custom OBJ |
| Audio | miniaudio |
| UI | Dear ImGui |
| AI Bridge | pybind11 (C++ ↔ Python) |
| Build | CMake 3.20+ |
| CI | GitHub Actions (Windows MSVC + Ubuntu GCC) |
| Data Bridge | JNI (C++ ↔ Java, optional) |

## Building

### Prerequisites

- CMake 3.20+
- C++20 compiler (MSVC 2022 / GCC 11+ / MinGW-w64)

### Windows (MinGW)

```bash
set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
.\build\sandbox\Sandbox.exe
```

### Windows (MSVC)

```bash
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
.\build\sandbox\Debug\Sandbox.exe
```

### Linux

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install libgl1-mesa-dev libx11-dev libxrandr-dev \
    libxinerama-dev libxcursor-dev libxi-dev

cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/sandbox/Sandbox
```

### Build Options

| Option | Default | Description |
| --- | :---: | --- |
| `BUILD_TESTS` | OFF | Build unit tests (Google Test) |
| `ENGINE_ENABLE_PYTHON` | OFF | Enable Python AI layer |
| `ENGINE_ENABLE_VULKAN` | OFF | Enable Vulkan rendering backend |
| `ENGINE_ENABLE_JAVA` | OFF | Enable Java data layer (JNI) |

```bash
# Example: Enable Vulkan and build
cmake -B build -DENGINE_ENABLE_VULKAN=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) for development workflow and code conventions.

## Acknowledgments

This project uses the following open-source libraries:

| Library | Purpose | License |
| --- | --- | --- |
| [GLFW](https://www.glfw.org/) | Window/Input management | Zlib |
| [GLM](https://github.com/g-truc/glm) | Math library | MIT |
| [stb](https://github.com/nothings/stb) | Image loading/writing + font rendering | Public Domain |
| [Dear ImGui](https://github.com/ocornut/imgui) | Debug UI / Editor | MIT |
| [miniaudio](https://miniaud.io/) | Audio system | Public Domain |
| [cgltf](https://github.com/jkuhlmann/cgltf) | glTF parsing | MIT |
| [pybind11](https://github.com/pybind/pybind11) | C++/Python bridge | BSD |
| [GLAD](https://glad.dav1d.de/) | OpenGL loading | MIT |

## License

This project is licensed under the [MIT License](LICENSE).
