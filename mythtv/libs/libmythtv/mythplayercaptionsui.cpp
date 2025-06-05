// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"

#include "captions/subtitlescreen.h"
#include "livetvchain.h"
#include "mheg/interactivetv.h"
#include "mythplayercaptionsui.h"
#include "tv_play.h"

#define LOC QString("PlayerCaptions: ")

MythPlayerCaptionsUI::MythPlayerCaptionsUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags)
  : MythPlayerAudioUI(MainWindow, Tv, Context, Flags),
    m_captionsOverlay(MainWindow, Tv, nullptr, m_painter)
{
    // Register state type for signalling
    qRegisterMetaType<MythCaptionsState>();

    m_itvEnabled = gCoreContext->GetBoolSetting("EnableMHEG", false);

    // Connect outgoing
    connect(this, &MythPlayerCaptionsUI::CaptionsStateChanged, m_tv, &TV::CaptionsStateChanged);

    // Inbound connections
    connect(m_tv, &TV::SetTrack, this, &MythPlayerCaptionsUI::SetTrack);
    connect(m_tv, &TV::ChangeAllowForcedSubtitles, this, &MythPlayerCaptionsUI::SetAllowForcedSubtitles);
    connect(m_tv, &TV::ToggleCaptions,  this, &MythPlayerCaptionsUI::ToggleCaptions);
    connect(m_tv, &TV::ToggleCaptionsByType, this, &MythPlayerCaptionsUI::ToggleCaptionsByType);
    connect(m_tv, &TV::SetCaptionsEnabled, this, &MythPlayerCaptionsUI::SetCaptionsEnabled);
    connect(m_tv, &TV::DisableCaptions, this, &MythPlayerCaptionsUI::DisableCaptions);
    connect(m_tv, &TV::EnableCaptions, this, &MythPlayerCaptionsUI::EnableCaptions);
    connect(m_tv, &TV::ChangeCaptionTrack, this, &MythPlayerCaptionsUI::ChangeCaptionTrack);
    connect(m_tv, &TV::ChangeTrack,     this, &MythPlayerCaptionsUI::ChangeTrack);
    connect(m_tv, &TV::ResetCaptions,   this, &MythPlayerCaptionsUI::ResetCaptions);
    connect(m_tv, &TV::EnableTeletext,  this, &MythPlayerCaptionsUI::EnableTeletext);
    connect(m_tv, &TV::ResetTeletext,   this, &MythPlayerCaptionsUI::ResetTeletext);
    connect(m_tv, &TV::SetTeletextPage, this, &MythPlayerCaptionsUI::SetTeletextPage);
    connect(m_tv, &TV::RestartITV,      this, &MythPlayerCaptionsUI::ITVRestart);
    connect(m_tv, &TV::HandleTeletextAction, this, &MythPlayerCaptionsUI::HandleTeletextAction);
    connect(m_tv, &TV::HandleITVAction, this, &MythPlayerCaptionsUI::ITVHandleAction);
    connect(m_tv, &TV::AdjustSubtitleZoom, this, &MythPlayerCaptionsUI::AdjustSubtitleZoom);
    connect(m_tv, &TV::AdjustSubtitleDelay, this, &MythPlayerCaptionsUI::AdjustSubtitleDelay);
    connect(&m_subReader, &SubtitleReader::TextSubtitlesUpdated, this, &MythPlayerCaptionsUI::ExternalSubtitlesUpdated);

    // Signalled connections (from MHIContext)
    connect(this, &MythPlayerCaptionsUI::SetInteractiveStream,    this, &MythPlayerCaptionsUI::SetStream);
    connect(this, &MythPlayerCaptionsUI::SetInteractiveStreamPos, this, &MythPlayerCaptionsUI::SetStreamPos);
    connect(this, &MythPlayerCaptionsUI::PlayInteractiveStream,   this, &MythPlayerCaptionsUI::StreamPlay);

    // Signalled from the decoder
    connect(this, &MythPlayerCaptionsUI::EnableSubtitles, this, [this](bool Enable) { this->SetCaptionsEnabled(Enable, false); });

    // Signalled from the base class
    connect(this, &MythPlayerCaptionsUI::RequestResetCaptions, this, &MythPlayerCaptionsUI::ResetCaptions);
}

