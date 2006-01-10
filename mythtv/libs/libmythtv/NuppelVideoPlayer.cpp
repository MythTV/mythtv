#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#include <cerrno>
#include <sys/time.h>
#include <ctime>
#include <cmath>
#include <qapplication.h>
#include <qstringlist.h>
#include <qmap.h>
#include <sched.h>

#include <sys/ioctl.h>

#include <algorithm>
#include <iostream>
using namespace std;

#include "config.h"
#include "mythdbcon.h"
#include "dialogbox.h"
#include "NuppelVideoPlayer.h"
#include "audiooutput.h"
#include "recordingprofile.h"
#include "osdtypes.h"
#include "osdsurface.h"
#include "remoteutil.h"
#include "programinfo.h"
#include "mythcontext.h"
#include "fifowriter.h"
#include "filtermanager.h"
#include "util.h"
#include "livetvchain.h"
#include "decoderbase.h"
#include "nuppeldecoder.h"
#include "avformatdecoder.h"
#include "jobqueue.h"

#include "NuppelVideoRecorder.h"
#include "tv_play.h"

extern "C" {
#include "vbitext/vbi.h"
#include "vsync.h"
}

#include "remoteencoder.h"

#include "videoout_null.h"

#ifdef USING_IVTV
#include "videoout_ivtv.h"
#include "ivtvdecoder.h"
#endif

#ifdef USING_DIRECTX
#include "videoout_dx.h"
#undef GetFreeSpace
#undef GetFileSize
#endif

#ifndef HAVE_ROUND
#define round(x) ((int) ((x) + 0.5))
#endif

#ifdef CONFIG_DARWIN
extern "C" {
int isnan(double);
}
#endif

#define FAST_RESTART 0
#define LOC QString("NVP: ")
#define LOC_ERR QString("NVP, Error: ")

NuppelVideoPlayer::NuppelVideoPlayer(QString inUseID, const ProgramInfo *info)
    : forceVideoOutput(kVideoOutput_Default),
      decoder(NULL), videoOutput(NULL), nvr_enc(NULL), m_playbackinfo(NULL),
      // Window stuff
      parentWidget(NULL), embedid(0), embx(-1), emby(-1), embw(-1), embh(-1),
      // State
      eof(false),                   m_double_framerate(false),
      m_can_double(false),          paused(false),
      pausevideo(false),            actuallypaused(false),
      video_actually_paused(false), playing(false),
      decoder_thread_alive(true),   killplayer(false),
      killvideo(false),             livetv(false),
      watchingrecording(false),     editmode(false),
      resetvideo(false),            using_null_videoout(false),
      no_audio_in(false),           no_audio_out(false),
      transcoding(false),
      hasFullPositionMap(false),    limitKeyRepeat(false),
      errored(false),
      m_DeintSetting(0),
      // Bookmark stuff
      bookmarkseek(0),              previewFromBookmark(false),
      // Seek
      fftime(0),                    seekamountpos(4),
      seekamount(30),               exactseeks(false),
      // Playback misc.
      videobuf_retries(0),          framesPlayed(0),
      totalFrames(0),               totalLength(0),
      rewindtime(0),                m_recusage(inUseID),
      // Input Video Attributes
      video_width(0), video_height(0), video_size(0),
      video_frame_rate(29.97f), video_aspect(4.0f / 3.0f),
      m_scan(kScan_Detect), keyframedist(30),
      // RingBuffer stuff
      filename("output.nuv"), weMadeBuffer(false), ringBuffer(NULL),
      // Prebuffering (RingBuffer) control
      prebuffering(false), prebuffer_tries(0),
      // Support for analog captions and teletext
      vbimode(VBIMode::None),       subtitlesOn(false),
      ttPageNum(0x888),             ccmode(CC_CC1),
      wtxt(0), rtxt(0), text_size(0), ccline(""), cccol(0), ccrow(0),
      // Support for captions, teletext, etc. decoded by libav
      osdHasSubtitles(false),       osdSubtitlesExpireAt(-1),
      // OSD stuff
      osd(NULL),                    timedisplay(NULL),
      dialogname(""),               dialogtype(0),
      // Audio stuff
      audioOutput(NULL),            audiodevice("/dev/dsp"),
      audio_channels(2),            audio_bits(-1),
      audio_samplerate(44100),      audio_stretchfactor(1.0f),
      // Picture-in-Picture
      pipplayer(NULL), setpipplayer(NULL), needsetpipplayer(false),
      // Filters
      videoFilterList(""),
      postfilt_width(0),            postfilt_height(0),
      videoFilters(NULL),           FiltMan(new FilterManager()),
      // Commercial filtering
      skipcommercials(0),           autocommercialskip(0),
      commercialskipmethod(0),      commrewindamount(0),
      commnotifyamount(0),          lastCommSkipDirection(0),
      lastCommSkipTime(0/*1970*/),  lastCommSkipStart(0),
      lastSkipTime(0 /*1970*/),
      deleteframe(0),
      hasdeletetable(false),        hasblanktable(false),
      hascommbreaktable(false),
      deleteIter(deleteMap.end()),  blankIter(blankMap.end()),
      commBreakIter(commBreakMap.end()),
      // Playback (output) speed control
      decoder_lock(true),
      next_play_speed(1.0f),        next_normal_speed(true),
      play_speed(1.0f),             normal_speed(true),
      frame_interval(30),           ffrew_skip(1),
      // Audio and video synchronization stuff
      videosync(NULL),              delay(0),
      vsynctol(30/4),               avsync_delay(0),
      avsync_adjustment(0),         avsync_avg(0),
      avsync_oldavg(0),             refreshrate(0),
      lastsync(false),              m_playing_slower(false),
      m_stored_audio_stretchfactor(1.0),
      // Audio warping stuff
      usevideotimebase(false), 
      warpfactor(1.0f),             warpfactor_avg(1.0f),
      warplbuff(NULL),              warprbuff(NULL),
      warpbuffsize(0),
      // Time Code stuff
      prevtc(0),
      tc_avcheck_framecounter(0),   tc_diff_estimate(0),
      // LiveTVChain stuff
      livetvchain(NULL), m_tv(NULL),
      // Debugging variables
      output_jmeter(NULL)
{
    if (info)
    {
        m_playbackinfo = new ProgramInfo(*info);
        m_playbackinfo->MarkAsInUse(true, m_recusage);
    }

    commrewindamount = gContext->GetNumSetting("CommRewindAmount",0);
    commnotifyamount = gContext->GetNumSetting("CommNotifyAmount",0);
    m_DeintSetting   = gContext->GetNumSetting("Deinterlace", 0);

    bzero(&txtbuffers, sizeof(txtbuffers));
    bzero(&tc_lastval, sizeof(tc_lastval));
    bzero(&tc_wrap,    sizeof(tc_wrap));

    // Get VBI page number
    QString mypage = gContext->GetSetting("VBIpageNr", "888");
    bool valid = false;
    uint tmp = mypage.toInt(&valid, 16);
    ttPageNum = (valid) ? tmp : ttPageNum;
}

NuppelVideoPlayer::~NuppelVideoPlayer(void)
{
    if (audioOutput)
        delete audioOutput;

    if (m_playbackinfo)
    {
        m_playbackinfo->MarkAsInUse(false);
        delete m_playbackinfo;
    }

    if (weMadeBuffer)
        delete ringBuffer;

    if (osdHasSubtitles || nonDisplayedSubtitles.size() > 0)
        ClearSubtitles();

    if (osd)
        delete osd;
    
    for (int i = 0; i < MAXTBUFFER; i++)
    {
        if (txtbuffers[i].buffer)
            delete [] txtbuffers[i].buffer;
    }

    SetDecoder(NULL);

    if (FiltMan)
        delete FiltMan;

    if (videoFilters)
        delete videoFilters;

    if (videosync)
        delete videosync;

    if (videoOutput)
        delete videoOutput;
}

void NuppelVideoPlayer::SetWatchingRecording(bool mode)
{
    watchingrecording = mode;
    if (GetDecoder())
        GetDecoder()->setWatchingRecording(mode);
}

void NuppelVideoPlayer::SetRecorder(RemoteEncoder *recorder)
{
    nvr_enc = recorder;
    if (GetDecoder())
        GetDecoder()->setRecorder(recorder);
}

void NuppelVideoPlayer::PauseDecoder(void)
{
    decoder_lock.lock();
    next_play_speed = 0.0;
    next_normal_speed = false;
    decoder_lock.unlock();

    if (!actuallypaused)
    {
        while (!decoderThreadPaused.wait(4000))
        {
            if (eof)
                return;
            VERBOSE(VB_IMPORTANT, "Waited too long for decoder to pause");
        }
    }
}

