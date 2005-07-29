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

#include <iostream>
using namespace std;

#include "config.h"
#include "mythdbcon.h"
#include "dialogbox.h"
#include "NuppelVideoPlayer.h"
#include "audiooutput.h"
#include "recordingprofile.h"
#include "osdtypes.h"
#include "remoteutil.h"
#include "programinfo.h"
#include "mythcontext.h"
#include "fifowriter.h"
#include "filtermanager.h"

#include "decoderbase.h"
#include "nuppeldecoder.h"
#include "avformatdecoder.h"
#include "jobqueue.h"

#include "NuppelVideoRecorder.h"

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

#ifdef CONFIG_DARWIN
extern "C" {
int isnan(double);
}
#endif

NuppelVideoPlayer::NuppelVideoPlayer(const ProgramInfo *info)
    : decoder_lock(true)
{
    m_playbackinfo = NULL;
    parentWidget = NULL;

    if (info)
        m_playbackinfo = new ProgramInfo(*info);

    playing = false;
    decoder_thread_alive = true;
    filename = "output.nuv";

    prebuffering_lock.lock();
    prebuffering = false;
    prebuffer_tries = 0;
    prebuffering_wait.wakeAll();
    prebuffering_lock.unlock();

    vbimode = ' ';
    QString mypage = gContext->GetSetting("VBIpageNr", "888");
    bool valid = false;
    if (mypage)
        vbipagenr = mypage.toInt(&valid, 16) << 16;
    else
        vbipagenr = 0x08880000;
    video_height = 0;
    video_width = 0;
    video_size = 0;
    text_size = 0;
    video_aspect = 1.33333;
    m_scan = kScan_Detect;
    m_double_framerate = false;
    m_can_double = false;
    video_frame_rate = 29.97;
    prevtc = 0;

    forceVideoOutput = kVideoOutput_Default;
    decoder = NULL;
    transcoding = false;

    bookmarkseek = 0;

    eof = 0;

    keyframedist = 30;

    wtxt = rtxt = 0;

    embedid = 0;
    embx = emby = embw = embh = -1;

    nvr_enc = NULL;

    paused = false;
    actuallypaused = false;
    video_actually_paused = false;

    audiodevice = "/dev/dsp";
    audioOutput = NULL;

    ringBuffer = NULL;
    weMadeBuffer = false;
    osd = NULL;

    audio_bits = -1;
    audio_channels = 2;
    audio_samplerate = 44100;

    audio_stretchfactor = 1.0;

    editmode = false;
    resetvideo = false;

    hasFullPositionMap = false;

    framesPlayed = 0;
    totalLength = 0;
    totalFrames = 0;
    play_speed = 1.0;
    normal_speed = true;
    ffrew_skip = 1;
    next_play_speed = 1.0;
    next_normal_speed = true;
    videobuf_retries = 0;

    using_null_videoout = disableaudio = false;

    setpipplayer = pipplayer = NULL;
    needsetpipplayer = false;

    videoFilterList = "";
    videoFilters = NULL;
    FiltMan = new FilterManager;

    videoOutput = NULL;
    watchingrecording = false;

    exactseeks = false;

    autocommercialskip = 0;
    commrewindamount = gContext->GetNumSetting("CommRewindAmount",0);
    commnotifyamount = gContext->GetNumSetting("CommNotifyAmount",0);
    lastCommSkipDirection = 0;
    lastCommSkipStart = 0;

    m_DeintSetting = gContext->GetNumSetting("Deinterlace", 0);

    timedisplay = NULL;
    seekamount = 30;
    seekamountpos = 4;
    deleteframe = 0;
    hasdeletetable = false;
    hascommbreaktable = false;

    dialogname = "";

    for (int i = 0; i <= MAXTBUFFER; i++)
    {
        txtbuffers[i].len = 0;
        txtbuffers[i].timecode = 0;
        txtbuffers[i].type = 0;
        txtbuffers[i].buffer = NULL;
    }

    killvideo = false;
    pausevideo = false;

    cc = false;
    ccmode = CC_CC1;
    ccline = "";
    cccol = 0;
    ccrow = 0;

    limitKeyRepeat = false;

    warplbuff = NULL;
    warprbuff = NULL;
    warpbuffsize = 0;

    videosync = NULL;

    errored = false;
    audio_timecode_offset = 0;

    osdHasSubtitles = false;
    osdSubtitlesExpireAt = -1;
}

NuppelVideoPlayer::~NuppelVideoPlayer(void)
{
    if (audioOutput)
        delete audioOutput;

    if (m_playbackinfo)
        delete m_playbackinfo;
    if (weMadeBuffer)
        delete ringBuffer;

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

    if (videoOutput)
        delete videoOutput;

    if (videosync)
        delete videosync;
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

void NuppelVideoPlayer::Pause(bool waitvideo)
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
    //cout << "starting other threads" << endl;
    UnpauseVideo();
    if (audioOutput && unpauseaudio)
        audioOutput->Pause(false);
    if (ringBuffer)
        ringBuffer->Unpause();

    decoder_lock.lock();
    next_play_speed = speed;
    next_normal_speed = normal;
    if (normal)
        audio_stretchfactor = speed;
    decoder_lock.unlock();

    return true;
}