MythPlayerCaptionsUI::~MythPlayerCaptionsUI()
{
    delete m_interactiveTV;
}

void MythPlayerCaptionsUI::InitialiseState()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Initialising captions");
    LoadExternalSubtitles();
    emit CaptionsStateChanged(m_captionsState);
    MythPlayerAudioUI::InitialiseState();
}

void MythPlayerCaptionsUI::LoadExternalSubtitles()
{
    auto filename = m_playerCtx->m_buffer->GetSubtitleFilename();
    bool inprogress = (m_playerCtx->GetState() == kState_WatchingRecording ||
                       m_playerCtx->GetState() == kState_WatchingLiveTV);
    m_subReader.LoadExternalSubtitles(filename, inprogress);
    m_captionsState.m_externalTextSubs = m_subReader.HasTextSubtitles();
}

void MythPlayerCaptionsUI::ExternalSubtitlesUpdated()
{
    m_captionsState.m_externalTextSubs = m_subReader.HasTextSubtitles();
    emit CaptionsStateChanged(m_captionsState);
}

void MythPlayerCaptionsUI::AdjustSubtitleZoom(int Delta)
{
    if (!OptionalCaptionEnabled(m_captionsState.m_textDisplayMode) || (m_browsing || m_editing))
        return;

    if (auto * subs = m_captionsOverlay.InitSubtitles(); subs)
    {
        auto newval = std::clamp(subs->GetZoom() + Delta, 50, 200);
        UpdateOSDStatus(tr("Adjust Subtitle Zoom"), tr("Subtitle Zoom"),
                        QString::number(newval), kOSDFunctionalType_SubtitleZoomAdjust,
                        "%", newval * 1000 / 200, kOSDTimeout_None);
        ChangeOSDPositionUpdates(false);
        subs->SetZoom(newval);
    }
}

void MythPlayerCaptionsUI::AdjustSubtitleDelay(std::chrono::milliseconds Delta)
{
    bool showing = (m_captionsState.m_textDisplayMode == kDisplayRawTextSubtitle) ||
                   (m_captionsState.m_textDisplayMode == kDisplayTextSubtitle);
    if (!showing || (m_browsing || m_editing))
        return;

    if (auto * subs = m_captionsOverlay.InitSubtitles(); subs)
    {
        auto newval = std::clamp(subs->GetDelay() + (Delta * 10), -5000ms, 5000ms);
        // range of -5000ms..+5000ms, scale to 0..1000
        UpdateOSDStatus(tr("Adjust Subtitle Delay"), tr("Subtitle Delay"),
                        QString::number(newval.count()), kOSDFunctionalType_SubtitleDelayAdjust,
                        "ms", (newval.count() / 10) + 500, kOSDTimeout_None);
        ChangeOSDPositionUpdates(false);
        subs->SetDelay(newval);
    }

}

void MythPlayerCaptionsUI::ResetCaptions()
{
    m_captionsOverlay.ClearSubtitles();
    m_captionsOverlay.TeletextClear();
}

static uint toCaptionType(uint Type)
{
    if (kTrackTypeCC608 == Type)            return kDisplayCC608;
    if (kTrackTypeCC708 == Type)            return kDisplayCC708;
    if (kTrackTypeSubtitle == Type)         return kDisplayAVSubtitle;
    if (kTrackTypeTeletextCaptions == Type) return kDisplayTeletextCaptions;
    if (kTrackTypeTextSubtitle == Type)     return kDisplayTextSubtitle;
    if (kTrackTypeRawText == Type)          return kDisplayRawTextSubtitle;
    return 0;
}

static uint toTrackType(uint Type)
{
    if (kDisplayCC608 == Type)            return kTrackTypeCC608;
    if (kDisplayCC708 == Type)            return kTrackTypeCC708;
    if (kDisplayAVSubtitle == Type)       return kTrackTypeSubtitle;
    if (kDisplayTeletextCaptions == Type) return kTrackTypeTeletextCaptions;
    if (kDisplayTextSubtitle == Type)     return kTrackTypeTextSubtitle;
    if (kDisplayRawTextSubtitle == Type)  return kTrackTypeRawText;
    return kTrackTypeUnknown;
}

