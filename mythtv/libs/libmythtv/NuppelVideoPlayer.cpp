// -*- Mode: c++ -*-

#undef HAVE_AV_CONFIG_H

// Std C headers
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
#include <QThreadPool>
#include <QKeyEvent>
#include <QDir>

#define NEW_AVSYNC

// MythTV headers
#include "mythconfig.h"
#include "mythdbcon.h"
#include "dialogbox.h"
#include "NuppelVideoPlayer.h"
#include "DetectLetterbox.h"
#include "audiooutput.h"
#include "recordingprofile.h"
#include "osdtypes.h"
#include "osdsurface.h"
#include "osdtypeteletext.h"
#include "remoteutil.h"
#include "programinfo.h"
#include "mythcontext.h"
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
#include "DVDRingBuffer.h"
#include "NuppelVideoRecorder.h"
#include "tv_play.h"
#include "interactivetv.h"
#include "util-osx-cocoa.h"
#include "mythverbose.h"
#include "myth_imgconvert.h"
#include "mythsystemevent.h"

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

#define FAST_RESTART 0

static unsigned dbg_ident(const NuppelVideoPlayer*);

#define LOC      QString("NVP(%1): ").arg(dbg_ident(this),0,36)
#define LOC_WARN QString("NVP(%1), Warning: ").arg(dbg_ident(this),0,36)
#define LOC_ERR  QString("NVP(%1), Error: ").arg(dbg_ident(this),0,36)

uint track_type_to_display_mode[kTrackTypeCount+2] =
{
    kDisplayNone,
    kDisplayAVSubtitle,
    kDisplayCC608,
    kDisplayCC708,
    kDisplayTeletextCaptions,
    kDisplayNone,
    kDisplayNUVTeletextCaptions,
};

NuppelVideoPlayer::NuppelVideoPlayer(bool muted)
    : decoder(NULL),                decoder_change_lock(QMutex::Recursive),
      videoOutput(NULL),            player_ctx(NULL),
      no_hardware_decoders(false),
      // Window stuff
      parentWidget(NULL), embedid(0),
      embx(-1), emby(-1), embw(-1), embh(-1),
      // State
      eof(false),
      m_double_framerate(false),    m_double_process(false),
      m_can_double(false),          m_deint_possible(true),
      paused(false),
      pausevideo(false),            actuallypaused(false),
      video_actually_paused(false), playing(false),
      decoder_thread_alive(true),   killplayer(false),
      killvideo(false),             livetv(false),
      watchingrecording(false),     editmode(false),
      resetvideo(false),            using_null_videoout(false),
      no_audio_in(false),           no_audio_out(false),
      transcoding(false),
      hasFullPositionMap(false),    limitKeyRepeat(false),
      errorMsg(QString::null),      errorType(kError_None),
      // Chapter stuff
      jumpchapter(0),
      // Bookmark stuff
      bookmarkseek(0),
      // Seek
      fftime(0),                    seekamountpos(4),
      seekamount(30),               exactseeks(false),
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
      keyframedist(30),
      // Prebuffering (RingBuffer) control
      prebuffering(false), prebuffer_tries(0), prebuffer_tries_total(0),

      // General Caption/Teletext/Subtitle support
      db_prefer708(true),
      textDisplayMode(kDisplayNone),
      prevTextDisplayMode(kDisplayNone),
      // Support for analog captions and teletext
      vbimode(VBIMode::None),
      ttPageNum(0x888),             ccmode(CC_CC1),
      wtxt(0), rtxt(0), text_size(0), ccline(""), cccol(0), ccrow(0),
      // Support for captions, teletext, etc. decoded by libav
      textDesired(false),
      osdHasSubtitles(false),       osdSubtitlesExpireAt(-1),

      // MHEG/MHI Interactive TV visible in OSD
      itvVisible(false),
      interactiveTV(NULL),
      itvEnabled(false),

      // OSD stuff
      osd(NULL),                    timedisplay(NULL),
      dialogname(""),               dialogtype(0),
      // Audio stuff
      audioOutput(NULL),
      audio_main_device(QString::null),
      audio_passthru_device(QString::null),
      audio_channels(2),            audio_codec(0),
      audio_bits(-1),               audio_samplerate(44100),
      audio_stretchfactor(1.0f),    audio_lock(QMutex::Recursive),
      audio_muted_on_creation(muted),
      // Picture-in-Picture stuff
      pip_active(false),            pip_visible(true),
      // Preview window support
      argb_buf(NULL),               argb_size(0,0),
      yuv2argb_conv(yuv2rgb_init_mmx(32, MODE_RGB)),
      yuv_need_copy(false),         yuv_desired_size(0,0),
      yuv_scaler(NULL),             yuv_frame_scaled(NULL),
      yuv_scaler_in_size(0,0),      yuv_scaler_out_size(0,0),
      // Filters
      videoFiltersForProgram(""),   videoFiltersOverride(""),
      postfilt_width(0),            postfilt_height(0),
      videoFilters(NULL),           FiltMan(new FilterManager()),
      // Commercial filtering
      skipcommercials(0),           autocommercialskip(kCommSkipOff),
      commrewindamount(0),
      commnotifyamount(0),          lastCommSkipDirection(0),
      lastCommSkipTime(0/*1970*/),  lastCommSkipStart(0),
      lastSkipTime(0 /*1970*/),
      deleteframe(0),
      hasdeletetable(false),        hasblanktable(false),
      hascommbreaktable(false),
      deleteIter(deleteMap.end()),  blankIter(blankMap.end()),
      commBreakIter(commBreakMap.end()),
      forcePositionMapSync(false),
      // Playback (output) speed control
      decoder_lock(QMutex::Recursive),
      next_play_speed(1.0f),        next_normal_speed(true),
      play_speed(1.0f),             normal_speed(true),
      frame_interval((int)(1000000.0f / 30)), m_frame_interval(0),
      ffrew_skip(1),
      // Audio and video synchronization stuff
      videosync(NULL),              delay(0),
      vsynctol(30/4),               avsync_delay(0),
      avsync_adjustment(0),         avsync_avg(0),
      avsync_oldavg(0),             refreshrate(0),
      lastsync(false),              m_playing_slower(false),
      m_stored_audio_stretchfactor(1.0),
      audio_paused(false),
      repeat_delay(0),
      // Audio warping stuff
      warpfactor(1.0f),             warpfactor_avg(1.0f),
      // Time Code stuff
      prevtc(0),                    prevrp(0),
      tc_avcheck_framecounter(0),   tc_diff_estimate(0),
      savedAudioTimecodeOffset(0),
      // LiveTVChain stuff
      m_tv(NULL),                   isDummy(false),
      // DVD stuff
      hidedvdbutton(true), need_change_dvd_track(0),
      dvd_stillframe_showing(false),
      // Debugging variables
      output_jmeter(NULL)
{

    // Playback (output) zoom control
    detect_letter_box = new DetectLetterbox(this);

    vbimode = VBIMode::Parse(gContext->GetSetting("VbiFormat"));

    commrewindamount = gContext->GetNumSetting("CommRewindAmount",0);
    commnotifyamount = gContext->GetNumSetting("CommNotifyAmount",0);
    decode_extra_audio=gContext->GetNumSetting("DecodeExtraAudio", 0);
    itvEnabled       = gContext->GetNumSetting("EnableMHEG", 0);
    db_prefer708     = gContext->GetNumSetting("Prefer708Captions", 1);
    autocommercialskip = (CommSkipMode)
        gContext->GetNumSetting("AutoCommercialSkip", kCommSkipOff);

    lastIgnoredManualSkip = QDateTime::currentDateTime().addSecs(-10);

    bzero(&txtbuffers, sizeof(txtbuffers));
    bzero(&tc_lastval, sizeof(tc_lastval));
    bzero(&tc_wrap,    sizeof(tc_wrap));

    // Get VBI page number
    QString mypage = gContext->GetSetting("VBIpageNr", "888");
    bool valid = false;
    uint tmp = mypage.toInt(&valid, 16);
    ttPageNum = (valid) ? tmp : ttPageNum;

    text_size = 8 * (sizeof(teletextsubtitle) + VT_WIDTH);
    for (int i = 0; i < MAXTBUFFER; i++)
        txtbuffers[i].buffer = new unsigned char[text_size + 1];

    memset(&CC708DelayedDeletes, 0, sizeof(CC708DelayedDeletes));
}

NuppelVideoPlayer::~NuppelVideoPlayer(void)
{
    QMutexLocker lk1(&vidExitLock);
    QMutexLocker lk2(&videofiltersLock);
    QMutexLocker lk3(&audio_lock);

    if (audioOutput)
    {
        delete audioOutput;
        audioOutput = NULL;
    }

    if (osdHasSubtitles || !nonDisplayedAVSubtitles.empty())
        ClearSubtitles();

    if (osd)
    {
        delete osd;
        osd = NULL;
    }

    for (int i = 0; i < MAXTBUFFER; i++)
    {
        if (txtbuffers[i].buffer)
        {
            delete [] txtbuffers[i].buffer;
            txtbuffers[i].buffer = NULL;
        }
    }

    SetDecoder(NULL);

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

    if (argb_buf)
    {
        delete [] argb_buf;
        argb_buf = NULL;
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

    ShutdownYUVResize();
}

void NuppelVideoPlayer::SetWatchingRecording(bool mode)
{
    QMutexLocker locker(&decoder_change_lock);

    watchingrecording = mode;
    if (GetDecoder())
        GetDecoder()->setWatchingRecording(mode);
}

void NuppelVideoPlayer::SetAudioInfo(const QString &main_device,
                                     const QString &passthru_device,
                                     uint           samplerate)
{
    audio_main_device = audio_passthru_device = QString::null;

    if (!main_device.isEmpty())
    {
        audio_main_device = main_device;
        audio_main_device.detach();
    }

    if (!passthru_device.isEmpty())
    {
        audio_passthru_device = passthru_device;
        audio_passthru_device.detach();
    }

    audio_samplerate      = (int)samplerate;
}

uint NuppelVideoPlayer::GetVolume(void)
{
    QMutexLocker lock(&audio_lock);
    if (audioOutput)
        return audioOutput->GetCurrentVolume();
    return 0;
}

bool NuppelVideoPlayer::SetMuted(bool mute)
{
    QMutexLocker lock(&audio_lock);
    bool is_muted = IsMuted();

    if (audioOutput && !is_muted && mute &&
        (kMuteAll == SetMuteState(kMuteAll)))
    {
        VERBOSE(VB_AUDIO, "muting sound " <<IsMuted());
        return true;
    }
    else if (audioOutput && is_muted && !mute &&
             (kMuteOff == SetMuteState(kMuteOff)))
    {
        VERBOSE(VB_AUDIO, "unmuting sound "<<IsMuted());
        return true;
    }

    VERBOSE(VB_AUDIO, "not changing sound mute state "<<IsMuted());

    return false;
}

MuteState NuppelVideoPlayer::SetMuteState(MuteState mstate)
{
    QMutexLocker lock(&audio_lock);
    if (audioOutput)
        return audioOutput->SetMuteState(mstate);
    return kMuteAll;
}

MuteState NuppelVideoPlayer::IncrMuteState(void)
{
    QMutexLocker lock(&audio_lock);
    MuteState mstate = kMuteAll;
    if (audioOutput)
        mstate = SetMuteState(VolumeBase::NextMuteState(GetMuteState()));
    return mstate;
}

MuteState NuppelVideoPlayer::GetMuteState(void)
{
    QMutexLocker lock(&audio_lock);
    if (audioOutput)
        return audioOutput->GetMuteState();
    return kMuteAll;
}

uint NuppelVideoPlayer::AdjustVolume(int change)
{
    QMutexLocker lock(&audio_lock);
    if (audioOutput)
        audioOutput->AdjustCurrentVolume(change);
    return GetVolume();
}

void NuppelVideoPlayer::PauseDecoder(void)
{
    decoder_lock.lock();
    next_play_speed = 0.0;
    next_normal_speed = false;
    decoder_lock.unlock();

    // Qt4 requires a QMutex as a parameter...
    // not sure if this is the best solution.
    // Mutex Must be locked before wait.
    QMutex mutex;
    mutex.lock();

    while (!eof && !actuallypaused &&
           !decoderThreadPaused.wait(&mutex, 100) &&
           !eof && !actuallypaused)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "Waited too long for decoder to pause");
    }
}

void NuppelVideoPlayer::Pause(bool waitvideo)
{
    PauseDecoder();

    //cout << "stopping other threads" << endl;
    internalPauseLock.lock();
    PauseVideo(waitvideo);
    internalPauseLock.unlock();

    audio_lock.lock();
    if (audioOutput)
    {
        audio_paused = true;
        audioOutput->Pause(true);
    }
    audio_lock.unlock();
    if (player_ctx->buffer)
        player_ctx->buffer->Pause();

    QMutexLocker locker(&decoder_change_lock);

    if (GetDecoder() && videoOutput)
    {
        //cout << "updating frames played" << endl;
        if (using_null_videoout)
            GetDecoder()->UpdateFramesPlayed();
        else
            framesPlayed = videoOutput->GetFramesPlayed();
    }
}

bool NuppelVideoPlayer::Play(float speed, bool normal, bool unpauseaudio)
{
    VERBOSE(VB_PLAYBACK, LOC +
            QString("Play(%1, normal %2, unpause audio %3)")
            .arg(speed,5,'f',1).arg(normal).arg(unpauseaudio));

    internalPauseLock.lock();
    if (editmode)
    {
        internalPauseLock.unlock();
        VERBOSE(VB_IMPORTANT, LOC + "Ignoring Play(), in edit mode.");
        return false;
    }
    UnpauseVideo();
    internalPauseLock.unlock();

    audio_lock.lock();
    if (audioOutput && unpauseaudio)
        audio_paused = false;

    audio_lock.unlock();
    if (player_ctx->buffer)
        player_ctx->buffer->Unpause();

    decoder_lock.lock();
    next_play_speed = speed;
    next_normal_speed = normal;
    decoder_lock.unlock();
    return true;
}

bool NuppelVideoPlayer::IsPaused(bool *is_pause_still_possible)
{
    bool rbf_playing = player_ctx->buffer && !player_ctx->buffer->isPaused();
    audio_lock.lock();
    bool aud_playing = audioOutput && !audioOutput->IsPaused();
    audio_lock.unlock();
    if (is_pause_still_possible)
    {
        bool decoder_pausing = (0.0f == next_play_speed) && !next_normal_speed;
        bool video_pausing = pausevideo;
        bool rbuf_paused = !rbf_playing;
        *is_pause_still_possible =
            decoder_pausing && video_pausing && rbuf_paused;
    }

    return (actuallypaused && !rbf_playing && !aud_playing &&
            IsVideoActuallyPaused());
}

void NuppelVideoPlayer::PauseVideo(bool wait)
{
    QMutexLocker locker(&pauseUnpauseLock);

    if (wait)
        video_actually_paused = false;
    pausevideo = true;

    for (uint i = 0; wait && !video_actually_paused; i++)
    {
        videoThreadPaused.wait(&pauseUnpauseLock, 250);

        if (video_actually_paused || eof)
            break;

        if ((i % 10) == 9)
            VERBOSE(VB_IMPORTANT, "Waited too long for video out to pause");
    }
}

void NuppelVideoPlayer::UnpauseVideo(bool wait)
{
    QMutexLocker locker(&pauseUnpauseLock);
    pausevideo = false;

    for (uint i = 0; wait && video_actually_paused; i++)
    {
        videoThreadUnpaused.wait(&pauseUnpauseLock, 250);

        if (!video_actually_paused || eof)
            break;

        if ((i % 10) == 9)
            VERBOSE(VB_IMPORTANT, "Waited too long for video out to unpause");
    }

    if (videoOutput)
        videoOutput->ExposeEvent();
}

void NuppelVideoPlayer::SetVideoActuallyPaused(bool val)
{
    QMutexLocker locker(&pauseUnpauseLock);
    video_actually_paused = val;

    if (val)
        videoThreadPaused.wakeAll();
    else
        videoThreadUnpaused.wakeAll();
}

bool NuppelVideoPlayer::IsVideoActuallyPaused(void) const
{
    QMutexLocker locker(&pauseUnpauseLock);
    return video_actually_paused;
}

void NuppelVideoPlayer::SetPlayingInfo(const ProgramInfo &pginfo)
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

void NuppelVideoPlayer::SetPrebuffering(bool prebuffer)
{
    prebuffering_lock.lock();

    if (prebuffer != prebuffering)
    {
        prebuffering = prebuffer;
        audio_lock.lock();
        if (audioOutput && !paused)
        {
            if (prebuffering)
                audioOutput->Pause(prebuffering);
            audio_paused = prebuffering;
        }
        audio_lock.unlock();
    }

    if (!prebuffering)
        prebuffering_wait.wakeAll();

    prebuffering_lock.unlock();
}

void NuppelVideoPlayer::SetPlaying(bool is_playing)
{
    QMutexLocker locker(&playingLock);

    playing = is_playing;

    playingWaitCond.wakeAll();
}

bool NuppelVideoPlayer::IsPlaying(uint wait_in_msec, bool wait_for) const
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

bool NuppelVideoPlayer::InitVideo(void)
{
    assert(player_ctx);
    if (!player_ctx)
        return false;

    PIPState pipState = player_ctx->GetPIPState();

    if (using_null_videoout)
    {
        videoOutput = new VideoOutputNull();
        if (!videoOutput->Init(video_disp_dim.width(), video_disp_dim.height(),
                               video_aspect, 0, 0, 0, 0, 0, 0))
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
            MythMainWindow *window = gContext->GetMainWindow();

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

        videoOutput = VideoOutput::Create(
            GetDecoder()->GetCodecDecoderName(),
            GetDecoder()->GetVideoCodecID(),
            GetDecoder()->GetVideoCodecPrivate(),
            pipState,
            video_disp_dim, video_aspect,
            widget->winId(), display_rect, (video_frame_rate * play_speed),
            0 /*embedid*/);

        if (!videoOutput)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Couldn't create VideoOutput instance. Exiting..");
            SetErrored(QObject::tr("Failed to initialize video output"));
            return false;
        }

        videoOutput->SetVideoScalingAllowed(true);

        // We need to tell it this for automatic deinterlacer settings
        videoOutput->SetVideoFrameRate(video_frame_rate * play_speed);
        CheckExtraAudioDecode();
    }

    if (embedid > 0 && pipState == kPIPOff)
    {
        videoOutput->EmbedInWidget(embx, emby, embw, embh);
    }

    SetCaptionsEnabled(gContext->GetNumSetting("DefaultCCMode"), false);

    InitFilters();

    return true;
}

void NuppelVideoPlayer::CheckExtraAudioDecode(void)
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

void NuppelVideoPlayer::ReinitOSD(void)
{
    if (videoOutput && !using_null_videoout)
    {
        QRect visible, total;
        float aspect, scaling;
        if (osd)
        {
            videoOutput->GetOSDBounds(total, visible, aspect,
                                      scaling, osd->GetThemeAspect());
            osd->Reinit(total, frame_interval, visible, aspect, scaling);
        }

#ifdef USING_MHEG
        if (GetInteractiveTV())
        {
            total = videoOutput->GetMHEGBounds();
            GetInteractiveTV()->Reinit(total);
            itvVisible = false;
        }
#endif // USING_MHEG
    }
}

void NuppelVideoPlayer::ReinitVideo(void)
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

QString NuppelVideoPlayer::ReinitAudio(void)
{
    QMutexLocker lock(&audio_lock);
    QString errMsg = QString::null;

    if ((audio_bits <= 0) || (audio_channels <= 0) || (audio_samplerate <= 0))
    {
        VERBOSE(VB_IMPORTANT, LOC +
                QString("Disabling Audio, params(%1,%2,%3)")
                .arg(audio_bits).arg(audio_channels).arg(audio_samplerate));

        no_audio_in = no_audio_out = true;
        if (audioOutput)
        {
            delete audioOutput;
            audioOutput = NULL;
        }
        return errMsg;
    }

    no_audio_in = false;

    if (!audioOutput && !using_null_videoout && player_ctx->IsAudioNeeded())
    {
        bool setVolume = gContext->GetNumSetting("MythControlsVolume", 1);
        audioOutput = AudioOutput::OpenAudio(audio_main_device,
                                             audio_passthru_device,
                                             audio_bits, audio_channels,
                                             audio_codec, audio_samplerate,
                                             AUDIOOUTPUT_VIDEO,
                                             setVolume, audio_passthru);
        if (!audioOutput)
            errMsg = QObject::tr("Unable to create AudioOutput.");
        else
            errMsg = audioOutput->GetError();

        if (!errMsg.isEmpty())
        {
            VERBOSE(VB_IMPORTANT, LOC + "Disabling Audio" +
                    QString(", reason is: %1").arg(errMsg));
            if (audioOutput)
            {
                delete audioOutput;
                audioOutput = NULL;
            }
            no_audio_out = true;
        }
        else if (no_audio_out)
        {
            VERBOSE(VB_IMPORTANT, LOC + "Enabling Audio");
            no_audio_out = false;
        }
        if (audio_muted_on_creation)
        {
            SetMuteState(kMuteAll);
            audio_muted_on_creation = false;
        }
    }

    if (audioOutput)
    {
        const AudioSettings settings(audio_bits, audio_channels, audio_codec,
                                     audio_samplerate, audio_passthru);
        audioOutput->Reconfigure(settings);
        if (audio_passthru)
            audio_channels = 2;
        errMsg = audioOutput->GetError();
        if (!errMsg.isEmpty())
            audioOutput->SetStretchFactor(audio_stretchfactor);
    }

    return errMsg;
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

FrameScanType NuppelVideoPlayer::detectInterlace(FrameScanType newScan,
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

    VERBOSE(VB_PLAYBACK, dbg+toQString(scan));

    return scan;
}

void NuppelVideoPlayer::SetKeyframeDistance(int keyframedistance)
{
    keyframedist = (keyframedistance > 0) ? keyframedistance : keyframedist;
}

/** \fn NuppelVideoPlayer::FallbackDeint(void)
 *  \brief Fallback to non-frame-rate-doubling deinterlacing method.
 */
void NuppelVideoPlayer::FallbackDeint(void)
{
     m_double_framerate = false;
     m_double_process   = false;

     if (videosync)
         videosync->SetFrameInterval(frame_interval, false);

     if (osd)
         osd->SetFrameInterval(frame_interval);

     if (videoOutput)
         videoOutput->FallbackDeint();
}

void NuppelVideoPlayer::AutoDeint(VideoFrame *frame)
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
            if (!player_ctx->buffer->isDVD())
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

    int min_count = (player_ctx->buffer->isDVD()) ? 0 : 2;
    if (abs(m_scan_tracker) <= min_count)
        return;

    SetScanType((m_scan_tracker > min_count) ? kScan_Interlaced : kScan_Progressive);
    m_scan_locked  = false;
}

void NuppelVideoPlayer::SetScanType(FrameScanType scan)
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
        videosync->SetFrameInterval(frame_interval, false);
        return;
    }

    if (interlaced)
    {
        m_deint_possible = videoOutput->SetDeinterlacingEnabled(true);
        if (!m_deint_possible)
        {
            VERBOSE(VB_IMPORTANT, "Failed to enable deinterlacing");
            m_scan = scan;
            return;
        }
        if (videoOutput->NeedsDoubleFramerate())
        {
            m_double_framerate = true;
            videosync->SetFrameInterval(frame_interval, true);
            // Make sure video sync can double frame rate
            m_can_double = videosync->UsesFieldInterval();
            if (!m_can_double)
            {
                VERBOSE(VB_IMPORTANT, "Video sync method can't support double "
                        "framerate (refresh rate too low for bob deint)");
                FallbackDeint();
            }
        }
        m_double_process = videoOutput->IsExtraProcessingRequired();
        VERBOSE(VB_PLAYBACK, "Enabled deinterlacing");
    }
    else
    {
        if (kScan_Progressive == scan)
        {
            m_double_process = false;
            m_double_framerate = false;
            videosync->SetFrameInterval(frame_interval, false);
            videoOutput->SetDeinterlacingEnabled(false);
            VERBOSE(VB_PLAYBACK, "Disabled deinterlacing");
        }
    }

    if (osd)
    {
        osd->SetFrameInterval(
            (m_double_framerate && m_double_process) ?
            (frame_interval>>1) : frame_interval);
    }

    m_scan = scan;
}

void NuppelVideoPlayer::SetVideoParams(int width, int height, double fps,
                                       int keyframedistance, float aspect,
                                       FrameScanType scan, bool video_codec_changed)
{
    if (width == 0 || height == 0 || isnan(aspect) || isnan(fps))
        return;

    if ((video_disp_dim == QSize(width, height)) &&
        (video_aspect == aspect) && (video_frame_rate == fps   ) &&
        ((keyframedistance <= 0) || (keyframedistance == keyframedist)) &&
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
            audio_stretchfactor : play_speed;
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

void NuppelVideoPlayer::SetFileLength(int total, int frames)
{
    totalLength = total;
    totalFrames = frames;
}

void NuppelVideoPlayer::OpenDummy(void)
{
    isDummy = true;

    float displayAspect =
        gContext->GetFloatSettingOnHost("XineramaMonitorAspectRatio",
                                        gContext->GetHostName(), 1.3333);

    if (!videoOutput)
    {
        SetVideoParams(720, 576, 25.00, 15, displayAspect);
    }

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    DummyDecoder *dec = new DummyDecoder(this, *(player_ctx->playingInfo));
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    SetDecoder(dec);
}

int NuppelVideoPlayer::OpenFile(bool skipDsp, uint retries,
                                bool allow_libmpeg2)
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
    player_ctx->buffer->Unpause(); // so we can read testbuf if we were paused

    // delete any pre-existing recorder
    SetDecoder(NULL);
    int testreadsize = 2048;

    while (testreadsize <= kDecoderProbeBufferSize)
    {
        if (player_ctx->buffer->Peek(testbuf, testreadsize) != testreadsize)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("NVP::OpenFile(): Error, couldn't read file: %1")
                    .arg(player_ctx->buffer->GetFilename()));
            return -1;
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
                                           allow_libmpeg2, noaccel));
        }
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        if (GetDecoder())
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

    if (audio_bits == -1)
        no_audio_in = no_audio_out = true;

    if (ret > 0)
    {
        hasFullPositionMap = true;

        LoadCutList();

        if (!deleteMap.isEmpty())
        {
            hasdeletetable = true;
            deleteIter = deleteMap.begin();
        }
    }

    if (player_ctx->buffer->isDVD())
        player_ctx->buffer->DVD()->JumpToTitle(true);

    bookmarkseek = GetBookmark();

    return IsErrored() ? -1 : 0;
}

