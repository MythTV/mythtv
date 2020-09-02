// Qt
#include <QGuiApplication>

// MythTV
#include "mythlogging.h"
#include "vulkan/mythdebugvulkan.h"
#include "vulkan/mythwindowvulkan.h"
#include "vulkan/mythpaintervulkan.h"
#include "vulkan/mythvideoshadersvulkan.h"
#include "vulkan/mythvideotexturevulkan.h"
#include "vulkan/mythvideovulkan.h"

#define LOC QString("VulkanVideo: ")

MythVideoVulkan::MythVideoVulkan(MythRender* Render, MythVideoColourSpace* ColourSpace,
                                 MythVideoBounds* Bounds, bool ViewportControl,
                                 const QString& Profile)
  : MythVideoGPU(Render, ColourSpace, Bounds, ViewportControl, Profile)
{
    MythVideoVulkan::Init();
}

MythVideoVulkan::MythVideoVulkan(MythRender* Render, MythVideoColourSpace* ColourSpace,
                                 QSize VideoDim, QSize VideoDispDim, QRect DisplayVisibleRect,
                                 QRect DisplayVideoRect, QRect VideoRect,
                                 bool ViewportControl, const QString& Profile)
  : MythVideoGPU(Render, ColourSpace, VideoDim, VideoDispDim, DisplayVisibleRect,
                 DisplayVideoRect, VideoRect, ViewportControl, Profile)
{
    MythVideoVulkan::Init();
}

MythVideoVulkan::~MythVideoVulkan()
{
    MythVideoVulkan::ResetFrameFormat();
    delete m_debugMarker;
}

void MythVideoVulkan::Init()
{
    if (!m_videoColourSpace)
        return;

    m_vulkanRender = MythRenderVulkan::GetVulkanRender();
    if (m_vulkanRender)
    {
        m_vulkanWindow = m_vulkanRender->GetVulkanWindow();
        if (m_vulkanWindow)
        {
            m_device = m_vulkanWindow->device();
            if (m_device)
            {
                m_devFuncs = m_vulkanWindow->vulkanInstance()->deviceFunctions(m_device);
                m_valid = m_devFuncs != nullptr;
            }
        }
    }

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO) && m_vulkanRender && m_device && m_devFuncs && m_vulkanWindow)
        m_debugMarker = MythDebugVulkan::Create(m_vulkanRender, m_device, m_devFuncs, m_vulkanWindow);
}

void MythVideoVulkan::ResetFrameFormat()
{
    MythVideoTextureVulkan::DeleteTextures(m_vulkanRender, m_device,
                                           m_devFuncs, nullptr, m_inputTextures);
    MythVideoGPU::ResetFrameFormat();
}

bool MythVideoVulkan::SetupFrameFormat(VideoFrameType InputType, VideoFrameType OutputType, QSize Size, VkCommandBuffer CmdBuffer)
{
    LOG(VB_GENERAL, LOG_INFO, LOC +
        QString("New frame format: %1:%2 %3x%4 -> %6:%7 %8x%9")
        .arg(format_description(m_inputType)).arg(format_description(m_outputType))
        .arg(m_videoDim.width()).arg(m_videoDim.height())
        .arg(format_description(InputType)).arg(format_description(OutputType))
        .arg(Size.width()).arg(Size.height()));

    ResetFrameFormat();

    m_inputType  = InputType;
    m_outputType = OutputType;
    m_videoDim   = Size;

    // Create textures
    m_inputTextures = MythVideoTextureVulkan::CreateTextures(m_vulkanRender, m_device,
                                                             m_devFuncs, CmdBuffer,
                                                             m_inputType, m_outputType,
                                                             m_videoDim);

    // Create shaders

    // Update video colourspace

    return true;
}

void MythVideoVulkan::StartFrame()
{
    if (!m_valid)
        return;

    // Tell the renderer that we are requesting a frame start
    m_vulkanRender->SetFrameExpected();

    // Signal DIRECTLY to the window to start the frame - which ensures
    // the event is not delayed and we can start to render immediately.
    QEvent update(QEvent::UpdateRequest);
    QGuiApplication::sendEvent(m_vulkanWindow, &update);
}

void MythVideoVulkan::PrepareFrame(VideoFrame* Frame, FrameScanType /*Scan*/)
{
    if (!m_valid || (Frame->codec == FMT_NONE))
        return;

    // No hardware frame support yet
    if (format_is_hw(Frame->codec))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid hardware video frame");
        return;
    }

    // Sanitise frame
    if ((Frame->width < 1) || (Frame->height < 1) || !Frame->buf)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid video frame");
        return;
    }

    // Can we render this frame format
    if (!format_is_yuv(Frame->codec))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Frame format is not supported");
        return;
    }

    VkCommandBuffer cmdbuffer = nullptr;

    // check for input changes
    if ((Frame->width  != m_videoDim.width()) || (Frame->height != m_videoDim.height()) ||
        (Frame->codec  != m_inputType))
    {
        VideoFrameType frametype = Frame->codec;
        QSize size(Frame->width, Frame->height);
        cmdbuffer = m_vulkanRender->CreateSingleUseCommandBuffer();
        if (!SetupFrameFormat(Frame->codec, frametype, size, cmdbuffer))
        {
            m_vulkanRender->FinishSingleUseCommandBuffer(cmdbuffer);
            return;
        }
    }

    m_videoColourSpace->UpdateColourSpace(Frame);

    m_discontinuityCounter = Frame->frameCounter;


    if (!cmdbuffer)
        cmdbuffer = m_vulkanRender->CreateSingleUseCommandBuffer();

    //MythVideoTexture::UpdateTextures(m_render, Frame, current ? m_inputTextures : m_nextTextures);

    if (m_debugMarker && cmdbuffer)
        m_debugMarker->BeginRegion(cmdbuffer, "PREPARE_FRAME", MythDebugVulkan::s_DebugBlue);

    if (m_debugMarker && cmdbuffer)
        m_debugMarker->EndRegion(cmdbuffer);

    m_vulkanRender->FinishSingleUseCommandBuffer(cmdbuffer);
}

void MythVideoVulkan::RenderFrame(VideoFrame* /*Frame*/, bool /*TopFieldFirst*/,
                                  FrameScanType /*Scan*/, StereoscopicMode /*Stereo*/,
                                  bool /*DrawBorder*/)
{
}

void MythVideoVulkan::EndFrame()
{
    if (m_valid)
        m_vulkanRender->EndFrame();
}

void MythVideoVulkan::ColourSpaceUpdate(bool /*PrimariesChanged*/)
{
}


