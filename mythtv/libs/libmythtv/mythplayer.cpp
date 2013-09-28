// -*- Mode: c++ -*-

#undef HAVE_AV_CONFIG_H

// Std C headers
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <ctime>

// POSIX headers
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/time.h>
#include <assert.h>
#include <math.h>

// C++ headers
#include <algorithm>
#include <iostream>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QKeyEvent>
#include <QDir>
#include <QtCore/qnumeric.h>

// MythTV headers
#include "mthread.h"
#include "mythconfig.h"
#include "mythdbcon.h"
#include "dialogbox.h"
#include "mythplayer.h"
#include "DetectLetterbox.h"
#include "audioplayer.h"
#include "recordingprofile.h"
#include "teletextscreen.h"
#include "interactivescreen.h"
#include "remoteutil.h"
#include "programinfo.h"
#include "mythcorecontext.h"
#include "fifowriter.h"
#include "filtermanager.h"
#include "mythdate.h"
#include "livetvchain.h"
#include "decoderbase.h"
#include "nuppeldecoder.h"
#include "avformatdecoder.h"
#include "dummydecoder.h"
#include "jobqueue.h"
#include "NuppelVideoRecorder.h"
#include "tv_play.h"
#include "interactivetv.h"
#include "myth_imgconvert.h"
#include "mythsystemevent.h"
#include "mythpainter.h"
#include "mythimage.h"
#include "mythuiimage.h"
#include "mythlogging.h"
#include "mythmiscutil.h"
#include "icringbuffer.h"
#include "audiooutput.h"

extern "C" {
#include "vbitext/vbi.h"
#include "vsync.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
}

#include "remoteencoder.h"

#include "videoout_null.h"

#if ! HAVE_ROUND
#define round(x) ((int) ((x) + 0.5))
#endif

static unsigned dbg_ident(const MythPlayer*);

#define LOC      QString("Player(%1): ").arg(dbg_ident(this),0,36)
#define LOC_DEC  QString("Player(%1): ").arg(dbg_ident(m_mp),0,36)

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

void DecoderThread::run(void)
{
    RunProlog();
    LOG(VB_PLAYBACK, LOG_INFO, LOC_DEC + "Decoder thread starting.");
    if (m_mp)
        m_mp->DecoderLoop(m_start_paused);
    LOG(VB_PLAYBACK, LOG_INFO, LOC_DEC + "Decoder thread exiting.");
    RunEpilog();
}

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

MythPlayer::MythPlayer(PlayerFlags flags)
    : playerFlags(flags),
      decoder(NULL),                decoder_change_lock(QMutex::Recursive),
      videoOutput(NULL),            player_ctx(NULL),
      decoderThread(NULL),          playerThread(NULL),
      // Window stuff
      parentWidget(NULL), embedding(false), embedRect(QRect()),
      // State
      totalDecoderPause(false), decoderPaused(false),
      inJumpToProgramPause(false),
      pauseDecoder(false), unpauseDecoder(false),
      killdecoder(false),   decoderSeek(-1),     decodeOneFrame(false),
      needNewPauseFrame(false),
      bufferPaused(false),  videoPaused(false),
      allpaused(false),     playing(false),
      m_double_framerate(false),    m_double_process(false),
      m_deint_possible(true),
      livetv(false),
      watchingrecording(false),
      transcoding(false),
      hasFullPositionMap(false),    limitKeyRepeat(false),
      errorMsg(QString::null),      errorType(kError_None),
      // Chapter stuff
      jumpchapter(0),
      // Bookmark stuff
      bookmarkseek(0),
      // Seek
      fftime(0),
      // Playback misc.
      videobuf_retries(0),          framesPlayed(0),
      framesPlayedExtra(0),
      totalFrames(0),               totalLength(0),
      totalDuration(0),
      rewindtime(0),
      // Input Video Attributes
      video_disp_dim(0,0), video_dim(0,0),
      video_frame_rate(29.97f), video_aspect(4.0f / 3.0f),
      forced_video_aspect(-1),
      resetScan(kScan_Ignore), m_scan(kScan_Interlaced),
      m_scan_locked(false), m_scan_tracker(0), m_scan_initialized(false),
      keyframedist(30),
      // Prebuffering
      buffering(false),
      // General Caption/Teletext/Subtitle support
      textDisplayMode(kDisplayNone),
      prevTextDisplayMode(kDisplayNone),
      prevNonzeroTextDisplayMode(kDisplayNone),
      // Support for analog captions and teletext
      vbimode(VBIMode::None),
      ttPageNum(0x888),
      // Support for captions, teletext, etc. decoded by libav
      textDesired(false), enableCaptions(false), disableCaptions(false),
      enableForcedSubtitles(false), disableForcedSubtitles(false),
      allowForcedSubtitles(true),
      // CC608/708
      cc608(this), cc708(this),
      // MHEG/MHI Interactive TV visible in OSD
      itvVisible(false),
      interactiveTV(NULL),
      itvEnabled(false),
      // OSD stuff
      osd(NULL), reinit_osd(false), osdLock(QMutex::Recursive),
      // Audio
      audio(this, (flags & kAudioMuted)),
      // Picture-in-Picture stuff
      pip_active(false),            pip_visible(true),
      // Filters
      videoFiltersForProgram(""),   videoFiltersOverride(""),
      postfilt_width(0),            postfilt_height(0),
      videoFilters(NULL),           FiltMan(new FilterManager()),

      forcePositionMapSync(false),  pausedBeforeEdit(false),
      speedBeforeEdit(1.0f),
      // Playback (output) speed control
      decoder_lock(QMutex::Recursive),
      next_play_speed(1.0f),        next_normal_speed(true),
      play_speed(1.0f),             normal_speed(true),
      frame_interval((int)(1000000.0f / 30)), m_frame_interval(0),
      ffrew_skip(1),ffrew_adjust(0),
      // Audio and video synchronization stuff
      videosync(NULL),              avsync_delay(0),
      avsync_adjustment(0),         avsync_avg(0),
      avsync_predictor(0),          avsync_predictor_enabled(false),
      refreshrate(0),
      lastsync(false),              repeat_delay(0),
      disp_timecode(0),             avsync_audiopaused(false),
      // Time Code stuff
      prevtc(0),                    prevrp(0),
      // LiveTVChain stuff
      m_tv(NULL),                   isDummy(false),
      // Debugging variables
      output_jmeter(new Jitterometer(LOC))
{
    memset(&tc_lastval, 0, sizeof(tc_lastval));
    memset(&tc_wrap,    0, sizeof(tc_wrap));

    playerThread = QThread::currentThread();
    // Playback (output) zoom control
    detect_letter_box = new DetectLetterbox(this);

    vbimode = VBIMode::Parse(gCoreContext->GetSetting("VbiFormat"));

    defaultDisplayAspect =
        gCoreContext->GetFloatSettingOnHost("XineramaMonitorAspectRatio",
                                            gCoreContext->GetHostName(), 1.7777);
    captionsEnabledbyDefault = gCoreContext->GetNumSetting("DefaultCCMode");
    decode_extra_audio = gCoreContext->GetNumSetting("DecodeExtraAudio", 0);
    itvEnabled         = gCoreContext->GetNumSetting("EnableMHEG", 0);
    clearSavedPosition = gCoreContext->GetNumSetting("ClearSavedPosition", 1);
    endExitPrompt      = gCoreContext->GetNumSetting("EndOfRecordingExitPrompt");
    pip_default_loc    = (PIPLocation)gCoreContext->GetNumSetting("PIPLocation", kPIPTopLeft);
    tc_wrap[TC_AUDIO]  = gCoreContext->GetNumSetting("AudioSyncOffset", 0);

    // Get VBI page number
    QString mypage = gCoreContext->GetSetting("VBIpageNr", "888");
    bool valid = false;
    uint tmp = mypage.toInt(&valid, 16);
    ttPageNum = (valid) ? tmp : ttPageNum;
    cc608.SetTTPageNum(ttPageNum);
}

MythPlayer::~MythPlayer(void)
{
    QMutexLocker lk1(&osdLock);
    QMutexLocker lk2(&vidExitLock);
    QMutexLocker lk3(&videofiltersLock);

    if (osd)
    {
        delete osd;
        osd = NULL;
    }

    SetDecoder(NULL);

    if (decoderThread)
    {
        delete decoderThread;
        decoderThread = NULL;
    }

    if (interactiveTV)
    {
        delete interactiveTV;
        interactiveTV = NULL;
    }

    if (FiltMan)
    {
        delete FiltMan;
        FiltMan = NULL;
    }

    if (videoFilters)
    {
        delete videoFilters;
        videoFilters = NULL;
    }

    if (videosync)
    {
        delete videosync;
        videosync = NULL;
    }

    if (videoOutput)
    {
        delete videoOutput;
        videoOutput = NULL;
    }

    if (output_jmeter)
    {
        delete output_jmeter;
        output_jmeter = NULL;
    }

    if (detect_letter_box)
    {
        delete detect_letter_box;
        detect_letter_box = NULL;
    }
}

void MythPlayer::SetWatchingRecording(bool mode)
{
    watchingrecording = mode;
    if (decoder)
        decoder->SetWatchingRecording(mode);
}

bool MythPlayer::IsWatchingInprogress(void) const
{
    return watchingrecording && player_ctx->recorder &&
        player_ctx->recorder->IsValidRecorder();
}

void MythPlayer::PauseBuffer(void)
{
    bufferPauseLock.lock();
    if (player_ctx->buffer)
    {
        player_ctx->buffer->Pause();
        player_ctx->buffer->WaitForPause();
    }
    bufferPaused = true;
    bufferPauseLock.unlock();
}

void MythPlayer::UnpauseBuffer(void)
{
    bufferPauseLock.lock();
    if (player_ctx->buffer)
        player_ctx->buffer->Unpause();
    bufferPaused = false;
    bufferPauseLock.unlock();
}

bool MythPlayer::Pause(void)
{
    if (!pauseLock.tryLock(100))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Waited 100ms to get pause lock.");
        DecoderPauseCheck();
    }
    bool already_paused = allpaused;
    if (already_paused)
    {
        pauseLock.unlock();
        return already_paused;
    }
    next_play_speed   = 0.0;
    next_normal_speed = false;
    PauseVideo();
    audio.Pause(true);
    PauseDecoder();
    PauseBuffer();
    allpaused = decoderPaused && videoPaused && bufferPaused;
    {
        if (FlagIsSet(kVideoIsNull) && decoder)
            decoder->UpdateFramesPlayed();
        else if (videoOutput && !FlagIsSet(kVideoIsNull))
            framesPlayed = videoOutput->GetFramesPlayed() + framesPlayedExtra;
    }
    pauseLock.unlock();
    return already_paused;
}

bool MythPlayer::Play(float speed, bool normal, bool unpauseaudio)
{
    pauseLock.lock();
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Play(%1, normal %2, unpause audio %3)")
            .arg(speed,5,'f',1).arg(normal).arg(unpauseaudio));

    if (deleteMap.IsEditing())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Ignoring Play(), in edit mode.");
        pauseLock.unlock();
        return false;
    }

    UnpauseBuffer();
    UnpauseDecoder();
    if (unpauseaudio)
        audio.Pause(false);
    UnpauseVideo();
    allpaused = false;
    next_play_speed   = speed;
    next_normal_speed = normal;
    pauseLock.unlock();
    return true;
}

void MythPlayer::PauseVideo(void)
{
    videoPauseLock.lock();
    needNewPauseFrame = true;
    videoPaused = true;
    videoPauseLock.unlock();
}

void MythPlayer::UnpauseVideo(void)
{
    videoPauseLock.lock();
    videoPaused = false;
    if (videoOutput)
        videoOutput->ExposeEvent();
    videoPauseLock.unlock();
}

void MythPlayer::SetPlayingInfo(const ProgramInfo &pginfo)
{
    assert(player_ctx);
    if (!player_ctx)
        return;

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    player_ctx->SetPlayingInfo(&pginfo);
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    SetVideoFilters("");
    InitFilters();
}

void MythPlayer::SetPlaying(bool is_playing)
{
    QMutexLocker locker(&playingLock);

    playing = is_playing;

    playingWaitCond.wakeAll();
}

bool MythPlayer::IsPlaying(uint wait_in_msec, bool wait_for) const
{
    QMutexLocker locker(&playingLock);

    if (!wait_in_msec)
        return playing;

    MythTimer t;
    t.start();

    while ((wait_for != playing) && ((uint)t.elapsed() < wait_in_msec))
    {
        playingWaitCond.wait(
            &playingLock, max(0,(int)wait_in_msec - t.elapsed()));
    }

    return playing;
}

bool MythPlayer::InitVideo(void)
{
    if (!player_ctx)
        return false;

    PIPState pipState = player_ctx->GetPIPState();

    if (!decoder)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Cannot create a video renderer without a decoder.");
        return false;
    }

    videoOutput = VideoOutput::Create(
                    decoder->GetCodecDecoderName(),
                    decoder->GetVideoCodecID(),
                    decoder->GetVideoCodecPrivate(),
                    pipState, video_dim, video_disp_dim, video_aspect,
                    parentWidget, embedRect,
                    video_frame_rate, (uint)playerFlags);

    if (!videoOutput)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
                "Couldn't create VideoOutput instance. Exiting..");
        SetErrored(tr("Failed to initialize video output"));
        return false;
    }

    CheckExtraAudioDecode();

    if (embedding && pipState == kPIPOff)
        videoOutput->EmbedInWidget(embedRect);

    if (decoder && decoder->GetVideoInverted())
        videoOutput->SetVideoFlip();

    InitFilters();

    return true;
}

void MythPlayer::CheckExtraAudioDecode(void)
{
    if (FlagIsSet(kVideoIsNull))
        return;

    bool force = false;
    if (videoOutput && videoOutput->NeedExtraAudioDecode())
    {
        LOG(VB_GENERAL, LOG_NOTICE, LOC +
            "Forcing decode extra audio option on (Video method requires it).");
        force = true;
    }

    if (decoder)
        decoder->SetLowBuffers(decode_extra_audio || force);
}

void MythPlayer::ReinitOSD(void)
{
    if (videoOutput && !FlagIsSet(kVideoIsNull))
    {
        osdLock.lock();
        if (!is_current_thread(playerThread))
        {
            reinit_osd = true;
            osdLock.unlock();
            return;
        }
        QRect visible, total;
        float aspect, scaling;
        if (osd)
        {
            osd->SetPainter(videoOutput->GetOSDPainter());
            videoOutput->GetOSDBounds(total, visible, aspect,
                                      scaling, 1.0f);
            int stretch = (int)((aspect * 100) + 0.5f);
            if ((osd->Bounds() != visible) ||
                (osd->GetFontStretch() != stretch))
            {
                uint old = textDisplayMode;
                ToggleCaptions(old);
                osd->Reinit(visible, aspect);
                EnableCaptions(old, false);
            }
        }

#ifdef USING_MHEG
        if (GetInteractiveTV())
        {
            QMutexLocker locker(&itvLock);
            total = videoOutput->GetMHEGBounds();
            interactiveTV->Reinit(total);
            itvVisible = false;
        }
#endif // USING_MHEG
        reinit_osd = false;
        osdLock.unlock();
    }
}

void MythPlayer::ReinitVideo(void)
{
    if (!videoOutput->IsPreferredRenderer(video_disp_dim))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Need to switch video renderer.");
        SetErrored(tr("Need to switch video renderer"));
        errorType |= kError_Switch_Renderer;
        return;
    }

    bool aspect_only = false;
    {
        QMutexLocker locker1(&osdLock);
        QMutexLocker locker2(&vidExitLock);
        QMutexLocker locker3(&videofiltersLock);

        videoOutput->SetVideoFrameRate(video_frame_rate);
        float aspect = (forced_video_aspect > 0) ? forced_video_aspect :
                                               video_aspect;
        if (!videoOutput->InputChanged(video_dim, video_disp_dim, aspect,
                                       decoder->GetVideoCodecID(),
                                       decoder->GetVideoCodecPrivate(),
                                       aspect_only))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Failed to Reinitialize Video. Exiting..");
            SetErrored(tr("Failed to reinitialize video output"));
            return;
        }

        // the display refresh rate may have been changed by VideoOutput
        if (videosync)
        {
            int ri = MythDisplay::GetDisplayInfo(frame_interval).Rate();
            if (ri != videosync->getRefreshInterval())
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC +
                    QString("Refresh rate has changed from %1 to %2")
                    .arg(videosync->getRefreshInterval())
                    .arg(ri));
                videosync->setRefreshInterval(ri);
             }
        }

        if (osd)
            osd->SetPainter(videoOutput->GetOSDPainter());
        ReinitOSD();
    }

    if (!aspect_only)
    {
        CheckExtraAudioDecode();
        ClearAfterSeek();
        InitFilters();
    }

    if (textDisplayMode)
    {
        EnableSubtitles(true);
    }
}

static inline QString toQString(FrameScanType scan) {
    switch (scan) {
        case kScan_Ignore: return QString("Ignore Scan");
        case kScan_Detect: return QString("Detect Scan");
        case kScan_Interlaced:  return QString("Interlaced Scan");
        case kScan_Progressive: return QString("Progressive Scan");
        default: return QString("Unknown Scan");
    }
}

FrameScanType MythPlayer::detectInterlace(FrameScanType newScan,
                                          FrameScanType scan,
                                          float fps, int video_height)
{
    QString dbg = QString("detectInterlace(") + toQString(newScan) +
        QString(", ") + toQString(scan) + QString(", ") +
        QString("%1").arg(fps) + QString(", ") +
        QString("%1").arg(video_height) + QString(") ->");

    if (kScan_Ignore != newScan || kScan_Detect == scan)
    {
        // The scanning mode should be decoded from the stream, but if it
        // isn't, we have to guess.

        scan = kScan_Interlaced; // default to interlaced
        if (720 == video_height) // ATSC 720p
            scan = kScan_Progressive;
        else if (fps > 45) // software deinterlacing
            scan = kScan_Progressive;

        if (kScan_Detect != newScan)
            scan = newScan;
    };

    LOG(VB_PLAYBACK, LOG_INFO, LOC + dbg+toQString(scan));

    return scan;
}

void MythPlayer::SetKeyframeDistance(int keyframedistance)
{
    keyframedist = (keyframedistance > 0) ? keyframedistance : keyframedist;
}

/** \fn MythPlayer::FallbackDeint(void)
 *  \brief Fallback to non-frame-rate-doubling deinterlacing method.
 */
void MythPlayer::FallbackDeint(void)
{
     m_double_framerate = false;
     m_double_process   = false;

     if (videoOutput)
         videoOutput->FallbackDeint();
}