void NuppelVideoPlayer::SetVideoFilters(const QString &override)
{
    videoFiltersOverride = override;
    videoFiltersOverride.detach();

    videoFiltersForProgram = player_ctx->GetFilters(
                             (using_null_videoout) ? "onefield" : "");
}

void NuppelVideoPlayer::InitFilters(void)
{
    QString filters = "";
    if (videoOutput)
        filters = videoOutput->GetFilters();

#if 0
    VERBOSE(VB_PLAYBACK, LOC +
            QString("InitFilters() vo '%1' prog '%2' over '%3'")
            .arg(filters).arg(videoFiltersForProgram)
            .arg(videoFiltersOverride));
#endif

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

int NuppelVideoPlayer::tbuffer_numvalid(void)
{
    /* thread safe, returns number of valid slots in the text buffer */
    int ret;
    text_buflock.lock();

    if (wtxt >= rtxt)
        ret = wtxt - rtxt;
    else
        ret = MAXTBUFFER - (rtxt - wtxt);

    text_buflock.unlock();
    return ret;
}

int NuppelVideoPlayer::tbuffer_numfree(void)
{
    return MAXTBUFFER - tbuffer_numvalid() - 1;
    /* There's one wasted slot, because the case when rtxt = wtxt is
       interpreted as an empty buffer. So the fullest the buffer can be is
       MAXTBUFFER - 1. */
}

/** \fn NuppelVideoPlayer::GetNextVideoFrame(bool)
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
VideoFrame *NuppelVideoPlayer::GetNextVideoFrame(bool allow_unsafe)
{
    return videoOutput->GetNextFreeFrame(false, allow_unsafe);
}

/** \fn NuppelVideoPlayer::ReleaseNextVideoFrame(VideoFrame*,long long)
 *  \brief Places frame on the queue of frames ready for display.
 */
void NuppelVideoPlayer::ReleaseNextVideoFrame(VideoFrame *buffer,
                                              long long timecode)
{
    if (!player_ctx->buffer->InDVDMenuOrStillFrame())
        WrapTimecode(timecode, TC_VIDEO);
    buffer->timecode = timecode;

    videoOutput->ReleaseFrame(buffer);

    detect_letter_box->Detect(buffer);
}

/** \fn NuppelVideoPlayer::DiscardVideoFrame(VideoFrame*)
 *  \brief Places frame in the available frames queue.
 */
void NuppelVideoPlayer::DiscardVideoFrame(VideoFrame *buffer)
{
    if (videoOutput)
        videoOutput->DiscardFrame(buffer);
}

/** \fn NuppelVideoPlayer::DiscardVideoFrames(bool)
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
void NuppelVideoPlayer::DiscardVideoFrames(bool next_frame_keyframe)
{
    if (videoOutput)
        videoOutput->DiscardFrames(next_frame_keyframe);
}

void NuppelVideoPlayer::DrawSlice(VideoFrame *frame, int x, int y, int w, int h)
{
    videoOutput->DrawSlice(frame, x, y, w, h);
}

void NuppelVideoPlayer::AddTextData(unsigned char *buffer, int len,
                                    long long timecode, char type)
{
    WrapTimecode(timecode, TC_CC);

    if (!(textDisplayMode & kDisplayNUVCaptions))
        return;

    if (!tbuffer_numfree())
    {
        VERBOSE(VB_IMPORTANT, "NVP::AddTextData(): Text buffer overflow");
        return;
    }

    if (len > text_size)
        len = text_size;

    txtbuffers[wtxt].timecode = timecode;
    txtbuffers[wtxt].type = type;
    txtbuffers[wtxt].len = len;
    memset(txtbuffers[wtxt].buffer, 0, text_size);
    memcpy(txtbuffers[wtxt].buffer, buffer, len);

    text_buflock.lock();
    wtxt = (wtxt+1) % MAXTBUFFER;
    text_buflock.unlock();
}

void NuppelVideoPlayer::CheckPrebuffering(void)
{
    if (videoOutput->hasHWAcceleration() &&
        videoOutput->EnoughPrebufferedFrames())
    {
        SetPrebuffering(false);
    }

#if FAST_RESTART
    if (videoOutput->EnoughPrebufferedFrames())
        SetPrebuffering(false);
#else
    if (videoOutput->EnoughDecodedFrames())
        SetPrebuffering(false);
#endif
}

bool NuppelVideoPlayer::GetFrameNormal(DecodeType decodetype)
{
    if (!GetDecoder()->GetFrame(decodetype))
        return false;

    CheckPrebuffering();

    if ((play_speed > 1.01f) && (audio_stretchfactor > 1.01f) &&
        (livetv || (watchingrecording &&
                    player_ctx->recorder &&
                    player_ctx->recorder->IsValidRecorder())) &&
        IsNearEnd())
    {
        VERBOSE(VB_PLAYBACK, LOC + "Near end, Slowing down playback.");
        Play(1.0f, true, true);
    }

    return true;
}

bool NuppelVideoPlayer::GetFrameFFREW(void)
{
    bool stopFFREW = false;

    if (player_ctx->buffer->isDVD() && GetDecoder())
        GetDecoder()->UpdateDVDFramesPlayed();

    if (ffrew_skip > 0)
    {
        long long delta = GetDecoder()->GetFramesRead() - framesPlayed;
        long long real_skip = CalcMaxFFTime(ffrew_skip + delta) - delta;
        if (real_skip >= 0)
        {
            long long frame = GetDecoder()->GetFramesRead() + real_skip;
            GetDecoder()->DoFastForward(frame, false);
        }
        stopFFREW = (CalcMaxFFTime(100, false) < 100);
    }
    else if (CalcRWTime(-ffrew_skip) >= 0)
    {
        long long curFrame  = GetDecoder()->GetFramesRead();
        bool      toBegin   = -curFrame > ffrew_skip;
        long long real_skip = (toBegin) ? -curFrame : ffrew_skip;
        GetDecoder()->DoRewind(curFrame + real_skip, false);
        if (player_ctx->buffer->isDVD())
            stopFFREW = (player_ctx->buffer->DVD()->GetCurrentTime() < 2);
        else
            stopFFREW = framesPlayed <= keyframedist;
    }

    if (stopFFREW)
    {
        float stretch = (ffrew_skip > 0) ? 1.0f : audio_stretchfactor;
        Play(stretch, true, true);
    }

    bool ret = GetDecoder()->GetFrame(kDecodeVideo);
    CheckPrebuffering();
    return ret;
}

bool NuppelVideoPlayer::GetFrame(DecodeType decodetype, bool unsafe)
{
    bool ret = false;

    // Wait for frames to be available for decoding onto
    if (!videoOutput->EnoughFreeFrames() && !unsafe)
    {
        //VERBOSE(VB_PLAYBACK, "Waiting for video buffer to drain.");
        SetPrebuffering(false);
        if (!videoOutput->WaitForAvailable(10) &&
            !videoOutput->EnoughFreeFrames())
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

    // Decode the correct frame
    if (!GetDecoder())
        VERBOSE(VB_IMPORTANT, LOC + "GetFrame() called with NULL decoder.");
    else if (ffrew_skip == 1)
        ret = GetFrameNormal(decodetype);
    else
        ret = GetFrameFFREW();

    return ret;
}

VideoFrame *NuppelVideoPlayer::GetCurrentFrame(int &w, int &h)
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

void NuppelVideoPlayer::ReleaseCurrentFrame(VideoFrame *frame)
{
    if (frame)
        vidExitLock.unlock();
}

void NuppelVideoPlayer::ShutdownYUVResize(void)
{
    if (yuv_frame_scaled)
    {
        delete [] yuv_frame_scaled;
        yuv_frame_scaled = NULL;
    }

    if (yuv_scaler)
    {
        sws_freeContext(yuv_scaler);
        yuv_scaler = NULL;
    }

    yuv_scaler_in_size  = QSize(0,0);
    yuv_scaler_out_size = QSize(0,0);
}

/** \fn NuppelVideoPlayer::GetScaledFrame(QSize&)
 *  \brief Returns a scaled version of the latest decoded frame.
 *
 *   The buffer returned is only valid until the next call to this function
 *   or GetARGBFrame(QSize&).
 *   If the size width and height are not multiples of two they will be
 *   scaled down to the nearest multiple of two.
 *
 *  \param size Used the pass the desired size of the frame in, and
 *              will contain the actual size of the returned image.
 */
const unsigned char *NuppelVideoPlayer::GetScaledFrame(QSize &size)
{
    QMutexLocker locker(&yuv_lock);
    yuv_desired_size = size = QSize(size.width() & ~0x7, size.height() & ~0x7);

    if ((size.width() > 0) && (size.height() > 0))
    {
        yuv_need_copy = true;
        while (yuv_wait.wait(locker.mutex(), 50) && yuv_need_copy);
        return yuv_frame_scaled;
    }
    return NULL;
}

/** \fn NuppelVideoPlayer::GetARGBFrame(QSize&)
 *  \brief Returns a QImage scaled to near the size specified in 'size'
 *
 *   The QImage returned is only valid until the next call to this function
 *   or GetScaledFrame(QSize&).
 *   And if the size width and height are not multiples of two they will be
 *   scaled down to the nearest multiple of two.
 *
 *  \param size Used the pass the desired size of the frame in, and
 *              will contain the actual size of the returned image.
 */
const QImage &NuppelVideoPlayer::GetARGBFrame(QSize &size)
{
    unsigned char *yuv_buf = (unsigned char*) GetScaledFrame(size);
    if (!yuv_buf)
        return argb_scaled_img;

    if (argb_size != size)
    {
        if (argb_buf)
            delete [] argb_buf;
        argb_buf = new unsigned char[(size.width() * size.height() * 4) + 128];
        argb_size = QSize(size.width(), size.height());
    }

    uint w = argb_size.width(), h = argb_size.height();
    yuv2argb_conv(argb_buf,
                  yuv_buf, yuv_buf + (w * h), yuv_buf + (w * h * 5 / 4),
                  w, h, w * 4, w, w / 2, 0);

    argb_scaled_img = QImage(argb_buf, argb_size.width(), argb_size.height(),
                             QImage::Format_RGB32);

    return argb_scaled_img;
}

void NuppelVideoPlayer::EmbedInWidget(int x, int y, int w, int h, WId id)
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

void NuppelVideoPlayer::StopEmbedding(void)
{
    if (videoOutput)
    {
        videoOutput->StopEmbedding();
        ReinitOSD();
    }
}

void NuppelVideoPlayer::WindowResized(const QSize &new_size)
{
    if (videoOutput)
        videoOutput->WindowResized(new_size);
}

void NuppelVideoPlayer::ResetCaptions(uint mode_override)
{
    uint origMode   = textDisplayMode;
    uint mode       = (mode_override) ? mode_override : origMode;
    textDisplayMode = kDisplayNone;

    // resets EIA-608 and Teletext NUV Captions
    if (mode & kDisplayNUVCaptions)
        ResetCC();

    // resets EIA-708
    uint i = (mode & kDisplayCC708) ? 1 : 64;
    for (; i < 64; i++)
        DeleteWindows(i, 0xff);

    textDisplayMode = origMode;
}

// caller has decoder_changed_lock
void NuppelVideoPlayer::DisableCaptions(uint mode, bool osd_msg)
{
    textDisplayMode &= ~mode;

    ResetCaptions(mode);

    if (!osd || !osd_msg)
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
    if (kDisplayAVSubtitle & mode)
    {
        msg += decoder->GetTrackDesc(kTrackTypeSubtitle,
                                     GetTrack(kTrackTypeSubtitle));
        if (player_ctx->buffer->isDVD())
            player_ctx->buffer->DVD()->SetTrack(kTrackTypeSubtitle, -1);
    }
    if (kDisplayTextSubtitle & mode)
    {
        msg += QObject::tr("Text subtitles");
    }
    if (kDisplayCC608 & mode)
    {
        msg += decoder->GetTrackDesc(kTrackTypeCC608,
                                     GetTrack(kTrackTypeCC608));
    }
    if (kDisplayCC708 & mode)
    {
        msg += decoder->GetTrackDesc(kTrackTypeCC708,
                                     GetTrack(kTrackTypeCC708));
    }

    if (! msg.isEmpty())
    {
        msg += " " + QObject::tr("Off");
        osd->SetSettingsText(msg, 3 /* seconds until message timeout */);
    }
}

// caller has decoder_changed_lock
void NuppelVideoPlayer::EnableCaptions(uint mode, bool osd_msg)
{
    QString msg = "";
    if (kDisplayAVSubtitle & mode)
    {
        msg += decoder->GetTrackDesc(kTrackTypeSubtitle,
                                     GetTrack(kTrackTypeSubtitle));

        if (player_ctx->buffer->isDVD() && osd_msg)
            player_ctx->buffer->DVD()->SetTrack(kTrackTypeSubtitle,
                                        GetTrack(kTrackTypeSubtitle));
    }
    if (kDisplayTextSubtitle & mode)
    {
        msg += QObject::tr("Text Subtitles");
    }
    if (kDisplayNUVTeletextCaptions & mode)
        msg += QObject::tr("TXT") + QString(" %1").arg(ttPageNum, 3, 16);
    if (kDisplayCC608 & mode)
    {
        msg += decoder->GetTrackDesc(kTrackTypeCC608,
                                     GetTrack(kTrackTypeCC608));
    }
    if (kDisplayCC708 & mode)
    {
        msg += decoder->GetTrackDesc(kTrackTypeCC708,
                                     GetTrack(kTrackTypeCC708));
    }
    if (kDisplayTeletextCaptions & mode)
    {
        msg += decoder->GetTrackDesc(kTrackTypeTeletextCaptions,
                                     GetTrack(kTrackTypeTeletextCaptions));

        int page = decoder->GetTrackLanguageIndex(
            kTrackTypeTeletextCaptions,
            GetTrack(kTrackTypeTeletextCaptions));

        TeletextViewer *tt_view = NULL;
        if (osd && (tt_view = osd->GetTeletextViewer()) && (page > 0))
        {
            EnableTeletext();
            tt_view->SetPage(page, -1);
            textDisplayMode = kDisplayTeletextCaptions;
        }
    }

    msg += " " + QObject::tr("On");

    textDisplayMode = mode;
    if (osd && osd_msg)
        osd->SetSettingsText(msg, 3 /* seconds until message timeout */);

}

void NuppelVideoPlayer::EnableTeletext(void)
{
    if (!GetOSD())
        return;

    OSDSet         *oset    = GetOSD()->GetSet("teletext");
    TeletextViewer *tt_view = GetOSD()->GetTeletextViewer();
    if (oset && tt_view)
    {
        decoder->SetTeletextDecoderViewer(tt_view);
        tt_view->SetDisplaying(true);
        tt_view->SetPage(0x100, -1);
        oset->Display();
        osd->SetVisible(oset, 0);
        prevTextDisplayMode = textDisplayMode;
        textDisplayMode = kDisplayTeletextMenu;
    }
}

void NuppelVideoPlayer::DisableTeletext(void)
{
    if (!GetOSD())
        return;

    TeletextViewer *tt_view = GetOSD()->GetTeletextViewer();
    if (tt_view)
        tt_view->SetDisplaying(false);
    GetOSD()->HideSet("teletext");

    textDisplayMode = kDisplayNone;

    /* If subtitles are enabled before the teletext menu was displayed,
       re-enabled them. */
    if (prevTextDisplayMode & kDisplayAllCaptions)
        EnableCaptions(prevTextDisplayMode, false);
}

void NuppelVideoPlayer::ResetTeletext(void)
{
    if (!GetOSD())
        return;

    TeletextViewer *tt_view = GetOSD()->GetTeletextViewer();
    if (tt_view)
        tt_view->Reset();
}

bool NuppelVideoPlayer::ToggleCaptions(void)
{
    SetCaptionsEnabled(!((bool)textDisplayMode));
    return textDisplayMode;
}

bool NuppelVideoPlayer::ToggleCaptions(uint type)
{
    uint mode = track_type_to_display_mode[type];
    uint origMode = textDisplayMode;

    QMutexLocker locker(&decoder_change_lock);

    if (textDisplayMode)
        DisableCaptions(textDisplayMode, origMode & mode);

    if (origMode & mode)
        return textDisplayMode;

    if (kDisplayNUVTeletextCaptions & mode)
        EnableCaptions(kDisplayNUVTeletextCaptions);

    if (kDisplayCC608 & mode)
        EnableCaptions(kDisplayCC608);

    if (kDisplayCC708 & mode)
        EnableCaptions(kDisplayCC708);

    if (kDisplayAVSubtitle & mode)
        EnableCaptions(kDisplayAVSubtitle);

    if (kDisplayTextSubtitle & mode)
        EnableCaptions(kDisplayTextSubtitle);

    if (kDisplayTeletextCaptions & mode)
        EnableCaptions(kDisplayTeletextCaptions);

    return textDisplayMode;
}

void NuppelVideoPlayer::SetCaptionsEnabled(bool enable, bool osd_msg)
{
    uint origMode = textDisplayMode;

    textDesired = enable;

    QMutexLocker locker(&decoder_change_lock);

    if (!enable)
    {
        DisableCaptions(origMode, osd_msg);
        return;
    }

    // figure out which text type to enable..
    bool captions_found = true;
    if (decoder->GetTrackCount(kTrackTypeSubtitle))
        EnableCaptions(kDisplayAVSubtitle, osd_msg);
    else if (textSubtitles.GetSubtitleCount() > 0)
        EnableCaptions(kDisplayTextSubtitle, osd_msg);
    else if (db_prefer708 && decoder->GetTrackCount(kTrackTypeCC708))
        EnableCaptions(kDisplayCC708, osd_msg);
    else if (decoder->GetTrackCount(kTrackTypeTeletextCaptions))
        EnableCaptions(kDisplayTeletextCaptions, osd_msg);
    else if (vbimode == VBIMode::PAL_TT)
        EnableCaptions(kDisplayNUVTeletextCaptions, osd_msg);
    else if (vbimode == VBIMode::NTSC_CC)
    {
        if (decoder->GetTrackCount(kTrackTypeCC608))
            EnableCaptions(kDisplayCC608, osd_msg);
        else
            captions_found = false;
    }
    else if (!db_prefer708 && decoder->GetTrackCount(kTrackTypeCC708))
        EnableCaptions(kDisplayCC708, osd_msg);
    else
        captions_found = false;

    if (!captions_found && osd && osd_msg)
    {
        QString msg = QObject::tr(
            "No captions", "CC/Teletext/Subtitle text not available");
        osd->SetSettingsText(msg, 3 /* seconds until message timeout */);
    }


    // Reset captions, disable old captions on change
    ResetCaptions(origMode);
    if (origMode != textDisplayMode)
        DisableCaptions(origMode, false);
}

/** \fn NuppelVideoPlayer::SetTeletextPage(uint)
 *  \brief Set Teletext NUV Caption page
 */
void NuppelVideoPlayer::SetTeletextPage(uint page)
{
    QMutexLocker locker(&decoder_change_lock);

    DisableCaptions(textDisplayMode);
    ttPageNum = page;
    textDisplayMode &= ~kDisplayAllCaptions;
    textDisplayMode |= kDisplayNUVTeletextCaptions;
}

bool NuppelVideoPlayer::HandleTeletextAction(const QString &action)
{
    if (!(textDisplayMode & kDisplayTeletextMenu) || !GetOSD())
        return false;

    bool handled = true;

    TeletextViewer *tt = GetOSD()->GetTeletextViewer();
    if (!tt)
        return false;

    if (action == "NEXTPAGE")
        tt->KeyPress(TTKey::kNextPage);
    else if (action == "PREVPAGE")
        tt->KeyPress(TTKey::kPrevPage);
    else if (action == "NEXTSUBPAGE")
        tt->KeyPress(TTKey::kNextSubPage);
    else if (action == "PREVSUBPAGE")
        tt->KeyPress(TTKey::kPrevSubPage);
    else if (action == "TOGGLEBACKGROUND")
        tt->KeyPress(TTKey::kTransparent);
    else if (action == "MENURED")
        tt->KeyPress(TTKey::kFlofRed);
    else if (action == "MENUGREEN")
        tt->KeyPress(TTKey::kFlofGreen);
    else if (action == "MENUYELLOW")
        tt->KeyPress(TTKey::kFlofYellow);
    else if (action == "MENUBLUE")
        tt->KeyPress(TTKey::kFlofBlue);
    else if (action == "MENUWHITE")
        tt->KeyPress(TTKey::kFlofWhite);
    else if (action == "REVEAL")
        tt->KeyPress(TTKey::kRevealHidden);
    else if (action == "0" || action == "1" || action == "2" ||
             action == "3" || action == "4" || action == "5" ||
             action == "6" || action == "7" || action == "8" ||
             action == "9")
        tt->KeyPress(action.toInt());
    else if (action == "MENU" || action == "TOGGLETT" ||
             action == "ESCAPE")
        DisableTeletext();
    else
        handled = false;

    return handled;
}

/** \fn NuppelVideoPlayer::ShowText(void)
 *  \brief Displays Teletext and EIA-608 style text captions.
 */
void NuppelVideoPlayer::ShowText(void)
{
    VideoFrame *last = videoOutput->GetLastShownFrame();

    // check if subtitles need to be updated on the OSD
    if (osd && tbuffer_numvalid() && txtbuffers[rtxt].timecode &&
        (last && txtbuffers[rtxt].timecode <= last->timecode))
    {
        if (txtbuffers[rtxt].type == 'T')
        {
            // display full page of teletext
            //
            // all formatting is always defined in the page itself,
            // if scrolling is needed for live closed captions this
            // is handled by the broadcaster:
            // the pages are then very often transmitted (sometimes as often as
            // every 2 frames) with small differences between them
            unsigned char *inpos = txtbuffers[rtxt].buffer;
            int pagenr;
            memcpy(&pagenr, inpos, sizeof(int));
            inpos += sizeof(int);

            if (pagenr == (ttPageNum<<16))
            {
                // show teletext subtitles
                osd->ClearAllCCText();
                (*inpos)++;
                while (*inpos)
                {
                    struct teletextsubtitle st;
                    memcpy(&st, inpos, sizeof(st));
                    inpos += sizeof(st);
                    QString s((const char*) inpos);
                    osd->AddCCText(s, st.row, st.col, st.fg, true);
                    inpos += st.len;
                }
            }
        }
        else if (txtbuffers[rtxt].type == 'C')
        {
            UpdateCC(txtbuffers[rtxt].buffer);
        }

        text_buflock.lock();
        if (rtxt != wtxt) // if a seek occurred, rtxt == wtxt, in this case do
                          // nothing
            rtxt = (rtxt + 1) % MAXTBUFFER;
        text_buflock.unlock();
    }
}

/** \fn NuppelVideoPlayer::ResetCC(void)
 *  \brief Resets Teletext and EIA-608 style text caption buffers.
 */
void NuppelVideoPlayer::ResetCC(void)
{
    ccline = "";
    cccol = 0;
    ccrow = 0;
    if (osd)
        osd->ClearAllCCText();
}

/** \fn NuppelVideoPlayer::ResetCC(void)
 *  \brief Adds EIA-608 style text caption to buffers.
 */
void NuppelVideoPlayer::UpdateCC(unsigned char *inpos)
{
    struct ccsubtitle subtitle;

    memcpy(&subtitle, inpos, sizeof(subtitle));
    inpos += sizeof(ccsubtitle);

    // skip undisplayed streams
    if ((subtitle.resumetext & CC_MODE_MASK) != ccmode)
        return;

    if (subtitle.row == 0)
        subtitle.row = 1;

    if (subtitle.clr)
    {
        //printf ("erase displayed memory\n");
        ResetCC();
        if (!subtitle.len)
            return;
    }

//    if (subtitle.len || !subtitle.clr)
    {
        unsigned char *end = inpos + subtitle.len;
        int row = 0;
        int linecont = (subtitle.resumetext & CC_LINE_CONT);

        vector<ccText*> *ccbuf = new vector<ccText*>;
        vector<ccText*>::iterator ccp;
        ccText *tmpcc = NULL;
        int replace = linecont;
        int scroll = 0;
        bool scroll_prsv = false;
        int scroll_yoff = 0;
        int scroll_ymax = 15;

        do
        {
            if (linecont)
            {
                // append to last line; needs to be redrawn
                replace = 1;
                // backspace into existing line if needed
                int bscnt = 0;
                while ((inpos < end) && *inpos != 0 && (char)*inpos == '\b')
                {
                    bscnt++;
                    inpos++;
                }
                if (bscnt)
                    ccline.remove(ccline.length() - bscnt, bscnt);
            }
            else
            {
                // new line:  count spaces to calculate column position
                row++;
                cccol = 0;
                ccline = "";
                while ((inpos < end) && *inpos != 0 && (char)*inpos == ' ')
                {
                    inpos++;
                    cccol++;
                }
            }

            ccrow = subtitle.row;
            unsigned char *cur = inpos;

            //null terminate at EOL
            while (cur < end && *cur != '\n' && *cur != 0)
                cur++;
            *cur = 0;

            if (*inpos != 0 || linecont)
            {
                if (linecont)
                    ccline += QString::fromUtf8((const char *)inpos, -1);
                else
                    ccline = QString::fromUtf8((const char *)inpos, -1);
                tmpcc = new ccText();
                tmpcc->text = ccline;
                tmpcc->x = cccol;
                tmpcc->y = ccrow;
                tmpcc->color = 0;
                tmpcc->teletextmode = false;
                ccbuf->push_back(tmpcc);
#if 0
                if (ccbuf->size() > 4)
                {
                    QByteArray ccl = ccline.toAscii();
                    printf("CC overflow:  ");
                    printf("%d %d %s\n", cccol, ccrow, ccl.constData());
                }
#endif
            }
            subtitle.row++;
            inpos = cur + 1;
            linecont = 0;
        } while (inpos < end);

        // adjust row position
        if (subtitle.resumetext & CC_TXT_MASK)
        {
            // TXT mode
            // - can use entire 15 rows
            // - scroll up when reaching bottom
            if (ccrow > 15)
            {
                if (row)
                    scroll = ccrow - 15;
                if (tmpcc)
                    tmpcc->y = 15;
            }
        }
        else if (subtitle.rowcount == 0 || row > 1)
        {
            // multi-line text
            // - fix display of old (badly-encoded) files
            if (ccrow > 15)
            {
                ccp = ccbuf->begin();
                for (; ccp != ccbuf->end(); ccp++)
                {
                    tmpcc = *ccp;
                    tmpcc->y -= (ccrow - 15);
                }
            }
        }
        else
        {
            // scrolling text
            // - scroll up previous lines if adding new line
            // - if caption is at bottom, row address is for last
            // row
            // - if caption is at top, row address is for first row (?)
            if (subtitle.rowcount > 4)
                subtitle.rowcount = 4;
            if (ccrow < subtitle.rowcount)
            {
                ccrow = subtitle.rowcount;
                if (tmpcc)
                    tmpcc->y = ccrow;
            }
            if (row)
            {
                scroll = row;
                scroll_prsv = true;
                scroll_yoff = ccrow - subtitle.rowcount;
                scroll_ymax = ccrow;
            }
        }

        if (osd)
            osd->UpdateCCText(ccbuf, replace, scroll,
                              scroll_prsv, scroll_yoff, scroll_ymax);
        delete ccbuf;
    }
}

/// Max amount the warpfactor can change in 1 frame.
#define MAXWARPDIFF 0.0005f
/// How long to average the warp over.
#define WARPAVLEN (video_frame_rate * 600)
/** How much we allow the warp to deviate from 1.0 (normal speed). */
#define WARPCLIP    0.1f
/** Number of frames of A/V divergence allowed adjustments are made. */
#define MAXDIVERGE  3.0f
/** A/V divergence above this amount is clipped to avoid a bad stream
 *  causing large playback glitches. */
#define DIVERGELIMIT 30.0f

float NuppelVideoPlayer::WarpFactor(void)
{
    // Calculate a new warp factor
    float   divergence;
    float   rate;
    float   newwarp = 1;
    float   warpdiff;

    // Number of frames the audio is out by
    divergence = (float)avsync_avg / (float)frame_interval;
    // Number of frames divergence is changing by per frame
    rate = (float)(avsync_avg - avsync_oldavg) / (float)frame_interval;
    avsync_oldavg = avsync_avg;
    newwarp = warpfactor_avg * (1 + ((divergence + rate) / 125));

    // Clip the amount changed so we don't get big frequency variations
    warpdiff = newwarp / warpfactor;
    if (warpdiff > (1 + MAXWARPDIFF))
        newwarp = warpfactor * (1 + MAXWARPDIFF);
    else if (warpdiff < (1 - MAXWARPDIFF))
        newwarp = warpfactor * (1 - MAXWARPDIFF);

    warpfactor = newwarp;

    // Clip final warp factor
    if (warpfactor < (1 - WARPCLIP))
        warpfactor = 1 - WARPCLIP;
    else if (warpfactor > (1 + (WARPCLIP * 2)))
        warpfactor = 1 + (WARPCLIP * 2);

    // Keep a 10 minute average
    warpfactor_avg = (warpfactor + (warpfactor_avg * (WARPAVLEN - 1))) /
                      WARPAVLEN;

    VERBOSE(VB_PLAYBACK+VB_TIMESTAMP,
            LOC + QString("A/V Divergence: %1, Rate: %2, Warpfactor: %3, "
                          "warpfactor_avg: %4")
            .arg(divergence).arg(rate).arg(warpfactor).arg(warpfactor_avg));

    return divergence;
}

void NuppelVideoPlayer::InitAVSync(void)
{
    videosync->Start();

    avsync_adjustment = 0;

    repeat_delay = 0;

    refreshrate = videoOutput ? videoOutput->GetDisplayInfo().rate : 0;
    if (refreshrate <= 0)
        refreshrate = frame_interval;
    vsynctol = refreshrate / 4;

    if (!using_null_videoout)
    {
        QString timing_type = videosync->getName();

        QString msg = QString("Video timing method: %1").arg(timing_type);
        VERBOSE(VB_GENERAL, msg);
        msg = QString("Refresh rate: %1, frame interval: %2")
                       .arg(refreshrate).arg(frame_interval);
        VERBOSE(VB_PLAYBACK, msg);

        // try to get preferential scheduling, but ignore if we fail to.
        myth_nice(-19);
    }
}

void NuppelVideoPlayer::AVSync(void)
{
    float diverge = 0.0f;
    // attempt to reduce fps for standalone PIP
    if (player_ctx->IsPIP() && framesPlayed % 2)
    {
        videosync->WaitForFrame(avsync_adjustment);
        videosync->AdvanceTrigger();
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

    // The warp calculation is only valid at "normal_speed" playback.
    if (normal_speed)
    {
        diverge = WarpFactor();
        // If we are WAY out of sync, we can't really adjust this much
        // so just adjust by DIVERGELIMIT and hope lip-sync remains.
        diverge = max(diverge, -DIVERGELIMIT);
        diverge = min(diverge, +DIVERGELIMIT);
    }

    FrameScanType ps = m_scan;
    if (kScan_Detect == m_scan || kScan_Ignore == m_scan)
        ps = kScan_Progressive;

    bool dropframe = false;
    if (diverge < -MAXDIVERGE)
    {
        dropframe = true;
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
            videoOutput->PrepareFrame(buffer, kScan_Intr2ndField);
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
            videoOutput->PrepareFrame(buffer, ps);

        VERBOSE(VB_PLAYBACK|VB_TIMESTAMP, QString("AVSync waitforframe %1 %2")
                .arg(avsync_adjustment).arg(m_double_framerate));
        videosync->WaitForFrame(avsync_adjustment + repeat_delay);
        VERBOSE(VB_PLAYBACK|VB_TIMESTAMP, "AVSync show");
        if (!resetvideo)
            videoOutput->Show(ps);

        if (videoOutput->IsErrored())
        {
            VERBOSE(VB_IMPORTANT, "NVP: Error condition detected "
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
                if (player_ctx->buffer->isDVD() &&
                    player_ctx->buffer->DVD()->InStillFrame() &&
                    videoOutput->ValidVideoFrames() < 3)
                {
                    videoOutput->ProcessFrame(buffer, NULL, NULL,
                                              pip_players, ps);
                }
                else
                {
                    videoOutput->ProcessFrame(
                        buffer, osd, videoFilters, pip_players, ps);
                }
                videofiltersLock.unlock();
            }

            if (buffer)
                videoOutput->PrepareFrame(buffer, ps);

            // Display the second field
            videosync->AdvanceTrigger();
#ifdef NEW_AVSYNC
            videosync->WaitForFrame(avsync_adjustment);
#else
            videosync->WaitForFrame(0);
#endif
            if (!resetvideo)
            {
                videoOutput->Show(ps);
            }
        }

        repeat_delay = frame_interval * buffer->repeat_pict * 0.5;

        if (repeat_delay)
            VERBOSE(VB_TIMESTAMP, QString("A/V repeat_pict, adding %1 repeat "
                    "delay").arg(repeat_delay));
    }
    else
    {
        videosync->WaitForFrame(0);
    }

    if (output_jmeter && output_jmeter->RecordCycleTime())
    {
        VERBOSE(VB_PLAYBACK+VB_TIMESTAMP, QString("A/V avsync_delay: %1, "
                "avsync_avg: %2, warpfactor: %3, warpfactor_avg: %4")
                .arg(avsync_delay / 1000).arg(avsync_avg / 1000)
                .arg(warpfactor).arg(warpfactor_avg));
    }

    if (!dropframe)
        videosync->AdvanceTrigger();
    avsync_adjustment = 0;

    if (diverge > MAXDIVERGE)
    {
        // If audio is way behind of video, adjust for it...
        // by cutting the frame rate in half for the length of this frame

#ifdef NEW_AVSYNC
        avsync_adjustment = refreshrate;
#else
        avsync_adjustment = frame_interval;
#endif
        lastsync = true;
        VERBOSE(VB_PLAYBACK, LOC +
                QString("Video is %1 frames ahead of audio,\n"
                        "\t\t\tdoubling video frame interval to slow down.").arg(diverge));
    }

    audio_lock.lock();
    if (audioOutput && normal_speed)
    {
        long long currentaudiotime = audioOutput->GetAudiotime();
        audio_lock.unlock();
#if 0
        VERBOSE(VB_PLAYBACK+VB_TIMESTAMP, QString(
                    "A/V timecodes audio %1 video %2 frameinterval %3 "
                    "avdel %4 avg %5 tcoffset %6")
                .arg(currentaudiotime)
                .arg(buffer->timecode)
                .arg(frame_interval)
                .arg(buffer->timecode - currentaudiotime)
                .arg(avsync_avg)
                .arg(tc_wrap[TC_AUDIO])
                 );
#endif
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
                //cerr << "+ ";
                videosync->AdvanceTrigger();
                if (m_double_framerate)
                    videosync->AdvanceTrigger();
            }
            prevrp = buffer->repeat_pict;

            avsync_delay = (buffer->timecode - currentaudiotime) * 1000;//usec
            // prevents major jitter when pts resets during dvd title
            if (avsync_delay > 2000000 && player_ctx->buffer->isDVD())
                avsync_delay = 90000;
            avsync_avg = (avsync_delay + (avsync_avg * 3)) / 4;

            /* If the audio time codes and video diverge, shift
               the video by one interlaced field (1/2 frame) */
            if (!lastsync)
            {
                if (avsync_avg > frame_interval * 3 / 2)
                {
                    avsync_adjustment = refreshrate;
                    lastsync = true;
                }
                else if (avsync_avg < 0 - frame_interval * 3 / 2)
                {
                    avsync_adjustment = -refreshrate;
                    lastsync = true;
                }
            }
            else
                lastsync = false;
        }
        else
        {
            avsync_avg = 0;
            avsync_oldavg = 0;
        }
    }
    else
        audio_lock.unlock();
}