void NuppelVideoPlayer::Pause(bool waitvideo)
{
    PauseDecoder();

    //cout << "stopping other threads" << endl;
    PauseVideo(waitvideo);
    if (audioOutput)
        audioOutput->Pause(true);
    if (ringBuffer)
        ringBuffer->Pause();

    if (GetDecoder() && videoOutput)
    {
        //cout << "updating frames played" << endl;
        if (using_null_videoout || forceVideoOutput == kVideoOutput_IVTV)
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

    UnpauseVideo();
    if (audioOutput && unpauseaudio)
        audioOutput->Pause(false);
    if (ringBuffer)
        ringBuffer->Unpause();

    decoder_lock.lock();
    next_play_speed = speed;
    next_normal_speed = normal;
    decoder_lock.unlock();
    return true;
}

bool NuppelVideoPlayer::GetPause(void) const
{
    return (actuallypaused &&
            (ringBuffer == NULL || ringBuffer->isPaused()) &&
            (audioOutput == NULL || audioOutput->GetPause()) &&
            GetVideoPause());
}

void NuppelVideoPlayer::PauseVideo(bool wait)
{
    video_actually_paused = false;
    pausevideo = true;

    if (wait && !video_actually_paused)
    {
        while (!videoThreadPaused.wait(1000))
        {
            if (eof)
                return;
            VERBOSE(VB_IMPORTANT, "Waited too long for video out to pause");
        }
    }
}

void NuppelVideoPlayer::UnpauseVideo(void)
{
    pausevideo = false;
}

void NuppelVideoPlayer::SetPrebuffering(bool prebuffer)
{
    prebuffering_lock.lock();

    if (prebuffer != prebuffering)
    {
        prebuffering = prebuffer;
        if (audioOutput && !paused)
            audioOutput->Pause(prebuffering);
    }

    if (!prebuffering)
        prebuffering_wait.wakeAll();

    prebuffering_lock.unlock();
}

void NuppelVideoPlayer::ForceVideoOutputType(VideoOutputType type)
{
    forceVideoOutput = type;
}

bool NuppelVideoPlayer::InitVideo(void)
{
    InitFilters();
    if (using_null_videoout)
    {
        videoOutput = new VideoOutputNull();
        if (!videoOutput->Init(video_width, video_height, video_aspect,
                               0, 0, 0, 0, 0, 0))
        {
            errored = true;
            return false;
        }
    }
    else
    {
        QWidget *widget = parentWidget;

        if (!widget)
        {
            MythMainWindow *window = gContext->GetMainWindow();
            assert(window);

            QObject *playbackwin = window->child("video playback window");

            QWidget *widget = (QWidget *)playbackwin;

            if (!widget)
            {
                VERBOSE(VB_IMPORTANT, "Couldn't find 'tv playback' widget");
                widget = window->currentWidget();
                assert(widget);
            }
        }

        if (!widget)
        {
            errored = true;
            return false;
        }

        videoOutput = VideoOutput::InitVideoOut(forceVideoOutput,
                                                GetDecoder()->GetVideoCodecID());

        if (!videoOutput)
        {
            errored = true;
            return false;
        }

        if (gContext->GetNumSetting("DecodeExtraAudio", 0))
            GetDecoder()->SetLowBuffers(true);

        if (!videoOutput->Init(video_width, video_height, video_aspect,
                               widget->winId(), 0, 0, widget->width(),
                               widget->height(), 0))
        {
            errored = true;
            return false;
        }
    }

    if (embedid > 0)
    {
        videoOutput->EmbedInWidget(embedid, embx, emby, embw, embh);
    }

    return true;
}

void NuppelVideoPlayer::ReinitOSD(void)
{
    if (osd)
    {
        QRect visible, total;
        float aspect, scaling;
        videoOutput->GetOSDBounds(total, visible, aspect, scaling);
        osd->Reinit(total, frame_interval, visible, aspect, scaling);
    }
}

void NuppelVideoPlayer::ReinitVideo(void)
{
    InitFilters();

    videofiltersLock.lock();
    videoOutput->InputChanged(video_width, video_height, video_aspect);
    if (videoOutput->IsErrored())
    {
        VERBOSE(VB_IMPORTANT, "ReinitVideo(): videoOutput->IsErrored()");
        qApp->lock();
        DialogBox dialog(gContext->GetMainWindow(),
                         QObject::tr("Failed to Reinit Video."));
        dialog.AddButton(QObject::tr("Return to menu."));
        dialog.exec();
        qApp->unlock();
        errored = true;
    }
    else
    {
        ReinitOSD();
    }

    videofiltersLock.unlock();

    ClearAfterSeek();
}

QString NuppelVideoPlayer::ReinitAudio(void)
{
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

    if (!audioOutput && !using_null_videoout)
    {
        bool setVolume = gContext->GetNumSetting("MythControlsVolume", 1);
        audioOutput = AudioOutput::OpenAudio(audiodevice, audio_bits,
                                             audio_channels, audio_samplerate, 
                                             AUDIOOUTPUT_VIDEO,
                                             setVolume);
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
    }

    if (audioOutput)
    {
        audioOutput->Reconfigure(audio_bits, audio_channels, audio_samplerate);
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
 *  \brief Fallback to non-bob a deinterlacing method.
 */
void NuppelVideoPlayer::FallbackDeint(void)
{
     m_double_framerate = false;

     if (videosync)
         videosync->SetFrameInterval(frame_interval, false);

     if (videoOutput)
     {
         videoOutput->SetupDeinterlace(false);
         videoOutput->SetupDeinterlace(true, "onefield");
     }
}

void NuppelVideoPlayer::SetVideoParams(int width, int height, double fps,
                                       int keyframedistance, float aspect,
                                       FrameScanType scan, bool reinit)
{
    if (width == 0 || height == 0 || isnan(aspect) || isnan(fps))
        return;

    if ((video_width  == width ) && (video_height     == height) && 
        (video_aspect == aspect) && (video_frame_rate == fps   ) &&
        ((keyframedistance <= 0) || (keyframedistance == keyframedist)))
    {
        return;
    }

    video_width  = (width  > 0) ? width  : video_width;
    video_height = (height > 0) ? height : video_height;
    video_size   = (video_height * video_width * 3) / 2;
    video_aspect = (aspect > 0.0f) ? aspect : video_aspect;
    keyframedist = (keyframedistance > 0) ? keyframedistance : keyframedist;

    if (fps > 0.0f && fps < 121.0f)
    {
        video_frame_rate = fps;
        float temp_speed = (play_speed == 0.0f) ?
            audio_stretchfactor : play_speed;
        frame_interval = (int)(1000000.0f / video_frame_rate / temp_speed);
    }

    if (reinit)
        ReinitVideo();

    if (IsErrored())
        return;

    videofiltersLock.lock();

    m_scan = detectInterlace(scan, m_scan, video_frame_rate, video_height);
    VERBOSE(VB_PLAYBACK, QString("Interlaced: %1  video_height: %2  fps: %3")
            .arg(toQString(m_scan)).arg(video_height)
            .arg(fps));

    // Set up deinterlacing in the video output method
    m_double_framerate = false;
    if (videoOutput)
    {
        videoOutput->SetupDeinterlace(false);
        if ((m_scan == kScan_Interlaced)        &&
            m_DeintSetting                      &&
            videoOutput->SetupDeinterlace(true) &&
            videoOutput->NeedsDoubleFramerate())
        {
            m_double_framerate = true;
            m_can_double = true;
        }
    }

    // Make sure video sync can double frame rate
    if (videosync && m_double_framerate)
    {
        videosync->SetFrameInterval(frame_interval, m_double_framerate);
        if (videosync->UsesFrameInterval())
        {
            VERBOSE(VB_IMPORTANT, "Video sync method can't support double "
                    "framerate (refresh rate too low for bob deint)");
            FallbackDeint();
        }
    }

    videofiltersLock.unlock();
}

void NuppelVideoPlayer::SetFileLength(int total, int frames)
{
    totalLength = total;
    totalFrames = frames;
}

int NuppelVideoPlayer::OpenFile(bool skipDsp, uint retries,
                                bool allow_libmpeg2)
{
    if (!skipDsp)
    {
        if (!ringBuffer)
        {
            QString msg("");

            if (m_playbackinfo)
            {
                msg = QString("\n\t\t\tm_playbackinfo filename is %1")
                    .arg(m_playbackinfo->GetRecordBasename());
            }

            VERBOSE(VB_IMPORTANT, LOC + "OpenFile() Warning, "
                    "old player exited before new ring buffer created. " + 
                    QString("\n\t\t\tRingBuffer will use filename '%1'.")
                    .arg(filename) + msg);

            ringBuffer = new RingBuffer(filename, false, true, retries);
            weMadeBuffer = true;
            livetv = false;
        }
        else
            livetv = ringBuffer->LiveMode();

        if (!ringBuffer->IsOpen())
        {
            VERBOSE(VB_IMPORTANT,
                    QString("NVP::OpenFile(): Error, file not found: %1")
                    .arg(ringBuffer->GetFilename().ascii()));
            return -1;
        }
    }

    ringBuffer->Start();
    char testbuf[2048];
    if (ringBuffer->Read(testbuf, 2048) != 2048)
    {
        VERBOSE(VB_IMPORTANT,
                QString("NVP::OpenFile(): Error, couldn't read file: %1")
                .arg(ringBuffer->GetFilename()));
        return -1;
    }

    // Seek back to begining so that OpenFile can find PAT/PMT more quickly
    ringBuffer->Pause();
    ringBuffer->WaitForPause();
    ringBuffer->Seek(0, SEEK_SET);
    ringBuffer->Unpause();

    // delete any pre-existing recorder
    SetDecoder(NULL);

    if (NuppelDecoder::CanHandle(testbuf))
        SetDecoder(new NuppelDecoder(this, m_playbackinfo));
#ifdef USING_IVTV
    else if (!using_null_videoout && IvtvDecoder::CanHandle(
                 testbuf, ringBuffer->GetFilename()))
    {
        SetDecoder(new IvtvDecoder(this, m_playbackinfo));
        no_audio_out = true; // no audio with ivtv.
        audio_bits = 16;
    }
    else if (forceVideoOutput == kVideoOutput_IVTV)
    {
        VERBOSE(VB_IMPORTANT,
                QString("NVP: Couldn't open '%1' with ivtv decoder")
                .arg(ringBuffer->GetFilename()));
        return -1;
    }
#endif
    else if (AvFormatDecoder::CanHandle(testbuf, ringBuffer->GetFilename()))
        SetDecoder(new AvFormatDecoder(this, m_playbackinfo,
                                       using_null_videoout, allow_libmpeg2));

    if (!GetDecoder())
    {
        VERBOSE(VB_IMPORTANT, 
                QString("NVP: Couldn't find a matching decoder for: %1").
                arg(ringBuffer->GetFilename()));
        return -1;
    } 
    else if (GetDecoder()->IsErrored())
    {
        VERBOSE(VB_IMPORTANT, 
                "NVP: NuppelDecoder encountered error during creation.");
        SetDecoder(NULL);
        return -1;
    }

    GetDecoder()->setExactSeeks(exactseeks);
    GetDecoder()->setLiveTVMode(livetv);
    GetDecoder()->setRecorder(nvr_enc);
    GetDecoder()->setWatchingRecording(watchingrecording);
    GetDecoder()->setTranscoding(transcoding);

    eof = false;
    text_size = 8 * (sizeof(teletextsubtitle) + VT_WIDTH);

    int ret;
    // We still want to locate decoder for video even if using_null_videoout is true
    bool disable_video_decoding = false; // set to true for audio only decodeing
    if ((ret = GetDecoder()->OpenFile(ringBuffer, disable_video_decoding, testbuf)) < 0)
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't open decoder for: %1")
                .arg(ringBuffer->GetFilename()));
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
    bookmarkseek = GetBookmark();

    return 0;
}

void NuppelVideoPlayer::InitFilters(void)
{
    VideoFrameType itmp = FMT_YV12;
    VideoFrameType otmp = FMT_YV12;
    int btmp;

    videofiltersLock.lock();

    if (videoFilters)
        delete videoFilters;

    postfilt_width = video_width;
    postfilt_height = video_height;
    videoFilters = FiltMan->LoadFilters(videoFilterList, itmp, otmp,
                                        postfilt_width, postfilt_height, btmp);

    videofiltersLock.unlock();
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
    WrapTimecode(timecode, TC_VIDEO);
    buffer->timecode = timecode;

    videoOutput->ReleaseFrame(buffer);
}

/** \fn NuppelVideoPlayer::DiscardVideoFrame(VideoFrame*)
 *  \brief Places frame in the available frames queue.
 */
void NuppelVideoPlayer::DiscardVideoFrame(VideoFrame *buffer)
{
    if (videoOutput)
        videoOutput->DiscardFrame(buffer);
}

/** \fn DiscardVideoFrames(bool)
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

void NuppelVideoPlayer::AddTextData(char *buffer, int len,
                                    long long timecode, char type)
{
    WrapTimecode(timecode, TC_CC);

    if (subtitlesOn)
    {
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
}

void NuppelVideoPlayer::CheckPrebuffering(void)
{
    if (kVideoOutput_IVTV == forceVideoOutput)
        return;

#if FAST_RESTART
    if (videoOutput->EnoughPrebufferedFrames())
        SetPrebuffering(false);
#else
    if (videoOutput->EnoughDecodedFrames())
        SetPrebuffering(false);
#endif
}

bool NuppelVideoPlayer::GetFrameNormal(int onlyvideo)
{
    if (!GetDecoder()->GetFrame(onlyvideo))
        return false;

    CheckPrebuffering();

    if ((play_speed > 1.01f) && (audio_stretchfactor > 1.01f) && IsNearEnd())
    {
        VERBOSE(VB_PLAYBACK, LOC + "Near end, Slowing down playback.");
        Play(1.0f, true, true);
    }

    return true;
}

bool NuppelVideoPlayer::GetFrameFFREW(void)
{
    bool stopFFREW = false;

    if (ffrew_skip > 0)
    {
        long long delta = GetDecoder()->GetFramesRead() - framesPlayed;
        long long real_skip = CalcMaxFFTime(ffrew_skip + delta) - delta;
        if (real_skip >= 0)
        {
            long long frame = GetDecoder()->GetFramesRead() + real_skip;
            GetDecoder()->DoFastForward(frame, false);
            stopFFREW = (CalcMaxFFTime(100, false) < 100);
        }
    }
    else if (CalcRWTime(-ffrew_skip) >= 0)
    {
        long long curFrame  = GetDecoder()->GetFramesRead();
        bool      toBegin   = -curFrame > ffrew_skip;
        long long real_skip = (toBegin) ? -curFrame : ffrew_skip;
        GetDecoder()->DoRewind(curFrame + real_skip, false);
        stopFFREW = framesPlayed <= keyframedist;
    }

    if (stopFFREW)
    {
        float stretch = (ffrew_skip > 0) ? 1.0f : audio_stretchfactor;
        Play(stretch, true, true);
    }

    bool ret = GetDecoder()->GetFrame(1);
    CheckPrebuffering();
    return ret;
}

bool NuppelVideoPlayer::GetFrame(int onlyvideo, bool unsafe)
{
    bool ret = false;

    // Wait for frames to be available for decoding onto
    if ((forceVideoOutput != kVideoOutput_IVTV) &&
        !videoOutput->EnoughFreeFrames()        &&
        !unsafe)
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
        ret = GetFrameNormal(onlyvideo);
    else
        ret = GetFrameFFREW(); 

    return ret;
}

VideoFrame *NuppelVideoPlayer::GetCurrentFrame(int &w, int &h)
{
    w = video_width;
    h = video_height;

    VideoFrame *retval = NULL;

    vidExitLock.lock();
    if (videoOutput)
        retval = videoOutput->GetLastShownFrame();

    if (!retval)
        vidExitLock.unlock();

    return retval;
}

void NuppelVideoPlayer::ReleaseCurrentFrame(VideoFrame *frame)
{
    if (frame)
        vidExitLock.unlock();
}

void NuppelVideoPlayer::EmbedInWidget(WId wid, int x, int y, int w, int h)
{
    if (videoOutput)
    {
        videoOutput->EmbedInWidget(wid, x, y, w, h);
    }
    else
    {
        embedid = wid;
        embx = x;
        emby = y;
        embw = w;
        embh = h;
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

/** \fn ToggleCC(uint, uint)
 *  \brief Sets vbimode. If arg is zero, toggles subtitlesOn,
 *         otherwise it turns subtitles on and sets a specific
 *         teletext page or cc text.
 *
 *  \param mode VBIMode expressing which VBI decoder to use.
 *  \param arg  if 0 toggles cc/teletext mode, otherwise specifies
 *              teletext page or cc text to use.
 */
void NuppelVideoPlayer::ToggleCC(uint mode, uint arg)
{
    QString msg;

    vbimode = mode;
    if (subtitlesOn && !arg)
    {
        subtitlesOn = false;
        ResetCC();
        bool tt = (vbimode == VBIMode::PAL_TT);
        msg = tt ? QObject::tr("TXT off") : QObject::tr("CC off");
        if (osd)
            osd->SetSettingsText(msg, 3);
        return;
    }

    subtitlesOn = true;
    if (vbimode == VBIMode::PAL_TT)
    {
        ttPageNum = (arg) ? arg : ttPageNum;
        msg = QObject::tr("TXT") + QString(" %1").arg(ttPageNum,3,16);
    }
    else if (vbimode == VBIMode::NTSC_CC)
    {
        static const uint lk[8] =
        {
            CC_CC1,  CC_CC2,  CC_CC3,  CC_CC4,
            CC_TXT1, CC_TXT2, CC_TXT3, CC_TXT4,
        };
        if ((1 <= arg) && (arg <= 8))
            ccmode = lk[arg-1];

        ResetCC();

        switch (ccmode)
        {
            case CC_CC2 :
                msg = QString("%1%2").arg(QObject::tr("CC")).arg(2);
                break;
            case CC_CC3 :
                msg = QString("%1%2").arg(QObject::tr("CC")).arg(3);
                break;
            case CC_CC4 :
                msg = QString("%1%2").arg(QObject::tr("CC")).arg(4);
                break;
            case CC_TXT1:
                msg = QString("%1%2").arg(QObject::tr("TXT")).arg(1);
                break;
            case CC_TXT2:
                msg = QString("%1%2").arg(QObject::tr("TXT")).arg(2);
                break;
            case CC_TXT3:
                msg = QString("%1%2").arg(QObject::tr("TXT")).arg(3);
                break;
            case CC_TXT4:
                msg = QString("%1%2").arg(QObject::tr("TXT")).arg(4);
                break;
            default     :
                msg = QString("%1%2").arg(QObject::tr("CC")).arg(1);
                break;
        }
    }
    else
        msg = QString(QObject::tr("CC/TXT enabled"));

    if (osd)
        osd->SetSettingsText(msg, 3);
}

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

void NuppelVideoPlayer::ResetCC(void)
{
    ccline = "";
    cccol = 0;
    ccrow = 0;
    if (osd)
        osd->ClearAllCCText();
}

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
                    printf("CC overflow:  ");
                    printf("%d %d %s\n", cccol, ccrow, ccline.ascii());
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
/// How much dwe multiply the warp by when storing it in an integer.
#define WARPMULTIPLIER 1000000000
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

    //cerr << "Divergence: " << divergence << "  Rate: " << rate
    //<< "  Warpfactor: " << warpfactor << "  warpfactor_avg: "
    //<< warpfactor_avg << endl;
    return divergence;
}

void NuppelVideoPlayer::InitAVSync(void)
{
    videosync->Start();

    avsync_adjustment = 0;

    if (usevideotimebase)
    {
        warpfactor_avg = gContext->GetNumSetting("WarpFactor", 0);
        if (warpfactor_avg)
            warpfactor_avg /= WARPMULTIPLIER;
        else
            warpfactor_avg = 1;
        // Reset the warpfactor if it's obviously bogus
        if (warpfactor_avg < (1 - WARPCLIP))
            warpfactor_avg = 1;
        if (warpfactor_avg > (1 + (WARPCLIP * 2)) )
            warpfactor_avg = 1;

        warpfactor = warpfactor_avg;
    }

    refreshrate = videoOutput->GetRefreshRate();
    if (refreshrate <= 0)
        refreshrate = frame_interval;
    vsynctol = refreshrate / 4;

    if (!using_null_videoout)
    {
        if (usevideotimebase)
            VERBOSE(VB_PLAYBACK, "Using video as timebase");
        else
            VERBOSE(VB_PLAYBACK, "Using audio as timebase");

        QString timing_type = videosync->getName();

        QString msg = QString("Video timing method: %1").arg(timing_type);
        VERBOSE(VB_GENERAL, msg);
        msg = QString("Refresh rate: %1, frame interval: %2")
                       .arg(refreshrate).arg(frame_interval);
        VERBOSE(VB_PLAYBACK, msg);
        nice(-19);
    }
}

void NuppelVideoPlayer::AVSync(void)
{
    float diverge = 0.0f;

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
        errored = true;
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

    FrameScanType ps = (m_double_framerate) ? kScan_Interlaced : kScan_Ignore;
    if (diverge < -MAXDIVERGE)
    {
        // If video is way ahead of audio, adjust for it...
        QString dbg = QString("Video is %1 frames ahead of audio, ")
            .arg(-diverge);

        // Reset A/V Sync
        lastsync = true;
 
        if (buffer && !using_null_videoout &&
            (videoOutput->hasMCAcceleration()   ||
             videoOutput->hasIDCTAcceleration() ||
             videoOutput->hasVLDAcceleration()))
        {
            // If we are using hardware decoding, so we've already done the
            // decoding; display the frame, but don't wait for A/V Sync.
            videoOutput->PrepareFrame(buffer, ps);
            videoOutput->Show(m_scan);
            VERBOSE(VB_PLAYBACK, LOC + dbg + "skipping A/V wait.");
        }
        else
        {
            // If we are using software decoding, skip this frame altogether.
            VERBOSE(VB_PLAYBACK, LOC + dbg + "dropping frame.");
        }
    }
    else if (!using_null_videoout)
    {
        // if we get here, we're actually going to do video output
        if (buffer)
            videoOutput->PrepareFrame(buffer, ps);

        videosync->WaitForFrame(avsync_adjustment);
        if (!resetvideo)
            videoOutput->Show(m_scan);

        if (videoOutput->IsErrored())
        {
            VERBOSE(VB_IMPORTANT, "NVP: Error condition detected "
                    "in videoOutput after Show(), aborting playback.");
            errored = true;
            return;
        }

        if (m_double_framerate)
        {
            // XvMC does not need the frame again
            if (!videoOutput->hasMCAcceleration() && buffer)
                videoOutput->PrepareFrame(buffer, kScan_Intr2ndField);

            // Display the second field
            videosync->AdvanceTrigger();
            videosync->WaitForFrame(0);
            if (!resetvideo)
                videoOutput->Show(kScan_Intr2ndField);
        }
    }
    else
    {
        videosync->WaitForFrame(0);
    }

    if (output_jmeter && output_jmeter->RecordCycleTime())
    {
        //cerr << "avsync_delay: " << avsync_delay / 1000
        //     << ", avsync_avg: " << avsync_avg / 1000
        //     << ", warpfactor: " << warpfactor
        //     << ", warpfactor_avg: " << warpfactor_avg << endl;
    }

    videosync->AdvanceTrigger();
    avsync_adjustment = 0;

    if (diverge > MAXDIVERGE)
    {
        // If audio is way ahead of video, adjust for it...
        // by cutting the frame rate in half for the length of this frame

        avsync_adjustment = frame_interval;
        lastsync = true;
        VERBOSE(VB_PLAYBACK, LOC + 
                QString("Audio is %1 frames ahead of video,\n"
                        "\t\t\tdoubling video frame interval.").arg(diverge));
    }

    if (audioOutput && normal_speed)
    {
        long long currentaudiotime = audioOutput->GetAudiotime();
#if 0
        VERBOSE(VB_PLAYBACK, QString(
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
                delta < (int) frame_interval / 1000 * 3)
            {
                //cerr << "+ ";
                videosync->AdvanceTrigger();
                if (m_double_framerate)
                    videosync->AdvanceTrigger();
            }

            avsync_delay = (buffer->timecode - currentaudiotime) * 1000;//usec
            avsync_avg = (avsync_delay + (avsync_avg * 3)) / 4;
            if (!usevideotimebase)
            {
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
        }
        else
        {
            avsync_avg = 0;
            avsync_oldavg = 0;
        }
    }
}

void NuppelVideoPlayer::ShutdownAVSync(void)
{
    if (usevideotimebase)
    {
        gContext->SaveSetting("WarpFactor",
            (int)(warpfactor_avg * WARPMULTIPLIER));

        if (warplbuff)
        {
            free(warplbuff);
            warplbuff = NULL;
        }

        if (warprbuff)
        {
            free(warprbuff);
            warprbuff = NULL;
        }
        warpbuffsize = 0;
    }
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

    video_actually_paused = true;
    videoThreadPaused.wakeAll();

    if (videoOutput->IsErrored())
    {
        errored = true;
        return;
    }

    videofiltersLock.lock();
    videoOutput->ProcessFrame(NULL, osd, videoFilters, pipplayer);
    videofiltersLock.unlock();

    videoOutput->PrepareFrame(NULL, kScan_Ignore);
    videoOutput->Show(kScan_Ignore);
    videosync->Start();
}

void NuppelVideoPlayer::DisplayNormalFrame(void)
{
    video_actually_paused = false;
    resetvideo = false;

    prebuffering_lock.lock();
    if (prebuffering)
    {
        VERBOSE(VB_PLAYBACK, LOC + QString("Waiting for prebuffer.. %1 %2")
                .arg(prebuffer_tries).arg(videoOutput->GetFrameStatus()));
        if (!prebuffering_wait.wait(&prebuffering_lock,
                                    frame_interval * 4 / 1000))
        {
            // timed out.. do we need to know?
        }
        ++prebuffer_tries;
        if (prebuffering && (prebuffer_tries >= 10))
        {
            VERBOSE(VB_IMPORTANT, LOC + "Prebuffer wait timed out 10 times.");
            if (!videoOutput->EnoughFreeFrames())
            {
                VERBOSE(VB_IMPORTANT, LOC + "Prebuffer wait timed out, and"
                        "\n\t\t\tthere are not enough free frames. "
                        "Discarding buffered frames.");
                // This call will result in some ugly frames, but allows us
                // to recover from serious problems if frames get leaked.
                DiscardVideoFrames(true);
            }
            prebuffer_tries = 0;
        }
        prebuffering_lock.unlock();
        videosync->Start();
        return;
    }
    prebuffering_lock.unlock();

    //VERBOSE(VB_PLAYBACK, LOC + "fs: " + videoOutput->GetFrameStatus());
    if (!videoOutput->EnoughPrebufferedFrames())
    {
        VERBOSE(VB_GENERAL, LOC + "prebuffering pause");
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
        return;
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
    prebuffering_lock.unlock();

    videoOutput->StartDisplayingFrame();

    VideoFrame *frame = videoOutput->GetLastShownFrame();

    if (subtitlesOn)
    {
        ShowText();
        DisplaySubtitles();
    }
    else if (osdHasSubtitles || nonDisplayedSubtitles.size() > 20)
    {
        ClearSubtitles();
    }

    videofiltersLock.lock();
    videoOutput->ProcessFrame(frame, osd, videoFilters, pipplayer);
    videofiltersLock.unlock();

    AVSync();

    videoOutput->DoneDisplayingFrame();
}

void NuppelVideoPlayer::OutputVideoLoop(void)
{
    delay = 0;
    avsync_delay = 0;
    avsync_avg = 0;
    avsync_oldavg = 0;
    refreshrate = 0;
    lastsync = false;

    usevideotimebase = gContext->GetNumSetting("UseVideoTimebase", 0);

    if ((print_verbose_messages & VB_PLAYBACK) != 0)
        output_jmeter = new Jitterometer("video_output", 100);
    else
        output_jmeter = NULL;

    refreshrate = frame_interval;

    float temp_speed = (play_speed == 0.0) ? audio_stretchfactor : play_speed;
    uint fr_int = (int)(1000000.0 / video_frame_rate / temp_speed);
    uint rf_int = 0;
    if (videoOutput)
        rf_int = videoOutput->GetRefreshRate();

    if (using_null_videoout)
    {
        videosync = new USleepVideoSync(videoOutput, (int)fr_int, 0, false);
    }
    else if (videoOutput)
    {
        // Set up deinterlacing in the video output method
        m_double_framerate = false;
        if (m_scan == kScan_Interlaced && m_DeintSetting &&
            videoOutput->SetupDeinterlace(true) &&
            videoOutput->NeedsDoubleFramerate())
        {
            m_double_framerate = true;
            m_can_double       = true;
        }

        videosync = VideoSync::BestMethod(
            videoOutput, fr_int, rf_int, m_double_framerate);

        // Make sure video sync can do it
        if (videosync != NULL && m_double_framerate)
        {
            videosync->SetFrameInterval(frame_interval, m_double_framerate);
            if (videosync->UsesFrameInterval())
            {
                VERBOSE(VB_IMPORTANT, "Video sync method can't support double "
                        "framerate (refresh rate too low for bob deint)");
                FallbackDeint();
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
        if (needsetpipplayer)
        {
            pipplayer = setpipplayer;
            needsetpipplayer = false;
        }

        if (pausevideo)
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

    ShutdownAVSync();
}

#ifdef USING_IVTV
/** \fn NuppelVideoPlayer::IvtvVideoLoop(void)
 *  \brief Handles the compositing of the OSD and PiP window.
 *
 *   Unlike OutputVideoLoop(void) this loop does not actually handle
 *   the display of the video, but only handles things that go on the
 *   composited OSD surface.
 */
void NuppelVideoPlayer::IvtvVideoLoop(void)
{
    refreshrate = frame_interval;
    int delay = frame_interval;

    VideoOutputIvtv *vidout = (VideoOutputIvtv *)videoOutput;
    vidout->SetFPS(GetFrameRate());

    while (!killvideo)
    {
        if (needsetpipplayer)
        {
            pipplayer = setpipplayer;
            needsetpipplayer = false;
        }

        resetvideo = false;
        video_actually_paused = pausevideo;

        if (pausevideo)
        {
            videoThreadPaused.wakeAll();
            videofiltersLock.lock();
            videoOutput->ProcessFrame(NULL, osd, videoFilters, pipplayer);
            videofiltersLock.unlock();
        }
        else
        {
            if (subtitlesOn)
                ShowText();
            videofiltersLock.lock();
            videoOutput->ProcessFrame(NULL, osd, videoFilters, pipplayer);
            videofiltersLock.unlock();
        }

        usleep(delay);
    }

    // no need to lock this, we don't have any video frames
    delete videoOutput;
    videoOutput = NULL;
}
#endif

void *NuppelVideoPlayer::kickoffOutputVideoLoop(void *player)
{
    NuppelVideoPlayer *nvp = (NuppelVideoPlayer *)player;

#ifdef USING_IVTV
    if (nvp->forceVideoOutput == kVideoOutput_IVTV)
        nvp->IvtvVideoLoop();
    else
#endif
        nvp->OutputVideoLoop();
    return NULL;
}

bool NuppelVideoPlayer::FastForward(float seconds)
{
    if (!videoOutput)
        return false;

    if (fftime <= 0)
        fftime = (int)(seconds * video_frame_rate);

    if (osdHasSubtitles || nonDisplayedSubtitles.size() > 0)
       ClearSubtitles();

    return fftime > CalcMaxFFTime(fftime, false);
}

bool NuppelVideoPlayer::Rewind(float seconds)
{
    if (!videoOutput)
        return false;

    if (rewindtime <= 0)
        rewindtime = (int)(seconds * video_frame_rate);

    if (osdHasSubtitles || nonDisplayedSubtitles.size() > 0)
       ClearSubtitles();

    return rewindtime >= framesPlayed;
}

void NuppelVideoPlayer::SkipCommercials(int direction)
{
    if (skipcommercials == 0)
        skipcommercials = direction;
}

void NuppelVideoPlayer::ResetPlaying(void)
{
    ClearAfterSeek();

    framesPlayed = 0;

    GetDecoder()->Reset();
    errored |= GetDecoder()->IsErrored();
}

void NuppelVideoPlayer::CheckTVChain(void)
{
    bool last = !(livetvchain->HasNext());
    SetWatchingRecording(last);
}

void NuppelVideoPlayer::SwitchToProgram(void)
{
    if (videoOutput)
    {
        int  sz  = ringBuffer->DataInReadAhead();
        uint vvf = videoOutput->ValidVideoFrames();
        if ((vvf > 3 && sz > 128000) || (sz > 256000))
            return;
    }

    bool discontinuity = false, newtype = false;
    ProgramInfo *pginfo = livetvchain->GetSwitchProgram(discontinuity, newtype);
    if (!pginfo)
        return;

    if (m_playbackinfo)
    {
        m_playbackinfo->MarkAsInUse(false);
        delete m_playbackinfo;
    }

    m_playbackinfo = pginfo;
    m_playbackinfo->MarkAsInUse(true, m_recusage);

    ringBuffer->Pause();
    ringBuffer->WaitForPause();

    ringBuffer->OpenFile(pginfo->pathname, 10 /* retries -- about 5 seconds */);

    if (eof)
        discontinuity = true;

    if (discontinuity || newtype)
    {
        ringBuffer->Reset(true);
        if (newtype)
            OpenFile();
        else
            ResetPlaying();
    }
    else
    {
        GetDecoder()->SetReadAdjust(ringBuffer->SetAdjustFilesize());
        GetDecoder()->SetWaitForChange();
        if (m_tv)
            m_tv->SetIgnoreKeys(true);
    }
    if (IsErrored())
    {
        eof = true;
        return;
    }

    ringBuffer->Unpause();

    if (discontinuity || newtype)
    {
        livetvchain->SetProgram(pginfo);
        GetDecoder()->SetProgramInfo(pginfo);
        if (m_tv)
            m_tv->SetCurrentlyPlaying(pginfo);

        CheckTVChain();
        GetDecoder()->SyncPositionMap();
    }

    eof = false;
}

void NuppelVideoPlayer::FileChangedCallback(void)
{
    ringBuffer->Pause();
    ringBuffer->WaitForPause();

    ringBuffer->Reset(false, true);

    ringBuffer->Unpause();

    if (m_tv)
        m_tv->SetIgnoreKeys(false);

    livetvchain->SetProgram(m_playbackinfo);
    GetDecoder()->SetProgramInfo(m_playbackinfo);
    if (m_tv)
        m_tv->SetCurrentlyPlaying(m_playbackinfo);

    CheckTVChain();
    GetDecoder()->SyncPositionMap();
}

void NuppelVideoPlayer::JumpToProgram(void)
{
    bool discontinuity = false, newtype = false;
    ProgramInfo *pginfo = livetvchain->GetSwitchProgram(discontinuity, newtype);
    if (!pginfo)
        return;

    long long nextpos = livetvchain->GetJumpPos();
    
    if (m_playbackinfo)
    {
        m_playbackinfo->MarkAsInUse(false);
        delete m_playbackinfo;
    }

    m_playbackinfo = pginfo;
    m_playbackinfo->MarkAsInUse(true, m_recusage);

    ringBuffer->Pause();
    ringBuffer->WaitForPause();

    ringBuffer->Reset(true);
    ringBuffer->OpenFile(pginfo->pathname);

    if (newtype)
        OpenFile();
    else
        ResetPlaying();

    if (!GetDecoder())
    {
        errored = true;
        return;
    }

    ringBuffer->Unpause();
    ringBuffer->IgnoreLiveEOF(false);

    livetvchain->SetProgram(pginfo);
    GetDecoder()->SetProgramInfo(pginfo);
    if (m_tv)
        m_tv->SetCurrentlyPlaying(pginfo);

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
}

void NuppelVideoPlayer::StartPlaying(void)
{
    killplayer = false;
    framesPlayed = 0;

    if (OpenFile() < 0)
        return;

    if (!no_audio_out ||
        (forceVideoOutput == kVideoOutput_IVTV &&
         !gContext->GetNumSetting("PVR350InternalAudioOnly")))
    {
        QString errMsg = ReinitAudio();
        int ret = 1;
        if ((errMsg != QString::null) &&
            gContext->GetNumSetting("AudioNag", 1))
        {
            DialogBox dialog(gContext->GetMainWindow(), errMsg);

            QString noaudio  = QObject::tr("Continue WITHOUT AUDIO!");
            QString dontask  = noaudio + " " + 
                QObject::tr("And, never ask again.");
            QString neverask = noaudio + " " +
                QObject::tr("And, don't ask again in this session.");
            QString quit     = QObject::tr("Return to menu.");

            dialog.AddButton(noaudio);
            dialog.AddButton(dontask);
            dialog.AddButton(neverask);
            dialog.AddButton(quit);

            qApp->lock();
            ret = dialog.exec();
            qApp->unlock();
        }
            
        if (ret == 2)
            gContext->SaveSetting("AudioNag", 0);
        if (ret == 3)
            gContext->SetSetting("AudioNag", 0);
        else if (ret == 4)
            return;
    }

    if (audioOutput)
        audioOutput->SetStretchFactor(audio_stretchfactor);
    next_play_speed = audio_stretchfactor;

    if (!InitVideo())
    {
        qApp->lock();
        DialogBox *dialog = new DialogBox(gContext->GetMainWindow(),
                                   QObject::tr("Unable to initialize video."));
        dialog->AddButton(QObject::tr("Return to menu."));
        dialog->exec();
        delete dialog;

        qApp->unlock();

        if (audioOutput)
        {
            delete audioOutput;
            audioOutput = NULL;
        }
        no_audio_out = true;
        return;
    }

    if (!using_null_videoout)
    {
        QRect visible, total;
        float aspect, scaling;
        videoOutput->GetOSDBounds(total, visible, aspect, scaling);
        osd = new OSD(total, frame_interval, visible, aspect, scaling);

        if (kCodec_NORMAL_END < GetDecoder()->GetVideoCodecID() &&
            kCodec_SPECIAL_END > GetDecoder()->GetVideoCodecID() &&
            (600 < video_height))
        {            
            osd->DisableFade();
        }
    }

    playing = true;

    rewindtime = fftime = 0;
    skipcommercials = 0;

    for (int i = 0; i < MAXTBUFFER; i++)
        txtbuffers[i].buffer = new unsigned char[text_size];

    ClearAfterSeek();

    /* This thread will fill the video and audio buffers, it does all CPU
       intensive operations. We fork two other threads which do nothing but
       write to the audio and video output devices.  These should use a
       minimum of CPU. */

    pthread_t output_video, decoder_thread;

    decoder_thread = pthread_self();
    pthread_create(&output_video, NULL, kickoffOutputVideoLoop, this);

    if (!using_null_videoout)
    {
        // Request that the video output thread run with realtime priority.
        // If mythyv/mythfrontend was installed SUID root, this will work.
#ifndef CONFIG_DARWIN
        gContext->addPrivRequest(MythPrivRequest::MythRealtime, &output_video);
#endif

        // Use realtime prio for decoder thread as well
        //gContext->addPrivRequest(MythPrivRequest::MythRealtime, &decoder_thread);
    }

    if (bookmarkseek > 30)
    {
        GetFrame(audioOutput == NULL || !normal_speed);

        bool seeks = exactseeks;

        GetDecoder()->setExactSeeks(false);

        fftime = bookmarkseek;
        DoFastForward();
        fftime = 0;

        GetDecoder()->setExactSeeks(seeks);

        if (gContext->GetNumSetting("ClearSavedPosition", 1))
            m_playbackinfo->SetBookmark(0);
    }

    commBreakMapLock.lock();
    LoadCommBreakList();
    if (!commBreakMap.isEmpty())
    {
        hascommbreaktable = true;
        SetCommBreakIter();
    }
    commBreakMapLock.unlock();

    while (!killplayer && !errored)
    {
        if (m_playbackinfo)
            m_playbackinfo->UpdateInUseMark();

        if ((!paused || eof) && livetvchain)
        {
            if (livetvchain->NeedsToSwitch())
                SwitchToProgram();
            else if (livetvchain->NeedsToJump())
                JumpToProgram();
        }

        if (IsErrored() || (nvr_enc && nvr_enc->GetErrorStatus()))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Unknown error, exiting decoder");
            errored = killplayer = true;
            break;
        }

        if (play_speed != next_play_speed)
        {
            decoder_lock.lock();

            play_speed = next_play_speed;
            normal_speed = next_normal_speed;
            VERBOSE(VB_PLAYBACK, LOC + "Changing speed to " << play_speed);

            if (play_speed == 0.0)
            {
                DoPause();
                decoderThreadPaused.wakeAll();
            }
            else
                DoPlay();

            decoder_lock.unlock();
            continue;
        }

        if (eof)
        {
            if (livetvchain)
            {
                if (!paused)
                {
                    livetvchain->JumpToNext(true, 1);
                    continue;
                }
            }
            else
                break;
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

                    GetFrame(audioOutput == NULL || !normal_speed);
                    resetvideo = true;
                    while (resetvideo)
                        usleep(50);
                }
                rewindtime = 0;
            }
            else if (fftime > 0)
            {
                fftime = CalcMaxFFTime(fftime);
                if (fftime > 0)
                {
                    DoFastForward();

                    GetFrame(audioOutput == NULL || !normal_speed);
                    resetvideo = true;
                    while (resetvideo)
                        usleep(50);
                }
                fftime = 0;
            }
            else if (livetvchain && livetvchain->NeedsToJump())
            {
                JumpToProgram();

                GetFrame(audioOutput == NULL || !normal_speed);
                resetvideo = true;
                while (resetvideo)
                    usleep(50);
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
                PauseVideo();

                DoRewind();

                UnpauseVideo();
                while (GetVideoPause())
                    usleep(50);
            }
            rewindtime = 0;
        }

        if (fftime > 0 && ffrew_skip == 1)
        {
            fftime = CalcMaxFFTime(fftime);

            if (fftime >= 5)
            {
                PauseVideo();

                if (fftime >= 5)
                    DoFastForward();

                if (eof)
                    continue;

                UnpauseVideo();
                while (GetVideoPause())
                    usleep(50);
            }

            fftime = 0;
        }

        if (skipcommercials != 0 && ffrew_skip == 1)
        {
            PauseVideo();

            DoSkipCommercials(skipcommercials);
            UnpauseVideo();
            while (GetVideoPause())
                usleep(50);

            skipcommercials = 0;
            continue;
        }

        GetFrame(audioOutput == NULL || !normal_speed);

        if (using_null_videoout || forceVideoOutput == kVideoOutput_IVTV)
            GetDecoder()->UpdateFramesPlayed();
        else
            framesPlayed = videoOutput->GetFramesPlayed();

        if (ffrew_skip != 1)
            continue;

        if (!hasdeletetable && autocommercialskip)
            AutoCommercialSkip();

        if (hasdeletetable && deleteIter.data() == 1 &&
            framesPlayed >= deleteIter.key())
        {
            ++deleteIter;
            if (deleteIter.key() == totalFrames)
                eof = true;
            else
            {
                PauseVideo();
                JumpToFrame(deleteIter.key());
                UnpauseVideo();
                while (GetVideoPause())
                    usleep(50);
            }
        }
    }

    VERBOSE(VB_PLAYBACK, LOC + "Exited decoder loop.");

    decoderThreadPaused.wakeAll();

    killvideo = true;
    pthread_join(output_video, NULL);

    // need to make sure video has exited first.
    if (audioOutput)
        delete audioOutput;
    audioOutput = NULL;

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

    playing = false;

    if (IsErrored())
    {
        qApp->lock();
        DialogBox *dialog =
            new DialogBox(gContext->GetMainWindow(),
                          QObject::tr("Error was encountered while displaying video."));
        dialog->AddButton(QObject::tr("Return to Menu"));
        dialog->exec();
        delete dialog;

        qApp->unlock();
    }
}

void NuppelVideoPlayer::SetAudioParams(int bps, int channels, int samplerate)
{
    audio_bits = bps;
    audio_channels = channels;
    audio_samplerate = samplerate;
}

void NuppelVideoPlayer::SetEffDsp(int dsprate)
{
    if (audioOutput)
        audioOutput->SetEffDsp(dsprate);
}

void NuppelVideoPlayer::SetTranscoding(bool value)
{
    transcoding = value;

    if (GetDecoder())
        GetDecoder()->setTranscoding(value);
};

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
    WrapTimecode(timecode, TC_AUDIO);

    int samplesize = (audio_channels * audio_bits) / 8; // bytes per sample
    if ((samplesize <= 0) || !audioOutput)
        return;

    int samples = len / samplesize;

    // If there is no warping, just send it to the audioOutput.
    if (!usevideotimebase)
    {
        if (!audioOutput->AddSamples(buffer, samples, timecode))
            VERBOSE(VB_IMPORTANT, "NVP::AddAudioData():p1: "
                    "Audio buffer overflow, audio data lost!");
        return;
    }

    // If we need warping, do it...
    int newsamples = (int)(samples / warpfactor);
    int newlen     = newsamples * samplesize;

    // We resize the buffers if they aren't big enough
    if ((warpbuffsize < newlen) || (!warplbuff))
    {
        warplbuff = (short int*) realloc(warplbuff, newlen);
        warprbuff = (short int*) realloc(warprbuff, newlen);
        warpbuffsize = newlen;
    }

    // Resample...
    float incount  = 0.0f;
    int   outcount = 0;
    while ((incount < samples) && (outcount < newsamples))
    {
        char *out_ptr = ((char*)warplbuff) + (outcount * samplesize);
        char *in_ptr  = buffer + (((int)incount) * samplesize);
        memcpy(out_ptr, in_ptr, samplesize);

        outcount += 1;
        incount  += warpfactor;
    }
    samples = outcount;

    // Send new warped audio to audioOutput
    if (!audioOutput->AddSamples((char*)warplbuff, samples, timecode))
        VERBOSE(VB_IMPORTANT,"NVP::AddAudioData():p2: "
                "Audio buffer overflow, audio data lost!");
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

    // If there is no warping, just send it to the audioOutput.
    if (!usevideotimebase)
    {
        buffers[0] = (char*) lbuffer;
        buffers[1] = (char*) rbuffer;
        if (!audioOutput->AddSamples(buffers, samples, timecode))
            VERBOSE(VB_IMPORTANT, "NVP::AddAudioData():p3: "
                    "Audio buffer overflow, audio data lost!");
        return;
    }

    // If we need warping, do it...
    int samplesize = sizeof(short int);
    int newsamples = (int)(samples / warpfactor);
    int newlen     = newsamples * samplesize;

    // We resize the buffers if they aren't big enough
    if ((warpbuffsize < newlen) || (!warplbuff) || (!warprbuff))
    {
        warplbuff = (short int*) realloc(warplbuff, newlen);
        warprbuff = (short int*) realloc(warprbuff, newlen);
        warpbuffsize = newlen;
    }

    // Resample...
    float incount  = 0.0f;
    int   outcount = 0;
    while ((incount < samples) && (outcount < newsamples))
    {
        warplbuff[outcount] = lbuffer[(uint)round(incount)];
        warprbuff[outcount] = rbuffer[(uint)round(incount)];

        outcount += 1;
        incount  += warpfactor;
    }
    samples = outcount;

    // Send new warped audio to audioOutput
    buffers[0] = (char*) warplbuff;
    buffers[1] = (char*) warprbuff;
    if (!audioOutput->AddSamples(buffers, samples, timecode))
        VERBOSE(VB_IMPORTANT, "NVP::AddAudioData():p4: "
                "Audio buffer overflow, audio data lost!");
}

void NuppelVideoPlayer::SetBookmark(void)
{
    if (livetv)
        return;
    if (!m_playbackinfo || !osd)
        return;

    m_playbackinfo->SetBookmark(framesPlayed);
    osd->SetSettingsText(QObject::tr("Position Saved"), 1);
}

void NuppelVideoPlayer::ClearBookmark(void)
{
    if (livetv)
        return;
    if (!m_playbackinfo || !osd)
        return;

    m_playbackinfo->SetBookmark(0);
    osd->SetSettingsText(QObject::tr("Position Cleared"), 1);
}

long long NuppelVideoPlayer::GetBookmark(void) const
{
    if (!m_playbackinfo)
        return 0;

    return m_playbackinfo->GetBookmark();
}

void NuppelVideoPlayer::DoPause(void)
{
    bool skip_changed;

#ifdef USING_IVTV
    if (forceVideoOutput == kVideoOutput_IVTV)
    {
        VideoOutputIvtv *vidout = (VideoOutputIvtv *)videoOutput;
        vidout->Pause();
    }
#endif

    skip_changed = (ffrew_skip != 1);
    ffrew_skip = 1;

    if (skip_changed)
    {
        //cout << "handling skip change" << endl;
        videoOutput->SetPrebuffering(ffrew_skip == 1);

        GetDecoder()->setExactSeeks(exactseeks && ffrew_skip == 1);
        GetDecoder()->DoFastForward(framesPlayed + ffrew_skip);
        ClearAfterSeek();
    }

    float temp_speed = audio_stretchfactor;
    frame_interval = (int)(1000000.0 * ffrew_skip / video_frame_rate / temp_speed);
    VERBOSE(VB_PLAYBACK, QString("rate: %1 speed: %2 skip: %3 = interval %4")
                                 .arg(video_frame_rate).arg(temp_speed)
                                 .arg(ffrew_skip).arg(frame_interval));
    if (osd && forceVideoOutput != kVideoOutput_IVTV)
        osd->SetFrameInterval(frame_interval);
    if (videosync != NULL)
        videosync->SetFrameInterval(frame_interval, m_double_framerate);

    //cout << "setting paused" << endl << endl;
    paused = actuallypaused = true;
}

void NuppelVideoPlayer::DoPlay(void)
{
    bool skip_changed;

    if (play_speed > 0.0f && play_speed <= 3.0f)
    {
        skip_changed = (ffrew_skip != 1);
        ffrew_skip = 1;
    }
    else
    {
        skip_changed = true;
        uint tmp = (uint) ceil(4.0 * abs(play_speed) / keyframedist);
        ffrew_skip = tmp * keyframedist;
        ffrew_skip = (play_speed > 0.0) ? ffrew_skip : -ffrew_skip;
    }

    if (skip_changed)
    {
        //cout << "handling skip change" << endl;
        videoOutput->SetPrebuffering(ffrew_skip == 1);

#ifdef USING_IVTV
        if (forceVideoOutput == kVideoOutput_IVTV)
        {
            VideoOutputIvtv *vidout = (VideoOutputIvtv *)videoOutput;
            vidout->NextPlay(play_speed / ffrew_skip, normal_speed, 
                             (ffrew_skip == 1) ? 2 : 0);
        }
#endif

        GetDecoder()->setExactSeeks(exactseeks && ffrew_skip == 1);
        GetDecoder()->DoRewind(framesPlayed);
        ClearAfterSeek();
    }

    float tmp = ffrew_skip / video_frame_rate / play_speed;
    frame_interval = (int) (1000000.0f * tmp);

    VERBOSE(VB_PLAYBACK, LOC + "DoPlay: " +
            QString("rate: %1 speed: %2 skip: %3 => new interval %4")
            .arg(video_frame_rate).arg(play_speed)
            .arg(ffrew_skip).arg(frame_interval));

    if (osd && forceVideoOutput != kVideoOutput_IVTV)
        osd->SetFrameInterval(frame_interval);

    if (videoOutput && videosync)
    {
        videosync->SetFrameInterval(frame_interval, m_double_framerate);

        // If using bob deinterlace, turn on or off if we
        // changed to or from synchronous playback speed.

        videofiltersLock.lock();
        if (m_double_framerate)
        {
            if (!normal_speed || play_speed < 0.99 || play_speed > 1.01)
            {
                FallbackDeint();
            }
        }
        else if (m_can_double && !m_double_framerate)
        {
            if (normal_speed && play_speed > 0.99 && play_speed < 1.01 &&
                (m_scan == kScan_Interlaced && m_DeintSetting))
            {
                videosync->SetFrameInterval(frame_interval, true);
                videoOutput->SetupDeinterlace(false);
                videoOutput->SetupDeinterlace(true);
                if (videoOutput->NeedsDoubleFramerate())
                    m_double_framerate = true;
            }
        }
        videofiltersLock.unlock();
    }

#ifdef USING_IVTV
    if (forceVideoOutput == kVideoOutput_IVTV)
    {
        VideoOutputIvtv *vidout = (VideoOutputIvtv *)videoOutput;
        vidout->Play(play_speed / ffrew_skip, normal_speed, 
                     (ffrew_skip == 1) ? 2 : 0);
    }
#endif

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
        audioOutput->SetStretchFactor(play_speed);
#ifdef USING_DIRECTX
        audioOutput->Reset();
#endif
    }

    //cout << "setting unpaused" << endl << endl;
    paused = actuallypaused = false;
}

bool NuppelVideoPlayer::DoRewind(void)
{
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
    bool hasliveprev = (livetv && livetvchain && livetvchain->HasPrev());

    if (!hasliveprev)
        return rw;

    if ((framesPlayed - rw + 1) < 0)
    {
        livetvchain->JumpToNext(false, (int)(-15.0 * video_frame_rate));
        return -1;
    }

    return rw;
}

long long NuppelVideoPlayer::CalcMaxFFTime(long long ff, bool setjump) const
{
    long long maxtime = (long long)(1.0 * video_frame_rate);
    bool islivetvcur = (livetv && livetvchain && !livetvchain->HasNext());

    if (livetv || (watchingrecording && nvr_enc && nvr_enc->IsValidRecorder()))
        maxtime = (long long)(3.0 * video_frame_rate);

    long long ret = ff;

    limitKeyRepeat = false;

    if (livetv && !islivetvcur)
    {
        if (totalFrames > 0)
        {
            long long behind = totalFrames - framesPlayed;
            if (behind < maxtime || behind - ff <= maxtime * 2)
            {
                ret = -1;
                if (setjump)
                    livetvchain->JumpToNext(true, 1);
            }
        }
    }
    else if (islivetvcur || (watchingrecording && nvr_enc && 
                        nvr_enc->IsValidRecorder()))
    {
        long long behind = nvr_enc->GetFramesWritten() - framesPlayed;
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

/** \fn NuppelVideoPlayer::IsNearEnd(long long) const
 *  \brief Returns true iff near end of recording.
 *  \param margin minimum number of frames we want before being near end,
 *                defaults to 2 seconds of video.
 */
bool NuppelVideoPlayer::IsNearEnd(long long margin) const
{
    long long framesRead, framesLeft;
    bool watchingTV = watchingrecording && nvr_enc && nvr_enc->IsValidRecorder();
    if (!livetv && !watchingTV)
        return false;

    if (livetv && livetvchain && livetvchain->HasNext())
        return false;

    margin = (margin >= 0) ? margin: (long long) (video_frame_rate*2);
    margin = (long long) (margin * audio_stretchfactor);

    framesRead = GetDecoder()->GetFramesRead();
    framesLeft = nvr_enc->GetCachedFramesWritten() - framesRead;

    // if it looks like we are near end, get an updated GetFramesWritten()
    if (framesLeft < margin)
        framesLeft = nvr_enc->GetFramesWritten() - framesRead;

    return (framesLeft < margin);
}

bool NuppelVideoPlayer::DoFastForward(void)
{
    long long number = fftime - 1;
    long long desiredFrame = framesPlayed + number;

    if (paused && !editmode)
        GetDecoder()->setExactSeeks(true);
    GetDecoder()->DoFastForward(desiredFrame);
    GetDecoder()->setExactSeeks(exactseeks);

    ClearAfterSeek();
    lastSkipTime = time(NULL);
    return true;
}

void NuppelVideoPlayer::JumpToFrame(long long frame)
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
        (watchingrecording && nvr_enc && nvr_enc->IsValidRecorder()))
    {
        if (nvr_enc->GetFramesWritten() < (framesPlayed + frames))
            return 1;
    }
    else if ((totalFrames) && (totalFrames < (framesPlayed + frames)))
    {
        return 1;
    }
    return 0;
}
#endif

/** \fn NuppelVideoPlayer::ClearAfterSeek(void)
 *  \brief This is to support seeking...
 *
 *   This resets the output classes and discards all
 *   frames no longer being used by the decoder class.
 *
 *   Note: caller should not hold any locks
 */
void NuppelVideoPlayer::ClearAfterSeek(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "ClearAfterSeek()");

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

    SetPrebuffering(true);
    if (audioOutput)
        audioOutput->Reset();

    if (osd)
        osd->ClearAllCCText();

    SetDeleteIter();
    SetCommBreakIter();

    if (livetvchain)
        livetvchain->ClearSwitch();
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

        if (deleteIter.data() == 0)
            ++deleteIter;
    }
}