void MythPlayer::AutoDeint(VideoFrame *frame, bool allow_lock)
{
    if (!frame || m_scan_locked)
        return;

    if (frame->interlaced_frame)
    {
        if (m_scan_tracker < 0)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("interlaced frame seen after %1 progressive frames")
                    .arg(abs(m_scan_tracker)));
            m_scan_tracker = 2;
            if (allow_lock)
            {
                LOG(VB_PLAYBACK, LOG_INFO, LOC + "Locking scan to Interlaced.");
                SetScanType(kScan_Interlaced);
                return;
            }
        }
        m_scan_tracker++;
    }
    else
    {
        if (m_scan_tracker > 0)
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                QString("progressive frame seen after %1 interlaced frames")
                    .arg(m_scan_tracker));
            m_scan_tracker = 0;
        }
        m_scan_tracker--;
    }

    if ((m_scan_tracker % 400) == 0)
    {
        QString type = (m_scan_tracker < 0) ? "progressive" : "interlaced";
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("%1 %2 frames seen.")
                .arg(abs(m_scan_tracker)).arg(type));
    }

    int min_count = !allow_lock ? 0 : 2;
    if (abs(m_scan_tracker) <= min_count)
        return;

    SetScanType((m_scan_tracker > min_count) ? kScan_Interlaced : kScan_Progressive);
    m_scan_locked  = false;
}

void MythPlayer::SetScanType(FrameScanType scan)
{
    QMutexLocker locker(&videofiltersLock);

    if (!is_current_thread(playerThread))
    {
        resetScan = scan;
        return;
    }

    if (!videoOutput || !videosync)
        return; // hopefully this will be called again later...

    resetScan = kScan_Ignore;

    if (m_scan_initialized &&
        m_scan == scan &&
        m_frame_interval == frame_interval)
        return;

    m_scan_locked = (scan != kScan_Detect);

    m_scan_initialized = true;
    m_frame_interval = frame_interval;

    bool interlaced = is_interlaced(scan);

    if (interlaced && !m_deint_possible)
    {
        m_scan = scan;
        return;
    }

    if (interlaced)
    {
        m_deint_possible = videoOutput->SetDeinterlacingEnabled(true);
        if (!m_deint_possible)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to enable deinterlacing");
            m_scan = scan;
            return;
        }
        if (videoOutput->NeedsDoubleFramerate())
        {
            m_double_framerate = true;
            if (!CanSupportDoubleRate())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Video sync method can't support double framerate "
                    "(refresh rate too low for 2x deint)");
                FallbackDeint();
            }
        }
        m_double_process = videoOutput->IsExtraProcessingRequired();
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Enabled deinterlacing");
    }
    else
    {
        if (kScan_Progressive == scan)
        {
            m_double_process = false;
            m_double_framerate = false;
            videoOutput->SetDeinterlacingEnabled(false);
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Disabled deinterlacing");
        }
    }

    m_scan = scan;
}

void MythPlayer::SetVideoParams(int width, int height, double fps,
                                FrameScanType scan)
{
    bool paramsChanged = false;

    if (width >= 1 && height >= 1)
    {
        paramsChanged  = true;
        video_dim      = QSize((width + 15) & ~0xf, (height + 15) & ~0xf);
        video_disp_dim = QSize(width, height);
        video_aspect   = (float)width / height;
    }

    if (!qIsNaN(fps) && fps > 0.0f && fps < 121.0f)
    {
        paramsChanged    = true;
        video_frame_rate = fps;
        if (ffrew_skip != 0 && ffrew_skip != 1)
        {
            UpdateFFRewSkip();
            videosync->setFrameInterval(frame_interval);
        }
        else
        {
            float temp_speed = (play_speed == 0.0f) ?
                audio.GetStretchFactor() : play_speed;
            SetFrameInterval(kScan_Progressive,
                             1.0 / (video_frame_rate * temp_speed));
        }
    }

    if (!paramsChanged)
        return;

    if (videoOutput)
        ReinitVideo();

    if (IsErrored())
        return;

    SetScanType(detectInterlace(scan, m_scan, video_frame_rate,
                                video_disp_dim.height()));
    m_scan_locked  = false;
    m_scan_tracker = (m_scan == kScan_Interlaced) ? 2 : 0;
}

void MythPlayer::SetFileLength(int total, int frames)
{
    totalLength = total;
    totalFrames = frames;
}

void MythPlayer::SetDuration(int duration)
{
    totalDuration = duration;
}

void MythPlayer::OpenDummy(void)
{
    isDummy = true;

    if (!videoOutput)
    {
        SetKeyframeDistance(15);
        SetVideoParams(720, 576, 25.00);
    }

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    DummyDecoder *dec = new DummyDecoder(this, *(player_ctx->playingInfo));
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    SetDecoder(dec);
}

void MythPlayer::CreateDecoder(char *testbuf, int testreadsize)
{
    if (NuppelDecoder::CanHandle(testbuf, testreadsize))
        SetDecoder(new NuppelDecoder(this, *player_ctx->playingInfo));
    else if (AvFormatDecoder::CanHandle(testbuf,
                                        player_ctx->buffer->GetFilename(),
                                        testreadsize))
    {
        SetDecoder(new AvFormatDecoder(this, *player_ctx->playingInfo,
                                       playerFlags));
    }
}

int MythPlayer::OpenFile(uint retries)
{
    // Disable hardware acceleration for second PBP
    if (player_ctx && (player_ctx->IsPBP() && !player_ctx->IsPrimaryPBP()) &&
        FlagIsSet(kDecodeAllowGPU))
    {
        playerFlags = (PlayerFlags)(playerFlags - kDecodeAllowGPU);
    }

    isDummy = false;

    if (!player_ctx || !player_ctx->buffer)
        return -1;

    livetv = player_ctx->tvchain && player_ctx->buffer->LiveMode();

    if (player_ctx->tvchain &&
        player_ctx->tvchain->GetCardType(player_ctx->tvchain->GetCurPos()) ==
        "DUMMY")
    {
        OpenDummy();
        return 0;
    }

    player_ctx->buffer->Start();
    /// OSX has a small stack, so we put this buffer on the heap instead.
    char *testbuf = new char[kDecoderProbeBufferSize];
    UnpauseBuffer();

    // delete any pre-existing recorder
    SetDecoder(NULL);
    int testreadsize = 2048;

    MythTimer bigTimer; bigTimer.start();
    int timeout = (retries + 1) * 500;
    while (testreadsize <= kDecoderProbeBufferSize)
    {
        MythTimer peekTimer; peekTimer.start();
        while (player_ctx->buffer->Peek(testbuf, testreadsize) != testreadsize)
        {
            if (peekTimer.elapsed() > 1000 || bigTimer.elapsed() > timeout)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("OpenFile(): Could not read first %1 bytes of '%2'")
                        .arg(testreadsize)
                        .arg(player_ctx->buffer->GetFilename()));
                delete[] testbuf;
                SetErrored(tr("Could not read first %1 bytes").arg(testreadsize));
                return -1;
            }
            LOG(VB_GENERAL, LOG_WARNING, LOC + "OpenFile() waiting on data");
            usleep(50 * 1000);
        }

        player_ctx->LockPlayingInfo(__FILE__, __LINE__);
        CreateDecoder(testbuf, testreadsize);
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        if (decoder || (bigTimer.elapsed() > timeout))
            break;
        testreadsize <<= 1;
    }

    if (!decoder)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Couldn't find an A/V decoder for: '%1'")
                .arg(player_ctx->buffer->GetFilename()));
        SetErrored(tr("Could not find an A/V decoder"));

        delete[] testbuf;
        return -1;
    }
    else if (decoder->IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Could not initialize A/V decoder.");
        SetDecoder(NULL);
        SetErrored(tr("Could not initialize A/V decoder"));

        delete[] testbuf;
        return -1;
    }

    decoder->SetSeekSnap(0);
    decoder->SetLiveTVMode(livetv);
    decoder->SetWatchingRecording(watchingrecording);
    decoder->SetTranscoding(transcoding);
    CheckExtraAudioDecode();

    // Set 'no_video_decode' to true for audio only decodeing
    bool no_video_decode = false;

    // We want to locate decoder for video even if using_null_videoout
    // is true, only disable if no_video_decode is true.
    int ret = decoder->OpenFile(
        player_ctx->buffer, no_video_decode, testbuf, testreadsize);
    delete[] testbuf;

    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't open decoder for: %1")
                .arg(player_ctx->buffer->GetFilename()));
        SetErrored(tr("Could not open decoder"));
        return -1;
    }

    audio.CheckFormat();

    if (ret > 0)
    {
        hasFullPositionMap = true;
        deleteMap.LoadMap();
        deleteMap.TrackerReset(0);
    }

    // Determine the initial bookmark and update it for the cutlist
    bookmarkseek = GetBookmark();
    deleteMap.TrackerReset(bookmarkseek);
    deleteMap.TrackerWantsToJump(bookmarkseek, bookmarkseek);

    if (!gCoreContext->IsDatabaseIgnored() &&
        player_ctx->playingInfo->QueryAutoExpire() == kLiveTVAutoExpire)
    {
        gCoreContext->SaveSetting(
            "DefaultChanid", player_ctx->playingInfo->GetChanID());
    }

    return IsErrored() ? -1 : 0;
}

void MythPlayer::SetFramesPlayed(uint64_t played)
{
    framesPlayed = played;
    framesPlayedExtra = 0;
    if (videoOutput)
        videoOutput->SetFramesPlayed(played);
}

void MythPlayer::SetVideoFilters(const QString &override)
{
    videoFiltersOverride = override;
    videoFiltersOverride.detach();

    videoFiltersForProgram = player_ctx->GetFilters(
                             (FlagIsSet(kVideoIsNull)) ? "onefield" : "");
}

void MythPlayer::InitFilters(void)
{
    QString filters = "";
    if (videoOutput)
        filters = videoOutput->GetFilters();

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("InitFilters() vo '%1' prog '%2' over '%3'")
            .arg(filters).arg(videoFiltersForProgram)
            .arg(videoFiltersOverride));

    if (!videoFiltersForProgram.isEmpty())
    {
        if (videoFiltersForProgram[0] != '+')
        {
            filters = videoFiltersForProgram;
        }
        else
        {
            if ((filters.length() > 1) && (!filters.endsWith(",")))
                filters += ",";
            filters += videoFiltersForProgram.mid(1);
        }
    }

    if (!videoFiltersOverride.isEmpty())
        filters = videoFiltersOverride;

    AvFormatDecoder *afd = dynamic_cast<AvFormatDecoder *>(decoder);
    if (afd && afd->GetVideoInverted() && !filters.contains("vflip"))
        filters += ",vflip";

    filters.detach();

    videofiltersLock.lock();

    if (videoFilters)
    {
        delete videoFilters;
        videoFilters = NULL;
    }

    if (!filters.isEmpty())
    {
        VideoFrameType itmp = FMT_YV12;
        VideoFrameType otmp = FMT_YV12;
        int btmp;
        postfilt_width = video_dim.width();
        postfilt_height = video_dim.height();

        videoFilters = FiltMan->LoadFilters(
            filters, itmp, otmp, postfilt_width, postfilt_height, btmp);
    }

    videofiltersLock.unlock();

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("LoadFilters('%1'..) -> 0x%2")
            .arg(filters).arg((uint64_t)videoFilters,0,16));
}

/** \fn MythPlayer::GetFreeVideoFrames(void)
 *  \brief Returns the number of frames available for decoding onto.
 */
int MythPlayer::GetFreeVideoFrames(void) const
{
    if (videoOutput)
        return videoOutput->FreeVideoFrames();
    return 0;
}

/** \fn MythPlayer::GetNextVideoFrame(bool)
 *  \brief Removes a frame from the available queue for decoding onto.
 *
 *   This places the frame in the limbo queue, from which frames are
 *   removed if they are added to another queue. Normally a frame is
 *   freed from limbo either by a ReleaseNextVideoFrame() or
 *   DiscardVideoFrame() call; but limboed frames are also freed
 *   during a seek reset.
 *
 *  \param allow_unsafe if true then a frame will be taken from the queue
 *         of frames ready for display if we can't find a frame in the
 *         available queue.
 */
VideoFrame *MythPlayer::GetNextVideoFrame(void)
{
    if (videoOutput)
        return videoOutput->GetNextFreeFrame();
    return NULL;
}

/** \fn MythPlayer::ReleaseNextVideoFrame(VideoFrame*, int64_t)
 *  \brief Places frame on the queue of frames ready for display.
 */
void MythPlayer::ReleaseNextVideoFrame(VideoFrame *buffer,
                                       int64_t timecode,
                                       bool wrap)
{
    if (wrap)
        WrapTimecode(timecode, TC_VIDEO);
    buffer->timecode = timecode;

    if (videoOutput)
        videoOutput->ReleaseFrame(buffer);

    detect_letter_box->Detect(buffer);
    if (allpaused)
        CheckAspectRatio(buffer);
}

/** \fn MythPlayer::ClearDummyVideoFrame(VideoFrame*)
 *  \brief Instructs VideoOutput to clear the frame to black.
 */
void MythPlayer::ClearDummyVideoFrame(VideoFrame *frame)
{
    if (videoOutput)
        videoOutput->ClearDummyFrame(frame);
}

/** \fn MythPlayer::DiscardVideoFrame(VideoFrame*)
 *  \brief Places frame in the available frames queue.
 */
void MythPlayer::DiscardVideoFrame(VideoFrame *buffer)
{
    if (videoOutput)
        videoOutput->DiscardFrame(buffer);
}

/** \fn MythPlayer::DiscardVideoFrames(bool)
 *  \brief Places frames in the available frames queue.
 *
 *   If called with 'next_frame_keyframe' set to false then all frames
 *   not in use by the decoder are made available for decoding. Otherwise,
 *   all frames are made available for decoding; this is only safe if
 *   the next frame is a keyframe.
 *
 *  \param next_frame_keyframe if this is true all frames are placed
 *         in the available queue.
 */
void MythPlayer::DiscardVideoFrames(bool next_frame_keyframe)
{
    if (videoOutput)
        videoOutput->DiscardFrames(next_frame_keyframe);
}

void MythPlayer::DrawSlice(VideoFrame *frame, int x, int y, int w, int h)
{
    if (videoOutput)
        videoOutput->DrawSlice(frame, x, y, w, h);
}

void* MythPlayer::GetDecoderContext(unsigned char* buf, uint8_t*& id)
{
    if (videoOutput)
        return videoOutput->GetDecoderContext(buf, id);
    return NULL;
}

bool MythPlayer::HasReachedEof(void) const
{
    EofState eof = GetEof();
    if (eof != kEofStateNone && !allpaused)
        return true;
    if (GetEditMode())
        return false;
    if (livetv)
        return false;
    if (!deleteMap.IsEmpty() && framesPlayed >= deleteMap.GetLastFrame())
        return true;
    return false;
}

VideoFrame *MythPlayer::GetCurrentFrame(int &w, int &h)
{
    w = video_dim.width();
    h = video_dim.height();

    VideoFrame *retval = NULL;

    vidExitLock.lock();
    if (videoOutput)
    {
        retval = videoOutput->GetLastShownFrame();
        videofiltersLock.lock();
        if (videoFilters && player_ctx->IsPIP())
            videoFilters->ProcessFrame(retval);
        videofiltersLock.unlock();
    }

    if (!retval)
        vidExitLock.unlock();

    return retval;
}

void MythPlayer::DeLimboFrame(VideoFrame *frame)
{
    if (videoOutput)
        videoOutput->DeLimboFrame(frame);
}

void MythPlayer::ReleaseCurrentFrame(VideoFrame *frame)
{
    if (frame)
        vidExitLock.unlock();
}

void MythPlayer::EmbedInWidget(QRect rect)
{
    if (videoOutput)
        videoOutput->EmbedInWidget(rect);
    else
    {
        embedRect = rect;
        embedding = true;
    }
}

void MythPlayer::StopEmbedding(void)
{
    if (videoOutput)
    {
        videoOutput->StopEmbedding();
        ReinitOSD();
    }
    else
    {
        embedRect = QRect();
        embedding = false;
    }
}

void MythPlayer::WindowResized(const QSize &new_size)
{
    if (videoOutput)
        videoOutput->WindowResized(new_size);
}

void MythPlayer::EnableTeletext(int page)
{
    QMutexLocker locker(&osdLock);
    if (!osd)
        return;

    osd->EnableTeletext(true, page);
    prevTextDisplayMode = textDisplayMode;
    textDisplayMode = kDisplayTeletextMenu;
}

void MythPlayer::DisableTeletext(void)
{
    QMutexLocker locker(&osdLock);
    if (!osd)
        return;

    osd->EnableTeletext(false, 0);
    textDisplayMode = kDisplayNone;

    /* If subtitles are enabled before the teletext menu was displayed,
       re-enabled them. */
    if (prevTextDisplayMode & kDisplayAllCaptions)
        EnableCaptions(prevTextDisplayMode, false);
}

void MythPlayer::ResetTeletext(void)
{
    QMutexLocker locker(&osdLock);
    if (!osd)
        return;

    osd->TeletextReset();
}

/** \fn MythPlayer::SetTeletextPage(uint)
 *  \brief Set Teletext NUV Caption page
 */
void MythPlayer::SetTeletextPage(uint page)
{
    osdLock.lock();
    DisableCaptions(textDisplayMode);
    ttPageNum = page;
    cc608.SetTTPageNum(ttPageNum);
    textDisplayMode &= ~kDisplayAllCaptions;
    textDisplayMode |= kDisplayNUVTeletextCaptions;
    osdLock.unlock();
}

bool MythPlayer::HandleTeletextAction(const QString &action)
{
    if (!(textDisplayMode & kDisplayTeletextMenu) || !osd)
        return false;

    bool handled = true;

    osdLock.lock();
    if (action == "MENU" || action == ACTION_TOGGLETT || action == "ESCAPE")
        DisableTeletext();
    else if (osd)
        handled = osd->TeletextAction(action);
    osdLock.unlock();

    return handled;
}

void MythPlayer::ResetCaptions(void)
{
    QMutexLocker locker(&osdLock);
    if (!osd)
        return;

    if (((textDisplayMode & kDisplayAVSubtitle)      ||
         (textDisplayMode & kDisplayTextSubtitle)    ||
         (textDisplayMode & kDisplayRawTextSubtitle) ||
         (textDisplayMode & kDisplayDVDButton)       ||
         (textDisplayMode & kDisplayCC608)           ||
         (textDisplayMode & kDisplayCC708)))
    {
        osd->ClearSubtitles();
    }
    else if ((textDisplayMode & kDisplayTeletextCaptions) ||
             (textDisplayMode & kDisplayNUVTeletextCaptions))
    {
        osd->TeletextClear();
    }
}