void NuppelVideoPlayer::DisplayPauseFrame(void)
{
    if (!video_actually_paused)
        videoOutput->UpdatePauseFrame();

    if (resetvideo)
    {
        videoOutput->UpdatePauseFrame();
        resetvideo = false;
    }

    SetVideoActuallyPaused(true);

    if (videoOutput->IsErrored())
    {
        SetErrored(QObject::tr("Serious error detected in Video Output"));
        return;
    }

    if (player_ctx->buffer->isDVD() &&
        player_ctx->buffer->DVD()->InStillFrame())
    {
        SetScanType(kScan_Progressive);
    }

    DisplayDVDButton();

    videofiltersLock.lock();
    videoOutput->ProcessFrame(NULL, osd, videoFilters, pip_players);
    videofiltersLock.unlock();

    videoOutput->PrepareFrame(NULL, kScan_Ignore);
    videoOutput->Show(kScan_Ignore);
    videosync->Start();
}

bool NuppelVideoPlayer::PrebufferEnoughFrames(void)
{
    prebuffering_lock.lock();
    if (prebuffering)
    {
        if (player_ctx->buffer->InDVDMenuOrStillFrame() &&
            prebuffer_tries > 3)
        {
            prebuffering = false;
            prebuffer_tries = 0;
            prebuffer_tries_total = 0;
            prebuffering_lock.unlock();
            return true;
        }

        audio_lock.lock();
        if (!player_ctx->buffer->InDVDMenuOrStillFrame() &&
            !audio_paused && audioOutput)
        {
           if (prebuffering)
                audioOutput->Pause(prebuffering);
            audio_paused = prebuffering;
        }
        audio_lock.unlock();

        VERBOSE(VB_PLAYBACK, LOC + QString("Waiting for prebuffer.. %1 %2")
                .arg(prebuffer_tries_total + prebuffer_tries, 2)
                .arg(videoOutput->GetFrameStatus()));
        if (!prebuffering_wait.wait(&prebuffering_lock,
                                    frame_interval * 4 / 1000))
        {
            // timed out.. do we need to know?
        }
        ++prebuffer_tries;
        if (prebuffering && (prebuffer_tries >= 10))
        {
            prebuffer_tries_total += prebuffer_tries;
            prebuffer_tries = 0;
            VERBOSE(VB_IMPORTANT, LOC +
                    QString("Prebuffer wait timed out %1 times.")
                    .arg(prebuffer_tries_total));
            if (prebuffer_tries_total >= 100)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Timed out waiting for prebuffering too long. "
                        "Exiting..");
                SetErrored(QObject::tr(
                               "Video frame buffering failed "
                               "too many times."));
            }
            else if (!videoOutput->EnoughFreeFrames())
            {
                VERBOSE(VB_IMPORTANT, LOC + "Prebuffer wait timed out, and"
                        "\n\t\t\tthere are not enough free frames. "
                        "Discarding buffered frames.");
                // This call will result in some ugly frames, but allows us
                // to recover from serious problems if frames get leaked.
                DiscardVideoFrames(true);
            }
        }
        prebuffering_lock.unlock();
        if (normal_speed)
            videosync->Start();

        return false;
    }
    prebuffering_lock.unlock();

    //VERBOSE(VB_PLAYBACK, LOC + "fs: " + videoOutput->GetFrameStatus());
    if (videoOutput && !videoOutput->EnoughPrebufferedFrames())
    {
        VERBOSE(VB_GENERAL, LOC + "prebuffering pause");
        videoOutput->CheckFrameStates();

        SetPrebuffering(true);
#if FAST_RESTART
        if (!m_playing_slower && audio_channels <= 2)
        {
            m_stored_audio_stretchfactor = GetAudioStretchFactor();
            Play(m_stored_audio_stretchfactor * 0.8, true);
            m_playing_slower = true;
            VERBOSE(VB_GENERAL, "playing slower due to falling behind...");
        }
#endif
        return false;
    }

#if FAST_RESTART
    if (m_playing_slower && videoOutput->EnoughDecodedFrames())
    {
        Play(m_stored_audio_stretchfactor, true);
        m_playing_slower = false;
        VERBOSE(VB_GENERAL, "playing at normal speed from falling behind...");
    }
#endif

    prebuffering_lock.lock();
    prebuffer_tries = 0;
    prebuffer_tries_total = 0;
    prebuffering_lock.unlock();

    return true;
}

void NuppelVideoPlayer::DisplayNormalFrame(void)
{
    SetVideoActuallyPaused(paused);
    resetvideo = false;

    if (!player_ctx->buffer->InDVDMenuOrStillFrame() ||
        (player_ctx->buffer->DVD()->NumMenuButtons() > 0 &&
        player_ctx->buffer->DVD()->GetChapterLength() > 3))
    {
        if (paused || !PrebufferEnoughFrames())
        {
            // When going to switch channels
            if (paused)
            {
                usleep(frame_interval);
                if (player_ctx && !player_ctx->IsPIP())
                    DisplayPauseFrame();
            }
            return;
        }
    }

    videoOutput->StartDisplayingFrame();

    VideoFrame *frame = videoOutput->GetLastShownFrame();

    if (yuv_need_copy)
    {
        // We need to make a preview copy of the frame...
        QMutexLocker locker(&yuv_lock);
        QSize vsize = video_dim;
        if ((vsize            != yuv_scaler_in_size) ||
            (yuv_desired_size != yuv_scaler_out_size))
        {
            ShutdownYUVResize();

            uint sz = yuv_desired_size.width() * yuv_desired_size.height();
            yuv_frame_scaled = new unsigned char[(sz * 3 / 2) + 128];

            yuv_scaler_in_size  = vsize;
            yuv_scaler_out_size = yuv_desired_size;

            yuv_scaler = sws_getCachedContext(yuv_scaler,
                yuv_scaler_in_size.width(),  yuv_scaler_in_size.height(),
                PIX_FMT_YUV420P,
                yuv_scaler_out_size.width(), yuv_scaler_out_size.height(),
                PIX_FMT_YUV420P,
                SWS_FAST_BILINEAR, NULL, NULL, NULL);
        }

        AVPicture img_out, img_in;
        avpicture_fill(&img_out, yuv_frame_scaled, PIX_FMT_YUV420P,
                       yuv_scaler_out_size.width(),
                       yuv_scaler_out_size.height());
        avpicture_fill(&img_in, frame->buf, PIX_FMT_YUV420P,
                       yuv_scaler_in_size.width(),
                       yuv_scaler_in_size.height());

        sws_scale(yuv_scaler, img_in.data, img_in.linesize, 0,
                  yuv_scaler_in_size.height(),
                  img_out.data, img_out.linesize);
        yuv_need_copy = false;
        yuv_wait.wakeAll();
    }

    DisplayDVDButton();

#ifdef USING_MHEG
    // handle Interactive TV
    if (GetInteractiveTV() && GetDecoder())
    {
        QMutexLocker locker(&itvLock);

        OSD *osd = GetOSD();
        if (osd)
        {
            OSDSet *itvosd = osd->GetSet("interactive");

            if (itvosd)
            {
                bool visible = false;
                if (interactiveTV->ImageHasChanged() || !itvVisible)
                {
                    interactiveTV->UpdateOSD(itvosd);
                    visible = true;
                    itvVisible = true;
                }

                if (visible)
                    osd->SetVisible(itvosd, 0);
            }
        }
    }
#endif // USING_MHEG

    // handle EIA-608 and Teletext
    if (textDisplayMode & kDisplayNUVCaptions)
        ShowText();

    // handle DVB/DVD subtitles decoded by ffmpeg (in AVSubtitle format)
    if (ffrew_skip == 1)
    {
        if (textDisplayMode & kDisplayAVSubtitle)
            DisplayAVSubtitles();
        else if (textDisplayMode & kDisplayTextSubtitle)
            DisplayTextSubtitles();
        else if (osdHasSubtitles)
            ClearSubtitles();
        else
            ExpireSubtitles();
    }

    // handle scan type changes
    AutoDeint(frame);

    detect_letter_box->SwitchTo(frame);

    FrameScanType ps = m_scan;
    if (kScan_Detect == m_scan || kScan_Ignore == m_scan)
        ps = kScan_Progressive;

    videofiltersLock.lock();
    if (player_ctx->buffer->isDVD() &&
        player_ctx->buffer->DVD()->InStillFrame() &&
        videoOutput->ValidVideoFrames() < 3)
    {
        videoOutput->ProcessFrame(frame, NULL, NULL, pip_players, ps);
    }
    else
    {
        videoOutput->ProcessFrame(frame, osd, videoFilters, pip_players, ps);
    }
    videofiltersLock.unlock();

    // NB This should probably be locked via audio_lock but doing so
    //    here causes significant playback problems
    if (audioOutput && !audio_paused && audioOutput->IsPaused())
        audioOutput->Pause(false);

    AVSync();

    videoOutput->DoneDisplayingFrame(frame);
}

void NuppelVideoPlayer::OutputVideoLoop(void)
{
    delay = 0;
    avsync_delay = 0;
    avsync_avg = 0;
    avsync_oldavg = 0;
    refreshrate = 0;
    lastsync = false;

    if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK))
        output_jmeter = new Jitterometer("video_output", 100);
    else
        output_jmeter = NULL;

    refreshrate = frame_interval;

    float temp_speed = (play_speed == 0.0) ? audio_stretchfactor : play_speed;
    uint fr_int = (int)(1000000.0 / video_frame_rate / temp_speed);
    uint rf_int = 0;
    if (videoOutput)
        rf_int = videoOutput->GetDisplayInfo().rate;

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
            videosync->SetFrameInterval(frame_interval, m_double_framerate);
            m_can_double = videosync->UsesFieldInterval();
            if (!m_can_double)
            {
                VERBOSE(VB_IMPORTANT, "Video sync method can't support double "
                        "framerate (refresh rate too low for bob deint)");
                FallbackDeint();
            }

            if (osd)
            {
                osd->SetFrameInterval(
                    (m_double_framerate && m_double_process) ?
                    (frame_interval>>1) : frame_interval);
            }
        }
    }
    if (!videosync)
    {
        videosync = new BusyWaitVideoSync(
            videoOutput, (int)fr_int, (int)rf_int, m_double_framerate);
    }

    InitAVSync();

    videosync->Start();

    while (!killvideo)
    {
        HandlePIPPlayerLists(4);

        if (player_ctx->buffer->isDVD())
        {
            int nbframes = 0;
            if (videoOutput)
                nbframes = videoOutput->ValidVideoFrames();

            if (nbframes < 2)
            {
                bool isWaiting  = player_ctx->buffer->DVD()->IsWaiting();

                if (isWaiting)
                {
                    player_ctx->buffer->DVD()->WaitSkip();
                    continue;
                }

                if (player_ctx->buffer->InDVDMenuOrStillFrame())
                {
                    if (nbframes == 0)
                    {
                        VERBOSE(VB_PLAYBACK+VB_EXTRA, LOC_ERR +
                                "In DVD Menu: No video frames in queue");
                        if (pausevideo)
                            UnpauseVideo();
                        usleep(10000);
                        continue;
                    }

                    if (!video_actually_paused && nbframes == 1)
                    {
                        dvd_stillframe_showing = true;
                        PauseVideo(false);
                    }
                }
            }

            if (dvd_stillframe_showing && nbframes > 1)
            {
                UnpauseVideo();
                dvd_stillframe_showing = false;
                continue;
            }
        }

        if (pausevideo || isDummy)
        {
            usleep(frame_interval);
            DisplayPauseFrame();
        }
        else
            DisplayNormalFrame();
    }

    {
        QMutexLocker locker(&vidExitLock);
        delete videosync;
        videosync = NULL;

        delete videoOutput;
        videoOutput = NULL;
    }
}

void *NuppelVideoPlayer::kickoffOutputVideoLoop(void *player)
{
    NuppelVideoPlayer *nvp = (NuppelVideoPlayer *)player;

    // OS X needs a garbage collector allocated..
    void *video_thread_pool = CreateOSXCocoaPool();
    nvp->OutputVideoLoop();
    DeleteOSXCocoaPool(video_thread_pool);

    return NULL;
}

bool NuppelVideoPlayer::FastForward(float seconds)
{
    if (!videoOutput)
        return false;

    if (player_ctx->buffer->isDVD() && GetDecoder())
        GetDecoder()->UpdateDVDFramesPlayed();

    if (fftime <= 0)
        fftime = (long long)(seconds * video_frame_rate);

    if (osdHasSubtitles || !nonDisplayedAVSubtitles.empty())
       ClearSubtitles();

    return fftime > CalcMaxFFTime(fftime, false);
}

bool NuppelVideoPlayer::Rewind(float seconds)
{
    if (!videoOutput)
        return false;

    if (player_ctx->buffer->isDVD() && GetDecoder())
       GetDecoder()->UpdateDVDFramesPlayed();

    if (rewindtime <= 0)
        rewindtime = (long long)(seconds * video_frame_rate);

    if (osdHasSubtitles || !nonDisplayedAVSubtitles.empty())
       ClearSubtitles();

    return rewindtime >= framesPlayed;
}

bool NuppelVideoPlayer::JumpToFrame(long long frame)
{
    if (!videoOutput)
        return false;

    if (player_ctx->buffer->isDVD() && GetDecoder())
        GetDecoder()->UpdateDVDFramesPlayed();

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

    if (osdHasSubtitles || !nonDisplayedAVSubtitles.empty())
       ClearSubtitles();

    return ret;
}


void NuppelVideoPlayer::JumpChapter(int chapter)
{
    if (jumpchapter == 0)
        jumpchapter = chapter;

    if (osdHasSubtitles || !nonDisplayedAVSubtitles.empty())
       ClearSubtitles();
}

void NuppelVideoPlayer::SkipCommercials(int direction)
{
    if (skipcommercials == 0)
        skipcommercials = direction;
}

void NuppelVideoPlayer::ResetPlaying(void)
{
    ClearAfterSeek();

    ffrew_skip = 1;

    if (!player_ctx->buffer->isDVD())
        framesPlayed = 0;

    GetDecoder()->Reset();
    if (GetDecoder()->IsErrored())
        SetErrored("Unable to reset video decoder");
}

void NuppelVideoPlayer::CheckTVChain(void)
{
    bool last = !(player_ctx->tvchain->HasNext());
    SetWatchingRecording(last);
}

