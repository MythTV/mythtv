#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#include <cerrno>
#include <sys/time.h>
#include <ctime>
#include <cmath>
#include <qstringlist.h>
#include <qsqldatabase.h>
#include <qmap.h>

#ifdef __linux__
#include <linux/rtc.h>
#endif

#include <sys/ioctl.h>

#include <iostream>
using namespace std;

#include "NuppelVideoPlayer.h"
#include "NuppelVideoRecorder.h"
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

NuppelVideoPlayer::NuppelVideoPlayer(QSqlDatabase *ldb,
                                     ProgramInfo *info)
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
    previously_paused = false;

    audiodevice = "/dev/dsp";
    audioOutput = NULL;

    ringBuffer = NULL;
    weMadeBuffer = false;
    osd = NULL;

    commDetect = NULL;

    audio_bits = 16;
    audio_channels = 2;
    audio_samplerate = 44100;

    editmode = false;
    resetvideo = false;

    hasFullPositionMap = false;

    totalLength = 0;
    totalFrames = 0;
    play_speed = 1.0;
    normal_speed = true;

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
    commercialskipmethod =
        gContext->GetNumSetting("CommercialSkipMethod", 1);

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
    lastccrow = 0;

    limitKeyRepeat = false;

    warplbuff = NULL;
    warprbuff = NULL;
    warpbuffsize = 0;
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
    if (paused)
        return;

    actuallypaused = false;

    // pause the decoder thread first, as it's usually waiting for the video
    // thread.  Make sure that it's done pausing before continuing on.
    paused = true;
    if (!actuallypaused)
    {
        while (!decoderThreadPaused.wait(1000))
            VERBOSE(VB_IMPORTANT, "Waited too long for decoder to pause");
    }

    PauseVideo(waitvideo);
    if (audioOutput)
        audioOutput->Pause(true);

    if (ringBuffer)
        ringBuffer->Pause();

    play_speed = 1.0;
    normal_speed = false;
    frame_interval = (int)(1000000.0 / video_frame_rate / 1.0);
    if (osd && forceVideoOutput != kVideoOutput_IVTV)
        osd->SetFrameInterval(frame_interval);
}

bool NuppelVideoPlayer::Play(float speed, bool normal, bool unpauseaudio)
{
    if (forceVideoOutput == kVideoOutput_IVTV && speed > 3.0)
        return false;

    play_speed = speed;
    normal_speed = normal;
    frame_interval = (int)(1000000.0 / video_frame_rate / speed);
    if (osd && forceVideoOutput != kVideoOutput_IVTV)
        osd->SetFrameInterval(frame_interval);

    if (!paused)
    {
        // Force speed update for IVTV
        previously_paused = true;
        return true;
    }

    paused = false;
    UnpauseVideo();
    if (audioOutput && unpauseaudio)
        audioOutput->Pause(false);

#ifdef USING_DIRECTX
    if (audioOutput && !normal_speed)
        audioOutput->Reset();
#endif

    if (ringBuffer)
        ringBuffer->Unpause();

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

void NuppelVideoPlayer::InitVideo(void)
{
    InitFilters();
    if (disablevideo)
    {
        videoOutput = new VideoOutputNull();
        videoOutput->Init(video_width, video_height, video_aspect,
                          0, 0, 0, 0, 0, 0);
    }
    else
    {
        QWidget *widget = parentWidget;

        if (!widget)
        {
            MythMainWindow *window = gContext->GetMainWindow();
            assert(window);

            QObject *playbackwin = window->child("tv playback");
        
            QWidget *widget = (QWidget *)playbackwin;

            if (!widget)
            {
                cerr << "Couldn't find 'tv playback' widget\n";
                widget = window->currentWidget();
                assert(widget);
            }  
        }

        videoOutput = VideoOutput::InitVideoOut(forceVideoOutput);

        if (gContext->GetNumSetting("DecodeExtraAudio", 0))
            decoder->setLowBuffers();

        videoOutput->Init(video_width, video_height, video_aspect,
                          widget->winId(), 0, 0, widget->width(), 
                          widget->height(), 0);
    }

    if (embedid > 0)
    {
        videoOutput->EmbedInWidget(embedid, embx, emby, embw, embh);
    }
}

void NuppelVideoPlayer::ReinitVideo(void)
{
    InitFilters();
    videoOutput->InputChanged(video_width, video_height, video_aspect);

    if (osd)
    {
        int dispx = 0, dispy = 0;
        int dispw = video_width, disph = video_height;

        videoOutput->GetDrawSize(dispx, dispy, dispw, disph);

        osd->Reinit(video_width, video_height, frame_interval,
                    dispx, dispy, dispw, disph);
    }

    ClearAfterSeek();
}

void NuppelVideoPlayer::ReinitAudio(void)
{
    if (audioOutput)
        audioOutput->Reconfigure(audio_bits, audio_channels, audio_samplerate);
}

void NuppelVideoPlayer::SetVideoParams(int width, int height, double fps, 
                                       int keyframedistance, float aspect)
{
    if (width > 0)
        video_width = width; 
    if (height > 0)
        video_height = height;
    if (fps > 0)
    {
        video_frame_rate = fps;
        frame_interval = (int)(1000000.0 / video_frame_rate / play_speed);
    }

    video_size = video_height * video_width * 3 / 2;
    keyframedist = keyframedistance;

    video_aspect = aspect;
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
    }
#endif
    else if (AvFormatDecoder::CanHandle(testbuf, ringBuffer->GetFilename()))
        decoder = new AvFormatDecoder(this, m_db, m_playbackinfo);

    if (!decoder)
    {
        cerr << "Couldn't find a matching decoder for: "
             << ringBuffer->GetFilename() << endl;
        return -1;
    }

    decoder->setExactSeeks(exactseeks);
    decoder->setLiveTVMode(livetv);
    decoder->setWatchingRecording(watchingrecording);
    decoder->setRecorder(nvr_enc);

    eof = 0;
    text_size = 8 * (sizeof(teletextsubtitle) + VT_WIDTH);

    int ret;
    db_lock.lock();
    if ((ret = decoder->OpenFile(ringBuffer, disablevideo, testbuf)) < 0)
    {
        db_lock.unlock();
        cerr << "Couldn't open decoder for: " << ringBuffer->GetFilename()
             << endl;
        return -1;
    }
    db_lock.unlock();

    if (ret > 0)
    {
        hasFullPositionMap = true;

        LoadCutList();
    
        if (!deleteMap.isEmpty())
        {
            hasdeletetable = true;
            deleteIter = deleteMap.begin();
        }

        bookmarkseek = GetBookmark();
    }

    commDetect = new CommDetect(video_width, video_height, video_frame_rate);

    commDetect->SetAggressiveDetection(
                    gContext->GetNumSetting("AggressiveCommDetect", 1));

    return 0;
}

