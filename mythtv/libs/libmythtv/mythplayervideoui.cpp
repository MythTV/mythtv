// MythTV
#include "libmythbase/mythlogging.h"

#include "mheg/interactivetv.h"
#include "mythplayervideoui.h"
#include "mythvideooutgpu.h"
#include "tv_play.h"

#define LOC QString("PlayerVideo: ")

MythPlayerVideoUI::MythPlayerVideoUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags)
  : MythPlayerCaptionsUI(MainWindow, Tv, Context, Flags)
{
    // Register our types for signalling
    qRegisterMetaType<MythVideoBoundsState>();
    qRegisterMetaType<MythVideoColourState>();
    connect(this, &MythPlayerVideoUI::CheckCallbacks, this, &MythPlayerVideoUI::ProcessCallbacks);
    m_interopTypes = MythInteropGPU::GetTypes(m_render);
}

/*! \brief Return a list of interop types supported by the current render device.
 *
 * \note This will be called from multiple threads but the types are set once
 * when playback starts and are not changed.
*/
const MythInteropGPU::InteropMap& MythPlayerVideoUI::GetInteropTypes() const
{
    return m_interopTypes;
}

bool MythPlayerVideoUI::InitVideo()
{
    if (!(m_playerCtx && m_decoder))
        return false;

    auto * video = MythVideoOutputGPU::Create(m_mainWindow, m_render, m_painter, m_display,
                                              m_decoder->GetCodecDecoderName(),
                                              m_decoder->GetVideoCodecID(),
                                              m_videoDim, m_videoDispDim, m_videoAspect,
                                              static_cast<float>(m_videoFrameRate),
                                              static_cast<uint>(m_playerFlags),
                                              m_codecName, m_maxReferenceFrames,
                                              m_renderFormats);

    if (!video)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Couldn't create VideoOutput instance. Exiting..");
        SetErrored(tr("Failed to initialize video output"));
        return false;
    }

    // Toggle detect letter box
    auto toggleDetectLetterbox = [&]()
    {
        if (m_videoOutput)
        {
            m_detectLetterBox.SetDetectLetterbox(!m_detectLetterBox.GetDetectLetterbox(),
                                                  m_videoOutput->GetAdjustFill());
        }
    };

    m_videoOutput = video;

    // Inbound connections
    connect(video, &MythVideoOutputGPU::PictureAttributeChanged,
                                                  this, &MythPlayerVideoUI::PictureAttributeChanged);
    connect(video, &MythVideoOutputGPU::SupportedAttributesChanged,
                                                  this, &MythPlayerVideoUI::SupportedAttributesChanged);
    connect(video, &MythVideoOutputGPU::PictureAttributesUpdated,
                                                  this, &MythPlayerVideoUI::PictureAttributesUpdated);
    connect(video, &MythVideoBounds::UpdateOSDMessage,
                                                  this, qOverload<const QString&>(&MythPlayerVideoUI::UpdateOSDMessage));
    connect(video, &MythVideoBounds::VideoBoundsStateChanged,
                                                  m_tv, &TV::VideoBoundsStateChanged);
    connect(m_tv,  &TV::ChangeOSDPositionUpdates, this,  &MythPlayerVideoUI::ChangeOSDPositionUpdates);
    connect(m_tv,  &TV::WindowResized,            this,  &MythPlayerVideoUI::ReinitOSD);
    connect(m_tv,  &TV::ChangeAdjustFill,         this,  &MythPlayerVideoUI::ToggleAdjustFill);
    connect(m_tv,  &TV::ChangeAspectOverride,     this,  &MythPlayerVideoUI::ReinitOSD);
    connect(m_tv,  &TV::ChangeZoom,               this,  &MythPlayerVideoUI::ReinitOSD);
    connect(m_tv,  &TV::ToggleMoveBottomLine,     this,  &MythPlayerVideoUI::ReinitOSD);
    connect(m_tv,  &TV::ToggleDetectLetterBox, toggleDetectLetterbox);

    // Passthrough signals
    connect(m_tv, &TV::ResizeScreenForVideo,     video, &MythVideoOutputGPU::ResizeForVideo);
    connect(m_tv, &TV::ChangePictureAttribute,   video, &MythVideoOutputGPU::ChangePictureAttribute);
    connect(m_tv, &TV::ChangeStereoOverride,     video, &MythVideoOutputGPU::SetStereoOverride);
    connect(m_tv, &TV::WindowResized,            video, &MythVideoOutputGPU::WindowResized);
    connect(m_tv, &TV::EmbedPlayback,            video, &MythVideoOutputGPU::EmbedPlayback);
    connect(m_tv, &TV::ChangeZoom,               video, &MythVideoOutputGPU::Zoom);
    connect(m_tv, &TV::ToggleMoveBottomLine,     video, &MythVideoOutputGPU::ToggleMoveBottomLine);
    connect(m_tv, &TV::SaveBottomLine,           video, &MythVideoOutputGPU::SaveBottomLine);
    connect(m_tv, &TV::ChangeAspectOverride,     video, &MythVideoOutputGPU::ToggleAspectOverride);
    connect(this, &MythPlayerVideoUI::ResizeForInteractiveTV,
                                                  video, &MythVideoOutputGPU::SetITVResize);
    connect(this, &MythPlayerVideoUI::VideoColourStateChanged,
                                                  m_tv, &TV::VideoColourStateChanged);
    connect(this, &MythPlayerVideoUI::RefreshVideoState, video, &MythVideoOutputGPU::RefreshState);

    // Update initial state. MythVideoOutput will have potentially adjusted state
    // at startup that we need to know about.
    emit RefreshVideoState();
    return true;
}