void NuppelVideoPlayer::SwitchToProgram(void)
{
    if (!IsReallyNearEnd())
        return;

    bool discontinuity = false, newtype = false;
    int newid = -1;
    ProgramInfo *pginfo = player_ctx->tvchain->GetSwitchProgram(
        discontinuity, newtype, newid);
    if (!pginfo)
        return;

    bool newIsDummy = player_ctx->tvchain->GetCardType(newid) == "DUMMY";

    SetPlayingInfo(*pginfo);

    player_ctx->buffer->Pause();
    player_ctx->buffer->WaitForPause();

    if (newIsDummy)
    {
        OpenDummy();
        ResetPlaying();
        DoPause();
        eof = false;
        delete pginfo;
        return;
    }

    uint retries = 10; // about 5 seconds of retries
    player_ctx->buffer->OpenFile(pginfo->GetPlaybackURL(), retries);

    if (!player_ctx->buffer->IsOpen())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "SwitchToProgram's OpenFile failed.");
        eof = true;
        SetErrored(QObject::tr("Error opening switch program buffer"));
        delete pginfo;
        return;
    }

    if (eof)
    {
        discontinuity = true;
        ClearSubtitles();
    }

    VERBOSE(VB_PLAYBACK, QString("SwitchToProgram(void) "
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
        player_ctx->SetNVPChangingBuffers(true);
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
        GetDecoder()->SyncPositionMap();
    }

    eof = false;
}

void NuppelVideoPlayer::FileChangedCallback(void)
{
    VERBOSE(VB_PLAYBACK, "FileChangedCallback");

    player_ctx->buffer->Pause();
    player_ctx->buffer->WaitForPause();

    if (dynamic_cast<AvFormatDecoder *>(GetDecoder()))
        player_ctx->buffer->Reset(false, true);
    else
        player_ctx->buffer->Reset(false, true, true);

    player_ctx->buffer->Unpause();

    eof = false;

    player_ctx->SetNVPChangingBuffers(false);

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    player_ctx->tvchain->SetProgram(*player_ctx->playingInfo);
    GetDecoder()->SetProgramInfo(*player_ctx->playingInfo);
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    CheckTVChain();
    GetDecoder()->SyncPositionMap();
}

void NuppelVideoPlayer::JumpToProgram(void)
{
    VERBOSE(VB_PLAYBACK, "JumpToProgram(void)");
    bool discontinuity = false, newtype = false;
    int newid = -1;
    long long nextpos = player_ctx->tvchain->GetJumpPos();
    ProgramInfo *pginfo = player_ctx->tvchain->GetSwitchProgram(
        discontinuity, newtype, newid);
    if (!pginfo)
        return;

    bool newIsDummy = player_ctx->tvchain->GetCardType(newid) == "DUMMY";

    SetPlayingInfo(*pginfo);

    player_ctx->buffer->Pause();
    player_ctx->buffer->WaitForPause();

    ClearSubtitles();

    player_ctx->tvchain->SetProgram(*pginfo);

    player_ctx->buffer->Reset(true);

    if (newIsDummy)
    {
        OpenDummy();
        ResetPlaying();
        DoPause();
        eof = false;
        delete pginfo;
        return;
    }

    SendMythSystemPlayEvent("PLAY_CHANGED", pginfo);

    player_ctx->buffer->OpenFile(pginfo->GetPlaybackURL());
    if (!player_ctx->buffer->IsOpen())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "JumpToProgram's OpenFile failed.");
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

    if (wasDummy)
        DoPlay();

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

    player_ctx->buffer->Unpause();
    player_ctx->buffer->IgnoreLiveEOF(false);

    GetDecoder()->SetProgramInfo(*pginfo);
    delete pginfo;

    CheckTVChain();
    GetDecoder()->SyncPositionMap();

    if (nextpos < 0)
        nextpos += totalFrames;
    if (nextpos < 0)
        nextpos = 0;

    if (nextpos > 10)
    {
        bool seeks = exactseeks;
        GetDecoder()->setExactSeeks(false);
        fftime = nextpos;
        DoFastForward();
        fftime = 0;
        GetDecoder()->setExactSeeks(seeks);
    }

    eof = false;
    player_ctx->SetNVPChangingBuffers(false);
}

void NuppelVideoPlayer::RefreshPauseFrame(void)
{
    DecodeType dt = (audioOutput && normal_speed) ? kDecodeAV : kDecodeVideo;
    GetFrame(dt);

    resetvideo = true;
    while (resetvideo)
        usleep(2000);
}

bool NuppelVideoPlayer::StartPlaying(bool openfile)
{
    assert(player_ctx);

    killplayer = false;
    framesPlayed = 0;

    if (openfile && OpenFile() < 0)
    {
        VERBOSE(VB_IMPORTANT, "Unable to open video file.");
        return false;
    }

    if (player_ctx->buffer->isDVD())
        player_ctx->buffer->DVD()->SetParent(this);

    audio_lock.lock();
    if (audioOutput)
    {
        audio_paused = true;
        audioOutput->Pause(true);
        audioOutput->SetStretchFactor(audio_stretchfactor);
    }

    next_play_speed = audio_stretchfactor;

    if (!InitVideo())
    {
        VERBOSE(VB_IMPORTANT, "Unable to initialize video.");

        if (audioOutput)
        {
            delete audioOutput;
            audioOutput = NULL;
        }
        no_audio_out = true;

        return false;
    }
    audio_lock.unlock();

    if (!using_null_videoout && !player_ctx->IsPIP())
    {
        QRect visible, total;
        float aspect, scaling;

        osd = new OSD();

        osd->SetListener(m_tv);

        videoOutput->GetOSDBounds(total, visible, aspect, scaling, osd->GetThemeAspect());
        osd->Init(total, frame_interval, visible, aspect, scaling);

        videoOutput->InitOSD(osd);

        osd->SetCC708Service(&CC708services[1]);

        TeletextViewer *tt_view = GetOSD()->GetTeletextViewer();
        if (tt_view)
        {
            decoder->SetTeletextDecoderViewer(tt_view);
            tt_view->SetDisplaying(false);
        }
        GetOSD()->HideSet("teletext");

#ifdef USING_MHEG
        if (GetInteractiveTV())
        {
            total = videoOutput->GetMHEGBounds();
            GetInteractiveTV()->Reinit(total);
        }
#endif // USING_MHEG
    }

    rewindtime = fftime = 0;
    jumpchapter = 0;
    skipcommercials = 0;

    if (bookmarkseek > 30)
        fftime = bookmarkseek;

    SetPlaying(true);

    ClearAfterSeek();

    /* This thread will fill the video and audio buffers, it does all CPU
       intensive operations. We fork two other threads which do nothing but
       write to the audio and video output devices.  These should use a
       minimum of CPU. */

    pthread_t output_video, decoder_thread;

    decoder_thread = pthread_self();
    pthread_create(&output_video, NULL, kickoffOutputVideoLoop, this);


    if (!using_null_videoout && !player_ctx->buffer->isDVD())
    {
        // Request that the video output thread run with realtime priority.
        // If mythyv/mythfrontend was installed SUID root, this will work.
#if !CONFIG_DARWIN
        gContext->addPrivRequest(MythPrivRequest::MythRealtime, &output_video);
#endif

        // Use realtime prio for decoder thread as well
        //gContext->addPrivRequest(MythPrivRequest::MythRealtime, &decoder_thread);
    }

    if (bookmarkseek > 30)
    {
        DecodeType dt = (audioOutput && normal_speed) ?
            kDecodeAV : kDecodeVideo;
        GetFrame(dt);

        bool seeks = exactseeks;

        GetDecoder()->setExactSeeks(false);

        if (player_ctx->buffer->isDVD())
            GetDVDBookmark();
        DoFastForward();
        fftime = 0;

        GetDecoder()->setExactSeeks(seeks);

        if (gContext->GetNumSetting("ClearSavedPosition", 1) &&
            !player_ctx->IsPIP())
        {
            if (player_ctx->buffer->isDVD())
                SetDVDBookmark(0);
            else
            {
                player_ctx->LockPlayingInfo(__FILE__, __LINE__);
                player_ctx->playingInfo->SetBookmark(0);
                player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
            }
        }
    }

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    {
        QMutexLocker locker(&commBreakMapLock);
        if (player_ctx->playingInfo)
        {
            player_ctx->playingInfo->setIgnoreBookmark(false);
            player_ctx->playingInfo->GetCommBreakList(commBreakMap);
        }
        if (!commBreakMap.isEmpty())
        {
            hascommbreaktable = true;
            SetCommBreakIter();
        }
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    if (isDummy)
    {
        DoPause();
    }

    while (!killplayer && !IsErrored())
    {
        player_ctx->LockPlayingInfo(__FILE__, __LINE__);
        if (player_ctx->playingInfo)
            player_ctx->playingInfo->UpdateInUseMark();
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

        if (isDummy && player_ctx->tvchain && player_ctx->tvchain->HasNext())
        {
            player_ctx->tvchain->JumpToNext(true, 1);
            JumpToProgram();
        }
        else if ((!paused || eof) && player_ctx->tvchain &&
                 !GetDecoder()->GetWaitForChange())
        {
            if (player_ctx->tvchain->NeedsToSwitch())
                SwitchToProgram();
        }

        if (player_ctx->tvchain && player_ctx->tvchain->NeedsToJump())
        {
            JumpToProgram();
        }

        if (forcePositionMapSync)
        {
            forcePositionMapSync = false;
            GetDecoder()->SyncPositionMap();
        }

        if (IsErrored() || player_ctx->IsRecorderErrored())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Unknown recorder error, exiting decoder");
            if (!IsErrored())
                SetErrored(QObject::tr("Irrecoverable recorder error"));
            killplayer = true;
            break;
        }

        if (play_speed != next_play_speed &&
            (!player_ctx->tvchain ||
             (player_ctx->tvchain && !player_ctx->tvchain->NeedsToJump())))
        {
            decoder_lock.lock();

            play_speed = next_play_speed;
            normal_speed = next_normal_speed;
            VERBOSE(VB_PLAYBACK, LOC + "Changing speed to " << play_speed);

            if (play_speed == 0.0)
            {
                DoPause();
            }
            else
            {
                player_ctx->buffer->UpdatePlaySpeed(play_speed);
                DoPlay();
            }

            decoder_lock.unlock();
            continue;
        }

        if (eof)
        {
            if (player_ctx->tvchain)
            {
                if (!paused && player_ctx->tvchain->HasNext())
                {
                    VERBOSE(VB_IMPORTANT, "LiveTV forcing JumpTo 1");
                    player_ctx->tvchain->JumpToNext(true, 1);
                    continue;
                }
                if (!paused)
                    VERBOSE(VB_PLAYBACK, "Ignoring livetv eof in decoder loop");
                usleep(50000);
            }
            else
                break;
        }

        if (isDummy)
        {
            decoderThreadPaused.wakeAll();
            usleep(500);
            continue;
        }

        if (rewindtime < 0)
            rewindtime = 0;
        if (fftime < 0)
            fftime = 0;

        if (paused)
        {
            decoderThreadPaused.wakeAll();

            if (rewindtime > 0)
            {
                rewindtime = CalcRWTime(rewindtime);
                if (rewindtime > 0)
                {
                    DoRewind();
                    RefreshPauseFrame();
                }
                rewindtime = 0;
            }
            else if (fftime > 0)
            {
                fftime = CalcMaxFFTime(fftime);
                if (fftime > 0)
                {
                    DoFastForward();
                    RefreshPauseFrame();
                }
                fftime = 0;
            }
            else if (need_change_dvd_track)
            {
                DoChangeDVDTrack();
                RefreshPauseFrame();
                need_change_dvd_track = 0;
            }
            else if (jumpchapter != 0)
            {
                DoJumpChapter(jumpchapter);
                RefreshPauseFrame();
            }
            else if (player_ctx->tvchain && player_ctx->tvchain->NeedsToJump())
            {
                JumpToProgram();
                RefreshPauseFrame();
            }
            else
            {
                //printf("startplaying waiting for unpause\n");
                usleep(500);
            }
            continue;
        }

        if (rewindtime > 0 && ffrew_skip == 1)
        {
            rewindtime = CalcRWTime(rewindtime);

            if (rewindtime >= 1)
            {
                QMutexLocker locker(&internalPauseLock);

                PauseVideo(true);
                DoRewind();
                RefreshPauseFrame();
                UnpauseVideo(true);
            }
            rewindtime = 0;
        }

        if (fftime > 0 && ffrew_skip == 1)
        {
            fftime = CalcMaxFFTime(fftime);

            if (fftime >= 5)
            {
                QMutexLocker locker(&internalPauseLock);

                PauseVideo(true);

                if (fftime >= 5)
                    DoFastForward();

                if (eof)
                    continue;

                UnpauseVideo(true);
            }

            fftime = 0;
        }

        if (need_change_dvd_track)
        {
            QMutexLocker locker(&internalPauseLock);

            PauseVideo(true);
            DoChangeDVDTrack();
            UnpauseVideo(true);

            need_change_dvd_track = 0;
        }

        if (jumpchapter != 0)
        {
            QMutexLocker locker(&internalPauseLock);

            PauseVideo(true);
            DoJumpChapter(jumpchapter);
            UnpauseVideo(true);
        }

        if (skipcommercials != 0 && ffrew_skip == 1)
        {
            QMutexLocker locker(&internalPauseLock);

            PauseVideo(true);
            DoSkipCommercials(skipcommercials);
            UnpauseVideo(true);

            skipcommercials = 0;
            continue;
        }

        DecodeType dt = (audioOutput && normal_speed) ?
            kDecodeAV : kDecodeVideo;
        GetFrame(dt);

        if (using_null_videoout)
            GetDecoder()->UpdateFramesPlayed();
        else
            framesPlayed = videoOutput->GetFramesPlayed();

        if (ffrew_skip != 1)
            continue;

        if (!hasdeletetable && (kCommSkipOff != autocommercialskip))
            AutoCommercialSkip();

        if (hasdeletetable && *deleteIter == 1 &&
            framesPlayed >= deleteIter.key())
        {
            ++deleteIter;
            if (deleteIter.key() == totalFrames)
            {
                if (!(gContext->GetNumSetting("EndOfRecordingExitPrompt") == 1
                      && !player_ctx->IsPIP() &&
                      player_ctx->GetState() == kState_WatchingPreRecorded))
                {
                    eof = true;
                }
            }
            else
            {
                QMutexLocker locker(&internalPauseLock);

                PauseVideo(true);
                DoJumpToFrame(deleteIter.key());
                UnpauseVideo(true);
            }
        }
    }

    VERBOSE(VB_PLAYBACK, LOC + "Exited decoder loop.");

    decoderThreadPaused.wakeAll();

    SetPlaying(false);
    killvideo = true;
    pthread_join(output_video, NULL);

    // need to make sure video has exited first.
    audio_lock.lock();
    if (audioOutput)
        delete audioOutput;
    audioOutput = NULL;
    audio_lock.unlock();

    if (player_ctx->buffer->isDVD())
        player_ctx->buffer->DVD()->SetParent(NULL);
/*
    if (!using_null_videoout)
    {
        // Reset to default scheduling
        struct sched_param sp = {0};
        int status = pthread_setschedparam(decoder_thread, SCHED_OTHER, &sp);
        if (status)
            perror("pthread_setschedparam");
    }
*/
    return !IsErrored();
}

void NuppelVideoPlayer::SetAudioParams(int bps, int channels, int codec,
                                       int samplerate, bool passthru)
{
    audio_bits = bps;
    audio_channels = channels;
    audio_codec = codec;
    audio_samplerate = samplerate;
    audio_passthru = passthru;
}

void NuppelVideoPlayer::SetEffDsp(int dsprate)
{
    if (audioOutput)
        audioOutput->SetEffDsp(dsprate);
}

bool NuppelVideoPlayer::GetAudioBufferStatus(uint &fill, uint &total) const
{
    fill = total = 0;

    if (audioOutput)
    {
        audioOutput->GetBufferStatus(fill, total);
        return true;
    }

    return false;
}

void NuppelVideoPlayer::SetTranscoding(bool value)
{
    transcoding = value;

    if (GetDecoder())
        GetDecoder()->setTranscoding(value);
};

bool NuppelVideoPlayer::AddPIPPlayer(
    NuppelVideoPlayer *pip, PIPLocation loc, uint timeout)
{
    QMutexLocker locker(&pip_players_lock);
    pip_players_add[pip] = loc;

    pip_players_wait.wait(&pip_players_lock, timeout);

    if (pip_players.find(pip) != pip_players.end())
        return true;

    PIPMap::iterator it = pip_players_add.find(pip);
    if (it != pip_players_add.end())
        pip_players_add.erase(it);

    return false;
}

bool NuppelVideoPlayer::RemovePIPPlayer(NuppelVideoPlayer *pip, uint timeout)
{
    QMutexLocker locker(&pip_players_lock);

    pip_players_rm[pip] = kPIP_END;

    if (videoOutput)
        videoOutput->RemovePIP(pip);

    pip_players_wait.wait(&pip_players_lock, timeout);

    if (pip_players.find(pip) == pip_players.end())
        return true;

    PIPMap::iterator it = pip_players_rm.find(pip);
    if (it != pip_players_rm.end())
        pip_players_rm.erase(it);

    return false;
}

void NuppelVideoPlayer::HandlePIPPlayerLists(uint max_cnt)
{
    QMutexLocker locker(&pip_players_lock);
    PIPMap::const_iterator it = pip_players_rm.begin();
    for (; it != pip_players_rm.end(); ++it)
    {
        PIPMap::iterator it2 = pip_players.find(it.key());
        if (it2 != pip_players.end())
            pip_players.erase(it2);
    }

    for (it = pip_players_add.begin();
         (it != pip_players_add.end()) &&
         ((uint)pip_players.size() <= max_cnt); ++it)
    {
        if (pip_players.find(it.key())    == pip_players.end() &&
            pip_players_rm.find(it.key()) == pip_players_rm.end())
        {
            pip_players[it.key()] = *it;
        }
    }
    pip_players_add.clear();
    pip_players_rm.clear();
    pip_players_wait.wakeAll();
}

PIPLocation NuppelVideoPlayer::GetNextPIPLocation(void) const
{
    QMutexLocker locker(&pip_players_lock);
    PIPMap sim_pip_players = pip_players;

    PIPMap::const_iterator it = pip_players_rm.begin();
    for (; it != pip_players_rm.end(); ++it)
    {
        PIPMap::iterator it2 = sim_pip_players.find(it.key());
        if (it2 != sim_pip_players.end())
            sim_pip_players.erase(it2);
    }

    for (it = pip_players_add.begin(); it != pip_players_add.end(); ++it)
    {
        if (sim_pip_players.find(it.key()) == sim_pip_players.end() &&
            pip_players_rm.find(it.key())  == pip_players_rm.end())
        {
            sim_pip_players[it.key()] = *it;
        }
    }

    if (sim_pip_players.isEmpty())
    {
        return (PIPLocation)gContext->GetNumSetting("PIPLocation", kPIPTopLeft);
    }

    // order of preference, could be stored in db if we want it configurable
    PIPLocation ols[] =
        { kPIPTopLeft, kPIPTopRight, kPIPBottomLeft, kPIPBottomRight };

    for (uint i = 0; i < sizeof(ols)/sizeof(PIPLocation); i++)
    {
        PIPMap::const_iterator it = sim_pip_players.begin();
        for (; it != sim_pip_players.end() && (*it != ols[i]); ++it);

        if (it == sim_pip_players.end())
            return ols[i];
    }

    return kPIP_END;
}

void NuppelVideoPlayer::WrapTimecode(long long &timecode, TCTypes tc_type)
{
    if ((tc_type == TC_AUDIO) && (tc_wrap[TC_AUDIO] == LONG_LONG_MIN))
    {
        long long newaudio;
        newaudio = tc_lastval[TC_VIDEO] - tc_diff_estimate;
        tc_wrap[TC_AUDIO] = newaudio - timecode;
        timecode = newaudio;
        tc_lastval[TC_AUDIO] = timecode;
        VERBOSE(VB_IMPORTANT, "Manual Resync AV sync values");
    }

    timecode += tc_wrap[tc_type];

#define DOTCWRAP 0
#if DOTCWRAP
    // wrapped
    if (timecode < tc_lastval[tc_type] - 10000)
    {
        timecode -= tc_wrap[tc_type];
        tc_wrap[tc_type] = tc_lastval[tc_type];
        timecode += tc_wrap[tc_type];
    }

    tc_lastval[tc_type] = timecode;

    // This really doesn't work right
    if (tc_type == TC_AUDIO)
    {
        tc_avcheck_framecounter++;
        if (tc_avcheck_framecounter == 30)
        {
#define AUTO_RESYNC 1
#if AUTO_RESYNC
            // something's terribly, terribly wrong.
            if (tc_lastval[TC_AUDIO] < tc_lastval[TC_VIDEO] - 10000000 ||
                tc_lastval[TC_VIDEO] < tc_lastval[TC_AUDIO] - 10000000)
            {
                long long newaudio;
                newaudio = tc_lastval[TC_VIDEO] - tc_diff_estimate;
                timecode -= tc_wrap[TC_AUDIO];
                tc_wrap[TC_AUDIO] = newaudio - timecode;
                timecode = newaudio;
                tc_lastval[TC_AUDIO] = timecode;
                VERBOSE(VB_IMPORTANT, "Guessing at new AV sync values");
            }
#endif

            tc_diff_estimate = tc_lastval[TC_VIDEO] - tc_lastval[TC_AUDIO];
            tc_avcheck_framecounter = 0;
        }
    }
#endif
}

/** \fn NuppelVideoPlayer::AddAudioData(char*,int,long long)
 *  \brief Sends audio to AudioOuput::AddSamples(char*,int,long long)
 *
 *   This uses point sampling filter to resample audio if we are
 *   using video as the timebase rather than the audio as the
 *   timebase. This causes ringing artifacts, but hopefully not
 *   too much of it.
 */
void NuppelVideoPlayer::AddAudioData(char *buffer, int len, long long timecode)
{
    if (!player_ctx->buffer->InDVDMenuOrStillFrame())
        WrapTimecode(timecode, TC_AUDIO);

    int samplesize = (audio_channels * audio_bits) / 8; // bytes per sample
    if ((samplesize <= 0) || !audioOutput)
        return;

    int samples = len / samplesize;

    if (player_ctx->buffer->isDVD() &&
        player_ctx->buffer->DVD()->InStillFrame())
    {
        audioOutput->Drain();
    }

    if (!audioOutput->AddSamples(buffer, samples, timecode))
        VERBOSE(VB_PLAYBACK, "NVP::AddAudioData():p1: "
                "Audio buffer overflow, audio data lost!");
    return;
}

/** \fn NuppelVideoPlayer::AddAudioData(short int*,short int*,int,long long)
 *  \brief Sends audio to AudioOuput::AddSamples(char *buffers[],int,long long)
 *
 *   This uses point sampling filter to resample audio if we are
 *   using video as the timebase rather than the audio as the
 *   timebase. This causes ringing artifacts, but hopefully not
 *   too much of it.
 */
void NuppelVideoPlayer::AddAudioData(short int *lbuffer, short int *rbuffer,
                                     int samples, long long timecode)
{
    char *buffers[2];

    WrapTimecode(timecode, TC_AUDIO);

    if (!audioOutput)
        return;

    buffers[0] = (char*) lbuffer;
    buffers[1] = (char*) rbuffer;
    if (!audioOutput->AddSamples(buffers, samples, timecode))
        VERBOSE(VB_PLAYBACK, "NVP::AddAudioData():p2: "
                "Audio buffer overflow, audio data lost!");
    return;
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
void NuppelVideoPlayer::SetWatched(bool forceWatched)
{
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (!player_ctx->playingInfo)
    {
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return;
    }

    long long numFrames = totalFrames;

    if (player_ctx->playingInfo->GetTranscodedStatus() != TRANSCODING_COMPLETE)
    {
        uint endtime;

        // If the recording is stopped early we need to use the recording end
        // time, not the programme end time
        if (player_ctx->playingInfo->recendts.toTime_t() <
            player_ctx->playingInfo->endts.toTime_t())
        {
            endtime = player_ctx->playingInfo->recendts.toTime_t();
        }
        else
        {
            endtime = player_ctx->playingInfo->endts.toTime_t();
        }

        numFrames = (long long)
            ((endtime - player_ctx->playingInfo->recstartts.toTime_t()) *
             video_frame_rate);
    }

    int offset = (int) round(0.14 * (numFrames / video_frame_rate));

    if (offset < 240)
        offset = 240; // 4 Minutes Min
    else if (offset > 720)
        offset = 720; // 12 Minutes Max

    if (forceWatched || framesPlayed > numFrames - (offset * video_frame_rate))
    {
        player_ctx->playingInfo->SetWatchedFlag(true);
        VERBOSE(VB_GENERAL, QString("Marking recording as watched using offset %1 minutes").arg(offset/60));
    }
    else
    {
        player_ctx->playingInfo->SetWatchedFlag(false);
        VERBOSE(VB_GENERAL, "Marking recording as unwatched");
    }

    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
}

void NuppelVideoPlayer::SetBookmark(void)
{
    bool saved = false;

    if (player_ctx->buffer->isDVD())
    {
        if (player_ctx->buffer->InDVDMenuOrStillFrame())
            SetDVDBookmark(0);
        else
        {
            SetDVDBookmark(framesPlayed);
            saved = true;
        }
    }
    else
    {
        player_ctx->LockPlayingInfo(__FILE__, __LINE__);
        if (player_ctx->playingInfo)
        {
            player_ctx->playingInfo->SetBookmark(framesPlayed);
            saved = true;
        }
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    }

    if (saved && osd)
    {
        osd->SetSettingsText(QObject::tr("Bookmark Saved"), 1);

        struct StatusPosInfo posInfo;
        calcSliderPos(posInfo);
        osd->ShowStatus(posInfo, false, QObject::tr("Position"), 2);
    }
}

void NuppelVideoPlayer::ClearBookmark(void)
{
    bool cleared = false;

    if (player_ctx->buffer->isDVD())
    {
        SetDVDBookmark(0);
        cleared = true;
    }
    else
    {
        player_ctx->LockPlayingInfo(__FILE__, __LINE__);
        if (player_ctx->playingInfo)
        {
            player_ctx->playingInfo->SetBookmark(0);
            cleared = true;
        }
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    }

    if (osd && cleared)
        osd->SetSettingsText(QObject::tr("Bookmark Cleared"), 1);
}

long long NuppelVideoPlayer::GetBookmark(void) const
{
    long long bookmark = 0;

    if (gContext->IsDatabaseIgnored())
        bookmark = 0;
    else if (player_ctx->buffer->isDVD())
        bookmark = GetDVDBookmark();
    else
    {
        player_ctx->LockPlayingInfo(__FILE__, __LINE__);
        if (player_ctx->playingInfo)
            bookmark = player_ctx->playingInfo->GetBookmark();
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    }

    return bookmark;
}

void NuppelVideoPlayer::DoPause(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "DoPause() -- begin");
    bool skip_changed;

    skip_changed = (ffrew_skip != 1);
    ffrew_skip = 1;

    if (skip_changed)
    {
        //cout << "handling skip change" << endl;
        videoOutput->SetPrebuffering(ffrew_skip == 1);

        GetDecoder()->setExactSeeks(exactseeks && ffrew_skip == 1);
        GetDecoder()->DoFastForward(framesPlayed + ffrew_skip);
        SaveAudioTimecodeOffset(GetAudioTimecodeOffset());
        ClearAfterSeek();
    }

    float temp_speed = audio_stretchfactor;
    frame_interval = (int)(1000000.0 * ffrew_skip / video_frame_rate / temp_speed);
    VERBOSE(VB_PLAYBACK, QString("rate: %1 speed: %2 skip: %3 = interval %4")
                                 .arg(video_frame_rate).arg(temp_speed)
                                 .arg(ffrew_skip).arg(frame_interval));
    if (osd)
    {
        osd->SetFrameInterval(
            (m_double_framerate && m_double_process) ?
            (frame_interval>>1) : frame_interval);
    }

    if (videosync != NULL)
        videosync->SetFrameInterval(frame_interval, m_double_framerate);

    VERBOSE(VB_PLAYBACK, LOC + "DoPause() -- setting paused");
    paused = actuallypaused = true;
    decoderThreadPaused.wakeAll();
}

