// -*- Mode: c++ -*-

#undef HAVE_AV_CONFIG_H

// C++ headers
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

// Qt headers
#include <QCoreApplication>
#include <QDir>
#include <QHash>
#include <QMap>
#include <QThread>
#include <QtCore/qnumeric.h>
#include <utility>

// MythTV headers
#include "mthread.h"
#include "mythconfig.h"
#include "mythplayer.h"
#include "DetectLetterbox.h"
#include "audioplayer.h"
#include "interactivescreen.h"
#include "programinfo.h"
#include "mythcorecontext.h"
#include "livetvchain.h"
#include "avformatdecoder.h"
#include "dummydecoder.h"
#include "tv_play.h"
#include "interactivetv.h"
#include "mythsystemevent.h"
#include "mythlogging.h"
#include "mythmiscutil.h"
#include "io/mythinteractivebuffer.h"
#include "audiooutput.h"
#include "cardutil.h"
#include "mythavutil.h"
#include "jitterometer.h"
#include "mythtimer.h"
#include "mythuiactions.h"
#include "io/mythmediabuffer.h"
#include "tv_actions.h"
#include "decoders/mythdecoderthread.h"
#include "mythvideooutnull.h"
#include "mythcodeccontext.h"

// MythUI headers
#include <mythmainwindow.h>

extern "C" {
#include "libavcodec/avcodec.h"
}

#include "remoteencoder.h"

#if ! HAVE_ROUND
#define round(x) ((int) ((x) + 0.5))
#endif

static unsigned dbg_ident(const MythPlayer* /*player*/);

#define LOC QString("Player(%1): ").arg(dbg_ident(this),0,36)

const int MythPlayer::kNightModeBrightenssAdjustment = 10;
const int MythPlayer::kNightModeContrastAdjustment = 10;

// Exact frame seeking, no inaccuracy allowed.
const double MythPlayer::kInaccuracyNone = 0;

// By default, when seeking, snap to a keyframe if the keyframe's
// distance from the target frame is less than 10% of the total seek
// distance.
const double MythPlayer::kInaccuracyDefault = 0.1;

// Allow greater inaccuracy (50%) in the cutlist editor (unless the
// editor seek distance is set to 1 frame or 1 keyframe).
const double MythPlayer::kInaccuracyEditor = 0.5;

// Any negative value means completely inexact, i.e. seek to the
// keyframe that is closest to the target.
const double MythPlayer::kInaccuracyFull = -1.0;

static int toCaptionType(int type)
{
    if (kTrackTypeCC608 == type)            return kDisplayCC608;
    if (kTrackTypeCC708 == type)            return kDisplayCC708;
    if (kTrackTypeSubtitle == type)         return kDisplayAVSubtitle;
    if (kTrackTypeTeletextCaptions == type) return kDisplayTeletextCaptions;
    if (kTrackTypeTextSubtitle == type)     return kDisplayTextSubtitle;
    if (kTrackTypeRawText == type)          return kDisplayRawTextSubtitle;
    return 0;
}

static int toTrackType(int type)
{
    if (kDisplayCC608 == type)            return kTrackTypeCC608;
    if (kDisplayCC708 == type)            return kTrackTypeCC708;
    if (kDisplayAVSubtitle == type)       return kTrackTypeSubtitle;
    if (kDisplayTeletextCaptions == type) return kTrackTypeTeletextCaptions;
    if (kDisplayTextSubtitle == type)     return kTrackTypeTextSubtitle;
    if (kDisplayRawTextSubtitle == type)  return kTrackTypeRawText;
    return kTrackTypeUnknown;
}

MythPlayer::MythPlayer(PlayerContext* Context, PlayerFlags Flags)
  : m_playerCtx(Context),
    m_playerFlags(Flags),
    // CC608/708
    m_cc608(this), m_cc708(this),
    // Audio
    m_audio(this, (Flags & kAudioMuted) != 0)
{
    m_playerThread = QThread::currentThread();
#ifdef Q_OS_ANDROID
    m_playerThreadId = gettid();
#endif
    m_deleteMap.SetPlayerContext(m_playerCtx);
    m_liveTV = m_playerCtx->m_tvchain;

    m_vbiMode = VBIMode::Parse(gCoreContext->GetSetting("VbiFormat"));
    m_captionsEnabledbyDefault = gCoreContext->GetBoolSetting("DefaultCCMode");
    m_itvEnabled         = gCoreContext->GetBoolSetting("EnableMHEG", false);
    m_clearSavedPosition = gCoreContext->GetNumSetting("ClearSavedPosition", 1);
    m_endExitPrompt      = gCoreContext->GetNumSetting("EndOfRecordingExitPrompt");

    // Get VBI page number
    QString mypage = gCoreContext->GetSetting("VBIpageNr", "888");
    bool valid = false;
    uint tmp = mypage.toInt(&valid, 16);
    m_ttPageNum = (valid) ? tmp : m_ttPageNum;
    m_cc608.SetTTPageNum(m_ttPageNum);
}

MythPlayer::~MythPlayer(void)
{
    // NB the interactiveTV thread is a client of OSD so must be deleted
    // before locking and deleting the OSD
    {
        QMutexLocker lk0(&m_itvLock);
        delete m_interactiveTV;
        m_interactiveTV = nullptr;
    }

    QMutexLocker lock1(&m_osdLock);
    QMutexLocker lock2(&m_vidExitLock);

    delete m_osd;
    m_osd = nullptr;

    SetDecoder(nullptr);

    delete m_decoderThread;
    m_decoderThread = nullptr;

    delete m_videoOutput;
    m_videoOutput = nullptr;
}

void MythPlayer::SetWatchingRecording(bool mode)
{
    m_watchingRecording = mode;
    if (m_decoder)
        m_decoder->SetWatchingRecording(mode);
}

bool MythPlayer::IsWatchingInprogress(void) const
{
    return m_watchingRecording && m_playerCtx->m_recorder &&
        m_playerCtx->m_recorder->IsValidRecorder();
}

void MythPlayer::PauseBuffer(void)
{
    m_bufferPauseLock.lock();
    if (m_playerCtx->m_buffer)
    {
        m_playerCtx->m_buffer->Pause();
        m_playerCtx->m_buffer->WaitForPause();
    }
    m_bufferPaused = true;
    m_bufferPauseLock.unlock();
}

void MythPlayer::UnpauseBuffer(void)
{
    m_bufferPauseLock.lock();
    if (m_playerCtx->m_buffer)
        m_playerCtx->m_buffer->Unpause();
    m_bufferPaused = false;
    m_bufferPauseLock.unlock();
}

bool MythPlayer::Pause(void)
{
    while (!m_pauseLock.tryLock(100))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Waited 100ms to get pause lock.");
        DecoderPauseCheck();
    }
    bool already_paused = m_allPaused;
    if (already_paused)
    {
        m_pauseLock.unlock();
        return already_paused;
    }
    m_nextPlaySpeed   = 0.0;
    m_nextNormalSpeed = false;
    PauseVideo();
    m_audio.Pause(true);
    PauseDecoder();
    PauseBuffer();
    if (!m_decoderPaused)
        PauseDecoder(); // Retry in case audio only stream
    m_allPaused = m_decoderPaused && m_videoPaused && m_bufferPaused;
    {
        if (FlagIsSet(kVideoIsNull) && m_decoder)
            m_decoder->UpdateFramesPlayed();
        else if (m_videoOutput && !FlagIsSet(kVideoIsNull))
            m_framesPlayed = m_videoOutput->GetFramesPlayed();
    }
    m_pauseLock.unlock();
    return already_paused;
}

bool MythPlayer::Play(float speed, bool normal, bool unpauseaudio)
{
    m_pauseLock.lock();
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Play(%1, normal %2, unpause audio %3)")
            .arg(speed,5,'f',1).arg(normal).arg(unpauseaudio));

    if (m_deleteMap.IsEditing())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Ignoring Play(), in edit mode.");
        m_pauseLock.unlock();
        return false;
    }

    m_avSync.InitAVSync();

    SetEof(kEofStateNone);
    UnpauseBuffer();
    UnpauseDecoder();
    if (unpauseaudio)
        m_audio.Pause(false);
    UnpauseVideo();
    m_allPaused = false;
    m_nextPlaySpeed   = speed;
    m_nextNormalSpeed = normal;
    m_pauseLock.unlock();
    return true;
}

void MythPlayer::PauseVideo(void)
{
    m_videoPauseLock.lock();
    m_needNewPauseFrame = true;
    m_videoPaused = true;
    m_videoPauseLock.unlock();
}

void MythPlayer::UnpauseVideo(void)
{
    m_videoPauseLock.lock();
    m_videoPaused = false;
    m_videoPauseLock.unlock();
}

void MythPlayer::SetPlayingInfo(const ProgramInfo &pginfo)
{
    assert(m_playerCtx);
    if (!m_playerCtx)
        return;

    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    m_playerCtx->SetPlayingInfo(&pginfo);
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
}

void MythPlayer::SetPlaying(bool is_playing)
{
    QMutexLocker locker(&m_playingLock);

    m_playing = is_playing;

    m_playingWaitCond.wakeAll();
}

bool MythPlayer::IsPlaying(uint wait_in_msec, bool wait_for) const
{
    QMutexLocker locker(&m_playingLock);

    if (!wait_in_msec)
        return m_playing;

    MythTimer t;
    t.start();

    while ((wait_for != m_playing) && ((uint)t.elapsed() < wait_in_msec))
    {
        m_playingWaitCond.wait(
            &m_playingLock, std::max(0,(int)wait_in_msec - t.elapsed()));
    }

    return m_playing;
}

bool MythPlayer::InitVideo(void)
{
    if (!m_playerCtx)
        return false;

    if (!m_decoder)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot create a video renderer without a decoder.");
        return false;
    }

    m_videoOutput = MythVideoOutputNull::Create(m_videoDim, m_videoDispDim, m_videoAspect,
                                                m_decoder->GetVideoCodecID());

    if (!m_videoOutput)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Couldn't create VideoOutput instance. Exiting..");
        SetErrored(tr("Failed to initialize video output"));
        return false;
    }

    return true;
}

void MythPlayer::ReinitOSD(void)
{
    if (m_videoOutput && !FlagIsSet(kVideoIsNull))
    {
        m_osdLock.lock();
        if (!is_current_thread(m_playerThread))
        {
            m_reinitOsd = true;
            m_osdLock.unlock();
            return;
        }
        QRect visible;
        QRect total;
        float aspect = NAN;
        float scaling = NAN;
        m_videoOutput->GetOSDBounds(total, visible, aspect, scaling, 1.0F);
        if (m_osd)
        {
            m_osd->SetPainter(m_videoOutput->GetOSDPainter());
            int stretch = lroundf(aspect * 100);
            if ((m_osd->Bounds() != visible) ||
                (m_osd->GetFontStretch() != stretch))
            {
                uint old = m_textDisplayMode;
                ToggleCaptions(old);
                m_osd->Reinit(visible, aspect);
                EnableCaptions(old, false);
                if (m_deleteMap.IsEditing())
                {
                    bool const changed = m_deleteMap.IsChanged();
                    m_deleteMap.SetChanged(true);
                    m_deleteMap.UpdateOSD(m_framesPlayed, m_videoFrameRate, m_osd);
                    m_deleteMap.SetChanged(changed);
                }
            }
        }

#ifdef USING_MHEG
        if (GetInteractiveTV())
        {
            QMutexLocker locker(&m_itvLock);
            m_interactiveTV->Reinit(total, visible, aspect);
            m_itvVisible = false;
        }
#endif // USING_MHEG
        m_reinitOsd = false;
        m_osdLock.unlock();
    }
}

void MythPlayer::ReinitVideo(bool ForceUpdate)
{

    bool aspect_only = false;
    {
        QMutexLocker lock1(&m_osdLock);
        QMutexLocker lock2(&m_vidExitLock);

        m_videoOutput->SetVideoFrameRate(static_cast<float>(m_videoFrameRate));
        float aspect = (m_forcedVideoAspect > 0) ? m_forcedVideoAspect : m_videoAspect;
        if (!m_videoOutput->InputChanged(m_videoDim, m_videoDispDim, aspect,
                                         m_decoder->GetVideoCodecID(), aspect_only,
                                         m_maxReferenceFrames, ForceUpdate))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Failed to Reinitialize Video. Exiting..");
            SetErrored(tr("Failed to reinitialize video output"));
            return;
        }

        if (m_osd)
            m_osd->SetPainter(m_videoOutput->GetOSDPainter());
        ReinitOSD();
    }

    if (!aspect_only)
        ClearAfterSeek();

    if (m_textDisplayMode)
        EnableSubtitles(true);
}

void MythPlayer::SetKeyframeDistance(int keyframedistance)
{
    m_keyframeDist = (keyframedistance > 0) ? static_cast<uint>(keyframedistance) : m_keyframeDist;
}

void MythPlayer::SetVideoParams(int width, int height, double fps,
                                float aspect, bool ForceUpdate,
                                int ReferenceFrames, FrameScanType /*scan*/, const QString& codecName)
{
    bool paramsChanged = ForceUpdate;

    if (width >= 0 && height >= 0)
    {
        paramsChanged  = true;
        m_videoDim      = m_videoDispDim = QSize(width, height);
        m_videoAspect   = aspect > 0.0F ? aspect : static_cast<float>(width) / height;
    }

    if (!qIsNaN(fps) && fps > 0.0 && fps < 121.0)
    {
        paramsChanged    = true;
        m_videoFrameRate = fps;
        if (m_ffrewSkip != 0 && m_ffrewSkip != 1)
        {
            UpdateFFRewSkip();
        }
        else
        {
            float temp_speed = (m_playSpeed == 0.0F) ?
                m_audio.GetStretchFactor() : m_playSpeed;
            SetFrameInterval(kScan_Progressive,
                             1.0 / (m_videoFrameRate * static_cast<double>(temp_speed)));
        }
    }

    if (!codecName.isEmpty())
    {
        m_codecName = codecName;
        paramsChanged = true;
    }

    if (ReferenceFrames > 0)
    {
        m_maxReferenceFrames = ReferenceFrames;
        paramsChanged = true;
    }

    if (!paramsChanged)
        return;

    if (m_videoOutput)
        ReinitVideo(ForceUpdate);

    if (IsErrored())
        return;
}