void NuppelVideoPlayer::SetCommBreakIter(void)
{
    if (hascommbreaktable)
    {
        commBreakIter = commBreakMap.begin();
        while (commBreakIter != commBreakMap.end())
        {
            if ((framesPlayed + 2) > commBreakIter.key())
            {
                commBreakIter++;
            }
            else
                break;
        }

        VERBOSE(VB_COMMFLAG, LOC + QString("new commBreakIter = %1 @ frame %2")
                                           .arg(commBreakIter.data())
                                           .arg(commBreakIter.key()));
    }
}

void NuppelVideoPlayer::SetAutoCommercialSkip(int autoskip)
{
    SetCommBreakIter();
    autocommercialskip = autoskip;
}

bool NuppelVideoPlayer::EnableEdit(void)
{
    editmode = false;

    if (!hasFullPositionMap || !m_playbackinfo || !osd)
        return false;

    bool alreadyediting = false;
    alreadyediting = m_playbackinfo->IsEditing();

    if (alreadyediting)
        return false;

    if (GetPause())
        osd->EndStatus();

    editmode = true;
    Pause();
    while (!GetPause())
        usleep(50);

    seekamount = keyframedist;
    seekamountpos = 3;

    dialogname = "";

    QMap<QString, QString> infoMap;
    m_playbackinfo->ToMap(infoMap);
    osd->SetText("editmode", infoMap, -1);

    UpdateEditSlider();
    UpdateTimeDisplay();
    UpdateSeekAmount(true);

    if (hasdeletetable)
    {
        if (deleteMap.contains(0))
            deleteMap.erase(0);
        if (deleteMap.contains(totalFrames))
            deleteMap.erase(totalFrames);

        QMap<long long, int>::Iterator it;
        for (it = deleteMap.begin(); it != deleteMap.end(); ++it)
             AddMark(it.key(), it.data());
    }

    m_playbackinfo->SetEditing(true);

    return editmode;
}