void NuppelVideoPlayer::DoPlay(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "DoPlay() -- begin");

    bool skip_changed;

    if (player_ctx->buffer->isDVD())
    {
        if (GetDecoder())
            GetDecoder()->UpdateDVDFramesPlayed();
        if (play_speed != normal_speed)
            player_ctx->buffer->DVD()->SetDVDSpeed(-1);
        else
            player_ctx->buffer->DVD()->SetDVDSpeed();
    }

    if (play_speed > 0.0f && play_speed <= 3.0f)
    {
        skip_changed = (ffrew_skip != 1);
        ffrew_skip = 1;
    }
    else
    {
        skip_changed = true;
        ffrew_skip = ((uint)ceil(4.0 * fabs(play_speed) / 30)) * 30;
        if (play_speed < 0.0)
            ffrew_skip = -ffrew_skip;
    }

    if (player_ctx->buffer->isDVD() && GetDecoder())
        GetDecoder()->UpdateDVDFramesPlayed();

    if (skip_changed && videoOutput)
    {
        //cout << "handling skip change" << endl;
        videoOutput->SetPrebuffering(ffrew_skip == 1);

        GetDecoder()->setExactSeeks(exactseeks && ffrew_skip == 1);
        GetDecoder()->DoRewind(framesPlayed);
        SaveAudioTimecodeOffset(GetAudioTimecodeOffset());
        ClearAfterSeek();
    }

    frame_interval = (int) (1000000.0f * ffrew_skip / video_frame_rate /
                            play_speed);

    VERBOSE(VB_PLAYBACK, LOC + "DoPlay: " +
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
        videosync->SetFrameInterval(frame_interval, m_double_framerate);
    }

    if (osd)
    {
        osd->SetFrameInterval(
            (m_double_framerate && m_double_process) ?
            (frame_interval>>1) : frame_interval);
    }

    if (normal_speed && audioOutput)
    {
        audio_stretchfactor = play_speed;
        if (decoder)
        {
            bool disable = (play_speed < 0.99f) || (play_speed > 1.01f);
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("Stretch Factor %1, %2 passthru ")
                    .arg(audio_stretchfactor)
                    .arg((disable) ? "disable" : "allow"));
            decoder->SetDisablePassThrough(disable);
        }
        audio_lock.lock();
        if (audioOutput)
        {
            audioOutput->SetStretchFactor(play_speed);
        }
        audio_lock.unlock();
    }

    VERBOSE(VB_PLAYBACK, LOC + "DoPlay() -- setting unpaused");
    paused = actuallypaused = false;
}

bool NuppelVideoPlayer::DoRewind(void)
{
    // Save Audio Sync setting before seeking
    SaveAudioTimecodeOffset(GetAudioTimecodeOffset());

    long long number = rewindtime + 1;
    long long desiredFrame = framesPlayed - number;

    if (!editmode && hasdeletetable && IsInDelete(desiredFrame))
    {
        QMap<long long, int>::Iterator it = deleteMap.begin();
        while (it != deleteMap.end())
        {
            if (desiredFrame > it.key())
                ++it;
            else
                break;
        }

        if (it != deleteMap.begin() && it != deleteMap.end())
        {
            long long over = it.key() - desiredFrame;
            --it;
            desiredFrame = it.key() - over;
        }
    }

    if (desiredFrame < 0)
        desiredFrame = 0;

    limitKeyRepeat = false;
    if (desiredFrame < video_frame_rate)
        limitKeyRepeat = true;

    if (paused && !editmode)
        GetDecoder()->setExactSeeks(true);
    GetDecoder()->DoRewind(desiredFrame);
    GetDecoder()->setExactSeeks(exactseeks);

    ClearAfterSeek();
    lastSkipTime = time(NULL);
    return true;
}

long long NuppelVideoPlayer::CalcRWTime(long long rw) const
{
    bool hasliveprev = (livetv && player_ctx->tvchain &&
                        player_ctx->tvchain->HasPrev());

    if (!hasliveprev)
        return rw;

    if ((framesPlayed - rw + 1) < 0)
    {
        player_ctx->tvchain->JumpToNext(false, (int)(-15.0 * video_frame_rate));
        return -1;
    }

    return rw;
}

long long NuppelVideoPlayer::CalcMaxFFTime(long long ff, bool setjump) const
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

            if (player_ctx->buffer->isDVD() &&
                player_ctx->buffer->DVD()->TitleTimeLeft() < 5)
            {
                ret = 0;
            }
        }
    }

    return ret;
}

/** \fn NuppelVideoPlayer::IsReallyNearEnd(void) const
 *  \brief Returns true iff really near end of recording.
 *
 *   This is used by SwitchToProgram() to determine if we are so
 *   close to the end that we need to switch to the next program.
 */
bool NuppelVideoPlayer::IsReallyNearEnd(void) const
{
    if (!videoOutput)
        return false;

    int    sz              = player_ctx->buffer->DataInReadAhead();
    uint   rbs             = player_ctx->buffer->GetReadBlockSize();
    uint   kbits_per_sec   = player_ctx->buffer->GetBitrate();
    uint   vvf             = videoOutput->ValidVideoFrames();
    double inv_fps         = 1.0 / GetDecoder()->GetFPS();
    double bytes_per_frame = kbits_per_sec * (1000.0/8.0) * inv_fps;
    double rh_frames       = sz / bytes_per_frame;

    // WARNING: rh_frames can greatly overestimate or underestimate
    //          the number of frames available in the read ahead buffer
    //          when rh_frames is less than the keyframe distance.

    bool near_end = ((vvf + rh_frames) < 10.0) || (sz < rbs*1.5);

    VERBOSE(VB_PLAYBACK, LOC + "IsReallyNearEnd()"
            <<" br("<<(kbits_per_sec/8)<<"KB)"
            <<" fps("<<((uint)(1.0/inv_fps))<<")"
            <<" sz("<<(sz / 1000)<<"KB)"
            <<" vfl("<<vvf<<")"
            <<" frh("<<((uint)rh_frames)<<")"
            <<" ne:"<<near_end);

    return near_end;
}

/** \fn NuppelVideoPlayer::IsNearEnd(long long) const
 *  \brief Returns true iff near end of recording.
 *  \param margin minimum number of frames we want before being near end,
 *                defaults to 2 seconds of video.
 */
bool NuppelVideoPlayer::IsNearEnd(long long margin) const
{
    long long framesRead, framesLeft = 0;

    if (!player_ctx)
        return false;

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (!player_ctx->playingInfo || player_ctx->playingInfo->isVideo ||
        !GetDecoder())
    {
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return false;
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    margin = (margin >= 0) ? margin: (long long) (video_frame_rate*2);
    margin = (long long) (margin * audio_stretchfactor);
    bool watchingTV = watchingrecording && player_ctx->recorder &&
        player_ctx->recorder->IsValidRecorder();

    framesRead = GetDecoder()->GetFramesRead();

    if (!player_ctx->IsPIP() &&
        player_ctx->GetState() == kState_WatchingPreRecorded)
    {
        framesLeft = margin;
        if (!editmode && hasdeletetable && IsInDelete(framesRead))
        {
            QMap<long long, int>::const_iterator it = deleteMap.end();
            --it;
            if (it.key() == totalFrames)
            {
                --it;
                if (framesRead >= it.key())
                    return true;
            }
        }
        else
            framesLeft = totalFrames - framesRead;
        return (framesLeft < margin);
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
        if (framesLeft < margin)
            framesLeft = player_ctx->recorder->GetFramesWritten() - framesRead;
    }

    return (framesLeft < margin);
}

bool NuppelVideoPlayer::DoFastForward(void)
{
    // Save Audio Sync setting before seeking
    SaveAudioTimecodeOffset(GetAudioTimecodeOffset());

    long long number = fftime - 1;
    long long desiredFrame = framesPlayed + number;

    if (!editmode && hasdeletetable && IsInDelete(desiredFrame))
    {
        QMap<long long, int>::Iterator it = deleteMap.end();
        --it;
        if ( it.key() == totalFrames)
        {
            --it;
            if (desiredFrame > it.key())
                desiredFrame = it.key();
        }
    }
    if (paused && !editmode)
        GetDecoder()->setExactSeeks(true);
    GetDecoder()->DoFastForward(desiredFrame);
    GetDecoder()->setExactSeeks(exactseeks);

    // Note: The video output will be reset by what the the decoder
    //       does, so we only need to reset the audio, subtitles, etc.
    ClearAfterSeek(false);

    lastSkipTime = time(NULL);
    return true;
}

void NuppelVideoPlayer::DoJumpToFrame(long long frame)
{
    bool exactstore = exactseeks;

    GetDecoder()->setExactSeeks(true);
    fftime = rewindtime = 0;

    if (frame > framesPlayed)
    {
        fftime = frame - framesPlayed;
        DoFastForward();
        fftime = 0;
    }
    else if (frame < framesPlayed)
    {
        rewindtime = framesPlayed - frame;
        DoRewind();
        rewindtime = 0;
    }

    GetDecoder()->setExactSeeks(exactstore);
}

#if 0
bool NuppelVideoPlayer::IsSkipTooCloseToEnd(int frames) const
{
    if ((livetv) ||
        (watchingrecording && player_ctx->recorder &&
         player_ctx->recorder->IsValidRecorder()))
    {
        if (player_ctx->recorder->GetFramesWritten() < (framesPlayed + frames))
            return 1;
    }
    else if ((totalFrames) && (totalFrames < (framesPlayed + frames)))
    {
        return 1;
    }
    return 0;
}
#endif

/** \fn NuppelVideoPlayer::ClearAfterSeek(bool)
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
void NuppelVideoPlayer::ClearAfterSeek(bool clearvideobuffers)
{
    VERBOSE(VB_PLAYBACK, LOC + "ClearAfterSeek("<<clearvideobuffers<<")");

    if (clearvideobuffers)
        videoOutput->ClearAfterSeek();

    for (int i = 0; i < MAXTBUFFER; i++)
        txtbuffers[i].timecode = 0;
    ResetCC();

    wtxt = 0;
    rtxt = 0;

    for (int j = 0; j < TCTYPESMAX; j++)
    {
        tc_wrap[j] = tc_lastval[j] = 0;
    }
    tc_avcheck_framecounter = 0;

    if (savedAudioTimecodeOffset)
    {
        tc_wrap[TC_AUDIO] = savedAudioTimecodeOffset;
        savedAudioTimecodeOffset = 0;
    }

    SetPrebuffering(true);
    audio_lock.lock();
    if (audioOutput)
        audioOutput->Reset();
    audio_lock.unlock();

    if (osd)
    {
        osd->ClearAllCCText();
        if (player_ctx->buffer->InDVDMenuOrStillFrame())
        {
            osd->HideSet("subtitles");
            osd->ClearAll("subtitles");
        }
    }

    SetDeleteIter();
    {
        QMutexLocker locker(&commBreakMapLock);
        SetCommBreakIter();
    }
}

void NuppelVideoPlayer::SetPlayerInfo(
        TV *tv, QWidget *widget, bool frame_exact_seek, PlayerContext *ctx)
{
    m_tv = tv;
    parentWidget = widget;
    exactseeks   = frame_exact_seek;
    player_ctx   = ctx;
    livetv       = ctx->tvchain;
}

void NuppelVideoPlayer::SetDeleteIter(void)
{
    deleteIter = deleteMap.begin();
    if (hasdeletetable)
    {
        while (deleteIter != deleteMap.end())
        {
            if ((framesPlayed + 2) > deleteIter.key())
            {
                ++deleteIter;
            }
            else
                break;
        }

        if (deleteIter != deleteMap.begin())
            --deleteIter;

        if (*deleteIter == 0)
            ++deleteIter;
    }
}

/** \brief Sets the Commercial break iteratator to the next break entry after
 *         the frame currently being shown.
 *
 *   NOTE: The commBreakMapLock must be held when calling this method.
 */
void NuppelVideoPlayer::SetCommBreakIter(void)
{
    if (!hascommbreaktable)
        return;

    commBreakIter = commBreakMap.begin();
    while (commBreakIter != commBreakMap.end())
    {
        if (framesPlayed <= commBreakIter.key())
            break;

        commBreakIter++;
    }

    if (commBreakIter != commBreakMap.end())
    {
        VERBOSE(VB_COMMFLAG, LOC + QString("new commBreakIter = %1 @ frame %2, "
                "framesPlayed = %3")
                .arg(*commBreakIter).arg(commBreakIter.key())
                .arg(framesPlayed));
    }
}

void NuppelVideoPlayer::SetAutoCommercialSkip(CommSkipMode autoskip)
{
    QMutexLocker locker(&commBreakMapLock);
    SetCommBreakIter();
    uint next = (kCommSkipIncr == autoskip) ?
        (uint) autocommercialskip + 1 : (uint) autoskip;
    autocommercialskip = (CommSkipMode) (next % kCommSkipCount);
}

CommSkipMode NuppelVideoPlayer::GetAutoCommercialSkip(void) const
{
    QMutexLocker locker(&commBreakMapLock);
    return autocommercialskip;
}

bool NuppelVideoPlayer::EnableEdit(void)
{
    editmode = false;

    if (!hasFullPositionMap)
    {
        VERBOSE(VB_IMPORTANT, "Cannot edit - no full position map");

        if (osd)
        {
            struct StatusPosInfo posInfo;
            calcSliderPos(posInfo);
            osd->ShowStatus(posInfo, false, QObject::tr("No Seektable"), 2);
        }

        return false;
    }

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (!hasFullPositionMap || !player_ctx->playingInfo || !osd)
    {
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        return false;
    }

    bool alreadyediting = false;
    alreadyediting = player_ctx->playingInfo->IsEditing();
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    if (alreadyediting)
        return false;

    // lock internal pause lock so that is_pause is definately still
    // valid when we enter pause wait loop.
    internalPauseLock.lock();
    bool is_paused = IsPaused();
    if (is_paused)
        osd->EndStatus(); // hide pause OSD

    editmode = true;
    bool pause_possible = false;
    while (!is_paused)
    {
        if (!pause_possible)
        {
            internalPauseLock.unlock();
            Pause(true);
            internalPauseLock.lock();
        }
        is_paused = IsPaused(&pause_possible);
        usleep(5000);
    }
    // safe to unlock now, play won't start with editmode enabled
    internalPauseLock.unlock();

    seekamount = keyframedist;
    seekamountpos = 3;

    dialogname = "";

    InfoMap infoMap;
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    player_ctx->playingInfo->ToMap(infoMap);
    infoMap.detach();
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    osd->SetText("editmode", infoMap, -1);

    UpdateEditSlider();
    UpdateTimeDisplay();
    UpdateSeekAmount(true);

    if (hasdeletetable)
    {
        if (deleteMap.contains(0))
            deleteMap.erase(deleteMap.find(0));
        if (deleteMap.contains(totalFrames))
            deleteMap.erase(deleteMap.find(totalFrames));

        QMap<long long, int>::Iterator it;
        for (it = deleteMap.begin(); it != deleteMap.end(); ++it)
             AddMark(it.key(), *it);
    }

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    player_ctx->playingInfo->SetEditing(true);
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    return editmode;
}

void NuppelVideoPlayer::DisableEdit(void)
{
    editmode = false;

    if (!player_ctx->playingInfo)
        return;

    QMap<long long, int>::Iterator i = deleteMap.begin();
    for (; i != deleteMap.end(); ++i)
        osd->HideEditArrow(i.key(), *i);
    osd->HideSet("editmode");

    timedisplay = NULL;

    SaveCutList();

    LoadCutList();
    if (!deleteMap.isEmpty())
    {
        hasdeletetable = true;
        SetDeleteIter();
    }
    else
    {
        hasdeletetable = false;
    }

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
        player_ctx->playingInfo->SetEditing(false);
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
}

bool NuppelVideoPlayer::DoKeypress(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    handled = gContext->GetMainWindow()->TranslateKeyPress("TV Editing", e, actions);

    if (handled)
        return true;

    if (dialogname != "")
    {
        for (int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "UP")
                osd->DialogUp(dialogname);
            else if (action == "DOWN")
                osd->DialogDown(dialogname);
            else if (action == "SELECT" || action == "ESCAPE")
            {
                if (action == "ESCAPE")
                    osd->DialogAbort(dialogname);
                osd->TurnDialogOff(dialogname);
                HandleResponse();
            }
            else
                handled = false;
        }

        return true;
    }

    bool retval = true;
    bool exactstore = exactseeks;
    GetDecoder()->setExactSeeks(true);

    for (int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;

        if (action == "SELECT")
            HandleSelect();
        else if (action == "LEFT")
        {
            if (seekamount > 0)
            {
                rewindtime = seekamount;
                while (rewindtime != 0)
                    usleep(1000);
                UpdateEditSlider();
            }
            else
                HandleArbSeek(false);
            UpdateTimeDisplay();
        }
        else if (action == "RIGHT")
        {
            if (seekamount > 0)
            {
                fftime = seekamount;
                while (fftime != 0)
                    usleep(1000);
                UpdateEditSlider();
            }
            else
                HandleArbSeek(true);
            UpdateTimeDisplay();
        }
        else if (action == "UP")
        {
            UpdateSeekAmount(true);
            UpdateTimeDisplay();
        }
        else if (action == "DOWN")
        {
            UpdateSeekAmount(false);
            UpdateTimeDisplay();
        }
        else if (action == "CLEARMAP")
        {
            QMap<long long, int>::Iterator it;
            for (it = deleteMap.begin(); it != deleteMap.end(); ++it)
                osd->HideEditArrow(it.key(), *it);

            deleteMap.clear();
            UpdateEditSlider();
        }
        else if (action == "INVERTMAP")
        {
            QMap<long long, int>::Iterator it;
            for (it = deleteMap.begin(); it != deleteMap.end(); ++it)
                ReverseMark(it.key());

            UpdateEditSlider();
            UpdateTimeDisplay();
        }
        else if (action == "LOADCOMMSKIP")
        {
            if (hascommbreaktable)
            {
                commBreakMapLock.lock();
                QMap<long long, int>::Iterator it;
                for (it = commBreakMap.begin(); it != commBreakMap.end(); ++it)
                {
                    if (!deleteMap.contains(it.key()))
                    {
                        if (*it == MARK_COMM_START)
                            AddMark(it.key(), MARK_CUT_START);
                        else
                            AddMark(it.key(), MARK_CUT_END);
                    }
                }
                commBreakMapLock.unlock();
                UpdateEditSlider();
                UpdateTimeDisplay();
            }
        }
        else if (action == "PREVCUT")
        {
            int old_seekamount = seekamount;
            seekamount = -2;
            HandleArbSeek(false);
            seekamount = old_seekamount;
            UpdateTimeDisplay();
        }
        else if (action == "NEXTCUT")
        {
            int old_seekamount = seekamount;
            seekamount = -2;
            HandleArbSeek(true);
            seekamount = old_seekamount;
            UpdateTimeDisplay();
        }
#define FFREW_MULTICOUNT 10
        else if (action == "BIGJUMPREW")
        {
             if (seekamount > 0)
                 rewindtime = seekamount * FFREW_MULTICOUNT;
            else
            {
                int fps = (int)ceil(video_frame_rate);
                rewindtime = fps * FFREW_MULTICOUNT / 2;
            }
            while (rewindtime != 0)
                usleep(1000);
            UpdateEditSlider();
            UpdateTimeDisplay();
        }
        else if (action == "BIGJUMPFWD")
        {
             if (seekamount > 0)
                 fftime = seekamount * FFREW_MULTICOUNT;
            else
            {
                int fps = (int)ceil(video_frame_rate);
                fftime = fps * FFREW_MULTICOUNT / 2;
            }
            while (fftime != 0)
                usleep(1000);
            UpdateEditSlider();
            UpdateTimeDisplay();
        }
        else if (action == "ESCAPE" || action == "MENU" ||
                 action == "EDIT")
        {
            DisableEdit();
            retval = false;
        }
        else
            handled = false;
    }

    GetDecoder()->setExactSeeks(exactstore);
    return retval;
}

AspectOverrideMode NuppelVideoPlayer::GetAspectOverride(void) const
{
    if (videoOutput)
        return videoOutput->GetAspectOverride();
    return kAspect_Off;
}

AdjustFillMode NuppelVideoPlayer::GetAdjustFill(void) const
{
    if (videoOutput)
        return videoOutput->GetAdjustFill();
    return kAdjustFill_Off;
}

void NuppelVideoPlayer::SetForcedAspectRatio(int mpeg2_aspect_value, int letterbox_permission)
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

void NuppelVideoPlayer::ToggleAspectOverride(AspectOverrideMode aspectMode)
{
    if (videoOutput)
    {
        videoOutput->ToggleAspectOverride(aspectMode);
        ReinitOSD();
    }
}

void NuppelVideoPlayer::ToggleAdjustFill(AdjustFillMode adjustfillMode)
{
    if (videoOutput)
    {
        detect_letter_box->SetDetectLetterbox(false);
        videoOutput->ToggleAdjustFill(adjustfillMode);
        ReinitOSD();
    }
}

bool NuppelVideoPlayer::ToggleUpmix()
{
    if (audioOutput)
        return audioOutput->ToggleUpmix();
    return false;
}

void NuppelVideoPlayer::Zoom(ZoomDirection direction)
{
    if (videoOutput)
    {
        videoOutput->Zoom(direction);
        ReinitOSD();
    }
}

void NuppelVideoPlayer::ExposeEvent(void)
{
    if (videoOutput)
        videoOutput->ExposeEvent();
}

bool NuppelVideoPlayer::IsEmbedding(void)
{
    if (videoOutput)
        return videoOutput->IsEmbedding();
    return false;
}

void NuppelVideoPlayer::UpdateSeekAmount(bool up)
{
    if (seekamountpos > 0 && !up)
        seekamountpos--;
    if (seekamountpos < 9 && up)
        seekamountpos++;

    QString text = "";

    switch (seekamountpos)
    {
        case 0: text = QObject::tr("cut point"); seekamount = -2; break;
        case 1: text = QObject::tr("keyframe"); seekamount = -1; break;
        case 2: text = QObject::tr("1 frame"); seekamount = 1; break;
        case 3: text = QObject::tr("0.5 seconds"); seekamount = (int)roundf(video_frame_rate / 2); break;
        case 4: text = QObject::tr("%n second(s)", "", 1); seekamount = (int)roundf(video_frame_rate); break;
        case 5: text = QObject::tr("%n second(s)", "", 5); seekamount = (int)roundf(video_frame_rate * 5); break;
        case 6: text = QObject::tr("%n second(s)", "", 20); seekamount = (int)roundf(video_frame_rate * 20); break;
        case 7: text = QObject::tr("%n minute(s)", "", 1); seekamount = (int)roundf(video_frame_rate * 60); break;
        case 8: text = QObject::tr("%n minute(s)", "", 5); seekamount = (int)roundf(video_frame_rate * 300); break;
        case 9: text = QObject::tr("%n minute(s)", "", 10); seekamount = (int)roundf(video_frame_rate * 600); break;
        default: text = QObject::tr("error"); seekamount = (int)roundf(video_frame_rate); break;
    }

    InfoMap infoMap;
    infoMap["seekamount"] = text;
    osd->SetText("editmode", infoMap, -1);
}

