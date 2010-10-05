// -*- Mode: c++ -*-

#undef HAVE_AV_CONFIG_H

// Std C headers
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cerrno>
#include <ctime>
#include <cmath>

// POSIX headers
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/time.h>

// C++ headers
#include <algorithm>
#include <iostream>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QThread>
#include <QKeyEvent>
#include <QDir>

// MythTV headers
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
#include "util.h"
#include "decodeencode.h"
#include "livetvchain.h"
#include "decoderbase.h"
#include "nuppeldecoder.h"
#include "avformatdecoder.h"
#include "dummydecoder.h"
#include "jobqueue.h"
#include "NuppelVideoRecorder.h"
#include "tv_play.h"
#include "interactivetv.h"
#include "mythverbose.h"
#include "myth_imgconvert.h"
#include "mythsystemevent.h"
#include "mythpainter.h"
#include "mythimage.h"
#include "mythuiimage.h"

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

#if CONFIG_DARWIN
extern "C" {
int isnan(double);
}
#endif

static unsigned dbg_ident(const MythPlayer*);

#define LOC      QString("Player(%1): ").arg(dbg_ident(this),0,36)
#define LOC_WARN QString("Player(%1), Warning: ").arg(dbg_ident(this),0,36)
#define LOC_ERR  QString("Player(%1), Error: ").arg(dbg_ident(this),0,36)
#define LOC_DEC  QString("Player(%1): ").arg(dbg_ident(m_mp),0,36)

QEvent::Type PlayerTimer::kPlayerEventType =
    (QEvent::Type) QEvent::registerEventType();

PlayerTimer::PlayerTimer(MythPlayer *player) : m_mp(player), m_queue_size(0)
{
    if (!m_mp)
        VERBOSE(VB_IMPORTANT, QString("PlayerTimer has no parent."));
    PostNextEvent();
}

void PlayerTimer::PostNextEvent(void)
{
    QEvent *event = new QEvent(kPlayerEventType);
    qApp->postEvent(this, event);
    m_queue_size++;
}

bool PlayerTimer::event(QEvent *e)
{
    if (e->type() == kPlayerEventType)
    {
        // TODO this may fail if events are lost and the queue size is wrong
        m_queue_size--;
        PostNextEvent();
        while (m_queue_size < 3)
            PostNextEvent();

        if (m_mp && !m_mp->IsErrored())
        {
            m_mp->EventLoop();
            m_mp->VideoLoop();
        }
        return true;
    }
    return false;
}

void DecoderThread::run(void)
{
    if (!m_mp)
        return;

    VERBOSE(VB_PLAYBACK, LOC_DEC + QString("Decoder thread starting."));
    m_mp->DecoderLoop(m_start_paused);
    VERBOSE(VB_PLAYBACK, LOC_DEC + QString("Decoder thread exiting."));
}

static const int toCaptionType(int type)
{
    if (kTrackTypeCC608 == type)            return kDisplayCC608;
    if (kTrackTypeCC708 == type)            return kDisplayCC708;
    if (kTrackTypeSubtitle == type)         return kDisplayAVSubtitle;
    if (kTrackTypeTeletextCaptions == type) return kDisplayTeletextCaptions;
    if (kTrackTypeTextSubtitle == type)     return kDisplayTextSubtitle;
    if (kTrackTypeRawText == type)          return kDisplayRawTextSubtitle;
    return 0;
}

static const int toTrackType(int type)
{
    if (kDisplayCC608 == type)            return kTrackTypeCC608;
    if (kDisplayCC708 == type)            return kTrackTypeCC708;
    if (kDisplayAVSubtitle == type)       return kTrackTypeSubtitle;
    if (kDisplayTeletextCaptions == type) return kTrackTypeTeletextCaptions;
    if (kDisplayTextSubtitle == type)     return kTrackTypeTextSubtitle;
    if (kDisplayRawTextSubtitle == type)  return kTrackTypeRawText;
    return 0;
}

MythPlayer::MythPlayer(bool muted)
    : decoder(NULL),                decoder_change_lock(QMutex::Recursive),
      videoOutput(NULL),            player_ctx(NULL),
      decoderThread(NULL),          playerThread(NULL),
      playerTimer(NULL),
      no_hardware_decoders(false),
      // Window stuff
      parentWidget(NULL), embedid(0),
      embx(-1), emby(-1), embw(-1), embh(-1),
      // State
      decoderPaused(false), pauseDecoder(false), unpauseDecoder(false),
      killdecoder(false),   decoderSeek(-1),     decodeOneFrame(false),
      needNewPauseFrame(false),
      bufferPaused(false),  videoPaused(false),
      allpaused(false),     playing(false),

      eof(false),
      m_double_framerate(false),    m_double_process(false),
      m_can_double(false),          m_deint_possible(true),
      livetv(false),
      watchingrecording(false),     using_null_videoout(false),
      transcoding(false),
      hasFullPositionMap(false),    limitKeyRepeat(false),
      errorMsg(QString::null),      errorType(kError_None),
      // Chapter stuff
      jumpchapter(0),
      // Bookmark stuff
      bookmarkseek(0),
      // Seek
      fftime(0),                    exactseeks(false),
      // Playback misc.
      videobuf_retries(0),          framesPlayed(0),
      totalFrames(0),               totalLength(0),
      rewindtime(0),
      // Input Video Attributes
      video_disp_dim(0,0), video_dim(0,0),
      video_frame_rate(29.97f), video_aspect(4.0f / 3.0f),
      forced_video_aspect(-1),
      m_scan(kScan_Interlaced),     m_scan_locked(false),
      m_scan_tracker(0),            m_scan_initialized(false),
      keyframedist(30),             noVideoTracks(false),
      // Prebuffering
      buffering(false), buffering_start(),
      // General Caption/Teletext/Subtitle support
      textDisplayMode(kDisplayNone),
      prevTextDisplayMode(kDisplayNone),
      // Support for analog captions and teletext
      vbimode(VBIMode::None),
      ttPageNum(0x888),
      // Support for captions, teletext, etc. decoded by libav
      textDesired(false), tracksChanged(false), initTeletext(false),
      // CC608/708
      db_prefer708(true), cc608(this), cc708(this),
      // MHEG/MHI Interactive TV visible in OSD
      itvVisible(false),
      interactiveTV(NULL),
      itvEnabled(false),
      // OSD stuff
      osd(NULL), reinit_osd(false),
      // Audio
      audio(this, muted),
      // Picture-in-Picture stuff
      pip_active(false),            pip_visible(true),
      // Filters
      videoFiltersForProgram(""),   videoFiltersOverride(""),
      postfilt_width(0),            postfilt_height(0),
      videoFilters(NULL),           FiltMan(new FilterManager()),

      forcePositionMapSync(false),  pausedBeforeEdit(false),
      // Playback (output) speed control
      decoder_lock(QMutex::Recursive),
      next_play_speed(1.0f),        next_normal_speed(true),
      play_speed(1.0f),             normal_speed(true),
      frame_interval((int)(1000000.0f / 30)), m_frame_interval(0),
      ffrew_skip(1),
      // Audio and video synchronization stuff
      videosync(NULL),              avsync_delay(0),
      avsync_adjustment(0),         avsync_avg(0),
      refreshrate(0),
      lastsync(false),              repeat_delay(0),
      // Time Code stuff
      prevtc(0),                    prevrp(0),
      savedAudioTimecodeOffset(0),
      // LiveTVChain stuff
      m_tv(NULL),                   isDummy(false),
      // Debugging variables
      output_jmeter(NULL)
{
    playerThread = QThread::currentThread();
    // Playback (output) zoom control
    detect_letter_box = new DetectLetterbox(this);

    vbimode = VBIMode::Parse(gCoreContext->GetSetting("VbiFormat"));
    decode_extra_audio = gCoreContext->GetNumSetting("DecodeExtraAudio", 0);
    itvEnabled         = gCoreContext->GetNumSetting("EnableMHEG", 0);
    db_prefer708       = gCoreContext->GetNumSetting("Prefer708Captions", 1);

    bzero(&tc_lastval, sizeof(tc_lastval));
    bzero(&tc_wrap,    sizeof(tc_wrap));

    // Get VBI page number
    QString mypage = gCoreContext->GetSetting("VBIpageNr", "888");
    bool valid = false;
    uint tmp = mypage.toInt(&valid, 16);
    ttPageNum = (valid) ? tmp : ttPageNum;
    cc608.SetTTPageNum(ttPageNum);
}

MythPlayer::~MythPlayer(void)
{
    QMutexLocker lk1(&vidExitLock);
    QMutexLocker lk2(&videofiltersLock);

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

    if (playerTimer)
    {
        delete playerTimer;
        playerTimer = NULL;
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
    QMutexLocker locker(&decoder_change_lock);

    watchingrecording = mode;
    if (GetDecoder())
        GetDecoder()->setWatchingRecording(mode);
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
        VERBOSE(VB_PLAYBACK, LOC + "Waited 100ms to get pause lock.");
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
    PauseDecoder();
    PauseVideo();
    audio.Pause(true);
    PauseBuffer();
    allpaused = decoderPaused && videoPaused && bufferPaused;
    QMutexLocker locker(&decoder_change_lock);
    if (GetDecoder() && videoOutput)
    {
        if (using_null_videoout)
            GetDecoder()->UpdateFramesPlayed();
        else
            framesPlayed = videoOutput->GetFramesPlayed();
    }
    pauseLock.unlock();
    return already_paused;
}

bool MythPlayer::Play(float speed, bool normal, bool unpauseaudio)
{
    pauseLock.lock();
    VERBOSE(VB_PLAYBACK, LOC +
            QString("Play(%1, normal %2, unpause audio %3)")
            .arg(speed,5,'f',1).arg(normal).arg(unpauseaudio));

    if (deleteMap.IsEditing())
    {
        VERBOSE(VB_IMPORTANT, LOC + "Ignoring Play(), in edit mode.");
        pauseLock.unlock();
        return false;
    }

    UnpauseBuffer();
    if (unpauseaudio)
        audio.Pause(false);
    UnpauseVideo();
    UnpauseDecoder();
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
    assert(player_ctx);
    if (!player_ctx)
        return false;

    PIPState pipState = player_ctx->GetPIPState();

    if (using_null_videoout && GetDecoder())
    {
        MythCodecID codec = GetDecoder()->GetVideoCodecID();
        videoOutput = new VideoOutputNull();
        if (!videoOutput->Init(video_disp_dim.width(), video_disp_dim.height(),
                               video_aspect, 0, 0, 0, 0, 0, codec, 0))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Unable to create null video out");
            SetErrored(QObject::tr("Unable to create null video out"));
            return false;
        }
    }
    else
    {
        QWidget *widget = parentWidget;

        if (!widget)
        {
            MythMainWindow *window = GetMythMainWindow();

            widget = window->findChild<QWidget*>("video playback window");

            if (!widget)
            {
                VERBOSE(VB_IMPORTANT, "Couldn't find 'tv playback' widget");
                widget = window->currentWidget();
            }
        }

        if (!widget)
        {
            VERBOSE(VB_IMPORTANT, "Couldn't find 'tv playback' widget. "
                    "Current widget doesn't exist. Exiting..");
            SetErrored(QObject::tr("'tv playback' widget missing."));
            return false;
        }

        QRect display_rect;
        if (pipState == kPIPStandAlone)
            display_rect = QRect(embx, emby, embw, embh);
        else
            display_rect = QRect(0, 0, widget->width(), widget->height());

        if (GetDecoder())
        {
            videoOutput = VideoOutput::Create(
                GetDecoder()->GetCodecDecoderName(),
                GetDecoder()->GetVideoCodecID(),
                GetDecoder()->GetVideoCodecPrivate(),
                pipState,
                video_disp_dim, video_aspect,
                widget->winId(), display_rect, (video_frame_rate * play_speed),
                0 /*embedid*/);
        }

        if (videoOutput)
        {
            videoOutput->SetVideoScalingAllowed(true);
            // We need to tell it this for automatic deinterlacer settings
            videoOutput->SetVideoFrameRate(video_frame_rate * play_speed);
            CheckExtraAudioDecode();
        }
    }

    if (!videoOutput)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Couldn't create VideoOutput instance. Exiting..");
        SetErrored(QObject::tr("Failed to initialize video output"));
        return false;
    }

    if (embedid > 0 && pipState == kPIPOff)
    {
        videoOutput->EmbedInWidget(embx, emby, embw, embh);
    }

    InitFilters();

    return true;
}