void MythPlayer::SetFrameRate(double fps)
{
    m_videoFrameRate = fps;
    float temp_speed = (m_playSpeed == 0.0F) ? m_audio.GetStretchFactor() : m_playSpeed;
    SetFrameInterval(kScan_Progressive, 1.0 / (m_videoFrameRate * static_cast<double>(temp_speed)));
}

void MythPlayer::SetFileLength(int total, int frames)
{
    m_totalLength = total;
    m_totalFrames = frames;
}

void MythPlayer::SetDuration(int duration)
{
    m_totalDuration = duration;
}

void MythPlayer::OpenDummy(void)
{
    m_isDummy = true;

    if (!m_videoOutput)
    {
        SetKeyframeDistance(15);
        SetVideoParams(720, 576, 25.00, 1.25F, false, 2);
    }

    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    auto *dec = new DummyDecoder(this, *(m_playerCtx->m_playingInfo));
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
    SetDecoder(dec);
}

void MythPlayer::CreateDecoder(TestBufferVec & TestBuffer)
{
    if (AvFormatDecoder::CanHandle(TestBuffer, m_playerCtx->m_buffer->GetFilename()))
        SetDecoder(new AvFormatDecoder(this, *m_playerCtx->m_playingInfo, m_playerFlags));
}

int MythPlayer::OpenFile(int Retries)
{
    // Sanity check
    if (!m_playerCtx || (m_playerCtx && !m_playerCtx->m_buffer))
        return -1;

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Opening '%1'").arg(m_playerCtx->m_buffer->GetSafeFilename()));

    m_isDummy = false;
    m_liveTV = m_playerCtx->m_tvchain && m_playerCtx->m_buffer->LiveMode();

    // Dummy setup for livetv transtions. Can we get rid of this?
    if (m_playerCtx->m_tvchain)
    {
        int currentposition = m_playerCtx->m_tvchain->GetCurPos();
        if (m_playerCtx->m_tvchain->GetInputType(currentposition) == "DUMMY")
        {
            OpenDummy();
            return 0;
        }
    }

    // Start the RingBuffer read ahead thread
    m_playerCtx->m_buffer->Start();

    /// OSX has a small stack, so we put this buffer on the heap instead.
    TestBufferVec testbuf {};
    testbuf.reserve(kDecoderProbeBufferSize);

    UnpauseBuffer();

    // delete any pre-existing recorder
    SetDecoder(nullptr);
    int testreadsize = 2048;

    // Test the incoming buffer and create a suitable decoder
    MythTimer bigTimer;
    bigTimer.start();
    int timeout = std::max((Retries + 1) * 500, 30000);
    while (testreadsize <= kDecoderProbeBufferSize)
    {
        testbuf.resize(testreadsize);
        MythTimer peekTimer;
        peekTimer.start();
        while (m_playerCtx->m_buffer->Peek(testbuf) != testreadsize)
        {
            // NB need to allow for streams encountering network congestion
            if (peekTimer.elapsed() > 30000 || bigTimer.elapsed() > timeout
                || m_playerCtx->m_buffer->GetStopReads())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("OpenFile(): Could not read first %1 bytes of '%2'")
                        .arg(testreadsize)
                        .arg(m_playerCtx->m_buffer->GetFilename()));
                SetErrored(tr("Could not read first %1 bytes").arg(testreadsize));
                return -1;
            }
            LOG(VB_GENERAL, LOG_WARNING, LOC + "OpenFile() waiting on data");
            usleep(50 * 1000);
        }

        m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
        CreateDecoder(testbuf);
        m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
        if (m_decoder || (bigTimer.elapsed() > timeout))
            break;
        testreadsize <<= 1;
    }

    // Fail
    if (!m_decoder)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Couldn't find an A/V decoder for: '%1'")
                .arg(m_playerCtx->m_buffer->GetFilename()));
        SetErrored(tr("Could not find an A/V decoder"));

        return -1;
    }

    if (m_decoder->IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Could not initialize A/V decoder.");
        SetDecoder(nullptr);
        SetErrored(tr("Could not initialize A/V decoder"));

        return -1;
    }

    // Pre-init the decoder
    m_decoder->SetSeekSnap(0);
    m_decoder->SetLiveTVMode(m_liveTV);
    m_decoder->SetWatchingRecording(m_watchingRecording);
    // TODO (re)move this into MythTranscode player
    m_decoder->SetTranscoding(m_transcoding);

    // Open the decoder
    int result = m_decoder->OpenFile(m_playerCtx->m_buffer, false, testbuf);

    if (result < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't open decoder for: %1")
                .arg(m_playerCtx->m_buffer->GetFilename()));
        SetErrored(tr("Could not open decoder"));
        return -1;
    }

    // Disable audio if necessary
    m_audio.CheckFormat();

    // Livetv, recording or in-progress
    if (result > 0)
    {
        m_hasFullPositionMap = true;
        m_deleteMap.LoadMap();
        m_deleteMap.TrackerReset(0);
    }

    // Determine the initial bookmark and update it for the cutlist
    m_bookmarkSeek = GetBookmark();
    m_deleteMap.TrackerReset(m_bookmarkSeek);
    m_deleteMap.TrackerWantsToJump(m_bookmarkSeek, m_bookmarkSeek);

    if (!gCoreContext->IsDatabaseIgnored() &&
        m_playerCtx->m_playingInfo->QueryAutoExpire() == kLiveTVAutoExpire)
    {
        gCoreContext->SaveSetting("DefaultChanid",
                                  static_cast<int>(m_playerCtx->m_playingInfo->GetChanID()));
        QString callsign = m_playerCtx->m_playingInfo->GetChannelSchedulingID();
        QString channum = m_playerCtx->m_playingInfo->GetChanNum();
        gCoreContext->SaveSetting("DefaultChanKeys", callsign + "[]:[]" + channum);
        if (m_playerCtx->m_recorder && m_playerCtx->m_recorder->IsValidRecorder())
        {
            uint cardid = static_cast<uint>(m_playerCtx->m_recorder->GetRecorderNumber());
            CardUtil::SetStartChannel(cardid, channum);
        }
    }

    return IsErrored() ? -1 : 0;
}

void MythPlayer::SetFramesPlayed(uint64_t played)
{
    m_framesPlayed = played;
    if (m_videoOutput)
        m_videoOutput->SetFramesPlayed(played);
}

/** \fn MythPlayer::GetFreeVideoFrames(void)
 *  \brief Returns the number of frames available for decoding onto.
 */
int MythPlayer::GetFreeVideoFrames(void) const
{
    if (m_videoOutput)
        return m_videoOutput->FreeVideoFrames();
    return 0;
}

/// \brief Return a list of frame types that can be rendered directly.
const VideoFrameTypes *MythPlayer::DirectRenderFormats(void)
{
    if (m_videoOutput)
        return m_videoOutput->DirectRenderFormats();
    return &MythVideoOutput::s_defaultFrameTypes;
}

/**
 *  \brief Removes a frame from the available queue for decoding onto.
 *
 *   This places the frame in the limbo queue, from which frames are
 *   removed if they are added to another queue. Normally a frame is
 *   freed from limbo either by a ReleaseNextVideoFrame() or
 *   DiscardVideoFrame() call; but limboed frames are also freed
 *   during a seek reset.
 */
MythVideoFrame *MythPlayer::GetNextVideoFrame(void)
{
    if (m_videoOutput)
        return m_videoOutput->GetNextFreeFrame();
    return nullptr;
}

/** \fn MythPlayer::ReleaseNextVideoFrame(VideoFrame*, int64_t)
 *  \brief Places frame on the queue of frames ready for display.
 */
void MythPlayer::ReleaseNextVideoFrame(MythVideoFrame *buffer,
                                       int64_t timecode,
                                       bool wrap)
{
    if (wrap)
        WrapTimecode(timecode, TC_VIDEO);
    buffer->timecode = timecode;
    m_latestVideoTimecode = timecode;

    if (m_videoOutput)
        m_videoOutput->ReleaseFrame(buffer);

    if (m_allPaused)
        CheckAspectRatio(buffer);
}

/** \fn MythPlayer::DiscardVideoFrame(VideoFrame*)
 *  \brief Places frame in the available frames queue.
 */
void MythPlayer::DiscardVideoFrame(MythVideoFrame *buffer)
{
    if (m_videoOutput)
        m_videoOutput->DiscardFrame(buffer);
}

/** \fn MythPlayer::DiscardVideoFrames(bool)
 *  \brief Places frames in the available frames queue.
 *
 *   If called with 'Keyframe' set to false then all frames
 *   not in use by the decoder are made available for decoding. Otherwise,
 *   all frames are made available for decoding; this is only safe if
 *   the next frame is a keyframe.
 *
 *  \param Keyframe if this is true all frames are placed
 *         in the available queue.
 *  \param Flushed indicates that the decoder has been flushed AND the decoder
 * requires ALL frames to be released (used for VAAPI and VDPAU pause frames)
 */
void MythPlayer::DiscardVideoFrames(bool KeyFrame, bool Flushed)
{
    if (m_videoOutput)
        m_videoOutput->DiscardFrames(KeyFrame, Flushed);
}

bool MythPlayer::HasReachedEof(void) const
{
    EofState eof = GetEof();
    if (eof != kEofStateNone && !m_allPaused)
        return true;
    if (GetEditMode())
        return false;
    if (m_liveTV)
        return false;
    if (!m_deleteMap.IsEmpty() && m_framesPlayed >= m_deleteMap.GetLastFrame())
        return true;
    return false;
}

MythVideoFrame *MythPlayer::GetCurrentFrame(int &w, int &h)
{
    w = m_videoDim.width();
    h = m_videoDim.height();

    MythVideoFrame *retval = nullptr;

    m_vidExitLock.lock();
    if (m_videoOutput)
        retval = m_videoOutput->GetLastShownFrame();

    if (!retval)
        m_vidExitLock.unlock();

    return retval;
}

void MythPlayer::DeLimboFrame(MythVideoFrame *frame)
{
    if (m_videoOutput)
        m_videoOutput->DeLimboFrame(frame);
}

void MythPlayer::ReleaseCurrentFrame(MythVideoFrame *frame)
{
    if (frame)
        m_vidExitLock.unlock();
}

void MythPlayer::EnableTeletext(int page)
{
    QMutexLocker locker(&m_osdLock);
    if (!m_osd)
        return;

    m_osd->EnableTeletext(true, page);
    m_prevTextDisplayMode = m_textDisplayMode;
    m_textDisplayMode = kDisplayTeletextMenu;
}

void MythPlayer::DisableTeletext(void)
{
    QMutexLocker locker(&m_osdLock);
    if (!m_osd)
        return;

    m_osd->EnableTeletext(false, 0);
    m_textDisplayMode = kDisplayNone;

    /* If subtitles are enabled before the teletext menu was displayed,
       re-enabled them. */
    if (m_prevTextDisplayMode & kDisplayAllCaptions)
        EnableCaptions(m_prevTextDisplayMode, false);
}

void MythPlayer::ResetTeletext(void)
{
    QMutexLocker locker(&m_osdLock);
    if (!m_osd)
        return;

    m_osd->TeletextReset();
}

/** \fn MythPlayer::SetTeletextPage(uint)
 *  \brief Set Teletext NUV Caption page
 */
void MythPlayer::SetTeletextPage(uint page)
{
    m_osdLock.lock();
    DisableCaptions(m_textDisplayMode);
    m_ttPageNum = page;
    m_cc608.SetTTPageNum(m_ttPageNum);
    m_textDisplayMode &= ~kDisplayAllCaptions;
    m_textDisplayMode |= kDisplayNUVTeletextCaptions;
    m_osdLock.unlock();
}

bool MythPlayer::HandleTeletextAction(const QString &action)
{
    if (!(m_textDisplayMode & kDisplayTeletextMenu) || !m_osd)
        return false;

    bool handled = true;

    m_osdLock.lock();
    if (action == "MENU" || action == ACTION_TOGGLETT || action == "ESCAPE")
        DisableTeletext();
    else if (m_osd)
        handled = m_osd->TeletextAction(action);
    m_osdLock.unlock();

    return handled;
}

void MythPlayer::ResetCaptions(void)
{
    QMutexLocker locker(&m_osdLock);
    if (!m_osd)
        return;

    if (((m_textDisplayMode & kDisplayAVSubtitle)      ||
         (m_textDisplayMode & kDisplayTextSubtitle)    ||
         (m_textDisplayMode & kDisplayRawTextSubtitle) ||
         (m_textDisplayMode & kDisplayDVDButton)       ||
         (m_textDisplayMode & kDisplayCC608)           ||
         (m_textDisplayMode & kDisplayCC708)))
    {
        m_osd->ClearSubtitles();
    }
    else if ((m_textDisplayMode & kDisplayTeletextCaptions) ||
             (m_textDisplayMode & kDisplayNUVTeletextCaptions))
    {
        m_osd->TeletextClear();
    }
}

void MythPlayer::DisableCaptions(uint mode, bool osd_msg)
{
    if (m_textDisplayMode)
        m_prevNonzeroTextDisplayMode = m_textDisplayMode;
    m_textDisplayMode &= ~mode;
    ResetCaptions();

    QMutexLocker locker(&m_osdLock);

    bool newTextDesired = (m_textDisplayMode & kDisplayAllTextCaptions) != 0U;
    // Only turn off textDesired if the Operator requested it.
    if (osd_msg || newTextDesired)
        m_textDesired = newTextDesired;
    QString msg = "";
    if (kDisplayNUVTeletextCaptions & mode)
        msg += tr("TXT CAP");
    if (kDisplayTeletextCaptions & mode)
    {
        if (m_decoder != nullptr)
            msg += m_decoder->GetTrackDesc(kTrackTypeTeletextCaptions,
                                       GetTrack(kTrackTypeTeletextCaptions));
        DisableTeletext();
    }
    int preserve = m_textDisplayMode & (kDisplayCC608 | kDisplayTextSubtitle |
                                        kDisplayAVSubtitle | kDisplayCC708 |
                                        kDisplayRawTextSubtitle);
    if ((kDisplayCC608 & mode) || (kDisplayCC708 & mode) ||
        (kDisplayAVSubtitle & mode) || (kDisplayRawTextSubtitle & mode))
    {
        int type = toTrackType(mode);
        if (m_decoder != nullptr)
            msg += m_decoder->GetTrackDesc(type, GetTrack(type));
        if (m_osd)
            m_osd->EnableSubtitles(preserve);
    }
    if (kDisplayTextSubtitle & mode)
    {
        msg += tr("Text subtitles");
        if (m_osd)
            m_osd->EnableSubtitles(preserve);
    }
    if (!msg.isEmpty() && osd_msg)
    {
        msg += " " + tr("Off");
        SetOSDMessage(msg, kOSDTimeout_Med);
    }
}