void NuppelVideoPlayer::UpdateTimeDisplay(void)
{
    int secs, frames, ss, mm, hh;

    secs = (int)(framesPlayed / video_frame_rate);
    frames = framesPlayed - (int)(secs * video_frame_rate);

    ss = secs;
    mm = ss / 60;
    ss %= 60;
    hh = mm / 60;
    mm %= 60;

    char timestr[128];
    sprintf(timestr, "%d:%02d:%02d.%02d", hh, mm, ss, frames);

    char framestr[128];
    sprintf(framestr, "%lld", framesPlayed);

    QString cutmarker = " ";
    if (IsInDelete(framesPlayed))
        cutmarker = QObject::tr("cut");

    InfoMap infoMap;
    infoMap["timedisplay"] = timestr;
    infoMap["framedisplay"] = framestr;
    infoMap["cutindicator"] = cutmarker;
    osd->SetText("editmode", infoMap, -1);
}

void NuppelVideoPlayer::HandleSelect(bool allowSelectNear)
{
    bool deletepoint = false;
    bool cut_after = false;
    int direction = 0;

    if(!deleteMap.isEmpty())
    {
        QMap<long long, int>::ConstIterator iter = deleteMap.begin();

        while((iter != deleteMap.end()) && (iter.key() < framesPlayed))
            ++iter;

        if (iter == deleteMap.end())
        {
            --iter;
            cut_after = !(*iter);
        }
        else if((iter != deleteMap.begin()) && (iter.key() != framesPlayed))
        {
            long long value = iter.key();
            if((framesPlayed - (--iter).key()) > (value - framesPlayed))
            {
                cut_after = !(*iter);
                ++iter;
            }
            else
                cut_after = !(*iter);
        }

        direction = (*iter);
        deleteframe = iter.key();

        if ((absLongLong(deleteframe - framesPlayed) <
                   (int)ceil(20 * video_frame_rate)) && !allowSelectNear)
        {
            deletepoint = true;
        }
    }

    if (deletepoint)
    {
        QString message = QObject::tr("You are close to an existing cut point. "
                                      "Would you like to:");
        QString option1 = QObject::tr("Delete this cut point");
        QString option2 = QObject::tr("Move this cut point to the current "
                                      "position");
        QString option3 = QObject::tr("Flip directions - delete to the ");
        if (direction == 0)
            option3 += QObject::tr("right");
        else
            option3 += QObject::tr("left");
        QString option4 = QObject::tr("Insert a new cut point");

        dialogname = "deletemark";
        dialogtype = 0;

        QStringList options;
        options += option1;
        options += option2;
        options += option3;
        options += option4;

        osd->NewDialogBox(dialogname, message, options, -1);
    }
    else
    {
        QString message = QObject::tr("Insert a new cut point?");
        QString option1 = QObject::tr("Delete before this frame");
        QString option2 = QObject::tr("Delete after this frame");

        dialogname = "addmark";
        dialogtype = 1;

        QStringList options;
        options += option1;
        options += option2;

        osd->NewDialogBox(dialogname, message, options, -1, cut_after);
    }
}

void NuppelVideoPlayer::HandleResponse(void)
{
    int result = osd->GetDialogResponse(dialogname);
    dialogname = "";

    if (dialogtype == 0)
    {
        int type = deleteMap[deleteframe];
        switch (result)
        {
            case 1:
                DeleteMark(deleteframe);
                break;
            case 2:
                DeleteMark(deleteframe);
                AddMark(framesPlayed, type);
                break;
            case 3:
                ReverseMark(deleteframe);
                break;
            case 4:
                HandleSelect(true);
                break;
            default:
                break;
        }
    }
    else if (dialogtype == 1)
    {
        switch (result)
        {
            case 1:
                AddMark(framesPlayed, MARK_CUT_END);
                break;
            case 2:
                AddMark(framesPlayed, MARK_CUT_START);
                break;
            case 3: case 0: default:
                break;
        }
    }
    UpdateEditSlider();
    UpdateTimeDisplay();
}

void NuppelVideoPlayer::UpdateEditSlider(void)
{
    osd->DoEditSlider(deleteMap, framesPlayed, totalFrames);
}

void NuppelVideoPlayer::AddMark(long long frames, int type)
{
    deleteMap[frames] = type;
    osd->ShowEditArrow(frames, totalFrames, type);
}

void NuppelVideoPlayer::DeleteMark(long long frames)
{
    osd->HideEditArrow(frames, deleteMap[frames]);
    deleteMap.remove(frames);
}

void NuppelVideoPlayer::ReverseMark(long long frames)
{
    osd->HideEditArrow(frames, deleteMap[frames]);

    if (deleteMap[frames] == MARK_CUT_END)
        deleteMap[frames] = MARK_CUT_START;
    else
        deleteMap[frames] = MARK_CUT_END;

    osd->ShowEditArrow(frames, totalFrames, deleteMap[frames]);
}

void NuppelVideoPlayer::HandleArbSeek(bool right)
{
    if (seekamount == -2)
    {
        QMap<long long, int>::Iterator i = deleteMap.begin();
        long long framenum = -1;
        if (right)
        {
            for (; i != deleteMap.end(); ++i)
            {
                if (i.key() > framesPlayed)
                {
                    framenum = i.key();
                    break;
                }
            }
            if (framenum == -1)
                framenum = totalFrames;

            fftime = framenum - framesPlayed;
            while (fftime > 0)
                usleep(1000);
        }
        else
        {
            for (; i != deleteMap.end(); ++i)
            {
                if (i.key() >= framesPlayed)
                    break;
                framenum = i.key();
            }
            if (framenum == -1)
                framenum = 0;

            rewindtime = framesPlayed - framenum;
            while (rewindtime > 0)
                usleep(1000);
        }
    }
    else
    {
        if (right)
        {
            // editKeyFrameDist is a workaround for when keyframe distance
            // is set to one, and keyframe detection is disabled because
            // the position map uses MARK_GOP_BYFRAME. (see DecoderBase)
            float editKeyFrameDist = keyframedist <= 2 ? 18 : keyframedist;

            GetDecoder()->setExactSeeks(false);
            fftime = (long long)(editKeyFrameDist * 1.1);
            while (fftime > 0)
                usleep(1000);
        }
        else
        {
            GetDecoder()->setExactSeeks(false);
            rewindtime = 2;
            while (rewindtime > 0)
                usleep(1000);
        }
    }

    UpdateEditSlider();
}

bool NuppelVideoPlayer::IsInDelete(long long testframe) const
{
    long long startpos = 0;
    long long endpos = 0;
    bool first = true;
    bool indelete = false;
    bool ret = false;

    QMap<long long, int>::const_iterator i;
    for (i = deleteMap.begin(); i != deleteMap.end(); ++i)
    {
        if (ret)
            break;

        long long frame = i.key();
        int direction = *i;

        if (direction == 0 && !indelete && first)
        {
            startpos = 0;
            endpos = frame;
            first = false;
            if (startpos <= testframe && endpos >= testframe)
                ret = true;
        }
        else if (direction == 0)
        {
            endpos = frame;
            indelete = false;
            first = false;
            if (startpos <= testframe && endpos >= testframe)
                ret = true;
        }
        else if (direction == 1 && !indelete)
        {
            startpos = frame;
            indelete = true;
            first = false;
        }
        first = false;
    }

    if (indelete && testframe >= startpos)
        ret = true;

    return ret;
}

bool NuppelVideoPlayer::HasTVChainNext(void) const
{
    return player_ctx->tvchain && player_ctx->tvchain->HasNext();
}

void NuppelVideoPlayer::SaveCutList(void)
{
    if (!player_ctx->playingInfo)
        return;

    long long startpos = 0;
    long long endpos = 0;
    bool first = true;
    bool indelete = false;

    long long lastpos = -1;
    int lasttype = -1;

    QMap<long long, int>::Iterator i;

    for (i = deleteMap.begin(); i != deleteMap.end();)
    {
        long long frame = i.key();
        int direction = *i;

        if (direction == 0 && !indelete && first)
        {
            deleteMap[0] = MARK_CUT_START;
            startpos = 0;
            endpos = frame;
        }
        else if (direction == 0)
        {
            endpos = frame;
            indelete = false;
            first = false;
        }
        else if (direction == 1 && !indelete)
        {
            startpos = frame;
            indelete = true;
            first = false;
        }

        if (direction == lasttype)
        {
            if (direction == 0)
            {
                deleteMap.remove(lastpos);
                ++i;
            }
            else
            {
                ++i;
                deleteMap.remove(frame);
            }
        }
        else
            ++i;

        lastpos = frame;
        lasttype = direction;
    }

    if (indelete)
        deleteMap[totalFrames] = MARK_CUT_END;

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
    {
        player_ctx->playingInfo->SetMarkupFlag(MARK_UPDATED_CUT, true);
        player_ctx->playingInfo->SetCutList(deleteMap);
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
}

void NuppelVideoPlayer::LoadCutList(void)
{
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo && !gContext->IsDatabaseIgnored())
        player_ctx->playingInfo->GetCutList(deleteMap);
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
}

bool NuppelVideoPlayer::FrameIsInMap(long long frameNumber,
                                     QMap<long long, int> &breakMap)
{
    if (breakMap.isEmpty())
        return false;

    QMap<long long, int>::Iterator it = breakMap.find(frameNumber);

    if (it != breakMap.end())
        return true;

    int lastType = MARK_UNSET;
    for (it = breakMap.begin(); it != breakMap.end(); ++it)
    {
        if (it.key() > frameNumber)
        {
            int type = *it;

            if (((type == MARK_COMM_END) ||
                 (type == MARK_CUT_END)) &&
                ((lastType == MARK_COMM_START) ||
                 (lastType == MARK_CUT_START)))
                return true;

            if ((type == MARK_COMM_START) ||
                (type == MARK_CUT_START))
                return false;
        }

        lastType = *it;
    }

    return false;
}

/** \fn NuppelVideoPlayer::GetScreenGrab(int,int&,int&,int&,float&)
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
char *NuppelVideoPlayer::GetScreenGrab(int secondsin, int &bufflen,
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
char *NuppelVideoPlayer::GetScreenGrabAtFrame(long long frameNum, bool absolute,
                                              int &bufflen, int &vw, int &vh,
                                              float &ar)
{
    long long      number    = 0;
    long long      oldnumber = 0;
    unsigned char *data      = NULL;
    unsigned char *outputbuf = NULL;
    VideoFrame    *frame     = NULL;
    AVPicture      orig;
    AVPicture      retbuf;
    bzero(&orig,   sizeof(AVPicture));
    bzero(&retbuf, sizeof(AVPicture));

    using_null_videoout = true;

    if (OpenFile(false, 0, false) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Could not open file for preview.");
        return NULL;
    }

    if (!hasFullPositionMap && !player_ctx->buffer->isDVD())
    {
        VERBOSE(VB_IMPORTANT, LOC + "GetScreenGrabAtFrame: Recording does not "
                "have position map so we will be unable to grab the desired "
                "frame.\n");
        player_ctx->LockPlayingInfo(__FILE__, __LINE__);
        if (player_ctx->playingInfo)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Run 'mythcommflag --file %1 --rebuild' to fix.")
                    .arg(player_ctx->playingInfo->GetRecordBasename()));
            VERBOSE(VB_IMPORTANT,
                    QString("If that does not work and this is a .mpg file, "
                            "try 'mythtranscode --mpeg2 --buildindex "
                            "--allkeys -c %1 -s %2'.")
                    .arg(player_ctx->playingInfo->chanid)
                    .arg(player_ctx->playingInfo->recstartts
                         .toString("yyyyMMddhhmmss")));
        }
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
    }

    if ((video_dim.width() <= 0) || (video_dim.height() <= 0))
    {
        VERBOSE(VB_PLAYBACK, QString("NVP: Video Resolution invalid %1x%2")
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

    if (player_ctx->buffer->isDVD())
    {
        if (GoToDVDMenu("menu"))
        {
            if (player_ctx->buffer->DVD()->IsInMenu() &&
                !player_ctx->buffer->DVD()->InStillFrame())
                GoToDVDProgram(1);
        }
        else if (player_ctx->buffer->DVD()->GetTotalTimeOfTitle() < 60)
        {
            GoToDVDProgram(1);
            GetFrame(kDecodeVideo);

            number = frameNum;
            if (number >= totalFrames)
                number = totalFrames / 2;
        }
    }
    else
    {
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
                oldnumber = number;
                LoadCutList();

                player_ctx->LockPlayingInfo(__FILE__, __LINE__);
                commBreakMapLock.lock();

                if (player_ctx->playingInfo)
                    player_ctx->playingInfo->GetCommBreakList(commBreakMap);

                bool started_in_break_map = false;
                while (FrameIsInMap(number, commBreakMap) ||
                       FrameIsInMap(number, deleteMap))
                {
                    started_in_break_map = true;
                    number += (long long) (30 * video_frame_rate);
                    if (number >= totalFrames)
                    {
                        number = oldnumber;
                        break;
                    }
                }

                commBreakMapLock.unlock();
                player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

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
    }

    // Only do seek if we have position map
    if (hasFullPositionMap && !player_ctx->buffer->isDVD())
    {
        GetFrame(kDecodeVideo);
        DiscardVideoFrame(videoOutput->GetLastDecodedFrame());

        fftime = number;
        DoFastForward();
        fftime = 0;
    }

    GetFrame(kDecodeVideo);

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

/** \fn NuppelVideoPlayer::GetRawVideoFrame(long long)
 *  \brief Returns a specific frame from the video.
 *
 *   NOTE: You must call DiscardVideoFrame(VideoFrame*) on
 *         the frame returned, as this marks the frame as
 *         being used and hence unavailable for decoding.
 */
VideoFrame* NuppelVideoPlayer::GetRawVideoFrame(long long frameNumber)
{
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
        player_ctx->playingInfo->UpdateInUseMark();
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    if (frameNumber >= 0)
    {
        DoJumpToFrame(frameNumber);
        ClearAfterSeek();
    }

    GetFrame(kDecodeVideo,true);

    return videoOutput->GetLastDecodedFrame();
}

QString NuppelVideoPlayer::GetEncodingType(void) const
{
    return get_encoding_type(GetDecoder()->GetVideoCodecID());
}

bool NuppelVideoPlayer::GetRawAudioState(void) const
{
    return GetDecoder()->GetRawAudioState();
}

QString NuppelVideoPlayer::GetXDS(const QString &key) const
{
    if (!GetDecoder())
        return QString::null;
    return GetDecoder()->GetXDS(key);
}

void NuppelVideoPlayer::TranscodeWriteText(void (*func)
                                           (void *, unsigned char *, int, int, int), void * ptr)
{

    while (tbuffer_numvalid())
    {
        int pagenr = 0;
        unsigned char *inpos = txtbuffers[rtxt].buffer;
        if (txtbuffers[rtxt].type == 'T')
        {
            memcpy(&pagenr, inpos, sizeof(int));
            inpos += sizeof(int);
            txtbuffers[rtxt].len -= sizeof(int);
        }
        func(ptr, inpos, txtbuffers[rtxt].len,
               txtbuffers[rtxt].timecode, pagenr);
        rtxt = (rtxt + 1) % MAXTBUFFER;
    }
}
void NuppelVideoPlayer::InitForTranscode(bool copyaudio, bool copyvideo)
{
    // Are these really needed?
    SetPlaying(true);
    keyframedist = 30;
    warpfactor = 1;
    warpfactor_avg = 1;

    if (!InitVideo())
    {
        VERBOSE(VB_IMPORTANT, "NVP: Unable to initialize video for transcode.");
        SetPlaying(false);
        return;
    }

    framesPlayed = 0;
    ClearAfterSeek();

    if (copyvideo)
        GetDecoder()->SetRawVideoState(true);
    if (copyaudio)
        GetDecoder()->SetRawAudioState(true);

    GetDecoder()->setExactSeeks(true);
    GetDecoder()->SetLowBuffers(true);
}

bool NuppelVideoPlayer::TranscodeGetNextFrame(QMap<long long, int>::Iterator &dm_iter,
                                              int *did_ff, bool *is_key, bool honorCutList)
{
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
        player_ctx->playingInfo->UpdateInUseMark();
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    long long lastDecodedFrameNumber =
        videoOutput->GetLastDecodedFrame()->frameNumber;

    if ((lastDecodedFrameNumber == 0) && honorCutList)
        dm_iter = deleteMap.begin();

    if (!GetDecoder()->GetFrame(kDecodeAV))
        return false;
    if (eof)
        return false;

    if (honorCutList && (!deleteMap.isEmpty()))
    {
        if (totalFrames && lastDecodedFrameNumber >= totalFrames)
            return false;

        if ((lastDecodedFrameNumber >= dm_iter.key()) ||
            (lastDecodedFrameNumber == -1 && dm_iter.key() == 0))
        {
            while(((*dm_iter) == 1) && (dm_iter != deleteMap.end()))
            {
                QString msg = QString("Fast-Forwarding from %1")
                              .arg((int)dm_iter.key());
                dm_iter++;
                msg += QString(" to %1").arg((int)dm_iter.key());
                VERBOSE(VB_GENERAL, msg);

                if(dm_iter.key() >= totalFrames)
                   return false;

                GetDecoder()->DoFastForward(dm_iter.key());
                GetDecoder()->ClearStoredData();
                ClearAfterSeek();
                GetDecoder()->GetFrame(kDecodeAV);
                *did_ff = 1;
            }
            while(((*dm_iter) == 0) && (dm_iter != deleteMap.end()))
            {
                dm_iter++;
            }
        }
    }
    if (eof)
      return false;
    *is_key = GetDecoder()->isLastFrameKey();
    return true;
}

long NuppelVideoPlayer::UpdateStoredFrameNum(long curFrameNum)
{
    return GetDecoder()->UpdateStoredFrameNum(curFrameNum);
}

void NuppelVideoPlayer::SetCutList(QMap<long long, int> newCutList)
{
    QMap<long long, int>::Iterator i;

    deleteMap.clear();
    for (i = newCutList.begin(); i != newCutList.end(); ++i)
        deleteMap[i.key()] = *i;
}

bool NuppelVideoPlayer::WriteStoredData(RingBuffer *outRingBuffer,
                                        bool writevideo, long timecodeOffset)
{
    if (writevideo && !GetDecoder()->GetRawVideoState())
        writevideo = false;
    GetDecoder()->WriteStoredData(outRingBuffer, writevideo, timecodeOffset);
    return writevideo;
}

void NuppelVideoPlayer::SetCommBreakMap(QMap<long long, int> &newMap)
{
    QMutexLocker locker(&commBreakMapLock);

    VERBOSE(VB_COMMFLAG,
            QString("Setting New Commercial Break List, old size %1, new %2")
                    .arg(commBreakMap.size()).arg(newMap.size()));

    commBreakMap.clear();
    commBreakMap = newMap;
    hascommbreaktable = !commBreakMap.isEmpty();
    SetCommBreakIter();

    forcePositionMapSync = true;
}

class RebuildSaver : public QRunnable
{
  public:
    RebuildSaver(DecoderBase *d, uint64_t f, uint64_t l)
        : m_decoder(d), m_first(f), m_last(l)
    {
        QMutexLocker locker(&s_lock);
        s_cnt[d]++;
    }

    virtual void run(void)
    {
        m_decoder->SavePositionMapDelta(m_first, m_last);

        QMutexLocker locker(&s_lock);
        s_cnt[m_decoder]--;
        if (!s_cnt[m_decoder])
            s_wait.wakeAll();
    }

    static uint GetCount(DecoderBase *d)
    {
        QMutexLocker locker(&s_lock);
        return s_cnt[d];
    }

    static void Wait(DecoderBase *d)
    {
        QMutexLocker locker(&s_lock);
        if (!s_cnt[d])
            return;
        while (s_wait.wait(&s_lock))
        {
            if (!s_cnt[d])
                return;
        }
    }

  private:
    DecoderBase *m_decoder;
    uint64_t     m_first;
    uint64_t     m_last;

    static QMutex                  s_lock;
    static QWaitCondition          s_wait;
    static QMap<DecoderBase*,uint> s_cnt;
};
QMutex                  RebuildSaver::s_lock;
QWaitCondition          RebuildSaver::s_wait;
QMap<DecoderBase*,uint> RebuildSaver::s_cnt;