void NuppelVideoPlayer::InitFilters(void)
{
    VideoFrameType itmp = FMT_YV12;
    VideoFrameType otmp = FMT_YV12;
    int btmp;

    if (videoFilters)
        delete videoFilters;

    postfilt_width = video_width;
    postfilt_height = video_height;
    videoFilters = FiltMan->LoadFilters(videoFilterList, itmp, otmp, 
                                        postfilt_width, postfilt_height, btmp);
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

void NuppelVideoPlayer::GetFrame(int onlyvideo, bool unsafe)
{
#ifdef USING_IVTV
    if (forceVideoOutput == kVideoOutput_IVTV)
    {
        decoder->GetFrame(onlyvideo);
        return;
    }
#endif

    while (!videoOutput->EnoughFreeFrames() && !unsafe)
    {
        //cout << "waiting for video buffer to drain.\n";
        setPrebuffering(false);
        if (!videoOutput->availableVideoBuffersWait()->wait(2000))
            cerr << "waiting for free video buffers timed out\n";
    }

    decoder->GetFrame(onlyvideo);

    if (videoOutput->EnoughDecodedFrames())
        setPrebuffering(false);
}

void NuppelVideoPlayer::ReduceJitter(struct timeval *nexttrigger)
{
    static int cheat = 5000;
    static int fudge = 0;

    int delay;
    int cnt = 0;

    cheat += 100;

    delay = UpdateDelay(nexttrigger);

    /* The usleep() is shortened by "cheat" so that this process
       gets the CPU early for about half the frames. */

    if (delay > (cheat - fudge))
        usleep(delay - (cheat - fudge));

    /* If late, draw the frame ASAP. If early, hold the CPU until
       half as late as the previous frame (fudge) */

    while (delay + fudge > 0)
    {
        delay = UpdateDelay(nexttrigger);
        cnt++;
    }

    fudge = abs(delay / 2);

    if (cnt > 1)
        cheat -= 200;
}

static void NormalizeTimeval(struct timeval *tv)
{
    while (tv->tv_usec > 999999)
    {
        tv->tv_sec++;
        tv->tv_usec -= 1000000;
    }
    while (tv->tv_usec < 0)
    {
        tv->tv_sec--;
        tv->tv_usec += 1000000;
    }
}

void NuppelVideoPlayer::ResetNexttrigger(struct timeval *nexttrigger)
{
    /* when we're paused or prebuffering, we need to update
       'nexttrigger' before we start playing again. Otherwise,
       the value of 'nexttrigger' will be far in the past, and
       the video will play really fast for a while.*/
    
    gettimeofday(nexttrigger, NULL);
    nexttrigger->tv_usec += frame_interval;
    NormalizeTimeval(nexttrigger);
}


int NuppelVideoPlayer::UpdateDelay(struct timeval *nexttrigger)
{
    struct timeval now; 

    gettimeofday(&now, NULL);

    int ret = (nexttrigger->tv_sec - now.tv_sec) * 1000000 +
              (nexttrigger->tv_usec - now.tv_usec); // uSecs

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
        videoOutput->StopEmbedding();
}

void NuppelVideoPlayer::ToggleCC(void)
{
    if (cc)
    {
        cc = false;
        osd->ClearAllCCText();
    }
    else
    {
        cc = true;
    }
}

void NuppelVideoPlayer::ShowText(void)
{
    VideoFrame *last = videoOutput->GetLastShownFrame();

    // check if subtitles need to be updated on the OSD
    if (tbuffer_numvalid() && txtbuffers[rtxt].timecode && 
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
                    osd->AddCCText(s, st.row, st.col, 1, true);
                    inpos += st.len;
                }
            }
        }
        else if (txtbuffers[rtxt].type == 'C')
        {
            int j;
            unsigned char *inpos = txtbuffers[rtxt].buffer;
            struct ccsubtitle subtitle;

            memcpy(&subtitle, inpos, sizeof(subtitle));
            inpos += sizeof(subtitle);

            if (subtitle.clr)
            {
                //printf ("erase displayed memory\n");
                for (j = 0; j < 4; j++)
                {
                    cclines[j] = "";
                }
                osd->ClearAllCCText();
            }

            if (subtitle.len)
            {
                unsigned char *end = inpos + subtitle.len;
                int row;

                if (subtitle.row == 0)
                    subtitle.row = lastccrow;

                lastccrow = subtitle.row;

                if (subtitle.rowcount == 0)
                {
                    // overwrite
                    row = 0;
                }
                else
                {
                    if (subtitle.rowcount > 4)
                    {
                        subtitle.rowcount = 4;
                    }
                    // scroll up one line
                    for (j = 0; j < subtitle.rowcount - 1 && j < 4; j++)
                    {
                        cclines[j] = cclines[j + 1];
                        ccindent[j] = ccindent[j + 1];
                    }
                    cclines[subtitle.rowcount - 1] = "";
                    row = subtitle.rowcount - 1;
                }

                while (inpos < end)
                {
                    ccindent[row] = 0;
                    while ((inpos < end) && *inpos != 0 && (char)*inpos == ' ')
                    {
                        inpos++;
                        ccindent[row]++;
                    }

                    unsigned char *cur = inpos;

                    //null terminate at EOL
                    while (cur < end && *cur != '\n' && *cur != 0)
                        cur++;
                    *cur = 0;

                    if (subtitle.rowcount > 0 && row >= subtitle.rowcount)
                    {
                        // multi-line in scroll mode -- shouldn't happen?
                        // scroll up again
                        for (j = 0; j < subtitle.rowcount - 1 && j < 4; j++)
                        {
                             cclines[j] = cclines[j + 1];
                             ccindent[j] = ccindent[j + 1];
                        }
                        row = subtitle.rowcount - 1;
                    }

                    if (row < 4)
                    {
                        cclines[row++] = QString((const char *)inpos);
                        //printf ("CC text: %s\n", inpos);
                    }
#if 0
                    else
                    {
                        printf("CC overflow: %s\n", inpos);
                    }
#endif

                    inpos = cur + 1;
                }

                if (subtitle.rowcount == 0)
                    subtitle.rowcount = row;

            }

            //redraw
            osd->ClearAllCCText ();
            for (j = 0; j < subtitle.rowcount && j < 4; j++)
            {
                if (cclines[j].isNull() || cclines[j].isEmpty())
                    continue;
                osd->AddCCText(cclines[j], ccindent[j], subtitle.row + j, 
                               1, false);
            }
        }

        text_buflock.lock();
        if (rtxt != wtxt) // if a seek occurred, rtxt == wtxt, in this case do
                          // nothing
            rtxt = (rtxt + 1) % MAXTBUFFER;
        text_buflock.unlock();
    }
}