void MythPlayerCaptionsUI::DisableCaptions(uint Mode, bool UpdateOSD)
{
    auto oldcaptions = m_captionsState.m_textDisplayMode;
    if (m_captionsState.m_textDisplayMode != kDisplayNone)
        m_lastValidTextDisplayMode = m_captionsState.m_textDisplayMode;
    m_captionsState.m_textDisplayMode &= ~Mode;
    if (oldcaptions != m_captionsState.m_textDisplayMode)
        emit CaptionsStateChanged(m_captionsState);
    ResetCaptions();

    QMutexLocker locker(&m_osdLock);

    bool newTextDesired = (m_captionsState.m_textDisplayMode & kDisplayAllTextCaptions) != 0U;
    // Only turn off textDesired if the Operator requested it.
    if (UpdateOSD || newTextDesired)
        m_textDesired = newTextDesired;

    auto msg = (kDisplayNUVTeletextCaptions & Mode) ? tr("TXT CAP") : "";
    if (kDisplayTeletextCaptions & Mode)
    {
        if (auto track = GetTrack(kTrackTypeTeletextCaptions); (track > 1) && (m_decoder != nullptr))
            msg += m_decoder->GetTrackDesc(kTrackTypeTeletextCaptions, static_cast<uint>(track));
        DisableTeletext();
    }
    int preserve = m_captionsState.m_textDisplayMode & (kDisplayCC608 | kDisplayTextSubtitle |
                                                        kDisplayAVSubtitle | kDisplayCC708 |
                                                        kDisplayRawTextSubtitle);
    if ((kDisplayCC608 & Mode) || (kDisplayCC708 & Mode) ||
        (kDisplayAVSubtitle & Mode) || (kDisplayRawTextSubtitle & Mode))
    {
        if (uint type = toTrackType(Mode); m_decoder != nullptr)
            if (auto track = GetTrack(type); track > -1)
                msg += m_decoder->GetTrackDesc(type, static_cast<uint>(track));

        m_captionsOverlay.EnableSubtitles(preserve);
    }

    if (kDisplayTextSubtitle & Mode)
    {
        msg += tr("Text subtitles");
        m_captionsOverlay.EnableSubtitles(preserve);
    }

    if (!msg.isEmpty() && UpdateOSD)
    {
        msg += " " + tr("Off");
        UpdateOSDMessage(msg, kOSDTimeout_Med);
    }
}

void MythPlayerCaptionsUI::EnableCaptions(uint Mode, bool UpdateOSD)
{
    QMutexLocker locker(&m_osdLock);
    bool newTextDesired = (Mode & kDisplayAllTextCaptions) != 0U;
    // Only turn off textDesired if the Operator requested it.
    if (UpdateOSD || newTextDesired)
        m_textDesired = newTextDesired;
    QString msg;
    if ((kDisplayCC608 & Mode) || (kDisplayCC708 & Mode) ||
        (kDisplayAVSubtitle & Mode) || (kDisplayRawTextSubtitle & Mode))
    {
        if (auto type = toTrackType(Mode); m_decoder != nullptr)
            if (auto track = GetTrack(type); track > -1)
                msg += m_decoder->GetTrackDesc(type, static_cast<uint>(track));

        m_captionsOverlay.EnableSubtitles(static_cast<int>(Mode));
    }

    if (kDisplayTextSubtitle & Mode)
    {
        m_captionsOverlay.EnableSubtitles(kDisplayTextSubtitle);
        AVSubtitles* subs = m_subReader.GetAVSubtitles();
        subs->m_needSync = true;
        msg += tr("Text subtitles");
    }

    if (kDisplayNUVTeletextCaptions & Mode)
        msg += tr("TXT %1").arg(m_ttPageNum, 3, 16);

    if ((kDisplayTeletextCaptions & Mode) && (m_decoder != nullptr))
    {
        msg += m_decoder->GetTrackDesc(kTrackTypeTeletextCaptions,
                                       static_cast<uint>(GetTrack(kTrackTypeTeletextCaptions)));

        int page = m_decoder->GetTrackLanguageIndex(kTrackTypeTeletextCaptions,
                                       static_cast<uint>(GetTrack(kTrackTypeTeletextCaptions)));

        EnableTeletext(page);
    }

    msg += " " + tr("On");
    LOG(VB_PLAYBACK, LOG_INFO, QString("EnableCaptions(%1) msg: %2").arg(Mode).arg(msg));

    auto oldcaptions = m_captionsState.m_textDisplayMode;
    m_captionsState.m_textDisplayMode = Mode;
    if (m_captionsState.m_textDisplayMode != kDisplayNone)
        m_lastValidTextDisplayMode = m_captionsState.m_textDisplayMode;
    if (oldcaptions != m_captionsState.m_textDisplayMode)
        emit CaptionsStateChanged(m_captionsState);
    if (UpdateOSD)
        UpdateOSDMessage(msg, kOSDTimeout_Med);
}

