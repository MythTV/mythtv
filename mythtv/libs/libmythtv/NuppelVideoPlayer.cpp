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

#ifdef USING_XVMC
#include "videoout_xvmc.h" // for hasIDCTAcceleration() call
#endif

#ifdef CONFIG_DARWIN
extern "C" {
int isnan(double);
}
#endif

NuppelVideoPlayer::NuppelVideoPlayer(MythSqlDatabase *ldb,
                                     ProgramInfo *info)
    : decoder_lock(true)
{
    m_db = ldb;
    m_playbackinfo = NULL;
    parentWidget = NULL;

    if (info)
        m_playbackinfo = new ProgramInfo(*info);

    playing = false;
    decoder_thread_alive = true;
    filename = "output.nuv";

    prebuffering_lock.lock();
    prebuffering = false;
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

    commDetect = NULL;

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

    osdtheme = "none";

    disablevideo = disableaudio = false;

    setpipplayer = pipplayer = NULL;
    needsetpipplayer = false;

    videoFilterList = "";
    videoFilters = NULL;
    FiltMan = new FilterManager;

    videoOutput = NULL;
    watchingrecording = false;

    exactseeks = false;

    autocommercialskip = 0;
    commercialskipmethod = gContext->GetNumSetting("CommercialSkipMethod",
                                                   COMM_DETECT_BLANKS);
    commrewindamount = gContext->GetNumSetting("CommRewindAmount",0);
    commnotifyamount = gContext->GetNumSetting("CommNotifyAmount",0);

    m_DeintSetting = gContext->GetNumSetting("Deinterlace", 0);

    timedisplay = NULL;
    seekamount = 30;
    seekamountpos = 4;
    deleteframe = 0;
    hasdeletetable = false;
    hasblanktable = false;
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
}

NuppelVideoPlayer::~NuppelVideoPlayer(void)
{
    if (audioOutput)
        delete audioOutput;

    if (m_playbackinfo)
        delete m_playbackinfo;
    if (weMadeBuffer)
        delete ringBuffer;
    if (osd)
        delete osd;
    if (commDetect)
        delete commDetect;

    for (int i = 0; i < MAXTBUFFER; i++)
    {
        if (txtbuffers[i].buffer)
            delete [] txtbuffers[i].buffer;
    }

    if (decoder)
        delete decoder;

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
    if (decoder)
        decoder->setWatchingRecording(mode);
}

void NuppelVideoPlayer::SetRecorder(RemoteEncoder *recorder)
{
    nvr_enc = recorder;
    if (decoder)
        decoder->setRecorder(recorder);
}

void NuppelVideoPlayer::Pause(bool waitvideo)
{
    decoder_lock.lock();
    next_play_speed = 0.0;
    next_normal_speed = false;
    decoder_lock.unlock();

    if (!actuallypaused)
    {
        while (!decoderThreadPaused.wait(1000))
            VERBOSE(VB_IMPORTANT, "Waited too long for decoder to pause");
    }

    //cout << "stopping other threads" << endl;
    PauseVideo(waitvideo);
    if (audioOutput)
        audioOutput->Pause(true);
    if (ringBuffer)
        ringBuffer->Pause();

    //cout << "updating frames played" << endl;
    if (disablevideo || forceVideoOutput == kVideoOutput_IVTV)
        decoder->UpdateFramesPlayed();
    else
        framesPlayed = videoOutput->GetFramesPlayed();
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
            VERBOSE(VB_IMPORTANT, "Waited too long for video out to pause");
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
    if (disablevideo)
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
                cerr << "Couldn't find 'tv playback' widget\n";
                widget = window->currentWidget();
                assert(widget);
            }
        }

        videoOutput = VideoOutput::InitVideoOut(forceVideoOutput);

        if (!videoOutput)
        {
            errored = true;
            return false;
        }

        if (gContext->GetNumSetting("DecodeExtraAudio", 0))
            decoder->setLowBuffers();

        if (!videoOutput->Init(video_width, video_height, video_aspect,
                               widget->winId(), 0, 0, widget->width(),
                               widget->height(), 0))
        {
            errored = true;
            return false;
        }

#ifdef USING_XVMC
        // We must tell the AvFormatDecoder whether we are using
        // an IDCT or MC pixel format.
        if (kVideoOutput_XvMC == forceVideoOutput)
        {
            bool idct = videoOutput->hasIDCTAcceleration();
            decoder->SetPixelFormat((idct) ? PIX_FMT_XVMC_MPEG2_IDCT :
                                             PIX_FMT_XVMC_MPEG2_MC);
        }
#endif

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
    videoOutput->InputChanged(video_width, video_height, video_aspect);

    videofiltersLock.lock();
    ReinitOSD();
    videofiltersLock.unlock();

    ClearAfterSeek();
}

