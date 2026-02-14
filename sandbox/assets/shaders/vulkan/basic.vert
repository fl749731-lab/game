#version 450

// ── Uniforms ───────────────────────────────────────────────
layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4  uView;
    mat4  uProjection;
    vec4  uCameraPos;
    vec4  uLightDir;    // xyz=direction, w=intensity
    vec4  uLightColor;  // xyz=color,     w=ambient
};

// ── Push Constants ─────────────────────────────────────────
layout(push_constant) uniform PushConstants {
    mat4 uModel;
};

// ── Vertex Input ───────────────────────────────────────────
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

// ── Output ─────────────────────────────────────────────────
layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vTexCoord;

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal   = mat3(transpose(inverse(uModel))) * aNormal;
    vTexCoord = aTexCoord;

    gl_Position = uProjection * uView * worldPos;
}