/*! \brief This tries to re-enable captions/subtitles if the user
 * wants them and one of the captions/subtitles tracks has changed.
 */
void MythPlayerCaptionsUI::tracksChanged(uint TrackType)
{
    if (m_textDesired && (TrackType >= kTrackTypeSubtitle) && (TrackType <= kTrackTypeTeletextCaptions))
        SetCaptionsEnabled(true, false);
}

void MythPlayerCaptionsUI::SetAllowForcedSubtitles(bool Allow)
{
    m_allowForcedSubtitles = Allow;
    UpdateOSDMessage(m_allowForcedSubtitles ? tr("Forced Subtitles On") : tr("Forced Subtitles Off"));
}

void MythPlayerCaptionsUI::ToggleCaptions()
{
    SetCaptionsEnabled(!(static_cast<bool>(m_captionsState.m_textDisplayMode)));
}

void MythPlayerCaptionsUI::ToggleCaptionsByType(uint Type)
{
    QMutexLocker locker(&m_osdLock);
    auto mode = toCaptionType(Type);
    auto origmode = m_captionsState.m_textDisplayMode;
    if (m_captionsState.m_textDisplayMode)
        DisableCaptions(m_captionsState.m_textDisplayMode, (origmode & mode) != 0U);
    if (origmode & mode)
        return;
    if (mode)
        EnableCaptions(mode);
}

void MythPlayerCaptionsUI::SetCaptionsEnabled(bool Enable, bool UpdateOSD)
{
    QMutexLocker locker(&m_osdLock);
    auto origmode = m_captionsState.m_textDisplayMode;

    // Only turn off textDesired if the Operator requested it.
    if (UpdateOSD || Enable)
        m_textDesired = Enable;

    if (!Enable)
    {
        DisableCaptions(origmode, UpdateOSD);
        return;
    }

    if (auto mode = HasCaptionTrack(m_lastValidTextDisplayMode) ?
            m_lastValidTextDisplayMode : NextCaptionTrack(kDisplayNone); origmode != mode)
    {
        DisableCaptions(origmode, false);
        if (kDisplayNone == mode)
        {
            if (UpdateOSD)
                UpdateOSDMessage(tr("No captions", "CC/Teletext/Subtitle text not available"), kOSDTimeout_Med);
            LOG(VB_PLAYBACK, LOG_INFO, "No captions available yet to enable.");
        }
        else
        {
            EnableCaptions(mode, UpdateOSD);
        }
    }
}

QStringList MythPlayerCaptionsUI::GetTracks(uint Type)
{
    if (m_decoder)
        return m_decoder->GetTracks(Type);
    return {};
}

uint MythPlayerCaptionsUI::GetTrackCount(uint Type)
{
    if (m_decoder)
        return m_decoder->GetTrackCount(Type);
    return 0;
}

void MythPlayerCaptionsUI::SetTrack(uint Type, uint TrackNo)
{
    if (!m_decoder)
        return;

    m_decoder->SetTrack(Type, static_cast<int>(TrackNo));
    if (kTrackTypeAudio == Type)
    {
        UpdateOSDMessage(m_decoder->GetTrackDesc(Type, static_cast<uint>(GetTrack(Type))), kOSDTimeout_Med);
    }
    else if (auto subtype = toCaptionType(Type); subtype)
    {
        DisableCaptions(m_captionsState.m_textDisplayMode, false);
        EnableCaptions(subtype, true);
        if ((kDisplayCC708 == subtype || kDisplayCC608 == subtype) && m_decoder)
            if (auto sid = m_decoder->GetTrackInfo(Type, TrackNo).m_stream_id; sid > 0)
                (kDisplayCC708 == subtype) ? m_cc708.SetCurrentService(sid) : m_cc608.SetMode(sid);
    }
}

