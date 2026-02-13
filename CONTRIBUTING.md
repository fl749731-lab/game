# 贡献指南

感谢你对 Game Engine 项目的兴趣！以下是参与贡献的指南。

## 开发环境

### 前置要求

- CMake 3.20+
- C++20 编译器（MinGW-w64 / MSVC 2022 / GCC 11+）
- Git

### 构建

```bash
# MinGW
set PATH=C:\msys64\mingw64\bin;C:\msys64\usr\bin;%PATH%
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build

# 运行测试
cd build && ctest --output-on-failure
```

## 代码风格

| 规则 | 说明 |
|------|------|
| 命名空间 | 所有引擎代码在 `Engine` 命名空间下 |
| 类型别名 | 使用 `types.h` 中的 `u32`, `f32`, `Scope<T>`, `Ref<T>` 等 |
| 日志 | 使用 `LOG_INFO` / `LOG_WARN` / `LOG_ERROR` 宏 |
| 命名 | 类/结构: `PascalCase`，函数: `PascalCase`，变量: `camelCase`，成员: `m_PascalCase` |
| 注释 | 中文注释优先，关键 API 使用 Doxygen 风格 |

## PR 流程

1. Fork 并创建功能分支：`feature/你的功能名`
2. 确保所有测试通过
3. 提交 PR，描述你的更改内容
4. 等待代码审查

## 项目结构

```
engine/include/engine/  ← 公开头文件
engine/src/             ← 实现文件
sandbox/                ← 测试应用
tests/                  ← 单元测试
third_party/            ← 第三方依赖
```

## 问题反馈

- 使用 Issue 模板报告 Bug 或提出功能请求
- 描述尽可能详细，附带复现步骤
