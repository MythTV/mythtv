// MythTV
#include "mythlogging.h"
#include "mythmainwindow.h"
#include "osd.h"
#include "vulkan/mythwindowvulkan.h"
#include "vulkan/mythpaintervulkan.h"
#include "vulkan/mythdebugvulkan.h"
#include "vulkan/mythvideovulkan.h"
#include "vulkan/mythvideooutputvulkan.h"

#define LOC QString("VidOutVulkan: ")

VideoFrameTypeVec MythVideoOutputVulkan::s_vulkanFrameTypes =
{
    FMT_YV12,     FMT_NV12,      FMT_YUV422P,   FMT_YUV444P,
    FMT_YUV420P9, FMT_YUV420P10, FMT_YUV420P12, FMT_YUV420P14, FMT_YUV420P16,
    FMT_YUV422P9, FMT_YUV422P10, FMT_YUV422P12, FMT_YUV422P14, FMT_YUV422P16,
    FMT_YUV444P9, FMT_YUV444P10, FMT_YUV444P12, FMT_YUV444P14, FMT_YUV444P16,
    FMT_P010,     FMT_P016
};

void MythVideoOutputVulkan::GetRenderOptions(RenderOptions &Options)
{
    QStringList safe(VULKAN_RENDERER);

    (*Options.safe_renderers)["dummy"].append(safe);
    (*Options.safe_renderers)["nuppel"].append(safe);
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

MythVideoOutputVulkan::MythVideoOutputVulkan(QString &Profile)
  : MythVideoOutputGPU(Profile)
{
    m_renderFrameTypes = &s_vulkanFrameTypes;

    m_vulkanRender = MythRenderVulkan::GetVulkanRender();
    if (m_vulkanRender)
    {
        // Note - strictly we shouldn't be using reference counting for MythRenderVulkan
        // as QVulkanWindow takes ownership. We need to ensure it is shared however,
        // otherwise the painter window is hidden by the incredibly annoying disabling of
        // drawing in MythMainWindow. There is no reason why QVulkanWindow should
        // delete it while video is playing though.
        m_vulkanRender->IncrRef();
        m_render= m_vulkanRender;
        m_vulkanWindow = m_vulkanRender->GetVulkanWindow();
        if (m_vulkanWindow)
        {
            m_device = m_vulkanWindow->device();
            if (m_device)
                m_devFuncs = m_vulkanWindow->vulkanInstance()->deviceFunctions(m_device);
        }
    }

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO) && m_vulkanRender && m_device && m_devFuncs && m_vulkanWindow)
        m_debugMarker = MythDebugVulkan::Create(m_vulkanRender, m_device, m_devFuncs, m_vulkanWindow);

    m_video = new MythVideoVulkan(m_vulkanRender, &m_videoColourSpace, this, true, QString {});

    if (!(m_vulkanRender && m_painter && m_video && m_device && m_devFuncs && m_vulkanWindow))
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise Vulkan video output");
}

MythVideoOutputVulkan::~MythVideoOutputVulkan()
{
    delete m_debugMarker;
}

bool MythVideoOutputVulkan::Init(const QSize& VideoDim, const QSize& VideoDispDim,
                                 float Aspect, MythDisplay* Display,
                                 const QRect& DisplayVisibleRect, MythCodecID CodecId)
{
    if (!(m_vulkanRender && m_painter && m_video && m_device && m_devFuncs && m_vulkanWindow))
        return false;

    if (!gCoreContext->IsUIThread())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot initialise from this thread");
        return false;
    }

    if (!InitGPU(VideoDim, VideoDispDim, Aspect, Display, DisplayVisibleRect, CodecId))
        return false;

    return true;
}

void MythVideoOutputVulkan::PrepareFrame(VideoFrame* Frame, const PIPMap& PiPPlayers, FrameScanType Scan)
{
    ProcessFrameGPU(Frame, PiPPlayers, Scan);
}

void MythVideoOutputVulkan::RenderFrame(VideoFrame* Frame, FrameScanType Scan, OSD* Osd)
{
    if (!(m_vulkanRender && m_vulkanWindow && m_video))
        return;

    // input changes need to be handled in ProcessFrame
    if (m_newCodecId != kCodec_NONE)
        return;

    // Start the frame...
    m_video->StartFrame();

    VkCommandBuffer currentcmdbuffer = m_vulkanWindow->currentCommandBuffer();
    if (m_debugMarker && currentcmdbuffer)
        m_debugMarker->BeginRegion(currentcmdbuffer, "PREPARE_FRAME", MythDebugVulkan::s_DebugBlue);

    // FIXME GetWindowRect() is a placeholder
    RenderFrameGPU(Frame, Scan, Osd, GetWindowRect());

    if (m_debugMarker && currentcmdbuffer)
        m_debugMarker->EndRegion(currentcmdbuffer);
}

void MythVideoOutputVulkan::EndFrame()
{
    if (m_video)
        m_video->EndFrame();
}

MythVideoGPU* MythVideoOutputVulkan::CreateSecondaryVideo(const QSize& VideoDim,
                                                          const QSize& VideoDispDim,
                                                          const QRect& DisplayVisibleRect,
                                                          const QRect& DisplayVideoRect,
                                                          const QRect& VideoRect)
{
    auto * colourspace = new MythVideoColourSpace(&m_videoColourSpace);
    auto * result = new MythVideoVulkan(m_render, colourspace,
                                        VideoDim, VideoDispDim,
                                        DisplayVisibleRect, DisplayVideoRect,
                                        VideoRect, false, QString{});
    colourspace->DecrRef();
    return result;
}