void NuppelVideoPlayer::ReinitAudio(void)
{
    if (audioOutput)
    {
        audioOutput->Reconfigure(audio_bits, audio_channels, audio_samplerate);
        audioOutput->SetStretchFactor(audio_stretchfactor);
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
        video_aspect = aspect;

    if (reinit)
        ReinitVideo();

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
            cerr << "Warning: Old player ringbuf creation\n";
            ringBuffer = new RingBuffer(filename, false);
            weMadeBuffer = true;
            livetv = false;
        }
        else
            livetv = ringBuffer->LiveMode();

        if (!ringBuffer->IsOpen())
        {
            fprintf(stderr, "File not found: %s\n",
                    ringBuffer->GetFilename().ascii());
            return -1;
        }
    }

    ringBuffer->Start();
    long long startpos = ringBuffer->GetTotalReadPosition();

    decoder = NULL;

    char testbuf[2048];

    if (ringBuffer->Read(testbuf, 2048) != 2048)
    {
        cerr << "Couldn't read file: " << ringBuffer->GetFilename() << endl;
        return -1;
    }

    ringBuffer->Seek(startpos, SEEK_SET);

    if (decoder)
        delete decoder;

    if (NuppelDecoder::CanHandle(testbuf))
        decoder = new NuppelDecoder(this, m_db, m_playbackinfo);
#ifdef USING_IVTV
    else if (!disablevideo && IvtvDecoder::CanHandle(testbuf,
                                                     ringBuffer->GetFilename()))
    {
        decoder = new IvtvDecoder(this, m_db, m_playbackinfo);
        disableaudio = true; // no audio with ivtv.
        audio_bits = 16;
    }
#endif
    else if (AvFormatDecoder::CanHandle(testbuf, ringBuffer->GetFilename()))
        decoder = new AvFormatDecoder(this, m_db, m_playbackinfo);

    if (!decoder)
    {
        VERBOSE(VB_IMPORTANT, 
                QString("NVP: Couldn't find a matching decoder for: %1").
                arg(ringBuffer->GetFilename()));
        return -1;
    } 
    else if (decoder->IsErrored())
    {
        VERBOSE(VB_IMPORTANT, 
                "NVP: NuppelDecoder encountered error during creation.");
        delete decoder;
        return -1;
    }

    decoder->setExactSeeks(exactseeks);
    decoder->setLiveTVMode(livetv);
    decoder->setWatchingRecording(watchingrecording);
    decoder->setRecorder(nvr_enc);

    eof = 0;
    text_size = 8 * (sizeof(teletextsubtitle) + VT_WIDTH);

    int ret;
    if ((ret = decoder->OpenFile(ringBuffer, disablevideo, testbuf)) < 0)
    {
        cerr << "Couldn't open decoder for: " << ringBuffer->GetFilename()
             << endl;
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

    commDetect = new CommDetect(video_width, video_height, video_frame_rate,
                                commercialskipmethod);

    commDetect->SetAggressiveDetection(
                    gContext->GetNumSetting("AggressiveCommDetect", 1));

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

VideoFrame *NuppelVideoPlayer::GetNextVideoFrame(void)
{
    return videoOutput->GetNextFreeFrame();
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
            cerr << "text buffer overflow\n";
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
            if (!videoOutput->availableVideoBuffersWait()->wait(10))
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

    if (!decoder->GetFrame(onlyvideo))
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
            long long delta = decoder->GetFramesRead() - framesPlayed;
            real_skip = CalcMaxFFTime(ffrew_skip + delta) - delta;
            decoder->DoFastForward(decoder->GetFramesRead() + real_skip, false);
            stop = (CalcMaxFFTime(100) < 100);
        }
        else
        {
            real_skip = (-decoder->GetFramesRead() > ffrew_skip) ? 
                -decoder->GetFramesRead() : ffrew_skip;
            decoder->DoRewind(decoder->GetFramesRead() + real_skip, false);
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
            // TODO: teletext page select
            //if (arg)
            //    vbipagenr = (arg & 0xffff) << 16;
            //msg = QString("%1%2").arg(QObject::tr("TXT pg")).arg(vbipagenr);
            msg = QObject::tr("TXT on");
        }
        else if (mode == 2)
        {
            cc = true;
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
            msg = QString(QObject::tr("CC/TXT disabled"));
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
                    printf("%s\n", (char*) inpos);
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

    if (!disablevideo)
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
    else if (!disablevideo)
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
            if (forceVideoOutput != kVideoOutput_XvMC)
            {
                // XvMC does not need the frame again
                if (buffer)
                    videoOutput->PrepareFrame(buffer, kScan_Intr2ndField);
            }
            // Display the second field
            videosync->AdvanceTrigger();
            videosync->WaitForFrame(0);
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
        // ms, same scale as timecodes
        lastaudiotime = audioOutput->GetAudiotime();
        if (lastaudiotime != 0 && buffer->timecode != 0)
        { // lastaudiotime = 0 after a seek
            // The time at the start of this frame (ie, now) is given by
            // last->timecode
            int delta = buffer->timecode - prevtc - (frame_interval / 1000);
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
    if (disablevideo)
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
                continue;
            }

            videofiltersLock.lock();
            videoOutput->ProcessFrame(NULL, osd, videoFilters, pipplayer);
            videofiltersLock.unlock();

            videoOutput->PrepareFrame(NULL, kScan_Ignore);

            //printf("video waiting for unpause\n");
            usleep(frame_interval);

            videoOutput->Show(kScan_Ignore);
            videosync->Start();
            continue;
        }
        video_actually_paused = false;
        resetvideo = false;

        prebuffering_lock.lock();
        if (prebuffering)
        {
            VERBOSE(VB_PLAYBACK, "waiting for prebuffer...");
            if (!prebuffering_wait.wait(&prebuffering_lock,
                                        frame_interval * 4 / 1000))
                VERBOSE(VB_PLAYBACK, "prebuffer wait timed out..");
            prebuffering_lock.unlock();
            videosync->Start();
            continue;
        }
        prebuffering_lock.unlock();

        if (!videoOutput->EnoughPrebufferedFrames())
        {
            VERBOSE(VB_GENERAL, "prebuffering pause");
            setPrebuffering(true);
            continue;
        }

        videoOutput->StartDisplayingFrame();

        VideoFrame *frame = videoOutput->GetLastShownFrame();

        if (cc)
            ShowText();

        videofiltersLock.lock();
        videoOutput->ProcessFrame(frame, osd, videoFilters, pipplayer);
        videofiltersLock.unlock();

        AVSync();

        videoOutput->DoneDisplayingFrame();

        //sched_yield();
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

    decoder->Reset();
}