void MythPlayer::DisableCaptions(uint mode, bool osd_msg)
{
    if (textDisplayMode)
        prevNonzeroTextDisplayMode = textDisplayMode;
    textDisplayMode &= ~mode;
    ResetCaptions();

    QMutexLocker locker(&osdLock);

    QString msg = "";
    if (kDisplayNUVTeletextCaptions & mode)
        msg += tr("TXT CAP");
    if (kDisplayTeletextCaptions & mode)
    {
        msg += decoder->GetTrackDesc(kTrackTypeTeletextCaptions,
                                     GetTrack(kTrackTypeTeletextCaptions));
        DisableTeletext();
    }
    int preserve = textDisplayMode & (kDisplayCC608 | kDisplayTextSubtitle |
                                      kDisplayAVSubtitle | kDisplayCC708 |
                                      kDisplayRawTextSubtitle);
    if ((kDisplayCC608 & mode) || (kDisplayCC708 & mode) ||
        (kDisplayAVSubtitle & mode) || (kDisplayRawTextSubtitle & mode))
    {
        int type = toTrackType(mode);
        msg += decoder->GetTrackDesc(type, GetTrack(type));
        if (osd)
            osd->EnableSubtitles(preserve);
    }
    if (kDisplayTextSubtitle & mode)
    {
        msg += tr("Text subtitles");
        if (osd)
            osd->EnableSubtitles(preserve);
    }
    if (!msg.isEmpty() && osd_msg)
    {
        msg += " " + tr("Off");
        SetOSDMessage(msg, kOSDTimeout_Med);
    }
}

void MythPlayer::EnableCaptions(uint mode, bool osd_msg)
{
    QMutexLocker locker(&osdLock);
    QString msg;
    if ((kDisplayCC608 & mode) || (kDisplayCC708 & mode) ||
        (kDisplayAVSubtitle & mode) || kDisplayRawTextSubtitle & mode)
    {
        int type = toTrackType(mode);
        msg += decoder->GetTrackDesc(type, GetTrack(type));
        if (osd)
            osd->EnableSubtitles(mode);
    }
    if (kDisplayTextSubtitle & mode)
    {
        if (osd)
            osd->EnableSubtitles(kDisplayTextSubtitle);
        msg += tr("Text subtitles");
    }
    if (kDisplayNUVTeletextCaptions & mode)
        msg += tr("TXT %1").arg(ttPageNum, 3, 16);
    if (kDisplayTeletextCaptions & mode)
    {
        msg += decoder->GetTrackDesc(kTrackTypeTeletextCaptions,
                                     GetTrack(kTrackTypeTeletextCaptions));

        int page = decoder->GetTrackLanguageIndex(
            kTrackTypeTeletextCaptions,
            GetTrack(kTrackTypeTeletextCaptions));

        EnableTeletext(page);
        textDisplayMode = kDisplayTeletextCaptions;
    }

    msg += " " + tr("On");

    LOG(VB_PLAYBACK, LOG_INFO, QString("EnableCaptions(%1) msg: %2")
        .arg(mode).arg(msg));

    textDisplayMode = mode;
    if (textDisplayMode)
        prevNonzeroTextDisplayMode = textDisplayMode;
    if (osd_msg)
        SetOSDMessage(msg, kOSDTimeout_Med);
}

bool MythPlayer::ToggleCaptions(void)
{
    SetCaptionsEnabled(!((bool)textDisplayMode));
    return textDisplayMode;
}

bool MythPlayer::ToggleCaptions(uint type)
{
    QMutexLocker locker(&osdLock);
    uint mode = toCaptionType(type);
    uint origMode = textDisplayMode;

    if (textDisplayMode)
        DisableCaptions(textDisplayMode, origMode & mode);
    if (origMode & mode)
        return textDisplayMode;
    if (mode)
        EnableCaptions(mode);
    return textDisplayMode;
}

void MythPlayer::SetCaptionsEnabled(bool enable, bool osd_msg)
{
    QMutexLocker locker(&osdLock);
    enableCaptions = disableCaptions = false;
    uint origMode = textDisplayMode;

    textDesired = enable;

    if (!enable)
    {
        DisableCaptions(origMode, osd_msg);
        return;
    }
    int mode = HasCaptionTrack(prevNonzeroTextDisplayMode) ?
        prevNonzeroTextDisplayMode : NextCaptionTrack(kDisplayNone);
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
        ResetCaptions();
    }
}

bool MythPlayer::GetCaptionsEnabled(void)
{
    return (kDisplayNUVTeletextCaptions == textDisplayMode) ||
           (kDisplayTeletextCaptions    == textDisplayMode) ||
           (kDisplayAVSubtitle          == textDisplayMode) ||
           (kDisplayCC608               == textDisplayMode) ||
           (kDisplayCC708               == textDisplayMode) ||
           (kDisplayTextSubtitle        == textDisplayMode) ||
           (kDisplayRawTextSubtitle     == textDisplayMode) ||
           (kDisplayTeletextMenu        == textDisplayMode);
}

QStringList MythPlayer::GetTracks(uint type)
{
    if (decoder)
        return decoder->GetTracks(type);
    return QStringList();
}

uint MythPlayer::GetTrackCount(uint type)
{
    if (decoder)
        return decoder->GetTrackCount(type);
    return 0;
}

int MythPlayer::SetTrack(uint type, int trackNo)
{
    int ret = -1;
    if (!decoder)
        return ret;

    ret = decoder->SetTrack(type, trackNo);
    if (kTrackTypeAudio == type)
    {
        QString msg = "";
        if (decoder)
            SetOSDMessage(decoder->GetTrackDesc(type, GetTrack(type)),
                          kOSDTimeout_Med);
        return ret;
    }

    uint subtype = toCaptionType(type);
    if (subtype)
    {
        DisableCaptions(textDisplayMode, false);
        EnableCaptions(subtype, true);
        if ((kDisplayCC708 == subtype || kDisplayCC608 == subtype) && decoder)
        {
            int sid = decoder->GetTrackInfo(type, trackNo).stream_id;
            if (sid >= 0)
            {
                (kDisplayCC708 == subtype) ? cc708.SetCurrentService(sid) :
                                             cc608.SetMode(sid);
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
        trackType <= kTrackTypeTeletextCaptions && textDesired)
    {
        enableCaptions = true;
    }
}

void MythPlayer::EnableSubtitles(bool enable)
{
    if (enable)
        enableCaptions = true;
    else
        disableCaptions = true;
}

void MythPlayer::EnableForcedSubtitles(bool enable)
{
    if (enable)
        enableForcedSubtitles = true;
    else
        disableForcedSubtitles = true;
}

void MythPlayer::SetAllowForcedSubtitles(bool allow)
{
    allowForcedSubtitles = allow;
    SetOSDMessage(allowForcedSubtitles ?
                      tr("Forced Subtitles On") :
                      tr("Forced Subtitles Off"),
                  kOSDTimeout_Med);
}

void MythPlayer::DoDisableForcedSubtitles(void)
{
    disableForcedSubtitles = false;
    osdLock.lock();
    if (osd)
        osd->DisableForcedSubtitles();
    osdLock.unlock();
}

void MythPlayer::DoEnableForcedSubtitles(void)
{
    enableForcedSubtitles = false;
    if (!allowForcedSubtitles)
        return;

    osdLock.lock();
    if (osd)
        osd->EnableSubtitles(kDisplayAVSubtitle, true /*forced only*/);
    osdLock.unlock();
}

int MythPlayer::GetTrack(uint type)
{
    if (decoder)
        return decoder->GetTrack(type);
    return -1;
}

int MythPlayer::ChangeTrack(uint type, int dir)
{
    if (!decoder)
        return -1;

    int retval = decoder->ChangeTrack(type, dir);
    if (retval >= 0)
    {
        SetOSDMessage(decoder->GetTrackDesc(type, GetTrack(type)),
                      kOSDTimeout_Med);
        return retval;
    }
    return -1;
}

void MythPlayer::ChangeCaptionTrack(int dir)
{
    if (!decoder || (dir < 0))
        return;

    if (!((textDisplayMode == kDisplayTextSubtitle) ||
          (textDisplayMode == kDisplayNUVTeletextCaptions) ||
          (textDisplayMode == kDisplayNone)))
    {
        int tracktype = toTrackType(textDisplayMode);
        if (GetTrack(tracktype) < decoder->NextTrack(tracktype))
        {
            SetTrack(tracktype, decoder->NextTrack(tracktype));
            return;
        }
    }
    int nextmode = NextCaptionTrack(textDisplayMode);
    if ((nextmode == kDisplayTextSubtitle) ||
        (nextmode == kDisplayNUVTeletextCaptions) ||
        (nextmode == kDisplayNone))
    {
        DisableCaptions(textDisplayMode, true);
        if (nextmode != kDisplayNone)
            EnableCaptions(nextmode, true);
    }
    else
    {
        int tracktype = toTrackType(nextmode);
        int tracks = decoder->GetTrackCount(tracktype);
        if (tracks)
        {
            DisableCaptions(textDisplayMode, true);
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
    else if (!(mode == kDisplayTextSubtitle) &&
               decoder->GetTrackCount(toTrackType(mode)))
    {
        return true;
    }
    return false;
}

int MythPlayer::NextCaptionTrack(int mode)
{
    // Text->TextStream->708->608->AVSubs->Teletext->NUV->None
    // NUV only offerred if PAL
    bool pal      = (vbimode == VBIMode::PAL_TT);
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
    frame_interval = (int)(1000000.0f * frame_period + 0.5f);
    if (!avsync_predictor_enabled)
        avsync_predictor = 0;
    avsync_predictor_enabled = false;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("SetFrameInterval ps:%1 scan:%2")
            .arg(play_speed).arg(scan));
    if (play_speed < 1 || play_speed > 2 || refreshrate <= 0)
        return;

    avsync_predictor_enabled = ((frame_interval-(frame_interval/200)) <
                                refreshrate);
}

void MythPlayer::ResetAVSync(void)
{
    avsync_avg = 0;
    if (!avsync_predictor_enabled || avsync_predictor >= refreshrate)
        avsync_predictor = 0;
    prevtc = 0;
    LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC + "A/V sync reset");
}

void MythPlayer::InitAVSync(void)
{
    videosync->Start();

    avsync_adjustment = 0;

    repeat_delay = 0;

    refreshrate = MythDisplay::GetDisplayInfo(frame_interval).Rate();

    if (!FlagIsSet(kVideoIsNull))
    {
        QString timing_type = videosync->getName();

        QString msg = QString("Video timing method: %1").arg(timing_type);
        LOG(VB_GENERAL, LOG_INFO, LOC + msg);
        msg = QString("Display Refresh Rate: %1 Video Frame Rate: %2")
                       .arg(1000000.0 / refreshrate, 0, 'f', 3)
                       .arg(1000000.0 / frame_interval, 0, 'f', 3);
        LOG(VB_PLAYBACK, LOG_INFO, LOC + msg);

        SetFrameInterval(m_scan, 1.0 / (video_frame_rate * play_speed));

        // try to get preferential scheduling, but ignore if we fail to.
        myth_nice(-19);
    }
}

int64_t MythPlayer::AVSyncGetAudiotime(void)
{
    int64_t currentaudiotime = 0;
    if (normal_speed)
    {
        currentaudiotime = audio.GetAudioTime();
    }
    return currentaudiotime;
}

#define MAXDIVERGE  3.0f
#define DIVERGELIMIT 30.0f
void MythPlayer::AVSync(VideoFrame *buffer, bool limit_delay)
{
    int repeat_pict  = 0;
    int64_t timecode = audio.GetAudioTime();

    if (buffer)
    {
        repeat_pict   = buffer->repeat_pict;
        timecode      = buffer->timecode;
        disp_timecode = buffer->disp_timecode;
    }

    float diverge = 0.0f;
    int frameDelay = m_double_framerate ? frame_interval / 2 : frame_interval;
    int vsync_delay_clock = 0;
    //int64_t currentaudiotime = 0;

    if (videoOutput->IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "AVSync: Unknown error in videoOutput, aborting playback.");
        SetErrored(tr("Failed to initialize A/V Sync"));
        return;
    }

    if (normal_speed)
    {
        diverge = (float)avsync_avg / (float)frame_interval;
        diverge = max(diverge, -DIVERGELIMIT);
        diverge = min(diverge, +DIVERGELIMIT);
    }

    FrameScanType ps = m_scan;
    if (kScan_Detect == m_scan || kScan_Ignore == m_scan)
        ps = kScan_Progressive;

    bool max_video_behind = diverge < -MAXDIVERGE;
    bool dropframe = false;
    QString dbg;

    if (avsync_predictor_enabled)
    {
        avsync_predictor += frame_interval;
        if (avsync_predictor >= refreshrate)
        {
            int refreshperiodsinframe = avsync_predictor/refreshrate;
            avsync_predictor -= refreshrate * refreshperiodsinframe;
        }
        else
        {
            dropframe = true;
            dbg = "A/V predict drop frame, ";
        }
    }

    if (max_video_behind)
    {
        dropframe = true;
        // If video is way behind of audio, adjust for it...
        dbg = QString("Video is %1 frames behind audio (too slow), ")
            .arg(-diverge);
    }

    if (!dropframe && avsync_audiopaused)
    {
        avsync_audiopaused = false;
        audio.Pause(false);
    }

    if (dropframe)
    {
        // Reset A/V Sync
        lastsync = true;
        //currentaudiotime = AVSyncGetAudiotime();
        LOG(VB_PLAYBACK, LOG_INFO, LOC + dbg + "dropping frame to catch up.");
        if (!audio.IsPaused() && max_video_behind)
        {
            audio.Pause(true);
            avsync_audiopaused = true;
        }
    }
    else if (!FlagIsSet(kVideoIsNull))
    {
        // if we get here, we're actually going to do video output
        osdLock.lock();
        videoOutput->PrepareFrame(buffer, ps, osd);
        osdLock.unlock();
        LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO,
            LOC + QString("AVSync waitforframe %1 %2")
                .arg(avsync_adjustment).arg(m_double_framerate));
        vsync_delay_clock = videosync->WaitForFrame
                            (frameDelay + avsync_adjustment + repeat_delay);
        //currentaudiotime = AVSyncGetAudiotime();
        LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC + "AVSync show");
        videoOutput->Show(ps);

        if (videoOutput->IsErrored())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Error condition detected "
                    "in videoOutput after Show(), aborting playback.");
            SetErrored(tr("Serious error detected in Video Output"));
            return;
        }

        if (m_double_framerate)
        {
            //second stage of deinterlacer processing
            ps = (kScan_Intr2ndField == ps) ?
                kScan_Interlaced : kScan_Intr2ndField;
            osdLock.lock();
            if (m_double_process && ps != kScan_Progressive)
            {
                videofiltersLock.lock();
                videoOutput->ProcessFrame(
                        buffer, osd, videoFilters, pip_players, ps);
                videofiltersLock.unlock();
            }

            videoOutput->PrepareFrame(buffer, ps, osd);
            osdLock.unlock();
            // Display the second field
            vsync_delay_clock = videosync->WaitForFrame(frameDelay +
                                                        avsync_adjustment);
            videoOutput->Show(ps);
        }

        repeat_delay = frame_interval * repeat_pict * 0.5;

        if (repeat_delay)
            LOG(VB_TIMESTAMP, LOG_INFO, LOC +
                QString("A/V repeat_pict, adding %1 repeat delay")
                    .arg(repeat_delay));
    }
    else
    {
        vsync_delay_clock = videosync->WaitForFrame(frameDelay);
        //currentaudiotime = AVSyncGetAudiotime();
    }

    if (output_jmeter && output_jmeter->RecordCycleTime())
    {
        LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
            QString("A/V avsync_delay: %1, avsync_avg: %2")
                .arg(avsync_delay / 1000).arg(avsync_avg / 1000));
    }

    avsync_adjustment = 0;

    if (diverge > MAXDIVERGE)
    {
        // If audio is way behind of video, adjust for it...
        // by cutting the frame rate in half for the length of this frame
        avsync_adjustment = frame_interval;
        lastsync = true;
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Video is %1 frames ahead of audio,\n"
                    "\t\t\tdoubling video frame interval to slow down.")
                .arg(diverge));
    }

    if (audio.HasAudioOut() && normal_speed)
    {
        // must be sampled here due to Show delays
        int64_t currentaudiotime = audio.GetAudioTime();
        LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
            QString("A/V timecodes audio %1 video %2 frameinterval %3 "
                    "avdel %4 avg %5 tcoffset %6 avp %7 avpen %8 avdc %9")
                .arg(currentaudiotime)
                .arg(timecode)
                .arg(frame_interval)
                .arg(timecode - currentaudiotime -
                     (int)(vsync_delay_clock*audio.GetStretchFactor()+500)/1000)
                .arg(avsync_avg)
                .arg(tc_wrap[TC_AUDIO])
                .arg(avsync_predictor)
                .arg(avsync_predictor_enabled)
                .arg(vsync_delay_clock)
                 );
        if (currentaudiotime != 0 && timecode != 0)
        { // currentaudiotime == 0 after a seek
            // The time at the start of this frame (ie, now) is given by
            // last->timecode
            if (prevtc != 0)
            {
                int delta = (int)((timecode - prevtc)/play_speed) -
                                  (frame_interval / 1000);
                // If timecode is off by a frame (dropped frame) wait to sync
                if (delta > (int) frame_interval / 1200 &&
                    delta < (int) frame_interval / 1000 * 3 &&
                    prevrp == 0)
                {
                    // wait an extra frame interval
                    LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
                        QString("A/V delay %1").arg(delta));
                    avsync_adjustment += frame_interval;
                    // If we're duplicating a frame, it may be because
                    // the container frame rate doesn't match the
                    // stream frame rate.  In this case, we increment
                    // the fake frame counter so that avformat
                    // timestamp-based seeking will work.
                    if (!decoder->HasPositionMap())
                        ++framesPlayedExtra;
                }
            }
            prevtc = timecode;
            prevrp = repeat_pict;

            // usec
            avsync_delay = (timecode - currentaudiotime) * 1000 -
                            (int)(vsync_delay_clock*audio.GetStretchFactor());

            // prevents major jitter when pts resets during dvd title
            if (avsync_delay > 2000000 && limit_delay)
                avsync_delay = 90000;
            avsync_avg = (avsync_delay + (avsync_avg * 3)) / 4;

            int avsync_used = avsync_avg;
            if (labs(avsync_used) > labs(avsync_delay))
                avsync_used = avsync_delay;

            /* If the audio time codes and video diverge, shift
               the video by one interlaced field (1/2 frame) */
            if (!lastsync)
            {
                if (avsync_used > refreshrate)
                {
                    avsync_adjustment += refreshrate;
                }
                else if (avsync_used < 0 - refreshrate)
                {
                    avsync_adjustment -= refreshrate;
                }
            }
            else
                lastsync = false;
        }
        else
        {
            ResetAVSync();
        }
    }
    else
    {
        LOG(VB_PLAYBACK | VB_TIMESTAMP, LOG_INFO, LOC +
            QString("A/V no sync proc ns:%1").arg(normal_speed));
    }
}