bool NuppelVideoPlayer::RebuildSeekTable(
    bool showPercentage, StatusCallback cb, void* cbData)
{
    int percentage = 0;
    uint64_t myFramesPlayed = 0, pmap_first = 0,  pmap_last  = 0;

    killplayer = false;
    framesPlayed = 0;
    using_null_videoout = true;

    // clear out any existing seektables
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
    {
        player_ctx->playingInfo->ClearPositionMap(MARK_KEYFRAME);
        player_ctx->playingInfo->ClearPositionMap(MARK_GOP_START);
        player_ctx->playingInfo->ClearPositionMap(MARK_GOP_BYFRAME);
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    if (OpenFile() < 0)
        return false;

    SetPlaying(true);

    if (!InitVideo())
    {
        VERBOSE(VB_IMPORTANT,
                LOC_ERR + "RebuildSeekTable unable to initialize video");
        SetPlaying(false);
        return false;
    }

    ClearAfterSeek();

    bool has_ui = showPercentage || cb;
    int save_timeout = 1001;
    MythTimer flagTime, ui_timer, inuse_timer, save_timer;
    flagTime.start();
    ui_timer.start();
    inuse_timer.start();
    save_timer.start();

    GetFrame(kDecodeNothing,true);
    if (showPercentage)
    {
        if (totalFrames)
            printf("             ");
        else
            printf("      ");
        fflush( stdout );
    }

    while (!eof)
    {
        myFramesPlayed++;

        if (inuse_timer.elapsed() > 2534)
        {
            inuse_timer.restart();
            player_ctx->LockPlayingInfo(__FILE__, __LINE__);
            if (player_ctx->playingInfo)
                player_ctx->playingInfo->UpdateInUseMark();
            player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        }

        if (save_timer.elapsed() > save_timeout)
        {
            // Give DB some breathing room if it gets far behind..
            if (myFramesPlayed - pmap_last > 5000)
                usleep(200 * 1000);

            // If we're already saving, just save a larger block next time..
            if (RebuildSaver::GetCount(GetDecoder()) < 1)
            {
                pmap_last = myFramesPlayed;
                QThreadPool::globalInstance()->start(
                    new RebuildSaver(GetDecoder(), pmap_first, pmap_last));
                pmap_first = pmap_last + 1;
            }

            save_timer.restart();
        }

        if (has_ui && (ui_timer.elapsed() > 98))
        {
            ui_timer.restart();

            if (totalFrames)
            {
                float elapsed = flagTime.elapsed() * 0.001f;
                int flagFPS = (elapsed > 0.0f) ?
                    (int)(myFramesPlayed / elapsed) : 0;

                percentage = myFramesPlayed * 100 / totalFrames;
                if (cb)
                    (*cb)(percentage, cbData);

                if (showPercentage)
                {
                    printf( "\b\b\b\b\b\b\b\b\b\b\b\b\b" );
                    printf( "%3d%%/%5dfps", percentage, flagFPS );
                    fflush( stdout );
                }
            }
            else if (showPercentage)
            {
                printf( "\b\b\b\b\b\b" );
                printf( "%6lld", (long long)myFramesPlayed );
                fflush( stdout );
            }
        }

        GetFrame(kDecodeNothing,true);
    }

    if (showPercentage)
    {
        if (totalFrames)
            printf( "\b\b\b\b\b\b\b" );
        printf( "\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b" );
        fflush( stdout );
    }

    SetPlaying(false);
    killplayer = true;

    QThreadPool::globalInstance()->start(
        new RebuildSaver(GetDecoder(), pmap_first, pmap_last));
    RebuildSaver::Wait(GetDecoder());

    return true;
}

int NuppelVideoPlayer::GetStatusbarPos(void) const
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

int NuppelVideoPlayer::GetSecondsBehind(void) const
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

void NuppelVideoPlayer::calcSliderPos(struct StatusPosInfo &posInfo,
                                      bool paddedFields)
{
    bool islive = false;
    posInfo.desc = "";
    posInfo.position = 0;
    posInfo.progBefore = false;
    posInfo.progAfter = false;

    if (player_ctx->buffer->isDVD() && player_ctx->buffer->DVD()->IsInMenu())
    {
        long long rPos = player_ctx->buffer->GetReadPosition();
        long long tPos = 1;//player_ctx->buffer->GetTotalReadPosition();

        player_ctx->buffer->DVD()->GetDescForPos(posInfo.desc);

        if (rPos)
            posInfo.position = (int)((rPos / tPos) * 1000.0);

        return;
    }

    int playbackLen = totalLength;

    if (livetv && player_ctx->tvchain)
    {
        posInfo.progBefore = player_ctx->tvchain->HasPrev();
        posInfo.progAfter = player_ctx->tvchain->HasNext();
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

    float secsplayed = 0.0f;
    if (player_ctx->buffer->isDVD())
    {
        if (!player_ctx->buffer->DVD()->IsInMenu())
#if !CONFIG_CYGWIN
            secsplayed = player_ctx->buffer->DVD()->GetCurrentTime();
#else
            // DVD playing non-functional under windows for now
            secsplayed = 0.0f;
#endif
    }
    else
        secsplayed = ((float)framesPlayed / video_frame_rate);

    playbackLen = max(playbackLen, 1);
    secsplayed  = min((float)playbackLen, max(secsplayed, 0.0f));

    posInfo.position = (int)(1000.0f * (secsplayed / (float)playbackLen));

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

    posInfo.desc = QObject::tr("%1 of %2").arg(text1).arg(text2);

    if (islive)
    {
        posInfo.extdesc = QObject::tr("%1 of %2 (%3 behind)")
                .arg(text1).arg(text2).arg(text3);
    }
    else
    {
        posInfo.extdesc = QObject::tr("%1 of %2 (%3 remaining)")
                .arg(text1).arg(text2).arg(text3);
    }
}

void NuppelVideoPlayer::MergeShortCommercials(void)
{
    double maxMerge = gContext->GetNumSetting("MergeShortCommBreaks", 0) *
                       video_frame_rate;
    if (maxMerge <= 0.0 || (commBreakIter == commBreakMap.end()))
        return;

    long long lastFrame = commBreakIter.key();
    ++commBreakIter;
    while ((commBreakIter != commBreakMap.end()) &&
           (commBreakIter.key() - lastFrame < maxMerge))
    {
        ++commBreakIter;
    }
    --commBreakIter;
}

void NuppelVideoPlayer::AutoCommercialSkip(void)
{
    QMutexLocker locker(&commBreakMapLock);

    if (!hascommbreaktable)
        return;

    if (((time(NULL) - lastSkipTime) <= 3) ||
        ((time(NULL) - lastCommSkipTime) <= 3))
    {
        SetCommBreakIter();
        return;
    }

    if (commBreakIter == commBreakMap.end())
        return;

    if (*commBreakIter == MARK_COMM_END)
        commBreakIter++;

    if (commBreakIter == commBreakMap.end())
        return;

    if (!((*commBreakIter == MARK_COMM_START) &&
          (((kCommSkipOn == autocommercialskip) &&
            (framesPlayed >= commBreakIter.key())) ||
           ((kCommSkipNotify == autocommercialskip) &&
            (framesPlayed + commnotifyamount * video_frame_rate >=
             commBreakIter.key())))))
    {
        return;
    }

    VERBOSE(VB_COMMFLAG, LOC + QString("AutoCommercialSkip(), current "
                                       "framesPlayed %1, commBreakIter "
                                       "frame %2, incrementing "
                                       "commBreakIter")
            .arg(framesPlayed).arg(commBreakIter.key()));

    ++commBreakIter;

    MergeShortCommercials();

    if (commBreakIter == commBreakMap.end())
    {
        VERBOSE(VB_COMMFLAG, LOC + "AutoCommercialSkip(), at "
                "end of commercial break list, will not skip.");
        return;
    }

    if (*commBreakIter == MARK_COMM_START)
    {
        VERBOSE(VB_COMMFLAG, LOC + "AutoCommercialSkip(), new "
                "commBreakIter mark is another start, will not skip.");
        return;
    }

    if (totalFrames &&
        ((commBreakIter.key() + (10 * video_frame_rate)) > totalFrames))
    {
        VERBOSE(VB_COMMFLAG, LOC + "AutoCommercialSkip(), skipping "
                "would take us to the end of the file, will not skip.");
        return;
    }

    VERBOSE(VB_COMMFLAG, LOC + QString("AutoCommercialSkip(), new "
                                       "commBreakIter frame %1")
            .arg(commBreakIter.key()));

    if (osd)
    {
        QString comm_msg;
        int skipped_seconds = (int)((commBreakIter.key() -
                                     framesPlayed) / video_frame_rate);
        QString skipTime;
        skipTime.sprintf("%d:%02d", skipped_seconds / 60,
                         abs(skipped_seconds) % 60);
        if (kCommSkipOn == autocommercialskip)
        {
            comm_msg = QString(QObject::tr("Skip %1"))
                .arg(skipTime);
        }
        else
        {
            comm_msg = QString(QObject::tr("Commercial: %1"))
                .arg(skipTime);
        }
        struct StatusPosInfo posInfo;
        calcSliderPos(posInfo);
        osd->ShowStatus(posInfo, false, comm_msg, 2);
    }

    if (kCommSkipOn == autocommercialskip)
    {
        VERBOSE(VB_COMMFLAG, LOC + QString("AutoCommercialSkip(), "
                                           "auto-skipping to frame %1")
                .arg(commBreakIter.key() -
                     (int)(commrewindamount * video_frame_rate)));

        lastCommSkipDirection = 1;
        lastCommSkipStart = framesPlayed;
        lastCommSkipTime = time(NULL);

        long long jump_to = commBreakIter.key() -
            (int)(commrewindamount * video_frame_rate);

        locker.unlock();

        {
            QMutexLocker locker(&internalPauseLock);
            PauseVideo(true);
            DoJumpToFrame(jump_to);
            UnpauseVideo(true);
        }

        GetFrame(kDecodeVideo, true);
    }
    else
    {
        ++commBreakIter;
    }
}

int NuppelVideoPlayer::GetNumChapters()
{
    return GetDecoder()->GetNumChapters();
}

int NuppelVideoPlayer::GetCurrentChapter()
{
    return GetDecoder()->GetCurrentChapter(framesPlayed);
}

void NuppelVideoPlayer::GetChapterTimes(QList<long long> &times)
{
    return GetDecoder()->GetChapterTimes(times);
}

bool NuppelVideoPlayer::DoJumpChapter(int chapter)
{
    long long desiredFrame = -1;
    int total = GetNumChapters();

    if (chapter < 0 || chapter > total)
    {
        int current = GetCurrentChapter() + 1;
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

    desiredFrame = GetDecoder()->GetChapter(chapter);

    if (desiredFrame < 0)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR + QString("DoJumpChapter failed."));
        jumpchapter = 0;
        return false;
    }

    if (paused && !editmode)
        GetDecoder()->setExactSeeks(true);
    if (desiredFrame < framesPlayed)
        GetDecoder()->DoRewind(desiredFrame);
    else
        GetDecoder()->DoFastForward(desiredFrame);
    GetDecoder()->setExactSeeks(exactseeks);

    // Note: The video output will be reset by what the the decoder
    //       does, so we only need to reset the audio, subtitles, etc.
    ClearAfterSeek(false);

    lastSkipTime = time(NULL);
    jumpchapter = 0;
    return true;
}

bool NuppelVideoPlayer::DoSkipCommercials(int direction)
{
    if (!hascommbreaktable)
    {
        if (osd)
        {
            struct StatusPosInfo posInfo;
            calcSliderPos(posInfo);
            osd->ShowStatus(posInfo, false, QObject::tr("Not Flagged"), 2);
        }

        QString message = "COMMFLAG_REQUEST ";
        player_ctx->LockPlayingInfo(__FILE__, __LINE__);
        message += player_ctx->playingInfo->chanid + " " +
                   player_ctx->playingInfo->recstartts.toString(Qt::ISODate);
        player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
        RemoteSendMessage(message);

        return false;
    }

    if ((direction == (0 - lastCommSkipDirection)) &&
        ((time(NULL) - lastCommSkipTime) <= 5))
    {
        if (osd)
        {
            struct StatusPosInfo posInfo;
            calcSliderPos(posInfo);
            osd->ShowStatus(posInfo, false, QObject::tr("Skipping Back."), 2);
        }

        if (lastCommSkipStart > (2.0 * video_frame_rate))
            lastCommSkipStart -= (long long) (2.0 * video_frame_rate);

        DoJumpToFrame(lastCommSkipStart);
        lastCommSkipDirection = 0;
        lastCommSkipTime = time(NULL);
        return true;
    }
    lastCommSkipDirection = direction;
    lastCommSkipStart = framesPlayed;
    lastCommSkipTime = time(NULL);

    commBreakMapLock.lock();
    SetCommBreakIter();

    if ((commBreakIter == commBreakMap.begin()) &&
        (direction < 0))
    {
        if (osd)
        {
            struct StatusPosInfo posInfo;
            calcSliderPos(posInfo);
            osd->ShowStatus(posInfo, false,
                            QObject::tr("Start of program."), 2);
        }

        commBreakMapLock.unlock();

        DoJumpToFrame(0);
        return true;
    }

    if ((direction > 0) &&
        ((commBreakIter == commBreakMap.end()) ||
         ((totalFrames) &&
          ((commBreakIter.key() + (10 * video_frame_rate)) > totalFrames))))
    {
        if (osd)
        {
            struct StatusPosInfo posInfo;
            calcSliderPos(posInfo);
            osd->ShowStatus(posInfo, false,
                            QObject::tr("At End, can not Skip."), 2);
        }
        commBreakMapLock.unlock();
        return false;
    }

    if (direction < 0)
    {
        commBreakIter--;

        int skipped_seconds = (int)((commBreakIter.key() -
                framesPlayed) / video_frame_rate);

        // special case when hitting 'skip backwards' <3 seconds after break
        if (skipped_seconds > -3)
        {
            if (commBreakIter == commBreakMap.begin())
            {
                if (osd)
                {
                    struct StatusPosInfo posInfo;
                    calcSliderPos(posInfo);
                    osd->ShowStatus(posInfo, false,
                                    QObject::tr("Start of program."), 2);
                }

                commBreakMapLock.unlock();
                DoJumpToFrame(0);

                return true;
            }
            else
                commBreakIter--;
        }
    }
    else if (*commBreakIter == MARK_COMM_START)
    {
        int skipped_seconds = (int)((commBreakIter.key() -
                framesPlayed) / video_frame_rate);

        // special case when hitting 'skip' < 20 seconds before break
        if (skipped_seconds < 20)
        {
            commBreakIter++;

            if ((commBreakIter == commBreakMap.end()) ||
                ((totalFrames) &&
                 ((commBreakIter.key() + (10 * video_frame_rate)) >
                                                                totalFrames)))
            {
                if (osd)
                {
                    struct StatusPosInfo posInfo;
                    calcSliderPos(posInfo);
                    osd->ShowStatus(posInfo, false,
                                    QObject::tr("At End, can not Skip."), 2);
                }
                commBreakMapLock.unlock();
                return false;
            }
        }
    }

    if (direction > 0)
        MergeShortCommercials();

    if (osd)
    {
        int skipped_seconds = (int)((commBreakIter.key() -
                framesPlayed) / video_frame_rate);
        int maxskip = gContext->GetNumSetting("MaximumCommercialSkip", 3600);
        QString skipTime;
        skipTime.sprintf("%d:%02d", skipped_seconds / 60,
                         abs(skipped_seconds) % 60);
        struct StatusPosInfo posInfo;
        calcSliderPos(posInfo);
        if ((lastIgnoredManualSkip.secsTo(QDateTime::currentDateTime()) > 3) &&
            (abs(skipped_seconds) >= maxskip))
        {
            osd->ShowStatus(posInfo, false,
                            QObject::tr("Too Far %1").arg(skipTime), 2);
            commBreakMapLock.unlock();
            lastIgnoredManualSkip = QDateTime::currentDateTime();
            return false;
        }
        osd->ShowStatus(posInfo, false,
                        QObject::tr("Skip %1").arg(skipTime), 2);
    }

    long long jumpto = (direction > 0) ?
        commBreakIter.key() - (long long)(commrewindamount * video_frame_rate):
        commBreakIter.key();
    commBreakIter++;
    commBreakMapLock.unlock();

    DoJumpToFrame(jumpto);

    return true;
}

QStringList NuppelVideoPlayer::GetTracks(uint type) const
{
    // needs lock?
    if (GetDecoder())
        return GetDecoder()->GetTracks(type);
    return QStringList();
}

int NuppelVideoPlayer::SetTrack(uint type, int trackNo)
{
    int ret = -1;

    QMutexLocker locker(&decoder_change_lock);

    if (decoder)
        ret = decoder->SetTrack(type, trackNo);

    if (kTrackTypeAudio == type)
    {
        QString msg = "";

        if (decoder)
        {
            msg = decoder->GetTrackDesc(type, GetTrack(type));

            if (player_ctx->buffer->isDVD())
                player_ctx->buffer->DVD()->SetTrack(type, trackNo);
        }
        if (osd)
            osd->SetSettingsText(msg, 3);
    }
    if (kTrackTypeSubtitle == type)
    {
        DisableCaptions(textDisplayMode, false);
        EnableCaptions(kDisplayAVSubtitle);
    }
    else if (kTrackTypeCC708 == type)
    {
        if (osd && decoder)
        {
            int sid = decoder->GetTrackInfo(type, trackNo).stream_id;
            if (sid >= 0)
                osd->SetCC708Service(&CC708services[sid]);
        }
        DisableCaptions(textDisplayMode, false);
        EnableCaptions(kDisplayCC708);
    }
    else if (kTrackTypeCC608 == type)
    {
        if (decoder)
        {
            int sid = decoder->GetTrackInfo(type, trackNo).stream_id;
            ccmode = (sid <= 2) ?
                ((sid == 1) ? CC_CC1 : CC_CC2) :
                ((sid == 3) ? CC_CC3 : CC_CC4);
        }
        DisableCaptions(textDisplayMode, false);
        EnableCaptions(kDisplayCC608, false);
    }
    else if (kTrackTypeTeletextCaptions == type)
    {
        DisableCaptions(textDisplayMode, false);
        EnableCaptions(kDisplayTeletextCaptions);
    }
    return ret;
}

/** \fn NuppelVideoPlayer::TracksChanged(uint)
 *  \brief This tries to re-enable captions/subtitles if the user
 *         wants them and one of the captions/subtitles tracks has
 *         changed.
 */
void NuppelVideoPlayer::TracksChanged(uint trackType)
{
    if (trackType >= kTrackTypeSubtitle &&
        trackType <= kTrackTypeTeletextCaptions && textDesired)
    {
        SetCaptionsEnabled(true, false);
    }
}

InteractiveTV *NuppelVideoPlayer::GetInteractiveTV(void)
{
#ifdef USING_MHEG
    if (!interactiveTV && osd && itvEnabled)
        interactiveTV = new InteractiveTV(this);
#endif // USING_MHEG
    return interactiveTV;
}

bool NuppelVideoPlayer::ITVHandleAction(const QString &action)
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

/** \fn NuppelVideoPlayer::ITVRestart(uint chanid, uint cardid, bool isLive)
 *  \brief Restart the MHEG/MHP engine.
 */
void NuppelVideoPlayer::ITVRestart(uint chanid, uint cardid, bool isLiveTV)
{
#ifdef USING_MHEG
    QMutexLocker locker(&decoder_change_lock);

    OSD *osd = GetOSD();
    if (!GetDecoder() || !osd)
        return;

    OSDSet *itvosd = osd->GetSet("interactive");
    if (!itvosd)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "No interactive TV set available");
        return;
    }

    {
        QMutexLocker locker(&itvLock);
        if (GetInteractiveTV())
            interactiveTV->Restart(chanid, cardid, isLiveTV);
    }

    osd->ClearAll("interactive");
    itvosd->Display();
    osd->SetVisible(itvosd, 0);
#endif // USING_MHEG
}

void NuppelVideoPlayer::SetVideoResize(const QRect &videoRect)
{
    if (videoOutput)
        videoOutput->SetVideoResize(videoRect);
}

/** \fn NuppelVideoPlayer::SetAudioByComponentTag(int tag)
 *  \brief Selects the audio stream using the DVB component tag.
 */
bool NuppelVideoPlayer::SetAudioByComponentTag(int tag)
{
    if (GetDecoder())
        return GetDecoder()->SetAudioByComponentTag(tag);

    return false;
}

/** \fn NuppelVideoPlayer::SetVideoByComponentTag(int tag)
 *  \brief Selects the video stream using the DVB component tag.
 */
bool NuppelVideoPlayer::SetVideoByComponentTag(int tag)
{
    if (GetDecoder())
        return GetDecoder()->SetVideoByComponentTag(tag);

    return false;
}

int NuppelVideoPlayer::GetTrack(uint type) const
{
    // needs lock?
    if (GetDecoder())
        return GetDecoder()->GetTrack(type);
    return -1;
}

int NuppelVideoPlayer::ChangeTrack(uint type, int dir)
{
    QMutexLocker locker(&decoder_change_lock);

    if (GetDecoder())
    {
        int retval;

        if ((retval = GetDecoder()->ChangeTrack(type, dir)) >= 0)
        {
            QString msg = "";

            //msg = toString((TrackType)type);

            msg = GetDecoder()->GetTrackDesc(type, GetTrack(type));

            if (osd)
                osd->SetSettingsText(msg, 3);

            return retval;
        }
    }
    return -1;
}

// Cycles through all available caption/subtitle types
// so far only dir > 0 implemented
void NuppelVideoPlayer::ChangeCaptionTrack(int dir)
{
    QMutexLocker locker(&decoder_change_lock);

    if (!decoder || (dir < 0))
        return;

    if (!textDisplayMode)
    {
        if (vbimode == VBIMode::PAL_TT)
        {
            if (decoder->GetTrackCount(kTrackTypeSubtitle))
                SetTrack(kTrackTypeSubtitle, 0);
            else if (decoder->GetTrackCount(kTrackTypeTeletextCaptions))
                SetTrack(kTrackTypeTeletextCaptions, 0);
            else
                EnableCaptions(kDisplayNUVTeletextCaptions);
        }
        else
        {
            if (decoder->GetTrackCount(kTrackTypeCC708))
                SetTrack(kTrackTypeCC708, 0);
            else if (decoder->GetTrackCount(kTrackTypeCC608))
                SetTrack(kTrackTypeCC608, 0);
            else if (decoder->GetTrackCount(kTrackTypeSubtitle))
                SetTrack(kTrackTypeSubtitle, 0);
        }
    }
    else if ((textDisplayMode & kDisplayAVSubtitle) &&
             (vbimode == VBIMode::PAL_TT))
    {
        if ((uint)GetTrack(kTrackTypeSubtitle) + 1 <
            decoder->GetTrackCount(kTrackTypeSubtitle))
        {
            SetTrack(kTrackTypeSubtitle, GetTrack(kTrackTypeSubtitle) + 1);
        }
        else
        {
            DisableCaptions(textDisplayMode, false);
            if (decoder->GetTrackCount(kTrackTypeTeletextCaptions))
                SetTrack(kTrackTypeTeletextCaptions, 0);
            else
                EnableCaptions(kDisplayNUVTeletextCaptions);
        }
    }
    else if ((textDisplayMode & kDisplayTeletextCaptions) &&
             (vbimode == VBIMode::PAL_TT))
    {
        if ((uint)GetTrack(kTrackTypeTeletextCaptions) + 1 <
            decoder->GetTrackCount(kTrackTypeTeletextCaptions))
        {
            SetTrack(kTrackTypeTeletextCaptions,
                     GetTrack(kTrackTypeTeletextCaptions) + 1);
        }
        else
        {
            DisableCaptions(textDisplayMode, false),
            EnableCaptions(kDisplayNUVTeletextCaptions);
        }
    }
    else if (textDisplayMode & kDisplayNUVTeletextCaptions)
        SetCaptionsEnabled(false);
    else if (textDisplayMode & kDisplayCC708)
    {
        if ((uint)GetTrack(kTrackTypeCC708) + 1 <
            decoder->GetTrackCount(kTrackTypeCC708))
        {
            SetTrack(kTrackTypeCC708, GetTrack(kTrackTypeCC708) + 1);
        }
        else if (decoder->GetTrackCount(kTrackTypeCC608))
            SetTrack(kTrackTypeCC608, 0);
        else if (decoder->GetTrackCount(kTrackTypeSubtitle))
            SetTrack(kTrackTypeSubtitle, 0);
        else
            SetCaptionsEnabled(false);
    }
    else if (textDisplayMode & kDisplayCC608)
    {
        if ((uint)GetTrack(kTrackTypeCC608) + 1 <
            decoder->GetTrackCount(kTrackTypeCC608))
        {
            SetTrack(kTrackTypeCC608, GetTrack(kTrackTypeCC608) + 1);
        }
        else if (decoder->GetTrackCount(kTrackTypeSubtitle))
            SetTrack(kTrackTypeSubtitle, 0);
        else
            SetCaptionsEnabled(false);
    }
    else if ((textDisplayMode & kDisplayAVSubtitle) &&
             (vbimode == VBIMode::NTSC_CC))
    {
        if ((uint)GetTrack(kTrackTypeSubtitle) + 1 <
            decoder->GetTrackCount(kTrackTypeSubtitle))
        {
            SetTrack(kTrackTypeSubtitle, GetTrack(kTrackTypeSubtitle) + 1);
        }
        else
            SetCaptionsEnabled(false);
    }
}

#define MAX_SUBTITLE_DISPLAY_TIME_MS 4500
/** \fn NuppelVideoPlayer::DisplayAVSubtitles(void)
 *  \brief Displays new subtitles and removes old ones.
 *
 *  This version is for AVSubtitles which are added with AddAVSubtitles()
 *  when found in the input stream.
 */