void MythPlayer::CheckExtraAudioDecode(void)
{
    if (using_null_videoout)
        return;

    bool force = false;
    if (videoOutput && videoOutput->NeedExtraAudioDecode())
    {
        VERBOSE(VB_IMPORTANT, LOC +
                "Forcing decode extra audio option on"
                " (Video method requires it).");
        force = true;
    }

    if (GetDecoder())
        GetDecoder()->SetLowBuffers(decode_extra_audio || force);
}

void MythPlayer::ReinitOSD(void)
{
    if (videoOutput && !using_null_videoout)
    {
        if (QThread::currentThread() != (QThread*)playerThread)
        {
            reinit_osd = true;
            return;
        }
        QRect visible, total;
        float aspect, scaling;
        if (osd)
        {
            videoOutput->GetOSDBounds(total, visible, aspect,
                                      scaling, 1.0f);
            if (osd->Bounds() != visible)
            {
                bool audio_paused = audio.IsPaused();
                bool was_paused = Pause();
                uint old = textDisplayMode;
                ToggleCaptions(old);
                osd->Reinit(visible, aspect);
                SetupTeletextViewer();
                EnableCaptions(old, false);
                if (!was_paused)
                    Play(play_speed, normal_speed, !audio_paused);
            }
        }

#ifdef USING_MHEG
        if (GetInteractiveTV())
        {
            total = videoOutput->GetMHEGBounds();
            GetInteractiveTV()->Reinit(total);
            itvVisible = false;
        }
#endif // USING_MHEG
        reinit_osd = false;
    }
}

void MythPlayer::ReinitVideo(void)
{
    if (!videoOutput->IsPreferredRenderer(video_disp_dim))
    {
        VERBOSE(VB_PLAYBACK, LOC + QString("Need to switch video renderer."));
        SetErrored(QObject::tr("Need to switch video renderer."));
        errorType |= kError_Switch_Renderer;
        return;
    }

    bool aspect_only = false;
    {
        QMutexLocker locker1(&vidExitLock);
        QMutexLocker locker2(&videofiltersLock);
        float aspect = (forced_video_aspect > 0) ? forced_video_aspect :
                                               video_aspect;
        if (!videoOutput->InputChanged(video_disp_dim, aspect,
                                       GetDecoder()->GetVideoCodecID(),
                                       GetDecoder()->GetVideoCodecPrivate(),
                                       aspect_only))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to Reinitialize Video. Exiting..");
            SetErrored(QObject::tr("Failed to reinitialize video output"));
            return;
        }

        // We need to tell it this for automatic deinterlacer settings
        videoOutput->SetVideoFrameRate(video_frame_rate * play_speed);
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
        DisableCaptions(textDisplayMode, false);
        SetCaptionsEnabled(true, false);
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
        QString(", ") + toQString(scan) + QString(", ") + QString("%1").arg(fps) +
        QString(", ") + QString("%1").arg(video_height) + QString(") ->");

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

    VERBOSE(VB_PLAYBACK, LOC + dbg+toQString(scan));

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
            VERBOSE(VB_PLAYBACK, LOC + "interlaced frame seen after "
                    << abs(m_scan_tracker) << " progressive frames");
            m_scan_tracker = 2;
            if (allow_lock)
            {
                VERBOSE(VB_PLAYBACK, LOC + "Locking scan to Interlaced.");
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
            VERBOSE(VB_PLAYBACK, LOC + "progressive frame seen after "
                    << m_scan_tracker << " interlaced  frames");
            m_scan_tracker = 0;
        }
        m_scan_tracker--;
    }

    if ((m_scan_tracker % 400) == 0)
    {
        QString type = (m_scan_tracker < 0) ? "progressive" : "interlaced";
        VERBOSE(VB_PLAYBACK, LOC + QString("%1 %2 frames seen.")
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

    if (!videoOutput || !videosync)
        return; // hopefully this will be called again later...

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
            VERBOSE(VB_IMPORTANT, LOC + "Failed to enable deinterlacing");
            m_scan = scan;
            return;
        }
        if (videoOutput->NeedsDoubleFramerate())
        {
            m_double_framerate = true;
            m_can_double = (frame_interval / 2 > videosync->getRefreshInterval() * 0.995);
            if (!m_can_double)
            {
                VERBOSE(VB_IMPORTANT, LOC + "Video sync method can't support "
                        "double framerate (refresh rate too low for 2x deint)");
                FallbackDeint();
            }
        }
        m_double_process = videoOutput->IsExtraProcessingRequired();
        VERBOSE(VB_PLAYBACK, LOC + "Enabled deinterlacing");
    }
    else
    {
        if (kScan_Progressive == scan)
        {
            m_double_process = false;
            m_double_framerate = false;
            videoOutput->SetDeinterlacingEnabled(false);
            VERBOSE(VB_PLAYBACK, LOC + "Disabled deinterlacing");
        }
    }

    m_scan = scan;
}

void MythPlayer::SetVideoParams(int width, int height, double fps,
                                int keyframedistance, float aspect,
                                FrameScanType scan, bool video_codec_changed)
{
    if (width == 0 || height == 0 || isnan(aspect) || isnan(fps))
        return;

    if ((video_disp_dim == QSize(width, height)) &&
        (video_aspect == aspect) && (video_frame_rate == fps   ) &&
        ((keyframedistance <= 0) ||
         ((uint64_t)keyframedistance == keyframedist)) &&
        !video_codec_changed)
    {
        return;
    }

    if ((width > 0) && (height > 0))
    {
        video_dim      = QSize((width + 15) & ~0xf, (height + 15) & ~0xf);
        video_disp_dim = QSize(width, height);
    }
    video_aspect = (aspect > 0.0f) ? aspect : video_aspect;
    keyframedist = (keyframedistance > 0) ? keyframedistance : keyframedist;

    if (fps > 0.0f && fps < 121.0f)
    {
        video_frame_rate = fps;
        float temp_speed = (play_speed == 0.0f) ?
            audio.GetStretchFactor() : play_speed;
        frame_interval = (int)(1000000.0f / video_frame_rate / temp_speed);
    }

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

void MythPlayer::OpenDummy(void)
{
    isDummy = true;

    float displayAspect =
        gCoreContext->GetFloatSettingOnHost("XineramaMonitorAspectRatio",
                                        gCoreContext->GetHostName(), 1.3333);

    if (!videoOutput)
    {
        SetVideoParams(720, 576, 25.00, 15, displayAspect);
    }

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    DummyDecoder *dec = new DummyDecoder(this, *(player_ctx->playingInfo));
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    SetDecoder(dec);
}

int MythPlayer::OpenFile(uint retries, bool allow_libmpeg2)
{
    isDummy = false;

    assert(player_ctx);
    if (!player_ctx || !player_ctx->buffer)
        return -1;

    livetv = player_ctx->tvchain;

    if (player_ctx->tvchain &&
        player_ctx->tvchain->GetCardType(player_ctx->tvchain->GetCurPos()) ==
        "DUMMY")
    {
        OpenDummy();
        return 0;
    }

    player_ctx->buffer->Start();
    char testbuf[kDecoderProbeBufferSize];
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
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        QString("OpenFile(): Could not read "
                                "first %1 bytes of '%2'")
                        .arg(testreadsize)
                        .arg(player_ctx->buffer->GetFilename()));
                return -1;
            }
            VERBOSE(VB_IMPORTANT, LOC_WARN + "OpenFile() waiting on data");
            usleep(50 * 1000);
        }

        player_ctx->LockPlayingInfo(__FILE__, __LINE__);
        if (NuppelDecoder::CanHandle(testbuf, testreadsize))
            SetDecoder(new NuppelDecoder(this, *player_ctx->playingInfo));
        else if (AvFormatDecoder::CanHandle(
                     testbuf, player_ctx->buffer->GetFilename(), testreadsize))
        {
            bool noaccel = no_hardware_decoders ||
                          (player_ctx->IsPBP() && !player_ctx->IsPrimaryPBP());
            SetDecoder(new AvFormatDecoder(this, *player_ctx->playingInfo,
                                           using_null_videoout,
                                           allow_libmpeg2, noaccel,
                                           player_ctx->GetSpecialDecode()));
        }
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        if (GetDecoder() || (bigTimer.elapsed() > timeout))
            break;
        testreadsize <<= 1;
    }

    if (!GetDecoder())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Couldn't find an A/V decoder for: '%1'")
                .arg(player_ctx->buffer->GetFilename()));

        return -1;
    }
    else if (GetDecoder()->IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Could not initialize A/V decoder.");
        SetDecoder(NULL);
        return -1;
    }

    GetDecoder()->setExactSeeks(exactseeks);
    GetDecoder()->setLiveTVMode(livetv);
    GetDecoder()->setWatchingRecording(watchingrecording);
    GetDecoder()->setTranscoding(transcoding);
    CheckExtraAudioDecode();
    noVideoTracks = GetDecoder() &&
                   !GetDecoder()->GetTrackCount(kTrackTypeVideo);


    eof = false;

    // Set 'no_video_decode' to true for audio only decodeing
    bool no_video_decode = false;

    // We want to locate decoder for video even if using_null_videoout
    // is true, only disable if no_video_decode is true.
    int ret = GetDecoder()->OpenFile(
        player_ctx->buffer, no_video_decode, testbuf, testreadsize);

    if (ret < 0)
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't open decoder for: %1")
                .arg(player_ctx->buffer->GetFilename()));
        return -1;
    }

    audio.CheckFormat();

    if (ret > 0)
    {
        hasFullPositionMap = true;
        deleteMap.LoadMap(totalFrames, player_ctx);
        deleteMap.TrackerReset(0, totalFrames);
    }

    bookmarkseek = GetBookmark();

    if (player_ctx->playingInfo->QueryAutoExpire() == kLiveTVAutoExpire)
        gCoreContext->SaveSetting("DefaultChanid",
                                  player_ctx->playingInfo->GetChanID());

    return IsErrored() ? -1 : 0;
}

void MythPlayer::SetVideoFilters(const QString &override)
{
    videoFiltersOverride = override;
    videoFiltersOverride.detach();

    videoFiltersForProgram = player_ctx->GetFilters(
                             (using_null_videoout) ? "onefield" : "");
}

