// MythTV
#include "libmythbase/mythlogging.h"
#include "vulkan/mythvideotexturevulkan.h"

#define LOC QString("VulkanVidTex: ")

std::vector<MythVideoTextureVulkan*>
MythVideoTextureVulkan::CreateTextures(MythVulkanObject* Vulkan,
                                       VkCommandBuffer CommandBuffer,
                                       VideoFrameType Type,
                                       VideoFrameType Format,
                                       QSize Size)
{
    if (!Vulkan || !Vulkan->IsValidVulkan() || Size.isEmpty())
        return std::vector<MythVideoTextureVulkan*>{};

    if (MythVideoFrame::HardwareFormat(Type))
        return std::vector<MythVideoTextureVulkan*>{};

    return CreateSoftwareTextures(Vulkan, CommandBuffer, Type, Format, Size);
}

MythVideoTextureVulkan::MythVideoTextureVulkan([[maybe_unused]] VideoFrameType Type,
                                               [[maybe_unused]] VideoFrameType Format)
#if 0
  : m_frameType(Type),
    m_frameFormat(Format),
    m_planeCount(MythVideoFrame::GetNumPlanes(Format))
#endif
{
}

void MythVideoTextureVulkan::DeleteTextures(MythVulkanObject *Vulkan,
                                            VkCommandBuffer CommandBuffer,
                                            std::vector<MythVideoTextureVulkan*>& Textures)
{
    if (!(Vulkan && Vulkan->IsValidVulkan()))
        return;

    VkCommandBuffer cmdbuffer = nullptr;
    if (!CommandBuffer)
    {
        cmdbuffer = Vulkan->Render()->CreateSingleUseCommandBuffer();
        CommandBuffer = cmdbuffer;
    }

    for (auto * texture : Textures)
        MythVideoTextureVulkan::DeleteTexture(Vulkan, CommandBuffer, texture);

    if (cmdbuffer)
        Vulkan->Render()->FinishSingleUseCommandBuffer(cmdbuffer);

    Textures.clear();
}

void MythVideoTextureVulkan::DeleteTexture(MythVulkanObject* Vulkan,
                                           VkCommandBuffer CommandBuffer,
                                           MythVideoTextureVulkan* Texture)
{
    if (!(Vulkan && Vulkan->IsValidVulkan() && CommandBuffer && Texture))
        return;

    delete Texture;
}

std::vector<MythVideoTextureVulkan*>
MythVideoTextureVulkan::CreateSoftwareTextures([[maybe_unused]] MythVulkanObject* Vulkan,
                                               [[maybe_unused]] VkCommandBuffer CommandBuffer,
                                               [[maybe_unused]] VideoFrameType Type,
                                               VideoFrameType Format,
                                               QSize Size)
{
    std::vector<MythVideoTextureVulkan*> result;

    uint count = MythVideoFrame::GetNumPlanes(Format);
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