void MythPlayer::RefreshPauseFrame(void)
{
    if (needNewPauseFrame)
    {
        if (videoOutput->ValidVideoFrames())
        {
            videoOutput->UpdatePauseFrame(disp_timecode);
            needNewPauseFrame = false;
        }
        else
        {
            decodeOneFrame = true;
        }
    }
}

void MythPlayer::DisplayPauseFrame(void)
{
    if (!videoOutput || ! videosync)
        return;

    if (videoOutput->IsErrored())
    {
        SetErrored(tr("Serious error detected in Video Output"));
        return;
    }

    // clear the buffering state
    SetBuffering(false);

    RefreshPauseFrame();
    PreProcessNormalFrame(); // Allow interactiveTV to draw on pause frame

    osdLock.lock();
    videofiltersLock.lock();
    videoOutput->ProcessFrame(NULL, osd, videoFilters, pip_players);
    videofiltersLock.unlock();
    videoOutput->PrepareFrame(NULL, kScan_Ignore, osd);
    osdLock.unlock();
    videoOutput->Show(kScan_Ignore);
    videosync->Start();
}

void MythPlayer::SetBuffering(bool new_buffering)
{
    if (!buffering && new_buffering)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Waiting for video buffers...");
        buffering = true;
        buffering_start = QTime::currentTime();
        buffering_last_msg = QTime::currentTime();
    }
    else if (buffering && !new_buffering)
    {
        buffering = false;
    }
}

bool MythPlayer::PrebufferEnoughFrames(int min_buffers)
{
    if (!videoOutput)
        return false;

    if (!(min_buffers ? (videoOutput->ValidVideoFrames() >= min_buffers) :
                        (GetEof() != kEofStateNone) ||
                        (videoOutput->hasHWAcceleration() ?
                            videoOutput->EnoughPrebufferedFrames() :
                            videoOutput->EnoughDecodedFrames())))
    {
        SetBuffering(true);
        usleep(frame_interval >> 3);
        int waited_for = buffering_start.msecsTo(QTime::currentTime());
        int last_msg = buffering_last_msg.msecsTo(QTime::currentTime());
        if (last_msg > 100)
        {
            LOG(VB_GENERAL, LOG_NOTICE, LOC +
                QString("Waited %1ms for video buffers %2")
                    .arg(waited_for).arg(videoOutput->GetFrameStatus()));
            buffering_last_msg = QTime::currentTime();
            if (audio.IsBufferAlmostFull())
            {
                // We are likely to enter this condition
                // if the audio buffer was too full during GetFrame in AVFD
                LOG(VB_AUDIO, LOG_INFO, LOC + "Resetting audio buffer");
                audio.Reset();
            }
        }
        if ((waited_for > 500) && !videoOutput->EnoughFreeFrames())
        {
            LOG(VB_GENERAL, LOG_NOTICE, LOC +
                "Timed out waiting for frames, and"
                "\n\t\t\tthere are not enough free frames. "
                "Discarding buffered frames.");
            // This call will result in some ugly frames, but allows us
            // to recover from serious problems if frames get leaked.
            DiscardVideoFrames(true);
        }
        if (waited_for > 20000) // 20 seconds
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Waited too long for decoder to fill video buffers. Exiting..");
            SetErrored(tr("Video frame buffering failed too many times."));
        }
        if (normal_speed)
            videosync->Start();
        return false;
    }

    SetBuffering(false);
    return true;
}

void MythPlayer::CheckAspectRatio(VideoFrame* frame)
{
    if (!frame)
        return;

    if (!qFuzzyCompare(frame->aspect, video_aspect) && frame->aspect > 0.0f)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Video Aspect ratio changed from %1 to %2")
            .arg(video_aspect).arg(frame->aspect));
        video_aspect = frame->aspect;
        if (videoOutput)
        {
            videoOutput->VideoAspectRatioChanged(video_aspect);
            ReinitOSD();
        }
    }
}

void MythPlayer::DisplayNormalFrame(bool check_prebuffer)
{
    if (allpaused || (check_prebuffer && !PrebufferEnoughFrames()))
        return;

    // clear the buffering state
    SetBuffering(false);

    // retrieve the next frame
    videoOutput->StartDisplayingFrame();
    VideoFrame *frame = videoOutput->GetLastShownFrame();

    // Check aspect ratio
    CheckAspectRatio(frame);

    // Player specific processing (dvd, bd, mheg etc)
    PreProcessNormalFrame();

    // handle scan type changes
    AutoDeint(frame);
    detect_letter_box->SwitchTo(frame);

    FrameScanType ps = m_scan;
    if (kScan_Detect == m_scan || kScan_Ignore == m_scan)
        ps = kScan_Progressive;

    osdLock.lock();
    videofiltersLock.lock();
    videoOutput->ProcessFrame(frame, osd, videoFilters, pip_players, ps);
    videofiltersLock.unlock();
    osdLock.unlock();

    AVSync(frame, 0);
    videoOutput->DoneDisplayingFrame(frame);
}

void MythPlayer::PreProcessNormalFrame(void)
{
#ifdef USING_MHEG
    // handle Interactive TV
    if (GetInteractiveTV())
    {
        osdLock.lock();
        itvLock.lock();
        if (osd && videoOutput->GetOSDPainter())
        {
            InteractiveScreen *window =
                (InteractiveScreen*)osd->GetWindow(OSD_WIN_INTERACT);
            if ((interactiveTV->ImageHasChanged() || !itvVisible) && window)
            {
                interactiveTV->UpdateOSD(window, videoOutput->GetOSDPainter());
                itvVisible = true;
            }
        }
        itvLock.unlock();
        osdLock.unlock();
    }
#endif // USING_MHEG
}

bool MythPlayer::CanSupportDoubleRate(void)
{
    if (!videosync)
        return false;
    return (frame_interval / 2 > videosync->getRefreshInterval() * 0.995);
}

void MythPlayer::EnableFrameRateMonitor(bool enable)
{
    if (!output_jmeter)
        return;
    int rate = enable ? video_frame_rate :
               VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_ANY) ?
               (video_frame_rate * 4) : 0;
    output_jmeter->SetNumCycles(rate);
}

void MythPlayer::ForceDeinterlacer(const QString &override)
{
    if (!videoOutput)
        return;

    bool normal = play_speed > 0.99f && play_speed < 1.01f && normal_speed;
    videofiltersLock.lock();

    m_double_framerate =
         videoOutput->SetupDeinterlace(true, override) &&
         videoOutput->NeedsDoubleFramerate();
    m_double_process = videoOutput->IsExtraProcessingRequired();

    if ((m_double_framerate && !CanSupportDoubleRate()) || !normal)
        FallbackDeint();

    videofiltersLock.unlock();
}

void MythPlayer::VideoStart(void)
{
    if (!FlagIsSet(kVideoIsNull) && !player_ctx->IsPIP())
    {
        QRect visible, total;
        float aspect, scaling;

        osdLock.lock();
        osd = new OSD(this, m_tv, videoOutput->GetOSDPainter());

        videoOutput->GetOSDBounds(total, visible, aspect, scaling, 1.0f);
        osd->Init(visible, aspect);
        videoOutput->InitOSD(osd);
        osd->EnableSubtitles(kDisplayNone);

#ifdef USING_MHEG
        if (GetInteractiveTV())
        {
            QMutexLocker locker(&itvLock);
            total = videoOutput->GetMHEGBounds();
            interactiveTV->Reinit(total);
        }
#endif // USING_MHEG

        // If there is a forced text subtitle track (which is possible
        // in e.g. a .mkv container), and forced subtitles are
        // allowed, then start playback with that subtitle track
        // selected.  Otherwise, use the frontend settings to decide
        // which captions/subtitles (if any) to enable at startup.
        // TODO: modify the fix to #10735 to use this approach
        // instead.
        bool hasForcedTextTrack = false;
        uint forcedTrackNumber = 0;
        if (GetAllowForcedSubtitles())
        {
            uint numTextTracks = decoder->GetTrackCount(kTrackTypeRawText);
            for (uint i = 0; !hasForcedTextTrack && i < numTextTracks; ++i)
            {
                if (decoder->GetTrackInfo(kTrackTypeRawText, i).forced)
                {
                    hasForcedTextTrack = true;
                    forcedTrackNumber = i;
                }
            }
        }
        if (hasForcedTextTrack)
            SetTrack(kTrackTypeRawText, forcedTrackNumber);
        else
            SetCaptionsEnabled(captionsEnabledbyDefault, false);

        osdLock.unlock();
    }

    SetPlaying(true);
    ClearAfterSeek(false);

    avsync_delay = 0;
    avsync_avg = 0;
    refreshrate = 0;
    lastsync = false;

    EnableFrameRateMonitor();
    refreshrate = frame_interval;

    float temp_speed = (play_speed == 0.0) ? audio.GetStretchFactor() : play_speed;
    int fr_int = (1000000.0 / video_frame_rate / temp_speed);
    int rf_int = MythDisplay::GetDisplayInfo(fr_int).Rate();

    // Default to Interlaced playback to allocate the deinterlacer structures
    // Enable autodetection of interlaced/progressive from video stream
    // And initialoze m_scan_tracker to 2 which will immediately switch to
    // progressive if the first frame is progressive in AutoDeint().
    m_scan             = kScan_Interlaced;
    m_scan_locked      = false;
    m_double_framerate = false;
    m_scan_tracker     = 2;

    if (player_ctx->IsPIP() && FlagIsSet(kVideoIsNull))
    {
        videosync = new DummyVideoSync(videoOutput, fr_int, 0, false);
    }
    else if (FlagIsSet(kVideoIsNull))
    {
        videosync = new USleepVideoSync(videoOutput, fr_int, 0, false);
    }
    else if (videoOutput)
    {
        // Set up deinterlacing in the video output method
        m_double_framerate =
            (videoOutput->SetupDeinterlace(true) &&
             videoOutput->NeedsDoubleFramerate());

        m_double_process = videoOutput->IsExtraProcessingRequired();

        videosync = VideoSync::BestMethod(
            videoOutput, (uint)fr_int, (uint)rf_int, m_double_framerate);

        // Make sure video sync can do it
        if (videosync != NULL && m_double_framerate)
        {
            if (!CanSupportDoubleRate())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Video sync method can't support double framerate "
                    "(refresh rate too low for 2x deint)");
                FallbackDeint();
            }
        }
    }
    if (!videosync)
    {
        videosync = new BusyWaitVideoSync(
            videoOutput, fr_int, rf_int, m_double_framerate);
    }

    InitAVSync();
    videosync->Start();
}

bool MythPlayer::VideoLoop(void)
{
    if (videoPaused || isDummy)
    {
        usleep(frame_interval);
        DisplayPauseFrame();
    }
    else
        DisplayNormalFrame();

    if (FlagIsSet(kVideoIsNull) && decoder)
        decoder->UpdateFramesPlayed();
    else if (decoder && decoder->GetEof() != kEofStateNone)
        ++framesPlayed;
    else
        framesPlayed = videoOutput->GetFramesPlayed() + framesPlayedExtra;
    return !IsErrored();
}

void MythPlayer::VideoEnd(void)
{
    osdLock.lock();
    vidExitLock.lock();
    delete osd;
    delete videosync;
    delete videoOutput;
    osd         = NULL;
    videosync   = NULL;
    videoOutput = NULL;
    vidExitLock.unlock();
    osdLock.unlock();
}

bool MythPlayer::FastForward(float seconds)
{
    if (!videoOutput)
        return false;

    if (fftime <= 0)
    {
        float current = ComputeSecs(framesPlayed, true);
        float dest = current + seconds;
        uint64_t target = FindFrame(dest, true);
        fftime = target - framesPlayed;
    }
    return fftime > CalcMaxFFTime(fftime, false);
}

bool MythPlayer::Rewind(float seconds)
{
    if (!videoOutput)
        return false;

    if (rewindtime <= 0)
    {
        float current = ComputeSecs(framesPlayed, true);
        float dest = current - seconds;
        if (dest < 0)
        {
            if (CalcRWTime(framesPlayed + 1) < 0)
                return true;
            dest = 0;
        }
        uint64_t target = FindFrame(dest, true);
        rewindtime = framesPlayed - target;
    }
    return (uint64_t)rewindtime >= framesPlayed;
}

bool MythPlayer::JumpToFrame(uint64_t frame)
{
    if (!videoOutput)
        return false;

    bool ret = false;
    fftime = rewindtime = 0;
    if (frame > framesPlayed)
    {
        fftime = frame - framesPlayed;
        ret = fftime > CalcMaxFFTime(fftime, false);
    }
    else if (frame < framesPlayed)
    {
        rewindtime = framesPlayed - frame;
        ret = fftime > CalcMaxFFTime(fftime, false);
    }
    return ret;
}


void MythPlayer::JumpChapter(int chapter)
{
    if (jumpchapter == 0)
        jumpchapter = chapter;
}

void MythPlayer::ResetPlaying(bool resetframes)
{
    ClearAfterSeek();
    ffrew_skip = 1;
    if (resetframes)
        framesPlayed = framesPlayedExtra = 0;
    if (decoder)
    {
        decoder->Reset(true, true, true);
        if (decoder->IsErrored())
            SetErrored("Unable to reset video decoder");
    }
}

void MythPlayer::CheckTVChain(void)
{
    bool last = !(player_ctx->tvchain->HasNext());
    SetWatchingRecording(last);
}

void MythPlayer::SwitchToProgram(void)
{
    if (!IsReallyNearEnd())
        return;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "SwitchToProgram - start");
    bool discontinuity = false, newtype = false;
    int newid = -1;
    ProgramInfo *pginfo = player_ctx->tvchain->GetSwitchProgram(
        discontinuity, newtype, newid);
    if (!pginfo)
        return;

    bool newIsDummy = player_ctx->tvchain->GetCardType(newid) == "DUMMY";

    SetPlayingInfo(*pginfo);
    Pause();
    ChangeSpeed();

    if (newIsDummy)
    {
        OpenDummy();
        ResetPlaying();
        SetEof(kEofStateNone);
        delete pginfo;
        return;
    }

    if (player_ctx->buffer->GetType() == ICRingBuffer::kRingBufferType)
    {
        // Restore original ringbuffer
        ICRingBuffer *ic = dynamic_cast< ICRingBuffer* >(player_ctx->buffer);
        if (ic) // should always be true
            player_ctx->buffer = ic->Take();
        delete ic;
    }

    player_ctx->buffer->OpenFile(
        pginfo->GetPlaybackURL(), RingBuffer::kLiveTVOpenTimeout);

    if (!player_ctx->buffer->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "SwitchToProgram's OpenFile failed " +
            QString("(card type: %1).")
            .arg(player_ctx->tvchain->GetCardType(newid)));
        LOG(VB_GENERAL, LOG_ERR, player_ctx->tvchain->toString());
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
        player_ctx->tvchain->SetProgram(*pginfo);
        if (decoder)
            decoder->SetProgramInfo(*pginfo);

        player_ctx->buffer->Reset(true);
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
        player_ctx->SetPlayerChangingBuffers(true);
        if (decoder)
        {
            decoder->SetReadAdjust(player_ctx->buffer->SetAdjustFilesize());
            decoder->SetWaitForChange();
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

    // the bitrate is reset by player_ctx->buffer->OpenFile()...
    if (decoder)
        player_ctx->buffer->UpdateRawBitrate(decoder->GetRawBitrate());
    player_ctx->buffer->Unpause();

    if (discontinuity || newtype)
    {
        CheckTVChain();
        forcePositionMapSync = true;
    }

    Play();
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "SwitchToProgram - end");
}

void MythPlayer::FileChangedCallback(void)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "FileChangedCallback");

    Pause();
    ChangeSpeed();
    if (dynamic_cast<AvFormatDecoder *>(decoder))
        player_ctx->buffer->Reset(false, true);
    else
        player_ctx->buffer->Reset(false, true, true);
    SetEof(kEofStateNone);
    Play();

    player_ctx->SetPlayerChangingBuffers(false);

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    player_ctx->tvchain->SetProgram(*player_ctx->playingInfo);
    if (decoder)
        decoder->SetProgramInfo(*player_ctx->playingInfo);
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    CheckTVChain();
    forcePositionMapSync = true;
}

void MythPlayer::JumpToProgram(void)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "JumpToProgram - start");
    bool discontinuity = false, newtype = false;
    int newid = -1;
    long long nextpos = player_ctx->tvchain->GetJumpPos();
    ProgramInfo *pginfo = player_ctx->tvchain->GetSwitchProgram(
        discontinuity, newtype, newid);
    if (!pginfo)
        return;

    inJumpToProgramPause = true;

    bool newIsDummy = player_ctx->tvchain->GetCardType(newid) == "DUMMY";
    SetPlayingInfo(*pginfo);

    Pause();
    ChangeSpeed();
    ResetCaptions();
    player_ctx->tvchain->SetProgram(*pginfo);
    player_ctx->buffer->Reset(true);

    if (newIsDummy)
    {
        OpenDummy();
        ResetPlaying();
        SetEof(kEofStateNone);
        delete pginfo;
        inJumpToProgramPause = false;
        return;
    }

    SendMythSystemPlayEvent("PLAY_CHANGED", pginfo);

    if (player_ctx->buffer->GetType() == ICRingBuffer::kRingBufferType)
    {
        // Restore original ringbuffer
        ICRingBuffer *ic = dynamic_cast< ICRingBuffer* >(player_ctx->buffer);
        if (ic) // should always be true
            player_ctx->buffer = ic->Take();
        delete ic;
    }

    player_ctx->buffer->OpenFile(
        pginfo->GetPlaybackURL(), RingBuffer::kLiveTVOpenTimeout);
    QString subfn = player_ctx->buffer->GetSubtitleFilename();
    TVState desiredState = player_ctx->GetState();
    bool isInProgress =
        desiredState == kState_WatchingRecording || kState_WatchingLiveTV;
    if (GetSubReader())
        GetSubReader()->LoadExternalSubtitles(subfn, isInProgress &&
                                              !subfn.isEmpty());

    if (!player_ctx->buffer->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "JumpToProgram's OpenFile failed " +
            QString("(card type: %1).")
                .arg(player_ctx->tvchain->GetCardType(newid)));
        LOG(VB_GENERAL, LOG_ERR, player_ctx->tvchain->toString());
        SetEof(kEofStateImmediate);
        SetErrored(tr("Error opening jump program file buffer"));
        delete pginfo;
        inJumpToProgramPause = false;
        return;
    }

    bool wasDummy = isDummy;
    if (newtype || wasDummy)
    {
        if (OpenFile() < 0)
            SetErrored(tr("Error opening jump program file"));
    }
    else
        ResetPlaying();

    if (IsErrored() || !decoder)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "JumpToProgram failed.");
        if (!IsErrored())
            SetErrored(tr("Error reopening video decoder"));
        delete pginfo;
        inJumpToProgramPause = false;
        return;
    }

    SetEof(kEofStateNone);

    // the bitrate is reset by player_ctx->buffer->OpenFile()...
    player_ctx->buffer->UpdateRawBitrate(decoder->GetRawBitrate());
    player_ctx->buffer->IgnoreLiveEOF(false);

    decoder->SetProgramInfo(*pginfo);
    delete pginfo;

    CheckTVChain();
    forcePositionMapSync = true;
    inJumpToProgramPause = false;
    Play();
    ChangeSpeed();

    if (nextpos < 0)
        nextpos += totalFrames;
    if (nextpos < 0)
        nextpos = 0;

    if (nextpos > 10)
        DoJumpToFrame(nextpos, kInaccuracyNone);

    player_ctx->SetPlayerChangingBuffers(false);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "JumpToProgram - end");
}