#define MAXWARPDIFF 0.0005 // Max amount the warpfactor can change in 1 frame
#define WARPMULTIPLIER 1000000000 // How much do we multiply the warp by when 
                                  //storing it in an integer
#define WARPAVLEN (video_frame_rate * 600) // How long to average the warp over
#define RTCRATE 1024 // RTC frequency if we have no vsync
#define WARPCLIP    0.1 // How much we allow the warp to deviate from 1 
                        // (normal speed)
#define MAXDIVERGE  20  // Maximum number of frames of A/V divergence allowed
                        // before dropping or extending video frames to 
                        // compensate
 
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

//    cerr << "Divergence: " << divergence << "  Rate: " << rate 
//         << "  Warpfactor: " << warpfactor << "  warpfactor_avg: " 
//         << warpfactor_avg << endl;
    return divergence;
}

void NuppelVideoPlayer::InitVTAVSync(void) 
{
    QString timing_type = "next trigger";

    //warpfactor_avg = 1;
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
    avsync_oldavg = 0;
    rtcfd = -1;
    if (!disablevideo) 
    {
        int ret = vsync_init();

        refreshrate = videoOutput->GetRefreshRate();
        if (refreshrate <= 0) 
            refreshrate = frame_interval;

        if (ret > 0) 
        {
            hasvsync = true;

            if ( ret == 1 ) timing_type = "nVidia polling";
            vsynctol = refreshrate / 4; 
            // How far out can the vsync be for us to use it?
        } 
#ifdef __linux__
        else 
        {
            rtcfd = open("/dev/rtc", O_RDONLY);
            if (rtcfd >= 0) 
            {
                if ((ioctl(rtcfd, RTC_IRQP_SET, RTCRATE) < 0) || 
                    (ioctl(rtcfd, RTC_PIE_ON, 0) < 0)) 
                {
                    close(rtcfd);
                    rtcfd = -1;
                } 
                else 
                {
                    vsynctol = 1000000 / RTCRATE; 
                    // How far out can the interrupt be for us to use it?
                    timing_type = "/dev/rtc";
                }
            }
        }
#endif

        nice(-19);

        QString msg = QString("Video timing method: %1").arg(timing_type);
        VERBOSE(VB_PLAYBACK, msg);
        msg = QString("Refresh rate: %1, frame interval: %2") 
                     .arg(refreshrate).arg(frame_interval);
        VERBOSE(VB_PLAYBACK, msg);
    }
}