void MythPlayerCaptionsUI::DoDisableForcedSubtitles()
{
    m_disableForcedSubtitles = false;
    m_osdLock.lock();
    m_captionsOverlay.DisableForcedSubtitles();
    m_osdLock.unlock();
}

void MythPlayerCaptionsUI::DoEnableForcedSubtitles()
{
    m_enableForcedSubtitles = false;
    if (!m_allowForcedSubtitles)
        return;

    m_osdLock.lock();
    m_captionsOverlay.EnableSubtitles(kDisplayAVSubtitle, true /*forced only*/);
    m_osdLock.unlock();
}

int MythPlayerCaptionsUI::GetTrack(uint Type)
{
    if (m_decoder)
        return m_decoder->GetTrack(Type);
    return -1;
}

void MythPlayerCaptionsUI::ChangeTrack(uint Type, int Direction)
{
    if (!m_decoder)
        return;
    if (auto ret = m_decoder->ChangeTrack(Type, Direction); ret >= 0)
        UpdateOSDMessage(m_decoder->GetTrackDesc(Type, static_cast<uint>(GetTrack(Type))), kOSDTimeout_Med);
}

void MythPlayerCaptionsUI::ChangeCaptionTrack(int Direction)
{
    if (!m_decoder || (Direction < 0))
        return;

    if ((m_captionsState.m_textDisplayMode != kDisplayTextSubtitle) &&
          (m_captionsState.m_textDisplayMode != kDisplayNUVTeletextCaptions) &&
          (m_captionsState.m_textDisplayMode != kDisplayNone))
    {
        uint tracktype = toTrackType(m_captionsState.m_textDisplayMode);
        if (GetTrack(tracktype) < m_decoder->NextTrack(tracktype))
        {
            SetTrack(tracktype, static_cast<uint>(m_decoder->NextTrack(tracktype)));
            return;
        }
    }
    uint nextmode = NextCaptionTrack(m_captionsState.m_textDisplayMode);
    if ((nextmode == kDisplayTextSubtitle) || (nextmode == kDisplayNUVTeletextCaptions) ||
        (nextmode == kDisplayNone))
    {
        DisableCaptions(m_captionsState.m_textDisplayMode, true);
        if (nextmode != kDisplayNone)
            EnableCaptions(nextmode, true);
    }
    else
    {
        uint tracktype = toTrackType(nextmode);
        uint tracks = m_decoder->GetTrackCount(tracktype);
        if (tracks)
        {
            DisableCaptions(m_captionsState.m_textDisplayMode, true);
            SetTrack(tracktype, 0);
        }
    }
}

bool MythPlayerCaptionsUI::HasCaptionTrack(uint Mode)
{
    if (Mode == kDisplayNone)
        return false;
    if (Mode == kDisplayNUVTeletextCaptions)
        return true;
    // External subtitles are now decoded with FFmpeg and are AVSubtitles.
    if ((Mode == kDisplayAVSubtitle || Mode == kDisplayTextSubtitle) && m_captionsState.m_externalTextSubs)
        return true;
    if (!(Mode == kDisplayTextSubtitle) && m_decoder->GetTrackCount(toTrackType(Mode)))
        return true;
    return false;
}