bool MythPlayer::StartPlaying(void)
{
    if (OpenFile() < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to open video file.");
        return false;
    }

    framesPlayed = 0;
    framesPlayedExtra = 0;
    rewindtime = fftime = 0;
    next_play_speed = audio.GetStretchFactor();
    jumpchapter = 0;
    commBreakMap.SkipCommercials(0);

    if (!InitVideo())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to initialize video.");
        audio.DeleteOutput();
        return false;
    }

    bool seek = bookmarkseek > 30;
    EventStart();
    DecoderStart(true);
    if (seek)
        InitialSeek();
    VideoStart();

    playerThread->setPriority(QThread::TimeCriticalPriority);
    UnpauseDecoder();
    return !IsErrored();
}

void MythPlayer::InitialSeek(void)
{
    // TODO handle initial commskip and/or cutlist skip as well
    if (bookmarkseek > 30)
    {
        DoJumpToFrame(bookmarkseek, kInaccuracyNone);
        if (clearSavedPosition && !player_ctx->IsPIP())
            SetBookmark(true);
    }
}


void MythPlayer::StopPlaying()
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("StopPlaying - begin"));
    playerThread->setPriority(QThread::NormalPriority);

    DecoderEnd();
    VideoEnd();
    AudioEnd();

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("StopPlaying - end"));
}

void MythPlayer::EventStart(void)
{
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    {
        if (player_ctx->playingInfo)
            player_ctx->playingInfo->SetIgnoreBookmark(false);
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    commBreakMap.LoadMap(player_ctx, framesPlayed);
}

void MythPlayer::EventLoop(void)
{
    // recreate the osd if a reinit was triggered by another thread
    if (reinit_osd)
        ReinitOSD();

    // reselect subtitle tracks if triggered by the decoder
    if (enableCaptions)
        SetCaptionsEnabled(true, false);
    if (disableCaptions)
        SetCaptionsEnabled(false, false);

    // enable/disable forced subtitles if signalled by the decoder
    if (enableForcedSubtitles)
        DoEnableForcedSubtitles();
    if (disableForcedSubtitles)
        DoDisableForcedSubtitles();

    // reset the scan (and hence deinterlacers) if triggered by the decoder
    if (resetScan != kScan_Ignore)
        SetScanType(resetScan);

    // refresh the position map for an in-progress recording while editing
    if (hasFullPositionMap && IsWatchingInprogress() && deleteMap.IsEditing())
    {
        if (editUpdateTimer.elapsed() > 2000)
        {
            // N.B. the positionmap update and osd refresh are asynchronous
            forcePositionMapSync = true;
            osdLock.lock();
            deleteMap.UpdateOSD(framesPlayed, video_frame_rate, osd);
            osdLock.unlock();
            editUpdateTimer.start();
        }
    }

    // Refresh the programinfo in use status
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
        player_ctx->playingInfo->UpdateInUseMark();
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    // Disable timestretch if we are too close to the end of the buffer
    if (ffrew_skip == 1 && (play_speed > 1.0f) && IsNearEnd())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Near end, Slowing down playback.");
        Play(1.0f, true, true);
    }

    if (isDummy && player_ctx->tvchain && player_ctx->tvchain->HasNext())
    {
        // Switch from the dummy recorder to the tuned program in livetv
        player_ctx->tvchain->JumpToNext(true, 1);
        JumpToProgram();
    }
    else if ((!allpaused || GetEof() != kEofStateNone) &&
             player_ctx->tvchain &&
             (decoder && !decoder->GetWaitForChange()))
    {
        // Switch to the next program in livetv
        if (player_ctx->tvchain->NeedsToSwitch())
            SwitchToProgram();
    }

    // Jump to the next program in livetv
    if (player_ctx->tvchain && player_ctx->tvchain->NeedsToJump())
    {
        JumpToProgram();
    }

    // Change interactive stream if requested
    { QMutexLocker locker(&itvLock);
    if (!m_newStream.isEmpty())
    {
        QString stream = m_newStream;
        m_newStream.clear();
        locker.unlock();
        JumpToStream(stream);
    }}

    // Disable fastforward if we are too close to the end of the buffer
    if (ffrew_skip > 1 && (CalcMaxFFTime(100, false) < 100))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Near end, stopping fastforward.");
        Play(1.0f, true, true);
    }

    // Disable rewind if we are too close to the beginning of the buffer
    if (CalcRWTime(-ffrew_skip) > 0 && (framesPlayed <= keyframedist))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Near start, stopping rewind.");
        float stretch = (ffrew_skip > 0) ? 1.0f : audio.GetStretchFactor();
        Play(stretch, true, true);
    }

    // Check for error
    if (IsErrored() || player_ctx->IsRecorderErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Unknown recorder error, exiting decoder");
        if (!IsErrored())
            SetErrored(tr("Irrecoverable recorder error"));
        killdecoder = true;
        return;
    }

    // Handle speed change
    if (play_speed != next_play_speed &&
        (!player_ctx->tvchain ||
         (player_ctx->tvchain && !player_ctx->tvchain->NeedsToJump())))
    {
        ChangeSpeed();
        return;
    }

    // Handle end of file
    EofState eof = GetEof();
    if (HasReachedEof())
    {
#ifdef USING_MHEG
        if (interactiveTV && interactiveTV->StreamStarted(false))
        {
            Pause();
            return;
        }
#endif
        if (player_ctx->tvchain && player_ctx->tvchain->HasNext())
        {
            LOG(VB_GENERAL, LOG_NOTICE, LOC + "LiveTV forcing JumpTo 1");
            player_ctx->tvchain->JumpToNext(true, 1);
            return;
        }

        bool videoDrained =
            videoOutput && videoOutput->ValidVideoFrames() < 1;
        bool audioDrained =
            !audio.GetAudioOutput() ||
            audio.IsPaused() ||
            audio.GetAudioOutput()->GetAudioBufferedTime() < 100;
        if (eof != kEofStateDelayed || (videoDrained && audioDrained))
        {
            if (eof == kEofStateDelayed)
                LOG(VB_PLAYBACK, LOG_INFO,
                    QString("waiting for no video frames %1")
                    .arg(videoOutput->ValidVideoFrames()));
            LOG(VB_PLAYBACK, LOG_INFO,
                QString("HasReachedEof() at framesPlayed=%1 totalFrames=%2")
                .arg(framesPlayed).arg(GetCurrentFrameCount()));
            Pause();
            SetPlaying(false);
            return;
        }
    }

    // Handle rewind
    if (rewindtime > 0 && (ffrew_skip == 1 || ffrew_skip == 0))
    {
        rewindtime = CalcRWTime(rewindtime);
        if (rewindtime > 0)
            DoRewind(rewindtime, kInaccuracyDefault);
    }

    // Handle fast forward
    if (fftime > 0 && (ffrew_skip == 1 || ffrew_skip == 0))
    {
        fftime = CalcMaxFFTime(fftime);
        if (fftime > 0)
        {
            DoFastForward(fftime, kInaccuracyDefault);
            if (GetEof() != kEofStateNone)
               return;
        }
    }

    // Handle chapter jump
    if (jumpchapter != 0)
        DoJumpChapter(jumpchapter);

    // Handle commercial skipping
    if (commBreakMap.GetSkipCommercials() != 0 && (ffrew_skip == 1))
    {
        if (!commBreakMap.HasMap())
        {
            //: The commercials/adverts have not been flagged
            SetOSDStatus(tr("Not Flagged"), kOSDTimeout_Med);
            QString message = "COMMFLAG_REQUEST ";
            player_ctx->LockPlayingInfo(__FILE__, __LINE__);
            message += QString("%1").arg(player_ctx->playingInfo->GetChanID()) +
                " " + player_ctx->playingInfo->MakeUniqueKey();
            player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
            gCoreContext->SendMessage(message);
        }
        else
        {
            QString msg;
            uint64_t jumpto = 0;
            uint64_t frameCount = GetCurrentFrameCount();
            // XXX CommBreakMap should use duration map not video_frame_rate
            bool jump = commBreakMap.DoSkipCommercials(jumpto, framesPlayed,
                                                       video_frame_rate,
                                                       frameCount, msg);
            if (!msg.isEmpty())
                SetOSDStatus(msg, kOSDTimeout_Med);
            if (jump)
                DoJumpToFrame(jumpto, kInaccuracyNone);
        }
        commBreakMap.SkipCommercials(0);
        return;
    }

    // Handle automatic commercial skipping
    uint64_t jumpto = 0;
    if (deleteMap.IsEmpty() && (ffrew_skip == 1) &&
       (kCommSkipOff != commBreakMap.GetAutoCommercialSkip()))
    {
        QString msg;
        uint64_t frameCount = GetCurrentFrameCount();
        // XXX CommBreakMap should use duration map not video_frame_rate
        bool jump = commBreakMap.AutoCommercialSkip(jumpto, framesPlayed,
                                                    video_frame_rate,
                                                    frameCount, msg);
        if (!msg.isEmpty())
            SetOSDStatus(msg, kOSDTimeout_Med);
        if (jump)
            DoJumpToFrame(jumpto, kInaccuracyNone);
    }

    // Handle cutlist skipping
    if (!allpaused && (ffrew_skip == 1) &&
        deleteMap.TrackerWantsToJump(framesPlayed, jumpto))
    {
        if (jumpto == totalFrames)
        {
            if (!(endExitPrompt == 1 && !player_ctx->IsPIP() &&
                  player_ctx->GetState() == kState_WatchingPreRecorded))
            {
                SetEof(kEofStateDelayed);
            }
        }
        else
        {
            DoJumpToFrame(jumpto, kInaccuracyNone);
        }
    }
}

void MythPlayer::AudioEnd(void)
{
    audio.DeleteOutput();
}

bool MythPlayer::PauseDecoder(void)
{
    decoderPauseLock.lock();
    if (is_current_thread(decoderThread))
    {
        decoderPaused = true;
        decoderThreadPause.wakeAll();
        decoderPauseLock.unlock();
        return decoderPaused;
    }

    int tries = 0;
    pauseDecoder = true;
    while (decoderThread && !killdecoder && (tries++ < 100) &&
           !decoderThreadPause.wait(&decoderPauseLock, 100))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Waited 100ms for decoder to pause");
    }
    pauseDecoder = false;
    decoderPauseLock.unlock();
    return decoderPaused;
}

void MythPlayer::UnpauseDecoder(void)
{
    decoderPauseLock.lock();

    if (is_current_thread(decoderThread))
    {
        decoderPaused = false;
        decoderThreadUnpause.wakeAll();
        decoderPauseLock.unlock();
        return;
    }

    if (!IsInStillFrame())
    {
        int tries = 0;
        unpauseDecoder = true;
        while (decoderThread && !killdecoder && (tries++ < 100) &&
              !decoderThreadUnpause.wait(&decoderPauseLock, 100))
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                "Waited 100ms for decoder to unpause");
        }
        unpauseDecoder = false;
    }
    decoderPauseLock.unlock();
}

void MythPlayer::DecoderStart(bool start_paused)
{
    if (decoderThread)
    {
        if (decoderThread->isRunning())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Decoder thread already running");
        }
        delete decoderThread;
    }

    killdecoder = false;
    decoderThread = new DecoderThread(this, start_paused);
    if (decoderThread)
        decoderThread->start();
}

void MythPlayer::DecoderEnd(void)
{
    PauseDecoder();
    SetPlaying(false);
    killdecoder = true;
    int tries = 0;
    while (decoderThread && !decoderThread->wait(100) && (tries++ < 50))
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            "Waited 100ms for decoder loop to stop");

    if (decoderThread && decoderThread->isRunning())
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to stop decoder loop.");
    else
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Exited decoder loop.");
    SetDecoder(NULL);
}

void MythPlayer::DecoderPauseCheck(void)
{
    if (is_current_thread(decoderThread))
    {
        if (pauseDecoder)
            PauseDecoder();
        if (unpauseDecoder)
            UnpauseDecoder();
    }
}

//// FIXME - move the eof ownership back into MythPlayer
EofState MythPlayer::GetEof(void) const
{
    if (is_current_thread(playerThread))
        return decoder ? decoder->GetEof() : kEofStateImmediate;

    if (!decoder_change_lock.tryLock(50))
        return kEofStateNone;

    EofState eof = decoder ? decoder->GetEof() : kEofStateImmediate;
    decoder_change_lock.unlock();
    return eof;
}

void MythPlayer::SetEof(EofState eof)
{
    if (is_current_thread(playerThread))
    {
        if (decoder)
            decoder->SetEofState(eof);
        return;
    }

    if (!decoder_change_lock.tryLock(50))
        return;

    if (decoder)
        decoder->SetEofState(eof);
    decoder_change_lock.unlock();
}
//// FIXME end

void MythPlayer::DecoderLoop(bool pause)
{
    if (pause)
        PauseDecoder();

    while (!killdecoder && !IsErrored())
    {
        DecoderPauseCheck();

        if (totalDecoderPause || inJumpToProgramPause)
        {
            usleep(1000);
            continue;
        }

        if (forcePositionMapSync)
        {
            if (!decoder_change_lock.tryLock(1))
                continue;
            if (decoder)
            {
                forcePositionMapSync = false;
                decoder->SyncPositionMap();
            }
            decoder_change_lock.unlock();
        }

        if (decoderSeek >= 0)
        {
            if (!decoder_change_lock.tryLock(1))
                continue;
            if (decoder)
            {
                decoderSeekLock.lock();
                if (((uint64_t)decoderSeek < framesPlayed) && decoder)
                    decoder->DoRewind(decoderSeek);
                else if (decoder)
                    decoder->DoFastForward(decoderSeek, !transcoding);
                decoderSeek = -1;
                decoderSeekLock.unlock();
            }
            decoder_change_lock.unlock();
        }

        bool obey_eof = (GetEof() != kEofStateNone) &&
                        !(player_ctx->tvchain && !allpaused);
        if (isDummy || ((decoderPaused || ffrew_skip == 0 || obey_eof) &&
            !decodeOneFrame))
        {
            usleep(1000);
            continue;
        }

        DecodeType dt = (audio.HasAudioOut() && normal_speed) ?
            kDecodeAV : kDecodeVideo;

        DecoderGetFrame(dt);
        decodeOneFrame = false;
    }

    // Clear any wait conditions
    DecoderPauseCheck();
    decoderSeek = -1;
}

bool MythPlayer::DecoderGetFrameFFREW(void)
{
    if (!decoder)
        return false;

    if (ffrew_skip > 0)
    {
        long long delta = decoder->GetFramesRead() - framesPlayed;
        long long real_skip = CalcMaxFFTime(ffrew_skip - ffrew_adjust + delta) - delta;
        long long target_frame = decoder->GetFramesRead() + real_skip;
        if (real_skip >= 0)
        {
            decoder->DoFastForward(target_frame, false);
        }
        long long seek_frame  = decoder->GetFramesRead();
        ffrew_adjust = seek_frame - target_frame;
    }
    else if (CalcRWTime(-ffrew_skip) >= 0)
    {
        DecoderGetFrameREW();
    }
    return decoder->GetFrame(kDecodeVideo);
}

bool MythPlayer::DecoderGetFrameREW(void)
{
    long long cur_frame    = decoder->GetFramesPlayed();
    bool      toBegin      = -cur_frame > ffrew_skip + ffrew_adjust;
    long long real_skip    = (toBegin) ? -cur_frame : ffrew_skip + ffrew_adjust;
    long long target_frame = cur_frame + real_skip;
    bool ret = decoder->DoRewind(target_frame, false);
    long long seek_frame  = decoder->GetFramesPlayed();
    ffrew_adjust = target_frame - seek_frame;
    return ret;
}

bool MythPlayer::DecoderGetFrame(DecodeType decodetype, bool unsafe)
{
    bool ret = false;
    if (!videoOutput)
        return false;

    // Wait for frames to be available for decoding onto
    if (!videoOutput->EnoughFreeFrames() && !unsafe && !killdecoder)
    {
        int tries = 0;
        while (!videoOutput->EnoughFreeFrames() && (tries++ < 10))
            usleep(1000);
        if (!videoOutput->EnoughFreeFrames())
        {
            if (++videobuf_retries >= 2000)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Decoder timed out waiting for free video buffers.");
                // We've tried for 20 seconds now, give up so that we don't
                // get stuck permanently in this state
                SetErrored("Decoder timed out waiting for free video buffers.");
            }
            return false;
        }
    }
    videobuf_retries = 0;

    if (!decoder_change_lock.tryLock(5))
        return false;
    if (killdecoder || !decoder || IsErrored())
    {
        decoder_change_lock.unlock();
        return false;
    }

    if (ffrew_skip == 1 || decodeOneFrame)
        ret = decoder->GetFrame(decodetype);
    else if (ffrew_skip != 0)
        ret = DecoderGetFrameFFREW();
    decoder_change_lock.unlock();
    return ret;
}

void MythPlayer::SetTranscoding(bool value)
{
    transcoding = value;

    if (decoder)
        decoder->SetTranscoding(value);
}

bool MythPlayer::AddPIPPlayer(MythPlayer *pip, PIPLocation loc, uint timeout)
{
    if (!is_current_thread(playerThread))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot add PiP from another thread");
        return false;
    }

    if (pip_players.contains(pip))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "PiPMap already contains PiP.");
        return false;
    }

    QList<PIPLocation> locs = pip_players.values();
    if (locs.contains(loc))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +"Already have a PiP at that location.");
        return false;
    }

    pip_players.insert(pip, loc);
    return true;
}