void NuppelVideoPlayer::VTAVSync(void)
{
    float           diverge;
    unsigned long   rtcdata;
    
    VideoFrame *buffer = videoOutput->GetLastShownFrame();
    if (!buffer) 
    {
        cerr << "No video buffer, error error\n";
        return;
    }

    if (normal_speed)
        diverge = WarpFactor();
    else
        diverge = 0;

    delay = UpdateDelay(&nexttrigger);
    // If video is way ahead of audio, drop some frames until we're close again.
    // If we're close to a vsync then we'll display a frame to keep the 
    // picture updated.
    if ((diverge < -MAXDIVERGE) && (delay < vsynctol)) 
        cerr << "A/V diverged by " << diverge 
             << " frames, dropping frame to keep audio in sync" << endl;
    else if (disablevideo) 
    {
        delay = UpdateDelay(&nexttrigger);
        if (delay > 0) 
            usleep(delay);
    } 
    else 
    {
        // if we get here, we're actually going to do video output
        delay = UpdateDelay(&nexttrigger);
        if (buffer) 
            videoOutput->PrepareFrame(buffer);

        if ((hasvsync) || (rtcfd >= 0)) 
        {
            delay = UpdateDelay(&nexttrigger);
            if (delay < (0 - vsynctol)) 
                cerr << "Late frame!  Delay: " << delay << endl;

            if (hasvsync) 
                vsync_wait_for_retrace();
#ifdef __linux__
            else 
                read(rtcfd,&rtcdata,sizeof(rtcdata));
#endif

            delay = UpdateDelay(&nexttrigger);
            while (delay > vsynctol) 
            {
                vsync_wait_for_retrace();
                delay = UpdateDelay(&nexttrigger);
            }
        } 
        else 
        {    
            // No vsync _or_ RTC, fall back to usleep() (yuck)
            delay = UpdateDelay(&nexttrigger);
            usleep(delay);
        }
        
        // Reset the frame timer to current time since we're trusting the 
        // video timing
        gettimeofday(&nexttrigger, NULL);
        
        // Display the frame
        videoOutput->Show();
    }
    
    if (output_jmeter && output_jmeter->RecordCycleTime()) 
        cout << "avsync_avg: " << avsync_avg / 1000 
             << ", warpfactor: " << warpfactor 
             << ", warpfactor_avg: " << warpfactor_avg << endl;

    // Schedule next frame
    nexttrigger.tv_usec += frame_interval;
    if (diverge > MAXDIVERGE) 
    {
        // Audio is way ahead of the video - cut the frame rate
        // until it's almost in sync
        cerr << "A/V diverged by " << diverge 
             << " frames, extending frame to keep audio in sync" << endl;
        nexttrigger.tv_usec += (frame_interval * ((int)diverge / MAXDIVERGE));
    }

    NormalizeTimeval(&nexttrigger);
    
    if (audioOutput && normal_speed) 
    {
        // ms, same scale as timecodes
        lastaudiotime = audioOutput->GetAudiotime();
        if (lastaudiotime != 0) { // lastaudiotime = 0 after a seek
            // The time at the start of this frame (ie, now) is given by 
            // last->timecode
            avsync_delay = (buffer->timecode - lastaudiotime) * 1000; // uSecs
            avsync_avg = (avsync_delay + (avsync_avg * 3)) / 4;
        } 
        else 
        {
            avsync_avg = 0;
            avsync_oldavg = 0;
        }
    }
}

void NuppelVideoPlayer::ShutdownVTAVSync(void) 
{
    if (hasvsync) 
        vsync_shutdown();
    if (hasvgasync) 
        vgasync_cleanup();
    gContext->SaveSetting("WarpFactor", (int)(warpfactor_avg * WARPMULTIPLIER));

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

#ifdef __linux__
    if (rtcfd >= 0)
    {
        close(rtcfd);
        rtcfd = -1;
    }
#endif
}

void NuppelVideoPlayer::InitExAVSync(void)
{
    QString timing_type = "next trigger";

    if (reducejitter && !disablevideo)
    {
        int ret = vsync_init();

        if (ret > 0)
        {
            hasvsync = true;

            if ( ret == 1 )
                timing_type = "nVidia polling";
            /*
             // not yet tested
            else if ( ret == 2 )
                timing_type = "DRM vblank";
            */
        }
        /*
        // not yet implemented
        else if (vgasync_init(0) == 1)
        {
             hasvgasync = true;

             timing_type = "VGA refresh";
        }
        */
        else
        {
            timing_type = "reduce jitter";
        }

        nice(-19);
    }

    if (!disablevideo)
    {
        QString msg = QString("Video timing method: %1").arg(timing_type);
        VERBOSE(VB_PLAYBACK, msg);
        refreshrate = videoOutput->GetRefreshRate();
        if (refreshrate <= 0)
            refreshrate = frame_interval;
        msg = QString("Refresh rate: %1, frame interval: %2")
              .arg(refreshrate).arg(frame_interval);
        VERBOSE(VB_PLAYBACK, msg);
    }
}