void MythPlayer::EnableCaptions(uint mode, bool osd_msg)
{
    QMutexLocker locker(&m_osdLock);
    bool newTextDesired = (mode & kDisplayAllTextCaptions) != 0U;
    // Only turn off textDesired if the Operator requested it.
    if (osd_msg || newTextDesired)
        m_textDesired = newTextDesired;
    QString msg = "";
    if ((kDisplayCC608 & mode) || (kDisplayCC708 & mode) ||
        (kDisplayAVSubtitle & mode) || kDisplayRawTextSubtitle & mode)
    {
        int type = toTrackType(mode);
        if (m_decoder != nullptr)
            msg += m_decoder->GetTrackDesc(type, GetTrack(type));
        if (m_osd)
            m_osd->EnableSubtitles(mode);
    }
    if (kDisplayTextSubtitle & mode)
    {
        if (m_osd)
            m_osd->EnableSubtitles(kDisplayTextSubtitle);
        msg += tr("Text subtitles");
    }
    if (kDisplayNUVTeletextCaptions & mode)
        msg += tr("TXT %1").arg(m_ttPageNum, 3, 16);
    if ((kDisplayTeletextCaptions & mode) && (m_decoder != nullptr))
    {
        msg += m_decoder->GetTrackDesc(kTrackTypeTeletextCaptions,
                                       GetTrack(kTrackTypeTeletextCaptions));

        int page = m_decoder->GetTrackLanguageIndex(
            kTrackTypeTeletextCaptions,
            GetTrack(kTrackTypeTeletextCaptions));

        EnableTeletext(page);
        m_textDisplayMode = kDisplayTeletextCaptions;
    }

    msg += " " + tr("On");

    LOG(VB_PLAYBACK, LOG_INFO, QString("EnableCaptions(%1) msg: %2")
        .arg(mode).arg(msg));

    m_textDisplayMode = mode;
    if (m_textDisplayMode)
        m_prevNonzeroTextDisplayMode = m_textDisplayMode;
    if (osd_msg)
        SetOSDMessage(msg, kOSDTimeout_Med);
}

bool MythPlayer::ToggleCaptions(void)
{
    SetCaptionsEnabled(!((bool)m_textDisplayMode));
    return m_textDisplayMode != 0U;
}

bool MythPlayer::ToggleCaptions(uint type)
{
    QMutexLocker locker(&m_osdLock);
    uint mode = toCaptionType(type);
    uint origMode = m_textDisplayMode;

    if (m_textDisplayMode)
        DisableCaptions(m_textDisplayMode, (origMode & mode) != 0U);
    if (origMode & mode)
        return m_textDisplayMode != 0U;
    if (mode)
        EnableCaptions(mode);
    return m_textDisplayMode != 0U;
}

void MythPlayer::SetCaptionsEnabled(bool enable, bool osd_msg)
{
    QMutexLocker locker(&m_osdLock);
    m_enableCaptions = m_disableCaptions = false;
    uint origMode = m_textDisplayMode;

    // Only turn off textDesired if the Operator requested it.
    if (osd_msg || enable)
        m_textDesired = enable;

    if (!enable)
    {
        DisableCaptions(origMode, osd_msg);
        return;
    }
    int mode = HasCaptionTrack(m_prevNonzeroTextDisplayMode) ?
        m_prevNonzeroTextDisplayMode : NextCaptionTrack(kDisplayNone);
    if (origMode != (uint)mode)
    {
        DisableCaptions(origMode, false);

        if (kDisplayNone == mode)
        {
            if (osd_msg)
            {
                SetOSDMessage(tr("No captions",
                                 "CC/Teletext/Subtitle text not available"),
                              kOSDTimeout_Med);
            }
            LOG(VB_PLAYBACK, LOG_INFO,
                "No captions available yet to enable.");
        }
        else if (mode)
        {
            EnableCaptions(mode, osd_msg);
        }
    }
    ResetCaptions();
}

bool MythPlayer::GetCaptionsEnabled(void) const
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

QStringList MythPlayer::GetTracks(uint type)
{
    if (m_decoder)
        return m_decoder->GetTracks(type);
    return QStringList();
}

uint MythPlayer::GetTrackCount(uint type)
{
    if (m_decoder)
        return m_decoder->GetTrackCount(type);
    return 0;
}

int MythPlayer::SetTrack(uint type, int trackNo)
{
    int ret = -1;
    if (!m_decoder)
        return ret;

    ret = m_decoder->SetTrack(type, trackNo);
    if (kTrackTypeAudio == type)
    {
        if (m_decoder)
            SetOSDMessage(m_decoder->GetTrackDesc(type, GetTrack(type)),
                          kOSDTimeout_Med);
        return ret;
    }

    uint subtype = toCaptionType(type);
    if (subtype)
    {
        DisableCaptions(m_textDisplayMode, false);
        EnableCaptions(subtype, true);
        if ((kDisplayCC708 == subtype || kDisplayCC608 == subtype) && m_decoder)
        {
            int sid = m_decoder->GetTrackInfo(type, trackNo).m_stream_id;
            if (sid >= 0)
            {
                (kDisplayCC708 == subtype) ? m_cc708.SetCurrentService(sid) :
                                             m_cc608.SetMode(sid);
            }
        }
    }
    return ret;
}

/** \fn MythPlayer::TracksChanged(uint)
 *  \brief This tries to re-enable captions/subtitles if the user
 *         wants them and one of the captions/subtitles tracks has
 *         changed.
 */
void MythPlayer::TracksChanged(uint trackType)
{
    if (trackType >= kTrackTypeSubtitle &&
        trackType <= kTrackTypeTeletextCaptions && m_textDesired)
    {
        m_enableCaptions = true;
    }
}

void MythPlayer::EnableSubtitles(bool enable)
{
    if (enable)
        m_enableCaptions = true;
    else
        m_disableCaptions = true;
}

void MythPlayer::EnableForcedSubtitles(bool enable)
{
    if (enable)
        m_enableForcedSubtitles = true;
    else
        m_disableForcedSubtitles = true;
}

void MythPlayer::SetAllowForcedSubtitles(bool allow)
{
    m_allowForcedSubtitles = allow;
    SetOSDMessage(m_allowForcedSubtitles ?
                      tr("Forced Subtitles On") :
                      tr("Forced Subtitles Off"),
                  kOSDTimeout_Med);
}

void MythPlayer::DoDisableForcedSubtitles(void)
{
    m_disableForcedSubtitles = false;
    m_osdLock.lock();
    if (m_osd)
        m_osd->DisableForcedSubtitles();
    m_osdLock.unlock();
}

void MythPlayer::DoEnableForcedSubtitles(void)
{
    m_enableForcedSubtitles = false;
    if (!m_allowForcedSubtitles)
        return;

    m_osdLock.lock();
    if (m_osd)
        m_osd->EnableSubtitles(kDisplayAVSubtitle, true /*forced only*/);
    m_osdLock.unlock();
}

int MythPlayer::GetTrack(uint type)
{
    if (m_decoder)
        return m_decoder->GetTrack(type);
    return -1;
}

int MythPlayer::ChangeTrack(uint type, int dir)
{
    if (!m_decoder)
        return -1;

    int retval = m_decoder->ChangeTrack(type, dir);
    if (retval >= 0)
    {
        SetOSDMessage(m_decoder->GetTrackDesc(type, GetTrack(type)),
                      kOSDTimeout_Med);
        return retval;
    }
    return -1;
}

void MythPlayer::ChangeCaptionTrack(int dir)
{
    if (!m_decoder || (dir < 0))
        return;

    if (!((m_textDisplayMode == kDisplayTextSubtitle) ||
          (m_textDisplayMode == kDisplayNUVTeletextCaptions) ||
          (m_textDisplayMode == kDisplayNone)))
    {
        int tracktype = toTrackType(m_textDisplayMode);
        if (GetTrack(tracktype) < m_decoder->NextTrack(tracktype))
        {
            SetTrack(tracktype, m_decoder->NextTrack(tracktype));
            return;
        }
    }
    int nextmode = NextCaptionTrack(m_textDisplayMode);
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
        int tracktype = toTrackType(nextmode);
        int tracks = m_decoder->GetTrackCount(tracktype);
        if (tracks)
        {
            DisableCaptions(m_textDisplayMode, true);
            SetTrack(tracktype, 0);
        }
    }
}

bool MythPlayer::HasCaptionTrack(int mode)
{
    if (mode == kDisplayNone)
        return false;
    if (((mode == kDisplayTextSubtitle) && HasTextSubtitles()) ||
         (mode == kDisplayNUVTeletextCaptions))
    {
        return true;
    }
    if (!(mode == kDisplayTextSubtitle) &&
               m_decoder->GetTrackCount(toTrackType(mode)))
    {
        return true;
    }
    return false;
}

int MythPlayer::NextCaptionTrack(int mode)
{
    // Text->TextStream->708->608->AVSubs->Teletext->NUV->None
    // NUV only offerred if PAL
    bool pal      = (m_vbiMode == VBIMode::PAL_TT);
    int  nextmode = kDisplayNone;

    if (kDisplayTextSubtitle == mode)
        nextmode = kDisplayRawTextSubtitle;
    else if (kDisplayRawTextSubtitle == mode)
        nextmode = kDisplayCC708;
    else if (kDisplayCC708 == mode)
        nextmode = kDisplayCC608;
    else if (kDisplayCC608 == mode)
        nextmode = kDisplayAVSubtitle;
    else if (kDisplayAVSubtitle == mode)
        nextmode = kDisplayTeletextCaptions;
    else if (kDisplayTeletextCaptions == mode)
        nextmode = pal ? kDisplayNUVTeletextCaptions : kDisplayNone;
    else if ((kDisplayNUVTeletextCaptions == mode) && pal)
        nextmode = kDisplayNone;
    else if (kDisplayNone == mode)
        nextmode = kDisplayTextSubtitle;

    if (nextmode == kDisplayNone || HasCaptionTrack(nextmode))
        return nextmode;

    return NextCaptionTrack(nextmode);
}

void MythPlayer::SetFrameInterval(FrameScanType scan, double frame_period)
{
    if (m_decoder)
        m_fpsMultiplier = m_decoder->GetfpsMultiplier();
    m_frameInterval = static_cast<int>(lround(1000000.0 * frame_period) / m_fpsMultiplier);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("SetFrameInterval Interval:%1 Speed:%2 Scan:%3 (Multiplier: %4)")
        .arg(m_frameInterval).arg(static_cast<double>(m_playSpeed)).arg(ScanTypeToString(scan)).arg(m_fpsMultiplier));
}

void MythPlayer::InitFrameInterval()
{
    // try to get preferential scheduling, but ignore if we fail to.
    myth_nice(-19);
}

void MythPlayer::SetBuffering(bool new_buffering)
{
    if (!m_buffering && new_buffering)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Waiting for video buffers...");
        m_buffering = true;
        m_bufferingStart = QTime::currentTime();
        m_bufferingLastMsg = QTime::currentTime();
    }
    else if (m_buffering && !new_buffering)
    {
        m_buffering = false;
    }
}

// For debugging playback set this to increase the timeout so that
// playback does not fail if stepping through code.
// Set PREBUFFERDEBUG to any value and you will get 30 minutes.
static char *preBufferDebug = getenv("PREBUFFERDEBUG");

bool MythPlayer::PrebufferEnoughFrames(int min_buffers)
{
    if (!m_videoOutput)
        return false;

    if (!(min_buffers ? (m_videoOutput->ValidVideoFrames() >= min_buffers) :
                        (GetEof() != kEofStateNone) || m_videoOutput->EnoughDecodedFrames()))
    {
        SetBuffering(true);

        // This piece of code is to address the problem, when starting
        // Live TV, of jerking and stuttering. Without this code
        // that could go on forever, but is cured by a pause and play.
        // This code inserts a brief pause and play when the potential
        // for the jerking is detected.

        bool watchingTV = IsWatchingInprogress();
        if ((m_liveTV || watchingTV) && !FlagIsSet(kMusicChoice))
        {
            uint64_t frameCount = GetCurrentFrameCount();
            uint64_t framesLeft = frameCount - m_framesPlayed;
            auto margin = static_cast<uint64_t>(m_videoFrameRate * 3);
            if (framesLeft < margin)
            {
                if (m_avSync.ResetAVSyncForLiveTV(&m_audio))
                {
                    LOG(VB_PLAYBACK, LOG_NOTICE, LOC + "Pause to allow live tv catch up");
                    LOG(VB_PLAYBACK, LOG_NOTICE, LOC + QString("Played: %1 Avail: %2 Buffered: %3 Margin: %4")
                        .arg(m_framesPlayed).arg(frameCount)
                        .arg(m_videoOutput->ValidVideoFrames()).arg(margin));
                }
            }
        }

        usleep(static_cast<uint>(m_frameInterval >> 3));
        int waited_for = m_bufferingStart.msecsTo(QTime::currentTime());
        int last_msg = m_bufferingLastMsg.msecsTo(QTime::currentTime());
        if (last_msg > 100 && !FlagIsSet(kMusicChoice))
        {
            if (++m_bufferingCounter == 10)
                LOG(VB_GENERAL, LOG_NOTICE, LOC +
                    "To see more buffering messages use -v playback");
            if (m_bufferingCounter >= 10)
            {
                LOG(VB_PLAYBACK, LOG_NOTICE, LOC +
                    QString("Waited %1ms for video buffers %2")
                    .arg(waited_for).arg(m_videoOutput->GetFrameStatus()));
            }
            else
            {
                LOG(VB_GENERAL, LOG_NOTICE, LOC +
                    QString("Waited %1ms for video buffers %2")
                        .arg(waited_for).arg(m_videoOutput->GetFrameStatus()));
            }
            m_bufferingLastMsg = QTime::currentTime();
            if (m_audio.IsBufferAlmostFull() && m_framesPlayed < 5
                && gCoreContext->GetBoolSetting("MusicChoiceEnabled", false))
            {
                m_playerFlags = static_cast<PlayerFlags>(m_playerFlags | kMusicChoice);
                LOG(VB_GENERAL, LOG_NOTICE, LOC + "Music Choice program detected - disabling AV Sync.");
                m_avSync.SetAVSyncMusicChoice(&m_audio);
            }
            if (waited_for > 7000 && m_audio.IsBufferAlmostFull()
                && !FlagIsSet(kMusicChoice))
            {
                // We are likely to enter this condition
                // if the audio buffer was too full during GetFrame in AVFD
                LOG(VB_GENERAL, LOG_NOTICE, LOC + "Resetting audio buffer");
                m_audio.Reset();
            }
            // Finish audio pause for sync after 1 second
            // in case of infrequent video frames (e.g. music choice)
            if (m_avSync.GetAVSyncAudioPause() && waited_for > 1000)
                m_avSync.SetAVSyncMusicChoice(&m_audio);
        }
        int msecs = 500;
        if (preBufferDebug)
            msecs = 1800000;
        if ((waited_for > msecs /*500*/) && !m_videoOutput->EnoughFreeFrames())
        {
            LOG(VB_GENERAL, LOG_NOTICE, LOC +
                "Timed out waiting for frames, and"
                "\n\t\t\tthere are not enough free frames. "
                "Discarding buffered frames.");
            // This call will result in some ugly frames, but allows us
            // to recover from serious problems if frames get leaked.
            DiscardVideoFrames(true, true);
        }
        msecs = 30000;
        if (preBufferDebug)
            msecs = 1800000;
        if (waited_for > msecs /*30000*/) // 30 seconds for internet streamed media
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Waited too long for decoder to fill video buffers. Exiting..");
            SetErrored(tr("Video frame buffering failed too many times."));
        }
        return false;
    }

    if (!m_avSync.GetAVSyncAudioPause())
        m_audio.Pause(false);
    SetBuffering(false);
    return true;
}

