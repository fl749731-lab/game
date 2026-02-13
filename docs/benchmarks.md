# 性能基准

## 测试环境

| 项目 | 配置 |
|------|------|
| CPU | — |
| GPU | — |
| RAM | — |
| OS | — |
| 编译器 | MinGW-w64 GCC |
| 构建类型 | Release |

## 测试场景

### 场景 1: 基础场景

- 100 个立方体 + 方向光 + 阴影
- 启用 Bloom + HDR

| 指标 | 数值 |
|------|------|
| 平均帧时间 | — ms |
| 平均 FPS | — |
| 帧时间 P99 | — ms |

### 场景 2: 粒子密集

- 10,000 粒子 + GPU Instancing
- 启用 Bloom

| 指标 | 数值 |
|------|------|
| 平均帧时间 | — ms |
| 平均 FPS | — |

### 场景 3: ECS 压力测试

- 10,000 个实体 + Transform + Velocity
- MovementSystem 每帧更新

| 指标 | 数值 |
|------|------|
| 系统更新耗时 | — ms |
| 创建 10K 实体耗时 | — ms |

## 使用引擎内置 Profiler

```cpp
PROFILE_SCOPE("MyFunction");
// ... 被测代码 ...
```

Profiler 会输出每个代码段的平均耗时和历史曲线，可在 Debug UI 中查看。

---

> 💡 请填写实际测试结果后提交。使用 `Release` 构建以获得准确数据。