void NuppelVideoPlayer::ExAVSync(void)
{
    VideoFrame *buffer = videoOutput->GetLastShownFrame();
    if (!buffer)
    {
        cerr << "No video buffer, error error\n";
        return;
    }

    if (disablevideo)
    {
        delay = UpdateDelay(&nexttrigger);
        if (delay > 0)
            usleep(delay);
    }
    else
    {
        // if we get here, we're actually going to do video output
        if (buffer)
            videoOutput->PrepareFrame(buffer);

        delay = UpdateDelay(&nexttrigger);

        if (hasvsync)
        {
            while (delay > 0)
            {
                vsync_wait_for_retrace();
                delay = UpdateDelay(&nexttrigger);
            }

            videoOutput->Show();

            // reset the clock if delay and refresh are too close
            if (delay > -1000 && !lastsync)
            {
                nexttrigger.tv_usec += 2000;
                NormalizeTimeval(&nexttrigger);
            }
        }
        else
        {
            // If delay is sometwhat more than a frame or < 0ms,
            // we clip it to these amounts and reset nexttrigger
            if (delay > frame_interval * 2)
            {
                // cerr << "Delaying to next trigger: " << delay << endl;
                usleep(frame_interval);
                delay = 0;
                avsync_avg = 0;
                gettimeofday(&nexttrigger, NULL);
            }
            else if (delay < 0 - frame_interval)
            {
                // cerr << "clipped negative delay " << delay << endl;
                delay = 0;
                avsync_avg = 0;
                gettimeofday(&nexttrigger, NULL);
            }

            if (reducejitter && normal_speed) 
                ReduceJitter(&nexttrigger);
            else
            {
                if (delay > 0)
                    usleep(delay);
            }

            videoOutput->Show();
        }
    }

    if (output_jmeter && output_jmeter->RecordCycleTime())
        cout << avsync_delay / 1000 << endl;

    /* The value of nexttrigger is perfect -- we calculated it to
       be exactly one frame time after the previous frame,
       Show frames at the frame rate as long as audio is in sync */

    nexttrigger.tv_usec += frame_interval;

    if (audioOutput && normal_speed)
    {
        lastaudiotime = audioOutput->GetAudiotime(); // ms, same scale as timecodes

        if (lastaudiotime != 0) // lastaudiotime = 0 after a seek
        {
            /* The time at the start of this frame (ie, now) is
               given by last->timecode */

            avsync_delay = (buffer->timecode - lastaudiotime) * 1000; // uSecs

            avsync_avg = (avsync_delay + (avsync_avg * 3)) / 4;

            /* If the audio time codes and video diverge, shift
               the video by one interlaced field (1/2 frame) */

            if (!lastsync)
            {
                if (avsync_avg > frame_interval * 3 / 2)
                {
                    nexttrigger.tv_usec += refreshrate;
                    lastsync = true;
                }
                else if (avsync_avg < 0 - frame_interval * 3 / 2)
                {
                    nexttrigger.tv_usec -= refreshrate;
                    lastsync = true;
                }
            }
            else
                lastsync = false;
        }
        else
            avsync_avg = 0;
    }
}

void NuppelVideoPlayer::ShutdownExAVSync(void)
{
    if (hasvsync)
        vsync_shutdown();

    if (hasvgasync)
        vgasync_cleanup();
}

void NuppelVideoPlayer::OldAVSync(void)
{
    VideoFrame *buffer = videoOutput->GetLastShownFrame();
    if (!buffer)
    {
        cerr << "No video buffer, error error\n";
        return;
    }

    // calculate 'delay', that we need to get from 'now' to 'nexttrigger'
    gettimeofday(&now, NULL);

    delay = (nexttrigger.tv_sec - now.tv_sec) * 1000000 +
            (nexttrigger.tv_usec - now.tv_usec); // uSecs

    if (reducejitter && normal_speed)
    {
        /* If delay is sometwhat more than a frame or < 0ms,
           we clip it to these amounts and reset nexttrigger */
        if (delay > frame_interval)
        {
            // cerr << "Delaying to next trigger: " << delay << endl;
            delay = frame_interval;
            usleep(delay);

            gettimeofday(&nexttrigger, NULL);
        }
        else if (delay <= 0)
        {
            // cerr << "clipped negative delay " << delay << endl;
            gettimeofday(&nexttrigger, NULL);

        }
        else
        {
            if (!disablevideo)
                ReduceJitter(&nexttrigger);
            else
                usleep(delay);
        }
    }
    else
    {
        if (delay > 200000)
        {
            QString msg = QString("Delaying to next trigger: %1")
                          .arg(delay);
            VERBOSE(VB_PLAYBACK, msg);
            delay = 200000;
            delay_clipping = true;
        }

        /* trigger */
        if (delay > 0)
            usleep(delay);
        else
        {
            //cout << "clipped negative delay: " << delay << endl;
            delay_clipping = true;
        }
        /* The time right now is a good approximation of nexttrigger. */
        /* It's not perfect, because usleep() is only pseudo-
           approximate about waking us up. So we jitter a few ms. */
        /* The value of nexttrigger is perfect -- we calculated it to
           be exactly one frame time after the previous frame,
           plus just enough feedback to stay synchronized with audio. */
        /* UNLESS... we had to clip.  In this case, resync nexttrigger
           with the wall clock. */
        if (delay_clipping)
        {
            gettimeofday(&nexttrigger, NULL);
            delay_clipping = false;
        }
    }

    if (!disablevideo)
    {
        videoOutput->PrepareFrame(buffer);
        videoOutput->Show();
    }
    /* a/v sync assumes that when 'Show' returns, that is the instant
       the frame has become visible on screen */

    if (output_jmeter && output_jmeter->RecordCycleTime())
        cerr << avsync_delay / 1000 << endl;

    /* The value of nexttrigger is perfect -- we calculated it to
        be exactly one frame time after the previous frame,
        plus just enough feedback to stay synchronized with audio. */

    nexttrigger.tv_usec += frame_interval;

    /* Apply just a little feedback. The ComputeAudiotime() function is
       jittery, so if we try to correct our entire A/V drift on each frame,
       video output is jerky. Instead, correct a fraction of the computed
       drift on each frame.

       In steady state, very little feedback is needed. However, if we are
       far out of sync, we need more feedback. So, we case on this. */

    if (audioOutput && normal_speed)
    {
        lastaudiotime = audioOutput->GetAudiotime(); // ms, same scale as timecodes
        if (lastaudiotime != 0) // lastaudiotime = 0 after a seek
        {
            /* if we were perfect, (timecodes[rpos] - frame_time)
               and lastaudiotime would match, and this adjustment
               wouldn't do anything */

            /* The time at the start of this frame (ie, now) is
               given by timecodes[rpos] */

            avsync_delay = (buffer->timecode - lastaudiotime) * 1000; // uSecs

            if (avsync_delay < -100000 || avsync_delay > 100000)
                nexttrigger.tv_usec += avsync_delay / 3; // re-syncing
            else
                nexttrigger.tv_usec += avsync_delay / 30; // steady state
        }
    }
}

