// MythTV
#include "livetvchain.h"
#include "tv_play.h"
#include "interactivetv.h"
#include "mythplayercaptionsui.h"

#define LOC QString("CaptionsUI: ")

MythPlayerCaptionsUI::MythPlayerCaptionsUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags)
  : MythPlayerAudioUI(MainWindow, Tv, Context, Flags)
{
    m_itvEnabled = gCoreContext->GetBoolSetting("EnableMHEG", false);

    // Connect outgoing
    connect(this, &MythPlayerCaptionsUI::CaptionsStateChanged, m_tv, &TV::CaptionsStateChanged);

    // Inbound connections
    connect(m_tv, &TV::RestartITV,      this, &MythPlayerCaptionsUI::ITVRestart);
    connect(m_tv, &TV::HandleITVAction, this, &MythPlayerCaptionsUI::ITVHandleAction);

    // Signalled connections (from MHIContext)
    connect(this, &MythPlayerCaptionsUI::SetInteractiveStream,    this, &MythPlayerCaptionsUI::SetStream);
    connect(this, &MythPlayerCaptionsUI::SetInteractiveStreamPos, this, &MythPlayerCaptionsUI::SetStreamPos);
    connect(this, &MythPlayerCaptionsUI::PlayInteractiveStream,   this, &MythPlayerCaptionsUI::StreamPlay);
}

MythPlayerCaptionsUI::~MythPlayerCaptionsUI()
{
    delete m_interactiveTV;
}