void MythPlayer::CheckAspectRatio(MythVideoFrame* frame)
{
    if (!frame)
        return;

    if (!qFuzzyCompare(frame->aspect, m_videoAspect) && frame->aspect > 0.0F)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Video Aspect ratio changed from %1 to %2")
            .arg(m_videoAspect).arg(frame->aspect));
        m_videoAspect = frame->aspect;
        if (m_videoOutput)
        {
            m_videoOutput->VideoAspectRatioChanged(m_videoAspect);
            ReinitOSD();
        }
    }
}

void MythPlayer::PreProcessNormalFrame(void)
{
#ifdef USING_MHEG
    // handle Interactive TV
    if (GetInteractiveTV())
    {
        m_osdLock.lock();
        m_itvLock.lock();
        if (m_osd && m_videoOutput->GetOSDPainter())
        {
            auto *window =
                qobject_cast<InteractiveScreen *>(m_osd->GetWindow(OSD_WIN_INTERACT));
            if ((m_interactiveTV->ImageHasChanged() || !m_itvVisible) && window)
            {
                m_interactiveTV->UpdateOSD(window, m_videoOutput->GetOSDPainter());
                m_itvVisible = true;
            }
        }
        m_itvLock.unlock();
        m_osdLock.unlock();
    }
#endif // USING_MHEG
}

void MythPlayer::ForceDeinterlacer(bool DoubleRate, MythDeintType Deinterlacer)
{
    if (m_videoOutput)
        m_videoOutput->SetDeinterlacing(true, DoubleRate, Deinterlacer);
}

void MythPlayer::VideoStart(void)
{
    SetPlaying(true);
    ClearAfterSeek(false);
    m_avSync.InitAVSync();
    InitFrameInterval();
}

void MythPlayer::VideoEnd(void)
{
    m_osdLock.lock();
    m_vidExitLock.lock();
    delete m_osd;
    delete m_videoOutput;
    m_osd         = nullptr;
    m_videoOutput = nullptr;
    m_vidExitLock.unlock();
    m_osdLock.unlock();
}

bool MythPlayer::FastForward(float seconds)
{
    if (!m_videoOutput)
        return false;

    if (m_ffTime <= 0)
    {
        float current   = ComputeSecs(m_framesPlayed, true);
        float dest      = current + seconds;
        float length    = ComputeSecs(m_totalFrames, true);

        if (dest > length)
        {
            int64_t pos = TranslatePositionMsToFrame(seconds * 1000, false);
            if (CalcMaxFFTime(pos) < 0)
                return true;
            // Reach end of recording, go to 1 or 3s before the end
            dest = (m_liveTV || IsWatchingInprogress()) ? -3.0 : -1.0;
        }
        uint64_t target = FindFrame(dest, true);
        m_ffTime = target - m_framesPlayed;
    }
    return m_ffTime > CalcMaxFFTime(m_ffTime, false);
}

bool MythPlayer::Rewind(float seconds)
{
    if (!m_videoOutput)
        return false;

    if (m_rewindTime <= 0)
    {
        float current = ComputeSecs(m_framesPlayed, true);
        float dest = current - seconds;
        if (dest < 0)
        {
            int64_t pos = TranslatePositionMsToFrame(seconds * 1000, false);
            if (CalcRWTime(pos) < 0)
                return true;
            dest = 0;
        }
        uint64_t target = FindFrame(dest, true);
        m_rewindTime = m_framesPlayed - target;
    }
    return (uint64_t)m_rewindTime >= m_framesPlayed;
}

bool MythPlayer::JumpToFrame(uint64_t frame)
{
    if (!m_videoOutput)
        return false;

    bool ret = false;
    m_ffTime = m_rewindTime = 0;
    if (frame > m_framesPlayed)
    {
        m_ffTime = frame - m_framesPlayed;
        ret = m_ffTime > CalcMaxFFTime(m_ffTime, false);
    }
    else if (frame < m_framesPlayed)
    {
        m_rewindTime = m_framesPlayed - frame;
        ret = m_ffTime > CalcMaxFFTime(m_ffTime, false);
    }
    return ret;
}


void MythPlayer::JumpChapter(int chapter)
{
    if (m_jumpChapter == 0)
        m_jumpChapter = chapter;
}

void MythPlayer::ResetPlaying(bool resetframes)
{
    ClearAfterSeek();
    m_ffrewSkip = 1;
    if (resetframes)
        m_framesPlayed = 0;
    if (m_decoder)
    {
        m_decoder->Reset(true, true, true);
        if (m_decoder->IsErrored())
            SetErrored("Unable to reset video decoder");
    }
}

void MythPlayer::CheckTVChain(void)
{
    bool last = !(m_playerCtx->m_tvchain->HasNext());
    SetWatchingRecording(last);
}

void MythPlayer::SwitchToProgram(void)
{
    if (!IsReallyNearEnd())
        return;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "SwitchToProgram - start");
    bool discontinuity = false;
    bool newtype = false;
    int newid = -1;
    ProgramInfo *pginfo = m_playerCtx->m_tvchain->GetSwitchProgram(
        discontinuity, newtype, newid);
    if (!pginfo)
        return;

    bool newIsDummy = m_playerCtx->m_tvchain->GetInputType(newid) == "DUMMY";

    SetPlayingInfo(*pginfo);
    Pause();
    ChangeSpeed();

    // Release all frames to ensure the current decoder resources are released
    DiscardVideoFrames(true, true);

    if (newIsDummy)
    {
        OpenDummy();
        ResetPlaying();
        SetEof(kEofStateNone);
        delete pginfo;
        return;
    }

    if (m_playerCtx->m_buffer->GetType() == kMythBufferMHEG)
    {
        // Restore original ringbuffer
        auto *ic = dynamic_cast<MythInteractiveBuffer*>(m_playerCtx->m_buffer);
        if (ic)
            m_playerCtx->m_buffer = ic->TakeBuffer();
        delete ic;
    }

    m_playerCtx->m_buffer->OpenFile(
        pginfo->GetPlaybackURL(), MythMediaBuffer::kLiveTVOpenTimeout);

    if (!m_playerCtx->m_buffer->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "SwitchToProgram's OpenFile failed " +
            QString("(input type: %1).")
            .arg(m_playerCtx->m_tvchain->GetInputType(newid)));
        LOG(VB_GENERAL, LOG_ERR, m_playerCtx->m_tvchain->toString());
        SetEof(kEofStateImmediate);
        SetErrored(tr("Error opening switch program buffer"));
        delete pginfo;
        return;
    }

    if (GetEof() != kEofStateNone)
    {
        discontinuity = true;
        ResetCaptions();
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("SwitchToProgram(void) "
        "discont: %1 newtype: %2 newid: %3 decoderEof: %4")
        .arg(discontinuity).arg(newtype).arg(newid).arg(GetEof()));

    if (discontinuity || newtype)
    {
        m_playerCtx->m_tvchain->SetProgram(*pginfo);
        if (m_decoder)
            m_decoder->SetProgramInfo(*pginfo);

        m_playerCtx->m_buffer->Reset(true);
        if (newtype)
        {
            if (OpenFile() < 0)
                SetErrored(tr("Error opening switch program file"));
        }
        else
            ResetPlaying();
    }
    else
    {
        m_playerCtx->SetPlayerChangingBuffers(true);
        if (m_decoder)
        {
            m_decoder->SetReadAdjust(m_playerCtx->m_buffer->SetAdjustFilesize());
            m_decoder->SetWaitForChange();
        }
    }
    delete pginfo;

    if (IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "SwitchToProgram failed.");
        SetEof(kEofStateDelayed);
        return;
    }

    SetEof(kEofStateNone);

    // the bitrate is reset by m_playerCtx->m_buffer->OpenFile()...
    if (m_decoder)
        m_playerCtx->m_buffer->UpdateRawBitrate(m_decoder->GetRawBitrate());
    m_playerCtx->m_buffer->Unpause();

    if (discontinuity || newtype)
    {
        CheckTVChain();
        m_forcePositionMapSync = true;
    }

    Play();
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "SwitchToProgram - end");
}

// This is called from decoder thread. Set an indicator that will
// be checked and actioned in the player thread.
void MythPlayer::FileChangedCallback(void)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "FileChangedCallback");
    m_fileChanged = true;
}

// Called from the player thread.
void MythPlayer::FileChanged(void)
{
    m_fileChanged = false;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "FileChanged");

    Pause();
    ChangeSpeed();
    if (dynamic_cast<AvFormatDecoder *>(m_decoder))
        m_playerCtx->m_buffer->Reset(false, true);
    else
        m_playerCtx->m_buffer->Reset(false, true, true);
    SetEof(kEofStateNone);
    Play();

    m_playerCtx->SetPlayerChangingBuffers(false);

    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    m_playerCtx->m_tvchain->SetProgram(*m_playerCtx->m_playingInfo);
    if (m_decoder)
        m_decoder->SetProgramInfo(*m_playerCtx->m_playingInfo);
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);

    CheckTVChain();
    m_forcePositionMapSync = true;
}

void MythPlayer::JumpToProgram(void)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "JumpToProgram - start");
    bool discontinuity = false;
    bool newtype = false;
    int newid = -1;
    long long nextpos = m_playerCtx->m_tvchain->GetJumpPos();
    ProgramInfo *pginfo = m_playerCtx->m_tvchain->GetSwitchProgram(
        discontinuity, newtype, newid);
    if (!pginfo)
        return;

    m_inJumpToProgramPause = true;

    bool newIsDummy = m_playerCtx->m_tvchain->GetInputType(newid) == "DUMMY";
    SetPlayingInfo(*pginfo);

    Pause();
    ChangeSpeed();
    ResetCaptions();

    // Release all frames to ensure the current decoder resources are released
    DiscardVideoFrames(true, true);

    m_playerCtx->m_tvchain->SetProgram(*pginfo);
    m_playerCtx->m_buffer->Reset(true);

    if (newIsDummy)
    {
        OpenDummy();
        ResetPlaying();
        SetEof(kEofStateNone);
        delete pginfo;
        m_inJumpToProgramPause = false;
        return;
    }

    SendMythSystemPlayEvent("PLAY_CHANGED", pginfo);

    if (m_playerCtx->m_buffer->GetType() == kMythBufferMHEG)
    {
        // Restore original ringbuffer
        auto *ic = dynamic_cast<MythInteractiveBuffer*>(m_playerCtx->m_buffer);
        if (ic)
            m_playerCtx->m_buffer = ic->TakeBuffer();
        delete ic;
    }

    m_playerCtx->m_buffer->OpenFile(pginfo->GetPlaybackURL(), MythMediaBuffer::kLiveTVOpenTimeout);
    QString subfn = m_playerCtx->m_buffer->GetSubtitleFilename();
    TVState desiredState = m_playerCtx->GetState();
    bool isInProgress = (desiredState == kState_WatchingRecording ||
                         desiredState == kState_WatchingLiveTV);
    if (GetSubReader())
        GetSubReader()->LoadExternalSubtitles(subfn, isInProgress &&
                                              !subfn.isEmpty());

    if (!m_playerCtx->m_buffer->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "JumpToProgram's OpenFile failed " +
            QString("(input type: %1).")
                .arg(m_playerCtx->m_tvchain->GetInputType(newid)));
        LOG(VB_GENERAL, LOG_ERR, m_playerCtx->m_tvchain->toString());
        SetEof(kEofStateImmediate);
        SetErrored(tr("Error opening jump program file buffer"));
        delete pginfo;
        m_inJumpToProgramPause = false;
        return;
    }

    bool wasDummy = m_isDummy;
    if (newtype || wasDummy)
    {
        if (OpenFile() < 0)
            SetErrored(tr("Error opening jump program file"));
    }
    else
        ResetPlaying();

    if (IsErrored() || !m_decoder)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "JumpToProgram failed.");
        if (!IsErrored())
            SetErrored(tr("Error reopening video decoder"));
        delete pginfo;
        m_inJumpToProgramPause = false;
        return;
    }

    SetEof(kEofStateNone);

    // the bitrate is reset by m_playerCtx->m_buffer->OpenFile()...
    m_playerCtx->m_buffer->UpdateRawBitrate(m_decoder->GetRawBitrate());
    m_playerCtx->m_buffer->IgnoreLiveEOF(false);

    m_decoder->SetProgramInfo(*pginfo);
    delete pginfo;

    CheckTVChain();
    m_forcePositionMapSync = true;
    m_inJumpToProgramPause = false;
    Play();
    ChangeSpeed();

    // check that we aren't too close to the end of program.
    // and if so set it to 10s from the end if completed recordings
    // or 3s if live
    long long duration = m_playerCtx->m_tvchain->GetLengthAtCurPos();
    int maxpos = m_playerCtx->m_tvchain->HasNext() ? 10 : 3;

    if (nextpos > (duration - maxpos))
    {
        nextpos = duration - maxpos;
        if (nextpos < 0)
        {
            nextpos = 0;
        }
    }
    else if (nextpos < 0)
    {
        // it's a relative position to the end
        nextpos += duration;
    }

    // nextpos is the new position to use in seconds
    nextpos = TranslatePositionMsToFrame(nextpos * 1000, true);

    if (nextpos > 10)
        DoJumpToFrame(nextpos, kInaccuracyNone);

    m_playerCtx->SetPlayerChangingBuffers(false);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "JumpToProgram - end");
}