void MythPlayerVideoUI::SupportedAttributesChanged(PictureAttributeSupported Supported)
{
    if (Supported != m_colourState.m_supportedAttributes)
    {
        m_colourState.m_supportedAttributes = Supported;
        emit VideoColourStateChanged(m_colourState);
    }
}

void MythPlayerVideoUI::PictureAttributeChanged(PictureAttribute Attribute, int Value)
{
    QString text = toString(Attribute) + " " + toTypeString(kAdjustingPicture_Playback);
    UpdateOSDStatus(toTitleString(kAdjustingPicture_Playback), text, QString::number(Value),
                    kOSDFunctionalType_PictureAdjust, "%", Value * 10, kOSDTimeout_Med);
    ChangeOSDPositionUpdates(false);
    m_colourState.m_attributeValues[Attribute] = Value;
    emit VideoColourStateChanged(m_colourState);
};

void MythPlayerVideoUI::PictureAttributesUpdated(const std::map<PictureAttribute,int>& Values)
{
    m_colourState.m_attributeValues = Values;
    emit VideoColourStateChanged(m_colourState);
}

/*! \brief Convenience function to request and wait for a callback into the main thread.
 *
 * This is used by hardware decoders to ensure certain resources are created
 * and destroyed in the UI (render) thread.
*/
void MythPlayerVideoUI::HandleDecoderCallback(const QString& Debug, DecoderCallback::Callback Function,
                                              void* Opaque1, void* Opaque2)
{
    if (!Function)
        return;

    m_decoderCallbackLock.lock();
    QAtomicInt ready{0};
    QWaitCondition wait;
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Queuing callback for %1").arg(Debug));
    m_decoderCallbacks.append(DecoderCallback(Debug, Function, &ready, &wait, Opaque1, Opaque2));
    int count = 0;
    while (!ready && !wait.wait(&m_decoderCallbackLock, 100) && (count += 100))
        LOG(VB_GENERAL, LOG_WARNING, QString("Waited %1ms for %2").arg(count).arg(Debug));
    m_decoderCallbackLock.unlock();
}

void MythPlayerVideoUI::ProcessCallbacks()
{
    m_decoderCallbackLock.lock();
    // NOLINTNEXTLINE(readability-qualified-auto) for Qt6
    for (auto it = m_decoderCallbacks.begin(); it != m_decoderCallbacks.end(); ++it)
    {
        if (it->m_function)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Executing %1").arg(it->m_debug));
            it->m_function(it->m_opaque1, it->m_opaque2, it->m_opaque3);
        }
        if (it->m_ready)
            it->m_ready->ref();
    }
    m_decoderCallbacks.clear();
    m_decoderCallbackLock.unlock();
}

void MythPlayerVideoUI::ToggleAdjustFill(AdjustFillMode Mode)
{
    if (m_videoOutput)
    {
        m_detectLetterBox.SetDetectLetterbox(false, m_videoOutput->GetAdjustFill());
        m_videoOutput->ToggleAdjustFill(Mode);
        ReinitOSD();
        QString text = toString(m_videoOutput->GetAdjustFill());
        UpdateOSDMessage(text);
    }
}

void MythPlayerVideoUI::ReinitOSD()
{
    if (m_videoOutput)
    {
        m_osdLock.lock();
        QRect visible;
        QRect total;
        float aspect = NAN;
        float scaling = NAN;
        m_videoOutput->GetOSDBounds(total, visible, aspect, scaling, 1.0F);
        int stretch = static_cast<int>(lroundf(aspect * 100));
        if ((m_osd.Bounds() != visible) || (m_osd.GetFontStretch() != stretch))
        {
            uint old = m_captionsState.m_textDisplayMode;
            ToggleCaptionsByType(old);
            m_osd.Init(visible, aspect);
            m_captionsOverlay.Init(visible, aspect);
            EnableCaptions(old, false);
            if (m_deleteMap.IsEditing())
            {
                bool const changed = m_deleteMap.IsChanged();
                m_deleteMap.SetChanged(true);
                m_deleteMap.UpdateOSD(m_framesPlayed, m_videoFrameRate, &m_osd);
                m_deleteMap.SetChanged(changed);
            }
        }

        m_reinitOsd = false;

#ifdef USING_MHEG
        if (GetInteractiveTV())
        {
            QMutexLocker locker(&m_itvLock);
            m_interactiveTV->Reinit(total, visible, aspect);
            m_itvVisible = false;
        }
#endif
        m_osdLock.unlock();
    }
}

void MythPlayerVideoUI::CheckAspectRatio(MythVideoFrame* Frame)
{
    if (!Frame)
        return;

    if (!qFuzzyCompare(Frame->m_aspect, m_videoAspect) && Frame->m_aspect > 0.0F)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Video Aspect ratio changed from %1 to %2")
            .arg(static_cast<qreal>(m_videoAspect)).arg(static_cast<qreal>(Frame->m_aspect)));
        m_videoAspect = Frame->m_aspect;
        if (m_videoOutput)
        {
            m_videoOutput->VideoAspectRatioChanged(m_videoAspect);
            ReinitOSD();
        }
    }
}