void MythPlayer::InitFilters(void)
{
    QString filters = "";
    if (videoOutput)
        filters = videoOutput->GetFilters();

    VERBOSE(VB_PLAYBACK|VB_EXTRA, LOC +
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
            if ((filters.length() > 1) && (filters.right(1) != ","))
                filters += ",";
            filters += videoFiltersForProgram.mid(1);
        }
    }

    if (!videoFiltersOverride.isEmpty())
        filters = videoFiltersOverride;

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

    VERBOSE(VB_PLAYBACK, LOC + QString("LoadFilters('%1'..) -> ")
            .arg(filters)<<videoFilters);
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
VideoFrame *MythPlayer::GetNextVideoFrame(bool allow_unsafe)
{
    return videoOutput->GetNextFreeFrame(false, allow_unsafe);
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

    videoOutput->ReleaseFrame(buffer);

    detect_letter_box->Detect(buffer);
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
    videoOutput->DrawSlice(frame, x, y, w, h);
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

void MythPlayer::ReleaseCurrentFrame(VideoFrame *frame)
{
    if (frame)
        vidExitLock.unlock();
}

void MythPlayer::EmbedInWidget(int x, int y, int w, int h, WId id)
{
    if (videoOutput)
        videoOutput->EmbedInWidget(x, y, w, h);
    else
    {
        embx = x;
        emby = y;
        embw = w;
        embh = h;
        embedid = id;
    }
}

void MythPlayer::StopEmbedding(void)
{
    if (videoOutput)
    {
        videoOutput->StopEmbedding();
        ReinitOSD();
    }
}

void MythPlayer::WindowResized(const QSize &new_size)
{
    if (videoOutput)
        videoOutput->WindowResized(new_size);
}

void MythPlayer::EnableTeletext(int page)
{
    if (!GetOSD())
        return;

    GetOSD()->EnableTeletext(true, page);
    prevTextDisplayMode = textDisplayMode;
    textDisplayMode = kDisplayTeletextMenu;
}

void MythPlayer::DisableTeletext(void)
{
    if (!GetOSD())
        return;

    GetOSD()->EnableTeletext(false, 0);
    textDisplayMode = kDisplayNone;

    /* If subtitles are enabled before the teletext menu was displayed,
       re-enabled them. */
    if (prevTextDisplayMode & kDisplayAllCaptions)
        EnableCaptions(prevTextDisplayMode, false);
}

void MythPlayer::ResetTeletext(void)
{
    if (!GetOSD())
        return;
    GetOSD()->TeletextReset();
}

/** \fn MythPlayer::SetTeletextPage(uint)
 *  \brief Set Teletext NUV Caption page
 */
void MythPlayer::SetTeletextPage(uint page)
{
    QMutexLocker locker(&decoder_change_lock);

    DisableCaptions(textDisplayMode);
    ttPageNum = page;
    cc608.SetTTPageNum(ttPageNum);
    textDisplayMode &= ~kDisplayAllCaptions;
    textDisplayMode |= kDisplayNUVTeletextCaptions;
}

bool MythPlayer::HandleTeletextAction(const QString &action)
{
    if (!(textDisplayMode & kDisplayTeletextMenu) || !GetOSD())
        return false;

    bool handled = true;

    if (action == "MENU" || action == "TOGGLETT" || action == "ESCAPE")
        DisableTeletext();
    else
        handled = GetOSD()->TeletextAction(action);

    return handled;
}

void MythPlayer::ResetCaptions(void)
{
    if (GetOSD() && ((textDisplayMode & kDisplayAVSubtitle)      ||
                     (textDisplayMode & kDisplayTextSubtitle)    ||
                     (textDisplayMode & kDisplayRawTextSubtitle) ||
                     (textDisplayMode & kDisplayDVDButton)       ||
                     (textDisplayMode & kDisplayCC608)           ||
                     (textDisplayMode & kDisplayCC708)))
        GetOSD()->ClearSubtitles();
}

// caller has decoder_changed_lock
void MythPlayer::DisableCaptions(uint mode, bool osd_msg)
{
    textDisplayMode &= ~mode;
    ResetCaptions();
    if (!osd)
        return;

    QString msg = "";
    if (kDisplayNUVTeletextCaptions & mode)
        msg += QObject::tr("TXT CAP");
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
        osd->EnableSubtitles(preserve);
    }
    if (kDisplayTextSubtitle & mode)
    {
        msg += QObject::tr("Text subtitles");
        osd->EnableSubtitles(preserve);
    }
    if (!msg.isEmpty() && osd_msg)
    {
        msg += " " + QObject::tr("Off");
        SetOSDMessage(msg, kOSDTimeout_Med);
    }
}

// caller has decoder_changed_lock
void MythPlayer::EnableCaptions(uint mode, bool osd_msg)
{
    QString msg = "";
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
        msg += QObject::tr("Text subtitles");
    }
    if (kDisplayNUVTeletextCaptions & mode)
        msg += QObject::tr("TXT") + QString(" %1").arg(ttPageNum, 3, 16);
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

    msg += " " + QObject::tr("On");

    textDisplayMode = mode;
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
    uint mode = toCaptionType(type);
    uint origMode = textDisplayMode;

    QMutexLocker locker(&decoder_change_lock);

    if (textDisplayMode)
        DisableCaptions(textDisplayMode, origMode & mode);
    if (origMode & mode)
        return textDisplayMode;
    if (mode)
        EnableCaptions(mode);
    return textDisplayMode;
}

void MythPlayer::SetupTeletextViewer(void)
{
    if (QThread::currentThread() != playerThread)
    {
        initTeletext = true;
        return;
    }

    if (GetOSD())
    {
        TeletextViewer* ttview =  (TeletextViewer*)osd->InitTeletext();
        if (ttview && GetDecoder())
        {
            initTeletext = false;
            GetDecoder()->SetTeletextDecoderViewer(ttview);
        }
    }
}

void MythPlayer::SetCaptionsEnabled(bool enable, bool osd_msg)
{
    tracksChanged = false;
    uint origMode = textDisplayMode;

    textDesired = enable;

    QMutexLocker locker(&decoder_change_lock);

    if (!enable)
    {
        DisableCaptions(origMode, osd_msg);
        return;
    }
    int mode = NextCaptionTrack(kDisplayNone);
    if (origMode != (uint)mode)
    {
        DisableCaptions(origMode, false);

        if ((kDisplayNone == mode) && osd_msg)
        {
            SetOSDMessage(QObject::tr(
                "No captions", "CC/Teletext/Subtitle text not available"),
                kOSDTimeout_Med);
        }
        else if (mode)
        {
            EnableCaptions(mode, osd_msg);
        }
        ResetCaptions();
    }
}

QStringList MythPlayer::GetTracks(uint type)
{
    QMutexLocker locker(&decoder_change_lock);
    if (GetDecoder())
        return GetDecoder()->GetTracks(type);
    return QStringList();
}

int MythPlayer::SetTrack(uint type, int trackNo)
{
    int ret = -1;
    QMutexLocker locker(&decoder_change_lock);
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
        tracksChanged = true;
    }
}

int MythPlayer::GetTrack(uint type)
{
    QMutexLocker locker(&decoder_change_lock);
    if (GetDecoder())
        return GetDecoder()->GetTrack(type);
    return -1;
}

int MythPlayer::ChangeTrack(uint type, int dir)
{
    QMutexLocker locker(&decoder_change_lock);
    if (GetDecoder())
    {
        int retval = GetDecoder()->ChangeTrack(type, dir);
        if (retval >= 0)
        {
            SetOSDMessage(GetDecoder()->GetTrackDesc(type, GetTrack(type)),
                          kOSDTimeout_Med);
            return retval;
        }
    }
    return -1;
}

void MythPlayer::ChangeCaptionTrack(int dir)
{
    QMutexLocker locker(&decoder_change_lock);
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

int MythPlayer::NextCaptionTrack(int mode)
{
    // Text->TextStream->708/608->608/708->AVSubs->Teletext->NUV->None
    // NUV only offerred if PAL
    bool pal      = (vbimode == VBIMode::PAL_TT);
    int  nextmode = kDisplayNone;

    if (kDisplayTextSubtitle == mode)
        nextmode = kDisplayRawTextSubtitle;
    if (kDisplayRawTextSubtitle == mode)
        nextmode = db_prefer708 ? kDisplayCC708 : kDisplayCC608;
    else if (kDisplayCC708 == mode)
        nextmode = db_prefer708 ? kDisplayCC608 : kDisplayAVSubtitle;
    else if (kDisplayCC608 == mode)
        nextmode = db_prefer708 ? kDisplayAVSubtitle : kDisplayCC708;
    else if (kDisplayAVSubtitle == mode)
        nextmode = kDisplayTeletextCaptions;
    else if (kDisplayTeletextCaptions == mode)
        nextmode = pal ? kDisplayNUVTeletextCaptions : kDisplayNone;
    else if ((kDisplayNUVTeletextCaptions == mode) && pal)
        nextmode = kDisplayNone;
    else if (kDisplayNone == mode)
        nextmode = kDisplayTextSubtitle;

    if (((nextmode == kDisplayTextSubtitle) && HasTextSubtitles()) ||
         (nextmode == kDisplayNUVTeletextCaptions) ||
         (nextmode == kDisplayNone))
    {
        return nextmode;
    }
    else if (!(nextmode == kDisplayTextSubtitle) &&
               decoder->GetTrackCount(toTrackType(nextmode)))
    {
        return nextmode;
    }
    return NextCaptionTrack(nextmode);
}

void MythPlayer::InitAVSync(void)
{
    videosync->Start();

    avsync_adjustment = 0;

    repeat_delay = 0;

    refreshrate = (int)MythDisplay::GetDisplayInfo().rate;
    if (refreshrate <= 0)
        refreshrate = frame_interval;

    if (!using_null_videoout)
    {
        QString timing_type = videosync->getName();

        QString msg = QString("Video timing method: %1").arg(timing_type);
        VERBOSE(VB_GENERAL, LOC + msg);
        msg = QString("Display Refresh Rate: %1 Video Frame Rate: %2")
                       .arg(1000000.0 / refreshrate, 0, 'f', 3)
                       .arg(1000000.0 / frame_interval, 0, 'f', 3);
        VERBOSE(VB_PLAYBACK, LOC + msg);

        // try to get preferential scheduling, but ignore if we fail to.
        myth_nice(-19);
    }
}

#define MAXDIVERGE  3.0f
#define DIVERGELIMIT 30.0f
void MythPlayer::AVSync(bool limit_delay)
{
    float diverge = 0.0f;
    int frameDelay = m_double_framerate ? frame_interval / 2 : frame_interval;
    // attempt to reduce fps for standalone PIP
    if (player_ctx->IsPIP() && framesPlayed % 2)
    {
        videosync->WaitForFrame(frameDelay + avsync_adjustment);
        if (!using_null_videoout)
            videoOutput->SetFramesPlayed(framesPlayed + 1);
        return;
    }

    VideoFrame *buffer = videoOutput->GetLastShownFrame();
    if (!buffer)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "AVSync: No video buffer");
        return;
    }
    if (videoOutput->IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "AVSync: "
                "Unknown error in videoOutput, aborting playback.");
        SetErrored(QObject::tr("Failed to initialize A/V Sync"));
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

    if (diverge < -MAXDIVERGE)
    {
        // If video is way behind of audio, adjust for it...
        QString dbg = QString("Video is %1 frames behind audio (too slow), ")
            .arg(-diverge);

        // Reset A/V Sync
        lastsync = true;

        if (buffer && !using_null_videoout &&
            videoOutput->hasHWAcceleration() &&
           !videoOutput->IsSyncLocked())
        {
            // If we are using certain hardware decoders, so we've already done
            // the decoding; display the frame, but don't wait for A/V Sync.
            // Excludes HW decoder/render methods that are locked to
            // the vertical sync (e.g. VDPAU)
            videoOutput->PrepareFrame(buffer, kScan_Intr2ndField, osd);
            videoOutput->Show(kScan_Intr2ndField);
            VERBOSE(VB_PLAYBACK, LOC + dbg + "skipping A/V wait.");
        }
        else
        {
            // If we are using software decoding, skip this frame altogether.
            VERBOSE(VB_PLAYBACK, LOC + dbg + "dropping frame to catch up.");
        }
    }
    else if (!using_null_videoout)
    {
        // if we get here, we're actually going to do video output
        if (buffer)
            videoOutput->PrepareFrame(buffer, ps, osd);

        VERBOSE(VB_PLAYBACK|VB_TIMESTAMP, QString("AVSync waitforframe %1 %2")
                .arg(avsync_adjustment).arg(m_double_framerate));
        videosync->WaitForFrame(frameDelay + avsync_adjustment + repeat_delay);
        VERBOSE(VB_PLAYBACK|VB_TIMESTAMP, "AVSync show");
        videoOutput->Show(ps);

        if (videoOutput->IsErrored())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Error condition detected "
                    "in videoOutput after Show(), aborting playback.");
            SetErrored(QObject::tr("Serious error detected in Video Output"));
            return;
        }

        if (m_double_framerate)
        {
            //second stage of deinterlacer processing
            ps = (kScan_Intr2ndField == ps) ?
                kScan_Interlaced : kScan_Intr2ndField;

            if (m_double_process && ps != kScan_Progressive)
            {
                videofiltersLock.lock();
                videoOutput->ProcessFrame(
                        buffer, osd, videoFilters, pip_players, ps);
                videofiltersLock.unlock();
            }

            if (buffer)
                videoOutput->PrepareFrame(buffer, ps, osd);

            // Display the second field
            videosync->WaitForFrame(frameDelay + avsync_adjustment);
            videoOutput->Show(ps);
        }

        repeat_delay = frame_interval * buffer->repeat_pict * 0.5;

        if (repeat_delay)
            VERBOSE(VB_TIMESTAMP, QString("A/V repeat_pict, adding %1 repeat "
                    "delay").arg(repeat_delay));
    }
    else
    {
        videosync->WaitForFrame(frameDelay);
    }

    if (output_jmeter)
        output_jmeter->RecordCycleTime();

    avsync_adjustment = 0;

    if (diverge > MAXDIVERGE)
    {
        // If audio is way behind of video, adjust for it...
        // by cutting the frame rate in half for the length of this frame
        avsync_adjustment = refreshrate;
        lastsync = true;
        VERBOSE(VB_PLAYBACK, LOC +
                QString("Video is %1 frames ahead of audio,\n"
                        "\t\t\tdoubling video frame interval to slow down.").arg(diverge));
    }

    if (audio.HasAudioOut() && normal_speed)
    {
        int64_t currentaudiotime = audio.GetAudioTime();
        VERBOSE(VB_TIMESTAMP, QString(
                    "A/V timecodes audio %1 video %2 frameinterval %3 "
                    "avdel %4 avg %5 tcoffset %6")
                .arg(currentaudiotime)
                .arg(buffer->timecode)
                .arg(frame_interval)
                .arg(buffer->timecode - currentaudiotime)
                .arg(avsync_avg)
                .arg(tc_wrap[TC_AUDIO])
                 );
        if (currentaudiotime != 0 && buffer->timecode != 0)
        { // currentaudiotime == 0 after a seek
            // The time at the start of this frame (ie, now) is given by
            // last->timecode
            int delta = (int)((buffer->timecode - prevtc)/play_speed) - (frame_interval / 1000);
            prevtc = buffer->timecode;
            //cerr << delta << " ";

            // If the timecode is off by a frame (dropped frame) wait to sync
            if (delta > (int) frame_interval / 1200 &&
                delta < (int) frame_interval / 1000 * 3 &&
                prevrp == 0)
            {
                // wait an extra frame interval
                avsync_adjustment += frame_interval;
            }
            prevrp = buffer->repeat_pict;

            avsync_delay = (buffer->timecode - currentaudiotime) * 1000;//usec
            // prevents major jitter when pts resets during dvd title
            if (avsync_delay > 2000000 && limit_delay)
                avsync_delay = 90000;
            avsync_avg = (avsync_delay + (avsync_avg * 3)) / 4;

            /* If the audio time codes and video diverge, shift
               the video by one interlaced field (1/2 frame) */
            if (!lastsync)
            {
                if (avsync_avg > frame_interval * 3 / 2)
                {
                    avsync_adjustment += refreshrate;
                    lastsync = true;
                }
                else if (avsync_avg < 0 - frame_interval * 3 / 2)
                {
                    avsync_adjustment -= refreshrate;
                    lastsync = true;
                }
            }
            else
                lastsync = false;
        }
        else
        {
            avsync_avg = 0;
        }
    }
}