void NuppelVideoPlayer::StartPlaying(void)
{
    consecutive_blanks = 0;
    killplayer = false;
    framesPlayed = 0;

    if (OpenFile() < 0)
        return;

    if (!disableaudio || forceVideoOutput == kVideoOutput_IVTV)
    {
        bool setVolume = gContext->GetNumSetting("MythControlsVolume", 1);
        audioOutput = AudioOutput::OpenAudio(audiodevice, audio_bits,
                                             audio_channels, audio_samplerate, 
                                             AUDIOOUTPUT_VIDEO,
                                             setVolume);

        DialogBox *dialog = NULL;
        if (audioOutput == NULL)
        {
            qApp->lock();
            dialog = new DialogBox(gContext->GetMainWindow(),
                                   QObject::tr("Unable to create AudioOutput."));
        }
        else if (audioOutput->GetError() != QString::null)
        {
            qApp->lock();
            dialog = new DialogBox(gContext->GetMainWindow(),
                                   audioOutput->GetError());
        }

        if (dialog != NULL)
        {
            dialog->AddButton(QObject::tr("Continue WITHOUT AUDIO!"));
            dialog->AddButton(QObject::tr("Return to menu."));
            int ret = dialog->exec();
            delete dialog;

            qApp->unlock();

            if (audioOutput)
            {
                delete audioOutput;
                audioOutput = NULL;
            }
            disableaudio = true;

            if (ret == 2)
                return;
        }
    }

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

    if (!disablevideo)
    {
        int dispx = 0, dispy = 0, dispw = video_width, disph = video_height;
        videoOutput->GetVisibleSize(dispx, dispy, dispw, disph);
        osd = new OSD(video_width, video_height, frame_interval,
                      osdfontname, osdccfontname, osdprefix, osdtheme,
                      dispx, dispy, dispw, disph);

        if (forceVideoOutput != kVideoOutput_Default)
            osd->DisableFade();
    }

    playing = true;

    rewindtime = fftime = 0;
    skipcommercials = 0;

    // in the player, pretend we use Blank-frame detection so we can
    // utilize the blank-frame info from the recorder if no commercial
    // skip list exists.
    if (commDetect)
        commDetect->SetCommDetectMethod(COMM_DETECT_BLANKS);

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

    if (!disablevideo)
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

        decoder->setExactSeeks(false);

        fftime = bookmarkseek;
        DoFastForward();
        fftime = 0;

        decoder->setExactSeeks(seeks);

        QMutexLocker lockit(m_db->mutex());

        if (gContext->GetNumSetting("ClearSavedPosition", 1))
            m_playbackinfo->SetBookmark(0, m_db->db());
    }

    LoadBlankList();
    if (!blankMap.isEmpty())
    {
        hasblanktable = true;
        blankIter = blankMap.begin();
    }

    LoadCommBreakList();
    if (!commBreakMap.isEmpty())
    {
        hascommbreaktable = true;
        commBreakIter = commBreakMap.begin();
    }

    SetCommBreakIter();

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

        if (disablevideo || forceVideoOutput == kVideoOutput_IVTV)
            decoder->UpdateFramesPlayed();
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

    killvideo = true;
    pthread_join(output_video, NULL);

    // need to make sure video has exited first.
    if (audioOutput)
        delete audioOutput;
    audioOutput = NULL;

