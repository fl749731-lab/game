#version 450

// ── Uniforms ───────────────────────────────────────────────
layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4  uView;
    mat4  uProjection;
    vec4  uCameraPos;
    vec4  uLightDir;    // xyz=direction, w=intensity
    vec4  uLightColor;  // xyz=color,     w=ambient
};

layout(set = 0, binding = 1) uniform sampler2D uTexture;

// ── Input from Vertex Shader ───────────────────────────────
layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vTexCoord;

// ── Output ─────────────────────────────────────────────────
layout(location = 0) out vec4 outColor;

void main() {
    // 采样纹理
    vec4 texColor = texture(uTexture, vTexCoord);

    // Blinn-Phong 光照
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-uLightDir.xyz);
    float intensity = uLightDir.w;
    float ambient   = uLightColor.w;

    // Diffuse
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = uLightColor.rgb * NdotL * intensity;

    // Specular (Blinn)
    vec3 V = normalize(uCameraPos.xyz - vWorldPos);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 64.0);
    vec3 specular = uLightColor.rgb * spec * intensity * 0.5;

    // 合成
    vec3 finalColor = texColor.rgb * (ambient + diffuse) + specular;

    // 简单 Reinhard 色调映射
    finalColor = finalColor / (finalColor + 1.0);
    // Gamma 校正
    finalColor = pow(finalColor, vec3(1.0 / 2.2));

    outColor = vec4(finalColor, texColor.a);
}
