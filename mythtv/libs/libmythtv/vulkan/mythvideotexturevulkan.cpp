// MythTV
#include "mythlogging.h"
#include "vulkan/mythvideotexturevulkan.h"

#define LOC QString("VulkanVidTex: ")

vector<MythVideoTextureVulkan*> MythVideoTextureVulkan::CreateTextures(MythRenderVulkan* Render,
                                                                       VkDevice Device,
                                                                       QVulkanDeviceFunctions* Functions,
                                                                       VkCommandBuffer CommandBuffer,
                                                                       VideoFrameType Type,
                                                                       VideoFrameType Format,
                                                                       QSize Size)
{
    if (!(Render && Device && Functions && CommandBuffer && !Size.isEmpty()))
        return vector<MythVideoTextureVulkan*>{};

    if (format_is_hw(Type))
        return vector<MythVideoTextureVulkan*>{};

    return CreateSoftwareTextures(Render, Device, Functions, CommandBuffer,
                                  Type, Format, Size);
}

MythVideoTextureVulkan::MythVideoTextureVulkan(VideoFrameType Type, VideoFrameType Format)
{
    m_frameType = Type;
    m_frameFormat = Format;
    m_valid = false;
    m_plane = 0;
    m_planeCount = planes(Format);
}

void MythVideoTextureVulkan::DeleteTextures(MythRenderVulkan* Render, VkDevice Device,
                                            QVulkanDeviceFunctions* Functions,
                                            VkCommandBuffer CommandBuffer,
                                            vector<MythVideoTextureVulkan*>& Textures)
{
    if (!(Render && Device && Functions))
        return;

    VkCommandBuffer cmdbuffer = nullptr;
    if (!CommandBuffer)
    {
        cmdbuffer = Render->CreateSingleUseCommandBuffer();
        CommandBuffer = cmdbuffer;
    }

    for (auto * texture : Textures)
        MythVideoTextureVulkan::DeleteTexture(Render, Device, Functions, CommandBuffer, texture);

    if (cmdbuffer)
        Render->FinishSingleUseCommandBuffer(cmdbuffer);

    Textures.clear();
}

void MythVideoTextureVulkan::DeleteTexture(MythRenderVulkan* Render, VkDevice Device,
                                           QVulkanDeviceFunctions* Functions,
                                           VkCommandBuffer CommandBuffer,
                                           MythVideoTextureVulkan* Texture)
{
    if (!(Render && Device && Functions && CommandBuffer && Texture))
        return;

    delete Texture;
}

vector<MythVideoTextureVulkan*> MythVideoTextureVulkan::CreateSoftwareTextures(MythRenderVulkan* Render,
                                                                               VkDevice Device,
                                                                               QVulkanDeviceFunctions* Functions,
                                                                               VkCommandBuffer CommandBuffer,
                                                                               VideoFrameType Type,
                                                                               VideoFrameType Format,
                                                                               QSize Size)
{
    (void)Render;
    (void)Device;
    (void)Functions;
    (void)CommandBuffer;
    (void)Type;

    vector<MythVideoTextureVulkan*> result;

    uint count = planes(Format);
    if (count < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid software frame format");
        return result;
    }

    for (uint plane = 0; plane < count; ++plane)
    {
        QSize size = Size;
        MythVideoTextureVulkan* texture = nullptr;
        switch (Format)
        {
            case FMT_YV12:
                if (plane > 0)
                    size = QSize(size.width() >> 1, size.height() >> 1);
                //texture = CreateTexture(Context, size, Target,
                //              QOpenGLTexture::UInt8, r8pixelfmtnorm, r8internalfmt);
                break;
            default: break;
        }

        if (texture)
            result.push_back(texture);
    }

    return result;
}