bool MythPlayer::RemovePIPPlayer(MythPlayer *pip, uint timeout)
{
    if (!is_current_thread(playerThread))
        return false;

    if (!pip_players.contains(pip))
        return false;

    pip_players.remove(pip);
    if (videoOutput)
        videoOutput->RemovePIP(pip);
    return true;
}

PIPLocation MythPlayer::GetNextPIPLocation(void) const
{
    if (!is_current_thread(playerThread))
        return kPIP_END;

    if (pip_players.isEmpty())
        return pip_default_loc;

    // order of preference, could be stored in db if we want it configurable
    PIPLocation ols[] =
        { kPIPTopLeft, kPIPTopRight, kPIPBottomLeft, kPIPBottomRight };

    for (uint i = 0; i < sizeof(ols)/sizeof(PIPLocation); i++)
    {
        PIPMap::const_iterator it = pip_players.begin();
        for (; it != pip_players.end() && (*it != ols[i]); ++it);

        if (it == pip_players.end())
            return ols[i];
    }

    return kPIP_END;
}

int64_t MythPlayer::AdjustAudioTimecodeOffset(int64_t v, int newsync)
{
    if ((newsync >= -1000) && (newsync <= 1000))
        tc_wrap[TC_AUDIO] = newsync;
    else
        tc_wrap[TC_AUDIO] += v;
    gCoreContext->SaveSetting("AudioSyncOffset", tc_wrap[TC_AUDIO]);
    return tc_wrap[TC_AUDIO];
}

void MythPlayer::WrapTimecode(int64_t &timecode, TCTypes tc_type)
{
    timecode += tc_wrap[tc_type];
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
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (!player_ctx->playingInfo)
    {
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return;
    }

    uint64_t numFrames = GetCurrentFrameCount();

    // For recordings we want to ignore the post-roll and account for
    // in-progress recordings where totalFrames doesn't represent
    // the full length of the recording. For videos we can only rely on
    // totalFrames as duration metadata can be wrong
    if (player_ctx->playingInfo->IsRecording() &&
        player_ctx->playingInfo->QueryTranscodeStatus() !=
        TRANSCODING_COMPLETE)
    {
        uint endtime;

        // If the recording is stopped early we need to use the recording end
        // time, not the programme end time
        if (player_ctx->playingInfo->GetRecordingEndTime().toTime_t() <
            player_ctx->playingInfo->GetScheduledEndTime().toTime_t())
        {
            endtime = player_ctx->playingInfo->GetRecordingEndTime().toTime_t();
        }
        else
        {
            endtime = player_ctx->playingInfo->GetScheduledEndTime().toTime_t();
        }

        numFrames = (long long)
            ((endtime -
              player_ctx->playingInfo->GetRecordingStartTime().toTime_t()) *
             video_frame_rate);
    }

    int offset = (int) round(0.14 * (numFrames / video_frame_rate));

    if (offset < 240)
        offset = 240; // 4 Minutes Min
    else if (offset > 720)
        offset = 720; // 12 Minutes Max

    if (forceWatched || framesPlayed > numFrames - (offset * video_frame_rate))
    {
        player_ctx->playingInfo->SaveWatched(true);
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Marking recording as watched using offset %1 minutes")
                .arg(offset/60));
    }

    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
}

void MythPlayer::SetBookmark(bool clear)
{
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
        player_ctx->playingInfo->SaveBookmark(clear ? 0 : framesPlayed);
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
}

uint64_t MythPlayer::GetBookmark(void)
{
    uint64_t bookmark = 0;

    if (gCoreContext->IsDatabaseIgnored() ||
       (player_ctx->buffer && !player_ctx->buffer->IsBookmarkAllowed()))
        bookmark = 0;
    else
    {
        player_ctx->LockPlayingInfo(__FILE__, __LINE__);
        if (player_ctx->playingInfo)
            bookmark = player_ctx->playingInfo->QueryBookmark();
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    }

    return bookmark;
}

bool MythPlayer::UpdateFFRewSkip(void)
{
    bool skip_changed;

    float temp_speed = (play_speed == 0.0) ?
        audio.GetStretchFactor() : play_speed;
    if (play_speed >= 0.0f && play_speed <= 3.0f)
    {
        skip_changed = (ffrew_skip != 1);
        frame_interval = (int) (1000000.0f / video_frame_rate / temp_speed);
        ffrew_skip = (play_speed != 0.0f);
    }
    else
    {
        skip_changed = true;
        frame_interval = 200000;
        frame_interval = (fabs(play_speed) >=   3.0) ? 133466 : frame_interval;
        frame_interval = (fabs(play_speed) >=   5.0) ? 133466 : frame_interval;
        frame_interval = (fabs(play_speed) >=   8.0) ? 250250 : frame_interval;
        frame_interval = (fabs(play_speed) >=  10.0) ? 133466 : frame_interval;
        frame_interval = (fabs(play_speed) >=  16.0) ? 187687 : frame_interval;
        frame_interval = (fabs(play_speed) >=  20.0) ? 150150 : frame_interval;
        frame_interval = (fabs(play_speed) >=  30.0) ? 133466 : frame_interval;
        frame_interval = (fabs(play_speed) >=  60.0) ? 133466 : frame_interval;
        frame_interval = (fabs(play_speed) >= 120.0) ? 133466 : frame_interval;
        frame_interval = (fabs(play_speed) >= 180.0) ? 133466 : frame_interval;
        float ffw_fps = fabs(play_speed) * video_frame_rate;
        float dis_fps = 1000000.0f / frame_interval;
        ffrew_skip = (int)ceil(ffw_fps / dis_fps);
        ffrew_skip = play_speed < 0.0f ? -ffrew_skip : ffrew_skip;
        ffrew_adjust = 0;
    }

    return skip_changed;
}

void MythPlayer::ChangeSpeed(void)
{
    float last_speed = play_speed;
    play_speed   = next_play_speed;
    normal_speed = next_normal_speed;

    bool skip_changed = UpdateFFRewSkip();
    videosync->setFrameInterval(frame_interval);

    if (skip_changed && videoOutput)
    {
        videoOutput->SetPrebuffering(ffrew_skip == 1);
        if (play_speed != 0.0f && !(last_speed == 0.0f && ffrew_skip == 1))
            DoJumpToFrame(framesPlayed + fftime - rewindtime, kInaccuracyFull);
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Play speed: " +
        QString("rate: %1 speed: %2 skip: %3 => new interval %4")
            .arg(video_frame_rate).arg(play_speed)
            .arg(ffrew_skip).arg(frame_interval));

    if (videoOutput && videosync)
    {
        // We need to tell it this for automatic deinterlacer settings
        videoOutput->SetVideoFrameRate(video_frame_rate);

        // If using bob deinterlace, turn on or off if we
        // changed to or from synchronous playback speed.
        bool play_1 = play_speed > 0.99f && play_speed < 1.01f && normal_speed;
        bool inter  = (kScan_Interlaced   == m_scan  ||
                       kScan_Intr2ndField == m_scan);

        videofiltersLock.lock();
        if (m_double_framerate && !play_1)
            videoOutput->FallbackDeint();
        else if (!m_double_framerate && CanSupportDoubleRate() && play_1 &&
                 inter)
            videoOutput->BestDeint();
        videofiltersLock.unlock();

        m_double_framerate = videoOutput->NeedsDoubleFramerate();
        m_double_process = videoOutput->IsExtraProcessingRequired();
    }

    if (normal_speed && audio.HasAudioOut())
    {
        audio.SetStretchFactor(play_speed);
        syncWithAudioStretch();
    }
}

bool MythPlayer::DoRewind(uint64_t frames, double inaccuracy)
{
    if (player_ctx->buffer && !player_ctx->buffer->IsSeekingAllowed())
        return false;

    uint64_t number = frames + 1;
    uint64_t desiredFrame = (framesPlayed > number) ? framesPlayed - number : 0;

    limitKeyRepeat = false;
    if (desiredFrame < video_frame_rate)
        limitKeyRepeat = true;

    uint64_t seeksnap_wanted = UINT64_MAX;
    if (inaccuracy != kInaccuracyFull)
        seeksnap_wanted = frames * inaccuracy;
    WaitForSeek(desiredFrame, seeksnap_wanted);
    rewindtime = 0;
    ClearAfterSeek();
    return true;
}

bool MythPlayer::DoRewindSecs(float secs, double inaccuracy, bool use_cutlist)
{
    float current = ComputeSecs(framesPlayed, use_cutlist);
    float target = current - secs;
    if (target < 0)
        target = 0;
    uint64_t targetFrame = FindFrame(target, use_cutlist);
    return DoRewind(framesPlayed - targetFrame, inaccuracy);
}

long long MythPlayer::CalcRWTime(long long rw) const
{
    bool hasliveprev = (livetv && player_ctx->tvchain &&
                        player_ctx->tvchain->HasPrev());

    if (!hasliveprev || ((int64_t)framesPlayed > (rw - 1)))
        return rw;

    player_ctx->tvchain->JumpToNext(false, (int)(-15.0 * video_frame_rate)); // XXX use seconds instead of assumed framerate
    return -1;
}