void NuppelVideoPlayer::DisableEdit(void)
{
    editmode = false;

    if (!m_playbackinfo)
        return;

    QMap<long long, int>::Iterator i = deleteMap.begin();
    for (; i != deleteMap.end(); ++i)
        osd->HideEditArrow(i.key(), i.data());
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

    m_playbackinfo->SetEditing(false);
}

bool NuppelVideoPlayer::DoKeypress(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("TV Editing", e, actions);

    if (dialogname != "")
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
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

    for (unsigned int i = 0; i < actions.size() && !handled; i++)
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
                    usleep(50);
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
                    usleep(50);
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
                osd->HideEditArrow(it.key(), it.data());

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
                        if (it.data() == MARK_COMM_START)
                            AddMark(it.key(), 1);
                        else
                            AddMark(it.key(), 0);
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
                usleep(50);
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
                usleep(50);
            UpdateEditSlider();
            UpdateTimeDisplay();
        }
        else if (action == "ESCAPE" || action == "MENU" ||
                 action == "TOGGLEEDIT")
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

int NuppelVideoPlayer::GetLetterbox(void) const
{
    if (videoOutput)
        return videoOutput->GetLetterbox();
    return false;
}

void NuppelVideoPlayer::ToggleLetterbox(int letterboxMode)
{
    if (videoOutput)
    {
        videoOutput->ToggleLetterbox(letterboxMode);
        ReinitOSD();
    }
}

