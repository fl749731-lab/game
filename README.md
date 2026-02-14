# 🎮 Game Engine

[![Build Engine](https://github.com/fl749731-lab/game/actions/workflows/build.yml/badge.svg)](https://github.com/fl749731-lab/game/actions/workflows/build.yml)
**[English](README_EN.md) | 中文**

一个从零构建的通用 2D/3D 游戏引擎，使用 C/C++ 开发。

> 🚧 **活跃开发中** — 个人学习项目，持续迭代。

## ✨ 特性一览

### 渲染

| 特性 | 状态 | 说明 |
| --- | :---: | --- |
| 延迟渲染 | ✅ | G-Buffer MRT + 全屏延迟光照 Pass |
| PBR 材质 | ✅ | Cook-Torrance GGX BRDF (Metallic/Roughness) |
| IBL 环境光照 | ✅ | HDR→Cubemap + 辐照度图 + 预滤波环境图 + BRDF LUT |
| SSAO | ✅ | 32 采样半球核 + 4×4 噪声旋转 + 模糊 |
| SSR | ✅ | 视图空间 Ray Marching 64 步 + HDR 采样 |
| 实例化渲染 | ✅ | Dynamic VBO + glDrawArraysInstanced (万级实例) |
| HDR + Bloom | ✅ | Reinhard 色调映射 + 高斯模糊泛光 |
| 阴影映射 | ✅ | 方向光 Shadow Map + CSM 级联 + PCF 软阴影 |
| GPU 骨骼蒙皮 | ✅ | 128 骨骼 × 4 权重，整型骨骼ID属性 |
| 法线贴图 | ✅ | TBN 矩阵，CPU 预计算 |
| 粒子系统 | ✅ | GPU Instancing |
| 程序化天空盒 | ✅ | 三层渐变 + 太阳光晕 |
| Overdraw 可视化 | ✅ | 片元叠加计数 + 热力图 (黑→蓝→绿→黄→红→白) |
| 暗角效果 / 视锥剔除 | ✅ | — |
| Vulkan 后端 | ✅ | 完整功能对等: Mipmap / PBR 材质 / G-Buffer MRT / 延迟光照 / Shadow Map (PCF) / Bloom / 后处理 (HDR+Gamma+晕影) / SSAO / SSR / 程序化天空盒 / IBL (BRDF LUT) / Compute Pipeline |

### 编辑器工具

| 特性 | 状态 | 说明 |
| --- | :---: | --- |
| 场景编辑器 | ✅ | ImGui 集成，多面板布局 |
| 层级面板 | ✅ | 场景树 + 拖拽排序 + 搜索过滤 |
| 检查器面板 | ✅ | 8 组件属性 Inspector (Transform/Light/Material...) |
| 节点图编辑器 | ✅ | 通用可视化节点图 (拖拽/连线/撤销/分类菜单) |
| 材质编辑器 | ✅ | 基于节点图的可视化材质编辑 + 预览 |
| 粒子编辑器 | ✅ | 实时粒子预览 + 4 种预设 + 参数调节 |
| 时间线编辑器 | ✅ | 多轨时间线 + 关键帧菱形标记 + 缩放/平移 |
| 曲线编辑器 | ✅ | Hermite 样条曲线 + 关键帧编辑 |
| 颜色拾取器 | ✅ | HSV 色轮 + HDR + 渐变编辑 + 吸管工具 + 收藏夹 |
| 资源浏览器 | ✅ | 文件系统浏览 + 图标网格/列表视图 |
| Gizmo | ✅ | 平移/旋转/缩放 3D Gizmo |
| 拖放系统 | ✅ | 跨面板拖拽 (实体/资源/颜色) |
| 热重载 | ✅ | 文件监控 + Shader/脚本自动重载 |
| Docking 布局 | ✅ | 多面板布局管理 |
| 截图系统 | ✅ | 帧缓冲截图 + 序列帧录制 |
| Prefab 系统 | ✅ | 预制件保存/实例化 |

### 核心引擎

| 特性 | 状态 | 说明 |
| --- | :---: | --- |
| ECS 架构 | ✅ | Entity-Component-System |
| AABB / OBB 物理 | ✅ | 碰撞检测 + 射线检测 + BVH 加速 + OBB/球/胶囊 |
| 骨骼动画系统 | ✅ | 采样/混合/Crossfade/状态机/分层遮罩/IK/Root Motion/事件 |
| 场景序列化 | ✅ | JSON Save/Load，16+ 组件全覆盖 |
| 脚本逻辑层 | ✅ | ScriptSystem + EngineAPI (30+ Python 接口) |
| Python AI | ✅ | pybind11 桥接 (巡逻/追击/防御) |
| 层级指挥链 AI | ✅ | 指挥官→小队长→士兵 三层决策 + 玩家意图记忆 |
| 音频系统 | ✅ | miniaudio (3D 空间音频 + 混音器) |
| glTF / OBJ 加载 | ✅ | cgltf + 蒙皮/骨骼/动画解析 + 自定义 OBJ (含切线计算) |
| 多线程 JobSystem | ✅ | 线程池 + ParallelFor (物理/ECS 并行) |
| 异步资源加载 | ✅ | AsyncLoader: 后台解码 → 主线程 GPU 上传 |

### 调试与性能

| 特性 | 状态 | 说明 |
| --- | :---: | --- |
| DebugDraw | ✅ | 线段/AABB/球体/射线/网格/坐标轴 |
| GPU Profiler | ✅ | Timer Query 计时 + ImGui 可视化 |
| 性能图表 | ✅ | 实时帧率/内存/系统曲线图 |
| 统计系统 | ✅ | Draw Call/Triangle/Entity 计数 |
| 控制台 | ✅ | 命令行控制台 + 日志过滤 |
| 引擎诊断 | ✅ | GL 上下文 / 内存 / 系统信息 |

## 架构

```text
engine/       ← C++ 引擎静态库 (核心渲染/物理/ECS/音频/编辑器)
sandbox/      ← 测试沙盒应用
ai/           ← 脚本逻辑层 (通用脚本 + 层级 AI 行为, 可选)
data/         ← Java 数据层 (JNI 桥接, 可选)
third_party/  ← 第三方依赖 (glfw, glad, glm, stb, imgui, cgltf, miniaudio, pybind11)
tests/        ← 单元测试 (Google Test)
docs/         ← 文档与基准测试
```

### ECS 设计

- **存储**: `ComponentArray<T>` — Sparse Set **SoA** 布局（Dense 数据紧密排列，缓存友好；Sparse 映射 O(1) 查找；swap-and-pop 删除）
- **实体 ID**: 32 位递增 ID，`CreateEntity()` 自动附加 `TagComponent`
- **遍历**: 模板化 `ForEach<T>()` 线性扫描 + `ForEach2<T1,T2>()` 双组件联合查询（以小池驱动）
- **并行**: `ParallelForEach<T>()` 多线程分块遍历（基于 `JobSystem` 线程池）
- **层级**: 父子关系 (`SetParent` / `GetRootEntities`) + `TransformSystem` 递归计算世界矩阵
- **系统**: 继承 `System` 基类，串行更新（内置 `MovementSystem`、`LifetimeSystem`、`TransformSystem`、`ScriptSystem`）
- **组件**: Transform、Tag、Health、Velocity、AI、Squad、Script、Render、Material、RotationAnim、Collider、RigidBody、Animator 等 16+ 组件
- **直接访问**: `RawData()` / `RawEntities()` 暴露底层数组指针，供高性能批处理使用

### 延迟渲染管线

```text
Pass 0: Shadow Map       → 方向光深度贴图 (CSM 级联 + PCF 软阴影)
Pass 1: G-Buffer 几何     → MRT (Position / Normal / Albedo+Metallic / Emissive+Roughness)
Pass 2: SSAO             → 32 采样半球核 + 4×4 旋转噪声 → Blur
Pass 3: 延迟 PBR 光照     → G-Buffer + 阴影 + AO + IBL → HDR FBO
Pass 4: SSR              → 视图空间 Ray Marching → 反射混合
Pass 5: 前向叠加          → 天空盒 / 透明物 / 自发光 / 粒子 / 调试线
Pass 6: Bloom + 后处理    → 亮度提取 → 高斯模糊 → Reinhard 色调映射 → 屏幕
Pass *: Overdraw 可视化   → 片元叠加计数 → 热力图覆盖 (可选)
```

### 骨骼动画管线

```text
glTF 加载 → Skeleton + AnimationClip → AnimationSampler (CPU 关键帧插值)
                                      → PoseBlender (姿势混合/Crossfade)
                                      → AnimStateMachine (FSM 状态驱动)
                                      → AnimLayerStack (基础层 + 遮罩叠加层)
                                      → TwoBoneIK / FABRIK (逆运动学)
                                      → RootMotionExtractor (XZ/XYZ 增量)
                                      → AnimEventDispatcher (帧事件回调)
                                      → SkinningUtils → skinning.glsl (GPU 蒙皮)
```

### 资源管理

- 全局 `ResourceManager` 统一管理 Shader / Texture / Mesh 缓存
- **异步加载**: `AsyncLoader` 利用 `JobSystem` 线程池在工作线程做 CPU 解码 (stbi_load)，主线程每帧 `FlushUploads()` 限量上传 GPU
- 支持同步 (`LoadTexture`) 和异步 (`LoadTextureAsync`) 两种 API，100% 向后兼容

### 脚本逻辑层

- **通用脚本**: `ScriptComponent` + `ScriptSystem`，任意实体可挂 Python 脚本
- **生命周期**: `on_create` / `on_update` / `on_event` / `on_destroy`
- **EngineAPI**: 30+ 个 Python 接口操作引擎 (Transform/Physics/Entity/Event/Audio)
- **AI 行为**: `AIComponent` + `AIManager` 专用于 NPC 行为决策
- **层级指挥链**: 指挥官→小队长→士兵 三层决策，命令逐层下发
- **玩家意图识别**: `PlayerTracker` 采集玩家行为 → 指挥官 AI 分析模式并记忆
- **阵型系统**: 三角/一字/散开/楔形 4 种阵型，动态位置计算
- **可选组件**: 通过 `ENGINE_ENABLE_PYTHON` 编译开关控制（默认 OFF）

## 技术栈

| 层级 | 技术 |
| ------ | ------ |
| 语言 | C17 / C++20 |
| 图形 | OpenGL 4.5 (GLSL 450), Vulkan 1.3 (可选) |
| 窗口/输入 | GLFW 3.x |
| 数学 | GLM |
| 图像/字体 | stb_image, stb_image_write, stb_truetype |
| GL 加载 | 自定义 GLAD |
| 模型 | cgltf (glTF 2.0 + 蒙皮), 自定义 OBJ |
| 音频 | miniaudio |
| UI | Dear ImGui |
| AI 桥接 | pybind11 (C++ ↔ Python) |
| 构建 | CMake 3.20+ |
| CI | GitHub Actions (Windows MSVC + Ubuntu GCC) |
| 数据桥接 | JNI (C++ ↔ Java, 可选) |

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
| --- | :---: | --- |
| `BUILD_TESTS` | OFF | 构建单元测试 (Google Test) |
| `ENGINE_ENABLE_PYTHON` | OFF | 启用 Python AI 层 |
| `ENGINE_ENABLE_VULKAN` | OFF | 启用 Vulkan 渲染后端 |
| `ENGINE_ENABLE_JAVA` | OFF | 启用 Java 数据层 (JNI) |

```bash
# 示例：启用 Vulkan 并构建
cmake -B build -DENGINE_ENABLE_VULKAN=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## 贡献

欢迎贡献代码！请阅读 [CONTRIBUTING.md](CONTRIBUTING.md) 了解开发流程和代码规范。

## 鸣谢

本项目使用了以下优秀的开源库：

| 库 | 用途 | 许可证 |
| --- | --- | --- |
| [GLFW](https://www.glfw.org/) | 窗口/输入管理 | Zlib |
| [GLM](https://github.com/g-truc/glm) | 数学库 | MIT |
| [stb](https://github.com/nothings/stb) | 图像加载/写入 + 字体渲染 | Public Domain |
| [Dear ImGui](https://github.com/ocornut/imgui) | 调试 UI / 编辑器 | MIT |
| [miniaudio](https://miniaud.io/) | 音频系统 | Public Domain |
| [cgltf](https://github.com/jkuhlmann/cgltf) | glTF 解析 | MIT |
| [pybind11](https://github.com/pybind/pybind11) | C++/Python 桥接 | BSD |
| [GLAD](https://glad.dav1d.de/) | OpenGL 加载 | MIT |

## 许可证

本项目使用 [MIT License](LICENSE) 授权。