void MythPlayer::DisplayPauseFrame(void)
{
    if (!videoOutput || ! videosync)
        return;

    if (videoOutput->IsErrored())
    {
        SetErrored(QObject::tr("Serious error detected in Video Output"));
        return;
    }

    if (needNewPauseFrame)
    {
        if (videoOutput->ValidVideoFrames())
        {
            videoOutput->UpdatePauseFrame();
            needNewPauseFrame = false;
        }
        else
        {
            decodeOneFrame = true;
        }
    }

    videofiltersLock.lock();
    videoOutput->ProcessFrame(NULL, osd, videoFilters, pip_players);
    videofiltersLock.unlock();

    videoOutput->PrepareFrame(NULL, kScan_Ignore, osd);
    videoOutput->Show(kScan_Ignore);
    videosync->Start();
}

bool MythPlayer::PrebufferEnoughFrames(bool pause_audio, int min_buffers)
{
    if (!videoOutput)
        return false;

    bool was_buffering = buffering;
    if (!(min_buffers ? (videoOutput->ValidVideoFrames() >= min_buffers) :
                        (videoOutput->hasHWAcceleration() ?
                            videoOutput->EnoughPrebufferedFrames() :
                            videoOutput->EnoughDecodedFrames())))
    {
        if (!buffering)
        {
            VERBOSE(VB_PLAYBACK, LOC + "Waiting for video buffers...");
            buffering = true;
            audio.Pause(pause_audio);
            buffering_start = QTime::currentTime();
        }
        usleep(frame_interval >> 3);
        int waited_for = buffering_start.msecsTo(QTime::currentTime());
        if ((waited_for & 100) == 100)
        {
            VERBOSE(VB_IMPORTANT, LOC +
                QString("Waited 100ms for video buffers %1")
                .arg(videoOutput->GetFrameStatus()));
        }
        if ((waited_for > 500) && !videoOutput->EnoughFreeFrames())
        {
            VERBOSE(VB_IMPORTANT, LOC + "Timed out waiting for frames, and"
                    "\n\t\t\tthere are not enough free frames. "
                    "Discarding buffered frames.");
            // This call will result in some ugly frames, but allows us
            // to recover from serious problems if frames get leaked.
            DiscardVideoFrames(true);
        }
        if (waited_for > 20000) // 20 seconds
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Waited too long for decoder to fill video buffers. Exiting..");
            SetErrored(QObject::tr("Video frame buffering failed too many times."));
        }
        if (normal_speed)
            videosync->Start();
        return false;
    }
    buffering = false;
    if (was_buffering)
        audio.Pause(false);
    return true;
}

void MythPlayer::DisplayNormalFrame(bool allow_pause)
{
    if (allow_pause && (allpaused || !PrebufferEnoughFrames()))
    {
        // When going to switch channels
        if (allpaused)
        {
            usleep(frame_interval);
            if (player_ctx && !player_ctx->IsPIP())
                DisplayPauseFrame();
        }
        return;
    }

    videoOutput->StartDisplayingFrame();
    VideoFrame *frame = videoOutput->GetLastShownFrame();
    PreProcessNormalFrame();

    // handle scan type changes
    AutoDeint(frame);
    detect_letter_box->SwitchTo(frame);

    FrameScanType ps = m_scan;
    if (kScan_Detect == m_scan || kScan_Ignore == m_scan)
        ps = kScan_Progressive;

    videofiltersLock.lock();
    videoOutput->ProcessFrame(frame, osd, videoFilters, pip_players, ps);
    videofiltersLock.unlock();

    AVSync();
    videoOutput->DoneDisplayingFrame(frame);
}

void MythPlayer::PreProcessNormalFrame(void)
{
#ifdef USING_MHEG
    // handle Interactive TV
    if (GetInteractiveTV() && GetDecoder())
    {
        QMutexLocker locker(&itvLock);

        OSD *osd = GetOSD();
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
    }
#endif // USING_MHEG
}

void MythPlayer::VideoStart(void)
{
    if (!using_null_videoout && !player_ctx->IsPIP())
    {
        QRect visible, total;
        float aspect, scaling;

        osd = new OSD(this, m_tv);

        videoOutput->GetOSDBounds(total, visible, aspect, scaling, 1.0f);
        osd->Init(visible, aspect);
        videoOutput->InitOSD(osd);
        SetupTeletextViewer();
        osd->EnableSubtitles(kDisplayNone);

#ifdef USING_MHEG
        if (GetInteractiveTV())
        {
            total = videoOutput->GetMHEGBounds();
            GetInteractiveTV()->Reinit(total);
        }
#endif // USING_MHEG

        SetCaptionsEnabled(gCoreContext->GetNumSetting("DefaultCCMode"), false);
    }

    SetPlaying(true);
    ClearAfterSeek(false);

    avsync_delay = 0;
    avsync_avg = 0;
    refreshrate = 0;
    lastsync = false;

    if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK))
        output_jmeter = new Jitterometer("video_output", 100);
    else
        output_jmeter = NULL;

    refreshrate = frame_interval;

    float temp_speed = (play_speed == 0.0) ? audio.GetStretchFactor() : play_speed;
    uint fr_int = (int)(1000000.0 / video_frame_rate / temp_speed);
    uint rf_int = (int)MythDisplay::GetDisplayInfo().rate;

    // Default to Interlaced playback to allocate the deinterlacer structures
    // Enable autodetection of interlaced/progressive from video stream
    // And initialoze m_scan_tracker to 2 which will immediately switch to
    // progressive if the first frame is progressive in AutoDeint().
    m_scan             = kScan_Interlaced;
    m_scan_locked      = false;
    m_double_framerate = false;
    m_can_double       = false;
    m_scan_tracker     = 2;

    if (using_null_videoout)
    {
        videosync = new USleepVideoSync(videoOutput, (int)fr_int, 0, false);
    }
    else if (videoOutput)
    {
        // Set up deinterlacing in the video output method
        m_double_framerate =
            (videoOutput->SetupDeinterlace(true) &&
             videoOutput->NeedsDoubleFramerate());

        m_double_process = videoOutput->IsExtraProcessingRequired();

        videosync = VideoSync::BestMethod(
            videoOutput, fr_int, rf_int, m_double_framerate);

        // Make sure video sync can do it
        if (videosync != NULL && m_double_framerate)
        {
            m_can_double = (frame_interval / 2 > videosync->getRefreshInterval() * 0.995);
            if (!m_can_double)
            {
                VERBOSE(VB_IMPORTANT, LOC + "Video sync method can't support "
                        "double framerate (refresh rate too low for 2x deint)");
                FallbackDeint();
            }
        }
    }
    if (!videosync)
    {
        videosync = new BusyWaitVideoSync(
            videoOutput, (int)fr_int, (int)rf_int, m_double_framerate);
    }

    if (isDummy)
        ChangeSpeed();

    InitAVSync();
    videosync->Start();
}

bool MythPlayer::VideoLoop(void)
{
    if (videoPaused || isDummy || noVideoTracks)
    {
        usleep(frame_interval);
        DisplayPauseFrame();
    }
    else
        DisplayNormalFrame();

    if (using_null_videoout)
        GetDecoder()->UpdateFramesPlayed();
    else
        framesPlayed = videoOutput->GetFramesPlayed();
    return !IsErrored();
}

void MythPlayer::VideoEnd(void)
{
    QMutexLocker locker(&vidExitLock);
    delete osd;
    delete videosync;
    delete videoOutput;
    osd         = NULL;
    videosync   = NULL;
    videoOutput = NULL;
}

bool MythPlayer::FastForward(float seconds)
{
    if (!videoOutput)
        return false;

    if (fftime >= 0)
        fftime = (long long)(seconds * video_frame_rate);
    return fftime > CalcMaxFFTime(fftime, false);
}

bool MythPlayer::Rewind(float seconds)
{
    if (!videoOutput)
        return false;

    if (rewindtime >= 0)
        rewindtime = (long long)(seconds * video_frame_rate);
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
        framesPlayed = 0;
    GetDecoder()->Reset();
    if (GetDecoder()->IsErrored())
        SetErrored("Unable to reset video decoder");
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

    VERBOSE(VB_PLAYBACK, LOC + "SwitchToProgram - start");
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
        eof = false;
        delete pginfo;
        return;
    }

    player_ctx->buffer->OpenFile(
        pginfo->GetPlaybackURL(), RingBuffer::kDefaultOpenTimeout);

    if (!player_ctx->buffer->IsOpen())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "SwitchToProgram's OpenFile failed " +
                QString("(card type: %1).")
                .arg(player_ctx->tvchain->GetCardType(newid)));
        VERBOSE(VB_IMPORTANT, QString("\n") + player_ctx->tvchain->toString());
        eof = true;
        SetErrored(QObject::tr("Error opening switch program buffer"));
        delete pginfo;
        return;
    }

    if (eof)
    {
        discontinuity = true;
        ResetCaptions();
    }

    VERBOSE(VB_PLAYBACK, LOC + QString("SwitchToProgram(void) "
            "discont: %1 newtype: %2 newid: %3 eof: %4")
            .arg(discontinuity).arg(newtype).arg(newid).arg(eof));

    if (discontinuity || newtype)
    {
        player_ctx->tvchain->SetProgram(*pginfo);
        GetDecoder()->SetProgramInfo(*pginfo);

        player_ctx->buffer->Reset(true);
        if (newtype)
        {
            if (OpenFile() < 0)
                SetErrored(QObject::tr("Error opening switch program file"));
        }
        else
            ResetPlaying();
    }
    else
    {
        player_ctx->SetPlayerChangingBuffers(true);
        GetDecoder()->SetReadAdjust(player_ctx->buffer->SetAdjustFilesize());
        GetDecoder()->SetWaitForChange();
    }
    delete pginfo;

    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "SwitchToProgram failed.");
        eof = true;
        return;
    }

    // the bitrate is reset by player_ctx->buffer->OpenFile()...
    player_ctx->buffer->UpdateRawBitrate(GetDecoder()->GetRawBitrate());
    player_ctx->buffer->Unpause();

    if (discontinuity || newtype)
    {
        CheckTVChain();
        forcePositionMapSync = true;
    }

    eof = false;
    Play();
    VERBOSE(VB_PLAYBACK, LOC + "SwitchToProgram - end");
}