long long MythPlayer::CalcMaxFFTime(long long ffframes, bool setjump) const
{
    float maxtime = 1.0;
    bool islivetvcur = (livetv && player_ctx->tvchain &&
                        !player_ctx->tvchain->HasNext());

    if (livetv || IsWatchingInprogress())
        maxtime = 3.0;

    long long ret = ffframes;
    float ff = ComputeSecs(ffframes, true);
    float secsPlayed = ComputeSecs(framesPlayed, true);

    limitKeyRepeat = false;

    if (livetv && !islivetvcur && player_ctx->tvchain)
    {
        if (totalFrames > 0)
        {
            float behind = ComputeSecs(totalFrames, true) - secsPlayed;
            if (behind < maxtime || behind - ff <= maxtime * 2)
            {
                ret = -1;
                if (setjump)
                    player_ctx->tvchain->JumpToNext(true, 1);
            }
        }
    }
    else if (islivetvcur || IsWatchingInprogress())
    {
        float secsWritten =
            ComputeSecs(player_ctx->recorder->GetFramesWritten(), true);
        float behind = secsWritten - secsPlayed;

        if (behind < maxtime) // if we're close, do nothing
            ret = 0;
        else if (behind - ff <= maxtime)
            ret = TranslatePositionMsToFrame(1000 * (secsWritten - maxtime),
                                             true) - framesPlayed;

        if (behind < maxtime * 3)
            limitKeyRepeat = true;
    }
    else
    {
        if (totalFrames > 0)
        {
            float behind = ComputeSecs(totalFrames, true) - secsPlayed;
            if (behind < maxtime)
                ret = 0;
            else if (behind - ff <= maxtime * 2)
            {
                uint64_t ms = 1000 *
                    (ComputeSecs(totalFrames, true) - maxtime * 2);
                ret = TranslatePositionMsToFrame(ms, true) - framesPlayed;
            }
        }
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
    if (!videoOutput || !decoder)
        return false;

    return player_ctx->buffer->IsNearEnd(
        decoder->GetFPS(), videoOutput->ValidVideoFrames());
}

/** \brief Returns true iff near end of recording.
 */
bool MythPlayer::IsNearEnd(void)
{
    uint64_t framesRead, framesLeft = 0;

    if (!player_ctx)
        return false;

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (!player_ctx->playingInfo || player_ctx->playingInfo->IsVideo() ||
        !decoder)
    {
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return false;
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    long long margin = (long long)(video_frame_rate * 2);
    margin = (long long) (margin * audio.GetStretchFactor());
    bool watchingTV = IsWatchingInprogress();

    framesRead = framesPlayed;

    if (!player_ctx->IsPIP() &&
        player_ctx->GetState() == kState_WatchingPreRecorded)
    {
        if (framesRead >= deleteMap.GetLastFrame())
            return true;
        uint64_t frameCount = GetCurrentFrameCount();
        framesLeft = (frameCount > framesRead) ? frameCount - framesRead : 0;
        return (framesLeft < (uint64_t)margin);
    }

    if (!livetv && !watchingTV)
        return false;

    if (livetv && player_ctx->tvchain && player_ctx->tvchain->HasNext())
        return false;

    if (player_ctx->recorder)
    {
        framesLeft =
            player_ctx->recorder->GetCachedFramesWritten() - framesRead;

        // if it looks like we are near end, get an updated GetFramesWritten()
        if (framesLeft < (uint64_t)margin)
            framesLeft = player_ctx->recorder->GetFramesWritten() - framesRead;
    }

    return (framesLeft < (uint64_t)margin);
}

bool MythPlayer::DoFastForward(uint64_t frames, double inaccuracy)
{
    if (player_ctx->buffer && !player_ctx->buffer->IsSeekingAllowed())
        return false;

    uint64_t number = (frames ? frames - 1 : 0);
    uint64_t desiredFrame = framesPlayed + number;

    if (!deleteMap.IsEditing() && IsInDelete(desiredFrame))
    {
        uint64_t endcheck = deleteMap.GetLastFrame();
        if (desiredFrame > endcheck)
            desiredFrame = endcheck;
    }

    uint64_t seeksnap_wanted = UINT64_MAX;
    if (inaccuracy != kInaccuracyFull)
        seeksnap_wanted = frames * inaccuracy;
    WaitForSeek(desiredFrame, seeksnap_wanted);
    fftime = 0;
    ClearAfterSeek(false);
    return true;
}

bool MythPlayer::DoFastForwardSecs(float secs, double inaccuracy,
                                   bool use_cutlist)
{
    float current = ComputeSecs(framesPlayed, use_cutlist);
    float target = current + secs;
    uint64_t targetFrame = FindFrame(target, use_cutlist);
    return DoFastForward(targetFrame - framesPlayed, inaccuracy);
}

void MythPlayer::DoJumpToFrame(uint64_t frame, double inaccuracy)
{
    if (frame > framesPlayed)
        DoFastForward(frame - framesPlayed, inaccuracy);
    else if (frame <= framesPlayed)
        DoRewind(framesPlayed - frame, inaccuracy);
}

void MythPlayer::WaitForSeek(uint64_t frame, uint64_t seeksnap_wanted)
{
    if (!decoder)
        return;

    SetEof(kEofStateNone);
    decoder->SetSeekSnap(seeksnap_wanted);

    bool islivetvcur = (livetv && player_ctx->tvchain &&
                        !player_ctx->tvchain->HasNext());

    uint64_t max = GetCurrentFrameCount();
    if (islivetvcur || IsWatchingInprogress())
    {
        max = (uint64_t)player_ctx->recorder->GetFramesWritten();
    }
    if (frame >= max)
        frame = max - 1;

    decoderSeekLock.lock();
    decoderSeek = frame;
    decoderSeekLock.unlock();

    int count = 0;
    bool need_clear = false;
    while (decoderSeek >= 0)
    {
        usleep(1000);

        // provide some on screen feedback if seeking is slow
        count++;
        if (!(count % 150) && !hasFullPositionMap)
        {
            int num = (count / 150) % 4;
            SetOSDMessage(tr("Searching") + QString().fill('.', num),
                          kOSDTimeout_Short);
            DisplayPauseFrame();
            need_clear = true;
        }
    }
    if (need_clear)
    {
        osdLock.lock();
        if (osd)
            osd->HideWindow("osd_message");
        osdLock.unlock();
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

    if (clearvideobuffers && videoOutput)
        videoOutput->ClearAfterSeek();

    int64_t savedAudioTimecodeOffset = tc_wrap[TC_AUDIO];

    for (int j = 0; j < TCTYPESMAX; j++)
        tc_wrap[j] = tc_lastval[j] = 0;

    tc_wrap[TC_AUDIO] = savedAudioTimecodeOffset;

    audio.Reset();
    ResetCaptions();
    deleteMap.TrackerReset(framesPlayed);
    commBreakMap.SetTracker(framesPlayed);
    commBreakMap.ResetLastSkip();
    needNewPauseFrame = true;
    ResetAVSync();
}

void MythPlayer::SetPlayerInfo(TV *tv, QWidget *widget, PlayerContext *ctx)
{
    deleteMap.SetPlayerContext(ctx);
    m_tv = tv;
    parentWidget = widget;
    player_ctx   = ctx;
    livetv       = ctx->tvchain;
}

bool MythPlayer::EnableEdit(void)
{
    deleteMap.SetEditing(false);

    if (!hasFullPositionMap)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot edit - no full position map");
        SetOSDStatus(tr("No Seektable"), kOSDTimeout_Med);
        return false;
    }

    if (deleteMap.IsFileEditing())
        return false;

    QMutexLocker locker(&osdLock);
    if (!osd)
        return false;

    speedBeforeEdit = play_speed;
    pausedBeforeEdit = Pause();
    deleteMap.SetEditing(true);
    osd->DialogQuit();
    ResetCaptions();
    osd->HideAll();

    bool loadedAutoSave = deleteMap.LoadAutoSaveMap();
    if (loadedAutoSave)
    {
        SetOSDMessage(tr("Using previously auto-saved cuts"),
                      kOSDTimeout_Short);
    }

    deleteMap.UpdateSeekAmount(0);
    deleteMap.UpdateOSD(framesPlayed, video_frame_rate, osd);
    deleteMap.SetFileEditing(true);
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
        player_ctx->playingInfo->SaveEditing(true);
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    editUpdateTimer.start();

    return deleteMap.IsEditing();
}

/** \fn MythPlayer::DisableEdit(int)
 *  \brief Leave cutlist edit mode, saving work in 1 of 3 ways.
 *
 *  \param howToSave If 1, save all changes.  If 0, discard all
 *  changes.  If -1, do not explicitly save changes but leave
 *  auto-save information intact in the database.
 */
void MythPlayer::DisableEdit(int howToSave)
{
    QMutexLocker locker(&osdLock);
    if (!osd)
        return;

    deleteMap.SetEditing(false, osd);
    if (howToSave == 0)
        deleteMap.LoadMap();
    // Unconditionally save to remove temporary marks from the DB.
    if (howToSave >= 0)
        deleteMap.SaveMap();
    deleteMap.TrackerReset(framesPlayed);
    deleteMap.SetFileEditing(false);
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
        player_ctx->playingInfo->SaveEditing(false);
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    if (!pausedBeforeEdit)
        Play(speedBeforeEdit);
    else
        SetOSDStatus(tr("Paused"), kOSDTimeout_None);
}

bool MythPlayer::HandleProgramEditorActions(QStringList &actions)
{
    bool handled = false;
    bool refresh = true;
    long long frame = GetFramesPlayed();

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;
        float seekamount = deleteMap.GetSeekAmount();
        if (action == ACTION_LEFT)
        {
            if (seekamount == 0) // 1 frame
                DoRewind(1, kInaccuracyNone);
            else if (seekamount > 0)
                DoRewindSecs(seekamount, kInaccuracyEditor, false);
            else
                HandleArbSeek(false);
        }
        else if (action == ACTION_RIGHT)
        {
            if (seekamount == 0) // 1 frame
                DoFastForward(1, kInaccuracyNone);
            else if (seekamount > 0)
                DoFastForwardSecs(seekamount, kInaccuracyEditor, false);
            else
                HandleArbSeek(true);
        }
        else if (action == ACTION_LOADCOMMSKIP)
        {
            if (commBreakMap.HasMap())
            {
                frm_dir_map_t map;
                commBreakMap.GetMap(map);
                deleteMap.LoadCommBreakMap(map);
            }
        }
        else if (action == ACTION_PREVCUT)
        {
            float old_seekamount = deleteMap.GetSeekAmount();
            deleteMap.SetSeekAmount(-2);
            HandleArbSeek(false);
            deleteMap.SetSeekAmount(old_seekamount);
        }
        else if (action == ACTION_NEXTCUT)
        {
            float old_seekamount = deleteMap.GetSeekAmount();
            deleteMap.SetSeekAmount(-2);
            HandleArbSeek(true);
            deleteMap.SetSeekAmount(old_seekamount);
        }
#define FFREW_MULTICOUNT 10
        else if (action == ACTION_BIGJUMPREW)
        {
            if (seekamount == 0)
                DoRewind(FFREW_MULTICOUNT, kInaccuracyNone);
            else if (seekamount > 0)
                DoRewindSecs(seekamount * FFREW_MULTICOUNT,
                             kInaccuracyEditor, false);
            else
                DoRewindSecs(FFREW_MULTICOUNT / 2,
                             kInaccuracyNone, false);
        }
        else if (action == ACTION_BIGJUMPFWD)
        {
            if (seekamount == 0)
                DoFastForward(FFREW_MULTICOUNT, kInaccuracyNone);
            else if (seekamount > 0)
                DoFastForwardSecs(seekamount * FFREW_MULTICOUNT,
                                  kInaccuracyEditor, false);
            else
                DoFastForwardSecs(FFREW_MULTICOUNT / 2,
                                  kInaccuracyNone, false);
        }
        else if (action == ACTION_SELECT)
        {
            deleteMap.NewCut(frame);
            SetOSDMessage(tr("New cut added."), kOSDTimeout_Short);
            refresh = true;
        }
        else if (action == "DELETE")
        {
            deleteMap.Delete(frame, tr("Delete"));
            refresh = true;
        }
        else if (action == "REVERT")
        {
            deleteMap.LoadMap(tr("Undo Changes"));
            refresh = true;
        }
        else if (action == "REVERTEXIT")
        {
            DisableEdit(0);
            refresh = false;
        }
        else if (action == ACTION_SAVEMAP)
        {
            deleteMap.SaveMap();
            refresh = true;
        }
        else if (action == "EDIT" || action == "SAVEEXIT")
        {
            DisableEdit(1);
            refresh = false;
        }
        else
        {
            QString undoMessage = deleteMap.GetUndoMessage();
            QString redoMessage = deleteMap.GetRedoMessage();
            handled = deleteMap.HandleAction(action, frame);
            if (handled && (action == "CUTTOBEGINNING" ||
                action == "CUTTOEND" || action == "NEWCUT"))
            {
                SetOSDMessage(tr("New cut added."), kOSDTimeout_Short);
            }
            else if (handled && action == "UNDO")
            {
                //: %1 is the undo message
                SetOSDMessage(tr("Undo - %1").arg(undoMessage),
                              kOSDTimeout_Short);
            }
            else if (handled && action == "REDO")
            {
                //: %1 is the redo message
                SetOSDMessage(tr("Redo - %1").arg(redoMessage),
                              kOSDTimeout_Short);
            }
        }
    }

    if (handled && refresh)
    {
        osdLock.lock();
        if (osd)
        {
            deleteMap.UpdateOSD(framesPlayed, video_frame_rate, osd);
        }
        osdLock.unlock();
    }

    return handled;
}

bool MythPlayer::IsInDelete(uint64_t frame)
{
    return deleteMap.IsInDelete(frame);
}

uint64_t MythPlayer::GetNearestMark(uint64_t frame, bool right)
{
    return deleteMap.GetNearestMark(frame, right);
}

bool MythPlayer::IsTemporaryMark(uint64_t frame)
{
    return deleteMap.IsTemporaryMark(frame);
}

bool MythPlayer::HasTemporaryMark(void)
{
    return deleteMap.HasTemporaryMark();
}

void MythPlayer::HandleArbSeek(bool right)
{
    if (deleteMap.GetSeekAmount() == -2)
    {
        uint64_t framenum = deleteMap.GetNearestMark(framesPlayed, right);
        if (right && (framenum > framesPlayed))
            DoFastForward(framenum - framesPlayed, kInaccuracyNone);
        else if (!right && (framesPlayed > framenum))
            DoRewind(framesPlayed - framenum, kInaccuracyNone);
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
    if (videoOutput)
        return videoOutput->GetAspectOverride();
    return kAspect_Off;
}

AdjustFillMode MythPlayer::GetAdjustFill(void) const
{
    if (videoOutput)
        return videoOutput->GetAdjustFill();
    return kAdjustFill_Off;
}

void MythPlayer::ToggleAspectOverride(AspectOverrideMode aspectMode)
{
    if (videoOutput)
    {
        videoOutput->ToggleAspectOverride(aspectMode);
        ReinitOSD();
    }
}

void MythPlayer::ToggleAdjustFill(AdjustFillMode adjustfillMode)
{
    if (videoOutput)
    {
        detect_letter_box->SetDetectLetterbox(false);
        videoOutput->ToggleAdjustFill(adjustfillMode);
        ReinitOSD();
    }
}

void MythPlayer::Zoom(ZoomDirection direction)
{
    if (videoOutput)
    {
        videoOutput->Zoom(direction);
        ReinitOSD();
    }
}

void MythPlayer::ExposeEvent(void)
{
    if (videoOutput)
        videoOutput->ExposeEvent();
}

bool MythPlayer::IsEmbedding(void)
{
    if (videoOutput)
        return videoOutput->IsEmbedding();
    return false;
}

bool MythPlayer::GetScreenShot(int width, int height, QString filename)
{
    if (videoOutput)
        return videoOutput->GetScreenShot(width, height, filename);
    return false;
}

bool MythPlayer::HasTVChainNext(void) const
{
    return player_ctx->tvchain && player_ctx->tvchain->HasNext();
}

/** \fn MythPlayer::GetScreenGrab(int,int&,int&,int&,float&)
 *  \brief Returns a one RGB frame grab from a video.
 *
 *   User is responsible for deleting the buffer with delete[].
 *   This also tries to skip any commercial breaks for a more
 *   useful screen grab for previews.
 *
 *   Warning: Don't use this on something you're playing!
 *
 *  \param secondsin [in]  Seconds to seek into the buffer
 *  \param bufflen   [out] Size of buffer returned in bytes
 *  \param vw        [out] Width of buffer returned
 *  \param vh        [out] Height of buffer returned
 *  \param ar        [out] Aspect of buffer returned
 */
char *MythPlayer::GetScreenGrab(int secondsin, int &bufflen,
                                int &vw, int &vh, float &ar)
{
    long long frameNum = (long long)(secondsin * video_frame_rate);

    return GetScreenGrabAtFrame(frameNum, false, bufflen, vw, vh, ar);
}

/**
 *  \brief Returns a one RGB frame grab from a video.
 *
 *   User is responsible for deleting the buffer with delete[].
 *   This also tries to skip any commercial breaks for a more
 *   useful screen grab for previews.
 *
 *   Warning: Don't use this on something you're playing!
 *
 *  \param frameNum  [in]  Frame number to capture
 *  \param absolute  [in]  If False, make sure we aren't in cutlist or Comm brk
 *  \param bufflen   [out] Size of buffer returned in bytes
 *  \param vw        [out] Width of buffer returned
 *  \param vh        [out] Height of buffer returned
 *  \param ar        [out] Aspect of buffer returned
 */
char *MythPlayer::GetScreenGrabAtFrame(uint64_t frameNum, bool absolute,
                                       int &bufflen, int &vw, int &vh,
                                       float &ar)
{
    uint64_t       number    = 0;
    unsigned char *data      = NULL;
    unsigned char *outputbuf = NULL;
    VideoFrame    *frame     = NULL;
    AVPicture      orig;
    AVPicture      retbuf;
    memset(&orig,   0, sizeof(AVPicture));
    memset(&retbuf, 0, sizeof(AVPicture));

    if (OpenFile(0) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Could not open file for preview.");
        return NULL;
    }

    if ((video_dim.width() <= 0) || (video_dim.height() <= 0))
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            QString("Video Resolution invalid %1x%2")
                .arg(video_dim.width()).arg(video_dim.height()));

        // This is probably an audio file, just return a grey frame.
        vw = 640;
        vh = 480;
        ar = 4.0f / 3.0f;

        bufflen = vw * vh * 4;
        outputbuf = new unsigned char[bufflen];
        memset(outputbuf, 0x3f, bufflen * sizeof(unsigned char));
        return (char*) outputbuf;
    }

    if (!InitVideo())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Unable to initialize video for screen grab.");
        return NULL;
    }

    ClearAfterSeek();
    if (!decoderThread)
        DecoderStart(true /*start paused*/);
    SeekForScreenGrab(number, frameNum, absolute);
    int tries = 0;
    while (!videoOutput->ValidVideoFrames() && ((tries++) < 500))
    {
        decodeOneFrame = true;
        usleep(10000);
        if ((tries & 10) == 10)
            LOG(VB_PLAYBACK, LOG_INFO, LOC +
                "ScreenGrab: Waited 100ms for video frame");
    }

    if (!(frame = videoOutput->GetLastDecodedFrame()))
    {
        bufflen = 0;
        vw = vh = 0;
        ar = 0;
        return NULL;
    }

    if (!(data = frame->buf))
    {
        bufflen = 0;
        vw = vh = 0;
        ar = 0;
        DiscardVideoFrame(frame);
        return NULL;
    }

    avpicture_fill(&orig, data, PIX_FMT_YUV420P,
                   video_dim.width(), video_dim.height());

    avpicture_deinterlace(&orig, &orig, PIX_FMT_YUV420P,
                          video_dim.width(), video_dim.height());

    bufflen = video_dim.width() * video_dim.height() * 4;
    outputbuf = new unsigned char[bufflen];

    avpicture_fill(&retbuf, outputbuf, PIX_FMT_RGB32,
                   video_dim.width(), video_dim.height());

    myth_sws_img_convert(
        &retbuf, PIX_FMT_RGB32, &orig, PIX_FMT_YUV420P,
                video_dim.width(), video_dim.height());

    vw = video_disp_dim.width();
    vh = video_disp_dim.height();
    ar = frame->aspect;

    DiscardVideoFrame(frame);
    return (char *)outputbuf;
}

void MythPlayer::SeekForScreenGrab(uint64_t &number, uint64_t frameNum,
                                   bool absolute)
{
    number = frameNum;
    if (number >= totalFrames)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC +
            "Screen grab requested for frame number beyond end of file.");
        number = totalFrames / 2;
    }

    if (!absolute && hasFullPositionMap)
    {
        bookmarkseek = GetBookmark();
        // Use the bookmark if we should, otherwise make sure we aren't
        // in the cutlist or a commercial break
        if (bookmarkseek > 30)
        {
            number = bookmarkseek;
        }
        else
        {
            uint64_t oldnumber = number;
            deleteMap.LoadMap();
            commBreakMap.LoadMap(player_ctx, framesPlayed);

            bool started_in_break_map = false;
            while (commBreakMap.IsInCommBreak(number) ||
                   IsInDelete(number))
            {
                started_in_break_map = true;
                number += (uint64_t) (30 * video_frame_rate);
                if (number >= totalFrames)
                {
                    number = oldnumber;
                    break;
                }
            }

            // Advance a few seconds from the end of the break
            if (started_in_break_map)
            {
                oldnumber = number;
                number += (long long) (10 * video_frame_rate);
                if (number >= totalFrames)
                    number = oldnumber;
            }
        }
    }

    DiscardVideoFrame(videoOutput->GetLastDecodedFrame());
    DoJumpToFrame(number, kInaccuracyNone);
}

/** \fn MythPlayer::GetRawVideoFrame(long long)
 *  \brief Returns a specific frame from the video.
 *
 *   NOTE: You must call DiscardVideoFrame(VideoFrame*) on
 *         the frame returned, as this marks the frame as
 *         being used and hence unavailable for decoding.
 */
VideoFrame* MythPlayer::GetRawVideoFrame(long long frameNumber)
{
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
        player_ctx->playingInfo->UpdateInUseMark();
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    if (!decoderThread)
        DecoderStart(false);

    if (frameNumber >= 0)
    {
        DoJumpToFrame(frameNumber, kInaccuracyNone);
        ClearAfterSeek();
    }

    int tries = 0;
    while (!videoOutput->ValidVideoFrames() && ((tries++) < 100))
    {
        decodeOneFrame = true;
        usleep(10000);
        if ((tries & 10) == 10)
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Waited 100ms for video frame");
    }

    videoOutput->StartDisplayingFrame();
    return videoOutput->GetLastShownFrame();
}

QString MythPlayer::GetEncodingType(void) const
{
    if (decoder)
        return get_encoding_type(decoder->GetVideoCodecID());
    return QString();
}

void MythPlayer::GetCodecDescription(InfoMap &infoMap)
{
    infoMap["audiocodec"]    = ff_codec_id_string((CodecID)audio.GetCodec());
    infoMap["audiochannels"] = QString::number(audio.GetOrigChannels());

    int width  = video_disp_dim.width();
    int height = video_disp_dim.height();
    infoMap["videocodec"]     = GetEncodingType();
    if (decoder)
        infoMap["videocodecdesc"] = decoder->GetRawEncodingType();
    infoMap["videowidth"]     = QString::number(width);
    infoMap["videoheight"]    = QString::number(height);
    infoMap["videoframerate"] = QString::number(video_frame_rate, 'f', 2);

    if (width < 640)
        return;

    bool interlaced = is_interlaced(m_scan);
    if (width == 1920 || height == 1080 || height == 1088)
        infoMap["videodescrip"] = interlaced ? "HD_1080_I" : "HD_1080_P";
    else if (height == 720 && !interlaced)
        infoMap["videodescrip"] = "HD_720_P";
    else if (height >= 720)
        infoMap["videodescrip"] = "HD";
    else infoMap["videodescrip"] = "SD";
}

bool MythPlayer::GetRawAudioState(void) const
{
    if (decoder)
        return decoder->GetRawAudioState();
    return false;
}

QString MythPlayer::GetXDS(const QString &key) const
{
    if (!decoder)
        return QString::null;
    return decoder->GetXDS(key);
}

void MythPlayer::InitForTranscode(bool copyaudio, bool copyvideo)
{
    // Are these really needed?
    SetPlaying(true);
    keyframedist = 30;

    if (!InitVideo())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Unable to initialize video for transcode.");
        SetPlaying(false);
        return;
    }

    framesPlayed = 0;
    framesPlayedExtra = 0;
    ClearAfterSeek();

    if (copyvideo && decoder)
        decoder->SetRawVideoState(true);
    if (copyaudio && decoder)
        decoder->SetRawAudioState(true);

    if (decoder)
    {
        decoder->SetSeekSnap(0);
        decoder->SetLowBuffers(true);
    }
}

bool MythPlayer::TranscodeGetNextFrame(
    frm_dir_map_t::iterator &dm_iter,
    int &did_ff, bool &is_key, bool honorCutList)
{
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
        player_ctx->playingInfo->UpdateInUseMark();
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    int64_t lastDecodedFrameNumber =
        videoOutput->GetLastDecodedFrame()->frameNumber;

    if ((lastDecodedFrameNumber == 0) && honorCutList)
        deleteMap.TrackerReset(0);

    if (!decoderThread)
        DecoderStart(true/*start paused*/);

    if (!decoder)
        return false;

    if (!decoder->GetFrame(kDecodeAV))
        return false;

    if (GetEof() != kEofStateNone)
        return false;

    if (honorCutList && !deleteMap.IsEmpty())
    {
        if (totalFrames && lastDecodedFrameNumber >= (int64_t)totalFrames)
            return false;

        uint64_t jumpto = 0;
        if (deleteMap.TrackerWantsToJump(lastDecodedFrameNumber, jumpto))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Fast-Forwarding from %1 to %2")
                    .arg(lastDecodedFrameNumber).arg(jumpto));
            if (jumpto >= totalFrames)
                return false;

            // For 0.25, move this to DoJumpToFrame(jumpto)
            WaitForSeek(jumpto, 0);
            decoder->ClearStoredData();
            ClearAfterSeek();
            decoder->GetFrame(kDecodeAV);
            did_ff = 1;
        }
    }
    if (GetEof() != kEofStateNone)
      return false;
    is_key = decoder->IsLastFrameKey();

    videofiltersLock.lock();
    if (videoFilters)
    {
        FrameScanType ps = m_scan;
        if (kScan_Detect == m_scan || kScan_Ignore == m_scan)
            ps = kScan_Progressive;

        videoFilters->ProcessFrame(videoOutput->GetLastDecodedFrame(), ps);
    }
    videofiltersLock.unlock();

    return true;
}

long MythPlayer::UpdateStoredFrameNum(long curFrameNum)
{
    if (decoder)
        return decoder->UpdateStoredFrameNum(curFrameNum);
    return 0;
}

void MythPlayer::SetCutList(const frm_dir_map_t &newCutList)
{
    deleteMap.SetMap(newCutList);
}

bool MythPlayer::WriteStoredData(RingBuffer *outRingBuffer,
                                 bool writevideo, long timecodeOffset)
{
    if (!decoder)
        return false;
    if (writevideo && !decoder->GetRawVideoState())
        writevideo = false;
    decoder->WriteStoredData(outRingBuffer, writevideo, timecodeOffset);
    return writevideo;
}

void MythPlayer::SetCommBreakMap(frm_dir_map_t &newMap)
{
    commBreakMap.SetMap(newMap, framesPlayed);
    forcePositionMapSync = true;
}

int MythPlayer::GetStatusbarPos(void) const
{
    double spos = 0.0;

    if (livetv || IsWatchingInprogress())
    {
        spos = 1000.0 * framesPlayed / player_ctx->recorder->GetFramesWritten();
    }
    else if (totalFrames)
    {
        spos = 1000.0 * framesPlayed / totalFrames;
    }

    return((int)spos);
}