bool MythPlayer::StartPlaying(void)
{
    if (OpenFile() < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to open video file.");
        return false;
    }

    m_framesPlayed = 0;
    m_rewindTime = m_ffTime = 0;
    m_nextPlaySpeed = m_audio.GetStretchFactor();
    m_jumpChapter = 0;
    m_commBreakMap.SkipCommercials(0);
    m_bufferingCounter=0;

    if (!InitVideo())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to initialize video.");
        m_audio.DeleteOutput();
        return false;
    }

    bool seek = m_bookmarkSeek > 30;
    EventStart();
    DecoderStart(true);
    if (seek)
        InitialSeek();
    VideoStart();

    m_playerThread->setPriority(QThread::TimeCriticalPriority);
#ifdef Q_OS_ANDROID
    setpriority(PRIO_PROCESS, m_playerThreadId, -20);
#endif
    ProcessCallbacks();
    UnpauseDecoder();
    return !IsErrored();
}

void MythPlayer::InitialSeek(void)
{
    // TODO handle initial commskip and/or cutlist skip as well
    if (m_bookmarkSeek > 30)
    {
        DoJumpToFrame(m_bookmarkSeek, kInaccuracyNone);
        if (m_clearSavedPosition)
            SetBookmark(true);
    }
}


void MythPlayer::StopPlaying()
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("StopPlaying - begin"));
    m_playerThread->setPriority(QThread::NormalPriority);
#ifdef Q_OS_ANDROID
    setpriority(PRIO_PROCESS, m_playerThreadId, 0);
#endif

    ProcessCallbacks();
    DecoderEnd();
    VideoEnd();
    AudioEnd();

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("StopPlaying - end"));
}

void MythPlayer::EventStart(void)
{
    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    {
        if (m_playerCtx->m_playingInfo)
        {
            // When initial playback gets underway, we override the ProgramInfo
            // flags such that future calls to GetBookmark() will consider only
            // an actual bookmark and not progstart or lastplaypos information.
            m_playerCtx->m_playingInfo->SetIgnoreBookmark(false);
            m_playerCtx->m_playingInfo->SetIgnoreProgStart(true);
            m_playerCtx->m_playingInfo->SetAllowLastPlayPos(false);
        }
    }
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
    m_commBreakMap.LoadMap(m_playerCtx, m_framesPlayed);
}

void MythPlayer::AudioEnd(void)
{
    m_audio.DeleteOutput();
}

/*! \brief Convenience function to request and wait for a callback into the main thread.
 *
 * This is used by hardware decoders to ensure certain resources are created
 * and destroyed in the UI (render) thread.
*/
void MythPlayer::HandleDecoderCallback(const QString &Debug, DecoderCallback::Callback Function,
                                       void *Opaque1, void *Opaque2)
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

void MythPlayer::ProcessCallbacks(void)
{
    m_decoderCallbackLock.lock();
    for (auto *it = m_decoderCallbacks.begin(); it != m_decoderCallbacks.end(); ++it)
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

bool MythPlayer::PauseDecoder(void)
{
    m_decoderPauseLock.lock();
    if (is_current_thread(m_decoderThread))
    {
        m_decoderPaused = true;
        m_decoderThreadPause.wakeAll();
        m_decoderPauseLock.unlock();
        return m_decoderPaused;
    }

    int tries = 0;
    m_pauseDecoder = true;
    while (m_decoderThread && !m_killDecoder && (tries++ < 100) &&
           !m_decoderThreadPause.wait(&m_decoderPauseLock, 100))
    {
        ProcessCallbacks();
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Waited 100ms for decoder to pause");
    }
    m_pauseDecoder = false;
    m_decoderPauseLock.unlock();
    return m_decoderPaused;
}

void MythPlayer::UnpauseDecoder(void)
{
    m_decoderPauseLock.lock();

    if (is_current_thread(m_decoderThread))
    {
        m_decoderPaused = false;
        m_decoderThreadUnpause.wakeAll();
        m_decoderPauseLock.unlock();
        return;
    }

    if (!IsInStillFrame())
    {
        int tries = 0;
        m_unpauseDecoder = true;
        while (m_decoderThread && !m_killDecoder && (tries++ < 100) &&
               !m_decoderThreadUnpause.wait(&m_decoderPauseLock, 100))
        {
            ProcessCallbacks();
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Waited 100ms for decoder to unpause");
        }
        m_unpauseDecoder = false;
    }
    m_decoderPauseLock.unlock();
}

void MythPlayer::DecoderStart(bool start_paused)
{
    if (m_decoderThread)
    {
        if (m_decoderThread->isRunning())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Decoder thread already running");
        }
        delete m_decoderThread;
    }

    m_killDecoder = false;
    m_decoderPaused = start_paused;
    m_decoderThread = new MythDecoderThread(this, start_paused);
    if (m_decoderThread)
        m_decoderThread->start();
}

void MythPlayer::DecoderEnd(void)
{
    PauseDecoder();
    SetPlaying(false);
    // Ensure any hardware frames are released (after pausing the decoder) to
    // allow the decoder to exit cleanly
    DiscardVideoFrames(true, true);

    m_killDecoder = true;
    int tries = 0;
    while (m_decoderThread && !m_decoderThread->wait(100) && (tries++ < 50))
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            "Waited 100ms for decoder loop to stop");

    if (m_decoderThread && m_decoderThread->isRunning())
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to stop decoder loop.");
    else
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Exited decoder loop.");
    SetDecoder(nullptr);
}

void MythPlayer::DecoderPauseCheck(void)
{
    if (is_current_thread(m_decoderThread))
    {
        if (m_pauseDecoder)
            PauseDecoder();
        if (m_unpauseDecoder)
            UnpauseDecoder();
    }
}

//// FIXME - move the eof ownership back into MythPlayer
EofState MythPlayer::GetEof(void) const
{
    if (is_current_thread(m_playerThread))
        return m_decoder ? m_decoder->GetEof() : kEofStateImmediate;

    if (!m_decoderChangeLock.tryLock(50))
        return kEofStateNone;

    EofState eof = m_decoder ? m_decoder->GetEof() : kEofStateImmediate;
    m_decoderChangeLock.unlock();
    return eof;
}

void MythPlayer::SetEof(EofState eof)
{
    if (is_current_thread(m_playerThread))
    {
        if (m_decoder)
            m_decoder->SetEofState(eof);
        return;
    }

    if (!m_decoderChangeLock.tryLock(50))
        return;

    if (m_decoder)
        m_decoder->SetEofState(eof);
    m_decoderChangeLock.unlock();
}
//// FIXME end

void MythPlayer::DecoderLoop(bool pause)
{
    if (pause)
        PauseDecoder();

    while (!m_killDecoder && !IsErrored())
    {
        DecoderPauseCheck();

        if (m_totalDecoderPause || m_inJumpToProgramPause)
        {
            usleep(1000);
            continue;
        }

        if (m_forcePositionMapSync)
        {
            if (!m_decoderChangeLock.tryLock(1))
                continue;
            if (m_decoder)
            {
                m_forcePositionMapSync = false;
                m_decoder->SyncPositionMap();
            }
            m_decoderChangeLock.unlock();
        }

        if (m_decoderSeek >= 0)
        {
            if (!m_decoderChangeLock.tryLock(1))
                continue;
            if (m_decoder)
            {
                m_decoderSeekLock.lock();
                if (((uint64_t)m_decoderSeek < m_framesPlayed) && m_decoder)
                    m_decoder->DoRewind(m_decoderSeek);
                else if (m_decoder)
                    m_decoder->DoFastForward(m_decoderSeek, !m_transcoding);
                m_decoderSeek = -1;
                m_decoderSeekLock.unlock();
            }
            m_decoderChangeLock.unlock();
        }

        bool obey_eof = (GetEof() != kEofStateNone) &&
                        !(m_playerCtx->m_tvchain && !m_allPaused);
        if (m_isDummy || ((m_decoderPaused || m_ffrewSkip == 0 || obey_eof) &&
                          !m_decodeOneFrame))
        {
            usleep(1000);
            continue;
        }

        DecodeType dt = m_deleteMap.IsEditing() || (m_audio.HasAudioOut() && m_normalSpeed) ?
            kDecodeAV : kDecodeVideo;

        DecoderGetFrame(dt);
        m_decodeOneFrame = false;
    }

    // Clear any wait conditions
    DecoderPauseCheck();
    m_decoderSeek = -1;
}

bool MythPlayer::DecoderGetFrameFFREW(void)
{
    if (!m_decoder)
        return false;

    if (m_ffrewSkip > 0)
    {
        long long delta = m_decoder->GetFramesRead() - m_framesPlayed;
        long long real_skip = CalcMaxFFTime(m_ffrewSkip - m_ffrewAdjust + delta) - delta;
        long long target_frame = m_decoder->GetFramesRead() + real_skip;
        if (real_skip >= 0)
        {
            m_decoder->DoFastForward(target_frame, false);
        }
        long long seek_frame  = m_decoder->GetFramesRead();
        m_ffrewAdjust = seek_frame - target_frame;
    }
    else if (CalcRWTime(-m_ffrewSkip) >= 0)
    {
        DecoderGetFrameREW();
    }
    return DoGetFrame(m_deleteMap.IsEditing() ? kDecodeAV : kDecodeVideo);
}

bool MythPlayer::DecoderGetFrameREW(void)
{
    long long cur_frame    = m_decoder->GetFramesPlayed();
    bool      toBegin      = -cur_frame > m_ffrewSkip + m_ffrewAdjust;
    long long real_skip    = (toBegin) ? -cur_frame : m_ffrewSkip + m_ffrewAdjust;
    long long target_frame = cur_frame + real_skip;
    bool ret = m_decoder->DoRewind(target_frame, false);
    long long seek_frame  = m_decoder->GetFramesPlayed();
    m_ffrewAdjust = target_frame - seek_frame;
    return ret;
}

bool MythPlayer::DecoderGetFrame(DecodeType decodetype, bool unsafe)
{
    bool ret = false;
    if (!m_videoOutput)
        return false;

    // Wait for frames to be available for decoding onto
    int tries = 0;
    while (!unsafe && (!m_videoOutput->EnoughFreeFrames() || m_audio.IsBufferAlmostFull()))
    {
        if (m_killDecoder || m_pauseDecoder)
            return false;

        if (++tries > 10)
        {
            if (++m_videobufRetries >= 2000)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Decoder timed out waiting for free video buffers.");
                // We've tried for 20 seconds now, give up so that we don't
                // get stuck permanently in this state
                SetErrored("Decoder timed out waiting for free video buffers.");
            }
            return false;
        }

        usleep(1000);
    }
    m_videobufRetries = 0;

    if (!m_decoderChangeLock.tryLock(5))
        return false;
    if (m_killDecoder || !m_decoder || m_pauseDecoder || IsErrored())
    {
        m_decoderChangeLock.unlock();
        return false;
    }

    if (m_ffrewSkip == 1 || m_decodeOneFrame)
        ret = DoGetFrame(decodetype);
    else if (m_ffrewSkip != 0)
        ret = DecoderGetFrameFFREW();
    m_decoderChangeLock.unlock();
    return ret;
}

/*! \brief Get one frame from the decoder.
 *
 * Certain decoders operate asynchronously and will return EAGAIN if a
 * video frame is not yet ready. We handle the retries here in MythPlayer
 * so that we can abort retries if we need to pause or stop the decoder.
 *
 * This is most relevant for MediaCodec decoding when using direct rendering
 * as there are a limited number of decoder output buffers that are retained by
 * the VideoOutput classes (VideoOutput and VideoBuffers) until they have been
 * used.
 *
 * \note The caller must hold m_decoderChangeLock.
 */
bool MythPlayer::DoGetFrame(DecodeType Type)
{
    bool ret = false;
    QElapsedTimer timeout;
    timeout.start();
    bool retry = true;
    // retry for a maximum of 5 seconds
    while (retry && !m_pauseDecoder && !m_killDecoder && !timeout.hasExpired(5000))
    {
        retry = false;
        ret = m_decoder->GetFrame(Type, retry);
        if (retry)
        {
            m_decoderChangeLock.unlock();
            QThread::usleep(10000);
            m_decoderChangeLock.lock();
        }
    }

    if (timeout.hasExpired(5000))
        return false;
    return ret;
}

int64_t MythPlayer::AdjustAudioTimecodeOffset(int64_t v, int newsync)
{
    if ((newsync >= -1000) && (newsync <= 1000))
        m_tcWrap[TC_AUDIO] = newsync;
    else
        m_tcWrap[TC_AUDIO] += v;
    return m_tcWrap[TC_AUDIO];
}

void MythPlayer::WrapTimecode(int64_t &timecode, TCTypes tc_type)
{
    timecode += m_tcWrap[tc_type];
}

bool MythPlayer::PrepareAudioSample(int64_t &timecode)
{
    WrapTimecode(timecode, TC_AUDIO);
    return false;
}

/**
 *  \brief Determines if the recording should be considered watched
 *
 *   By comparing the number of framesPlayed to the total number of
 *   frames in the video minus an offset (14%) we determine if the
 *   recording is likely to have been watched to the end, ignoring
 *   end credits and trailing adverts.
 *
 *   PlaybackInfo::SetWatchedFlag is then called with the argument TRUE
 *   or FALSE accordingly.
 *
 *   \param forceWatched Forces a recording watched ignoring the amount
 *                       actually played (Optional)
 */