void MythPlayer::FileChangedCallback(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "FileChangedCallback");

    Pause();
    ChangeSpeed();
    if (dynamic_cast<AvFormatDecoder *>(GetDecoder()))
        player_ctx->buffer->Reset(false, true);
    else
        player_ctx->buffer->Reset(false, true, true);
    Play();

    eof = false;

    player_ctx->SetPlayerChangingBuffers(false);

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    player_ctx->tvchain->SetProgram(*player_ctx->playingInfo);
    GetDecoder()->SetProgramInfo(*player_ctx->playingInfo);
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    CheckTVChain();
    forcePositionMapSync = true;
}

void MythPlayer::JumpToProgram(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "JumpToProgram - start");
    bool discontinuity = false, newtype = false;
    int newid = -1;
    long long nextpos = player_ctx->tvchain->GetJumpPos();
    ProgramInfo *pginfo = player_ctx->tvchain->GetSwitchProgram(
        discontinuity, newtype, newid);
    if (!pginfo)
        return;

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
        eof = false;
        delete pginfo;
        return;
    }

    SendMythSystemPlayEvent("PLAY_CHANGED", pginfo);

    player_ctx->buffer->OpenFile(
        pginfo->GetPlaybackURL(), RingBuffer::kDefaultOpenTimeout);

    if (!player_ctx->buffer->IsOpen())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "JumpToProgram's OpenFile failed " +
                QString("(card type: %1).")
                .arg(player_ctx->tvchain->GetCardType(newid)));
        VERBOSE(VB_IMPORTANT, QString("\n") + player_ctx->tvchain->toString());

        eof = true;
        SetErrored(QObject::tr("Error opening jump program file buffer"));
        delete pginfo;
        return;
    }

    bool wasDummy = isDummy;
    if (newtype || wasDummy)
    {
        if (OpenFile() < 0)
            SetErrored(QObject::tr("Error opening jump program file"));
    }
    else
        ResetPlaying();

    if (IsErrored() || !GetDecoder())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "JumpToProgram failed.");
        if (!IsErrored())
            SetErrored(QObject::tr("Error reopening video decoder"));
        delete pginfo;
        return;
    }

    // the bitrate is reset by player_ctx->buffer->OpenFile()...
    player_ctx->buffer->UpdateRawBitrate(GetDecoder()->GetRawBitrate());
    player_ctx->buffer->IgnoreLiveEOF(false);

    GetDecoder()->SetProgramInfo(*pginfo);
    delete pginfo;

    CheckTVChain();
    forcePositionMapSync = true;
    Play();
    ChangeSpeed();

    if (nextpos < 0)
        nextpos += totalFrames;
    if (nextpos < 0)
        nextpos = 0;

    if (nextpos > 10)
        DoFastForward(nextpos, true, false);

    eof = false;
    player_ctx->SetPlayerChangingBuffers(false);
    VERBOSE(VB_PLAYBACK, LOC + "JumpToProgram - end");
}

bool MythPlayer::StartPlaying(void)
{
    if (OpenFile() < 0)
    {
        VERBOSE(VB_IMPORTANT, "Unable to open video file.");
        return false;
    }

    framesPlayed = 0;
    rewindtime = fftime = 0;
    next_play_speed = audio.GetStretchFactor();
    jumpchapter = 0;
    commBreakMap.SkipCommercials(0);

    if (!InitVideo())
    {
        VERBOSE(VB_IMPORTANT, "Unable to initialize video.");
        audio.DeleteOutput();
        return false;
    }

    bool seek = bookmarkseek > 30;
    EventStart();
    DecoderStart(seek);
    if (seek)
        InitialSeek();
    VideoStart();

    if (playerTimer)
        delete playerTimer;
    playerTimer = new PlayerTimer(this);
    playerThread->setPriority(QThread::TimeCriticalPriority);
    return !IsErrored();
}

void MythPlayer::InitialSeek(void)
{
    // TODO handle initial commskip and/or cutlist skip as well
    if (bookmarkseek > 30)
    {
        DoFastForward(bookmarkseek, true, false);
        if (gCoreContext->GetNumSetting("ClearSavedPosition", 1) &&
            !player_ctx->IsPIP())
        {
            ClearBookmark(false);
        }
    }
    UnpauseDecoder();
}