/*
    if (!disablevideo)
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
                    cerr << "Couldn't allocate warped audio buffer!" << endl;
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
                    cerr << "Couldn't allocate left warped audio buffer!\n";
                    return;
                }
                warplbuff = newlbuffer;

                newrbuffer = (short int *)realloc(warprbuff, newlen);
                if (!newrbuffer)
                {
                    cerr << "Couldn't allocate right warped audio buffer!\n";
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
            VERBOSE(VB_IMPORTANT, "Audio buffer overflow, audio data lost!");
    }
}

void NuppelVideoPlayer::SetBookmark(void)
{
    if (livetv)
        return;
    if (!m_db || !m_playbackinfo || !osd)
        return;

    QMutexLocker lockit(m_db->mutex());

    m_playbackinfo->SetBookmark(framesPlayed, m_db->db());
    osd->SetSettingsText(QObject::tr("Position Saved"), 1);
}

void NuppelVideoPlayer::ClearBookmark(void)
{
    if (livetv)
        return;
    if (!m_db || !m_playbackinfo || !osd)
        return;

    QMutexLocker lockit(m_db->mutex());

    m_playbackinfo->SetBookmark(0, m_db->db());
    osd->SetSettingsText(QObject::tr("Position Cleared"), 1);
}

long long NuppelVideoPlayer::GetBookmark(void)
{
    if (!m_db || !m_playbackinfo)
        return 0;

    QMutexLocker lockit(m_db->mutex());
    return m_playbackinfo->GetBookmark(m_db->db());
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

        decoder->setExactSeeks(exactseeks && ffrew_skip == 1);
        decoder->DoRewind(framesPlayed);
        ClearAfterSeek();
    }

    float temp_speed = audio_stretchfactor;
    frame_interval = (int)(1000000.0 * ffrew_skip / video_frame_rate / temp_speed);
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

        decoder->setExactSeeks(exactseeks && ffrew_skip == 1);
        decoder->DoRewind(framesPlayed);
        ClearAfterSeek();
    }

    float temp_speed = play_speed;
    frame_interval = (int)(1000000.0 * ffrew_skip / video_frame_rate / temp_speed);
    if (osd && forceVideoOutput != kVideoOutput_IVTV)
        osd->SetFrameInterval(frame_interval);
    if (videoOutput && videosync != NULL)
    {
        // If using bob deinterlace, turn on or off if we
        // changed to or from synchronous playback speed.

        videofiltersLock.lock();
        if (m_double_framerate)
        {
            if (!normal_speed || play_speed < 0.99 || play_speed > 1.01)
            {
                m_double_framerate = false;
                m_scan = kScan_Ignore;
                videosync->SetFrameInterval(frame_interval, false);
                videoOutput->SetupDeinterlace(false);
            }
        }
        else if (m_can_double && !m_double_framerate)
        {
            if (normal_speed && play_speed > 0.99 && play_speed < 1.01)
            {
                m_scan = kScan_Interlaced;
                videosync->SetFrameInterval(frame_interval, true);
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
        decoder->setExactSeeks(true);
    decoder->DoRewind(desiredFrame);
    decoder->setExactSeeks(exactseeks);

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
        decoder->setExactSeeks(true);
    decoder->DoFastForward(desiredFrame);
    decoder->setExactSeeks(exactseeks);

    ClearAfterSeek();
    return true;
}

void NuppelVideoPlayer::JumpToFrame(long long frame)
{
    bool exactstore = exactseeks;

    decoder->setExactSeeks(true);
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

    decoder->setExactSeeks(exactstore);
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
    SetBlankIter();
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

void NuppelVideoPlayer::SetBlankIter(void)
{
    blankIter = blankMap.begin();
    if (hasblanktable)
    {
        while (blankIter != blankMap.end())
        {
            if ((framesPlayed + 2) > blankIter.key())
            {
                ++blankIter;
            }
            else
                break;
        }

        if (blankIter != blankMap.begin())
            --blankIter;
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

    if (!hasFullPositionMap || !m_playbackinfo || !m_db || !osd)
        return false;

    bool alreadyediting = false;
    m_db->lock();
    alreadyediting = m_playbackinfo->IsEditing(m_db->db());
    m_db->unlock();

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
    m_db->lock();
    m_playbackinfo->ToMap(m_db->db(), infoMap);
    m_db->unlock();
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

    m_db->lock();
    m_playbackinfo->SetEditing(true, m_db->db());
    m_db->unlock();

    return editmode;
}

void NuppelVideoPlayer::DisableEdit(void)
{
    editmode = false;

    if (!m_playbackinfo || !m_db)
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

    QMutexLocker lockit(m_db->mutex());
    m_playbackinfo->SetEditing(false, m_db->db());
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
    decoder->setExactSeeks(true);

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

    decoder->setExactSeeks(exactstore);
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
            decoder->setExactSeeks(false);
            fftime = (long long)(keyframedist * 1.1);
            while (fftime > 0)
                usleep(50);
        }
        else
        {
            decoder->setExactSeeks(false);
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
    if (!m_playbackinfo || !m_db)
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

    QMutexLocker lockit(m_db->mutex());
    m_playbackinfo->SetMarkupFlag(MARK_UPDATED_CUT, true, m_db->db());
    m_playbackinfo->SetCutList(deleteMap, m_db->db());
}

void NuppelVideoPlayer::LoadCutList(void)
{
    if (!m_playbackinfo || !m_db)
        return;

    QMutexLocker lockit(m_db->mutex());
    m_playbackinfo->GetCutList(deleteMap, m_db->db());
}

void NuppelVideoPlayer::LoadBlankList(void)
{
    if (!m_playbackinfo || !m_db)
        return;

    QMutexLocker lockit(m_db->mutex());
    m_playbackinfo->GetBlankFrameList(blankMap, m_db->db());
}

void NuppelVideoPlayer::LoadCommBreakList(void)
{
    if (!m_playbackinfo || !m_db)
        return;

    QMutexLocker lockit(m_db->mutex());
    m_playbackinfo->GetCommBreakList(commBreakMap, m_db->db());
}

char *NuppelVideoPlayer::GetScreenGrab(int secondsin, int &bufflen, int &vw,
                                       int &vh)
{
    disablevideo = true;

    if (OpenFile() < 0)
        return NULL;
    if (!decoder)
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

    int number = (int)(secondsin * video_frame_rate);
    if (number >= totalFrames)
        number = totalFrames / 2;

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
        return NULL;
    }

    unsigned char *data = frame->buf;

    if (!data)
    {
        bufflen = 0;
        vw = vh = 0;
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

    return (char *)outputbuf;
}

QString NuppelVideoPlayer::GetEncodingType(void)
{
    return decoder->GetEncodingType();
}

bool NuppelVideoPlayer::GetRawAudioState(void)
{
    return decoder->GetRawAudioState();
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
        decoder->SetRawVideoState(true);
    if (copyaudio)
        decoder->SetRawAudioState(true);

    decoder->setExactSeeks(true);
}

bool NuppelVideoPlayer::TranscodeGetNextFrame(QMap<long long, int>::Iterator &dm_iter,
                                              int *did_ff, bool *is_key, bool honorCutList)
{
    if (dm_iter == NULL && honorCutList)
        dm_iter = deleteMap.begin();
    
    if (!decoder->GetFrame(0))
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
                decoder->DoFastForward(dm_iter.key());
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
    *is_key = decoder->isLastFrameKey();
    return true;
}

long NuppelVideoPlayer::UpdateStoredFrameNum(long curFrameNum)
{
    return decoder->UpdateStoredFrameNum(curFrameNum);
}
bool NuppelVideoPlayer::WriteStoredData(RingBuffer *outRingBuffer, bool writevideo)
{
    if (writevideo && !decoder->GetRawVideoState())
        writevideo = false;
    decoder->WriteStoredData(outRingBuffer, writevideo);
    return writevideo;
}
bool NuppelVideoPlayer::LastFrameIsBlank(void)
{
    return FrameIsBlank(videoOutput->GetLastDecodedFrame());
}

int NuppelVideoPlayer::FlagCommercials(bool showPercentage, bool fullSpeed,
                                       int *controlFlag, bool inJobQueue)
{
    (void)controlFlag;
    int comms_found = 0;
    int percentage = 0;
    int jobID = -1;
    bool flaggingPaused = false;
    int flagFPS = 0;
    float elapsed = 0.0;
    VideoFrame *currentFrame;

    killplayer = false;
    framesPlayed = 0;

    blankMap.clear();
    commBreakMap.clear();

    disablevideo = true;

    if (OpenFile() < 0)
        return(254);

    m_db->lock();
    m_playbackinfo->SetCommFlagged(COMM_FLAG_PROCESSING, m_db->db());
    m_db->unlock();

    if (inJobQueue)
    {
        m_db->lock();
        jobID = JobQueue::GetJobID(m_db->db(), JOB_COMMFLAG,
                                  m_playbackinfo->chanid,
                                  m_playbackinfo->startts);
        JobQueue::ChangeJobStatus(m_db->db(), jobID, JOB_RUNNING,
                                  "0% " + QObject::tr("Completed"));
        m_db->unlock();
    }

    playing = true;

    if (!InitVideo())
    {
        VERBOSE(VB_IMPORTANT, "NVP: Unable to initialize video for FlagCommercials.");
        playing = false;
        m_db->lock();
        m_playbackinfo->SetCommFlagged(COMM_FLAG_NOT_FLAGGED, m_db->db());
        m_db->unlock();
        return(254);
    }

    for (int i = 0; i < MAXTBUFFER; i++)
        txtbuffers[i].buffer = new unsigned char[text_size];

    ClearAfterSeek();

    commDetect->SetCommSkipAllBlanks(
        gContext->GetNumSetting("CommSkipAllBlanks", 1));

    if (commercialskipmethod & COMM_DETECT_LOGO)
    {
        if (inJobQueue)
        {
            m_db->lock();
            JobQueue::ChangeJobStatus(m_db->db(), jobID, JOB_RUNNING,
                                      QObject::tr("Searching for Logo"));
            m_db->unlock();
        }

        if (showPercentage)
        {
            printf( "Finding Logo" );
            fflush( stdout );
        }
        commDetect->SearchForLogo(this, fullSpeed);
        if (showPercentage)
        {
            printf( "\b\b\b\b\b\b\b\b\b\b\b\b            "
                    "\b\b\b\b\b\b\b\b\b\b\b\b" );
            fflush( stdout );
        }
    }


    QTime flagTime;
    flagTime.start();

    // the meat of the offline commercial detection code, scan through whole
    // file looking for indications of commercial breaks
    GetFrame(1,true);
    if (showPercentage)
    {
        if (totalFrames)
            printf( "%3d%%/      ", 0 );
        else
            printf( "%6lld", 0LL );
    }

    while (!eof)
    {
        currentFrame = videoOutput->GetLastDecodedFrame();

        if ((jobID != -1) && ((currentFrame->frameNumber % 500) == 0))
        {
            int curCmd;

            m_db->lock();
            curCmd = JobQueue::GetJobCmd(m_db->db(), jobID);
            m_db->unlock();

            if (curCmd == JOB_STOP)
            {
                playing = false;
                killplayer = true;

                m_db->lock();
                m_playbackinfo->SetCommFlagged(COMM_FLAG_NOT_FLAGGED, m_db->db());
                m_db->unlock();

                return(255);
            }
            else if (curCmd == JOB_PAUSE)
            {
                if (!flaggingPaused && jobID != -1)
                {
                    m_db->lock();
                    JobQueue::ChangeJobStatus(m_db->db(), jobID, JOB_PAUSED,
                                              QObject::tr("Paused"));
                    m_db->unlock();

                    flaggingPaused = true;
                }
                sleep(1);
                continue;
            }
        }

        if (flaggingPaused && jobID != -1)
        {
            m_db->lock();
            JobQueue::ChangeJobStatus(m_db->db(), jobID, JOB_RUNNING,
                                      QObject::tr("Restarting"));
            JobQueue::ChangeJobFlags(m_db->db(), jobID, JOB_RUN);
            m_db->unlock();

            flaggingPaused = false;
        }

        // sleep a little so we don't use all cpu even if we're niced
        if (!fullSpeed)
            usleep(10000);

        if (((jobID != -1) && ((currentFrame->frameNumber % 500) == 0)) ||
            ((showPercentage) && (currentFrame->frameNumber % 100) == 0))
        {
            elapsed = flagTime.elapsed() / 1000.0;

            if (elapsed)
                flagFPS = (int)(currentFrame->frameNumber / elapsed);
            else
                flagFPS = 0;

            if (totalFrames)
                percentage = currentFrame->frameNumber * 100 / totalFrames;
            else
                percentage = 0;

            if (showPercentage)
            {
                if (totalFrames)
                {
                    printf( "\b\b\b\b\b\b\b\b\b\b\b" );
                    printf( "%3d%%/%3dfps", percentage, flagFPS );
                }
                else
                {
                    printf( "\b\b\b\b\b\b" );
                    printf( "%6lld", currentFrame->frameNumber );
                }
                fflush( stdout );
            }

            if ((jobID != -1) && ((currentFrame->frameNumber % 500) == 0))
            {
                m_db->lock();
                JobQueue::ChangeJobComment(m_db->db(), jobID,
                                          QObject::tr("%1% Completed @ %2 fps.")
                                                      .arg(percentage)
                                                      .arg(flagFPS));
                m_db->unlock();
            }
        }

        commDetect->ProcessNextFrame(currentFrame, currentFrame->frameNumber);

        GetFrame(1,true);
    }

    if (showPercentage)
    {
        elapsed = flagTime.elapsed() / 1000.0;

        if (elapsed)
            flagFPS = (int)(currentFrame->frameNumber / elapsed);
        else
            flagFPS = 0;

        if (totalFrames)
            printf( "\b\b\b\b\b\b      \b\b\b\b\b\b" );
        else
            printf( "\b\b\b\b\b\b\b\b\b\b\b           \b\b\b\b\b\b\b\b\b\b\b" );
    }

    if (commercialskipmethod & COMM_DETECT_BLANKS)
    {
        commDetect->GetBlankFrameMap(blankMap);

        if (blankMap.size())
        {
            m_db->lock();
            m_playbackinfo->SetBlankFrameList(blankMap, m_db->db());
            m_db->unlock();
        }
    }

    commDetect->GetCommBreakMap(commBreakMap);

    if (commBreakMap.size())
    {
        m_db->lock();
        m_playbackinfo->SetMarkupFlag(MARK_UPDATED_CUT, true, m_db->db());
        m_playbackinfo->SetCommBreakList(commBreakMap, m_db->db());
        m_db->unlock();
    }
    comms_found = commBreakMap.size() / 2;

    playing = false;
    killplayer = true;

    m_db->lock();

    if (!hasFullPositionMap)
        decoder->SetPositionMap();

    m_playbackinfo->SetCommFlagged(COMM_FLAG_DONE, m_db->db());

    if (jobID != -1)
    {
        flagFPS = 0;
        elapsed = flagTime.elapsed() / 1000.0;

        if (elapsed)
            flagFPS = (int)(totalFrames / elapsed);

        JobQueue::ChangeJobStatus(m_db->db(), jobID, JOB_STOPPING,
                                  QString("100% ") +
                                  QObject::tr("Completed, %1 FPS").arg(flagFPS)
                                  + ", " + QString("%1").arg(comms_found) + " "
                                  + QObject::tr("Commercial Breaks Found"));
    }

    m_db->unlock();

    return(comms_found);
}

bool NuppelVideoPlayer::RebuildSeekTable(bool showPercentage, StatusCallback cb, void* cbData)
{
    int percentage = 0;
    long long myFramesPlayed = 0;
    bool looped = false;

    killplayer = false;
    framesPlayed = 0;
    disablevideo = true;

    if (OpenFile() < 0)
        return(0);

    // clear out any existing seektables
    m_db->lock();
    m_playbackinfo->ClearPositionMap(MARK_KEYFRAME, m_db->db());
    m_playbackinfo->ClearPositionMap(MARK_GOP_START, m_db->db());
    m_playbackinfo->ClearPositionMap(MARK_GOP_BYFRAME, m_db->db());
    m_db->unlock();

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

    m_db->lock();
    decoder->SetPositionMap();
    m_db->unlock();

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

bool NuppelVideoPlayer::FrameIsBlank(VideoFrame *frame)
{
    if (commercialskipmethod & COMM_DETECT_BLANKS)
    {
        commDetect->ProcessNextFrame(frame);

        return commDetect->FrameIsBlank();
    }
    else
    {
        return false;
    }
}

void NuppelVideoPlayer::AutoCommercialSkip(void)
{
    if (hascommbreaktable)
    {
        if (commBreakIter.data() == MARK_COMM_END)
            commBreakIter++;

        if (commBreakIter == commBreakMap.end())
            return;

        if ((commBreakIter.data() == MARK_COMM_START) &&
            (((autocommercialskip == 1) &&
              (framesPlayed >= commBreakIter.key())) ||
             ((autocommercialskip == 2) &&
              (framesPlayed >= commBreakIter.key())) ||
             ((autocommercialskip == 3) &&
              (framesPlayed + commnotifyamount * video_frame_rate >=
               commBreakIter.key()))))
        {
            ++commBreakIter;
            if (commBreakIter == commBreakMap.end())
                return;

            if (commBreakIter.data() == MARK_COMM_START)
                return;

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
}

void NuppelVideoPlayer::SkipCommercialsByBlanks(void)
{
    int scanned_frames;
    int blanks_found;
    int found_blank_seq;
    int first_blank_frame;
    int saved_position;
    int min_blank_frame_seq = 1;
    int comm_lengths[] = { 30, 15, 60, 0 }; // commercial lengths
    int comm_window = 2;                    // seconds to scan for break

    if ((hasblanktable) && (!hascommbreaktable))
    {
        commDetect->ClearAllMaps();
        commDetect->SetBlankFrameMap(blankMap);
        commDetect->GetCommBreakMap(commBreakMap);
        if (!commBreakMap.isEmpty())
        {
            hascommbreaktable = true;
            DoSkipCommercials(1);
            return;
        }
    }

    saved_position = framesPlayed;

    // rewind 2 seconds in case user hit Skip right after a break
    JumpToNetFrame((long long int)(-2 * video_frame_rate));

    // scan through up to 64 seconds to find first break
    // 64 == 2 seconds we rewound, plus up to 62 seconds to find first break
    scanned_frames = blanks_found = found_blank_seq = first_blank_frame = 0;

    GetFrame(1, true);
    while (!eof && (scanned_frames < (64 * video_frame_rate)) && !killplayer)
    {
        if (LastFrameIsBlank())
        {
            blanks_found++;
            if (!first_blank_frame)
                first_blank_frame = framesPlayed;
            if (blanks_found >= min_blank_frame_seq)
                break;
        }
        else if (blanks_found)
        {
            blanks_found = 0;
            first_blank_frame = 0;
        }
        if (SkipTooCloseToEnd(min_blank_frame_seq - blanks_found))
        {
            JumpToFrame(saved_position);
            if (osd)
                osd->EndPause();
            return;
        }

        scanned_frames++;
        GetFrame(1, true);
    }

    if (killplayer)
        return;

    if (!first_blank_frame)
    {
        JumpToFrame(saved_position);
        if (osd)
            osd->EndPause();
        return;
    }

    // if we make it here, then a blank was found
    int blank_seq_found = 0;
    int commercials_found = 0;
    long long starting_pos = framesPlayed;
    do
    {
        if (killplayer)
            break;

        int *comm_length = comm_lengths;

        blank_seq_found = 0;
        saved_position = framesPlayed;
        while (*comm_length && !killplayer)
        {
            long long int new_frame = saved_position +
                       (long long int)((*comm_length - 1) * video_frame_rate);
            if (SkipTooCloseToEnd(new_frame))
            {
                comm_length++;
                continue;
            }

            JumpToFrame(new_frame);

            scanned_frames = blanks_found = found_blank_seq = 0;
            first_blank_frame = 0;
            while (scanned_frames < (comm_window * video_frame_rate) &&
                   !killplayer)
            {
                GetFrame(1, true);
                if (LastFrameIsBlank())
                {
                    blanks_found++;
                    if (!first_blank_frame)
                        first_blank_frame = framesPlayed;
                    if (blanks_found >= min_blank_frame_seq)
                        break;
                }
                else if (blanks_found)
                {
                    blanks_found = 0;
                    first_blank_frame = 0;
                }

                if (SkipTooCloseToEnd(min_blank_frame_seq - blanks_found))
                {
                    JumpToFrame(saved_position);
                    return;
                }

                scanned_frames++;
            }

            if (blanks_found >= min_blank_frame_seq)
            {
                if (osd)
                {
                    QString comm_msg;
                    QString desc;
                    int spos = calcSliderPos(desc);

                    blank_seq_found = 1;

                    commercials_found++;
                    comm_msg = QString(QObject::tr("Found %1 sec. commercial"))
                                      .arg(*comm_length);

                    osd->StartPause(spos, false, comm_msg, desc, 5);
                }
                break;
            }

            comm_length++;
        }
    } while ((blank_seq_found) &&
             ((framesPlayed - starting_pos) < (300 * video_frame_rate)));

    if (killplayer)
        return;

    if (osd)
        osd->EndPause();

    JumpToFrame(saved_position);

    // skip all blank frames so we get right to the show
    GetFrame(1, true);
    while (LastFrameIsBlank() && !killplayer)
        GetFrame(1, true);
}

bool NuppelVideoPlayer::DoSkipCommercials(int direction)
{
    if (( !hasblanktable && !hascommbreaktable) ||
        (livetv) ||
        (watchingrecording && nvr_enc && nvr_enc->IsValidRecorder()))
    {
        hasblanktable = false;
        hascommbreaktable = false;
        LoadBlankList();
        if (!blankMap.isEmpty())
        {
            hasblanktable = true;
            blankIter = blankMap.begin();
            commBreakMap.clear();
        }
    }

    if (( ! hascommbreaktable) &&
        (hasblanktable))
    {
        commDetect->ClearAllMaps();
        commDetect->SetBlankFrameMap(blankMap);
        commDetect->GetCommBreakMap(commBreakMap);
        if (!commBreakMap.isEmpty())
            hascommbreaktable = true;
    }

    if (!hascommbreaktable && !tryunflaggedskip)
    {
        QString desc = "";
        int pos = calcSliderPos(desc);
        QString mesg = QObject::tr("Not Flagged");
        if (osd)
            osd->StartPause(pos, false, mesg, desc, 2);
        return false;
    }

    if (hascommbreaktable)
    {
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
            return true;
        }

        if ((commBreakIter == commBreakMap.end()) &&
            (direction > 0))
        {
            QString comm_msg = QString(QObject::tr("At End, can not Skip."));
            QString desc;
            int spos = calcSliderPos(desc);
            if (osd)
                osd->StartPause(spos, false, comm_msg, desc, 2);
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
                    skipped_seconds = (int)((0 -
                                             framesPlayed) / video_frame_rate);
                    int spos = calcSliderPos(desc);
                    if (osd)
                        osd->StartPause(spos, false, comm_msg, desc, 2);

                    JumpToFrame(0);
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

        GetFrame(1, true);
        if (gContext->GetNumSetting("CommSkipAllBlanks",0))
            while (LastFrameIsBlank() && !eof && !killplayer)
                GetFrame(1, true);

        return true;
    }

    if (commercialskipmethod & COMM_DETECT_BLANKS)
        SkipCommercialsByBlanks();
    else if (osd)
        osd->EndPause();

    return true;
}



void NuppelVideoPlayer::incCurrentAudioTrack()
{
    if (decoder)
        decoder->incCurrentAudioTrack();
}

void NuppelVideoPlayer::decCurrentAudioTrack()
{
    if (decoder)
        decoder->decCurrentAudioTrack();
}

bool NuppelVideoPlayer::setCurrentAudioTrack(int trackNo)
{
    if (decoder)
        return decoder->setCurrentAudioTrack(trackNo);
    else
        return false;
}


int NuppelVideoPlayer::getCurrentAudioTrack()
{
    if (decoder)
        return decoder->getCurrentAudioTrack() + 1;
    else
        return 0;
}