void MythPlayer::SetWatched(bool forceWatched)
{
    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    if (!m_playerCtx->m_playingInfo)
    {
        m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
        return;
    }

    uint64_t numFrames = GetCurrentFrameCount();

    // For recordings we want to ignore the post-roll and account for
    // in-progress recordings where totalFrames doesn't represent
    // the full length of the recording. For videos we can only rely on
    // totalFrames as duration metadata can be wrong
    if (m_playerCtx->m_playingInfo->IsRecording() &&
        m_playerCtx->m_playingInfo->QueryTranscodeStatus() !=
        TRANSCODING_COMPLETE)
    {

        // If the recording is stopped early we need to use the recording end
        // time, not the programme end time
        ProgramInfo *pi = m_playerCtx->m_playingInfo;
        qint64 starttime = pi->GetRecordingStartTime().toSecsSinceEpoch();
        qint64 endactual = pi->GetRecordingEndTime().toSecsSinceEpoch();
        qint64 endsched = pi->GetScheduledEndTime().toSecsSinceEpoch();
        qint64 endtime = std::min(endactual, endsched);
        numFrames = (long long) ((endtime - starttime) * m_videoFrameRate);
    }

    int offset = (int) round(0.14 * (numFrames / m_videoFrameRate));

    if (offset < 240)
        offset = 240; // 4 Minutes Min
    else if (offset > 720)
        offset = 720; // 12 Minutes Max

    if (forceWatched || m_framesPlayed > numFrames - (offset * m_videoFrameRate))
    {
        m_playerCtx->m_playingInfo->SaveWatched(true);
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Marking recording as watched using offset %1 minutes")
            .arg(offset/60));
    }

    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
}

void MythPlayer::SetBookmark(bool clear)
{
    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    if (m_playerCtx->m_playingInfo)
        m_playerCtx->m_playingInfo->SaveBookmark(clear ? 0 : m_framesPlayed);
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
}

uint64_t MythPlayer::GetBookmark(void)
{
    uint64_t bookmark = 0;

    if (gCoreContext->IsDatabaseIgnored() ||
        (m_playerCtx->m_buffer && !m_playerCtx->m_buffer->IsBookmarkAllowed()))
        bookmark = 0;
    else
    {
        m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
        if (const ProgramInfo *pi = m_playerCtx->m_playingInfo)
        {
            bookmark = pi->QueryBookmark();
            // Disable progstart if the program has a cutlist.
            if (bookmark == 0 && !pi->HasCutlist())
                bookmark = pi->QueryProgStart();
            if (bookmark == 0)
                bookmark = pi->QueryLastPlayPos();
        }
        m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
    }

    return bookmark;
}

bool MythPlayer::UpdateFFRewSkip(void)
{
    bool skip_changed = false;

    float temp_speed = (m_playSpeed == 0.0F) ?
        m_audio.GetStretchFactor() : m_playSpeed;
    if (m_playSpeed >= 0.0F && m_playSpeed <= 3.0F)
    {
        skip_changed = (m_ffrewSkip != 1);
        if (m_decoder)
            m_fpsMultiplier = m_decoder->GetfpsMultiplier();
        m_frameInterval = (int) (1000000.0 / m_videoFrameRate / static_cast<double>(temp_speed))
            / m_fpsMultiplier;
        m_ffrewSkip = static_cast<int>(m_playSpeed != 0.0F);
    }
    else
    {
        skip_changed = true;
        m_frameInterval = 200000;
        m_frameInterval = (fabs(m_playSpeed) >=   3.0F) ? 133466 : m_frameInterval;
        m_frameInterval = (fabs(m_playSpeed) >=   5.0F) ? 133466 : m_frameInterval;
        m_frameInterval = (fabs(m_playSpeed) >=   8.0F) ? 250250 : m_frameInterval;
        m_frameInterval = (fabs(m_playSpeed) >=  10.0F) ? 133466 : m_frameInterval;
        m_frameInterval = (fabs(m_playSpeed) >=  16.0F) ? 187687 : m_frameInterval;
        m_frameInterval = (fabs(m_playSpeed) >=  20.0F) ? 150150 : m_frameInterval;
        m_frameInterval = (fabs(m_playSpeed) >=  30.0F) ? 133466 : m_frameInterval;
        m_frameInterval = (fabs(m_playSpeed) >=  60.0F) ? 133466 : m_frameInterval;
        m_frameInterval = (fabs(m_playSpeed) >= 120.0F) ? 133466 : m_frameInterval;
        m_frameInterval = (fabs(m_playSpeed) >= 180.0F) ? 133466 : m_frameInterval;
        float ffw_fps = fabs(static_cast<double>(m_playSpeed)) * m_videoFrameRate;
        float dis_fps = 1000000.0F / m_frameInterval;
        m_ffrewSkip = (int)ceil(ffw_fps / dis_fps);
        m_ffrewSkip = m_playSpeed < 0.0F ? -m_ffrewSkip : m_ffrewSkip;
        m_ffrewAdjust = 0;
    }

    return skip_changed;
}

void MythPlayer::ChangeSpeed(void)
{
    float last_speed = m_playSpeed;
    m_playSpeed   = m_nextPlaySpeed;
    m_normalSpeed = m_nextNormalSpeed;
    m_avSync.ResetAVSyncClockBase();

    bool skip_changed = UpdateFFRewSkip();

    if (skip_changed && m_videoOutput)
    {
        m_videoOutput->SetPrebuffering(m_ffrewSkip == 1);
        if (m_playSpeed != 0.0F && !(last_speed == 0.0F && m_ffrewSkip == 1))
            DoJumpToFrame(m_framesPlayed + m_ffTime - m_rewindTime, kInaccuracyFull);
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Play speed: " +
        QString("rate: %1 speed: %2 skip: %3 => new interval %4")
        .arg(m_videoFrameRate).arg(static_cast<double>(m_playSpeed))
        .arg(m_ffrewSkip).arg(m_frameInterval));

    if (m_videoOutput)
        m_videoOutput->SetVideoFrameRate(static_cast<float>(m_videoFrameRate));

    if (m_normalSpeed && m_audio.HasAudioOut())
    {
        m_audio.SetStretchFactor(m_playSpeed);
        syncWithAudioStretch();
    }
}

bool MythPlayer::DoRewind(uint64_t frames, double inaccuracy)
{
    if (m_playerCtx->m_buffer && !m_playerCtx->m_buffer->IsSeekingAllowed())
        return false;

    uint64_t number = frames + 1;
    uint64_t desiredFrame = (m_framesPlayed > number) ? m_framesPlayed - number : 0;

    m_limitKeyRepeat = false;
    if (desiredFrame < m_videoFrameRate)
        m_limitKeyRepeat = true;

    uint64_t seeksnap_wanted = UINT64_MAX;
    if (inaccuracy != kInaccuracyFull)
        seeksnap_wanted = frames * inaccuracy;
    ClearBeforeSeek(frames);
    WaitForSeek(desiredFrame, seeksnap_wanted);
    m_rewindTime = 0;
    ClearAfterSeek();
    return true;
}

bool MythPlayer::DoRewindSecs(float secs, double inaccuracy, bool use_cutlist)
{
    float current = ComputeSecs(m_framesPlayed, use_cutlist);
    float target = current - secs;
    if (target < 0)
        target = 0;
    uint64_t targetFrame = FindFrame(target, use_cutlist);
    return DoRewind(m_framesPlayed - targetFrame, inaccuracy);
}

/**
 * CalcRWTime(rw): rewind rw frames back.
 * Handle livetv transitions if necessary
 *
 */
long long MythPlayer::CalcRWTime(long long rw) const
{
    bool hasliveprev = (m_liveTV && m_playerCtx->m_tvchain &&
                        m_playerCtx->m_tvchain->HasPrev());

    if (!hasliveprev || ((int64_t)m_framesPlayed >= rw))
    {
        return rw;
    }

    m_playerCtx->m_tvchain->JumpToNext(false, ((int64_t)m_framesPlayed - rw) / m_videoFrameRate);

    return -1;
}

/**
 * CalcMaxFFTime(ffframes): forward ffframes forward.
 * Handle livetv transitions if necessay
 */
long long MythPlayer::CalcMaxFFTime(long long ffframes, bool setjump) const
{
    float maxtime = 1.0;
    bool islivetvcur = (m_liveTV && m_playerCtx->m_tvchain &&
                        !m_playerCtx->m_tvchain->HasNext());

    if (m_liveTV || IsWatchingInprogress())
        maxtime = 3.0;

    long long ret       = ffframes;
    float ff            = ComputeSecs(ffframes, true);
    float secsPlayed    = ComputeSecs(m_framesPlayed, true);
    float secsWritten   = ComputeSecs(m_totalFrames, true);

    m_limitKeyRepeat = false;

    if (m_liveTV && !islivetvcur && m_playerCtx->m_tvchain)
    {
        // recording has completed, totalFrames will always be up to date
        if ((ffframes + m_framesPlayed > m_totalFrames) && setjump)
        {
            ret = -1;
            // Number of frames to be skipped is from the end of the current segment
            m_playerCtx->m_tvchain->JumpToNext(true, ((int64_t)m_totalFrames - (int64_t)m_framesPlayed - ffframes) / m_videoFrameRate);
        }
    }
    else if (islivetvcur || IsWatchingInprogress())
    {
        if ((ff + secsPlayed) > secsWritten)
        {
            // If we attempt to seek past the last known duration,
            // check for up to date data
            long long framesWritten = m_playerCtx->m_recorder->GetFramesWritten();

            secsWritten = ComputeSecs(framesWritten, true);
        }

        float behind = secsWritten - secsPlayed;

        if (behind < maxtime) // if we're close, do nothing
            ret = 0;
        else if (behind - ff <= maxtime)
            ret = TranslatePositionMsToFrame(1000 * (secsWritten - maxtime),
                                             true) - m_framesPlayed;

        if (behind < maxtime * 3)
            m_limitKeyRepeat = true;
    }
    else if (IsPaused())
    {
        uint64_t lastFrame =
            m_deleteMap.IsEmpty() ? m_totalFrames : m_deleteMap.GetLastFrame();
        if (m_framesPlayed + ffframes >= lastFrame)
            ret = lastFrame - 1 - m_framesPlayed;
    }
    else
    {
        float secsMax = secsWritten - 2.F * maxtime;
        if (secsMax <= 0.F)
            ret = 0;
        else if (secsMax < secsPlayed + ff)
            ret = TranslatePositionMsToFrame(1000 * secsMax, true)
                    - m_framesPlayed;
    }

    return ret;
}

/** \fn MythPlayer::IsReallyNearEnd(void) const
 *  \brief Returns true iff really near end of recording.
 *
 *   This is used by SwitchToProgram() to determine if we are so
 *   close to the end that we need to switch to the next program.
 */
bool MythPlayer::IsReallyNearEnd(void) const
{
    if (!m_videoOutput || !m_decoder)
        return false;

    return m_playerCtx->m_buffer->IsNearEnd(
        m_decoder->GetFPS(), m_videoOutput->ValidVideoFrames());
}

/** \brief Returns true iff near end of recording.
 */
bool MythPlayer::IsNearEnd(void)
{
    if (!m_playerCtx)
        return false;

    m_playerCtx->LockPlayingInfo(__FILE__, __LINE__);
    if (!m_playerCtx->m_playingInfo || m_playerCtx->m_playingInfo->IsVideo() ||
        !m_decoder)
    {
        m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);
        return false;
    }
    m_playerCtx->UnlockPlayingInfo(__FILE__, __LINE__);

    auto margin = (long long)(m_videoFrameRate * 2);
    margin = (long long) (margin * m_audio.GetStretchFactor());
    bool watchingTV = IsWatchingInprogress();

    uint64_t framesRead = m_framesPlayed;
    uint64_t framesLeft = 0;

    if (m_playerCtx->GetState() == kState_WatchingPreRecorded)
    {
        if (framesRead >= m_deleteMap.GetLastFrame())
            return true;
        uint64_t frameCount = GetCurrentFrameCount();
        framesLeft = (frameCount > framesRead) ? frameCount - framesRead : 0;
        return (framesLeft < (uint64_t)margin);
    }

    if (!m_liveTV && !watchingTV)
        return false;

    if (m_liveTV && m_playerCtx->m_tvchain && m_playerCtx->m_tvchain->HasNext())
        return false;

    if (m_playerCtx->m_recorder)
    {
        framesLeft =
            m_playerCtx->m_recorder->GetCachedFramesWritten() - framesRead;

        // if it looks like we are near end, get an updated GetFramesWritten()
        if (framesLeft < (uint64_t)margin)
            framesLeft = m_playerCtx->m_recorder->GetFramesWritten() - framesRead;
    }

    return (framesLeft < (uint64_t)margin);
}

bool MythPlayer::DoFastForward(uint64_t frames, double inaccuracy)
{
    if (m_playerCtx->m_buffer && !m_playerCtx->m_buffer->IsSeekingAllowed())
        return false;

    uint64_t number = (frames ? frames - 1 : 0);
    uint64_t desiredFrame = m_framesPlayed + number;

    if (!m_deleteMap.IsEditing() && IsInDelete(desiredFrame))
    {
        uint64_t endcheck = m_deleteMap.GetLastFrame();
        if (desiredFrame > endcheck)
            desiredFrame = endcheck;
    }

    uint64_t seeksnap_wanted = UINT64_MAX;
    if (inaccuracy != kInaccuracyFull)
        seeksnap_wanted = frames * inaccuracy;
    ClearBeforeSeek(frames);
    WaitForSeek(desiredFrame, seeksnap_wanted);
    m_ffTime = 0;
    ClearAfterSeek(false);
    return true;
}

bool MythPlayer::DoFastForwardSecs(float secs, double inaccuracy,
                                   bool use_cutlist)
{
    float current = ComputeSecs(m_framesPlayed, use_cutlist);
    float target = current + secs;
    uint64_t targetFrame = FindFrame(target, use_cutlist);
    return DoFastForward(targetFrame - m_framesPlayed, inaccuracy);
}

void MythPlayer::DoJumpToFrame(uint64_t frame, double inaccuracy)
{
    if (frame > m_framesPlayed)
        DoFastForward(frame - m_framesPlayed, inaccuracy);
    else if (frame <= m_framesPlayed)
        DoRewind(m_framesPlayed - frame, inaccuracy);
}

