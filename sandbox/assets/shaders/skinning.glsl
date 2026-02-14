#version 450 core

// ── 骨骼蒙皮顶点着色器 ──────────────────────────────────────
// 支持最多 128 骨骼、每顶点 4 骨骼权重
// 用法: 在常规 PBR/Deferred 着色器的顶点阶段 #include 此文件

// ── 顶点输入 ────────────────────────────────────────────────
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;
layout(location = 5) in ivec4 aBoneIDs;     // 骨骼索引 (最多4个)
layout(location = 6) in vec4  aWeights;      // 骨骼权重 (最多4个)

// ── Uniforms ────────────────────────────────────────────────
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uBones[128];
uniform bool uHasSkinning;                   // 是否启用骨骼蒙皮

uniform mat4 uLightSpaceMatrix;              // 阴影贴图用

// ── 输出到片段着色器 ────────────────────────────────────────
out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out vec3 vTangent;
out vec3 vBitangent;
out vec4 vLightSpacePos;

void main() {
    mat4 skinMatrix = mat4(1.0);

    if (uHasSkinning) {
        skinMatrix = mat4(0.0);

        // 累加骨骼变换 (最多4个骨骼影响)
        for (int i = 0; i < 4; i++) {
            if (aBoneIDs[i] >= 0 && aBoneIDs[i] < 128) {
                skinMatrix += aWeights[i] * uBones[aBoneIDs[i]];
            }
        }

        // 权重归一化保护
        float totalWeight = aWeights.x + aWeights.y + aWeights.z + aWeights.w;
        if (totalWeight > 0.0 && abs(totalWeight - 1.0) > 0.001) {
            skinMatrix /= totalWeight;
        }
    }

    // 最终世界空间位置
    vec4 worldPos = uModel * skinMatrix * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;

    // 法线变换 (使用法线矩阵)
    mat3 normalMatrix = mat3(transpose(inverse(uModel * skinMatrix)));
    vNormal    = normalize(normalMatrix * aNormal);
    vTangent   = normalize(normalMatrix * aTangent);
    vBitangent = normalize(normalMatrix * aBitangent);

    vUV = aUV;

    // 阴影空间坐标
    vLightSpacePos = uLightSpaceMatrix * worldPos;

    gl_Position = uProjection * uView * worldPos;
}
