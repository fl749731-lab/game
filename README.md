# 🎮 Game Engine

一个从零构建的通用 2D/3D 游戏引擎，使用 C/C++ 开发，采用多语言架构。

## 架构

```
engine/       ← C++ 引擎静态库 (核心渲染/物理/ECS/音频)
sandbox/      ← 测试沙盒应用
ai/           ← Python AI 模块 (pybind11 桥接)
data/         ← Java 数据层 (JNI 桥接)
third_party/  ← 第三方依赖 (GLFW, GLAD, GLM, stb)
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
| 构建 | CMake + MinGW-w64 (MSYS2) |
| AI 桥接 | pybind11 (C++ ↔ Python) |
| 数据桥接 | JNI (C++ ↔ Java) |

## 引擎功能

### 渲染

- 前向渲染管线 (Lit + Emissive 双 Shader)
- HDR 帧缓冲 + Reinhard 色调映射 + 伽马校正
- Bloom 后处理 (亮度提取 → 高斯模糊 → 混合)
- 阴影映射 (方向光 Shadow Map + PCF 软阴影)
- 法线贴图 (TBN 矩阵，CPU 预计算法线矩阵)
- 粒子系统 (GPU Instancing)
- 程序化天空盒 (三层渐变 + 太阳光晕)
- 暗角效果
- 视锥剔除

### 核心

- Entity-Component-System (ECS)
- 场景管理器
- 统一资源管理 (Shader/Texture/Mesh 缓存)
- 事件系统
- FPS 相机控制器

### 物理

- AABB 碰撞检测 (含穿透方向+深度)
- 射线检测 (Ray vs AABB / Plane)
- 刚体组件 + 碰撞回调

### 调试

- DebugDraw (线段/AABB/球体/射线/网格/坐标轴)
- DebugUI (屏幕文本叠加)
- Profiler (代码段计时 + 历史曲线)

### 几何体

- 内置 Cube / Plane / Sphere
- OBJ 模型加载器 (含切线计算)

## 环境配置

### 前置要求

- [MSYS2](https://www.msys2.org/) (MinGW-w64 工具链)
- CMake 3.20+

### 构建步骤

```bash
# 1. 设置环境变量
set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%

# 2. 配置 CMake
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug

# 3. 编译
cmake --build build

# 4. 运行
.\build\sandbox\Sandbox.exe
```

## 项目状态

🚧 **活跃开发中** — 个人学习项目，持续迭代。

## 许可证

本项目使用 [MIT License](LICENSE) 授权。