uint MythPlayerCaptionsUI::NextCaptionTrack(uint Mode)
{
    // Text->TextStream->708->608->AVSubs->Teletext->NUV->None
    // NUV only offerred if PAL
    bool pal      = (m_vbiMode == VBIMode::PAL_TT);
    uint nextmode = kDisplayNone;

    if (kDisplayTextSubtitle == Mode)
        nextmode = kDisplayRawTextSubtitle;
    else if (kDisplayRawTextSubtitle == Mode)
        nextmode = kDisplayCC708;
    else if (kDisplayCC708 == Mode)
        nextmode = kDisplayCC608;
    else if (kDisplayCC608 == Mode)
        nextmode = kDisplayAVSubtitle;
    else if (kDisplayAVSubtitle == Mode)
        nextmode = kDisplayTeletextCaptions;
    else if (kDisplayTeletextCaptions == Mode)
        nextmode = pal ? kDisplayNUVTeletextCaptions : kDisplayNone;
    else if ((kDisplayNUVTeletextCaptions == Mode) && pal)
        nextmode = kDisplayNone;
    else if (kDisplayNone == Mode)
        nextmode = kDisplayTextSubtitle;

    if (nextmode == kDisplayNone || HasCaptionTrack(nextmode))
        return nextmode;

    return NextCaptionTrack(nextmode);
}

void MythPlayerCaptionsUI::EnableTeletext(int Page)
{
    QMutexLocker locker(&m_osdLock);
    auto oldcaptions = m_captionsState.m_textDisplayMode;
    m_captionsOverlay.EnableTeletext(true, Page);
    m_lastTextDisplayMode = m_captionsState.m_textDisplayMode;
    m_captionsState.m_textDisplayMode = kDisplayTeletextMenu;
    if (oldcaptions != m_captionsState.m_textDisplayMode)
        emit CaptionsStateChanged(m_captionsState);
}

void MythPlayerCaptionsUI::DisableTeletext()
{
    QMutexLocker locker(&m_osdLock);
    m_captionsOverlay.EnableTeletext(false, 0);
    auto oldcaptions = m_captionsState.m_textDisplayMode;
    m_captionsState.m_textDisplayMode = kDisplayNone;
    if (oldcaptions != m_captionsState.m_textDisplayMode)
        emit CaptionsStateChanged(m_captionsState);

    // If subtitles were enabled before the teletext menu was displayed then re-enable them
    if (m_lastTextDisplayMode & kDisplayAllCaptions)
        EnableCaptions(m_lastTextDisplayMode, false);
}

void MythPlayerCaptionsUI::ResetTeletext()
{
    QMutexLocker locker(&m_osdLock);
    m_captionsOverlay.TeletextReset();
}

/*! \brief Set Teletext NUV Caption page
 */
void MythPlayerCaptionsUI::SetTeletextPage(uint Page)
{
    m_osdLock.lock();
    DisableCaptions(m_captionsState.m_textDisplayMode);
    auto oldcaptions = m_captionsState.m_textDisplayMode;
    m_ttPageNum = static_cast<int>(Page);
    m_cc608.SetTTPageNum(m_ttPageNum);
    m_captionsState.m_textDisplayMode &= static_cast<uint>(~kDisplayAllCaptions);
    m_captionsState.m_textDisplayMode |= kDisplayNUVTeletextCaptions;
    if (oldcaptions != m_captionsState.m_textDisplayMode)
        emit CaptionsStateChanged(m_captionsState);
    m_osdLock.unlock();
}

void MythPlayerCaptionsUI::HandleTeletextAction(const QString& Action, bool &Handled)
{
    if (!(m_captionsState.m_textDisplayMode & kDisplayTeletextMenu))
        return;

    bool exit = false;
    m_osdLock.lock();
    Handled = m_captionsOverlay.TeletextAction(Action, exit);
    m_osdLock.unlock();
    if (exit)
        DisableTeletext();
}

InteractiveTV* MythPlayerCaptionsUI::GetInteractiveTV()
{
#if CONFIG_MHEG
    bool update = false;
    {
        QMutexLocker lock1(&m_osdLock);
        QMutexLocker lock2(&m_itvLock);
        if (!m_interactiveTV && m_itvEnabled && !FlagIsSet(kNoITV))
        {
            m_interactiveTV = new InteractiveTV(this);
            m_captionsState.m_haveITV = true;
            update = true;
        }
    }
    if (update)
        emit CaptionsStateChanged(m_captionsState);
#endif
    return m_interactiveTV;
}