void NuppelVideoPlayer::OutputVideoLoop(void)
{
    lastaudiotime = 0;
    delay = 0;
    avsync_delay = 0;
    avsync_avg = 0;

    delay_clipping = false;

    refreshrate = 0;
    lastsync = false;
    hasvsync = false;
    hasvgasync = false;

    reducejitter = gContext->GetNumSetting("ReduceJitter", 0);
    experimentalsync = gContext->GetNumSetting("ExperimentalAVSync", 0);
    usevideotimebase = gContext->GetNumSetting("UseVideoTimebase", 0);

    if ((print_verbose_messages & VB_PLAYBACK) != 0)
        output_jmeter = new Jitterometer("video_output", 100);
    else
        output_jmeter = NULL;

    refreshrate = frame_interval;

    gettimeofday(&nexttrigger, NULL);

    if (usevideotimebase)
        InitVTAVSync();
    else if (experimentalsync)
        InitExAVSync();

    while (!eof && !killvideo)
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

            videoOutput->ProcessFrame(NULL, osd, videoFilters, pipplayer);
            videoOutput->PrepareFrame(NULL); 
            videoOutput->Show();
            ResetNexttrigger(&nexttrigger);

            //printf("video waiting for unpause\n");
            usleep(frame_interval);
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
            ResetNexttrigger(&nexttrigger);
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

        videoOutput->ProcessFrame(frame, osd, videoFilters, pipplayer);

        if (usevideotimebase)
            VTAVSync();
        else if (experimentalsync)
            ExAVSync();
        else 
            OldAVSync();

        NormalizeTimeval(&nexttrigger);

        videoOutput->DoneDisplayingFrame();

        //sched_yield();
    }

    vidExitLock.lock();
    delete videoOutput;
    videoOutput = NULL;
    vidExitLock.unlock();

    if (usevideotimebase)
        ShutdownVTAVSync();
    else if (experimentalsync)
        ShutdownExAVSync();
}

#ifdef USING_IVTV
void NuppelVideoPlayer::IvtvVideoLoop(void)
{
    refreshrate = frame_interval;
    int delay = frame_interval;

    VideoOutputIvtv *vidout = (VideoOutputIvtv *)videoOutput;
    vidout->SetFPS(GetFrameRate());

    while (!eof && !killvideo)
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
            videoOutput->ProcessFrame(NULL, osd, videoFilters, pipplayer);
        }
        else
        {
            if (cc)
                ShowText();
            videoOutput->ProcessFrame(NULL, osd, videoFilters, pipplayer);
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
    if (fftime == 0)
        fftime = (int)(seconds * video_frame_rate);

    return fftime > CalcMaxFFTime(fftime);
}

bool NuppelVideoPlayer::Rewind(float seconds)
{
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
 
    if (!disableaudio)
    {
        audioOutput = AudioOutput::OpenAudio(audiodevice, audio_bits,
                                             audio_channels, audio_samplerate);
    }

    InitVideo();

    if (!disablevideo)
    {
        int dispx = 0, dispy = 0, dispw = video_width, disph = video_height;
        videoOutput->GetDrawSize(dispx, dispy, dispw, disph);
        osd = new OSD(video_width, video_height, frame_interval,
                      osdfontname, osdccfontname, osdprefix, osdtheme,
                      dispx, dispy, dispw, disph);
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

    pthread_t output_video;
    pthread_create(&output_video, NULL, kickoffOutputVideoLoop, this);

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

        QMutexLocker lockit(&db_lock);

        if (gContext->GetNumSetting("ClearSavedPosition", 1))
            m_playbackinfo->SetBookmark(0, m_db);
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

    while (!eof && !killplayer)
    {
        if (paused)
        { 
            if (!previously_paused)
            {
#ifdef USING_IVTV
                if (forceVideoOutput == kVideoOutput_IVTV)
                {
                    VideoOutputIvtv *vidout = (VideoOutputIvtv *)videoOutput;
                    vidout->Pause();
                }
#endif
                decoder->UpdateFramesPlayed();
                previously_paused = true;
            }

            actuallypaused = true;
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

        if (previously_paused)
        {
#ifdef USING_IVTV
            if (forceVideoOutput == kVideoOutput_IVTV)
            {
                VideoOutputIvtv *vidout = (VideoOutputIvtv *)videoOutput;
                vidout->Play(play_speed, normal_speed);
            }
#endif
            previously_paused = false;
        }

        if (rewindtime > 0)
        {
            PauseVideo();

            if (rewindtime >= 1)
                DoRewind();

            UnpauseVideo();
            while (GetVideoPause())
                usleep(50);
            rewindtime = 0;
        }

        if (fftime > 0)
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

        if (skipcommercials != 0)
        {
            PauseVideo();

            DoSkipCommercials(skipcommercials);
            UnpauseVideo();

            skipcommercials = 0;
            continue;
        }

        GetFrame(audioOutput == NULL || !normal_speed);

        if (!hasdeletetable && autocommercialskip)
            AutoCommercialSkip();

        if (hasdeletetable && deleteIter.data() == 1 && 
            framesPlayed >= deleteIter.key())
        {
            ++deleteIter;
            if (deleteIter.key() == totalFrames)
                eof = 1;
            else
                JumpToFrame(deleteIter.key());
            ++deleteIter;
        }
    }

    killvideo = true;
    pthread_join(output_video, NULL);

    // need to make sure video has exited first.
    if (audioOutput)
        delete audioOutput;
    audioOutput = NULL;

    playing = false;
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
            audioOutput->AddSamples((char *)newbuffer, samples, timecode);
        }
        else
            audioOutput->AddSamples(buffer, len / 
                                    (audio_channels * audio_bits / 8),
                                    timecode);
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
            buffers[1] = (char *)newlbuffer;

            for (incount = 0, outcount = 0; 
                 (incount < samples) && (outcount < newsamples); 
                 outcount++, incount += warpfactor) 
            {
                newlbuffer[outcount] = lbuffer[(int)incount];
                newrbuffer[outcount] = rbuffer[(int)incount];
            }

            samples = outcount;
        }

        audioOutput->AddSamples(buffers, samples, timecode);
    }
}