bool NuppelVideoPlayer::GetPause(void)
{
    return (actuallypaused &&
            (ringBuffer == NULL || ringBuffer->isPaused()) &&
            (audioOutput == NULL || audioOutput->GetPause()) &&
            GetVideoPause());
}

inline bool NuppelVideoPlayer::GetVideoPause(void)
{
    return video_actually_paused;
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

void NuppelVideoPlayer::setPrebuffering(bool prebuffer)
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
        int dispx = 0, dispy = 0;
        int dispw = video_width, disph = video_height;

        videoOutput->GetVisibleSize(dispx, dispy, dispw, disph);

        osd->Reinit(video_width, video_height, frame_interval,
                    dispx, dispy, dispw, disph);
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

        if (errMsg != QString::null)
        {
            VERBOSE(VB_IMPORTANT, QString("Disabling Audio, reason is: %1")
                    .arg(errMsg));
            if (audioOutput)
            {
                delete audioOutput;
                audioOutput = NULL;
            }
            disableaudio = true;
        }
    }
    else if (audioOutput)
    {
        audioOutput->Reconfigure(audio_bits, audio_channels, audio_samplerate);
        errMsg = audioOutput->GetError();
    }
    if (audioOutput)
        audioOutput->SetStretchFactor(audio_stretchfactor);
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
    if (keyframedistance > 0)
        keyframedist = keyframedistance;
}

void NuppelVideoPlayer::SetVideoParams(int width, int height, double fps,
                                       int keyframedistance, float aspect,
                                       FrameScanType scan, bool reinit)
{
    if (width == 0 || height == 0 || isnan(aspect) || isnan(fps))
        return;

    if (video_width == width && video_height == height && 
        aspect == video_aspect && fps == video_frame_rate)
    {
        return;
    }

    if (width > 0)
        video_width = width;
    if (height > 0)
        video_height = height;
    if (fps > 0 && fps < 120)
    {
        video_frame_rate = fps;
        float temp_speed = (play_speed == 0.0) ? audio_stretchfactor : play_speed;
        frame_interval = (int)(1000000.0 / video_frame_rate / temp_speed);
    }

    video_size = video_height * video_width * 3 / 2;
    if (keyframedistance > 0)
        keyframedist = keyframedistance;

    if (aspect > 0.0f)
    {
        video_aspect = aspect;
    }

    if (reinit)
        ReinitVideo();

    if (IsErrored())
        return;

    videofiltersLock.lock();

    m_scan = detectInterlace(scan, m_scan, video_frame_rate, video_height);
    VERBOSE(VB_PLAYBACK, QString("Interlaced: %1  video_height: %2  fps: %3")
                                .arg(toQString(m_scan)).arg(video_height).arg(fps));

    // Set up deinterlacing in the video output method
    m_double_framerate = false;
    if (videoOutput)
        videoOutput->SetupDeinterlace(false);
    if (m_scan == kScan_Interlaced && m_DeintSetting) {
        if (videoOutput && videoOutput->SetupDeinterlace(true)) {
            if (videoOutput->NeedsDoubleFramerate())
            {
                m_double_framerate = true;
                m_can_double = true;
            }
        }
    }
    // Make sure video sync can do it
    if (videosync != NULL && m_double_framerate)
    {
        videosync->SetFrameInterval(frame_interval, m_double_framerate);
        if (!videosync->isInterlaced()) {
            VERBOSE(VB_IMPORTANT, "Video sync method can't support double "
                    "framerate (refresh rate too low for bob deint)");
            m_scan = kScan_Ignore;
            m_double_framerate = false;
            m_can_double = false;
            if (videoOutput)
                videoOutput->SetupDeinterlace(false);
        }
    }

    videofiltersLock.unlock();
}

void NuppelVideoPlayer::SetFileLength(int total, int frames)
{
    totalLength = total;
    totalFrames = frames;
}

