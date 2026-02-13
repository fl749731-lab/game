# 🎮 Game Engine

[![Build Engine](https://github.com/fl749731-lab/game/actions/workflows/build.yml/badge.svg)](https://github.com/fl749731-lab/game/actions/workflows/build.yml)

一个从零构建的通用 2D/3D 游戏引擎，使用 C/C++ 开发。

<!-- 在此添加引擎渲染效果截图 -->
<!-- ![渲染效果](docs/screenshots/demo.png) -->

> 🚧 **活跃开发中** — 个人学习项目，持续迭代。

## ✨ 特性一览

| 特性 | 状态 | 说明 |
| ------ | :----: | ------ |
| 延迟渲染 | ✅ | G-Buffer MRT + 全屏延迟光照 Pass |
| PBR 材质 | ✅ | Cook-Torrance GGX BRDF (Metallic/Roughness) |
| SSAO | ✅ | 32 采样半球核 + 4×4 噪声旋转 + 模糊 |
| SSR | ✅ | 视图空间 Ray Marching 64 步 + HDR 采样 |
| 实例化渲染 | ✅ | Dynamic VBO + glDrawArraysInstanced (万级实例) |
| HDR + Bloom | ✅ | Reinhard 色调映射 + 高斯模糊泛光 |
| 阴影映射 | ✅ | 方向光 Shadow Map + PCF 软阴影 |
| 法线贴图 | ✅ | TBN 矩阵，CPU 预计算 |
| 粒子系统 | ✅ | GPU Instancing |
| 程序化天空盒 | ✅ | 三层渐变 + 太阳光晕 |
| ECS 架构 | ✅ | Entity-Component-System |
| AABB 物理 | ✅ | 碰撞检测 + 射线检测 + Sphere 碰撞 |
| 场景编辑器 | ✅ | ImGui 集成 (8 组件 Inspector + Performance) |
| Python AI | ✅ | pybind11 桥接 |
| 音频系统 | ✅ | miniaudio (3D 空间音频) |
| glTF 加载 | ✅ | cgltf |
| OBJ 加载 | ✅ | 含切线计算 |
| 视锥剔除 | ✅ | — |
| 暗角效果 | ✅ | — |
| Vulkan 后端 | 🔜 | 计划中 |

## 架构

```text
engine/       ← C++ 引擎静态库 (核心渲染/物理/ECS/音频)
sandbox/      ← 测试沙盒应用
ai/           ← Python AI 模块 (pybind11 桥接, 可选)
third_party/  ← 第三方依赖
tests/        ← 单元测试 (Google Test)
docs/         ← 文档与基准测试
```

### ECS 设计

- **存储**: 基于类型擦除的 `ComponentPool`（`unordered_map<Entity, unique_ptr<Component>>`，AoS 布局）
- **实体 ID**: 32 位递增 ID，`CreateEntity()` 自动附加 `TagComponent`
- **遍历**: 模板化 `ForEach<T>()` 回调，避免 `std::function` 堆分配
- **系统**: 继承 `System` 基类，串行更新（内置 `MovementSystem`、`LifetimeSystem`）
- **当前适用规模**: 中小场景（<1000 实体）
- **优化路线**: 计划迁移至 SoA 稀疏集存储，支持多线程系统并行

### 延迟渲染管线

```text
Pass 0: Shadow Map       → 方向光深度贴图 (PCF 软阴影)
Pass 1: G-Buffer 几何     → MRT (Position / Normal / Albedo+Metallic / Emissive+Roughness)
Pass 2: SSAO             → 32 采样半球核 + 4×4 旋转噪声 → Blur
Pass 3: 延迟 PBR 光照     → G-Buffer + 阴影 + AO → HDR FBO
Pass 4: SSR              → 视图空间 Ray Marching → 反射混合
Pass 5: 前向叠加          → 天空盒 / 透明物 / 自发光 / 粒子 / 调试线
Pass 6: Bloom + 后处理    → 亮度提取 → 高斯模糊 → Reinhard 色调映射 → 屏幕
```

### 资源管理

- 全局 `ResourceManager` 统一管理 Shader / Texture / Mesh 缓存
- 当前所有资源为 **同步加载（主线程）**，无线程安全风险
- 计划: 预留 `LoadAsync` 接口，配合 `std::future` 实现异步加载

### Python AI 定位

- **可选组件**：通过 `ENGINE_ENABLE_PYTHON` 编译开关控制（默认 OFF）
- **非运行时必需**：禁用时有完整 stub 实现，引擎功能不受影响
- **调用频率**：AI 更新 0.5 秒/次，不在渲染热路径中

## 技术栈

| 层级 | 技术 |
| ------ | ------ |
| 语言 | C17 / C++20 |
| 图形 | OpenGL 4.5 (GLSL 450) |
| 窗口/输入 | GLFW 3.x |
| 数学 | GLM |
| 图像 | stb_image |
| GL 加载 | 自定义 GLAD |
| 模型 | cgltf (glTF), 自定义 OBJ |
| 音频 | miniaudio |
| UI | Dear ImGui |
| AI 桥接 | pybind11 (C++ ↔ Python) |
| 构建 | CMake 3.20+ |
| CI | GitHub Actions (Windows MSVC + Ubuntu GCC) |

## 构建

### 前置要求

- CMake 3.20+
- C++20 编译器（MSVC 2022 / GCC 11+ / MinGW-w64）

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
# 安装依赖 (Ubuntu/Debian)
sudo apt-get install libgl1-mesa-dev libx11-dev libxrandr-dev \
    libxinerama-dev libxcursor-dev libxi-dev

cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/sandbox/Sandbox
```

### 构建选项

| 选项 | 默认 | 说明 |
|------|:----:|------|
| `BUILD_TESTS` | OFF | 构建单元测试 (Google Test) |
| `ENGINE_ENABLE_PYTHON` | OFF | 启用 Python AI 层 |

```bash
# 示例：构建并运行测试
cmake -B build -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cd build && ctest --output-on-failure
```

## 引擎功能详解

### 渲染

- **延迟渲染管线** (G-Buffer MRT + 全屏延迟 PBR 光照)
- **PBR 材质** (Cook-Torrance GGX BRDF, Metallic/Roughness 工作流)
- **SSAO** (Screen Space Ambient Occlusion, 32 采样 + Blur)
- **SSR** (Screen Space Reflections, 视图空间 Ray Marching)
- **实例化渲染** (Dynamic VBO + glDrawArraysInstanced, 万级实例)
- HDR 帧缓冲 + Reinhard 色调映射 + 伽马校正
- Bloom 后处理 (亮度提取 → 高斯模糊 → 混合)
- 阴影映射 (方向光 Shadow Map + PCF 软阴影)
- 法线贴图 (TBN 矩阵，CPU 预计算法线矩阵)
- 粒子系统 (GPU Instancing)
- 程序化天空盒 (三层渐变 + 太阳光晕)
- 暗角效果 / 视锥剔除

### 核心

- Entity-Component-System (ECS) / 场景管理器
- 统一资源管理 (Shader/Texture/Mesh 缓存)
- 事件系统 / FPS 相机控制器

### 物理

- AABB 碰撞检测 (含穿透方向+深度)
- 射线检测 (Ray vs AABB / Plane)
- 刚体组件 + 碰撞回调

### 调试

- DebugDraw (线段/AABB/球体/射线/网格/坐标轴)
- DebugUI (屏幕文本叠加)
- Profiler (代码段计时 + 历史曲线)

## 贡献

欢迎贡献代码！请阅读 [CONTRIBUTING.md](CONTRIBUTING.md) 了解开发流程和代码规范。

## 鸣谢

本项目使用了以下优秀的开源库：

| 库 | 用途 | 许可证 |
|----|------|--------|
| [GLFW](https://www.glfw.org/) | 窗口/输入管理 | Zlib |
| [GLM](https://github.com/g-truc/glm) | 数学库 | MIT |
| [stb](https://github.com/nothings/stb) | 图像加载 | Public Domain |
| [Dear ImGui](https://github.com/ocornut/imgui) | 调试 UI / 编辑器 | MIT |
| [miniaudio](https://miniaud.io/) | 音频系统 | Public Domain |
| [cgltf](https://github.com/jkuhlmann/cgltf) | glTF 解析 | MIT |
| [pybind11](https://github.com/pybind/pybind11) | C++/Python 桥接 | BSD |
| [GLAD](https://glad.dav1d.de/) | OpenGL 加载 | MIT |

## 许可证

本项目使用 [MIT License](LICENSE) 授权。