void NuppelVideoPlayer::SetBookmark(void)
{
    if (livetv)
        return;
    if (!m_db || !m_playbackinfo)
        return;

    QMutexLocker lockit(&db_lock);

    m_playbackinfo->SetBookmark(framesPlayed, m_db);
    osd->SetSettingsText("Position Saved", 1); 
}

void NuppelVideoPlayer::ClearBookmark(void)
{
    if (livetv)
        return;
    if (!m_db || !m_playbackinfo)
        return;

    QMutexLocker lockit(&db_lock);

    m_playbackinfo->SetBookmark(0, m_db);
    osd->SetSettingsText("Position Cleared", 1); 
}

long long NuppelVideoPlayer::GetBookmark(void)
{
    if (!m_db || !m_playbackinfo)
        return 0;

    QMutexLocker lockit(&db_lock);
    return m_playbackinfo->GetBookmark(m_db);
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
            if (framesPlayed > deleteIter.key())
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
            if (framesPlayed > blankIter.key())
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
            if (framesPlayed > commBreakIter.key())
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

    if (!hasFullPositionMap || !m_playbackinfo || !m_db)
        return false;

    bool alreadyediting = false;
    db_lock.lock();
    alreadyediting = m_playbackinfo->IsEditing(m_db);
    db_lock.unlock();

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
    db_lock.lock();
    m_playbackinfo->ToMap(m_db, infoMap);
    db_lock.unlock();
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

    db_lock.lock();
    m_playbackinfo->SetEditing(true, m_db);
    db_lock.unlock();

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
   
    QMutexLocker lockit(&db_lock);
    m_playbackinfo->SetEditing(false, m_db);
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

void NuppelVideoPlayer::ToggleLetterbox(void)
{
    if (videoOutput)
        videoOutput->ToggleLetterbox();
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
                DeleteMark(deleteframe);
                AddMark(deleteframe, 1 - type);
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
            fftime = keyframedist * 3 / 2;
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

    QMutexLocker lockit(&db_lock);
    m_playbackinfo->SetMarkupFlag(MARK_UPDATED_CUT, true, m_db);
    m_playbackinfo->SetCutList(deleteMap, m_db);
}

void NuppelVideoPlayer::LoadCutList(void)
{
    if (!m_playbackinfo || !m_db)
        return;

    QMutexLocker lockit(&db_lock);
    m_playbackinfo->GetCutList(deleteMap, m_db);
}

void NuppelVideoPlayer::LoadBlankList(void)
{
    if (!m_playbackinfo || !m_db)
        return;

    QMutexLocker lockit(&db_lock);
    m_playbackinfo->GetBlankFrameList(blankMap, m_db);
}

void NuppelVideoPlayer::LoadCommBreakList(void)
{
    if (!m_playbackinfo || !m_db)
        return;

    QMutexLocker lockit(&db_lock);
    m_playbackinfo->GetCommBreakList(commBreakMap, m_db);
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

    InitVideo();

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

    InitVideo();
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

    decoder->GetFrame(0);
    if (eof)
        return false;

    if (honorCutList && (!deleteMap.isEmpty()))
    {
        if (framesPlayed >= dm_iter.key())
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

int NuppelVideoPlayer::FlagCommercials(bool showPercentage, bool fullSpeed)
{
    int comms_found = 0;
    int percentage = 0;

    killplayer = false;
    framesPlayed = 0;

    blankMap.clear();
    commBreakMap.clear();

    disablevideo = true;

    if (OpenFile() < 0)
        return(0);

    db_lock.lock();
    // set to processing
    m_playbackinfo->SetCommFlagged(2, m_db);
    m_playbackinfo->SetMarkupFlag(MARK_PROCESSING, true, m_db);
    db_lock.unlock();

    playing = true;

    InitVideo();

    for (int i = 0; i < MAXTBUFFER; i++)
        txtbuffers[i].buffer = new unsigned char[text_size];

    ClearAfterSeek();

    if (!(commercialskipmethod & 0x01))
        commDetect->SetBlankFrameDetection(false);
    if (commercialskipmethod & 0x02)
        commDetect->SetSceneChangeDetection(true);

    commDetect->SetCommSkipAllBlanks(
        gContext->GetNumSetting("CommSkipAllBlanks", 1));

    // the meat of the offline commercial detection code, scan through whole
    // file looking for indications of commercial breaks
    GetFrame(1,true);
    if (showPercentage)
        printf( "%3d%%", 0 );

    while (!eof)
    {
        // sleep a little so we don't use all cpu even if we're niced
        if (!fullSpeed)
            usleep(10000);

        if ((showPercentage) &&
            ((framesPlayed % 100) == 0))
        {
            if (totalFrames)
            {
                percentage = framesPlayed * 100 / totalFrames;
                printf( "\b\b\b\b" );
                printf( "%3d%%", percentage );
            }
            else
            {
                printf( "\b\b\b\b\b\b" );
                printf( "%6lld", framesPlayed );
            }
            fflush( stdout );
        }

        commDetect->ProcessNextFrame(videoOutput->GetLastDecodedFrame(), 
                                     framesPlayed);

        GetFrame(1,true);
    }

    if (showPercentage)
    {
        if (totalFrames)
            printf( "\b\b" );
        printf( "\b\b\b\b        \b\b\b\b\b\b" );
    }

    if (commercialskipmethod & 0x01)
    {
        commDetect->GetBlankFrameMap(blankMap);

        if (blankMap.size())
        {
            db_lock.lock();
            m_playbackinfo->SetBlankFrameList(blankMap, m_db);
            db_lock.unlock();
        }
    }

    commDetect->GetCommBreakMap(commBreakMap);

    if (commBreakMap.size())
    {
        db_lock.lock();
        m_playbackinfo->SetMarkupFlag(MARK_UPDATED_CUT, true, m_db);
        m_playbackinfo->SetCommBreakList(commBreakMap, m_db);
        db_lock.unlock();
    }
    comms_found = commBreakMap.size() / 2;

    playing = false;
    killplayer = true;

    db_lock.lock();

    if (!hasFullPositionMap)
        decoder->SetPositionMap();

    m_playbackinfo->SetMarkupFlag(MARK_PROCESSING, false, m_db);
    // also clears processing status
    m_playbackinfo->SetCommFlagged(1, m_db);
    db_lock.unlock();

    return(comms_found);
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

int NuppelVideoPlayer::calcSliderPos(QString &desc)
{
    float ret;

    char text[512];

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
            if (osd->getTimeType() == 0)
            {
                sprintf(text,
                        QObject::tr("%02d:%02d:%02d behind  --  %.2f%% full"), 
                        hours, mins, secs, (1000 - ret) / 10);
            }
            else
            {
                sprintf(text, QObject::tr("%02d:%02d:%02d behind"),
                        hours, mins, secs);
            }
        }
        else
        {
            if (osd->getTimeType() == 0)
            {
                sprintf(text, QObject::tr("%02d:%02d behind  --  %.2f%% full"), 
                        mins, secs, (1000 - ret) / 10);
            }
            else
            {
                sprintf(text, QObject::tr("%02d:%02d behind"), mins, secs);
            }
        }

        desc = text;
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
        sprintf(text, QObject::tr("%02d:%02d:%02d of %02d:%02d:%02d"), 
                phours, pmins, psecs, shours, smins, ssecs);
    else
        sprintf(text, QObject::tr("%02d:%02d of %02d:%02d"),
                pmins, psecs, smins, ssecs);

    desc = text;

    return (int)(ret);
}

bool NuppelVideoPlayer::FrameIsBlank(VideoFrame *frame)
{
    commDetect->ProcessNextFrame(frame);

    return commDetect->FrameIsBlank();
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
            (framesPlayed >= commBreakIter.key()))
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
                    if (autocommercialskip == 1)
                    {
                        comm_msg = QString(QObject::tr("Skip %1 seconds"))
                                   .arg(skipped_seconds);
                    }
                    else if (autocommercialskip == 2)
                    {
                        comm_msg = QString(QObject::tr(
                                   "Commercial: %1 seconds")) 
                                   .arg(skipped_seconds);
                    }
                    QString desc;
                    int spos = calcSliderPos(desc);
                    osd->StartPause(spos, false, comm_msg, desc, 2);
                }

                if (autocommercialskip == 1)
                    JumpToFrame(commBreakIter.key());
            }

            if (autocommercialskip == 1)
                GetFrame(1, true);

            ++commBreakIter;
        }
        return;
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
    if (( ! hasblanktable) ||
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
                    osd->StartPause(spos, false, comm_msg, desc, 2);
                    return false;
                }
            }
		}

        if (osd)
        {
            int skipped_seconds = (int)((commBreakIter.key() -
                    framesPlayed) / video_frame_rate);
            QString comm_msg = QString(QObject::tr("Skip %1 seconds"))
                                  .arg(skipped_seconds);
            QString desc;
            int spos = calcSliderPos(desc);
            osd->StartPause(spos, false, comm_msg, desc, 2);
        }
        JumpToFrame(commBreakIter.key());
        commBreakIter++;

        GetFrame(1, true);
        while (LastFrameIsBlank() && !eof && !killplayer)
            GetFrame(1, true);

        return true;
    }

    switch (commercialskipmethod)
    {
        case COMMERCIAL_SKIP_SCENE:  osd->EndPause();
                                     break;
        case COMMERCIAL_SKIP_BLANKS:
        default                    : SkipCommercialsByBlanks();
                                     break;
    }

    return true;
}


