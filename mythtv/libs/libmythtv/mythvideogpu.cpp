// MythTV
#include "libmythbase/mythcorecontext.h"
#include "mythvideobounds.h"
#include "mythvideoprofile.h"
#include "mythvideogpu.h"

#define LOC QString("VideoGPU: ")

MythVideoGPU::MythVideoGPU(MythRender *Render, MythVideoColourSpace* ColourSpace,
                           MythVideoBounds* Bounds, const MythVideoProfilePtr &VideoProfile,
                           QString Profile)
  : m_render(Render),
    m_profile(std::move(Profile)),
    m_videoDispDim(Bounds->GetVideoDispDim()),
    m_videoDim(Bounds->GetVideoDim()),
    m_masterViewportSize(Bounds->GetDisplayVisibleRect().size()),
    m_displayVideoRect(Bounds->GetDisplayVideoRect()),
    m_videoRect(Bounds->GetVideoRect()),
    m_videoColourSpace(ColourSpace),
    m_inputTextureSize(Bounds->GetVideoDim())
{
    if (m_render)
        m_render->IncrRef();

    if (m_videoColourSpace)
    {
        m_videoColourSpace->IncrRef();
        // Not a call to UpdateColourSpace. Only a callback registration.
        connect(m_videoColourSpace, &MythVideoColourSpace::Updated,
                this, &MythVideoGPU::UpdateColourSpace);
    }

    m_stereoMode = gCoreContext->GetBoolSetting("DiscardStereo3D", true) ?
                    kStereoscopicModeSideBySideDiscard : kStereoscopicModeIgnore3D;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Discard stereoscopic fields: %1")
        .arg(m_stereoMode == kStereoscopicModeIgnore3D ? "No" : "Yes"));

    connect(Bounds, &MythVideoBounds::VideoSizeChanged,  this,   &MythVideoGPU::SetVideoDimensions);
    connect(Bounds, &MythVideoBounds::VideoRectsChanged, this,   &MythVideoGPU::SetVideoRects);
    connect(Bounds, &MythVideoBounds::WindowRectChanged, this,   &MythVideoGPU::SetViewportRect);
    connect(this,   &MythVideoGPU::OutputChanged,        Bounds, &MythVideoBounds::SourceChanged);

    if (VideoProfile)
    {
        UpscalerChanged(VideoProfile->GetUpscaler());
        connect(VideoProfile.get(), &MythVideoProfile::UpscalerChanged, this, &MythVideoGPU::UpscalerChanged);
    }
}

MythVideoGPU::~MythVideoGPU()
{
    if (m_render)
        m_render->DecrRef();
    if (m_videoColourSpace)
        m_videoColourSpace->DecrRef();
}

/// \note QObject::connect with function pointers does not work when the slot
/// is overridden and connected from the base class- so use an intermediate slot.
void MythVideoGPU::UpdateColourSpace(bool PrimariesChanged)
{
    ColourSpaceUpdate(PrimariesChanged);
}

void MythVideoGPU::UpscalerChanged(const QString& Upscaler)
{
    auto oldbicubic = m_bicubicUpsize;
    m_bicubicUpsize = Upscaler == UPSCALE_HQ1;
    if (m_bicubicUpsize != oldbicubic)
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("New upscaler preference: '%1'").arg(Upscaler));
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
    if ((Resize & ToneMap)      == ToneMap)      reasons << "Tonemapping";
    if ((Resize & Bicubic)      == Bicubic)      reasons << "Bicubic";
    return reasons.join(",");
}

void MythVideoGPU::SetVideoDimensions(QSize VideoDim, QSize VideoDispDim)
{
    m_videoDim = VideoDim;
    m_videoDispDim = VideoDispDim;
}

void MythVideoGPU::SetVideoRects(QRect DisplayVideoRect, QRect VideoRect)
{
    m_displayVideoRect = DisplayVideoRect;
    m_videoRect = VideoRect;
}

void MythVideoGPU::SetViewportRect(QRect DisplayVisibleRect)
{
    SetMasterViewport(DisplayVisibleRect.size());
}
