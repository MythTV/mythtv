// MythTV
#include "videooutwindow.h"
#include "mythvideogpu.h"

MythVideoGPU::MythVideoGPU(MythRender *Render, MythVideoColourSpace* ColourSpace,
                           const VideoOutWindow& Window,
                           bool ViewportControl, QString Profile)
  : m_render(Render),
    m_profile(std::move(Profile)),
    m_videoDispDim(Window.GetVideoDispDim()),
    m_videoDim(Window.GetVideoDim()),
    m_masterViewportSize(Window.GetDisplayVisibleRect().size()),
    m_displayVideoRect(Window.GetDisplayVideoRect()),
    m_videoRect(Window.GetVideoRect()),
    m_videoColourSpace(ColourSpace),
    m_inputTextureSize(Window.GetVideoDim()),
    m_viewportControl(ViewportControl)
{
    CommonInit();

    connect(&Window, &VideoOutWindow::VideoSizeChanged,  this,   &MythVideoGPU::SetVideoDimensions);
    connect(&Window, &VideoOutWindow::VideoRectsChanged, this,   &MythVideoGPU::SetVideoRects);
    connect(&Window, &VideoOutWindow::WindowRectChanged, this,   &MythVideoGPU::SetViewportRect);
    connect(this,    &MythVideoGPU::OutputChanged,      &Window, &VideoOutWindow::InputChanged);
}

MythVideoGPU::MythVideoGPU(MythRender* Render, MythVideoColourSpace* ColourSpace,
                           QSize VideoDim, QSize VideoDispDim, QRect DisplayVisibleRect,
                           QRect DisplayVideoRect, QRect VideoRect,
                           bool ViewportControl, QString Profile)
  : m_render(Render),
    m_profile(std::move(Profile)),
    m_videoDispDim(VideoDispDim),
    m_videoDim(VideoDim),
    m_masterViewportSize(DisplayVisibleRect.size()),
    m_displayVideoRect(DisplayVideoRect),
    m_videoRect(VideoRect),
    m_videoColourSpace(ColourSpace),
    m_inputTextureSize(VideoDim),
    m_viewportControl(ViewportControl)
{
    CommonInit();
}

MythVideoGPU::~MythVideoGPU()
{
    if (m_render)
        m_render->DecrRef();
    if (m_videoColourSpace)
        m_videoColourSpace->DecrRef();
}

void MythVideoGPU::CommonInit()
{
    if (m_render)
        m_render->IncrRef();

    if (m_videoColourSpace)
    {
        m_videoColourSpace->IncrRef();
        connect(m_videoColourSpace, &MythVideoColourSpace::Updated, this, &MythVideoGPU::UpdateColourSpace);
    }
}

/// \note QObject::connect with function pointers does not work when the slot
/// is overridden and connected from the base class- so use an intermediate slot.
void MythVideoGPU::UpdateColourSpace(bool PrimariesChanged)
{
    ColourSpaceUpdate(PrimariesChanged);
}

bool MythVideoGPU::IsValid() const
{
    return m_valid;
}

void MythVideoGPU::SetProfile(const QString& Profile)
{
    m_profile = Profile;
}

QString MythVideoGPU::GetProfile() const
{
    return m_profile;
}

void MythVideoGPU::SetMasterViewport(QSize Size)
{
    m_masterViewportSize = Size;
}

QSize MythVideoGPU::GetVideoDim() const
{
    return m_videoDim;
}

void MythVideoGPU::ResetFrameFormat()
{
    m_inputType        = FMT_NONE;
    m_outputType       = FMT_NONE;
    m_inputTextureSize = QSize();
    m_deinterlacer     = DEINT_NONE;
    // textures are created with Linear filtering - which matches no resize
    m_resizing         = None;
}

QString MythVideoGPU::VideoResizeToString(VideoResizing Resize)
{
    QStringList reasons;
    if ((Resize & Deinterlacer) == Deinterlacer) reasons << "Deinterlacer";
    if ((Resize & Sampling)     == Sampling)     reasons << "Sampling";
    if ((Resize & Performance)  == Performance)  reasons << "Performance";
    if ((Resize & Framebuffer)  == Framebuffer)  reasons << "Framebuffer";
    return reasons.join(",");
}

void MythVideoGPU::SetVideoDimensions(const QSize& VideoDim, const QSize& VideoDispDim)
{
    m_videoDim = VideoDim;
    m_videoDispDim = VideoDispDim;
}

void MythVideoGPU::SetVideoRects(const QRect& DisplayVideoRect, const QRect& VideoRect)
{
    m_displayVideoRect = DisplayVideoRect;
    m_videoRect = VideoRect;
}

void MythVideoGPU::SetViewportRect(const QRect& DisplayVisibleRect)
{
    SetMasterViewport(DisplayVisibleRect.size());
}
