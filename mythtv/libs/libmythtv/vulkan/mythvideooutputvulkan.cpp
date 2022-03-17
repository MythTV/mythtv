// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/vulkan/mythdebugvulkan.h"
#include "libmythui/vulkan/mythpaintervulkan.h"
#include "libmythui/vulkan/mythwindowvulkan.h"

#include "osd.h"
#include "vulkan/mythvideooutputvulkan.h"
#include "vulkan/mythvideovulkan.h"

#define LOC QString("VidOutVulkan: ")

void MythVideoOutputVulkan::GetRenderOptions(RenderOptions &Options)
{
    QStringList safe(VULKAN_RENDERER);

    (*Options.safe_renderers)["dummy"].append(safe);
    (*Options.safe_renderers)["ffmpeg"].append(safe);

    for (auto & decoder : *Options.decoders)
        if (decoder.endsWith("-dec"))
            (*Options.safe_renderers)[decoder].append(safe);

    Options.renderers->append(VULKAN_RENDERER);
    Options.priorities->insert(VULKAN_RENDERER, 75);
}

QStringList MythVideoOutputVulkan::GetAllowedRenderers(MythCodecID CodecId)
{
    QStringList allowed;

    if (MythRenderVulkan::GetVulkanRender() == nullptr)
        return allowed;

    if (codec_sw_copy(CodecId))
    {
        allowed << VULKAN_RENDERER;
        return allowed;
    }

    return allowed;
}

MythVideoOutputVulkan::MythVideoOutputVulkan(MythMainWindow* MainWindow, MythRenderVulkan* Render,
                                             MythPainterVulkan* Painter, MythDisplay* Display,
                                             const MythVideoProfilePtr& VideoProfile, QString& Profile)
  : MythVideoOutputGPU(MainWindow, Render, Painter, Display, VideoProfile, Profile),
    MythVulkanObject(Render)
{
    static VideoFrameTypes s_vulkanRenderFormats =
    {
        FMT_YV12,     FMT_NV12,      FMT_YUV422P,   FMT_YUV444P,
        FMT_YUV420P9, FMT_YUV420P10, FMT_YUV420P12, FMT_YUV420P14, FMT_YUV420P16,
        FMT_YUV422P9, FMT_YUV422P10, FMT_YUV422P12, FMT_YUV422P14, FMT_YUV422P16,
        FMT_YUV444P9, FMT_YUV444P10, FMT_YUV444P12, FMT_YUV444P14, FMT_YUV444P16,
        FMT_P010,     FMT_P016
    };

    m_renderFormats = &s_vulkanRenderFormats;
    if (IsValidVulkan())
    {
        m_video = new MythVideoVulkan(this, &m_videoColourSpace, this, m_videoProfile, QString {});
        if (m_video && !m_video->IsValid())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create valid Vulkan video");
            delete m_video;
            m_video = nullptr;
        }
    }

    if (!(IsValidVulkan() && m_painter && m_video))
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise Vulkan video output");
}

bool MythVideoOutputVulkan::Init(const QSize VideoDim, const QSize VideoDispDim,
                                 float Aspect, const QRect DisplayVisibleRect, MythCodecID CodecId)
{
    if (!(IsValidVulkan() && m_painter && m_video))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Fatal error");
        return false;
    }

    if (!gCoreContext->IsUIThread())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot initialise from this thread");
        return false;
    }

    return MythVideoOutputGPU::Init(VideoDim, VideoDispDim, Aspect, DisplayVisibleRect, CodecId);
}

void MythVideoOutputVulkan::PrepareFrame(MythVideoFrame* Frame, FrameScanType Scan)
{
    MythVideoOutputGPU::PrepareFrame(Frame, Scan);
}

void MythVideoOutputVulkan::RenderFrame(MythVideoFrame* Frame, FrameScanType Scan)
{
    if (!(IsValidVulkan() && m_video))
        return;

    // input changes need to be handled in ProcessFrame
    if (m_newCodecId != kCodec_NONE)
        return;

    // Start the frame...
    m_video->StartFrame();

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
    {
        m_vulkanRender->BeginDebugRegion(m_vulkanWindow->currentCommandBuffer(),
                                         "RENDER_FRAME", MythDebugVulkan::kDebugBlue);
    }

    // Actual render
    MythVideoOutputGPU::RenderFrame(Frame, Scan);
}

void MythVideoOutputVulkan::RenderEnd()
{
    if (!(IsValidVulkan()))
        return;

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_vulkanRender->EndDebugRegion(m_vulkanWindow->currentCommandBuffer());
}

void MythVideoOutputVulkan::EndFrame()
{
    if (m_video)
        m_video->EndFrame();
}