void MythPlayer::WaitForSeek(uint64_t frame, uint64_t seeksnap_wanted)
{
    if (!m_decoder)
        return;

    SetEof(kEofStateNone);
    m_decoder->SetSeekSnap(seeksnap_wanted);

    bool islivetvcur = (m_liveTV && m_playerCtx->m_tvchain &&
                        !m_playerCtx->m_tvchain->HasNext());

    uint64_t max = GetCurrentFrameCount();
    if (islivetvcur || IsWatchingInprogress())
    {
        max = (uint64_t)m_playerCtx->m_recorder->GetFramesWritten();
    }
    if (frame >= max)
        frame = max - 1;

    m_decoderSeekLock.lock();
    m_decoderSeek = frame;
    m_decoderSeekLock.unlock();

    int count = 0;
    bool need_clear = false;
    while (m_decoderSeek >= 0)
    {
        // Waiting blocks the main UI thread but the decoder may
        // have initiated a callback into the UI thread to create
        // certain resources. Ensure the callback is processed.
        // Ideally MythPlayer should be fully event driven and these
        // calls wouldn't be necessary.
        ProcessCallbacks();

        usleep(50 * 1000);

        // provide some on screen feedback if seeking is slow
        count++;
        if (!(count % 3) && !m_hasFullPositionMap)
        {
            int num = count % 3;
            SetOSDMessage(tr("Searching") + QString().fill('.', num),
                          kOSDTimeout_Short);
            DisplayPauseFrame();
            need_clear = true;
        }
    }
    if (need_clear)
    {
        m_osdLock.lock();
        if (m_osd)
            m_osd->HideWindow("osd_message");
        m_osdLock.unlock();
    }
}

/** \fn MythPlayer::ClearAfterSeek(bool)
 *  \brief This is to support seeking...
 *
 *   This resets the output classes and discards all
 *   frames no longer being used by the decoder class.
 *
 *   Note: caller should not hold any locks
 *
 *  \param clearvideobuffers This clears the videooutput buffers as well,
 *                           this is only safe if no old frames are
 *                           required to continue decoding.
 */
void MythPlayer::ClearAfterSeek(bool clearvideobuffers)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("ClearAfterSeek(%1)")
            .arg(clearvideobuffers));

    if (clearvideobuffers && m_videoOutput)
        m_videoOutput->ClearAfterSeek();

    int64_t savedTC = m_tcWrap[TC_AUDIO];

    m_tcWrap.fill(0);
    m_tcWrap[TC_AUDIO] = savedTC;
    m_audio.Reset();

    // Reenable (or re-disable) subtitles, which ultimately does
    // nothing except to call ResetCaptions() to erase any captions
    // currently on-screen.  The key is that the erasing is done in
    // the UI thread, not the decoder thread.
    EnableSubtitles(m_textDesired);
    m_deleteMap.TrackerReset(m_framesPlayed);
    m_commBreakMap.SetTracker(m_framesPlayed);
    m_commBreakMap.ResetLastSkip();
    m_needNewPauseFrame = true;

    m_avSync.InitAVSync();
}

/*! \brief Discard video frames prior to seeking
 * \note This is only used for MediaCodec surface rendering where the decoder will stall
 * waiting for buffers if we do not free those buffers first. This is currently
 * only an issue for recordings and livetv as the decoder is not paused before seeking when
 * using a position map.
 * \note m_watchingRecording does not appear to be accurate - so is currently ignored.
*/
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void MythPlayer::ClearBeforeSeek(uint64_t Frames)
{
#ifdef USING_MEDIACODEC
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("ClearBeforeSeek: decoder %1 frames %2 recording %3 livetv %4")
       .arg(m_codecName).arg(Frames).arg(m_watchingRecording).arg(m_liveTV));

    if ((Frames < 2) || !m_videoOutput /*|| !(m_liveTV || m_watchingRecording)*/)
        return;

    m_decoderChangeLock.lock();
    MythCodecID codec = m_decoder ? m_decoder->GetVideoCodecID() : kCodec_NONE;
    m_decoderChangeLock.unlock();
    if (codec_is_mediacodec(codec))
        m_videoOutput->DiscardFrames(true, true);
#else
    Q_UNUSED(Frames);
#endif
}

bool MythPlayer::IsInDelete(uint64_t frame)
{
    return m_deleteMap.IsInDelete(frame);
}

void MythPlayer::HandleArbSeek(bool right)
{
    if (m_deleteMap.GetSeekAmount() == -2)
    {
        uint64_t framenum = m_deleteMap.GetNearestMark(m_framesPlayed, right);
        if (right && (framenum > m_framesPlayed))
            DoFastForward(framenum - m_framesPlayed, kInaccuracyNone);
        else if (!right && (m_framesPlayed > framenum))
            DoRewind(m_framesPlayed - framenum, kInaccuracyNone);
    }
    else
    {
        if (right)
            DoFastForward(2, kInaccuracyFull);
        else
            DoRewind(2, kInaccuracyFull);
    }
}

AspectOverrideMode MythPlayer::GetAspectOverride(void) const
{
    if (m_videoOutput)
        return m_videoOutput->GetAspectOverride();
    return kAspect_Off;
}

AdjustFillMode MythPlayer::GetAdjustFill(void) const
{
    if (m_videoOutput)
        return m_videoOutput->GetAdjustFill();
    return kAdjustFill_Off;
}

void MythPlayer::ToggleAspectOverride(AspectOverrideMode aspectMode)
{
    if (m_videoOutput)
    {
        m_videoOutput->ToggleAspectOverride(aspectMode);
        ReinitOSD();
    }
}

void MythPlayer::Zoom(ZoomDirection direction)
{
    if (m_videoOutput)
    {
        m_videoOutput->Zoom(direction);
        ReinitOSD();
    }
}

void MythPlayer::ToggleMoveBottomLine(void)
{
    if (m_videoOutput)
    {
        m_videoOutput->ToggleMoveBottomLine();
        ReinitOSD();
    }
}

void MythPlayer::SaveBottomLine(void)
{
    if (m_videoOutput)
        m_videoOutput->SaveBottomLine();
}

bool MythPlayer::HasTVChainNext(void) const
{
    return m_playerCtx->m_tvchain && m_playerCtx->m_tvchain->HasNext();
}

QString MythPlayer::GetEncodingType(void) const
{
    if (m_decoder)
        return get_encoding_type(m_decoder->GetVideoCodecID());
    return QString();
}

bool MythPlayer::GetRawAudioState(void) const
{
    if (m_decoder)
        return m_decoder->GetRawAudioState();
    return false;
}

QString MythPlayer::GetXDS(const QString &key) const
{
    if (!m_decoder)
        return QString();
    return m_decoder->GetXDS(key);
}

void MythPlayer::SetCommBreakMap(frm_dir_map_t &newMap)
{
    m_commBreakMap.SetMap(newMap, m_framesPlayed);
    m_forcePositionMapSync = true;
}

int MythPlayer::GetStatusbarPos(void) const
{
    double spos = 0.0;

    if (m_liveTV || IsWatchingInprogress())
    {
        spos = 1000.0 * m_framesPlayed / m_playerCtx->m_recorder->GetFramesWritten();
    }
    else if (m_totalFrames)
    {
        spos = 1000.0 * m_framesPlayed / m_totalFrames;
    }

    return((int)spos);
}

int64_t MythPlayer::GetSecondsPlayed(bool honorCutList, int divisor)
{
    int64_t pos = TranslatePositionFrameToMs(m_framesPlayed, honorCutList);
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("GetSecondsPlayed: framesPlayed %1, honorCutList %2, divisor %3, pos %4")
        .arg(m_framesPlayed).arg(honorCutList).arg(divisor).arg(pos));
    return TranslatePositionFrameToMs(m_framesPlayed, honorCutList) / divisor;
}

int64_t MythPlayer::GetTotalSeconds(bool honorCutList, int divisor) const
{
    uint64_t pos = m_totalFrames;

    if (IsWatchingInprogress())
        pos = UINT64_MAX;

    return TranslatePositionFrameToMs(pos, honorCutList) / divisor;
}

// Returns the total frame count, as totalFrames for a completed
// recording, or the most recent frame count from the recorder for
// live TV or an in-progress recording.
uint64_t MythPlayer::GetCurrentFrameCount(void) const
{
    uint64_t result = m_totalFrames;
    if (IsWatchingInprogress())
        result = m_playerCtx->m_recorder->GetFramesWritten();
    return result;
}

// Finds the frame number associated with the given time offset.  A
// positive offset or +0.0F indicate offset from the beginning.  A
// negative offset or -0.0F indicate offset from the end.  Limit the
// result to within bounds of the video.
uint64_t MythPlayer::FindFrame(float offset, bool use_cutlist) const
{
    bool islivetvcur    = (m_liveTV && m_playerCtx->m_tvchain &&
                        !m_playerCtx->m_tvchain->HasNext());
    uint64_t length_ms  = TranslatePositionFrameToMs(m_totalFrames, use_cutlist);
    uint64_t position_ms = 0;

    if (signbit(offset))
    {
        // Always get an updated totalFrame value for in progress recordings
        if (islivetvcur || IsWatchingInprogress())
        {
            uint64_t framesWritten = m_playerCtx->m_recorder->GetFramesWritten();

            if (m_totalFrames < framesWritten)
            {
                // Known duration is less than what the backend reported, use new value
                length_ms =
                    TranslatePositionFrameToMs(framesWritten, use_cutlist);
            }
        }
        uint64_t offset_ms = llroundf(-offset * 1000);
        position_ms = (offset_ms > length_ms) ? 0 : length_ms - offset_ms;
    }
    else
    {
        position_ms = llroundf(offset * 1000);

        if (offset > length_ms)
        {
            // Make sure we have an updated totalFrames
            if ((islivetvcur || IsWatchingInprogress()) &&
                (length_ms < offset))
            {
                long long framesWritten =
                    m_playerCtx->m_recorder->GetFramesWritten();

                length_ms =
                    TranslatePositionFrameToMs(framesWritten, use_cutlist);
            }
            position_ms = std::min(position_ms, length_ms);
        }
    }
    return TranslatePositionMsToFrame(position_ms, use_cutlist);
}

void MythPlayer::calcSliderPos(osdInfo &info, bool paddedFields)
{
    if (!m_decoder)
        return;

    bool islive = false;
    info.text.insert("chapteridx",    QString());
    info.text.insert("totalchapters", QString());
    info.text.insert("titleidx",      QString());
    info.text.insert("totaltitles",   QString());
    info.text.insert("angleidx",      QString());
    info.text.insert("totalangles",   QString());
    info.values.insert("position",   0);
    info.values.insert("progbefore", 0);
    info.values.insert("progafter",  0);

    int playbackLen = 0;
    bool fixed_playbacklen = false;

    if (m_liveTV && m_playerCtx->m_tvchain)
    {
        info.values["progbefore"] = (int)m_playerCtx->m_tvchain->HasPrev();
        info.values["progafter"]  = (int)m_playerCtx->m_tvchain->HasNext();
        playbackLen = m_playerCtx->m_tvchain->GetLengthAtCurPos();
        islive = true;
        fixed_playbacklen = true;
    }
    else if (IsWatchingInprogress())
    {
        islive = true;
    }
    else
    {
        int chapter  = GetCurrentChapter();
        int chapters = GetNumChapters();
        if (chapter && chapters > 1)
        {
            info.text["chapteridx"] = QString::number(chapter + 1);
            info.text["totalchapters"] =  QString::number(chapters);
        }

        int title  = GetCurrentTitle();
        int titles = GetNumTitles();
        if (title && titles > 1)
        {
            info.text["titleidx"] = QString::number(title + 1);
            info.text["totaltitles"] = QString::number(titles);
        }

        int angle  = GetCurrentAngle();
        int angles = GetNumAngles();
        if (angle && angles > 1)
        {
            info.text["angleidx"] = QString::number(angle + 1);
            info.text["totalangles"] = QString::number(angles);
        }
    }

    // Set the raw values, followed by the translated values.
    for (int i = 0; i < 2 ; ++i)
    {
        bool honorCutList = (i > 0);
        bool stillFrame = false;
        int  pos = 0;

        QString relPrefix = (honorCutList ? "rel" : "");
        if (!fixed_playbacklen)
            playbackLen = GetTotalSeconds(honorCutList);
        int secsplayed = GetSecondsPlayed(honorCutList);

        stillFrame = (secsplayed < 0);
        playbackLen = std::max(playbackLen, 0);
        secsplayed = std::min(playbackLen, std::max(secsplayed, 0));
        int secsbehind = std::max((playbackLen - secsplayed), 0);

        if (playbackLen > 0)
            pos = (int)(1000.0F * (secsplayed / (float)playbackLen));

        info.values.insert(relPrefix + "secondsplayed", secsplayed);
        info.values.insert(relPrefix + "totalseconds", playbackLen);
        info.values[relPrefix + "position"] = pos;

        QString text1;
        QString text2;
        QString text3;
        if (paddedFields)
        {
            text1 = MythFormatTime(secsplayed, "HH:mm:ss");
            text2 = MythFormatTime(playbackLen, "HH:mm:ss");
            text3 = MythFormatTime(secsbehind, "HH:mm:ss");
        }
        else
        {
            QString fmt = (playbackLen >= ONEHOURINSEC) ? "H:mm:ss" : "m:ss";
            text1 = MythFormatTime(secsplayed, fmt);
            text2 = MythFormatTime(playbackLen, fmt);

            if (secsbehind >= ONEHOURINSEC)
            {
                text3 = MythFormatTime(secsbehind, "H:mm:ss");
            }
            else if (secsbehind >= ONEMININSEC)
            {
                text3 = MythFormatTime(secsbehind, "m:ss");
            }
            else
            {
                text3 = tr("%n second(s)", "", secsbehind);
            }
        }

        QString desc = stillFrame ? tr("Still Frame") :
                                    tr("%1 of %2").arg(text1).arg(text2);

        info.text[relPrefix + "description"] = desc;
        info.text[relPrefix + "playedtime"] = text1;
        info.text[relPrefix + "totaltime"] = text2;
        info.text[relPrefix + "remainingtime"] = islive ? QString() : text3;
        info.text[relPrefix + "behindtime"] = islive ? text3 : QString();
    }
}

// If position == -1, it signifies that we are computing the current
// duration of an in-progress recording.  In this case, we fetch the
// current frame rate and frame count from the recorder.
uint64_t MythPlayer::TranslatePositionFrameToMs(uint64_t position,
                                                bool use_cutlist) const
{
    float frameRate = GetFrameRate();
    if (position == UINT64_MAX &&
        m_playerCtx->m_recorder && m_playerCtx->m_recorder->IsValidRecorder())
    {
        float recorderFrameRate = m_playerCtx->m_recorder->GetFrameRate();
        if (recorderFrameRate > 0)
            frameRate = recorderFrameRate;
        position = m_playerCtx->m_recorder->GetFramesWritten();
    }
    return m_deleteMap.TranslatePositionFrameToMs(position, frameRate,
                                                use_cutlist);
}

int MythPlayer::GetNumChapters()
{
    if (m_decoder)
        return m_decoder->GetNumChapters();
    return 0;
}