void NuppelVideoPlayer::DisplayAVSubtitles(void)
{
    OSDSet *subtitleOSD;
    bool setVisible = false;
    long long subtitles_start_at=0;
    VideoFrame *currentFrame = videoOutput->GetLastShownFrame();

    if (!osd || !currentFrame || !(subtitleOSD = osd->GetSet("subtitles")))
        return;

    subtitleLock.lock();

    // hide the subs if they have been long enough in the screen without
    // new subtitles replacing them
    if (osdHasSubtitles && currentFrame->timecode >= osdSubtitlesExpireAt)
    {
        osd->HideSet("subtitles");
        VERBOSE(VB_PLAYBACK,
                QString("Clearing expired subtitles from OSD set."));
        osd->ClearAll("subtitles");
        osdHasSubtitles = false;
    }

    while (!nonDisplayedAVSubtitles.empty())
    {
        const AVSubtitle subtitlePage = nonDisplayedAVSubtitles.front();

        if (subtitlePage.start_display_time > currentFrame->timecode)
            break;

        nonDisplayedAVSubtitles.pop_front();

        // clear old subtitles
        if (osdHasSubtitles)
        {
            osd->HideSet("subtitles");
            osd->ClearAll("subtitles");
            osdHasSubtitles = false;
        }

        // draw the subtitle bitmap(s) to the OSD
        for (std::size_t i = 0; i < subtitlePage.num_rects; ++i)
        {
            AVSubtitleRect* rect = subtitlePage.rects[i];

            bool displaysub = true;
            if (nonDisplayedAVSubtitles.size() > 0 &&
                nonDisplayedAVSubtitles.front().end_display_time <
                currentFrame->timecode)
            {
                displaysub = false;
            }

            if (displaysub && rect->type == SUBTITLE_BITMAP)
            {
                // AVSubtitleRect's image data's not guaranteed to be 4 byte
                // aligned.

                QRect qrect(rect->x, rect->y, rect->w, rect->h);
                QRect scaled = videoOutput->GetImageRect(qrect);

                QImage qImage(rect->w, rect->h, QImage::Format_ARGB32);
                for (int y = 0; y < rect->h; ++y)
                {
                    for (int x = 0; x < rect->w; ++x)
                    {
                        const uint8_t color = rect->pict.data[0][y*rect->pict.linesize[0] + x];
                        const uint32_t pixel = *((uint32_t*)rect->pict.data[1]+color);
                        qImage.setPixel(x, y, pixel);
                    }
                }


                if (scaled.size() != qrect.size())
                {
                    qImage = qImage.scaled(scaled.width(), scaled.height(),
                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                }

                OSDTypeImage* image = new OSDTypeImage();
                image->SetPosition(scaled.topLeft(), 1.0f, 1.0f);
                image->Load(qImage);

                subtitleOSD->AddType(image);

                subtitles_start_at   = subtitlePage.start_display_time;
                osdSubtitlesExpireAt = subtitlePage.end_display_time;
                // fix subtitles that don't display for very long (if at all).
                if (subtitlePage.end_display_time <=
                    subtitlePage.start_display_time)
                {
                    osdSubtitlesExpireAt = subtitles_start_at + MAX_SUBTITLE_DISPLAY_TIME_MS;

                    if (nonDisplayedAVSubtitles.size() > 0)
                    {
                        long long next_start = nonDisplayedAVSubtitles.front().start_display_time;
                        osdSubtitlesExpireAt = min(osdSubtitlesExpireAt, next_start);
                    }
                }
                // fix delayed subtitles
                else if (subtitles_start_at < currentFrame->timecode)
                {
                    VERBOSE(VB_PLAYBACK, LOC + QString("Delayed subtitle: %1ms late")
                            .arg(currentFrame->timecode - subtitles_start_at));
                    osdSubtitlesExpireAt = currentFrame->timecode
                        + subtitlePage.end_display_time - subtitles_start_at;

                    if (nonDisplayedAVSubtitles.size() > 0)
                    {
                        long long next_start = nonDisplayedAVSubtitles.front().start_display_time;
                        osdSubtitlesExpireAt = min(osdSubtitlesExpireAt, next_start);
                    }
                }

                setVisible = true;
                osdHasSubtitles = true;
            }

            av_free(rect->pict.data[0]);
            av_free(rect->pict.data[1]);
            av_free(rect);
        }

        if (subtitlePage.num_rects > 0)
            av_free(subtitlePage.rects);
    }

    subtitleLock.unlock();

    if (setVisible)
    {
        VERBOSE(VB_PLAYBACK,
                QString("Setting subtitles visible, frame_timecode=%1 "
                        "starts=%2 expires=%3")
                .arg(currentFrame->timecode).arg(subtitles_start_at)
                .arg(osdSubtitlesExpireAt));
        osd->SetVisible(subtitleOSD, 0);
    }
}

/** \fn NuppelVideoPlayer::DisplayTextSubtitles(void)
 *  \brief Displays subtitles in textual format.
 *
 *  This version is for subtitles that are loaded from an external subtitle
 *  file by using the LoadExternalSubtitles() method. Subtitles are not
 *  deleted after displaying so they can be displayed again after seeking.
 */
void NuppelVideoPlayer::DisplayTextSubtitles(void)
{
    VideoFrame *currentFrame = videoOutput->GetLastShownFrame();

    if (!osd || !currentFrame)
    {
        VERBOSE(VB_PLAYBACK, "osd or current video frame not found");
        return;
    }

    QMutexLocker locker(&subtitleLock);

    // frame time code in frames shown or millisecs from the start
    // depending on the timing type of the subtitles
    uint64_t playPos = 0;
    if (textSubtitles.IsFrameBasedTiming())
    {
        // frame based subtitles get out of synch after running mythcommflag
        // for the file, i.e., the following number is wrong and does not
        // match the subtitle frame numbers:
        playPos = currentFrame->frameNumber;
    }
    else
    {
        // Use timecodes for time based SRT subtitles. Feeding this into
        // NormalizeVideoTimecode() should adjust for non-zero start times
        // and wraps. For MPEG, wraps will occur just once every 26.5 hours
        // and other formats less frequently so this should be sufficient.
        // Note: timecodes should now always be valid even in the case
        // when a frame doesn't have a valid timestamp. If an exception is
        // found where this is not true then we need to use the frameNumber
        // when timecode is not defined by uncommenting the following lines.
        //if (currentFrame->timecode == 0)
        //    playPos = (uint64_t)
        //        ((currentFrame->frameNumber / video_frame_rate) * 1000);
        //else
        playPos = GetDecoder()->NormalizeVideoTimecode(currentFrame->timecode);
    }

    if (!textSubtitles.HasSubtitleChanged(playPos))
        return;

    QStringList subtitlesToShow = textSubtitles.GetSubtitles(playPos);

    osdHasSubtitles = !subtitlesToShow.empty();
    if (osdHasSubtitles)
        osd->SetTextSubtitles(subtitlesToShow);
    else
        osd->ClearTextSubtitles();
}

/** \fn NuppelVideoPlayer::ExpireSubtitles(void)
 *  \brief Discard non-displayed subtitles.
 */
void NuppelVideoPlayer::ExpireSubtitles(void)
{
    QMutexLocker locker(&subtitleLock);

    if (!videoOutput)
        return;

    VideoFrame *currentFrame = videoOutput->GetLastShownFrame();

    while (!nonDisplayedAVSubtitles.empty())
    {
        const AVSubtitle subtitlePage = nonDisplayedAVSubtitles.front();

        // Stop when we hit one old enough
        if (subtitlePage.end_display_time > currentFrame->timecode)
            break;

        nonDisplayedAVSubtitles.pop_front();

        for (std::size_t i = 0; i < subtitlePage.num_rects; ++i)
        {
            AVSubtitleRect* rect = subtitlePage.rects[i];
            av_free(rect->pict.data[0]);
            av_free(rect->pict.data[1]);
            av_free(rect);
        }

        if (subtitlePage.num_rects > 0)
            av_free(subtitlePage.rects);
    }
}

/** \fn NuppelVideoPlayer::ClearSubtitles(void)
 *  \brief Hide subtitles and free the undisplayed subtitles.
 */
void NuppelVideoPlayer::ClearSubtitles(void)
{
    subtitleLock.lock();

    while (!nonDisplayedAVSubtitles.empty())
    {
        AVSubtitle& subtitle = nonDisplayedAVSubtitles.front();

        // Because the subtitles were not displayed, OSDSet does not
        // free the OSDTypeImages in ClearAll(), so we have to free
        // the dynamic buffers here
        for (std::size_t i = 0; i < subtitle.num_rects; ++i)
        {
             AVSubtitleRect* rect = subtitle.rects[i];
             av_free(rect->pict.data[0]);
             av_free(rect->pict.data[1]);
        }

        if (subtitle.num_rects > 0)
            av_free(subtitle.rects);

        nonDisplayedAVSubtitles.pop_front();
    }

    subtitleLock.unlock();

    // Clear subtitles from OSD
    if (osdHasSubtitles && osd)
    {
        if (OSDSet *osdSet = osd->GetSet("subtitles"))
        {
            osd->HideSet("subtitles");
            osdSet->Clear();
            osdHasSubtitles = false;
        }
    }
}

/** \fn void NuppelVideoPlayer::AddAVSubtitle(const AVSubtitle&)
 *  \brief Adds a new AVSubtitle to be shown.
 *
 *  NOTE: Assumes the subtitles are pushed in the order they should be shown.
 *
 *  FIXME: Need to fix subtitles to use a 64bit timestamp
 */
void NuppelVideoPlayer::AddAVSubtitle(const AVSubtitle &subtitle)
{
    subtitleLock.lock();
    nonDisplayedAVSubtitles.push_back(subtitle);
    subtitleLock.unlock();
}

/** \fn NuppelVideoPlayer::SetDecoder(DecoderBase*)
 *  \brief Sets the stream decoder, deleting any existing recorder.
 */
void NuppelVideoPlayer::SetDecoder(DecoderBase *dec)
{
#if 0
    DummyDecoder    *dummy = dynamic_cast<DummyDecoder*>(dec);
    NuppelDecoder   *nuv   = dynamic_cast<NuppelDecoder*>(dec);
    AvFormatDecoder *av    = dynamic_cast<AvFormatDecoder*>(dec);
    IvtvDecoder     *ivtv  = dynamic_cast<IvtvDecoder*>(dec);

    QString dec_class = (dummy) ? "dummy" :
        ((nuv) ? "nuppel" : ((av) ? "av" : ((ivtv) ? "ivtv": "null")));

    QString dec_type = "null";
    if (dec)
        dec_type = dec->GetCodecDecoderName();

    VERBOSE(VB_IMPORTANT, "SetDecoder("<<dec<<") "
            <<QString("%1:%2").arg(dec_class).arg(dec_type));
#endif
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

/** \fn NuppelVideoPlayer::LoadExternalSubtitles(const QString&)
 *  \brief Loads any external subtitles.
 *
 *  \return true if the subtitles were loaded successfully
 */
bool NuppelVideoPlayer::LoadExternalSubtitles(const QString &subtitleFileName)
{
    QMutexLocker locker(&subtitleLock);
    textSubtitles.Clear();
    return TextSubtitleParser::LoadSubtitles(subtitleFileName, textSubtitles);
}

void NuppelVideoPlayer::ChangeDVDTrack(bool ffw)
{
    if (!player_ctx->buffer->isDVD())
       return;

    need_change_dvd_track = (ffw ? 1 : -1);
}

void NuppelVideoPlayer::DoChangeDVDTrack(void)
{
    GetDecoder()->ChangeDVDTrack(need_change_dvd_track > 0);
    if (player_ctx->buffer->InDVDMenuOrStillFrame())
        ClearAfterSeek(false);
    else
        ClearAfterSeek(true);
}

void NuppelVideoPlayer::DisplayDVDButton(void)
{
    if (!player_ctx->buffer->isDVD() || !osd)
        return;

    VideoFrame *buffer = videoOutput->GetLastShownFrame();

    bool numbuttons = player_ctx->buffer->DVD()->NumMenuButtons();
    bool osdshown = osd->IsSetDisplaying("subtitles");

    if ((!numbuttons) ||
        (osdshown) ||
        (dvd_stillframe_showing && buffer->timecode > 0) ||
        ((!osdshown) &&
            (!pausevideo) &&
            (hidedvdbutton) &&
            (buffer->timecode > 0)))
    {
        return;
    }

    OSDSet *subtitleOSD = NULL;
    AVSubtitle *dvdSubtitle = player_ctx->buffer->DVD()->GetMenuSubtitle();
    AVSubtitleRect *hl_button = NULL;

    subtitleLock.lock();

    if (dvdSubtitle)
        subtitleOSD = osd->GetSet("subtitles");

    if (dvdSubtitle && subtitleOSD)
    {
        hl_button = dvdSubtitle->rects[0];
        osd->HideSet("subtitles");
        osd->ClearAll("subtitles");
        uint h = hl_button->h;
        uint w  = hl_button->w;
        int linesize = hl_button->pict.linesize[0];
        QPoint topleft = QPoint(hl_button->x, hl_button->y);
        QRect buttonPos = player_ctx->buffer->DVD()->GetButtonCoords();
        QImage hl_image(w, h, QImage::Format_ARGB32);
        uint8_t color;
        uint32_t pixel, *palette;
        QPoint currentPos = QPoint(0,0);
        for (uint y = 2; y < h; y++)
        {
            for (uint x = 0; x < w; x++)
            {
                currentPos.setY(y);
                currentPos.setX(x);
                color = hl_button->pict.data[0][(y)*linesize+(x)];
                // use rgba palette from dvd nav for drawing
                // highlighted button
                if (buttonPos.contains(currentPos))
                    palette = (uint32_t *)(dvdSubtitle->rects[1]->pict.data[1]);
                else
                    palette = (uint32_t *)(hl_button->pict.data[1]);
                pixel = palette[color];
                hl_image.setPixel(x, y, pixel);
            }
        }

        // scale highlight image to match OSD size, if required
        QRect button(topleft, QSize(w, h));
        QRect scaled = videoOutput->GetImageRect(button);
        if (scaled.size() != button.size())
        {
            hl_image = hl_image.scaled(scaled.width(), scaled.height(),
                       Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

        OSDTypeImage* image = new OSDTypeImage();
        image->SetPosition(scaled.topLeft(), 1.0f, 1.0f);
        image->Load(hl_image);
        image->SetDontRoundPosition(true);

        subtitleOSD->AddType(image);
        osd->SetVisible(subtitleOSD, 0);

        hidedvdbutton = false;
    }

    subtitleLock.unlock();

    player_ctx->buffer->DVD()->ReleaseMenuButton();
}

void NuppelVideoPlayer::ActivateDVDButton(void)
{
    if (!player_ctx->buffer->isDVD())
        return;

    player_ctx->buffer->DVD()->ActivateButton();
}

bool NuppelVideoPlayer::GoToDVDMenu(QString str)
{
    if (!player_ctx->buffer->isDVD())
        return false;

    textDisplayMode = kDisplayNone;
    bool ret = player_ctx->buffer->DVD()->GoToMenu(str);

    if (!ret)
    {
        if (osd)
            osd->SetSettingsText(QObject::tr("DVD Menu Not Available"), 1);
        VERBOSE(VB_GENERAL, "No DVD Menu available.");
        return false;
    }

    return true;
}

/** \fn NuppelVideoPlayer::GoToDVDProgram(bool direction)
 *  \brief Go forward or back by one DVD program
*/
void NuppelVideoPlayer::GoToDVDProgram(bool direction)
{
    if (!player_ctx->buffer->isDVD())
        return;

    if (direction == 0)
        player_ctx->buffer->DVD()->GoToPreviousProgram();
    else
        player_ctx->buffer->DVD()->GoToNextProgram();
}

long long NuppelVideoPlayer::GetDVDBookmark(void) const
{
    if (!player_ctx->buffer->isDVD())
        return 0;

    QStringList dvdbookmark = QStringList();
    QString name;
    QString serialid;
    long long frames = 0;
    bool delbookmark, jumptotitle;
    delbookmark = jumptotitle = player_ctx->buffer->DVD()->JumpToTitle();
    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
    {
        if (!player_ctx->buffer->DVD()->GetNameAndSerialNum(name, serialid))
        {
            player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
            return 0;
        }
        dvdbookmark = player_ctx->playingInfo->GetDVDBookmark(serialid,
                                                        !delbookmark);
        if (!dvdbookmark.empty())
        {
            QStringList::Iterator it = dvdbookmark.begin();
            int title = (*it).toInt();
            frames = (long long)((*++it).toLongLong() & 0xffffffffLL);
            if (jumptotitle)
            {
                player_ctx->buffer->DVD()->PlayTitleAndPart(title, 1);
                int audiotrack    = (*++it).toInt();
                int subtitletrack = (*++it).toInt();
                player_ctx->buffer->DVD()->SetTrack(
                    kTrackTypeAudio, audiotrack);
                player_ctx->buffer->DVD()->SetTrack(
                    kTrackTypeSubtitle, subtitletrack);
                player_ctx->buffer->DVD()->JumpToTitle(false);
            }
        }
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);

    return frames;
}

void NuppelVideoPlayer::SetDVDBookmark(long long frames)
{
    if (!player_ctx->buffer->isDVD())
        return;

    long long framenum = frames;
    QStringList fields;
    QString name;
    QString serialid;
    int title = 0;
    int part;
    int audiotrack = -1;
    int subtitletrack = -1;
    if (!player_ctx->buffer->DVD()->GetNameAndSerialNum(name, serialid))
        return;

    if (!player_ctx->buffer->InDVDMenuOrStillFrame() &&
            player_ctx->buffer->DVD()->GetTotalTimeOfTitle() > 120 &&
            frames > 0)
    {
        audiotrack = GetTrack(kTrackTypeAudio);
        if (GetCaptionMode() == kDisplayAVSubtitle)
        {
            subtitletrack = player_ctx->buffer->DVD()->GetTrack(
                kTrackTypeSubtitle);
        }
        player_ctx->buffer->DVD()->GetPartAndTitle(part, title);
    }
    else
        framenum = 0;

    player_ctx->LockPlayingInfo(__FILE__, __LINE__);
    if (player_ctx->playingInfo)
    {
        fields += serialid;
        fields += name;
        fields += QString("%1").arg(title);
        fields += QString("%1").arg(audiotrack);
        fields += QString("%1").arg(subtitletrack);
        fields += QString("%1").arg(framenum);
        player_ctx->playingInfo->SetDVDBookmark(fields);
    }
    player_ctx->UnlockPlayingInfo(__FILE__, __LINE__);
}

bool NuppelVideoPlayer::PosMapFromEnc(unsigned long long          start,
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

void NuppelVideoPlayer::SetErrored(const QString &reason) const
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

bool NuppelVideoPlayer::IsErrored(void) const
{
    QMutexLocker locker(&errorLock);
    return !errorMsg.isEmpty();
}

QString NuppelVideoPlayer::GetError(void) const
{
    QMutexLocker locker(&errorLock);
    QString tmp = errorMsg;
    tmp.detach();
    return tmp;
}

// EIA-708 caption support -- begin
void NuppelVideoPlayer::SetCurrentWindow(uint service_num, int window_id)
{
    if (!(textDisplayMode & kDisplayCC708))
        return;

    VERBOSE(VB_VBI, LOC + QString("SetCurrentWindow(%1, %2)")
            .arg(service_num).arg(window_id));
    CC708services[service_num].current_window = window_id;
}

void NuppelVideoPlayer::DefineWindow(
    uint service_num,     int window_id,
    int priority,         int visible,
    int anchor_point,     int relative_pos,
    int anchor_vertical,  int anchor_horizontal,
    int row_count,        int column_count,
    int row_lock,         int column_lock,
    int pen_style,        int window_style)
{
    if (decoder)
    {
        StreamInfo si(-1, 0, 0, service_num, false, false);
        decoder->InsertTrack(kTrackTypeCC708, si);
    }

    if (!(textDisplayMode & kDisplayCC708))
        return;

    {
        int i = window_id;
        {
#if 0
            if ((1<<i) & CC708DelayedDeletes[service_num&63])
            {
                QString txt;
                vector<CC708String*> list = win.GetStrings();
                for (uint j = 0; j < list.size(); j++)
                {
                    txt = list[j]->str + ((j+1!=list.size()) ? "\n" : "");
                    delete list[j];
                }
                VERBOSE(VB_VBI, LOC +
                        QString("DeleteWindow(%1, %2) text: '%3'")
                        .arg(service_num).arg(i).arg(txt));
            }
#endif
            CC708DelayedDeletes[service_num&63] &= ~(1<<i);
            if (GetOSD())
                GetOSD()->CC708Updated();
        }
    }

    VERBOSE(VB_VBI, LOC + QString("DefineWindow(%1, %2,\n\t\t\t\t\t")
            .arg(service_num).arg(window_id) +
            QString("  prio %1, vis %2, ap %3, rp %4, av %5, ah %6")
            .arg(priority).arg(visible).arg(anchor_point).arg(relative_pos)
            .arg(anchor_vertical).arg(anchor_horizontal) +
            QString("\n\t\t\t\t\t  row_cnt %1, row_lck %2, "
                    "col_cnt %3, col_lck %4 ")
            .arg(row_count).arg(row_lock)
            .arg(column_count).arg(column_lock) +
            QString("\n\t\t\t\t\t  pen style %1, win style %2)")
            .arg(pen_style).arg(window_style));

    GetCCWin(service_num, window_id)
        .DefineWindow(priority,         visible,
                      anchor_point,     relative_pos,
                      anchor_vertical,  anchor_horizontal,
                      row_count,        column_count,
                      row_lock,         column_lock,
                      pen_style,        window_style);

    CC708services[service_num].current_window = window_id;
}

void NuppelVideoPlayer::DeleteWindows(uint service_num, int window_map)
{
    VERBOSE(VB_VBI, LOC + QString("DeleteWindows(%1, 0x%2)")
            .arg(service_num).arg(window_map,0,16));
    for (uint i=0; i<8; i++)
        if ((1<<i) & window_map)
            GetCCWin(service_num, i).Clear();
    CC708DelayedDeletes[service_num&63] |= window_map;
}

void NuppelVideoPlayer::DisplayWindows(uint service_num, int window_map)
{
    if (!(textDisplayMode & kDisplayCC708))
        return;

    VERBOSE(VB_VBI, LOC + QString("DisplayWindows(%1, 0x%2)")
            .arg(service_num).arg(window_map,0,16));

    for (uint i=0; i<8; i++)
    {
        if ((1<<i) & CC708DelayedDeletes[service_num&63])
        {
            CC708Window &win = GetCCWin(service_num, i);

            if (0)
            {
                QString txt;
                vector<CC708String*> list = win.GetStrings();
                for (uint j = 0; j < list.size(); j++)
                {
                    txt = list[j]->str + ((j+1!=list.size()) ? "\n" : "");
                    delete list[j];
                }
                VERBOSE(VB_VBI, LOC +
                        QString("DeleteWindow(%1, %2) text: '%3'")
                        .arg(service_num).arg(i).arg(txt));
            }

            win.exists = false;
            if (win.text)
            {
                delete [] win.text;
                win.text = NULL;
            }
        }
        CC708DelayedDeletes[service_num&63] = 0;
    }

    for (uint i=0; i<8; i++)
    {
        if ((1<<i) & window_map)
        {
            GetCCWin(service_num, i).visible = true;
            {
                CC708Window &win = GetCCWin(service_num, i);
                QString txt;
                vector<CC708String*> list = win.GetStrings();
                for (uint j = 0; j < list.size(); j++)
                {
                    txt = list[j]->str + ((j+1!=list.size()) ? "\n" : "");
                    delete list[j];
                }
                VERBOSE(VB_VBI, LOC +
                        QString("DisplayedWindow(%1, %2) text: '%3'")
                        .arg(service_num).arg(i).arg(txt));
            }
        }
    }

    if (GetOSD())
        GetOSD()->CC708Updated();
}

void NuppelVideoPlayer::HideWindows(uint service_num, int window_map)
{
    if (!(textDisplayMode & kDisplayCC708))
        return;

    VERBOSE(VB_VBI, LOC + QString("HideWindows(%1, 0x%2)")
            .arg(service_num).arg(window_map,0,16));

    for (uint i=0; i<8; i++)
    {
        if ((1<<i) & window_map)
            GetCCWin(service_num, i).visible = false;
    }
}

void NuppelVideoPlayer::ClearWindows(uint service_num, int window_map)
{
    if (!(textDisplayMode & kDisplayCC708))
        return;

    VERBOSE(VB_VBI, LOC + QString("ClearWindows(%1, 0x%2)")
            .arg(service_num).arg(window_map,0,16));

    for (uint i=0; i<8; i++)
    {
        if ((1<<i) & window_map)
            GetCCWin(service_num, i).Clear();
    }
}

void NuppelVideoPlayer::ToggleWindows(uint service_num, int window_map)
{
    if (!(textDisplayMode & kDisplayCC708))
        return;

    VERBOSE(VB_VBI, LOC + QString("ToggleWindows(%1, 0x%2)")
            .arg(service_num).arg(window_map,0,16));

    for (uint i=0; i<8; i++)
    {
        if ((1<<i) & window_map)
        {
            GetCCWin(service_num, i).visible =
                !GetCCWin(service_num, i).visible;
        }
    }
}

void NuppelVideoPlayer::SetWindowAttributes(
    uint service_num,
    int fill_color,     int fill_opacity,
    int border_color,   int border_type,
    int scroll_dir,     int print_dir,
    int effect_dir,
    int display_effect, int effect_speed,
    int justify,        int word_wrap)
{
    if (!(textDisplayMode & kDisplayCC708))
        return;

    VERBOSE(VB_VBI, LOC + QString("SetWindowAttributes(%1...)")
            .arg(service_num));

    CC708Window &win = GetCCWin(service_num);

    win.fill_color     = fill_color   & 0x3f;
    win.fill_opacity   = fill_opacity;
    win.border_color   = border_color & 0x3f;
    win.border_type    = border_type;
    win.scroll_dir     = scroll_dir;
    win.print_dir      = print_dir;
    win.effect_dir     = effect_dir;
    win.display_effect = display_effect;
    win.effect_speed   = effect_speed;
    win.justify        = justify;
    win.word_wrap      = word_wrap;
}

void NuppelVideoPlayer::SetPenAttributes(
    uint service_num, int pen_size,
    int offset,       int text_tag,  int font_tag,
    int edge_type,    int underline, int italics)
{
    if (!(textDisplayMode & kDisplayCC708))
        return;

    VERBOSE(VB_VBI, LOC + QString("SetPenAttributes(%1, %2,")
            .arg(service_num).arg(CC708services[service_num].current_window) +
            QString("\n\t\t\t\t\t      pen_size %1, offset %2, text_tag %3, "
                    "font_tag %4,"
                    "\n\t\t\t\t\t      edge_type %5, underline %6, italics %7")
            .arg(pen_size).arg(offset).arg(text_tag).arg(font_tag)
            .arg(edge_type).arg(underline).arg(italics));

    GetCCWin(service_num).pen.SetAttributes(
        pen_size, offset, text_tag, font_tag, edge_type, underline, italics);
}

void NuppelVideoPlayer::SetPenColor(
    uint service_num,
    int fg_color, int fg_opacity,
    int bg_color, int bg_opacity,
    int edge_color)
{
    if (!(textDisplayMode & kDisplayCC708))
        return;

    VERBOSE(VB_VBI, LOC + QString("SetPenColor(%1...)")
            .arg(service_num));

    CC708CharacterAttribute &attr = GetCCWin(service_num).pen.attr;

    attr.fg_color   = fg_color;
    attr.fg_opacity = fg_opacity;
    attr.bg_color   = bg_color;
    attr.bg_opacity = bg_opacity;
    attr.edge_color = edge_color;
}

void NuppelVideoPlayer::SetPenLocation(uint service_num, int row, int column)
{
    if (!(textDisplayMode & kDisplayCC708))
        return;

    VERBOSE(VB_VBI, LOC + QString("SetPenLocation(%1, (c %2, r %3))")
            .arg(service_num).arg(column).arg(row));
    GetCCWin(service_num).SetPenLocation(row, column);
}

void NuppelVideoPlayer::Delay(uint service_num, int tenths_of_seconds)
{
    if (!(textDisplayMode & kDisplayCC708))
        return;

    VERBOSE(VB_VBI, LOC + QString("Delay(%1, %2 seconds)")
            .arg(service_num).arg(tenths_of_seconds * 0.1f));
}

void NuppelVideoPlayer::DelayCancel(uint service_num)
{
    if (!(textDisplayMode & kDisplayCC708))
        return;

    VERBOSE(VB_VBI, LOC + QString("DelayCancel(%1)").arg(service_num));
}

void NuppelVideoPlayer::Reset(uint service_num)
{
    if (!(textDisplayMode & kDisplayCC708))
        return;

    VERBOSE(VB_VBI, LOC + QString("Reset(%1)").arg(service_num));
    DeleteWindows(service_num, 0x7);
    DelayCancel(service_num);
}

void NuppelVideoPlayer::TextWrite(uint service_num,
                                  short* unicode_string, short len)
{
    if (!(textDisplayMode & kDisplayCC708))
        return;

    {
        CC708Window &win = GetCCWin(service_num);
        QMutexLocker locker(&win.lock);
        for (uint i = 0; i < (uint)len; i++)
            win.AddChar(QChar(unicode_string[i]));
    }

    if (GetOSD())
        GetOSD()->CC708Updated();
}

void NuppelVideoPlayer::SetOSDFontName(const QString osdfonts[22],
                                       const QString &prefix)
{
    osdfontname   = osdfonts[0]; osdfontname.detach();
    osdccfontname = osdfonts[1]; osdccfontname.detach();
    for (int i = 2; i < 22; i++)
    {
        QString tmp = osdfonts[i]; tmp.detach();
        osd708fontnames[i - 2] = tmp;
    }
    osdprefix = prefix; osdprefix.detach();
}

void NuppelVideoPlayer::SetOSDThemeName(const QString themename)
{
    osdtheme = themename; osdtheme.detach();
}

// EIA-708 caption support -- end

static unsigned dbg_ident(const NuppelVideoPlayer *nvp)
{
    static QMutex   dbg_lock;
    static unsigned dbg_next_ident = 0;
    typedef QMap<const NuppelVideoPlayer*, unsigned> DbgMapType;
    static DbgMapType dbg_ident;

    QMutexLocker locker(&dbg_lock);
    DbgMapType::iterator it = dbg_ident.find(nvp);
    if (it != dbg_ident.end())
        return *it;
    return dbg_ident[nvp] = dbg_next_ident++;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