void NuppelVideoPlayer::Zoom(int direction)
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

void NuppelVideoPlayer::UpdateSeekAmount(bool up)
{
    if (seekamountpos > 0 && !up)
        seekamountpos--;
    if (seekamountpos < 9 && up)
        seekamountpos++;

    QString text = "";

    int fps = (int)ceil(video_frame_rate);

    switch (seekamountpos)
    {
        case 0: text = QObject::tr("cut point"); seekamount = -2; break;
        case 1: text = QObject::tr("keyframe"); seekamount = -1; break;
        case 2: text = QObject::tr("1 frame"); seekamount = 1; break;
        case 3: text = QObject::tr("0.5 seconds"); seekamount = fps / 2; break;
        case 4: text = QObject::tr("1 second"); seekamount = fps; break;
        case 5: text = QObject::tr("5 seconds"); seekamount = fps * 5; break;
        case 6: text = QObject::tr("20 seconds"); seekamount = fps * 20; break;
        case 7: text = QObject::tr("1 minute"); seekamount = fps * 60; break;
        case 8: text = QObject::tr("5 minutes"); seekamount = fps * 300; break;
        case 9: text = QObject::tr("10 minutes"); seekamount = fps * 600; break;
        default: text = QObject::tr("error"); seekamount = fps; break;
    }

    QMap<QString, QString> infoMap;
    infoMap["seekamount"] = text;
    osd->SetText("editmode", infoMap, -1);
}