int MythPlayer::GetCurrentChapter()
{
    if (m_decoder)
        return m_decoder->GetCurrentChapter(m_framesPlayed);
    return 0;
}

void MythPlayer::GetChapterTimes(QList<long long> &times)
{
    if (m_decoder)
        return m_decoder->GetChapterTimes(times);
}

bool MythPlayer::DoJumpChapter(int chapter)
{
    int64_t desiredFrame = -1;
    int total = GetNumChapters();
    int current = GetCurrentChapter();

    if (chapter < 0 || chapter > total)
    {

        if (chapter < 0)
        {
            chapter = current -1;
            if (chapter < 0) chapter = 0;
        }
        else if (chapter > total)
        {
            chapter = current + 1;
            if (chapter > total) chapter = total;
        }
    }

    desiredFrame = GetChapter(chapter);
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("DoJumpChapter: current %1 want %2 (frame %3)")
            .arg(current).arg(chapter).arg(desiredFrame));

    if (desiredFrame < 0)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("DoJumpChapter failed."));
        m_jumpChapter = 0;
        return false;
    }

    DoJumpToFrame(desiredFrame, kInaccuracyNone);
    m_jumpChapter = 0;
    return true;
}

int64_t MythPlayer::GetChapter(int chapter)
{
    if (m_decoder)
        return m_decoder->GetChapter(chapter);
    return 0;
}

InteractiveTV *MythPlayer::GetInteractiveTV(void)
{
#ifdef USING_MHEG
    if (!m_interactiveTV && m_itvEnabled && !FlagIsSet(kNoITV))
    {
        QMutexLocker lock1(&m_osdLock);
        QMutexLocker lock2(&m_itvLock);
        if (!m_interactiveTV && m_osd)
            m_interactiveTV = new InteractiveTV(this);
    }
#endif // USING_MHEG
    return m_interactiveTV;
}

bool MythPlayer::ITVHandleAction(const QString &action)
{
    bool result = false;

#ifdef USING_MHEG
    if (!GetInteractiveTV())
        return result;

    QMutexLocker locker(&m_itvLock);
    result = m_interactiveTV->OfferKey(action);
#else
    Q_UNUSED(action);
#endif // USING_MHEG

    return result;
}

/** \fn MythPlayer::ITVRestart(uint chanid, uint cardid, bool isLive)
 *  \brief Restart the MHEG/MHP engine.
 */
void MythPlayer::ITVRestart(uint chanid, uint cardid, bool isLiveTV)
{
#ifdef USING_MHEG
    if (!GetInteractiveTV())
        return;

    QMutexLocker locker(&m_itvLock);
    m_interactiveTV->Restart(chanid, cardid, isLiveTV);
    m_itvVisible = false;
#else
    Q_UNUSED(chanid);
    Q_UNUSED(cardid);
    Q_UNUSED(isLiveTV);
#endif // USING_MHEG
}

// Called from the interactiveTV (MHIContext) thread
void MythPlayer::SetVideoResize(const QRect &videoRect)
{
    QMutexLocker locker(&m_osdLock);
    if (m_videoOutput)
        m_videoOutput->SetITVResize(videoRect);
}

/** \fn MythPlayer::SetAudioByComponentTag(int tag)
 *  \brief Selects the audio stream using the DVB component tag.
 */
// Called from the interactiveTV (MHIContext) thread
bool MythPlayer::SetAudioByComponentTag(int tag)
{
    QMutexLocker locker(&m_decoderChangeLock);
    if (m_decoder)
        return m_decoder->SetAudioByComponentTag(tag);
    return false;
}

/** \fn MythPlayer::SetVideoByComponentTag(int tag)
 *  \brief Selects the video stream using the DVB component tag.
 */
// Called from the interactiveTV (MHIContext) thread
bool MythPlayer::SetVideoByComponentTag(int tag)
{
    QMutexLocker locker(&m_decoderChangeLock);
    if (m_decoder)
        return m_decoder->SetVideoByComponentTag(tag);
    return false;
}

static inline double SafeFPS(DecoderBase *m_decoder)
{
    if (!m_decoder)
        return 25;
    double fps = m_decoder->GetFPS();
    return fps > 0 ? fps : 25.0;
}

// Called from the interactiveTV (MHIContext) thread
bool MythPlayer::SetStream(const QString &stream)
{
    // The stream name is empty if the stream is closing
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("SetStream '%1'").arg(stream));

    QMutexLocker locker(&m_streamLock);
    m_newStream = stream;
    locker.unlock();
    // Stream will be changed by JumpToStream called from EventLoop
    // If successful will call m_interactiveTV->StreamStarted();

    if (stream.isEmpty() && m_playerCtx->m_tvchain &&
        m_playerCtx->m_buffer->GetType() == kMythBufferMHEG)
    {
        // Restore livetv
        SetEof(kEofStateDelayed);
        m_playerCtx->m_tvchain->JumpToNext(false, 0);
        m_playerCtx->m_tvchain->JumpToNext(true, 0);
    }

    return !stream.isEmpty();
}

// Called from EventLoop pn the main application thread
void MythPlayer::JumpToStream(const QString &stream)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "JumpToStream - begin");

    if (stream.isEmpty())
        return; // Shouldn't happen

    Pause();
    ResetCaptions();

    ProgramInfo pginfo(stream);
    SetPlayingInfo(pginfo);

    if (m_playerCtx->m_buffer->GetType() != kMythBufferMHEG)
        m_playerCtx->m_buffer = new MythInteractiveBuffer(stream, m_playerCtx->m_buffer);
    else
        m_playerCtx->m_buffer->OpenFile(stream);

    if (!m_playerCtx->m_buffer->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "JumpToStream buffer OpenFile failed");
        SetEof(kEofStateImmediate);
        SetErrored(tr("Error opening remote stream buffer"));
        return;
    }

    m_watchingRecording = false;
    m_totalLength = 0;
    m_totalFrames = 0;
    m_totalDuration = 0;

    if (OpenFile(120) < 0) // 120 retries ~= 60 seconds
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "JumpToStream OpenFile failed.");
        SetEof(kEofStateImmediate);
        SetErrored(tr("Error opening remote stream"));
        return;
    }

    if (m_totalLength == 0)
    {
        long long len = m_playerCtx->m_buffer->GetRealFileSize();
        m_totalLength = (int)(len / ((m_decoder->GetRawBitrate() * 1000) / 8));
        m_totalFrames = (int)(m_totalLength * SafeFPS(m_decoder));
    }
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("JumpToStream length %1 bytes @ %2 Kbps = %3 Secs, %4 frames @ %5 fps")
        .arg(m_playerCtx->m_buffer->GetRealFileSize()).arg(m_decoder->GetRawBitrate())
        .arg(m_totalLength).arg(m_totalFrames).arg(m_decoder->GetFPS()) );

    SetEof(kEofStateNone);

    // the bitrate is reset by m_playerCtx->m_buffer->OpenFile()...
    m_playerCtx->m_buffer->UpdateRawBitrate(m_decoder->GetRawBitrate());
    m_decoder->SetProgramInfo(pginfo);

    Play();
    ChangeSpeed();

    m_playerCtx->SetPlayerChangingBuffers(false);
#ifdef USING_MHEG
    if (m_interactiveTV) m_interactiveTV->StreamStarted();
#endif

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "JumpToStream - end");
}

// Called from the interactiveTV (MHIContext) thread
long MythPlayer::GetStreamPos()
{
    return (long)((1000 * GetFramesPlayed()) / SafeFPS(m_decoder));
}

// Called from the interactiveTV (MHIContext) thread
long MythPlayer::GetStreamMaxPos()
{
    long maxpos = (long)(1000 * (m_totalDuration > 0 ? m_totalDuration : m_totalLength));
    long pos = GetStreamPos();
    return maxpos > pos ? maxpos : pos;
}

// Called from the interactiveTV (MHIContext) thread
long MythPlayer::SetStreamPos(long ms)
{
    auto frameNum = (uint64_t)((ms * SafeFPS(m_decoder)) / 1000);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("SetStreamPos %1 mS = frame %2, now=%3")
        .arg(ms).arg(frameNum).arg(GetFramesPlayed()) );
    JumpToFrame(frameNum);
    return ms;
}

// Called from the interactiveTV (MHIContext) thread
void MythPlayer::StreamPlay(bool play)
{
    if (play)
        Play();
    else
        Pause();
}

/** \fn MythPlayer::SetDecoder(DecoderBase*)
 *  \brief Sets the stream decoder, deleting any existing recorder.
 */
void MythPlayer::SetDecoder(DecoderBase *dec)
{
    m_totalDecoderPause = true;
    PauseDecoder();

    {
        while (!m_decoderChangeLock.tryLock(10))
            LOG(VB_GENERAL, LOG_INFO, LOC + "Waited 10ms for decoder lock");
        delete m_decoder;
        m_decoder = dec;
        m_decoderChangeLock.unlock();
    }
    // reset passthrough override
    m_disablePassthrough = false;
    syncWithAudioStretch();
    m_totalDecoderPause = false;
}

bool MythPlayer::PosMapFromEnc(uint64_t start,
                               frm_pos_map_t &posMap,
                               frm_pos_map_t &durMap)
{
    // Reads only new positionmap entries from encoder
    if (!(m_liveTV || (m_playerCtx->m_recorder &&
                     m_playerCtx->m_recorder->IsValidRecorder())))
        return false;

    // if livetv, and we're not the last entry, don't get it from the encoder
    if (HasTVChainNext())
        return false;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Filling position map from %1 to %2") .arg(start).arg("end"));

    m_playerCtx->m_recorder->FillPositionMap(start, -1, posMap);
    m_playerCtx->m_recorder->FillDurationMap(start, -1, durMap);

    return true;
}

void MythPlayer::SetErrored(const QString &reason)
{
    QMutexLocker locker(&m_errorLock);

    if (m_videoOutput)
        m_errorType |= m_videoOutput->GetError();

    if (m_errorMsg.isEmpty())
    {
        m_errorMsg = reason;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("%1").arg(reason));
    }
}

void MythPlayer::ResetErrored(void)
{
    QMutexLocker locker(&m_errorLock);

    m_errorMsg = QString();
}

bool MythPlayer::IsErrored(void) const
{
    QMutexLocker locker(&m_errorLock);
    return !m_errorMsg.isEmpty();
}

QString MythPlayer::GetError(void) const
{
    QMutexLocker locker(&m_errorLock);
    return m_errorMsg;
}

void MythPlayer::ToggleNightMode(void)
{
    if (!m_videoOutput)
        return;

    if (!(m_videoOutput->GetSupportedPictureAttributes() &
          kPictureAttributeSupported_Brightness))
        return;

    int b = m_videoOutput->GetPictureAttribute(kPictureAttribute_Brightness);
    int c = 0;
    bool has_contrast = (m_videoOutput->GetSupportedPictureAttributes() &
                         kPictureAttributeSupported_Contrast) != 0;
    if (has_contrast)
        c = m_videoOutput->GetPictureAttribute(kPictureAttribute_Contrast);

    int nm = gCoreContext->GetNumSetting("NightModeEnabled", 0);
    QString msg;
    if (!nm)
    {
        msg = tr("Enabled Night Mode");
        b -= kNightModeBrightenssAdjustment;
        c -= kNightModeContrastAdjustment;
    }
    else
    {
        msg = tr("Disabled Night Mode");
        b += kNightModeBrightenssAdjustment;
        c += kNightModeContrastAdjustment;
    }
    b = clamp(b, 0, 100);
    c = clamp(c, 0, 100);

    gCoreContext->SaveSetting("NightModeEnabled", nm ? "0" : "1");
    m_videoOutput->SetPictureAttribute(kPictureAttribute_Brightness, b);
    if (has_contrast)
        m_videoOutput->SetPictureAttribute(kPictureAttribute_Contrast, c);

    SetOSDMessage(msg, kOSDTimeout_Med);
}

void MythPlayer::SetOSDMessage(const QString &msg, OSDTimeout timeout)
{
    QMutexLocker locker(&m_osdLock);
    if (!m_osd)
        return;

    InfoMap info;
    info.insert("message_text", msg);
    m_osd->SetText("osd_message", info, timeout);
}

void MythPlayer::SetOSDStatus(const QString &title, OSDTimeout timeout)
{
    QMutexLocker locker(&m_osdLock);
    if (!m_osd)
        return;

    osdInfo info;
    calcSliderPos(info);
    info.text.insert("title", title);
    m_osd->SetText("osd_status", info.text, timeout);
    m_osd->SetValues("osd_status", info.values, timeout);
}

void MythPlayer::SaveTotalDuration(void)
{
    if (!m_decoder)
        return;

    m_decoder->SaveTotalDuration();
}

void MythPlayer::ResetTotalDuration(void)
{
    if (!m_decoder)
        return;

    m_decoder->ResetTotalDuration();
}

void MythPlayer::SaveTotalFrames(void)
{
    if (!m_decoder)
        return;

    m_decoder->SaveTotalFrames();
}

void MythPlayer::syncWithAudioStretch()
{
    if (m_decoder && m_audio.HasAudioOut())
    {
        float stretch = m_audio.GetStretchFactor();
        m_disablePassthrough |= (stretch < 0.99F) || (stretch > 1.01F);
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Stretch Factor %1, %2 passthru ")
            .arg(m_audio.GetStretchFactor())
            .arg((m_disablePassthrough) ? "disable" : "allow"));
        SetDisablePassThrough(m_disablePassthrough);
    }
}

void MythPlayer::SetDisablePassThrough(bool disabled)
{
    if (m_decoder && m_audio.HasAudioOut())
    {
        m_decoder->SetDisablePassThrough(m_disablePassthrough || disabled);
    }
}

void MythPlayer::ForceSetupAudioStream(void)
{
    if (m_decoder && m_audio.HasAudioOut())
    {
        m_decoder->ForceSetupAudioStream();
    }
}

static unsigned dbg_ident(const MythPlayer *player)
{
    static QMutex   s_dbgLock;
    static unsigned s_dbgNextIdent = 0;
    using DbgMapType = QHash<const MythPlayer*, unsigned>;
    static DbgMapType s_dbgIdent;

    QMutexLocker locker(&s_dbgLock);
    DbgMapType::iterator it = s_dbgIdent.find(player);
    if (it != s_dbgIdent.end())
        return *it;
    return s_dbgIdent[player] = s_dbgNextIdent++;
}
