# 🎮 Game Engine

一个从零构建的通用 2D/3D 游戏引擎，使用 C/C++ 开发，采用多语言架构。

<!-- 在此添加引擎渲染效果截图 -->
<!-- ![渲染效果](docs/screenshots/demo.png) -->

> 🚧 **活跃开发中** — 个人学习项目，持续迭代。

## ✨ 特性一览

| 特性 | 状态 | 说明 |
|------|:----:|------|
| HDR + Bloom | ✅ | Reinhard 色调映射 + 高斯模糊泛光 |
| 阴影映射 | ✅ | 方向光 Shadow Map + PCF 软阴影 |
| 法线贴图 | ✅ | TBN 矩阵，CPU 预计算 |
| 粒子系统 | ✅ | GPU Instancing |
| 程序化天空盒 | ✅ | 三层渐变 + 太阳光晕 |
| ECS 架构 | ✅ | Entity-Component-System |
| AABB 物理 | ✅ | 碰撞检测 + 射线检测 |
| 场景编辑器 | ✅ | ImGui 集成 |
| Python AI | ✅ | pybind11 桥接 |
| 音频系统 | ✅ | miniaudio |
| glTF 加载 | ✅ | cgltf |
| OBJ 加载 | ✅ | 含切线计算 |
| 视锥剔除 | ✅ | — |
| 暗角效果 | ✅ | — |
| Vulkan 后端 | 🔜 | 计划中 |

## 架构

```
engine/       ← C++ 引擎静态库 (核心渲染/物理/ECS/音频)
sandbox/      ← 测试沙盒应用
ai/           ← Python AI 模块 (pybind11 桥接)
third_party/  ← 第三方依赖
tests/        ← 单元测试 (Google Test)
docs/         ← 文档与基准测试
```

## 技术栈

| 层级 | 技术 |
|------|------|
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

- 前向渲染管线 (Lit + Emissive 双 Shader)
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