void NuppelVideoPlayer::UpdateTimeDisplay(void)
{
    int hours = 0;
    int mins = 0;
    int secs = 0;
    int frames = 0;

    int fps = (int)ceil(video_frame_rate);

    hours = (framesPlayed / fps) / 60 / 60;
    mins = (framesPlayed / fps) / 60 - (hours * 60);
    secs = (framesPlayed / fps) - (mins * 60) - (hours * 60 * 60);
    frames = framesPlayed - ((secs * fps) + (mins * 60 * fps) +
                             (hours * 60 * 60 * fps));

    char timestr[128];
    sprintf(timestr, "%1d:%02d:%02d.%02d",
        hours, mins, secs, frames);

    char framestr[128];
    sprintf(framestr, "%lld", framesPlayed);

    QString cutmarker = " ";
    if (IsInDelete(framesPlayed))
        cutmarker = QObject::tr("cut");

    QMap<QString, QString> infoMap;
    infoMap["timedisplay"] = timestr;
    infoMap["framedisplay"] = framestr;
    infoMap["cutindicator"] = cutmarker;
    osd->SetText("editmode", infoMap, -1);
}

void NuppelVideoPlayer::HandleSelect(void)
{
    bool deletepoint = false;
    QMap<long long, int>::Iterator i;
    int direction = 0;

    for (i = deleteMap.begin(); i != deleteMap.end(); ++i)
    {
        long long pos = framesPlayed - i.key();
        if (pos < 0)
            pos = 0 - pos;
        if (pos < (int)ceil(20 * video_frame_rate))
        {
            deletepoint = true;
            deleteframe = i.key();
            direction = i.data();
            break;
        }
    }

    if (deletepoint)
    {
        QString message = QObject::tr("You are close to an existing cut point.  Would you "
                          "like to:");
        QString option1 = QObject::tr("Delete this cut point");
        QString option2 = QObject::tr("Move this cut point to the current position");
        QString option3 = QObject::tr("Flip directions - delete to the ");
        if (direction == 0)
            option3 += QObject::tr("right");
        else
            option3 += QObject::tr("left");
        QString option4 = QObject::tr("Cancel");

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
        QString option3 = QObject::tr("Cancel");

        dialogname = "addmark";
        dialogtype = 1;

        QStringList options;
        options += option1;
        options += option2;
        options += option3;

        osd->NewDialogBox(dialogname, message, options, -1);
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
            default:
                break;
        }
    }
    else if (dialogtype == 1)
    {
        switch (result)
        {
            case 1:
                AddMark(framesPlayed, 0);
                break;
            case 2:
                AddMark(framesPlayed, 1);
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

    if (deleteMap[frames] == 0)
        deleteMap[frames] = 1;
    else
        deleteMap[frames] = 0;

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
                usleep(50);
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
                usleep(50);
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
                usleep(50);
        }
        else
        {
            GetDecoder()->setExactSeeks(false);
            rewindtime = 2;
            while (rewindtime > 0)
                usleep(50);
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
        int direction = i.data();

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

void NuppelVideoPlayer::SaveCutList(void)
{
    if (!m_playbackinfo)
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
        int direction = i.data();

        if (direction == 0 && !indelete && first)
        {
            deleteMap[0] = 1;
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
        deleteMap[totalFrames] = 0;

    m_playbackinfo->SetMarkupFlag(MARK_UPDATED_CUT, true);
    m_playbackinfo->SetCutList(deleteMap);
}

void NuppelVideoPlayer::LoadCutList(void)
{
    if (!m_playbackinfo)
        return;

    m_playbackinfo->GetCutList(deleteMap);
}

void NuppelVideoPlayer::LoadCommBreakList(void)
{
    if (!m_playbackinfo)
        return;

    m_playbackinfo->GetCommBreakList(commBreakMap);
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
            int type = it.data();

            if (((type == MARK_COMM_END) ||
                 (type == MARK_CUT_END)) &&
                ((lastType == MARK_COMM_START) ||
                 (lastType == MARK_CUT_START)))
                return true;

            if ((type == MARK_COMM_START) ||
                (type == MARK_CUT_START))
                return false;
        }

        lastType = it.data();
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

    if (!hasFullPositionMap)
    {
        VERBOSE(VB_IMPORTANT, LOC + "Recording does not have position map.\n" +
                QString("\t\t\tRun 'mythcommflag --file %1 --rebuild' to fix")
                .arg(m_playbackinfo->GetRecordBasename()));
    }

    if ((video_width <= 0) || (video_height <= 0))
    {
        VERBOSE(VB_IMPORTANT, QString("NVP: Video Resolution invalid %1x%2")
            .arg(video_width).arg(video_height));
        return NULL;
    }

    if (!InitVideo())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Unable to initialize video for screen grab.");
        return NULL;
    }

    for (int i = 0; i < MAXTBUFFER; i++)
        txtbuffers[i].buffer = new unsigned char[text_size];

    ClearAfterSeek();

    number = (long long) (secondsin * video_frame_rate);
    number = (number >= totalFrames) ? (totalFrames/2) : number;

    // Only get bookmark if we have position map
    if (hasFullPositionMap)
    {
        previewFromBookmark = gContext->GetNumSetting("PreviewFromBookmark");
        if (previewFromBookmark != 0)
        {
            bookmarkseek = GetBookmark();
            if (bookmarkseek > 30)
                number = bookmarkseek;
        }
    }

    // Only get commercial map if we have position map
    if (hasFullPositionMap)
    {
        oldnumber = number;
        LoadCutList();

        QMutexLocker locker(&commBreakMapLock);
        LoadCommBreakList();

        while ((FrameIsInMap(number, commBreakMap) || 
                (FrameIsInMap(number, deleteMap))))
        {
            number += (long long) (30 * video_frame_rate);
            if (number >= totalFrames)
            {
                number = oldnumber;
                break;
            }
        }
    }

    // Only do seek if we have position map
    if (hasFullPositionMap)
    {
        GetFrame(1);
        DiscardVideoFrame(videoOutput->GetLastDecodedFrame());

        fftime = number;
        DoFastForward();
        fftime = 0;
    }

    GetFrame(1);

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

    avpicture_fill(&orig, data, PIX_FMT_YUV420P, video_width, video_height);

    avpicture_deinterlace(&orig, &orig, PIX_FMT_YUV420P, video_width,
                          video_height);

    bufflen = video_width * video_height * 4;
    outputbuf = new unsigned char[bufflen];

    avpicture_fill(&retbuf, outputbuf, PIX_FMT_RGBA32, video_width,
                   video_height);

    img_convert(&retbuf, PIX_FMT_RGBA32, &orig, PIX_FMT_YUV420P,
                video_width, video_height);

    vw = video_width;
    vh = video_height;
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
    if (m_playbackinfo)
        m_playbackinfo->UpdateInUseMark();

    if (frameNumber >= 0)
    {
        JumpToFrame(frameNumber);
        ClearAfterSeek();
    }

    GetFrame(1,true);

    return videoOutput->GetLastDecodedFrame();
}

QString NuppelVideoPlayer::GetEncodingType(void) const
{
    return GetDecoder()->GetEncodingType();
}

bool NuppelVideoPlayer::GetRawAudioState(void) const
{
    return GetDecoder()->GetRawAudioState();
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
    playing = true;
    keyframedist = 30;
    warpfactor = 1;
    warpfactor_avg = 1;

    if (!InitVideo())
    {
        VERBOSE(VB_IMPORTANT, "NVP: Unable to initialize video for transcode.");
        playing = false;
        return;
    }

    for (int i = 0; i < MAXTBUFFER; i++)
        txtbuffers[i].buffer = new unsigned char[text_size];

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
    if (m_playbackinfo)
        m_playbackinfo->UpdateInUseMark();

    if (dm_iter == NULL && honorCutList)
        dm_iter = deleteMap.begin();
    
    if (!GetDecoder()->GetFrame(0))
        return false;
    if (eof)
        return false;

    if (honorCutList && (!deleteMap.isEmpty()))
    {
        if (videoOutput->GetLastDecodedFrame()->frameNumber >= dm_iter.key())
        {
            while((dm_iter.data() == 1) && (dm_iter != deleteMap.end()))
            {
                QString msg = QString("Fast-Forwarding from %1")
                              .arg((int)dm_iter.key());
                dm_iter++;
                msg += QString(" to %1").arg((int)dm_iter.key());
                VERBOSE(VB_GENERAL, msg);
                GetDecoder()->DoFastForward(dm_iter.key());
                GetDecoder()->ClearStoredData();
                ClearAfterSeek();
                GetDecoder()->GetFrame(0);
                *did_ff = 1;
            }
            while((dm_iter.data() == 0) && (dm_iter != deleteMap.end()))
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
    VERBOSE(VB_COMMFLAG,
            QString("Setting New Commercial Break List, old size %1, new %2")
                    .arg(commBreakMap.size()).arg(newMap.size()));

    commBreakMapLock.lock();
    commBreakMap.clear();
    commBreakMap = newMap;
    hascommbreaktable = !commBreakMap.isEmpty();
    SetCommBreakIter();
    commBreakMapLock.unlock();
}

bool NuppelVideoPlayer::RebuildSeekTable(bool showPercentage, StatusCallback cb, void* cbData)
{
    int percentage = 0;
    long long myFramesPlayed = 0;
    bool looped = false;

    killplayer = false;
    framesPlayed = 0;
    using_null_videoout = true;

    // clear out any existing seektables
    m_playbackinfo->ClearPositionMap(MARK_KEYFRAME);
    m_playbackinfo->ClearPositionMap(MARK_GOP_START);
    m_playbackinfo->ClearPositionMap(MARK_GOP_BYFRAME);

    if (OpenFile() < 0)
        return(0);

    playing = true;

    if (!InitVideo())
    {
        VERBOSE(VB_IMPORTANT, "NVP: Unable to initialize video for RebuildSeekTable.");
        playing = false;
        return 0;
    }

    for (int i = 0; i < MAXTBUFFER; i++)
        txtbuffers[i].buffer = new unsigned char[text_size];

    ClearAfterSeek();

    MythTimer flagTime;
    flagTime.start();

    GetFrame(-1,true);
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
        looped = true;
        myFramesPlayed++;

        if ((myFramesPlayed % 100) == 0)
        {
            if (m_playbackinfo)
                m_playbackinfo->UpdateInUseMark();

            if (totalFrames)
            {

                int flagFPS;
                float elapsed = flagTime.elapsed() / 1000.0;

                if (elapsed)
                    flagFPS = (int)(myFramesPlayed / elapsed);
                else
                    flagFPS = 0;

                percentage = myFramesPlayed * 100 / totalFrames;
                if (cb)
                    (*cb)(percentage, cbData);

                if (showPercentage)
                {
                    printf( "\b\b\b\b\b\b\b\b\b\b\b\b\b" );
                    printf( "%3d%%/%5dfps", percentage, flagFPS );
                }
            }
            else
            {
                if (showPercentage)
                {
                    printf( "\b\b\b\b\b\b" );
                    printf( "%6lld", myFramesPlayed );
                }
            }

            if (showPercentage)
                fflush( stdout );
        }

        GetFrame(-1,true);
    }

    if (showPercentage)
    {
        if (totalFrames)
            printf( "\b\b\b\b\b\b\b" );
        printf( "\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b" );
    }

    playing = false;
    killplayer = true;

    GetDecoder()->SetPositionMap();

    return true;
}

int NuppelVideoPlayer::GetStatusbarPos(void) const
{
    double spos = 0.0;

    if ((livetv) ||
        (watchingrecording && nvr_enc &&
         nvr_enc->IsValidRecorder()))
    {
        spos = 1000.0 * framesPlayed / nvr_enc->GetFramesWritten();
    }
    else if (totalFrames)
    {
        spos = 1000.0 * framesPlayed / totalFrames;
    }

    return((int)spos);
}

int NuppelVideoPlayer::GetSecondsBehind(void) const
{
    if (!nvr_enc)
        return 0;

    long long written = nvr_enc->GetFramesWritten();
    long long played = framesPlayed;

    if (played > written)
        played = written;
    if (played < 0)
        played = 0;

    return (int)((float)(written - played) / video_frame_rate);
}

void NuppelVideoPlayer::calcSliderPos(struct StatusPosInfo &posInfo)
{
    posInfo.desc = "";
    posInfo.position = 0;
    posInfo.progBefore = false;
    posInfo.progAfter = false;

    if (ringBuffer->isDVD())
    {
        long long rPos = ringBuffer->GetReadPosition();
        long long tPos = 1;//ringBuffer->GetTotalReadPosition();
        
        ringBuffer->getDescForPos(posInfo.desc);

        if (rPos)
            posInfo.position = (int)((rPos / tPos) * 1000.0);

        return;
    }

    int playbackLen = totalLength;
    
    if (livetv && livetvchain)
    {
        posInfo.progBefore = livetvchain->HasPrev();
        posInfo.progAfter = livetvchain->HasNext();
        playbackLen = livetvchain->GetLengthAtCurPos();
    }
    else if (watchingrecording && nvr_enc && nvr_enc->IsValidRecorder())
        playbackLen =
            (int)(((float)nvr_enc->GetFramesWritten() / video_frame_rate));

    float secsplayed = ((float)framesPlayed / video_frame_rate);
    playbackLen = max(playbackLen, 1);
    secsplayed  = min((float)playbackLen, max(secsplayed, 0.0f));

    posInfo.position = (int)(1000.0 * (secsplayed / (float)playbackLen));

    int phours = (int)secsplayed / 3600;
    int pmins = ((int)secsplayed - phours * 3600) / 60;
    int psecs = ((int)secsplayed - phours * 3600 - pmins * 60);

    int shours = playbackLen / 3600;
    int smins = (playbackLen - shours * 3600) / 60;
    int ssecs = (playbackLen - shours * 3600 - smins * 60);

    QString text1, text2;
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

    posInfo.desc = QObject::tr("%1 of %2").arg(text1).arg(text2);
}

void NuppelVideoPlayer::AutoCommercialSkip(void)
{
    if (((time(NULL) - lastSkipTime) <= 2) ||
        ((time(NULL) - lastCommSkipTime) <= 2))
        return;

    commBreakMapLock.lock();
    if (hascommbreaktable)
    {
        if (commBreakIter.data() == MARK_COMM_END)
            commBreakIter++;

        if (commBreakIter == commBreakMap.end())
        {
            commBreakMapLock.unlock();
            return;
        }

        if ((commBreakIter.data() == MARK_COMM_START) &&
            (((autocommercialskip == 1) &&
              (framesPlayed >= commBreakIter.key())) ||
             ((autocommercialskip == 2) &&
              (framesPlayed + commnotifyamount * video_frame_rate >=
               commBreakIter.key()))))
        {
            VERBOSE(VB_COMMFLAG, LOC + QString("AutoCommercialSkip(), current "
                    "framesPlayed %1, commBreakIter frame %2, incrementing "
                    "commBreakIter")
                    .arg(framesPlayed).arg(commBreakIter.key()));

            ++commBreakIter;
            if (commBreakIter == commBreakMap.end())
            {
                commBreakMapLock.unlock();
                VERBOSE(VB_COMMFLAG, LOC + "AutoCommercialSkip(), at "
                        "end of commercial break list, will not skip.");
                return;
            }

            if (commBreakIter.data() == MARK_COMM_START)
            {
                commBreakMapLock.unlock();
                VERBOSE(VB_COMMFLAG, LOC + "AutoCommercialSkip(), new "
                        "commBreakIter mark is another start, will not skip.");
                return;
            }

            VERBOSE(VB_COMMFLAG, LOC + QString("AutoCommercialSkip(), new "
                    "commBreakIter frame %1").arg(commBreakIter.key()));

            if (commBreakIter.key() == totalFrames)
            {
                eof = true;
            }
            else
            {
                if (osd)
                {
                    QString comm_msg;
                    int skipped_seconds = (int)((commBreakIter.key() -
                            framesPlayed) / video_frame_rate);
                    QString skipTime;
                    skipTime.sprintf("%d:%02d", skipped_seconds / 60,
                                     abs(skipped_seconds) % 60);
                    if (autocommercialskip == 1)
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

                if (autocommercialskip == 1)
                {
                    VERBOSE(VB_COMMFLAG, LOC + QString("AutoCommercialSkip(), "
                        "auto-skipping to frame %1")
                        .arg(commBreakIter.key() - 
                        (int)(commrewindamount * video_frame_rate)));

                    PauseVideo();
                    JumpToFrame(commBreakIter.key() -
                        (int)(commrewindamount * video_frame_rate));
                    UnpauseVideo();
                    while (GetVideoPause())
                        usleep(50);

                    GetFrame(1, true);
                }
                else
                {
                    ++commBreakIter;
                }
            }
        }
    }

    commBreakMapLock.unlock();
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
        message += m_playbackinfo->chanid + " " +
                   m_playbackinfo->startts.toString(Qt::ISODate);
        RemoteSendMessage(message);

        return false;
    }

    if ((direction == (0 - lastCommSkipDirection)) &&
        ((time(NULL) - lastCommSkipTime) <= 3))
    {
        if (osd)
        {
            struct StatusPosInfo posInfo;
            calcSliderPos(posInfo);
            osd->ShowStatus(posInfo, false, QObject::tr("Skipping Back."), 2);
        }

        if (lastCommSkipStart > (2.0 * video_frame_rate))
            lastCommSkipStart -= (long long) (2.0 * video_frame_rate);

        JumpToFrame(lastCommSkipStart);
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

        JumpToFrame(0);
        commBreakMapLock.unlock();
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

                JumpToFrame(0);
                commBreakMapLock.unlock();
                return true;
            }
            else
                commBreakIter--;
        }
    }
    else if (commBreakIter.data() == MARK_COMM_START)
    {
        int skipped_seconds = (int)((commBreakIter.key() -
                framesPlayed) / video_frame_rate);

        // special case when hitting 'skip' < 20 seconds before break
        if (skipped_seconds < 20)
        {
            commBreakIter++;

            if (commBreakIter == commBreakMap.end())
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

    if (osd)
    {
        int skipped_seconds = (int)((commBreakIter.key() -
                framesPlayed) / video_frame_rate);
        QString skipTime;
        skipTime.sprintf("%d:%02d", skipped_seconds / 60,
                         abs(skipped_seconds) % 60);
        struct StatusPosInfo posInfo;
        calcSliderPos(posInfo);
        osd->ShowStatus(posInfo, false,
                        QObject::tr("Skip %1").arg(skipTime), 2);
    }

    if (direction > 0)
        JumpToFrame(commBreakIter.key() -
                    (int)(commrewindamount * video_frame_rate));
    else
        JumpToFrame(commBreakIter.key());

    commBreakIter++;
    commBreakMapLock.unlock();

    return true;
}

void NuppelVideoPlayer::incCurrentAudioTrack()
{
    if (GetDecoder())
        GetDecoder()->incCurrentAudioTrack();
}

void NuppelVideoPlayer::decCurrentAudioTrack()
{
    if (GetDecoder())
        GetDecoder()->decCurrentAudioTrack();
}

bool NuppelVideoPlayer::setCurrentAudioTrack(int trackNo)
{
    if (GetDecoder())
        return GetDecoder()->setCurrentAudioTrack(trackNo);
    else
        return false;
}


int NuppelVideoPlayer::getCurrentAudioTrack() const
{
    if (GetDecoder())
        return GetDecoder()->getCurrentAudioTrack() + 1;
    else
        return 0;
}

QStringList NuppelVideoPlayer::listAudioTracks() const
{
    if (decoder)
        return decoder->listAudioTracks();
    else
        return QStringList();
}

void NuppelVideoPlayer::incCurrentSubtitleTrack()
{
    if (decoder)
        decoder->incCurrentSubtitleTrack();
}

void NuppelVideoPlayer::decCurrentSubtitleTrack()
{
    if (decoder)
        decoder->decCurrentSubtitleTrack();
}

bool NuppelVideoPlayer::setCurrentSubtitleTrack(int trackNo)
{
    subtitlesOn = false;
    if ((trackNo >= 0) && decoder)
        subtitlesOn = decoder->setCurrentSubtitleTrack(trackNo);
    return subtitlesOn;
}


int NuppelVideoPlayer::getCurrentSubtitleTrack() const
{
    int trackNo = 0;
    if (subtitlesOn && decoder)
        trackNo = decoder->getCurrentSubtitleTrack() + 1;
    return trackNo;
}

QStringList NuppelVideoPlayer::listSubtitleTracks() const
{
    QStringList list;
    if (decoder)
        list = decoder->listSubtitleTracks();
    return list;
}

// updates new subtitles to the screen and clears old ones
void NuppelVideoPlayer::DisplaySubtitles()
{
    OSDSet *subtitleOSD;
    bool setVisible = false;
    VideoFrame *currentFrame = videoOutput->GetLastShownFrame();

    if (!osd || !currentFrame || !(subtitleOSD = osd->GetSet("subtitles")))
        return;

    subtitleLock.lock();

    subtitleOSD = osd->GetSet("subtitles");

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

    while (nonDisplayedSubtitles.size() > 0)
    {
        const AVSubtitle subtitlePage = nonDisplayedSubtitles.front();

        if (subtitlePage.start_display_time > currentFrame->timecode)
            break;

        nonDisplayedSubtitles.pop_front();

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
            AVSubtitleRect* rect = &subtitlePage.rects[i];

            bool displaysub = true;
            if (nonDisplayedSubtitles.size() > 0 &&
                nonDisplayedSubtitles.front().start_display_time < 
                currentFrame->timecode)
            {
                displaysub = false;
            }

            if (displaysub)
            {
                // AVSubtitleRect's image data's not guaranteed to be 4 byte
                // aligned.
                QImage qImage(rect->w, rect->h, 32);
                qImage.setAlphaBuffer(true);
                for (int y = 0; y < rect->h; ++y) 
                {
                    for (int x = 0; x < rect->w; ++x) 
                    {
                        const uint8_t color = rect->bitmap[y*rect->linesize + x];
                        const uint32_t pixel = rect->rgba_palette[color];
                        qImage.setPixel(x, y, pixel);
                    }
                }

                // scale the subtitle images which are scaled and positioned for
                // a 720x576 video resolution to fit the current OSD resolution
                float hmult = osd->GetSubtitleBounds().width() / 720.0;
                float vmult = osd->GetSubtitleBounds().height() / 576.0;
                rect->x = (int)(rect->x * hmult);
                rect->y = (int)(rect->y * vmult);
                rect->w = (int)(rect->w * hmult);
                rect->h = (int)(rect->h * vmult);
                QImage scaledImage = qImage.smoothScale(rect->w, rect->h);

                OSDTypeImage* image = new OSDTypeImage();
                image->SetPosition(QPoint(rect->x, rect->y));
                image->LoadFromQImage(scaledImage);

                subtitleOSD->AddType(image);

                osdSubtitlesExpireAt = subtitlePage.end_display_time;
                setVisible = true;
                osdHasSubtitles = true;
            }

            av_free(rect->rgba_palette);
            av_free(rect->bitmap);
        }

        if (subtitlePage.num_rects > 0)
            av_free(subtitlePage.rects);
    }

    subtitleLock.unlock();

    if (setVisible)
    {
        VERBOSE(VB_PLAYBACK,
                QString("Setting subtitles visible, frame_timecode=%1")
                .arg(currentFrame->timecode));
        osd->SetVisible(subtitleOSD, 0);
    }
}

// hide subtitles and free the undisplayed subtitles
void NuppelVideoPlayer::ClearSubtitles() 
{
    subtitleLock.lock();

    while (nonDisplayedSubtitles.size() > 0) 
    {
        AVSubtitle& subtitle = nonDisplayedSubtitles.front();

        // Because the subtitles were not displayed, OSDSet does not
        // free the OSDTypeImages in ClearAll(), so we have to free
        // the dynamic buffers here
        for (std::size_t i = 0; i < subtitle.num_rects; ++i) 
        {
             AVSubtitleRect* rect = &subtitle.rects[i];
             av_free(rect->rgba_palette);
             av_free(rect->bitmap);
        }

        if (subtitle.num_rects > 0)
            av_free(subtitle.rects);

        nonDisplayedSubtitles.pop_front();
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

// adds a new subtitle to be shown
// FIXME: Need to fix subtitles to use a 64bit timestamp
void NuppelVideoPlayer::AddSubtitle(const AVSubtitle &subtitle) 
{
    subtitleLock.lock();
    nonDisplayedSubtitles.push_back(subtitle);
    subtitleLock.unlock();
}

/** \fn NuppelVideoPlayer::SetDecoder(DecoderBase*)
 *  \brief Sets the stream decoder, deleting any existing recorder.
 */
void NuppelVideoPlayer::SetDecoder(DecoderBase *dec)
{
    //VERBOSE(VB_IMPORTANT, "SetDecoder("<<dec<<") was "<<decoder);
    if (!decoder)
        decoder = dec;
    else
    {
        DecoderBase *d = decoder;
        decoder = dec;
        delete d;
    }
}

void NuppelVideoPlayer::ChangeDVDTrack(bool ffw)
{
    if (!ringBuffer->isDVD())
       return;

    GetDecoder()->ChangeDVDTrack(ffw);
    usleep(100000);
    ClearAfterSeek();
}