int NuppelVideoPlayer::OpenFile(bool skipDsp)
{
    if (!skipDsp)
    {
        if (!ringBuffer)
        {
            VERBOSE(VB_IMPORTANT, "NVP::OpenFile() Warning, old player exited"
                    "before new ring buffer created");
            ringBuffer = new RingBuffer(filename, false);
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
    else if (!using_null_videoout && IvtvDecoder::CanHandle(testbuf,
                                                     ringBuffer->GetFilename()))
    {
        SetDecoder(new IvtvDecoder(this, m_playbackinfo));
        disableaudio = true; // no audio with ivtv.
        audio_bits = 16;
    }
#endif
    else if (AvFormatDecoder::CanHandle(testbuf, ringBuffer->GetFilename()))
        SetDecoder(new AvFormatDecoder(this, m_playbackinfo, using_null_videoout));

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
    GetDecoder()->setWatchingRecording(watchingrecording);
    GetDecoder()->setRecorder(nvr_enc);
    GetDecoder()->setTranscoding(transcoding);

    eof = 0;
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
        disableaudio = true;

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

VideoFrame *NuppelVideoPlayer::GetNextVideoFrame(bool allow_unsafe)
{
    return videoOutput->GetNextFreeFrame(false, allow_unsafe);
}

void NuppelVideoPlayer::ReleaseNextVideoFrame(VideoFrame *buffer,
                                              long long timecode)
{
    buffer->timecode = timecode;

    videoOutput->ReleaseFrame(buffer);
}

void NuppelVideoPlayer::DiscardVideoFrame(VideoFrame *buffer)
{
    videoOutput->DiscardFrame(buffer);
}

void NuppelVideoPlayer::DiscardVideoFrames()
{
    videoOutput->DiscardFrames();
}

void NuppelVideoPlayer::DrawSlice(VideoFrame *frame, int x, int y, int w, int h)
{
    videoOutput->DrawSlice(frame, x, y, w, h);
}

void NuppelVideoPlayer::AddTextData(char *buffer, int len,
                                    long long timecode, char type)
{
    if (cc)
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

bool NuppelVideoPlayer::GetFrame(int onlyvideo, bool unsafe)
{
#ifdef USING_IVTV
    if (forceVideoOutput != kVideoOutput_IVTV)
#endif
    {
        if (!videoOutput->EnoughFreeFrames() && !unsafe)
        {
            //cout << "waiting for video buffer to drain.\n";
            setPrebuffering(false);
            if (!videoOutput->WaitForAvailable(10))
            {
                if (++videobuf_retries >= 200)
                {
                    VERBOSE(VB_IMPORTANT, "Timed out waiting for free video buffers.");
                    if (videosync==NULL && tryingVideoSync)
                    {
                        VERBOSE(VB_IMPORTANT, "Attempting video sync thread restart");
                        //pthread_create(&output_video, NULL, kickoffOutputVideoLoop, this);
                    }
                    videobuf_retries = 0;
                }
                return false;
            }
        }
        videobuf_retries = 0;
    }

    if (!GetDecoder())
    {
        VERBOSE(VB_IMPORTANT, "NVP::GetFrame() called with NULL decoder.");
        return false;
    }

    if (!GetDecoder()->GetFrame(onlyvideo))
        return false;

#ifdef USING_IVTV
    if (forceVideoOutput != kVideoOutput_IVTV)
#endif
    {
        if (videoOutput->EnoughDecodedFrames())
            setPrebuffering(false);
    }

    if (ffrew_skip != 1)
    {
        bool stop;
        long long real_skip;

        if (ffrew_skip > 0)
        {
            long long delta = GetDecoder()->GetFramesRead() - framesPlayed;
            real_skip = CalcMaxFFTime(ffrew_skip + delta) - delta;
            GetDecoder()->DoFastForward(GetDecoder()->GetFramesRead() + real_skip, false);
            stop = (CalcMaxFFTime(100) < 100);
        }
        else
        {
            real_skip = (-GetDecoder()->GetFramesRead() > ffrew_skip) ? 
                -GetDecoder()->GetFramesRead() : ffrew_skip;
            GetDecoder()->DoRewind(GetDecoder()->GetFramesRead() + real_skip, false);
            stop = framesPlayed <= keyframedist;
        }
        if (stop)
            Play(audio_stretchfactor, true, true);
    }

    return true;
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
        videoOutput->StopEmbedding();
}

void NuppelVideoPlayer::ToggleCC(char mode, int arg)
{
    QString msg;

    vbimode = mode;
    if (cc && !arg)
    {
        cc = false;
        ResetCC();
        if (mode == 1)
            msg = QObject::tr("TXT off");
        else
            msg = QObject::tr("CC off");
    }
    else
    {
        cc = true;
        if (mode == 1)
        {
            if (arg)
                vbipagenr = arg;
            msg = QObject::tr("TXT on");
        }
        else if (mode == 2)
        {
            switch (arg)
            {
            case  0:                   break;  // same mode
            case  2: ccmode = CC_CC2;  break;
            case  3: ccmode = CC_CC3;  break;
            case  4: ccmode = CC_CC4;  break;
            case  5: ccmode = CC_TXT1; break;
            case  6: ccmode = CC_TXT2; break;
            case  7: ccmode = CC_TXT3; break;
            case  8: ccmode = CC_TXT4; break;
            default: ccmode = CC_CC1;
            }

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
    }

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

            if (pagenr == vbipagenr)
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


#define MAXWARPDIFF 0.0005 // Max amount the warpfactor can change in 1 frame
#define WARPMULTIPLIER 1000000000 // How much do we multiply the warp by when
                                  //storing it in an integer
#define WARPAVLEN (video_frame_rate * 600) // How long to average the warp over
#define WARPCLIP    0.1 // How much we allow the warp to deviate from 1
                        // (normal speed)
#define MAXDIVERGE  3   // Maximum number of frames of A/V divergence allowed
                        // before dropping or extending video frames to
                        // compensate
#define DIVERGELIMIT 30 // A/V divergence above this amount is clipped
                        // to avoid bad stream data causing huge pauses

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
    float           diverge;

    VideoFrame *buffer = videoOutput->GetLastShownFrame();
    if (!buffer)
    {
        VERBOSE(VB_IMPORTANT, "No video buffer");
        return;
    }

    if (normal_speed)
        diverge = WarpFactor();
    else
        diverge = 0;

    // If video is way ahead of audio, drop some frames until we're
    // close again.
    if (diverge < -MAXDIVERGE)
    {
        if (diverge < -DIVERGELIMIT)
            diverge = -DIVERGELIMIT;

        VERBOSE(VB_PLAYBACK, QString("A/V diverged by %1 frames, dropping "
            "frame to keep audio in sync").arg(diverge));
        lastsync = true;
    }
    else if (!using_null_videoout)
    {
        if (videoOutput->IsErrored())
        {   // this check prevents calling prepareframe
            VERBOSE(VB_IMPORTANT, "NVP: Error condition detected "
                    "in videoOutput before PrepareFrame(), aborting playback.");
            errored = true;
            return;
        }

        // if we get here, we're actually going to do video output
        if (buffer)
        {
            if (m_double_framerate)
                videoOutput->PrepareFrame(buffer, kScan_Interlaced);
            else
                videoOutput->PrepareFrame(buffer, kScan_Ignore);
        }

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
        if (diverge > DIVERGELIMIT)
            diverge = DIVERGELIMIT;

        // Audio is way ahead of the video - cut the frame rate to
        // half speed until it is within normal range
        VERBOSE(VB_PLAYBACK, QString("A/V diverged by %1 frames, extending "
            "frame to keep audio in sync").arg(diverge));
        avsync_adjustment = frame_interval;
        lastsync = true;
    }

    if (audioOutput && normal_speed)
    {
        long long currentaudiotime = audioOutput->GetAudiotime();
        if (audio_timecode_offset == (long long)0x8000000000000000LL)
            audio_timecode_offset = buffer->timecode - currentaudiotime;

        // ms, same scale as timecodes
        lastaudiotime = currentaudiotime + audio_timecode_offset;
#if 0
        VERBOSE(VB_PLAYBACK, QString("A/V timecodes audio %1 video %2 frameinterval %3 avdel %4 avg %5 tcoffset %6")
                .arg(lastaudiotime)
                .arg(buffer->timecode)
                .arg(frame_interval)
                .arg(buffer->timecode - lastaudiotime)
                .arg(avsync_avg)
                .arg(audio_timecode_offset)
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

            avsync_delay = (buffer->timecode - lastaudiotime) * 1000; // uSecs
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
        VERBOSE(VB_PLAYBACK, "waiting for prebuffer... "<<prebuffer_tries);
        if (!prebuffering_wait.wait(&prebuffering_lock,
                                    frame_interval * 4 / 1000))
        {
            // timed out.. do we need to know?
        }
        ++prebuffer_tries;
        if (prebuffer_tries > 10)
        {
            VERBOSE(VB_IMPORTANT, "Prebuffer wait timed out 10 times.");
            if (!videoOutput->EnoughFreeFrames())
            {
                VERBOSE(VB_IMPORTANT, "Prebuffer wait timed out, and "
                        "not enough free frames. Discarding buffers.");
                DiscardVideoFrames();
            }
            prebuffer_tries = 0;
        }
        prebuffering_lock.unlock();
        videosync->Start();
        return;
    }
    prebuffering_lock.unlock();

    if (!videoOutput->EnoughPrebufferedFrames())
    {
        VERBOSE(VB_GENERAL, "prebuffering pause");
        setPrebuffering(true);
        return;
    }

    prebuffering_lock.lock();
    prebuffer_tries = 0;
    prebuffering_lock.unlock();

    videoOutput->StartDisplayingFrame();

    VideoFrame *frame = videoOutput->GetLastShownFrame();

    if (cc)
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
    lastaudiotime = 0;
    delay = 0;
    avsync_delay = 0;
    avsync_avg = 0;
    avsync_oldavg = 0;

    delay_clipping = false;

    refreshrate = 0;
    lastsync = false;

    usevideotimebase = gContext->GetNumSetting("UseVideoTimebase", 0);

    if ((print_verbose_messages & VB_PLAYBACK) != 0)
        output_jmeter = new Jitterometer("video_output", 100);
    else
        output_jmeter = NULL;

    refreshrate = frame_interval;

    float temp_speed = (play_speed == 0.0) ? audio_stretchfactor : play_speed;
    int fr_int = (int)(1000000.0 / video_frame_rate / temp_speed);
    if (using_null_videoout)
    {
        videosync = new USleepVideoSync(fr_int, 0, false);
    }
    else
    {
        // Set up deinterlacing in the video output method
        m_double_framerate = false;
        if (m_scan == kScan_Interlaced && m_DeintSetting) {
            if (videoOutput && videoOutput->SetupDeinterlace(true)) {
                if (videoOutput->NeedsDoubleFramerate())
                {
                    m_double_framerate = true;
                    m_can_double = true;
                }
            }
        }
        videosync = VideoSync::BestMethod(
            fr_int, videoOutput->GetRefreshRate(), m_double_framerate);
        // Make sure video sync can do it
        if (videosync != NULL && m_double_framerate)
        {
            videosync->SetFrameInterval(frame_interval, m_double_framerate);
            if (!videosync->isInterlaced()) {
                m_scan = kScan_Ignore;
                m_double_framerate = false;
                m_can_double = false;
                if (videoOutput)
                    videoOutput->SetupDeinterlace(false);
            }
        }
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

    vidExitLock.lock();
    delete videoOutput;
    videoOutput = NULL;
    vidExitLock.unlock();

    ShutdownAVSync();
}

#ifdef USING_IVTV
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
            if (cc)
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

    if (fftime == 0)
        fftime = (int)(seconds * video_frame_rate);

    return fftime > CalcMaxFFTime(fftime);
}

bool NuppelVideoPlayer::Rewind(float seconds)
{
    if (!videoOutput)
        return false;

    if (rewindtime == 0)
        rewindtime = (int)(seconds * video_frame_rate);

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
}

void NuppelVideoPlayer::StartPlaying(void)
{
    killplayer = false;
    framesPlayed = 0;

    if (OpenFile() < 0)
        return;

    if (!disableaudio || (forceVideoOutput == kVideoOutput_IVTV &&
                          !gContext->GetNumSetting("PVR350InternalAudioOnly")))
    {
        QString errMsg = ReinitAudio();
        int ret = 1;
        if ((errMsg != QString::null) && gContext->GetNumSetting("AudioNag", 1))
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
        disableaudio = true;
        return;
    }

    if (!using_null_videoout)
    {
        int dispx = 0, dispy = 0, dispw = video_width, disph = video_height;
        videoOutput->GetVisibleSize(dispx, dispy, dispw, disph);
        osd = new OSD(video_width, video_height, frame_interval,
                      dispx, dispy, dispw, disph);

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

    int pausecheck = 0;

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
        commBreakIter = commBreakMap.begin();
    }

    SetCommBreakIter();
    commBreakMapLock.unlock();

    while (!eof && !killplayer && !errored)
    {
        if (nvr_enc && nvr_enc->GetErrorStatus())
        {
            errored = killplayer = true;
            break;
        }

        if (play_speed != next_play_speed)
        {
            decoder_lock.lock();

            play_speed = next_play_speed;
            normal_speed = next_normal_speed;
            //cout << "changing speed to " << play_speed << endl;

            if (play_speed == 0.0)
                DoPause();
            else
                DoPlay();

            decoder_lock.unlock();
        }

        if (rewindtime < 0)
            rewindtime = 0;
        if (fftime < 0)
            fftime = 0;

        if (paused)
        {
            decoderThreadPaused.wakeAll();
            pausecheck++;

            if (!(pausecheck % 20))
            {
                if (livetv && ringBuffer->GetFreeSpace() < -1000)
                {
                    Play();
                    printf("forced unpause\n");
                }
                pausecheck = 0;
            }

            if (rewindtime > 0)
            {
                DoRewind();

                GetFrame(audioOutput == NULL || !normal_speed);
                resetvideo = true;
                while (resetvideo)
                    usleep(50);

                rewindtime = 0;
                continue;
            }
            else if (fftime > 0)
            {
                fftime = CalcMaxFFTime(fftime);
                DoFastForward();

                GetFrame(audioOutput == NULL || !normal_speed);
                resetvideo = true;
                while (resetvideo)
                    usleep(50);

                fftime = 0;
                continue;
            }
            else
            {
                //printf("startplaying waiting for unpause\n");
                usleep(500);
                continue;
            }
        }

        if (rewindtime > 0 && ffrew_skip == 1)
        {
            PauseVideo();

            if (rewindtime >= 1)
                DoRewind();

            UnpauseVideo();
            while (GetVideoPause())
                usleep(50);
            rewindtime = 0;
        }

        if (fftime > 0 && ffrew_skip == 1)
        {
            fftime = CalcMaxFFTime(fftime);
            PauseVideo();

            if (fftime >= 5)
                DoFastForward();

            if (eof)
                continue;

            UnpauseVideo();
            while (GetVideoPause())
                usleep(50);
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
                eof = 1;
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

void NuppelVideoPlayer::AddAudioData(char *buffer, int len, long long timecode)
{
    if (audioOutput)
    {
        if (usevideotimebase)
        {
            int samples;
            short int * newbuffer;
            float incount = 0;
            int outcount;
            int samplesize;
            int newsamples;
            int newlen;

            samplesize = audio_channels * audio_bits / 8;
            samples = len / samplesize;
            newsamples = (int)(samples / warpfactor);
            newlen = newsamples * samplesize;

            // We use the left warp buffer to store the new data.
            // If it isn't big enough it is resized.
            if ((warpbuffsize < newlen) || (! warplbuff))
            {
                if (warprbuff)
                {
                    // Make sure this isn't allocated since we're only
                    // resizing 1 buffer.
                    free(warprbuff);
                    warprbuff = NULL;
                }

                newbuffer = (short int *)realloc(warplbuff, newlen);
                if (!newbuffer)
                {
                    VERBOSE(VB_IMPORTANT, "NVP::AddAudioData: Error, could "
                            "not allocate warped audio buffer!");
                    return;
                }
                warplbuff = newbuffer;
                warpbuffsize = newlen;
            }
            else
                newbuffer = warplbuff;

            for (incount = 0, outcount = 0;
                 (incount < samples) && (outcount < newsamples);
                 outcount++, incount += warpfactor)
            {
                memcpy(((char *)newbuffer) + (outcount * samplesize),
                       buffer + (((int)incount) * samplesize), samplesize);
            }

            samples = outcount;
            if (!audioOutput->AddSamples((char *)newbuffer, samples, timecode))
               VERBOSE(VB_IMPORTANT, "Audio buffer overflow, audio data lost!");
        }
        else
        {
            if (!audioOutput->AddSamples(buffer, len /
                                         (audio_channels * audio_bits / 8),
                                         timecode))
                VERBOSE(VB_IMPORTANT, "Audio buffer overflow, audio data lost!");
        }
    }
}

void NuppelVideoPlayer::AddAudioData(short int *lbuffer, short int *rbuffer,
                                     int samples, long long timecode)
{
    if (audioOutput)
    {
        char *buffers[] = {(char *)lbuffer, (char *)rbuffer};
        if (usevideotimebase)
        {
            short int *newlbuffer;
            short int *newrbuffer;
            float incount = 0;
            int outcount;
            int newlen;
            int newsamples;

            newsamples = (int)(samples / warpfactor);
            newlen = newsamples * sizeof(short int);

            // We resize the buffers if they aren't big enough
            if ((warpbuffsize < newlen) || (!warplbuff) || (!warprbuff))
            {
                newlbuffer = (short int *)realloc(warplbuff, newlen);
                if (!newlbuffer)
                {
                    VERBOSE(VB_IMPORTANT, "NVP::AddAudioData: Error, could "
                            "not allocate left warped audio buffer!");
                    return;
                }
                warplbuff = newlbuffer;

                newrbuffer = (short int *)realloc(warprbuff, newlen);
                if (!newrbuffer)
                {
                    VERBOSE(VB_IMPORTANT, "NVP::AddAudioData: Error, could "
                            "not allocate right warped audio buffer!");
                    return;
                }
                warprbuff = newrbuffer;

                warpbuffsize = newlen;
            }
            else
            {
                newlbuffer = warplbuff;
                newrbuffer = warprbuff;
            }

            buffers[0] = (char *)newlbuffer;
            buffers[1] = (char *)newrbuffer;

            for (incount = 0, outcount = 0;
                 (incount < samples) && (outcount < newsamples);
                 outcount++, incount += warpfactor)
            {
                newlbuffer[outcount] = lbuffer[(int)incount];
                newrbuffer[outcount] = rbuffer[(int)incount];
            }

            samples = outcount;
        }

        if (!audioOutput->AddSamples(buffers, samples, timecode))
            VERBOSE(VB_IMPORTANT, "NVP::AddAudioData: Error, audio "
                    "buffer overflow, audio data lost!");
    }
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

long long NuppelVideoPlayer::GetBookmark(void)
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

    if (play_speed > 0.0 && play_speed <= 3.0)
    {
        skip_changed = (ffrew_skip != 1);
        ffrew_skip = 1;
    }
    else
    {
        skip_changed = true;
        ffrew_skip = int(ceil(4.0 * fabs(play_speed) / keyframedist)) * keyframedist;
        if (play_speed < 0.0)
            ffrew_skip = -ffrew_skip;
    }
    //cout << "skip changed to " << ffrew_skip << endl;

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

    float temp_speed = play_speed;
    frame_interval = (int)(1000000.0 * ffrew_skip / video_frame_rate / temp_speed);
    VERBOSE(VB_PLAYBACK, QString("rate: %1 speed: %2 skip: %3 = interval %4")
                                 .arg(video_frame_rate).arg(temp_speed)
                                 .arg(ffrew_skip).arg(frame_interval));
    if (osd && forceVideoOutput != kVideoOutput_IVTV)
        osd->SetFrameInterval(frame_interval);
    if (videoOutput && videosync != NULL)
    {
        videosync->SetFrameInterval(frame_interval, m_double_framerate);

        // If using bob deinterlace, turn on or off if we
        // changed to or from synchronous playback speed.

        videofiltersLock.lock();
        if (m_double_framerate)
        {
            if (!normal_speed || play_speed < 0.99 || play_speed > 1.01)
            {
                m_double_framerate = false;
                videosync->SetFrameInterval(frame_interval, false);
                videoOutput->SetupDeinterlace(false);
                videoOutput->SetupDeinterlace(true, "onefield");
            }
        }
        else if (m_can_double && !m_double_framerate)
        {
            if (normal_speed && play_speed > 0.99 && play_speed < 1.01)
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
    return true;
}

long long NuppelVideoPlayer::CalcMaxFFTime(long long ff)
{
    long long maxtime = (long long)(1.0 * video_frame_rate);
    if (livetv || (watchingrecording && nvr_enc && nvr_enc->IsValidRecorder()))
        maxtime = (long long)(3.0 * video_frame_rate);

    long long ret = ff;

    limitKeyRepeat = false;

    if (livetv || (watchingrecording && nvr_enc && nvr_enc->IsValidRecorder()))
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

bool NuppelVideoPlayer::DoFastForward(void)
{
    long long number = fftime - 1;
    long long desiredFrame = framesPlayed + number;

    if (paused && !editmode)
        GetDecoder()->setExactSeeks(true);
    GetDecoder()->DoFastForward(desiredFrame);
    GetDecoder()->setExactSeeks(exactseeks);

    ClearAfterSeek();
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

int NuppelVideoPlayer::SkipTooCloseToEnd(int frames)
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

void NuppelVideoPlayer::ClearAfterSeek(void)
{
    videoOutput->ClearAfterSeek();

    for (int i = 0; i < MAXTBUFFER; i++)
        txtbuffers[i].timecode = 0;
    ResetCC();

    wtxt = 0;
    rtxt = 0;

    setPrebuffering(true);
    if (audioOutput)
        audioOutput->Reset();

    if (osd)
        osd->ClearAllCCText();

    SetDeleteIter();
    SetCommBreakIter();
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
    commBreakIter = commBreakMap.begin();
    if (hascommbreaktable)
    {
        while (commBreakIter != commBreakMap.end())
        {
            if ((framesPlayed + 2) > commBreakIter.key())
            {
                commBreakIter++;
            }
            else
                break;
        }
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
        osd->EndPause();

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

int NuppelVideoPlayer::GetLetterbox(void)
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
            GetDecoder()->setExactSeeks(false);
            fftime = (long long)(keyframedist * 1.1);
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

bool NuppelVideoPlayer::IsInDelete(long long testframe)
{
    long long startpos = 0;
    long long endpos = 0;
    bool first = true;
    bool indelete = false;
    bool ret = false;

    QMap<long long, int>::Iterator i;
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
        }

        lastType = it.data();
    }

    return false;
}

char *NuppelVideoPlayer::GetScreenGrab(int secondsin, int &bufflen, int &vw,
                                       int &vh, float &ar)
{
    using_null_videoout = true;

    if (OpenFile() < 0)
        return NULL;
    if (!GetDecoder())
        return NULL;
    if (!hasFullPositionMap)
        return NULL;

    if (!InitVideo())
    {
        VERBOSE(VB_IMPORTANT, "NVP: Unable to initialize video for screen grab.");
        return NULL;
    }

    for (int i = 0; i < MAXTBUFFER; i++)
        txtbuffers[i].buffer = new unsigned char[text_size];

    ClearAfterSeek();

    long long number = (int)(secondsin * video_frame_rate);
    if (number >= totalFrames)
        number = totalFrames / 2;

    previewFromBookmark = gContext->GetNumSetting("PreviewFromBookmark");
    if (previewFromBookmark != 0)
    {
        bookmarkseek = GetBookmark();
        if (bookmarkseek > 30)
            number = bookmarkseek;
    }

    long long oldnumber = number;
    LoadCutList();

    commBreakMapLock.lock();
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
    commBreakMapLock.unlock();

    GetFrame(1, true);

    fftime = number;
    DoFastForward();
    fftime = 0;

    GetFrame(1, true);

    VideoFrame *frame = videoOutput->GetLastDecodedFrame();

    if (!frame)
    {
        bufflen = 0;
        vw = vh = 0;
        ar = 0;
        return NULL;
    }

    unsigned char *data = frame->buf;

    if (!data)
    {
        bufflen = 0;
        vw = vh = 0;
        ar = 0;
        return NULL;
    }

    AVPicture orig, retbuf;
    avpicture_fill(&orig, data, PIX_FMT_YUV420P, video_width, video_height);

    avpicture_deinterlace(&orig, &orig, PIX_FMT_YUV420P, video_width,
                          video_height);

    bufflen = video_width * video_height * 4;
    unsigned char *outputbuf = new unsigned char[bufflen];

    avpicture_fill(&retbuf, outputbuf, PIX_FMT_RGBA32, video_width,
                   video_height);

    img_convert(&retbuf, PIX_FMT_RGBA32, &orig, PIX_FMT_YUV420P,
                video_width, video_height);

    vw = video_width;
    vh = video_height;
    ar = video_aspect;

    return (char *)outputbuf;
}

VideoFrame* NuppelVideoPlayer::GetRawVideoFrame(long long frameNumber)
{
    //todo: let GetScreenGrab() above here use this method to get its framedata.

    if (frameNumber >= 0)
    {
        JumpToFrame(frameNumber);
        ClearAfterSeek();
    }

    GetFrame(1,true);

    return videoOutput->GetLastDecodedFrame();
}

QString NuppelVideoPlayer::GetEncodingType(void)
{
    return GetDecoder()->GetEncodingType();
}

bool NuppelVideoPlayer::GetRawAudioState(void)
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

    if (OpenFile() < 0)
        return(0);

    // clear out any existing seektables
    m_playbackinfo->ClearPositionMap(MARK_KEYFRAME);
    m_playbackinfo->ClearPositionMap(MARK_GOP_START);
    m_playbackinfo->ClearPositionMap(MARK_GOP_BYFRAME);

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

    QTime flagTime;
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

int NuppelVideoPlayer::GetStatusbarPos(void)
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

int NuppelVideoPlayer::GetSecondsBehind(void)
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

int NuppelVideoPlayer::calcSliderPos(QString &desc)
{
    float ret;

    QString text1, text2;

    if (livetv)
    {
        ret = ringBuffer->GetFreeSpace() /
              ((float)ringBuffer->GetFileSize() - ringBuffer->GetSmudgeSize());
        ret *= 1000.0;

        long long written = nvr_enc->GetFramesWritten();
        long long played = framesPlayed;

        if (played > written)
            played = written;
        if (played < 0)
            played = 0;

        int secsbehind = (int)((float)(written - played) / video_frame_rate);

        if (secsbehind < 0)
            secsbehind = 0;

        int hours = (int)secsbehind / 3600;
        int mins = ((int)secsbehind - hours * 3600) / 60;
        int secs = ((int)secsbehind - hours * 3600 - mins * 60);

        if (hours > 0)
        {
            text1.sprintf("%d:%02d:%02d", hours, mins, secs);
            if (osd && osd->getTimeType() == 0)
            {
                text2.sprintf("%.2f%%", (1000 - ret) / 10);
                desc = QObject::tr("%1 behind  --  %2 full")
                                   .arg(text1).arg(text2);
            }
            else
            {
                desc = QObject::tr("%1 behind").arg(text1);
            }
        }
        else
        {
            text1.sprintf("%d:%02d", mins, secs);
            if (osd && osd->getTimeType() == 0)
            {
                text2.sprintf("%.2f%%", (1000 - ret) / 10);
                desc = QObject::tr("%1 behind  --  %2 full")
                        .arg(text1).arg(text2);
            }
            else
            {
                desc = QObject::tr("%1 behind").arg(text1);
            }
        }

        
        return (int)(1000 - ret);
    }
    else if (ringBuffer->isDVD())
    {
        long long rPos = ringBuffer->GetReadPosition();
        long long tPos = ringBuffer->GetTotalReadPosition();
        
        ringBuffer->getDescForPos(desc);
        
        
        if (rPos)
            return (int)((rPos / tPos) * 1000.0);
        else
            return 0;
    }                
    

    int playbackLen;
    if (watchingrecording && nvr_enc && nvr_enc->IsValidRecorder())
        playbackLen =
            (int)(((float)nvr_enc->GetFramesWritten() / video_frame_rate));
    else
        playbackLen = totalLength;

    float secsplayed = ((float)framesPlayed / video_frame_rate);
    if (secsplayed < 0)
        secsplayed = 0;
    if (secsplayed > playbackLen)
        secsplayed = playbackLen;

    ret = secsplayed / (float)playbackLen;
    ret *= 1000.0;

    int phours = (int)secsplayed / 3600;
    int pmins = ((int)secsplayed - phours * 3600) / 60;
    int psecs = ((int)secsplayed - phours * 3600 - pmins * 60);

    int shours = playbackLen / 3600;
    int smins = (playbackLen - shours * 3600) / 60;
    int ssecs = (playbackLen - shours * 3600 - smins * 60);

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

    desc = QObject::tr("%1 of %2").arg(text1).arg(text2);

    return (int)(ret);
}

void NuppelVideoPlayer::AutoCommercialSkip(void)
{
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
            ++commBreakIter;
            if ((commBreakIter == commBreakMap.end()) ||
                (commBreakIter.data() == MARK_COMM_START))
            {
                commBreakMapLock.unlock();
                return;
            }

            if (commBreakIter.key() == totalFrames)
            {
                eof = 1;
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
                    QString desc;
                    int spos = calcSliderPos(desc);
                    osd->StartPause(spos, false, comm_msg, desc, 2);
                }

                if (autocommercialskip == 1)
                {
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
        QString desc = "";
        int pos = calcSliderPos(desc);
        QString mesg = QObject::tr("Not Flagged");
        if (osd)
            osd->StartPause(pos, false, mesg, desc, 2);

        QString message = "COMMFLAG_REQUEST ";
        message += m_playbackinfo->chanid + " " +
                   m_playbackinfo->startts.toString(Qt::ISODate);
        RemoteSendMessage(message);

        return false;
    }

    if ((direction == (0 - lastCommSkipDirection)) &&
        ((time(NULL) - lastCommSkipTime) <= 3))
    {
        QString comm_msg = QString(QObject::tr("Skipping Back."));
        QString desc;
        int spos = calcSliderPos(desc);
        if (osd)
            osd->StartPause(spos, false, comm_msg, desc, 2);

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
        QString comm_msg = QString(QObject::tr("Start of program."));
        QString desc;
        int spos = calcSliderPos(desc);
        if (osd)
            osd->StartPause(spos, false, comm_msg, desc, 2);

        JumpToFrame(0);
        commBreakMapLock.unlock();
        return true;
    }

    if ((direction > 0) &&
        ((commBreakIter == commBreakMap.end()) ||
         ((totalFrames) &&
          ((commBreakIter.key() + (10 * video_frame_rate)) > totalFrames))))
    {
        QString comm_msg = QString(QObject::tr("At End, can not Skip."));
        QString desc;
        int spos = calcSliderPos(desc);
        if (osd)
            osd->StartPause(spos, false, comm_msg, desc, 2);
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
                QString comm_msg = QString(QObject::tr("Start of program."));
                QString desc;
                skipped_seconds = (int)((0 - framesPlayed) / video_frame_rate);
                int spos = calcSliderPos(desc);
                if (osd)
                    osd->StartPause(spos, false, comm_msg, desc, 2);

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
                QString comm_msg =
                            QString(QObject::tr("At End, can not Skip."));
                QString desc;
                int spos = calcSliderPos(desc);
                if (osd)
                    osd->StartPause(spos, false, comm_msg, desc, 2);
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
        QString comm_msg = QString(QObject::tr("Skip %1")).arg(skipTime);
        QString desc;
        int spos = calcSliderPos(desc);
        osd->StartPause(spos, false, comm_msg, desc, 2);
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


int NuppelVideoPlayer::getCurrentAudioTrack()
{
    if (GetDecoder())
        return GetDecoder()->getCurrentAudioTrack() + 1;
    else
        return 0;
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
                        const uint8_t color = rect->bitmap[y*rect->w + x];
                        const uint32_t pixel = rect->rgba_palette[color];
                        qImage.setPixel(x, y, pixel);
                    }
                }

                OSDTypeImage* image = new OSDTypeImage();
                image->SetPosition(QPoint(rect->x, rect->y));
                image->LoadFromQImage(qImage);

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