/*! \brief Submit Action to the interactiveTV object
 *
 * This is a little contrived as this method is signalled from the parent object. Rather
 * than return a value (which is not possible with a signal) we update the Handled
 * parameter. This is fine as long as there is only one signal/slot connection but
 * I'm guessing won't work as well if signalled across threads.
*/
void MythPlayerCaptionsUI::ITVHandleAction([[maybe_unused]] const QString &Action,
                                           [[maybe_unused]] bool& Handled)
{
#if CONFIG_MHEG
    if (!GetInteractiveTV())
    {
        Handled = false;
        return;
    }

    QMutexLocker locker(&m_itvLock);
    Handled = m_interactiveTV->OfferKey(Action);
#endif
}

/// \brief Restart the MHEG/MHP engine.
void MythPlayerCaptionsUI::ITVRestart([[maybe_unused]] uint Chanid,
                                      [[maybe_unused]] uint Cardid,
                                      [[maybe_unused]] bool IsLiveTV)
{
#if CONFIG_MHEG
    if (!GetInteractiveTV())
        return;

    QMutexLocker locker(&m_itvLock);
    m_interactiveTV->Restart(static_cast<int>(Chanid), static_cast<int>(Cardid), IsLiveTV);
    m_itvVisible = false;
#endif
}

/*! \brief Selects the audio stream using the DVB component tag.
 *
 * This is called from the InteractiveTV thread and really needs to be processed
 * in the decoder thread. So for the time being do not convert this to a signal/slot
 * (as there are no slots in the decoder classes yet) and just pass through with
 * the protection of the decoder change lock.
*/
bool MythPlayerCaptionsUI::SetAudioByComponentTag(int Tag)
{
    QMutexLocker locker(&m_decoderChangeLock);
    if (m_decoder)
        return m_decoder->SetAudioByComponentTag(Tag);
    return false;
}

/*! \brief Selects the video stream using the DVB component tag.
 *
 * See SetAudioByComponentTag comments
*/
bool MythPlayerCaptionsUI::SetVideoByComponentTag(int Tag)
{
    QMutexLocker locker(&m_decoderChangeLock);
    if (m_decoder)
        return m_decoder->SetVideoByComponentTag(Tag);
    return false;
}

double MythPlayerCaptionsUI::SafeFPS()
{
    QMutexLocker locker(&m_decoderChangeLock);
    if (!m_decoder)
        return 25;
    double fps = m_decoder->GetFPS();
    return fps > 0 ? fps : 25.0;
}

void MythPlayerCaptionsUI::SetStream(const QString& Stream)
{
    // The stream name is empty if the stream is closing
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("SetStream '%1'").arg(Stream));

    // Stream will be changed by JumpToStream called from EventLoop
    // If successful will call m_interactiveTV->StreamStarted();
    m_newStream = Stream;

    if (Stream.isEmpty() && m_playerCtx->m_tvchain && m_playerCtx->m_buffer->GetType() == kMythBufferMHEG)
    {
        // Restore livetv
        SetEof(kEofStateDelayed);
        m_playerCtx->m_tvchain->JumpToNext(false, 0s);
        m_playerCtx->m_tvchain->JumpToNext(true, 0s);
    }
}

// Called from the interactiveTV (MHIContext) thread
std::chrono::milliseconds MythPlayerCaptionsUI::GetStreamPos()
{
    return millisecondsFromFloat((1000 * GetFramesPlayed()) / SafeFPS());
}

// Called from the interactiveTV (MHIContext) thread
std::chrono::milliseconds MythPlayerCaptionsUI::GetStreamMaxPos()
{
    std::chrono::seconds maxsecs = m_totalDuration > 0s ? m_totalDuration : m_totalLength;
    auto maxpos = duration_cast<std::chrono::milliseconds>(maxsecs);
    auto pos = GetStreamPos();
    return maxpos > pos ? maxpos : pos;
}

void MythPlayerCaptionsUI::SetStreamPos(std::chrono::milliseconds Position)
{
    auto frameNum = static_cast<uint64_t>((Position.count() * SafeFPS()) / 1000);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("SetStreamPos %1 mS = frame %2, now=%3")
        .arg(Position.count()).arg(frameNum).arg(GetFramesPlayed()) );
    JumpToFrame(frameNum);
}

void MythPlayerCaptionsUI::StreamPlay(bool Playing)
{
    if (Playing)
        Play();
    else
        Pause();
}