void MythPlayer::GetPlaybackData(InfoMap &infoMap)
{
    QString samplerate = RingBuffer::BitrateToString(audio.GetSampleRate(),
                                                     true);
    infoMap.insert("samplerate",  samplerate);
    infoMap.insert("filename",    player_ctx->buffer->GetSafeFilename());
    infoMap.insert("decoderrate", player_ctx->buffer->GetDecoderRate());
    infoMap.insert("storagerate", player_ctx->buffer->GetStorageRate());
    infoMap.insert("bufferavail", player_ctx->buffer->GetAvailableBuffer());
    infoMap.insert("buffersize",
        QString::number(player_ctx->buffer->GetBufferSize() >> 20));
    infoMap.insert("avsync",
            QString::number((float)avsync_avg / (float)frame_interval, 'f', 2));
    if (videoOutput)
    {
        QString frames = QString("%1/%2").arg(videoOutput->ValidVideoFrames())
                                         .arg(videoOutput->FreeVideoFrames());
        infoMap.insert("videoframes", frames);
    }
    if (decoder)
        infoMap["videodecoder"] = decoder->GetCodecDecoderName();
    if (output_jmeter)
    {
        infoMap["framerate"] = QString("%1%2%3")
            .arg(output_jmeter->GetLastFPS(), 0, 'f', 2)
            .arg(QChar(0xB1, 0))
            .arg(output_jmeter->GetLastSD(), 0, 'f', 2);
        infoMap["load"] = output_jmeter->GetLastCPUStats();
    }
    GetCodecDescription(infoMap);
}

int MythPlayer::GetSecondsBehind(void) const
{
    if (!player_ctx->recorder)
        return 0;

    long long written = player_ctx->recorder->GetFramesWritten();
    long long played = framesPlayed;

    if (played > written)
        played = written;
    if (played < 0)
        played = 0;

    return (int)((float)(written - played) / video_frame_rate);
}

int64_t MythPlayer::GetSecondsPlayed(bool honorCutList, int divisor) const
{
    int64_t pos = TranslatePositionFrameToMs(framesPlayed, honorCutList);
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC +
        QString("GetSecondsPlayed: framesPlayed %1, honorCutList %2, divisor %3, pos %4")
        .arg(framesPlayed).arg(honorCutList).arg(divisor).arg(pos));
    return TranslatePositionFrameToMs(framesPlayed, honorCutList) / divisor;
}

int64_t MythPlayer::GetTotalSeconds(bool honorCutList, int divisor) const
{
    uint64_t pos = totalFrames;

    if (IsWatchingInprogress())
        pos = (uint64_t)-1;

    return TranslatePositionFrameToMs(pos, honorCutList) / divisor;
}

// Returns the total frame count, as totalFrames for a completed
// recording, or the most recent frame count from the recorder for
// live TV or an in-progress recording.
uint64_t MythPlayer::GetCurrentFrameCount(void) const
{
    uint64_t result = totalFrames;
    if (IsWatchingInprogress())
        result = player_ctx->recorder->GetFramesWritten();
    return result;
}

// Finds the frame number associated with the given time offset.  A
// positive offset or +0.0f indicate offset from the beginning.  A
// negative offset or -0.0f indicate offset from the end.  Limit the
// result to within bounds of the video.
uint64_t MythPlayer::FindFrame(float offset, bool use_cutlist) const
{
    uint64_t length_ms = TranslatePositionFrameToMs(totalFrames, use_cutlist);
    uint64_t position_ms;
    if (signbit(offset))
    {
        uint64_t offset_ms = -offset * 1000 + 0.5;
        position_ms = (offset_ms > length_ms) ? 0 : length_ms - offset_ms;
    }
    else
    {
        position_ms = offset * 1000 + 0.5;
        position_ms = min(position_ms, length_ms);
    }
    return TranslatePositionMsToFrame(position_ms, use_cutlist);
}

void MythPlayer::calcSliderPos(osdInfo &info, bool paddedFields)
{
    if (!decoder)
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

    if (decoder->GetCodecDecoderName() == "nuppel")
    {
        playbackLen = totalLength;
        fixed_playbacklen = true;
    }

    if (livetv && player_ctx->tvchain)
    {
        info.values["progbefore"] = (int)player_ctx->tvchain->HasPrev();
        info.values["progafter"]  = (int)player_ctx->tvchain->HasNext();
        playbackLen = player_ctx->tvchain->GetLengthAtCurPos();
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
        playbackLen = max(playbackLen, 0);
        secsplayed = min(playbackLen, max(secsplayed, 0));

        if (playbackLen > 0)
            pos = (int)(1000.0f * (secsplayed / (float)playbackLen));

        info.values.insert(relPrefix + "secondsplayed", secsplayed);
        info.values.insert(relPrefix + "totalseconds", playbackLen);
        info.values[relPrefix + "position"] = pos;

        int phours = (int)secsplayed / 3600;
        int pmins = ((int)secsplayed - phours * 3600) / 60;
        int psecs = ((int)secsplayed - phours * 3600 - pmins * 60);

        int shours = playbackLen / 3600;
        int smins = (playbackLen - shours * 3600) / 60;
        int ssecs = (playbackLen - shours * 3600 - smins * 60);

        int secsbehind = max((playbackLen - (int) secsplayed), 0);
        int sbhours = secsbehind / 3600;
        int sbmins = (secsbehind - sbhours * 3600) / 60;
        int sbsecs = (secsbehind - sbhours * 3600 - sbmins * 60);

        QString text1, text2, text3;
        if (paddedFields)
        {
            text1.sprintf("%02d:%02d:%02d", phours, pmins, psecs);
            text2.sprintf("%02d:%02d:%02d", shours, smins, ssecs);
            text3.sprintf("%02d:%02d:%02d", sbhours, sbmins, sbsecs);
        }
        else
        {
            if (shours > 0)
            {
                text1.sprintf("%d:%02d:%02d", phours, pmins, psecs);
                text2.sprintf("%d:%02d:%02d", shours, smins, ssecs);
            }
            else
            {
                text1.sprintf("%d:%02d", pmins, psecs);
                text2.sprintf("%d:%02d", smins, ssecs);
            }

            if (sbhours > 0)
            {
                text3.sprintf("%d:%02d:%02d", sbhours, sbmins, sbsecs);
            }
            else if (sbmins > 0)
            {
                text3.sprintf("%d:%02d", sbmins, sbsecs);
            }
            else
            {
                text3 = tr("%n second(s)", "", sbsecs);
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
    if (position == (uint64_t)-1 &&
        player_ctx->recorder && player_ctx->recorder->IsValidRecorder())
    {
        float recorderFrameRate = player_ctx->recorder->GetFrameRate();
        if (recorderFrameRate > 0)
            frameRate = recorderFrameRate;
        position = player_ctx->recorder->GetFramesWritten();
    }
    return deleteMap.TranslatePositionFrameToMs(position, frameRate,
                                                use_cutlist);
}

int MythPlayer::GetNumChapters()
{
    if (decoder)
        return decoder->GetNumChapters();
    return 0;
}

int MythPlayer::GetCurrentChapter()
{
    if (decoder)
        return decoder->GetCurrentChapter(framesPlayed);
    return 0;
}

void MythPlayer::GetChapterTimes(QList<long long> &times)
{
    if (decoder)
        return decoder->GetChapterTimes(times);
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
        jumpchapter = 0;
        return false;
    }

    DoJumpToFrame(desiredFrame, kInaccuracyNone);
    jumpchapter = 0;
    return true;
}

int64_t MythPlayer::GetChapter(int chapter)
{
    if (decoder)
        return decoder->GetChapter(chapter);
    return 0;
}

InteractiveTV *MythPlayer::GetInteractiveTV(void)
{
#ifdef USING_MHEG
    if (!interactiveTV && itvEnabled)
    {
        QMutexLocker locker1(&osdLock);
        QMutexLocker locker2(&itvLock);
        interactiveTV = new InteractiveTV(this);
    }
#endif // USING_MHEG
    return interactiveTV;
}

bool MythPlayer::ITVHandleAction(const QString &action)
{
    bool result = false;

#ifdef USING_MHEG
    if (!GetInteractiveTV())
        return result;

    QMutexLocker locker(&itvLock);
    result = interactiveTV->OfferKey(action);
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

    QMutexLocker locker(&itvLock);
    interactiveTV->Restart(chanid, cardid, isLiveTV);
    itvVisible = false;
#endif // USING_MHEG
}

void MythPlayer::SetVideoResize(const QRect &videoRect)
{
    if (videoOutput)
        videoOutput->SetVideoResize(videoRect);
}

/** \fn MythPlayer::SetAudioByComponentTag(int tag)
 *  \brief Selects the audio stream using the DVB component tag.
 */
bool MythPlayer::SetAudioByComponentTag(int tag)
{
    QMutexLocker locker(&decoder_change_lock);
    if (decoder)
        return decoder->SetAudioByComponentTag(tag);
    return false;
}

/** \fn MythPlayer::SetVideoByComponentTag(int tag)
 *  \brief Selects the video stream using the DVB component tag.
 */
bool MythPlayer::SetVideoByComponentTag(int tag)
{
    QMutexLocker locker(&decoder_change_lock);
    if (decoder)
        return decoder->SetVideoByComponentTag(tag);
    return false;
}

static inline double SafeFPS(DecoderBase *decoder)
{
    if (!decoder)
        return 25;
    double fps = decoder->GetFPS();
    return fps > 0 ? fps : 25.0;
}

// Called from MHIContext::Begin/End/Stream on the MHIContext::StartMHEGEngine thread
bool MythPlayer::SetStream(const QString &stream)
{
    // The stream name is empty if the stream is closing
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("SetStream '%1'").arg(stream));

    QMutexLocker locker(&itvLock);
    m_newStream = stream;
    m_newStream.detach();
    // Stream will be changed by JumpToStream called from EventLoop
    // If successful will call interactiveTV->StreamStarted();

    if (stream.isEmpty() && player_ctx->tvchain &&
        player_ctx->buffer->GetType() == ICRingBuffer::kRingBufferType)
    {
        // Restore livetv
        SetEof(kEofStateDelayed);
        player_ctx->tvchain->JumpToNext(false, 1);
        player_ctx->tvchain->JumpToNext(true, 1);
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

    if (player_ctx->buffer->GetType() != ICRingBuffer::kRingBufferType)
        player_ctx->buffer = new ICRingBuffer(stream, player_ctx->buffer);
    else
        player_ctx->buffer->OpenFile(stream);

    if (!player_ctx->buffer->IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "JumpToStream buffer OpenFile failed");
        SetEof(kEofStateImmediate);
        SetErrored(tr("Error opening remote stream buffer"));
        return;
    }

    watchingrecording = false;
    totalLength = 0;
    totalFrames = 0;
    totalDuration = 0;

    if (OpenFile(120) < 0) // 120 retries ~= 60 seconds
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "JumpToStream OpenFile failed.");
        SetEof(kEofStateImmediate);
        SetErrored(tr("Error opening remote stream"));
        return;
    }

    if (totalLength == 0)
    {
        long long len = player_ctx->buffer->GetRealFileSize();
        totalLength = (int)(len / ((decoder->GetRawBitrate() * 1000) / 8));
        totalFrames = (int)(totalLength * SafeFPS(decoder));
    }
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("JumpToStream length %1 bytes @ %2 Kbps = %3 Secs, %4 frames @ %5 fps")
        .arg(player_ctx->buffer->GetRealFileSize()).arg(decoder->GetRawBitrate())
        .arg(totalLength).arg(totalFrames).arg(decoder->GetFPS()) );

    SetEof(kEofStateNone);

    // the bitrate is reset by player_ctx->buffer->OpenFile()...
    player_ctx->buffer->UpdateRawBitrate(decoder->GetRawBitrate());
    decoder->SetProgramInfo(pginfo);

    Play();
    ChangeSpeed();

    player_ctx->SetPlayerChangingBuffers(false);
#ifdef USING_MHEG
    if (interactiveTV) interactiveTV->StreamStarted();
#endif

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "JumpToStream - end");
}

long MythPlayer::GetStreamPos()
{
    return (long)((1000 * GetFramesPlayed()) / SafeFPS(decoder));
}

long MythPlayer::GetStreamMaxPos()
{
    long maxpos = (long)(1000 * (totalDuration > 0 ? totalDuration : totalLength));
    long pos = GetStreamPos();
    return maxpos > pos ? maxpos : pos;
}

long MythPlayer::SetStreamPos(long ms)
{
    uint64_t frameNum = (uint64_t)((ms * SafeFPS(decoder)) / 1000);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("SetStreamPos %1 mS = frame %2, now=%3")
        .arg(ms).arg(frameNum).arg(GetFramesPlayed()) );
    JumpToFrame(frameNum);
    return ms;
}

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
    totalDecoderPause = true;
    PauseDecoder();

    {
        while (!decoder_change_lock.tryLock(10))
            LOG(VB_GENERAL, LOG_INFO, LOC + "Waited 10ms for decoder lock");

        if (!decoder)
            decoder = dec;
        else
        {
            DecoderBase *d = decoder;
            decoder = dec;
            delete d;
        }
        decoder_change_lock.unlock();
    }
    syncWithAudioStretch();
    totalDecoderPause = false;
}

bool MythPlayer::PosMapFromEnc(uint64_t start,
                               frm_pos_map_t &posMap,
                               frm_pos_map_t &durMap)
{
    // Reads only new positionmap entries from encoder
    if (!(livetv || (player_ctx->recorder &&
                     player_ctx->recorder->IsValidRecorder())))
        return false;

    // if livetv, and we're not the last entry, don't get it from the encoder
    if (HasTVChainNext())
        return false;

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Filling position map from %1 to %2") .arg(start).arg("end"));

    player_ctx->recorder->FillPositionMap(start, -1, posMap);
    player_ctx->recorder->FillDurationMap(start, -1, durMap);

    return true;
}

void MythPlayer::SetErrored(const QString &reason)
{
    QMutexLocker locker(&errorLock);

    if (videoOutput)
        errorType |= videoOutput->GetError();

    if (errorMsg.isEmpty())
    {
        errorMsg = reason;
        errorMsg.detach();
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("%1").arg(reason));
    }
}

void MythPlayer::ResetErrored(void)
{
    QMutexLocker locker(&errorLock);

    errorMsg = QString();
}

bool MythPlayer::IsErrored(void) const
{
    QMutexLocker locker(&errorLock);
    return !errorMsg.isEmpty();
}

QString MythPlayer::GetError(void) const
{
    QMutexLocker locker(&errorLock);
    QString tmp = errorMsg;
    tmp.detach();
    return tmp;
}

void MythPlayer::ToggleStudioLevels(void)
{
    if (!videoOutput)
        return;

    if (!(videoOutput->GetSupportedPictureAttributes() &
          kPictureAttributeSupported_StudioLevels))
        return;

    int val = videoOutput->GetPictureAttribute(kPictureAttribute_StudioLevels);
    val = (val > 0) ? 0 : 1;
    videoOutput->SetPictureAttribute(kPictureAttribute_StudioLevels, val);
    QString msg = (val > 0) ? tr("Enabled Studio Levels") :
                              tr("Disabled Studio Levels");
    SetOSDMessage(msg, kOSDTimeout_Med);
}

void MythPlayer::ToggleNightMode(void)
{
    if (!videoOutput)
        return;

    if (!(videoOutput->GetSupportedPictureAttributes() &
          kPictureAttributeSupported_Brightness))
        return;

    int b = videoOutput->GetPictureAttribute(kPictureAttribute_Brightness);
    int c = 0;
    bool has_contrast = (videoOutput->GetSupportedPictureAttributes() &
                         kPictureAttributeSupported_Contrast);
    if (has_contrast)
        c = videoOutput->GetPictureAttribute(kPictureAttribute_Contrast);

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
    videoOutput->SetPictureAttribute(kPictureAttribute_Brightness, b);
    if (has_contrast)
        videoOutput->SetPictureAttribute(kPictureAttribute_Contrast, c);

    SetOSDMessage(msg, kOSDTimeout_Med);
}

bool MythPlayer::CanVisualise(void)
{
    if (videoOutput)
        return videoOutput->
            CanVisualise(&audio, GetMythMainWindow()->GetRenderDevice());
    return false;
}

bool MythPlayer::IsVisualising(void)
{
    if (videoOutput)
        return videoOutput->GetVisualisation();
    return false;
}

QString MythPlayer::GetVisualiserName(void)
{
    if (videoOutput)
        return videoOutput->GetVisualiserName();
    return QString("");
}

QStringList MythPlayer::GetVisualiserList(void)
{
    if (videoOutput)
        return videoOutput->GetVisualiserList();
    return QStringList();
}

bool MythPlayer::EnableVisualisation(bool enable, const QString &name)
{
    if (videoOutput)
        return videoOutput->EnableVisualisation(&audio, enable, name);
    return false;
}

void MythPlayer::SetOSDMessage(const QString &msg, OSDTimeout timeout)
{
    QMutexLocker locker(&osdLock);
    if (!osd)
        return;

    InfoMap info;
    info.insert("message_text", msg);
    osd->SetText("osd_message", info, timeout);
}

void MythPlayer::SetOSDStatus(const QString &title, OSDTimeout timeout)
{
    QMutexLocker locker(&osdLock);
    if (!osd)
        return;

    osdInfo info;
    calcSliderPos(info);
    info.text.insert("title", title);
    osd->SetText("osd_status", info.text, timeout);
    osd->SetValues("osd_status", info.values, timeout);
}

void MythPlayer::SaveTotalDuration(void)
{
    if (!decoder)
        return;

    decoder->SaveTotalDuration();
}

void MythPlayer::ResetTotalDuration(void)
{
    if (!decoder)
        return;

    decoder->ResetTotalDuration();
}

void MythPlayer::SaveTotalFrames(void)
{
    if (!decoder)
        return;

    decoder->SaveTotalFrames();
}

void MythPlayer::syncWithAudioStretch()
{
    if (decoder && audio.HasAudioOut())
    {
        float stretch = audio.GetStretchFactor();
        bool disable = (stretch < 0.99f) || (stretch > 1.01f);
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Stretch Factor %1, %2 passthru ")
            .arg(audio.GetStretchFactor())
            .arg((disable) ? "disable" : "allow"));
        decoder->SetDisablePassThrough(disable);
    }
    return;
}

static unsigned dbg_ident(const MythPlayer *player)
{
    static QMutex   dbg_lock;
    static unsigned dbg_next_ident = 0;
    typedef QMap<const MythPlayer*, unsigned> DbgMapType;
    static DbgMapType dbg_ident;

    QMutexLocker locker(&dbg_lock);
    DbgMapType::iterator it = dbg_ident.find(player);
    if (it != dbg_ident.end())
        return *it;
    return dbg_ident[player] = dbg_next_ident++;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */

