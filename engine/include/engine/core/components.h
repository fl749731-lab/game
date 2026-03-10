#pragma once

// ── 引擎组件定义 ────────────────────────────────────────────
// 从 ecs.h 拆分出的独立组件头文件。
// 每个组件都是纯数据结构 (POD-ish)，挂载到 Entity 上。

#include "engine/core/types.h"
#include "engine/core/ecs.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>
#include <unordered_map>

namespace Engine {

// ── Transform ───────────────────────────────────────────────

struct TransformComponent : public Component {
    // ── 局部变换（相对于父节点）──────────────────────────────
    f32 X = 0, Y = 0, Z = 0;
    f32 RotX = 0, RotY = 0, RotZ = 0;
    f32 ScaleX = 1, ScaleY = 1, ScaleZ = 1;

    // ── 层级关系 ─────────────────────────────────────────────
    Entity Parent = INVALID_ENTITY;
    std::vector<Entity> Children;

    // ── 世界矩阵缓存（由 TransformSystem 每帧更新）──────────
    glm::mat4 WorldMatrix = glm::mat4(1.0f);
    bool WorldMatrixDirty = true;

    // ── 局部矩阵构建 (TRS) ──────────────────────────────────
    glm::mat4 GetLocalMatrix() const {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), {X, Y, Z});
        m = glm::rotate(m, glm::radians(RotY), {0, 1, 0});
        m = glm::rotate(m, glm::radians(RotX), {1, 0, 0});
        m = glm::rotate(m, glm::radians(RotZ), {0, 0, 1});
        m = glm::scale(m, {ScaleX, ScaleY, ScaleZ});
        return m;
    }

    // ── 便捷方法 ─────────────────────────────────────────────
    glm::vec3 GetPosition() const { return {X, Y, Z}; }
    void SetPosition(const glm::vec3& p) { X = p.x; Y = p.y; Z = p.z; }
    glm::vec3 GetScale() const { return {ScaleX, ScaleY, ScaleZ}; }
    void SetScale(const glm::vec3& s) { ScaleX = s.x; ScaleY = s.y; ScaleZ = s.z; }
    void SetScale(f32 uniform) { ScaleX = ScaleY = ScaleZ = uniform; }

    /// 获取世界位置（从缓存的世界矩阵提取）
    glm::vec3 GetWorldPosition() const { return glm::vec3(WorldMatrix[3]); }
};

// ── Tag ─────────────────────────────────────────────────────

struct TagComponent : public Component {
    std::string Name = "Entity";
};

// ── Health ──────────────────────────────────────────────────

struct HealthComponent : public Component {
    f32 Current = 100.0f;
    f32 Max = 100.0f;
};

// ── Velocity ────────────────────────────────────────────────

struct VelocityComponent : public Component {
    f32 VX = 0, VY = 0, VZ = 0;
};

// ── AI ──────────────────────────────────────────────────────

struct AIComponent : public Component {
    std::string ScriptModule = "default_ai";
    std::string State = "Idle";
    f32 DetectRange = 10.0f;
    f32 AttackRange = 2.0f;
};

// ── Squad ───────────────────────────────────────────────────

struct SquadComponent : public Component {
    u32 SquadID = 0;                    // 所属小队 ID（0 = 无小队）
    u32 LeaderEntity = INVALID_ENTITY;  // 队长实体 ID
    u32 CommanderEntity = INVALID_ENTITY; // 指挥官实体 ID
    std::string Role = "soldier";       // "commander" | "leader" | "soldier"
    std::string CurrentOrder;           // 当前接收到的命令（JSON字符串）
    std::string OrderStatus = "idle";   // "idle" | "executing" | "completed" | "failed"
};

// ── Script ──────────────────────────────────────────────────

struct ScriptComponent : public Component {
    std::string ScriptModule;                                  // Python 模块名
    bool Initialized = false;                                  // on_create 是否已调用
    bool Enabled = true;                                       // 是否启用
    std::unordered_map<std::string, f32> FloatVars;           // 脚本自定义浮点变量
    std::unordered_map<std::string, std::string> StringVars;  // 脚本自定义字符串变量
};

// ── Render ──────────────────────────────────────────────────

struct RenderComponent : public Component {
    std::string MeshType = "cube";  // cube, sphere, plane, obj
    std::string ObjPath;
    // 兼容 — 新代码请使用 MaterialComponent
    f32 ColorR = 1, ColorG = 1, ColorB = 1;
    f32 Shininess = 32.0f;
};

// ── Material ────────────────────────────────────────────────

struct MaterialComponent : public Component {
    f32 DiffuseR = 0.8f, DiffuseG = 0.8f, DiffuseB = 0.8f;
    f32 SpecularR = 0.8f, SpecularG = 0.8f, SpecularB = 0.8f;
    f32 Shininess = 32.0f;
    f32 Roughness = 0.5f;       // PBR 粗糙度 (0=光滑 1=粗糙)
    f32 Metallic  = 0.0f;       // PBR 金属度 (0=非金属 1=金属)
    std::string TextureName;    // 空 = 无纹理
    std::string NormalMapName;  // 空 = 无法线贴图
    bool Emissive = false;      // 自发光物体 (跳过光照计算)
    f32 EmissiveR = 1.0f, EmissiveG = 1.0f, EmissiveB = 1.0f;
    f32 EmissiveIntensity = 1.0f;
};

// ── RotationAnim ────────────────────────────────────────────
/// 旋转动画组件 — 支持多轴自动旋转

struct RotationAnimComponent : public Component {
    f32 SpeedY = 0.6f;  // 绕 Y 轴旋转速度 (rad/s)
    f32 SpeedX = 0.2f;  // 绕 X 轴旋转速度 (rad/s)
    f32 SpeedZ = 0.0f;  // 绕 Z 轴旋转速度 (rad/s)
};

// ── Lifetime ────────────────────────────────────────────────
/// 生命周期组件 — 倒计时后自动销毁

struct LifetimeComponent : public Component {
    f32 TimeRemaining = 5.0f;
};

} // namespace Engine
