#include "engine/renderer/sprite_batch.h"
#include "engine/renderer/font.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>

namespace Engine {

// ── 顶点结构 ────────────────────────────────────────────────

struct SpriteVertex {
    glm::vec2 Position;
    glm::vec2 TexCoord;
    glm::vec4 Color;
    f32 TexIndex;  // 0 = 白色纹理 (纯色), 1~15 = 用户纹理
};

// ── 内置着色器 ──────────────────────────────────────────────

static const char* s_SpriteVS = R"(
#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
layout(location = 3) in float aTexIndex;

out vec2 vTexCoord;
out vec4 vColor;
out float vTexIndex;

uniform mat4 uProjection;

void main() {
    vTexCoord = aTexCoord;
    vColor = aColor;
    vTexIndex = aTexIndex;
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
)";

static const char* s_SpriteFS = R"(
#version 450 core
in vec2 vTexCoord;
in vec4 vColor;
in float vTexIndex;

out vec4 FragColor;

uniform sampler2D uTextures[16];

void main() {
    int idx = int(vTexIndex);
    vec4 texColor = texture(uTextures[idx], vTexCoord);
    FragColor = texColor * vColor;
}
)";

// ── 静态状态 ────────────────────────────────────────────────

static u32 s_VAO = 0;
static u32 s_VBO = 0;
static u32 s_EBO = 0;
static Ref<Shader> s_Shader = nullptr;
static u32 s_WhiteTexture = 0;

static SpriteVertex* s_VertexBuffer = nullptr;
static SpriteVertex* s_VertexPtr = nullptr;
static u32 s_QuadCount = 0;
static u32 s_DrawCalls = 0;

static u32 s_TextureSlots[SpriteBatch::MAX_TEXTURE_SLOTS];
static u32 s_TextureSlotIndex = 1; // slot 0 = 白色纹理

static glm::mat4 s_Projection = glm::mat4(1.0f);

// ── 初始化 ──────────────────────────────────────────────────

void SpriteBatch::Init() {
    // 顶点缓冲
    s_VertexBuffer = new SpriteVertex[MAX_VERTICES];

    glGenVertexArrays(1, &s_VAO);
    glGenBuffers(1, &s_VBO);
    glGenBuffers(1, &s_EBO);

    glBindVertexArray(s_VAO);

    // VBO (动态)
    glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_VERTICES * sizeof(SpriteVertex), nullptr, GL_DYNAMIC_DRAW);

    // 顶点属性
    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex),
                          (void*)offsetof(SpriteVertex, Position));
    // TexCoord
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex),
                          (void*)offsetof(SpriteVertex, TexCoord));
    // Color
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex),
                          (void*)offsetof(SpriteVertex, Color));
    // TexIndex
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex),
                          (void*)offsetof(SpriteVertex, TexIndex));

    // 索引缓冲 (预生成)
    auto* indices = new u32[MAX_INDICES];
    u32 offset = 0;
    for (u32 i = 0; i < MAX_INDICES; i += 6) {
        indices[i + 0] = offset + 0;
        indices[i + 1] = offset + 1;
        indices[i + 2] = offset + 2;
        indices[i + 3] = offset + 2;
        indices[i + 4] = offset + 3;
        indices[i + 5] = offset + 0;
        offset += 4;
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_INDICES * sizeof(u32), indices, GL_STATIC_DRAW);
    delete[] indices;

    glBindVertexArray(0);

    // 白色纹理 (1x1)
    u32 whiteData = 0xFFFFFFFF;
    glGenTextures(1, &s_WhiteTexture);
    glBindTexture(GL_TEXTURE_2D, s_WhiteTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &whiteData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Shader
    s_Shader = std::make_shared<Shader>(s_SpriteVS, s_SpriteFS);
    s_Shader->Bind();
    // 逐个设置 sampler uniform (GLAD 未导出 glUniform1iv)
    for (int i = 0; i < (int)MAX_TEXTURE_SLOTS; i++) {
        std::string name = "uTextures[" + std::to_string(i) + "]";
        glUniform1i(glGetUniformLocation(s_Shader->GetID(), name.c_str()), i);
    }

    // 纹理槽初始化
    s_TextureSlots[0] = s_WhiteTexture;
    for (u32 i = 1; i < MAX_TEXTURE_SLOTS; i++) s_TextureSlots[i] = 0;

    LOG_INFO("[SpriteBatch] 初始化完成 (最大 %u 四边形/批次)", MAX_QUADS);
}

