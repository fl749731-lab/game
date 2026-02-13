#include "engine/renderer/g_buffer.h"
#include "engine/core/log.h"

#include <glad/glad.h>

namespace Engine {

std::unique_ptr<Framebuffer> GBuffer::s_FBO;
u32 GBuffer::s_Width = 0;
u32 GBuffer::s_Height = 0;

void GBuffer::Init(u32 width, u32 height) {
    s_Width = width;
    s_Height = height;

    FramebufferSpec spec;
    spec.Width = width;
    spec.Height = height;
    spec.DepthAttachment = true;
    spec.ColorFormats = {
        TextureFormat::RGB16F,   // RT0: Position
        TextureFormat::RGB16F,   // RT1: Normal
        TextureFormat::RGBA8,    // RT2: Albedo (RGB) + Specular (A)
        TextureFormat::RGBA8,    // RT3: Emissive (RGB) + Reserved (A)
    };

    s_FBO = std::make_unique<Framebuffer>(spec);
    LOG_INFO("[G-Buffer] 初始化完成 %ux%u (4 RT + Depth)", width, height);
}

void GBuffer::Shutdown() {
    s_FBO.reset();
    LOG_DEBUG("[G-Buffer] 已销毁");
}

void GBuffer::Resize(u32 width, u32 height) {
    if (width == s_Width && height == s_Height) return;
    s_Width = width;
    s_Height = height;
    if (s_FBO) s_FBO->Resize(width, height);
    LOG_INFO("[G-Buffer] 尺寸变更 → %ux%u", width, height);
}

void GBuffer::Bind() {
    if (s_FBO) s_FBO->Bind();
}

void GBuffer::Unbind() {
    if (s_FBO) s_FBO->Unbind();
}

void GBuffer::BindTextures(u32 startUnit) {
    if (!s_FBO) return;
    for (u32 i = 0; i < s_FBO->GetColorAttachmentCount(); i++) {
        glActiveTexture(GL_TEXTURE0 + startUnit + i);
        glBindTexture(GL_TEXTURE_2D, s_FBO->GetColorAttachmentID(i));
    }
}

u32 GBuffer::GetPositionTexture()   { return s_FBO ? s_FBO->GetColorAttachmentID(0) : 0; }
u32 GBuffer::GetNormalTexture()     { return s_FBO ? s_FBO->GetColorAttachmentID(1) : 0; }
u32 GBuffer::GetAlbedoSpecTexture() { return s_FBO ? s_FBO->GetColorAttachmentID(2) : 0; }
u32 GBuffer::GetEmissiveTexture()   { return s_FBO ? s_FBO->GetColorAttachmentID(3) : 0; }
u32 GBuffer::GetDepthTexture()      { return s_FBO ? s_FBO->GetDepthAttachmentID() : 0; }

u32 GBuffer::GetFBO()    { return s_FBO ? s_FBO->GetFBO() : 0; }
u32 GBuffer::GetWidth()  { return s_Width; }
u32 GBuffer::GetHeight() { return s_Height; }

} // namespace Engine