void MythPlayerCaptionsUI::ResetCaptions()
{
    QMutexLocker locker(&m_osdLock);

    if (((m_textDisplayMode & kDisplayAVSubtitle)      ||
         (m_textDisplayMode & kDisplayTextSubtitle)    ||
         (m_textDisplayMode & kDisplayRawTextSubtitle) ||
         (m_textDisplayMode & kDisplayDVDButton)       ||
         (m_textDisplayMode & kDisplayCC608)           ||
         (m_textDisplayMode & kDisplayCC708)))
    {
        m_osd.ClearSubtitles();
    }
    else if ((m_textDisplayMode & kDisplayTeletextCaptions) ||
             (m_textDisplayMode & kDisplayNUVTeletextCaptions))
    {
        m_osd.TeletextClear();
    }
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
    if (m_textDisplayMode)
        m_prevNonzeroTextDisplayMode = m_textDisplayMode;
    m_textDisplayMode &= ~Mode;
    ResetCaptions();

    QMutexLocker locker(&m_osdLock);

    bool newTextDesired = (m_textDisplayMode & kDisplayAllTextCaptions) != 0U;
    // Only turn off textDesired if the Operator requested it.
    if (UpdateOSD || newTextDesired)
        m_textDesired = newTextDesired;

    QString msg = "";
    if (kDisplayNUVTeletextCaptions & Mode)
        msg += tr("TXT CAP");

    if (kDisplayTeletextCaptions & Mode)
    {
        if (m_decoder != nullptr)
        {
            int track = GetTrack(kTrackTypeTeletextCaptions);
            if (track > -1)
                msg += m_decoder->GetTrackDesc(kTrackTypeTeletextCaptions, static_cast<uint>(track));
        }
        DisableTeletext();
    }
    int preserve = m_textDisplayMode & (kDisplayCC608 | kDisplayTextSubtitle |
                                        kDisplayAVSubtitle | kDisplayCC708 |
                                        kDisplayRawTextSubtitle);
    if ((kDisplayCC608 & Mode) || (kDisplayCC708 & Mode) ||
        (kDisplayAVSubtitle & Mode) || (kDisplayRawTextSubtitle & Mode))
    {
        uint type = toTrackType(Mode);
        if (m_decoder != nullptr)
        {
            int track = GetTrack(type);
            if (track > -1)
                msg += m_decoder->GetTrackDesc(type, static_cast<uint>(track));
        }
        m_osd.EnableSubtitles(preserve);
    }

    if (kDisplayTextSubtitle & Mode)
    {
        msg += tr("Text subtitles");
        m_osd.EnableSubtitles(preserve);
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
    QString msg = "";
    if ((kDisplayCC608 & Mode) || (kDisplayCC708 & Mode) ||
        (kDisplayAVSubtitle & Mode) || kDisplayRawTextSubtitle & Mode)
    {
        uint type = toTrackType(Mode);
        if (m_decoder != nullptr)
        {
            int track = GetTrack(type);
            if (track > -1)
                msg += m_decoder->GetTrackDesc(type, static_cast<uint>(track));
        }

        m_osd.EnableSubtitles(static_cast<int>(Mode));
    }
    if (kDisplayTextSubtitle & Mode)
    {
        m_osd.EnableSubtitles(kDisplayTextSubtitle);
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
        m_textDisplayMode = kDisplayTeletextCaptions;
    }

    msg += " " + tr("On");

    LOG(VB_PLAYBACK, LOG_INFO, QString("EnableCaptions(%1) msg: %2")
        .arg(Mode).arg(msg));

    m_textDisplayMode = Mode;
    if (m_textDisplayMode)
        m_prevNonzeroTextDisplayMode = m_textDisplayMode;
    if (UpdateOSD)
        UpdateOSDMessage(msg, kOSDTimeout_Med);
}

bool MythPlayerCaptionsUI::ToggleCaptions()
{
    SetCaptionsEnabled(!(static_cast<bool>(m_textDisplayMode)));
    return m_textDisplayMode != 0U;
}

bool MythPlayerCaptionsUI::HasTextSubtitles()
{
    return m_subReader.HasTextSubtitles();
}

bool MythPlayerCaptionsUI::ToggleCaptions(uint Type)
{
    QMutexLocker locker(&m_osdLock);
    uint mode = toCaptionType(Type);
    uint origMode = m_textDisplayMode;

    if (m_textDisplayMode)
        DisableCaptions(m_textDisplayMode, (origMode & mode) != 0U);
    if (origMode & mode)
        return m_textDisplayMode != 0U;
    if (mode)
        EnableCaptions(mode);
    return m_textDisplayMode != 0U;
}

void MythPlayerCaptionsUI::SetCaptionsEnabled(bool Enable, bool UpdateOSD)
{
    QMutexLocker locker(&m_osdLock);
    m_enableCaptions = m_disableCaptions = false;
    uint origMode = m_textDisplayMode;

    // Only turn off textDesired if the Operator requested it.
    if (UpdateOSD || Enable)
        m_textDesired = Enable;

    if (!Enable)
    {
        DisableCaptions(origMode, UpdateOSD);
        return;
    }
    uint mode = HasCaptionTrack(m_prevNonzeroTextDisplayMode) ?
        m_prevNonzeroTextDisplayMode : NextCaptionTrack(kDisplayNone);
    if (origMode != mode)
    {
        DisableCaptions(origMode, false);

        if (kDisplayNone == mode)
        {
            if (UpdateOSD)
            {
                UpdateOSDMessage(tr("No captions", "CC/Teletext/Subtitle text not available"),
                                 kOSDTimeout_Med);
            }
            LOG(VB_PLAYBACK, LOG_INFO, "No captions available yet to enable.");
        }
        else if (mode)
        {
            EnableCaptions(mode, UpdateOSD);
        }
    }
    ResetCaptions();
}

bool MythPlayerCaptionsUI::GetCaptionsEnabled() const
{
    return (kDisplayNUVTeletextCaptions == m_textDisplayMode) ||
           (kDisplayTeletextCaptions    == m_textDisplayMode) ||
           (kDisplayAVSubtitle          == m_textDisplayMode) ||
           (kDisplayCC608               == m_textDisplayMode) ||
           (kDisplayCC708               == m_textDisplayMode) ||
           (kDisplayTextSubtitle        == m_textDisplayMode) ||
           (kDisplayRawTextSubtitle     == m_textDisplayMode) ||
           (kDisplayTeletextMenu        == m_textDisplayMode);
}

QStringList MythPlayerCaptionsUI::GetTracks(uint Type)
{
    if (m_decoder)
        return m_decoder->GetTracks(Type);
    return QStringList();
}

uint MythPlayerCaptionsUI::GetTrackCount(uint Type)
{
    if (m_decoder)
        return m_decoder->GetTrackCount(Type);
    return 0;
}

int MythPlayerCaptionsUI::SetTrack(uint Type, uint TrackNo)
{
    int ret = -1;
    if (!m_decoder)
        return ret;

    ret = m_decoder->SetTrack(Type, static_cast<int>(TrackNo));
    if (kTrackTypeAudio == Type)
    {
        if (m_decoder)
        {
            UpdateOSDMessage(m_decoder->GetTrackDesc(Type,
                static_cast<uint>(GetTrack(Type))), kOSDTimeout_Med);
        }
        return ret;
    }

    uint subtype = toCaptionType(Type);
    if (subtype)
    {
        DisableCaptions(m_textDisplayMode, false);
        EnableCaptions(subtype, true);
        if ((kDisplayCC708 == subtype || kDisplayCC608 == subtype) && m_decoder)
        {
            int sid = m_decoder->GetTrackInfo(Type, TrackNo).m_stream_id;
            if (sid >= 0)
            {
                (kDisplayCC708 == subtype) ? m_cc708.SetCurrentService(sid) :
                                             m_cc608.SetMode(sid);
            }
        }
    }
    return ret;
}

void MythPlayerCaptionsUI::DoDisableForcedSubtitles()
{
    m_disableForcedSubtitles = false;
    m_osdLock.lock();
    m_osd.DisableForcedSubtitles();
    m_osdLock.unlock();
}

void MythPlayerCaptionsUI::DoEnableForcedSubtitles()
{
    m_enableForcedSubtitles = false;
    if (!m_allowForcedSubtitles)
        return;

    m_osdLock.lock();
    m_osd.EnableSubtitles(kDisplayAVSubtitle, true /*forced only*/);
    m_osdLock.unlock();
}

int MythPlayerCaptionsUI::GetTrack(uint Type)
{
    if (m_decoder)
        return m_decoder->GetTrack(Type);
    return -1;
}

int MythPlayerCaptionsUI::ChangeTrack(uint Type, int Direction)
{
    if (!m_decoder)
        return -1;

    int retval = m_decoder->ChangeTrack(Type, Direction);
    if (retval >= 0)
    {
        UpdateOSDMessage(m_decoder->GetTrackDesc(Type, static_cast<uint>(GetTrack(Type))),
                         kOSDTimeout_Med);
        return retval;
    }
    return -1;
}

void MythPlayerCaptionsUI::ChangeCaptionTrack(int Direction)
{
    if (!m_decoder || (Direction < 0))
        return;

    if (!((m_textDisplayMode == kDisplayTextSubtitle) ||
          (m_textDisplayMode == kDisplayNUVTeletextCaptions) ||
          (m_textDisplayMode == kDisplayNone)))
    {
        uint tracktype = toTrackType(m_textDisplayMode);
        if (GetTrack(tracktype) < m_decoder->NextTrack(tracktype))
        {
            SetTrack(tracktype, static_cast<uint>(m_decoder->NextTrack(tracktype)));
            return;
        }
    }
    uint nextmode = NextCaptionTrack(m_textDisplayMode);
    if ((nextmode == kDisplayTextSubtitle) ||
        (nextmode == kDisplayNUVTeletextCaptions) ||
        (nextmode == kDisplayNone))
    {
        DisableCaptions(m_textDisplayMode, true);
        if (nextmode != kDisplayNone)
            EnableCaptions(nextmode, true);
    }
    else
    {
        uint tracktype = toTrackType(nextmode);
        uint tracks = m_decoder->GetTrackCount(tracktype);
        if (tracks)
        {
            DisableCaptions(m_textDisplayMode, true);
            SetTrack(tracktype, 0);
        }
    }
}

bool MythPlayerCaptionsUI::HasCaptionTrack(uint Mode)
{
    if (Mode == kDisplayNone)
        return false;

    if (((Mode == kDisplayTextSubtitle) && HasTextSubtitles()) || (Mode == kDisplayNUVTeletextCaptions))
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
    m_osd.EnableTeletext(true, Page);
    m_prevTextDisplayMode = m_textDisplayMode;
    m_textDisplayMode = kDisplayTeletextMenu;
}

void MythPlayerCaptionsUI::DisableTeletext()
{
    QMutexLocker locker(&m_osdLock);
    m_osd.EnableTeletext(false, 0);
    m_textDisplayMode = kDisplayNone;

    // If subtitles were enabled before the teletext menu was displayed then re-enable them
    if (m_prevTextDisplayMode & kDisplayAllCaptions)
        EnableCaptions(m_prevTextDisplayMode, false);
}

void MythPlayerCaptionsUI::ResetTeletext()
{
    QMutexLocker locker(&m_osdLock);
    m_osd.TeletextReset();
}

/*! \brief Set Teletext NUV Caption page
 */
void MythPlayerCaptionsUI::SetTeletextPage(uint Page)
{
    m_osdLock.lock();
    DisableCaptions(m_textDisplayMode);
    m_ttPageNum = static_cast<int>(Page);
    m_cc608.SetTTPageNum(m_ttPageNum);
    m_textDisplayMode &= static_cast<uint>(~kDisplayAllCaptions);
    m_textDisplayMode |= kDisplayNUVTeletextCaptions;
    m_osdLock.unlock();
}

// TODO When captions state is complete, process action in TV
#include "tv_actions.h"
bool MythPlayerCaptionsUI::HandleTeletextAction(const QString& Action)
{
    if (!(m_textDisplayMode & kDisplayTeletextMenu))
        return false;

    bool handled = true;

    m_osdLock.lock();
    if (Action == "MENU" || Action == ACTION_TOGGLETT || Action == "ESCAPE")
        DisableTeletext();
    else
        handled = m_osd.TeletextAction(Action);
    m_osdLock.unlock();

    return handled;
}

InteractiveTV* MythPlayerCaptionsUI::GetInteractiveTV()
{
#ifdef USING_MHEG
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
void MythPlayerCaptionsUI::ITVHandleAction(const QString &Action, bool& Handled)
{
#ifdef USING_MHEG
    if (!GetInteractiveTV())
    {
        Handled = false;
        return;
    }

    QMutexLocker locker(&m_itvLock);
    Handled = m_interactiveTV->OfferKey(Action);
#else
    Q_UNUSED(Action);
#endif
}

/// \brief Restart the MHEG/MHP engine.
void MythPlayerCaptionsUI::ITVRestart(uint Chanid, uint Cardid, bool IsLiveTV)
{
#ifdef USING_MHEG
    if (!GetInteractiveTV())
        return;

    QMutexLocker locker(&m_itvLock);
    m_interactiveTV->Restart(static_cast<int>(Chanid), static_cast<int>(Cardid), IsLiveTV);
    m_itvVisible = false;
#else
    Q_UNUSED(Chanid);
    Q_UNUSED(Cardid);
    Q_UNUSED(IsLiveTV);
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

    if (Stream.isEmpty() && m_playerCtx->m_tvchain &&
        m_playerCtx->m_buffer->GetType() == kMythBufferMHEG)
    {
        // Restore livetv
        SetEof(kEofStateDelayed);
        m_playerCtx->m_tvchain->JumpToNext(false, 0);
        m_playerCtx->m_tvchain->JumpToNext(true, 0);
    }
}

// Called from the interactiveTV (MHIContext) thread
long MythPlayerCaptionsUI::GetStreamPos()
{
    return static_cast<long>((1000 * GetFramesPlayed()) / SafeFPS());
}

// Called from the interactiveTV (MHIContext) thread
long MythPlayerCaptionsUI::GetStreamMaxPos()
{
    long maxpos = static_cast<long>(1000 * (m_totalDuration > 0 ? m_totalDuration : m_totalLength));
    long pos = GetStreamPos();
    return maxpos > pos ? maxpos : pos;
}

long MythPlayerCaptionsUI::SetStreamPos(long Position)
{
    auto frameNum = static_cast<uint64_t>((Position * SafeFPS()) / 1000);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("SetStreamPos %1 mS = frame %2, now=%3")
        .arg(Position).arg(frameNum).arg(GetFramesPlayed()) );
    JumpToFrame(frameNum);
    return Position;
}

void MythPlayerCaptionsUI::StreamPlay(bool Playing)
{
    if (Playing)
        Play();
    else
        Pause();
}