void SpriteBatch::Shutdown() {
    delete[] s_VertexBuffer;
    s_VertexBuffer = nullptr;

    if (s_VAO) { glDeleteVertexArrays(1, &s_VAO); s_VAO = 0; }
    if (s_VBO) { glDeleteBuffers(1, &s_VBO); s_VBO = 0; }
    if (s_EBO) { glDeleteBuffers(1, &s_EBO); s_EBO = 0; }
    if (s_WhiteTexture) { glDeleteTextures(1, &s_WhiteTexture); s_WhiteTexture = 0; }
    s_Shader.reset();
    LOG_DEBUG("[SpriteBatch] 已清理");
}

// ── 开始/结束 ───────────────────────────────────────────────

void SpriteBatch::Begin(u32 screenWidth, u32 screenHeight) {
    s_Projection = glm::ortho(0.0f, (f32)screenWidth, (f32)screenHeight, 0.0f, -1.0f, 1.0f);
    s_DrawCalls = 0;
    StartBatch();
}

void SpriteBatch::StartBatch() {
    s_QuadCount = 0;
    s_VertexPtr = s_VertexBuffer;
    s_TextureSlotIndex = 1;
}

void SpriteBatch::End() {
    Flush();
}

void SpriteBatch::Flush() {
    if (s_QuadCount == 0) return;

    u32 dataSize = (u32)((u8*)s_VertexPtr - (u8*)s_VertexBuffer);

    // 绑定纹理
    for (u32 i = 0; i < s_TextureSlotIndex; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, s_TextureSlots[i]);
    }

    // 上传顶点
    glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, dataSize, s_VertexBuffer);

    // 渲染
    s_Shader->Bind();
    s_Shader->SetMat4("uProjection", glm::value_ptr(s_Projection));

    // 保存当前 GL 状态
    GLint prevDepthTest, prevBlend;
    glGetIntegerv(GL_DEPTH_TEST, &prevDepthTest);
    glGetIntegerv(GL_BLEND, &prevBlend);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(s_VAO);
    glDrawElements(GL_TRIANGLES, s_QuadCount * 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    // 恢复 GL 状态
    if (prevDepthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (!prevBlend) glDisable(GL_BLEND);

    s_DrawCalls++;
    StartBatch();
}

// ── 绘制 ────────────────────────────────────────────────────

void SpriteBatch::Draw(Ref<Texture2D> texture,
                       const glm::vec2& position,
                       const glm::vec2& size,
                       f32 rotation,
                       const glm::vec4& tint) {
    if (s_QuadCount >= MAX_QUADS) Flush();

    // 查找或分配纹理槽位
    f32 texIndex = 0.0f;
    u32 texID = texture ? texture->GetID() : 0;
    if (texID != 0) {
        // 查找是否已绑定
        for (u32 i = 1; i < s_TextureSlotIndex; i++) {
            if (s_TextureSlots[i] == texID) {
                texIndex = (f32)i;
                break;
            }
        }
        // 未找到 → 分配新槽位
        if (texIndex == 0.0f) {
            if (s_TextureSlotIndex >= MAX_TEXTURE_SLOTS) Flush();
            s_TextureSlots[s_TextureSlotIndex] = texID;
            texIndex = (f32)s_TextureSlotIndex;
            s_TextureSlotIndex++;
        }
    }

    // 四个角的位置 (旋转支持)
    glm::vec2 positions[4];
    if (std::abs(rotation) < 0.001f) {
        // 无旋转 — 快速路径
        positions[0] = position;                                          // 左上
        positions[1] = {position.x + size.x, position.y};                // 右上
        positions[2] = {position.x + size.x, position.y + size.y};       // 右下
        positions[3] = {position.x, position.y + size.y};                // 左下
    } else {
        // 绕中心旋转
        glm::vec2 center = position + size * 0.5f;
        f32 c = cosf(rotation);
        f32 s = sinf(rotation);
        glm::vec2 halfSize = size * 0.5f;
        glm::vec2 corners[4] = {
            {-halfSize.x, -halfSize.y},
            { halfSize.x, -halfSize.y},
            { halfSize.x,  halfSize.y},
            {-halfSize.x,  halfSize.y},
        };
        for (int i = 0; i < 4; i++) {
            positions[i] = center + glm::vec2(
                corners[i].x * c - corners[i].y * s,
                corners[i].x * s + corners[i].y * c
            );
        }
    }

    // UV 坐标
    glm::vec2 uvs[4] = {{0,0}, {1,0}, {1,1}, {0,1}};

    // 写入 4 个顶点
    for (int i = 0; i < 4; i++) {
        s_VertexPtr->Position = positions[i];
        s_VertexPtr->TexCoord = uvs[i];
        s_VertexPtr->Color = tint;
        s_VertexPtr->TexIndex = texIndex;
        s_VertexPtr++;
    }

    s_QuadCount++;
}

void SpriteBatch::DrawRect(const glm::vec2& position,
                           const glm::vec2& size,
                           const glm::vec4& color,
                           f32 rotation) {
    Draw(nullptr, position, size, rotation, color);
}

void SpriteBatch::DrawText(Font& font,
                           const std::string& text,
                           const glm::vec2& position,
                           f32 scale,
                           const glm::vec4& color) {
    if (!font.IsValid()) return;

    // 查找或分配字体纹理槽位
    u32 fontTexID = font.GetTextureID();
    f32 texIndex = 0.0f;
    for (u32 i = 1; i < s_TextureSlotIndex; i++) {
        if (s_TextureSlots[i] == fontTexID) {
            texIndex = (f32)i;
            break;
        }
    }
    if (texIndex == 0.0f && fontTexID != 0) {
        if (s_TextureSlotIndex >= MAX_TEXTURE_SLOTS) Flush();
        s_TextureSlots[s_TextureSlotIndex] = fontTexID;
        texIndex = (f32)s_TextureSlotIndex;
        s_TextureSlotIndex++;
    }

    f32 cursorX = position.x;
    f32 cursorY = position.y;

    for (char c : text) {
        if (c == '\n') {
            cursorX = position.x;
            cursorY += font.GetLineHeight() * scale;
            continue;
        }

        if (s_QuadCount >= MAX_QUADS) {
            Flush();
            // Flush 重置了纹理槽，重新分配字体纹理
            if (fontTexID != 0) {
                s_TextureSlots[s_TextureSlotIndex] = fontTexID;
                texIndex = (f32)s_TextureSlotIndex;
                s_TextureSlotIndex++;
            }
        }

        const auto& g = font.GetGlyph(c);

        f32 x = cursorX + g.OffsetX * scale;
        f32 y = cursorY + g.OffsetY * scale + font.GetLineHeight() * scale;
        f32 w = g.Width * scale;
        f32 h = g.Height * scale;

        // 4 个顶点 (左上、右上、右下、左下)
        s_VertexPtr->Position = {x, y};
        s_VertexPtr->TexCoord = {g.U0, g.V0};
        s_VertexPtr->Color = color;
        s_VertexPtr->TexIndex = texIndex;
        s_VertexPtr++;

        s_VertexPtr->Position = {x + w, y};
        s_VertexPtr->TexCoord = {g.U1, g.V0};
        s_VertexPtr->Color = color;
        s_VertexPtr->TexIndex = texIndex;
        s_VertexPtr++;

        s_VertexPtr->Position = {x + w, y + h};
        s_VertexPtr->TexCoord = {g.U1, g.V1};
        s_VertexPtr->Color = color;
        s_VertexPtr->TexIndex = texIndex;
        s_VertexPtr++;

        s_VertexPtr->Position = {x, y + h};
        s_VertexPtr->TexCoord = {g.U0, g.V1};
        s_VertexPtr->Color = color;
        s_VertexPtr->TexIndex = texIndex;
        s_VertexPtr++;

        s_QuadCount++;
        cursorX += g.Advance * scale;
    }
}

// ── 统计 ────────────────────────────────────────────────────

u32 SpriteBatch::GetDrawCallCount() { return s_DrawCalls; }
u32 SpriteBatch::GetQuadCount()     { return s_QuadCount; }

} // namespace Engine