void MythPlayer::StopPlaying()
{
    VERBOSE(VB_PLAYBACK, LOC + QString("StopPlaying - begin"));
    playerThread->setPriority(QThread::NormalPriority);

    if (playerTimer)
    {
        delete playerTimer;
        playerTimer = NULL;
    }

    DecoderEnd();
    VideoEnd();
    AudioEnd();

    VERBOSE(VB_PLAYBACK, LOC + QString("StopPlaying - end"));
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
    if (tracksChanged)
        SetCaptionsEnabled(true, false);

    // (re)initialise the teletext viewer
    if (initTeletext)
        SetupTeletextViewer();

    // Refresh the programinfo in use status
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
        player_ctx->playingInfo->UpdateInUseMark();
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    // Disable timestretch if we are too close to the end of the buffer
    if ((play_speed > 1.01f) && IsNearEnd())
    {
        VERBOSE(VB_PLAYBACK, LOC + "Near end, Slowing down playback.");
        Play(1.0f, true, true);
    }

    if (isDummy && player_ctx->tvchain && player_ctx->tvchain->HasNext())
    {
        // Switch from the dummy recorder to the tuned program in livetv
        player_ctx->tvchain->JumpToNext(true, 1);
        JumpToProgram();
    }
    else if ((!allpaused || eof) && player_ctx->tvchain &&
             !GetDecoder()->GetWaitForChange())
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

    // Disable fastforward if we are too close to the end of the buffer
    if (ffrew_skip > 1 && (CalcMaxFFTime(100, false) < 100))
    {
        VERBOSE(VB_PLAYBACK, LOC + "Near end, stopping fastforward.");
        Play(1.0f, true, true);
    }

    // Disable rewind if we are too close to the beginning of the buffer
    if (CalcRWTime(-ffrew_skip) > 0 &&
       (!noVideoTracks && (framesPlayed <= keyframedist)))
    {
        VERBOSE(VB_PLAYBACK, LOC + "Near start, stopping rewind.");
        float stretch = (ffrew_skip > 0) ? 1.0f : audio.GetStretchFactor();
        Play(stretch, true, true);
    }

    // Check for error
    if (IsErrored() || player_ctx->IsRecorderErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Unknown recorder error, exiting decoder");
        if (!IsErrored())
            SetErrored(QObject::tr("Irrecoverable recorder error"));
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
    if (eof)
    {
        if (player_ctx->tvchain)
        {
            if (!allpaused && player_ctx->tvchain->HasNext())
            {
                VERBOSE(VB_IMPORTANT, "LiveTV forcing JumpTo 1");
                player_ctx->tvchain->JumpToNext(true, 1);
                return;
            }
        }
        else if (!allpaused)
        {
            SetPlaying(false);
            return;
        }
    }

    // Handle rewind
    if (rewindtime > 0 && (ffrew_skip == 1 || ffrew_skip == 0))
    {
        rewindtime = CalcRWTime(rewindtime);
        if (rewindtime > 0)
            DoRewind(rewindtime);
    }

    // Handle fast forward
    if (fftime > 0 && (ffrew_skip == 1 || ffrew_skip == 0))
    {
        fftime = CalcMaxFFTime(fftime);
        if (fftime > 0)
        {
            DoFastForward(fftime);
            if (eof)
               return;
        }
    }

    // Handle chapter jump (currently matroska only)
    if (jumpchapter != 0)
        DoJumpChapter(jumpchapter);

    // Handle commercial skipping
    if (commBreakMap.GetSkipCommercials() != 0 && (ffrew_skip == 1))
    {
        if (!commBreakMap.HasMap())
        {
            SetOSDStatus(QObject::tr("Not Flagged"), kOSDTimeout_Med);
            QString message = "COMMFLAG_REQUEST ";
            player_ctx->LockPlayingInfo(__FILE__, __LINE__);
            message += player_ctx->playingInfo->GetChanID() + " " +
                   player_ctx->playingInfo->MakeUniqueKey();
            player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
            RemoteSendMessage(message);
        }
        else
        {
            QString msg;
            uint64_t jumpto = 0;
            bool jump = commBreakMap.DoSkipCommercials(jumpto, framesPlayed,
                                            video_frame_rate, totalFrames, msg);
            if (!msg.isEmpty())
                SetOSDStatus(msg, kOSDTimeout_Med);
            if (jump)
                DoJumpToFrame(jumpto, true, true);
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
        bool jump = commBreakMap.AutoCommercialSkip(jumpto, framesPlayed,
                                                    video_frame_rate,
                                                    totalFrames, msg);
        if (!msg.isEmpty())
            SetOSDStatus(msg, kOSDTimeout_Med);
        if (jump)
            DoJumpToFrame(jumpto, true, true);
    }

    // Handle cutlist skipping
    if (deleteMap.TrackerWantsToJump(framesPlayed, totalFrames, jumpto) &&
        (ffrew_skip == 1))
    {
        if (jumpto == totalFrames)
        {
            if (!(gCoreContext->GetNumSetting("EndOfRecordingExitPrompt") == 1
                  && !player_ctx->IsPIP() &&
                  player_ctx->GetState() == kState_WatchingPreRecorded))
            {
                eof = true;
            }
        }
        else
        {
            DoJumpToFrame(jumpto, true, true);
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
    if (QThread::currentThread() == (QThread*)decoderThread)
    {
        decoderPaused = true;
        decoderThreadPause.wakeAll();
        decoderPauseLock.unlock();
        return decoderPaused;
    }

    int tries = 0;
    pauseDecoder = true;
    while (decoderThread && !killdecoder && !eof && (tries++ < 100) &&
          !decoderThreadPause.wait(&decoderPauseLock, 100))
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Waited 100ms for decoder to pause");
    }
    pauseDecoder = false;
    decoderPauseLock.unlock();
    return decoderPaused;
}

void MythPlayer::UnpauseDecoder(void)
{
    decoderPauseLock.lock();

    if (QThread::currentThread() == (QThread*)decoderThread)
    {
        decoderPaused = false;
        decoderThreadUnpause.wakeAll();
        decoderPauseLock.unlock();
        return;
    }

    int tries = 0;
    unpauseDecoder = true;
    while (decoderThread && !killdecoder && (tries++ < 100) &&
          !decoderThreadUnpause.wait(&decoderPauseLock, 100))
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Waited 100ms for decoder to unpause");
    }
    unpauseDecoder = false;
    decoderPauseLock.unlock();
}

void MythPlayer::DecoderStart(bool start_paused)
{
    if (decoderThread)
    {
        if (decoderThread->isRunning())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Decoder thread already running"));
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
    SetPlaying(false);
    killdecoder = true;
    int tries = 0;
    while (decoderThread && !decoderThread->wait(100) && (tries++ < 50))
        VERBOSE(VB_PLAYBACK, LOC + "Waited 100ms for decoder loop to stop");

    if (decoderThread && decoderThread->isRunning())
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to stop decoder loop.");
    else
        VERBOSE(VB_PLAYBACK, LOC + "Exited decoder loop.");
}

void MythPlayer::DecoderPauseCheck(void)
{
    if (QThread::currentThread() != (QThread*)decoderThread)
        return;
    if (pauseDecoder)
        PauseDecoder();
    if (unpauseDecoder)
        UnpauseDecoder();
}

void MythPlayer::DecoderLoop(bool pause)
{
    if (pause)
        PauseDecoder();

    while (!killdecoder && !IsErrored())
    {
        noVideoTracks = GetDecoder() &&
                    !GetDecoder()->GetTrackCount(kTrackTypeVideo);

        DecoderPauseCheck();

        if (forcePositionMapSync)
        {
            forcePositionMapSync = false;
            GetDecoder()->SyncPositionMap();
        }

        if (decoderSeek >= 0)
        {
            decoderSeekLock.lock();
            if (((uint64_t)decoderSeek < framesPlayed) && GetDecoder())
                GetDecoder()->DoRewind(decoderSeek);
            else if (GetDecoder())
                GetDecoder()->DoFastForward(decoderSeek);
            decoderSeek = -1;
            decoderSeekLock.unlock();
        }

        bool obey_eof = eof && !(eof && player_ctx->tvchain && !allpaused);
        if (isDummy || ((decoderPaused || ffrew_skip == 0 || obey_eof) &&
            !decodeOneFrame))
        {
            usleep(1000);
            continue;
        }

        DecodeType dt = (audio.HasAudioOut() && normal_speed) ?
            kDecodeAV : kDecodeVideo;
        if (noVideoTracks && audio.HasAudioOut())
            dt = kDecodeAudio;
        DecoderGetFrame(dt);
        decodeOneFrame = false;
    }

    // Clear any wait conditions
    PauseDecoder();
    UnpauseDecoder();
    decoderSeek = -1;
}

bool MythPlayer::DecoderGetFrameFFREW(void)
{
    if (ffrew_skip > 0)
    {
        long long delta = GetDecoder()->GetFramesRead() - framesPlayed;
        long long real_skip = CalcMaxFFTime(ffrew_skip + delta) - delta;
        if (real_skip >= 0)
        {
            long long frame = GetDecoder()->GetFramesRead() + real_skip;
            GetDecoder()->DoFastForward(frame, false);
        }
    }
    else if (CalcRWTime(-ffrew_skip) >= 0)
    {
        DecoderGetFrameREW();
    }
    return GetDecoder()->GetFrame(kDecodeVideo);
}

bool MythPlayer::DecoderGetFrameREW(void)
{
    long long curFrame  = GetDecoder()->GetFramesRead();
    bool      toBegin   = -curFrame > ffrew_skip;
    long long real_skip = (toBegin) ? -curFrame : ffrew_skip;
    return GetDecoder()->DoRewind(curFrame + real_skip, false);
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
            usleep(10000);
        if (!videoOutput->EnoughFreeFrames())
        {
            if (++videobuf_retries >= 200)
            {
                VERBOSE(VB_IMPORTANT, LOC +
                        "Timed out waiting for free video buffers.");
                videobuf_retries = 0;
            }
            return false;
        }
        videobuf_retries = 0;
    }

    if (killdecoder)
        return false;
    if (!GetDecoder())
    {
        VERBOSE(VB_IMPORTANT, LOC + "DecoderGetFrame() called with NULL decoder.");
        return false;
    }
    else if (ffrew_skip == 1 || decodeOneFrame)
        ret = GetDecoder()->GetFrame(decodetype);
    else if (ffrew_skip != 0)
        ret = DecoderGetFrameFFREW();
    return ret;
}

void MythPlayer::SetTranscoding(bool value)
{
    transcoding = value;

    if (GetDecoder())
        GetDecoder()->setTranscoding(value);
}

bool MythPlayer::AddPIPPlayer(MythPlayer *pip, PIPLocation loc, uint timeout)
{
    if (QThread::currentThread() != playerThread)
    {
        VERBOSE(VB_IMPORTANT, QString("Cannot add PiP from another thread"));
        return false;
    }

    if (pip_players.contains(pip))
    {
        VERBOSE(VB_IMPORTANT, QString("PiPMap already contains PiP."));
        return false;
    }

    QList<PIPLocation> locs = pip_players.values();
    if (locs.contains(loc))
    {
        VERBOSE(VB_IMPORTANT, QString("Already have a PiP at that location."));
        return false;
    }

    pip_players.insert(pip, loc);
    return true;
}

bool MythPlayer::RemovePIPPlayer(MythPlayer *pip, uint timeout)
{
    if (QThread::currentThread() != playerThread)
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
    if (QThread::currentThread() != playerThread)
        return kPIP_END;

    if (pip_players.isEmpty())
        return (PIPLocation)gCoreContext->GetNumSetting("PIPLocation", kPIPTopLeft);

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

void MythPlayer::WrapTimecode(int64_t &timecode, TCTypes tc_type)
{
    if ((tc_type == TC_AUDIO) && (tc_wrap[TC_AUDIO] == INT64_MIN))
    {
        int64_t newaudio;
        newaudio = tc_lastval[TC_VIDEO];
        tc_wrap[TC_AUDIO] = newaudio - timecode;
        timecode = newaudio;
        tc_lastval[TC_AUDIO] = timecode;
        VERBOSE(VB_IMPORTANT, "Manual Resync AV sync values");
    }

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

    long long numFrames = totalFrames;

    if (player_ctx->playingInfo->QueryTranscodeStatus() !=
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
        VERBOSE(VB_GENERAL, QString("Marking recording as watched using "
                                    "offset %1 minutes").arg(offset/60));
    }
    else
    {
        player_ctx->playingInfo->SaveWatched(false);
        VERBOSE(VB_GENERAL, "Marking recording as unwatched");
    }

    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
}

void MythPlayer::SetBookmark(void)
{
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
    {
        player_ctx->playingInfo->SaveBookmark(framesPlayed);
        SetOSDStatus(QObject::tr("Position"), kOSDTimeout_Med);
        SetOSDMessage(QObject::tr("Bookmark Saved"), kOSDTimeout_Med);
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
}

void MythPlayer::ClearBookmark(bool message)
{
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
    {
        player_ctx->playingInfo->SaveBookmark(0);
        if (message)
            SetOSDMessage(QObject::tr("Bookmark Cleared"), kOSDTimeout_Med);
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
}

uint64_t MythPlayer::GetBookmark(void)
{
    uint64_t bookmark = 0;

    if (gCoreContext->IsDatabaseIgnored())
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

void MythPlayer::ChangeSpeed(void)
{
    float last_speed = play_speed;
    play_speed   = next_play_speed;
    normal_speed = next_normal_speed;

    bool skip_changed;
    if (play_speed > 0.0f && play_speed <= 3.0f)
    {
        skip_changed = (ffrew_skip != 1);
        ffrew_skip = 1;
    }
    else
    {
        skip_changed = true;
        int new_skip = ((uint)ceil(4.0 * fabs(play_speed) / 30)) * 30;
        if (play_speed < 0.0)
            new_skip = -new_skip;
        ffrew_skip = new_skip;
    }

    if (skip_changed && videoOutput)
    {
        videoOutput->SetPrebuffering(ffrew_skip == 1);
        if (play_speed != 0.0f && !(last_speed == 0.0f && ffrew_skip == 1))
            DoJumpToFrame(framesPlayed);
    }

    float temp_speed = (play_speed == 0.0) ? audio.GetStretchFactor() : play_speed;
    frame_interval = (int) (1000000.0f * ffrew_skip / video_frame_rate /
                            temp_speed);

    VERBOSE(VB_PLAYBACK, LOC + "Play speed: " +
            QString("rate: %1 speed: %2 skip: %3 => new interval %4")
            .arg(video_frame_rate).arg(play_speed)
            .arg(ffrew_skip).arg(frame_interval));

    if (videoOutput && videosync)
    {
        // We need to tell it this for automatic deinterlacer settings
        videoOutput->SetVideoFrameRate(video_frame_rate * play_speed);

        // If using bob deinterlace, turn on or off if we
        // changed to or from synchronous playback speed.
        bool play_1 = play_speed > 0.99f && play_speed < 1.01f && normal_speed;
        bool inter  = (kScan_Interlaced   == m_scan  ||
                       kScan_Intr2ndField == m_scan);

        videofiltersLock.lock();
        if (m_double_framerate && !play_1)
            videoOutput->FallbackDeint();
        else if (!m_double_framerate && m_can_double && play_1 && inter)
            videoOutput->BestDeint();
        videofiltersLock.unlock();

        m_double_framerate = videoOutput->NeedsDoubleFramerate();
        m_double_process = videoOutput->IsExtraProcessingRequired();
    }

    if (normal_speed && audio.HasAudioOut())
    {
        audio.SetStretchFactor(play_speed);
        if (decoder)
        {
            bool disable = (play_speed < 0.99f) || (play_speed > 1.01f);
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("Stretch Factor %1, %2 passthru ")
                    .arg(audio.GetStretchFactor())
                    .arg((disable) ? "disable" : "allow"));
            decoder->SetDisablePassThrough(disable);
        }
    }
}

bool MythPlayer::DoRewind(uint64_t frames, bool override_seeks,
                          bool seeks_wanted)
{
    SaveAudioTimecodeOffset(GetAudioTimecodeOffset());

    uint64_t number = frames + 1;
    uint64_t desiredFrame = (framesPlayed > number) ? framesPlayed - number : 0;

    limitKeyRepeat = false;
    if (desiredFrame < video_frame_rate)
        limitKeyRepeat = true;

    WaitForSeek(desiredFrame, override_seeks, seeks_wanted);
    rewindtime = 0;
    ClearAfterSeek();
    return true;
}

long long MythPlayer::CalcRWTime(long long rw) const
{
    bool hasliveprev = (livetv && player_ctx->tvchain &&
                        player_ctx->tvchain->HasPrev());

    if (!hasliveprev || ((int64_t)framesPlayed > (rw - 1)))
        return rw;

    player_ctx->tvchain->JumpToNext(false, (int)(-15.0 * video_frame_rate));
    return -1;
}

long long MythPlayer::CalcMaxFFTime(long long ff, bool setjump) const
{
    long long maxtime = (long long)(1.0 * video_frame_rate);
    bool islivetvcur = (livetv && player_ctx->tvchain &&
                        !player_ctx->tvchain->HasNext());

    if (livetv || (watchingrecording && player_ctx->recorder &&
                   player_ctx->recorder->IsValidRecorder()))
        maxtime = (long long)(3.0 * video_frame_rate);

    long long ret = ff;

    limitKeyRepeat = false;

    if (livetv && !islivetvcur && player_ctx->tvchain)
    {
        if (totalFrames > 0)
        {
            long long behind = totalFrames - framesPlayed;
            if (behind < maxtime || behind - ff <= maxtime * 2)
            {
                ret = -1;
                if (setjump)
                    player_ctx->tvchain->JumpToNext(true, 1);
            }
        }
    }
    else if (islivetvcur || (watchingrecording && player_ctx->recorder &&
                             player_ctx->recorder->IsValidRecorder()))
    {
        long long behind = player_ctx->recorder->GetFramesWritten() -
            framesPlayed;

        if (behind < maxtime) // if we're close, do nothing
            ret = 0;
        else if (behind - ff <= maxtime)
            ret = behind - maxtime;

        if (behind < maxtime * 3)
            limitKeyRepeat = true;
    }
    else
    {
        if (totalFrames > 0)
        {
            long long behind = totalFrames - framesPlayed;
            if (behind < maxtime)
                ret = 0;
            else if (behind - ff <= maxtime * 2)
                ret = behind - maxtime * 2;
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
    if (!videoOutput)
        return false;

    return player_ctx->buffer->IsNearEnd(
        GetDecoder()->GetFPS(), videoOutput->ValidVideoFrames());
}

/** \brief Returns true iff near end of recording.
 *  \param margin minimum number of frames we want before being near end,
 *                defaults to 2 seconds of video.
 */
bool MythPlayer::IsNearEnd(int64_t margin)
{
    uint64_t framesRead, framesLeft = 0;

    if (!player_ctx)
        return false;

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (!player_ctx->playingInfo || player_ctx->playingInfo->IsVideo() ||
        !GetDecoder())
    {
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return false;
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    margin = (margin >= 0) ? margin: (long long) (video_frame_rate*2);
    margin = (long long) (margin * audio.GetStretchFactor());
    bool watchingTV = watchingrecording && player_ctx->recorder &&
        player_ctx->recorder->IsValidRecorder();

    framesRead = GetDecoder()->GetFramesRead();

    if (!player_ctx->IsPIP() &&
        player_ctx->GetState() == kState_WatchingPreRecorded)
    {
        if (framesRead >= deleteMap.GetLastFrame(totalFrames))
            return true;
        framesLeft = (totalFrames > framesRead) ? totalFrames - framesRead : 0;
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

bool MythPlayer::DoFastForward(uint64_t frames, bool override_seeks,
                               bool seeks_wanted)
{
    SaveAudioTimecodeOffset(GetAudioTimecodeOffset());

    uint64_t number = frames - 1;
    uint64_t desiredFrame = framesPlayed + number;

    if (!deleteMap.IsEditing() && IsInDelete(desiredFrame))
    {
        uint64_t endcheck = deleteMap.GetLastFrame(totalFrames);
        if (desiredFrame > endcheck)
            desiredFrame = endcheck;
    }

    WaitForSeek(desiredFrame, override_seeks, seeks_wanted);
    fftime = 0;
    ClearAfterSeek(false);
    return true;
}

void MythPlayer::DoJumpToFrame(uint64_t frame, bool override_seeks,
                               bool seeks_wanted)
{
    if (frame > framesPlayed)
        DoFastForward(frame - framesPlayed, override_seeks, seeks_wanted);
    else if (frame < framesPlayed)
        DoRewind(framesPlayed - frame, override_seeks, seeks_wanted);
}

void MythPlayer::WaitForSeek(uint64_t frame, bool override_seeks,
                             bool seeks_wanted)
{
    if (!GetDecoder())
        return;

    bool after  = exactseeks && (ffrew_skip == 1);
    bool before = override_seeks ? seeks_wanted :
                           (allpaused && !deleteMap.IsEditing()) ? true: after;
    GetDecoder()->setExactSeeks(before);

    uint64_t max = totalFrames;
    if ((livetv || (watchingrecording && player_ctx->recorder &&
                   player_ctx->recorder->IsValidRecorder())))
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
            SetOSDMessage(QObject::tr("Searching") + QString().fill('.', num),
                          kOSDTimeout_Short);
            DisplayPauseFrame();
            need_clear = true;
        }
    }
    if (need_clear && osd)
        osd->HideWindow("osd_message");
    GetDecoder()->setExactSeeks(after);
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
    VERBOSE(VB_PLAYBACK, LOC + "ClearAfterSeek("<<clearvideobuffers<<")");

    if (clearvideobuffers && videoOutput)
        videoOutput->ClearAfterSeek();

    for (int j = 0; j < TCTYPESMAX; j++)
        tc_wrap[j] = tc_lastval[j] = 0;

    if (savedAudioTimecodeOffset)
    {
        tc_wrap[TC_AUDIO] = savedAudioTimecodeOffset;
        savedAudioTimecodeOffset = 0;
    }

    audio.Reset();
    ResetCaptions();
    deleteMap.TrackerReset(framesPlayed, totalFrames);
    commBreakMap.SetTracker(framesPlayed);
    commBreakMap.ResetLastSkip();
    needNewPauseFrame = true;
}

void MythPlayer::SetPlayerInfo(TV *tv, QWidget *widget,
                               bool frame_exact_seek, PlayerContext *ctx)
{
    m_tv = tv;
    parentWidget = widget;
    exactseeks   = frame_exact_seek;
    player_ctx   = ctx;
    livetv       = ctx->tvchain;
}

bool MythPlayer::EnableEdit(void)
{
    deleteMap.SetEditing(false);

    if (!hasFullPositionMap)
    {
        VERBOSE(VB_IMPORTANT, "Cannot edit - no full position map");
        SetOSDStatus(QObject::tr("No Seektable"), kOSDTimeout_Med);
        return false;
    }

    if (!osd || deleteMap.IsFileEditing(player_ctx))
        return false;

    pausedBeforeEdit = Pause();
    deleteMap.SetEditing(true);
    osd->DialogQuit();
    osd->HideAll();
    deleteMap.UpdateSeekAmount(0, video_frame_rate);
    deleteMap.UpdateOSD(framesPlayed, totalFrames, video_frame_rate,
                        player_ctx, osd);
    deleteMap.SetFileEditing(player_ctx, true);
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
        player_ctx->playingInfo->SaveEditing(true);
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    return deleteMap.IsEditing();
}

void MythPlayer::DisableEdit(bool save)
{
    deleteMap.SetEditing(false, osd);
    if (save)
        deleteMap.SaveMap(totalFrames, player_ctx);
    else
        deleteMap.LoadMap(totalFrames, player_ctx);
    deleteMap.TrackerReset(framesPlayed, totalFrames);
    deleteMap.SetFileEditing(player_ctx, false);
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
        player_ctx->playingInfo->SaveEditing(false);
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    if (!pausedBeforeEdit)
        Play();
    else
        SetOSDStatus(QObject::tr("Paused"), kOSDTimeout_None);
}

bool MythPlayer::HandleProgramEditorActions(QStringList &actions,
                                             long long frame)
{
    bool handled = false;
    bool refresh = true;

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;
        if (action == "LEFT")
        {
            if (deleteMap.GetSeekAmount() > 0)
            {
                DoRewind(deleteMap.GetSeekAmount(), true, true);
            }
            else
                HandleArbSeek(false);
        }
        else if (action == "RIGHT")
        {
            if (deleteMap.GetSeekAmount() > 0)
            {
                DoFastForward(deleteMap.GetSeekAmount(), true, true);
            }
            else
                HandleArbSeek(true);
        }
        else if (action == "LOADCOMMSKIP")
        {
            if (commBreakMap.HasMap())
            {
                frm_dir_map_t map;
                commBreakMap.GetMap(map);
                deleteMap.LoadCommBreakMap(totalFrames, map);
            }
        }
        else if (action == "PREVCUT")
        {
            int old_seekamount = deleteMap.GetSeekAmount();
            deleteMap.SetSeekAmount(-2);
            HandleArbSeek(false);
            deleteMap.SetSeekAmount(old_seekamount);
        }
        else if (action == "NEXTCUT")
        {
            int old_seekamount = deleteMap.GetSeekAmount();
            deleteMap.SetSeekAmount(-2);
            HandleArbSeek(true);
            deleteMap.SetSeekAmount(old_seekamount);
        }
#define FFREW_MULTICOUNT 10
        else if (action == "BIGJUMPREW")
        {
            if (deleteMap.GetSeekAmount() > 0)
                DoRewind(deleteMap.GetSeekAmount() * FFREW_MULTICOUNT,
                         true, true);
            else
            {
                int fps = (int)ceil(video_frame_rate);
                DoRewind(fps * FFREW_MULTICOUNT / 2, true, true);
            }
        }
        else if (action == "BIGJUMPFWD")
        {
            if (deleteMap.GetSeekAmount() > 0)
                DoFastForward(deleteMap.GetSeekAmount() * FFREW_MULTICOUNT,
                              true, true);
            else
            {
                int fps = (int)ceil(video_frame_rate);
                DoFastForward(fps * FFREW_MULTICOUNT / 2, true, true);
            }
        }
        else if (action == "SELECT")
        {
            deleteMap.NewCut(frame, totalFrames);
            refresh = true;
        }
        else if (action == "DELETE")
        {
            if (IsInDelete(frame))
            {
                deleteMap.Delete(frame, totalFrames);
                refresh = true;
            }
        }
        else if (action == "REVERT")
        {
            deleteMap.LoadMap(totalFrames, player_ctx);
            refresh = true;
        }
        else if (action == "REVERTEXIT")
        {
            DisableEdit(false);
            refresh = false;
        }
        else if (action == "SAVEMAP")
        {
            deleteMap.SaveMap(totalFrames, player_ctx);
            refresh = true;
        }
        else if (action == "EDIT" || action == "SAVEEXIT")
        {
            DisableEdit();
            refresh = false;
        }
        else
            handled = deleteMap.HandleAction(action, frame, framesPlayed,
                                             totalFrames, video_frame_rate);
    }

    if (handled && refresh)
    {
        deleteMap.UpdateOSD(framesPlayed, totalFrames, video_frame_rate,
                            player_ctx, osd);
    }

    return handled;
}

bool MythPlayer::IsInDelete(uint64_t frame)
{
    return deleteMap.IsInDelete(frame);
}

uint64_t MythPlayer::GetNearestMark(uint64_t frame, bool right)
{
    return deleteMap.GetNearestMark(frame, totalFrames, right);
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
        long long framenum = deleteMap.GetNearestMark(framesPlayed,
                                                      totalFrames, right);
        if (right && (framenum > (int64_t)framesPlayed))
            DoFastForward(framenum - framesPlayed, true, true);
        else if (!right && ((int64_t)framesPlayed > framenum))
            DoRewind(framesPlayed - framenum, true, true);
    }
    else
    {
        if (right)
        {
            // editKeyFrameDist is a workaround for when keyframe distance
            // is set to one, and keyframe detection is disabled because
            // the position map uses MARK_GOP_BYFRAME. (see DecoderBase)
            float editKeyFrameDist = keyframedist <= 2 ? 18 : keyframedist;

            DoFastForward((long long)(editKeyFrameDist * 1.1), true, false);
        }
        else
        {
            DoRewind(2, true, false);
        }
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

void MythPlayer::SetForcedAspectRatio(int mpeg2_aspect_value, int letterbox_permission)
{
    (void)letterbox_permission;

    float forced_aspect_old = forced_video_aspect;

    if (mpeg2_aspect_value == 2) // 4:3
        forced_video_aspect = 4.0f / 3.0f;
    else if (mpeg2_aspect_value == 3) // 16:9
        forced_video_aspect = 16.0f / 9.0f;
    else
        forced_video_aspect = -1;

    if (videoOutput && forced_video_aspect != forced_aspect_old)
    {
        float aspect = (forced_video_aspect > 0) ? forced_video_aspect : video_aspect;
        videoOutput->VideoAspectRatioChanged(aspect);
    }
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
    bzero(&orig,   sizeof(AVPicture));
    bzero(&retbuf, sizeof(AVPicture));

    using_null_videoout = true;

    if (OpenFile(0, false) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Could not open file for preview.");
        return NULL;
    }

    if ((video_dim.width() <= 0) || (video_dim.height() <= 0))
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + QString("Video Resolution invalid %1x%2")
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
        VERBOSE(VB_IMPORTANT, LOC_ERR +
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
            VERBOSE(VB_PLAYBACK, LOC + QString("Waited 100ms for video frame"));
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

    vw = video_dim.width();
    vh = video_dim.height();
    ar = video_aspect;

    DiscardVideoFrame(frame);
    return (char *)outputbuf;
}

void MythPlayer::SeekForScreenGrab(uint64_t &number, uint64_t frameNum,
                                   bool absolute)
{
    if (!hasFullPositionMap)
    {
        VERBOSE(VB_IMPORTANT, LOC + "GetScreenGrabAtFrame: Recording does not "
                "have position map so we will be unable to grab the desired "
                "frame.\n");
        player_ctx->LockPlayingInfo(__FILE__, __LINE__);
        if (player_ctx->playingInfo)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Run 'mythcommflag --file %1 --rebuild' to fix.")
                    .arg(player_ctx->playingInfo->GetBasename()));
            VERBOSE(VB_IMPORTANT,
                    QString("If that does not work and this is a .mpg file, "
                            "try 'mythtranscode --mpeg2 --buildindex "
                            "--allkeys -c %1 -s %2'.")
                    .arg(player_ctx->playingInfo->GetChanID())
                    .arg(player_ctx->playingInfo->GetRecordingStartTime()
                         .toString("yyyyMMddhhmmss")));
        }
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    }

    number = frameNum;
    if (number >= totalFrames)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
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
            deleteMap.LoadMap(totalFrames, player_ctx);
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

    // Only do seek if we have position map
    if (hasFullPositionMap)
    {
        DiscardVideoFrame(videoOutput->GetLastDecodedFrame());
        DoFastForward(number);
    }
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
        DoJumpToFrame(frameNumber, true, true);
        ClearAfterSeek();
    }

    int tries = 0;
    while (!videoOutput->ValidVideoFrames() && ((tries++) < 100))
    {
        decodeOneFrame = true;
        usleep(10000);
        if ((tries & 10) == 10)
            VERBOSE(VB_PLAYBACK, LOC + QString("Waited 100ms for video frame"));
    }

    return videoOutput->GetLastDecodedFrame();
}

QString MythPlayer::GetEncodingType(void) const
{
    return get_encoding_type(GetDecoder()->GetVideoCodecID());
}

void MythPlayer::GetCodecDescription(InfoMap &infoMap)
{
    infoMap["audiocodec"]    = ff_codec_id_string((CodecID)audio.GetCodec());
    infoMap["audiochannels"] = QString::number(audio.GetOrigChannels());

    int width  = video_disp_dim.width();
    int height = video_disp_dim.height();
    infoMap["videocodec"]     = GetEncodingType();
    infoMap["videocodecdesc"] = GetDecoder()->GetRawEncodingType();
    infoMap["videowidth"]     = QString::number(width);
    infoMap["videoheight"]    = QString::number(height);
    infoMap["videoframerate"] = QString::number(video_frame_rate, 'f', 2);

    if (height < 480)
        return;

    bool interlaced = is_interlaced(m_scan);
    if (height == 480 || height == 576)
        infoMap["videodescrip"] = "SD";
    else if (height == 720 && !interlaced)
        infoMap["videodescrip"] = "HD_720_P";
    else if (height == 1080 || height == 1088)
        infoMap["videodescrip"] = interlaced ? "HD_1080_I" : "HD_1080_P";
}

bool MythPlayer::GetRawAudioState(void) const
{
    return GetDecoder()->GetRawAudioState();
}

QString MythPlayer::GetXDS(const QString &key) const
{
    if (!GetDecoder())
        return QString::null;
    return GetDecoder()->GetXDS(key);
}

void MythPlayer::InitForTranscode(bool copyaudio, bool copyvideo)
{
    // Are these really needed?
    SetPlaying(true);
    keyframedist = 30;

    if (!InitVideo())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
            "Unable to initialize video for transcode.");
        SetPlaying(false);
        return;
    }

    framesPlayed = 0;
    ClearAfterSeek();

    if (copyvideo)
        GetDecoder()->SetRawVideoState(true);
    if (copyaudio)
        GetDecoder()->SetRawAudioState(true);

    SetExactSeeks(true);
    GetDecoder()->setExactSeeks(exactseeks);
    GetDecoder()->SetLowBuffers(true);
}

bool MythPlayer::TranscodeGetNextFrame(
    frm_dir_map_t::iterator &dm_iter,
    int &did_ff, bool &is_key, bool honorCutList)
{
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
        player_ctx->playingInfo->UpdateInUseMark();
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    uint64_t lastDecodedFrameNumber =
        videoOutput->GetLastDecodedFrame()->frameNumber;

    if ((lastDecodedFrameNumber == 0) && honorCutList)
        deleteMap.TrackerReset(0, 0);

    if (!decoderThread)
        DecoderStart(true/*start paused*/);

    if (!GetDecoder()->GetFrame(kDecodeAV))
        return false;
    if (eof)
        return false;

    if (honorCutList && !deleteMap.IsEmpty())
    {
        if (totalFrames && lastDecodedFrameNumber >= totalFrames)
            return false;

        uint64_t jumpto = 0;
        if (deleteMap.TrackerWantsToJump(lastDecodedFrameNumber, totalFrames,
                                         jumpto))
        {
            VERBOSE(VB_GENERAL, QString("Fast-Forwarding from %1 to %2")
                .arg(lastDecodedFrameNumber).arg(jumpto));
            if (jumpto >= totalFrames)
                return false;

            // For 0.25, move this to DoJumpToFrame(jumpto)
            WaitForSeek(jumpto);
            GetDecoder()->ClearStoredData();
            ClearAfterSeek();
            GetDecoder()->GetFrame(kDecodeAV);
            did_ff = 1;
        }
    }
    if (eof)
      return false;
    is_key = GetDecoder()->isLastFrameKey();
    return true;
}

long MythPlayer::UpdateStoredFrameNum(long curFrameNum)
{
    return GetDecoder()->UpdateStoredFrameNum(curFrameNum);
}

void MythPlayer::SetCutList(const frm_dir_map_t &newCutList)
{
    deleteMap.SetMap(newCutList);
}

bool MythPlayer::WriteStoredData(RingBuffer *outRingBuffer,
                                 bool writevideo, long timecodeOffset)
{
    if (writevideo && !GetDecoder()->GetRawVideoState())
        writevideo = false;
    GetDecoder()->WriteStoredData(outRingBuffer, writevideo, timecodeOffset);
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

    if ((livetv) ||
        (watchingrecording && player_ctx->recorder &&
         player_ctx->recorder->IsValidRecorder()))
    {
        spos = 1000.0 * framesPlayed / player_ctx->recorder->GetFramesWritten();
    }
    else if (totalFrames)
    {
        spos = 1000.0 * framesPlayed / totalFrames;
    }

    return((int)spos);
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

void MythPlayer::calcSliderPos(osdInfo &info, bool paddedFields)
{
    bool islive = false;
    int chapter = GetCurrentChapter() + 1;
    int title = GetCurrentTitle() + 1;
    info.text.insert("chapteridx", chapter ? QString().number(chapter) : QString());
    info.text.insert("titleidx", title ? QString().number(title) : QString());
    info.values.insert("position",   0);
    info.values.insert("progbefore", 0);
    info.values.insert("progafter",  0);

    int playbackLen = totalLength;

    if (livetv && player_ctx->tvchain)
    {
        info.values["progbefore"] = (int)player_ctx->tvchain->HasPrev();
        info.values["progafter"]  = (int)player_ctx->tvchain->HasNext();
        playbackLen = player_ctx->tvchain->GetLengthAtCurPos();
        islive = true;
    }
    else if (watchingrecording && player_ctx->recorder &&
             player_ctx->recorder->IsValidRecorder())
    {
        playbackLen =
            (int)(((float)player_ctx->recorder->GetFramesWritten() /
                   video_frame_rate));
        islive = true;
    }

    float secsplayed = ((float)framesPlayed / video_frame_rate);
    calcSliderPosPriv(info, paddedFields, playbackLen, secsplayed, islive);
}

void MythPlayer::calcSliderPosPriv(osdInfo &info, bool paddedFields,
                                   int playbackLen, float secsplayed,
                                   bool islive)
{
    playbackLen = max(playbackLen, 1);
    secsplayed  = min((float)playbackLen, max(secsplayed, 0.0f));

    info.values["position"] = (int)(1000.0f * (secsplayed / (float)playbackLen));

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
            text3 = QObject::tr("%n second(s)", "", sbsecs);
        }
    }

    info.text["description"] = QObject::tr("%1 of %2").arg(text1).arg(text2);
    info.text["playedtime"] = text1;
    info.text["totaltime"] = text2;
    info.text["remainingtime"] = islive ? QString() : text3;
    info.text["behindtime"] = islive ? text3 : QString();
}

int MythPlayer::GetNumChapters()
{
    if (GetDecoder())
        return GetDecoder()->GetNumChapters();
    return 0;
}

int MythPlayer::GetCurrentChapter()
{
    if (GetDecoder())
        return GetDecoder()->GetCurrentChapter(framesPlayed);
    return 0;
}

void MythPlayer::GetChapterTimes(QList<long long> &times)
{
    if (GetDecoder())
        return GetDecoder()->GetChapterTimes(times);
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
    VERBOSE(VB_PLAYBACK, LOC +
            QString("DoJumpChapter: current %1 want %2 (frame %3)")
            .arg(current).arg(chapter).arg(desiredFrame));

    if (desiredFrame < 0)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + QString("DoJumpChapter failed."));
        jumpchapter = 0;
        return false;
    }

    SaveAudioTimecodeOffset(GetAudioTimecodeOffset());
    DoJumpToFrame(desiredFrame);
    jumpchapter = 0;
    return true;
}

int64_t MythPlayer::GetChapter(int chapter)
{
    if (GetDecoder())
        return GetDecoder()->GetChapter(chapter);
    return 0;
}

InteractiveTV *MythPlayer::GetInteractiveTV(void)
{
#ifdef USING_MHEG
    if (!interactiveTV && osd && itvEnabled)
        interactiveTV = new InteractiveTV(this);
#endif // USING_MHEG
    return interactiveTV;
}

bool MythPlayer::ITVHandleAction(const QString &action)
{
#ifdef USING_MHEG
    if (!GetInteractiveTV())
        return false;

    QMutexLocker locker(&decoder_change_lock);

    if (GetDecoder())
    {
        QMutexLocker locker(&itvLock);
        if (GetInteractiveTV())
            return interactiveTV->OfferKey(action);
    }
#endif // USING_MHEG

    return false;
}

/** \fn MythPlayer::ITVRestart(uint chanid, uint cardid, bool isLive)
 *  \brief Restart the MHEG/MHP engine.
 */
void MythPlayer::ITVRestart(uint chanid, uint cardid, bool isLiveTV)
{
#ifdef USING_MHEG
    QMutexLocker locker(&decoder_change_lock);

    OSD *osd = GetOSD();
    if (!GetDecoder() || !osd)
        return;

    {
        QMutexLocker locker(&itvLock);
        if (GetInteractiveTV())
        {
            interactiveTV->Restart(chanid, cardid, isLiveTV);
            itvVisible = false;
        }
    }
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
    if (GetDecoder())
        return GetDecoder()->SetAudioByComponentTag(tag);
    return false;
}

/** \fn MythPlayer::SetVideoByComponentTag(int tag)
 *  \brief Selects the video stream using the DVB component tag.
 */
bool MythPlayer::SetVideoByComponentTag(int tag)
{
    QMutexLocker locker(&decoder_change_lock);
    if (GetDecoder())
        return GetDecoder()->SetVideoByComponentTag(tag);
    return false;
}

/** \fn MythPlayer::SetDecoder(DecoderBase*)
 *  \brief Sets the stream decoder, deleting any existing recorder.
 */
void MythPlayer::SetDecoder(DecoderBase *dec)
{
    QMutexLocker locker(&decoder_change_lock);

    if (!decoder)
        decoder = dec;
    else
    {
        DecoderBase *d = decoder;
        decoder = dec;
        delete d;
    }
}

bool MythPlayer::PosMapFromEnc(unsigned long long start,
                               QMap<long long, long long> &posMap)
{
    // Reads only new positionmap entries from encoder
    if (!(livetv || (player_ctx->recorder &&
                     player_ctx->recorder->IsValidRecorder())))
        return false;

    // if livetv, and we're not the last entry, don't get it from the encoder
    if (HasTVChainNext())
        return false;

    VERBOSE(VB_PLAYBACK, LOC + QString("Filling position map from %1 to %2")
            .arg(start).arg("end"));

    player_ctx->recorder->FillPositionMap(start, -1, posMap);

    return true;
}

void MythPlayer::SetErrored(const QString &reason) const
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
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("%1").arg(reason));
    }
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

void MythPlayer::SetOSDMessage(const QString &msg, OSDTimeout timeout)
{
    if (!osd)
        return;

    QHash<QString,QString> info;
    info.insert("message_text", msg);
    osd->SetText("osd_message", info, timeout);
}

void MythPlayer::SetOSDStatus(const QString &title, OSDTimeout timeout)
{
    if (!osd)
        return;

    osdInfo info;
    calcSliderPos(info);
    info.text.insert("title", title);
    osd->SetText("osd_status", info.text, timeout);
    osd->SetValues("osd_status", info.values, timeout);
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

