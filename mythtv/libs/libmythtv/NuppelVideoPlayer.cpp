#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <qstringlist.h>
#include <qsqldatabase.h>
#include <qmap.h>

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

#include "decoderbase.h"
#include "nuppeldecoder.h"
#include "avformatdecoder.h"

extern "C" {
#include "../libvbitext/vbi.h"
#include "vsync.h"
}

#include "remoteencoder.h"

#include "videoout_null.h"

NuppelVideoPlayer::NuppelVideoPlayer(QSqlDatabase *ldb,
                                     ProgramInfo *info)
{
    m_db = ldb;
    m_playbackinfo = NULL;

    if (info)
        m_playbackinfo = new ProgramInfo(*info);

    playing = false;
    decoder_thread_alive = true;
    filename = "output.nuv";
    prebuffering = false;

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

    paused = 0;

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

    if (videoFilters.size() > 0)
    {
        filters_cleanup(&videoFilters[0], videoFilters.size());
        videoFilters.clear();
    }

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
    actuallypaused = false;

    // pause the decoder thread first, as it's usually waiting for the video
    // thread.  Make sure that it's done pausing before continuing on.
    paused = true;
    while (!actuallypaused)
        usleep(50);

    PauseVideo(waitvideo);
    if (audioOutput)
        audioOutput->Pause(true);

    if (ringBuffer)
        ringBuffer->Pause();
}

void NuppelVideoPlayer::Unpause(bool unpauseaudio)
{
    paused = false;
    UnpauseVideo();
    if (audioOutput && unpauseaudio)
        audioOutput->Pause(false);

    if (ringBuffer)
        ringBuffer->Unpause();
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

    while (wait && !video_actually_paused)
        usleep(50);
}

void NuppelVideoPlayer::UnpauseVideo(void)
{
    pausevideo = false;
}

void NuppelVideoPlayer::SetPlaySpeed(float speed)
{
    play_speed = speed;
    normal_speed = (speed == 1.0);
    frame_interval = (int)(1000000.0 / video_frame_rate / speed);
    if (osd)
        osd->SetFrameInterval(frame_interval);
}

void NuppelVideoPlayer::setPrebuffering(bool prebuffer)
{
    if (prebuffer != prebuffering)
    {
        prebuffering = prebuffer;
        if (audioOutput && !paused)
            audioOutput->Pause(prebuffering);
    }
}

void NuppelVideoPlayer::ForceVideoOutputType(VideoOutputType type)
{
    forceVideoOutput = type;
}

void NuppelVideoPlayer::InitVideo(void)
{
    if (disablevideo)
    {
        videoOutput = new VideoOutputNull();
        videoOutput->Init(video_width, video_height, video_aspect,
                          0, 0, 0, 0, 0, 0);
    }
    else
    {
        MythMainWindow *window = gContext->GetMainWindow();
        if (!window)
        {
            cerr << "No main window, aborting\n";
            exit(0);
        }

        QWidget *widget = window->currentWidget();
        if (!widget)
        {
            cerr << "No top level widget, aborting\n";
            exit(0);
        }

        videoOutput = VideoOutput::InitVideoOut(forceVideoOutput);

        if (gContext->GetNumSetting("DecodeExtraAudio", 0))
            decoder->setLowBuffers();

        videoOutput->Init(video_width, video_height, video_aspect,
                          widget->winId(), 0, 0, widget->width(), 
                          widget->height(), embedid);
    }

    if (embedid > 0)
    {
        videoOutput->EmbedInWidget(embedid, embx, emby, embw, embh);
    }
}

void NuppelVideoPlayer::ReinitVideo(void)
{
    videoOutput->InputChanged(video_width, video_height, video_aspect);

    int dispx = 0, dispy = 0, dispw = video_width, disph = video_height;

    videoOutput->GetDrawSize(dispx, dispy, dispw, disph);

    osd->Reinit(video_width, video_height, frame_interval,
                dispx, dispy, dispw, disph);

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
            ringBuffer = new RingBuffer(NULL, filename, false);
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
    QStringList filters = QStringList::split(",", videoFilterList);
    for (QStringList::Iterator i = filters.begin(); i != filters.end(); i++)
    {
        VideoFilter *filter = load_videoFilter((char *)((*i).ascii()),
                                               NULL);
        if (filter != NULL)
            videoFilters.push_back(filter);
    }   
}

int NuppelVideoPlayer::tbuffer_numvalid(void)
{
    /* thread safe, returns number of valid slots in the text buffer */
    int ret;
    pthread_mutex_lock(&text_buflock);

    if (wtxt >= rtxt)
        ret = wtxt - rtxt;
    else
        ret = MAXTBUFFER - (rtxt - wtxt);

    pthread_mutex_unlock(&text_buflock);
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

        pthread_mutex_lock(&text_buflock);
        wtxt = (wtxt+1) % MAXTBUFFER;
        pthread_mutex_unlock(&text_buflock);
    }
}

void NuppelVideoPlayer::GetFrame(int onlyvideo, bool unsafe)
{
    while (!videoOutput->EnoughFreeFrames() && !unsafe)
    {
        //cout << "waiting for video buffer to drain.\n";
        setPrebuffering(false);
        usleep(2000);
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

    return videoOutput->GetLastShownFrame();
}

void NuppelVideoPlayer::EmbedInWidget(unsigned long wid, int x, int y, int w, 
                                      int h)
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
    }
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

        /* update rtxt */
        pthread_mutex_lock(&text_buflock);
        if (rtxt != wtxt) // if a seek occurred, rtxt == wtxt, in this case do
                          // nothing
            rtxt = (rtxt + 1) % MAXTBUFFER;
        pthread_mutex_unlock(&text_buflock);
    }
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

    output_jmeter = NULL;
    //output_jmeter = new Jitterometer("video_output", 100);

    refreshrate = frame_interval;

    gettimeofday(&nexttrigger, NULL);

    if (experimentalsync)
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

            videoOutput->ProcessFrame(NULL, osd, videoFilters, pipplayer);
            videoOutput->PrepareFrame(NULL); 
            videoOutput->Show();
            ResetNexttrigger(&nexttrigger);

            //printf("video waiting for unpause\n");
            usleep(frame_interval * 2);
            continue;
        }
        video_actually_paused = false;
        resetvideo = false;

        if (prebuffering)
        {
            //printf("prebuffering...\n");
            usleep(frame_interval);
            ResetNexttrigger(&nexttrigger);
            continue;
        }

        if (videoOutput->ValidVideoFrames() < 2)
        {
            //printf("entering prebuffering mode\n");
            setPrebuffering(true); 
            continue;
        }

        videoOutput->StartDisplayingFrame(); 

        VideoFrame *frame = videoOutput->GetLastShownFrame();

        if (cc)
            ShowText();

        videoOutput->ProcessFrame(frame, osd, videoFilters, pipplayer);

        if (experimentalsync)
            ExAVSync();
        else 
            OldAVSync();

        NormalizeTimeval(&nexttrigger);

        videoOutput->DoneDisplayingFrame();

        //sched_yield();
    }

    delete videoOutput;
    videoOutput = NULL;

    if (experimentalsync)
        ShutdownExAVSync();
}

void *NuppelVideoPlayer::kickoffOutputVideoLoop(void *player)
{
    ((NuppelVideoPlayer *)player)->OutputVideoLoop();
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

    InitFilters();

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

    pthread_mutex_init(&text_buflock, NULL);

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
            actuallypaused = true;
            pausecheck++;

            if (!(pausecheck % 20))
            {
                if (livetv && ringBuffer->GetFreeSpace() < -1000)
                {
                    Unpause();
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
                fftime = deleteIter.key() - framesPlayed;
            ++deleteIter;
        }
    }

    delete audioOutput;
    audioOutput = NULL;

    killvideo = true;

    // these threads will also exit when killplayer or eof is true
    pthread_join(output_video, NULL);

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
        audioOutput->AddSamples(buffer, len / (audio_channels * audio_bits / 8),
                                timecode);
}

void NuppelVideoPlayer::AddAudioData(short int *lbuffer, short int *rbuffer,
                                     int samples, long long timecode)
{
    if (audioOutput)
    {
        char *buffers[] = {(char *)lbuffer, (char *)rbuffer};
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
            else if (behind - ff <= maxtime)
                ret = behind - maxtime;
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

    QMap<QString, QString> regexpMap;
    db_lock.lock();
    m_playbackinfo->ToMap(m_db, regexpMap);
    db_lock.unlock();
    osd->SetTextByRegexp("editmode", regexpMap, -1);

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

    Unpause();
}

void NuppelVideoPlayer::DoKeypress(int keypress)
{
    if (dialogname != "")
    {
        switch(keypress)
        {
            case Qt::Key_Up: osd->DialogUp(dialogname); break;
            case Qt::Key_Down: osd->DialogDown(dialogname); break;
            case Qt::Key_Escape: osd->DialogAbort(dialogname);
                // fall through
            case Qt::Key_Space: case Qt::Key_Enter: case Qt::Key_Return: 
            {
                osd->TurnDialogOff(dialogname); 
                HandleResponse();
                break;
            }
            default: break;
        }
        return;
    }

    bool exactstore = exactseeks;
    decoder->setExactSeeks(true);

    switch (keypress)
    {
        case Qt::Key_Space: case Qt::Key_Enter: case Qt::Key_Return:
        {
            HandleSelect();
            break;
        }
        case Qt::Key_Left: case Qt::Key_A: 
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
            break;           
        }
        case Qt::Key_Right: case Qt::Key_D:
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
            break;
        }
        case Qt::Key_Up: case Qt::Key_W:
        {
            UpdateSeekAmount(true);
            UpdateTimeDisplay();
            break;
        }
        case Qt::Key_Down: case Qt::Key_S:
        {
            UpdateSeekAmount(false);
            UpdateTimeDisplay();
            break; 
        }
        case Qt::Key_C:
        {
            QMap<long long, int>::Iterator it;
            for (it = deleteMap.begin(); it != deleteMap.end(); ++it)
                osd->HideEditArrow(it.key(), it.data());

            deleteMap.clear();
            UpdateEditSlider();
            break;
        }
        case Qt::Key_Z:
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
            break;
        }
        case Qt::Key_PageUp:
        {
            int old_seekamount = seekamount;
            seekamount = -2;
            HandleArbSeek(false);
            seekamount = old_seekamount;
            UpdateTimeDisplay();
            break;
        }
        case Qt::Key_PageDown:
        {
            int old_seekamount = seekamount;
            seekamount = -2;
            HandleArbSeek(true);
            seekamount = old_seekamount;
            UpdateTimeDisplay();
            break;
        }
#define FFREW_MULTICOUNT 10
        case Qt::Key_Less: case Qt::Key_Comma:
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
            break;
        }
        case Qt::Key_Greater: case Qt::Key_Period:
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
            break;
        }
        case Qt::Key_Escape: case Qt::Key_E: case Qt::Key_M:
        {
            DisableEdit();
            break;
        }
        default: break;
    }

    decoder->setExactSeeks(exactstore);
}

bool NuppelVideoPlayer::GetLetterbox(void)
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

    QMap<QString, QString> regexpMap;
    regexpMap["seekamount"] = text;
    osd->SetTextByRegexp("editmode", regexpMap, -1);
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

    QMap<QString, QString> regexpMap;
    regexpMap["timedisplay"] = timestr;
    regexpMap["framedisplay"] = framestr;
    regexpMap["cutindicator"] = cutmarker;
    osd->SetTextByRegexp("editmode", regexpMap, -1);
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
    if (OpenFile() < 0)
        return NULL;
    if (!decoder)
        return NULL;
    if (!hasFullPositionMap)
        return NULL;

    pthread_mutex_init(&text_buflock, NULL);

    disablevideo = true;

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

#define SetProfileOption(profile, name) { \
    int value = profile.byName(name)->getValue().toInt(); \
    nvr->SetOption(name, value); \
}

// This class is to act as a fake audio output device to store the data
// for reencoding.

class AudioReencodeBuffer : public AudioOutput
{
 public:
    AudioReencodeBuffer(int audio_bits, int audio_channels)
    {
        Reset();
        Reconfigure(audio_bits, audio_channels, 0);
        bufsize = 512000;
        audiobuffer = new unsigned char[bufsize];
    }

   ~AudioReencodeBuffer()
    {
        delete [] audiobuffer;
    }

    // reconfigure sound out for new params
    virtual void Reconfigure(int audio_bits,
                        int audio_channels, int audio_samplerate)
    {
        (void)audio_samplerate;
        bits = audio_bits;
        channels = audio_channels;
        bytes_per_sample = bits * channels / 8;
    }

    // dsprate is in 100 * samples/second
    virtual void SetEffDsp(int dsprate)
    {
        eff_audiorate = (dsprate / 100);
    }

    virtual void SetBlocking(bool block) { (void)block; }
    virtual void Reset(void)
    {
        audiobuffer_len = 0;
    }

    // timecode is in milliseconds.
    virtual void AddSamples(char *buffer, int samples, long long timecode)
    {
        int freebuf = bufsize - audiobuffer_len;

        if (samples * bytes_per_sample > freebuf)
        {
            bufsize += samples * bytes_per_sample - freebuf;
            unsigned char *tmpbuf = new unsigned char[bufsize];
            memcpy(tmpbuf, audiobuffer, audiobuffer_len);
            delete [] audiobuffer;
            audiobuffer = tmpbuf;
        }

        memcpy(audiobuffer + audiobuffer_len, buffer, 
               samples * bytes_per_sample);
        audiobuffer_len += samples * bytes_per_sample;
        // last_audiotime is at the end of the sample
        last_audiotime = timecode + samples * 1000 / eff_audiorate;
    }

    virtual void AddSamples(char *buffers[], int samples, long long timecode)
    {
        int audio_bytes = bits / 8;
        int freebuf = bufsize - audiobuffer_len;

        if (samples * bytes_per_sample > freebuf)
        {
            bufsize += samples * bytes_per_sample - freebuf;
            unsigned char *tmpbuf = new unsigned char[bufsize];
            memcpy(tmpbuf, audiobuffer, audiobuffer_len);
            delete [] audiobuffer;
            audiobuffer = tmpbuf;
        }

        for (int itemp = 0; itemp < samples*audio_bytes; itemp+=audio_bytes)
        {
            for(int chan = 0; chan < channels; chan++)
            {
                audiobuffer[audiobuffer_len++] = buffers[chan][itemp];
                if(bits == 16)
                    audiobuffer[audiobuffer_len++] = buffers[chan][itemp+1];
            }
        }

        // last_audiotime is at the end of the sample
        last_audiotime = timecode + samples * 1000 / eff_audiorate;
    }

    virtual void SetTimecode(long long timecode)
    {
        last_audiotime = timecode;
    }
    virtual bool GetPause(void)
    {
        return false;
    }
    virtual void Pause(bool paused)
    {
        (void)paused;
    }

    virtual int GetAudiotime(void)
    {
        return last_audiotime;
    }

    int bufsize;
    unsigned char *audiobuffer;
    int audiobuffer_len, channels, bits, bytes_per_sample, eff_audiorate;
    long long last_audiotime;
};

void NuppelVideoPlayer::ReencoderAddKFA(
                                QPtrList<struct kfatable_entry> *kfa_table,
                                long curframe, long lastkey, long num_keyframes)
{
    long delta = curframe - lastkey;
    if (delta != 0 && delta != keyframedist)
    {
        struct kfatable_entry *kfate = new struct kfatable_entry;
        kfate->adjust = keyframedist - delta;
        kfate->keyframe_number = num_keyframes;
        kfa_table->append(kfate);
    }
}

int NuppelVideoPlayer::ReencodeFile(char *inputname, char *outputname,
                                    RecordingProfile &profile,
                                    bool honorCutList, bool framecontrol,
                                    bool chkTranscodeDB, QString fifodir)
{ 
    NuppelVideoRecorder *nvr;

    inputname = inputname;
    outputname = outputname;

    filename = inputname;
     
    int audioframesize;
    int audioFrame = 0;

    QPtrList<struct kfatable_entry> kfa_table;

    QDateTime curtime = QDateTime::currentDateTime();
    if (honorCutList && m_playbackinfo)
    {
        if (m_playbackinfo->IsEditing(m_db) ||
                m_playbackinfo->CheckMarkupFlag(MARK_PROCESSING, m_db)) {
            return REENCODE_CUTLIST_CHANGE;
        }
        db_lock.lock();
        m_playbackinfo->SetMarkupFlag(MARK_UPDATED_CUT, false, m_db);
        db_lock.unlock();
        curtime = curtime.addSecs(60);
    }

    // Input setup
    nvr = new NuppelVideoRecorder(NULL);
    ringBuffer = new RingBuffer(filename, false, false);

    audioOutput = new AudioReencodeBuffer(audio_bits, audio_channels);
    AudioReencodeBuffer *arb = ((AudioReencodeBuffer*)audioOutput);

    if (OpenFile(false) < 0)
    {
        delete nvr;
        return REENCODE_ERROR;
    }

    if (arb->eff_audiorate > 0)
        audio_samplerate = arb->eff_audiorate;

    // Recorder setup
    nvr->SetFrameRate(video_frame_rate);

    // this is ripped from tv_rec SetupRecording. It'd be nice to consolodate
    nvr->SetOption("width", video_width);
    nvr->SetOption("height", video_height);

    nvr->SetOption("tvformat", gContext->GetSetting("TVFormat"));
    nvr->SetOption("vbiformat", gContext->GetSetting("VbiFormat"));

    QString vidsetting = profile.byName("videocodec")->getValue();
    if (vidsetting == "MPEG-4") 
    {
        nvr->SetOption("codec", "mpeg4");

        SetProfileOption(profile, "mpeg4bitrate");
        SetProfileOption(profile, "mpeg4scalebitrate");
        SetProfileOption(profile, "mpeg4maxquality");
        SetProfileOption(profile, "mpeg4minquality");
        SetProfileOption(profile, "mpeg4qualdiff");
        SetProfileOption(profile, "mpeg4optionvhq");
        SetProfileOption(profile, "mpeg4option4mv");
        nvr->SetupAVCodec();
    } 
    else if (vidsetting == "RTjpeg")
    {
        nvr->SetOption("codec", "rtjpeg");
        SetProfileOption(profile, "rtjpegquality");
        SetProfileOption(profile, "rtjpegchromafilter");
        SetProfileOption(profile, "rtjpeglumafilter");
        nvr->SetupRTjpeg();
    } 
    else 
    {
        cerr << "Unknown video codec: " << vidsetting << endl;
    }

    QString audsetting = profile.byName("audiocodec")->getValue();
    nvr->SetOption("samplerate", audio_samplerate);
    if (audsetting == "MP3") 
    {
        nvr->SetOption("audiocompression", 1);
        SetProfileOption(profile, "mp3quality");
        decoder->SetRawAudioState(true);
    } 
    else if (audsetting == "Uncompressed")
    {
        nvr->SetOption("audiocompression", 0);
    } 
    else 
    {
        cerr << "Unknown audio codec: " << audsetting << endl;
    }

    nvr->AudioInit(true);

    RingBuffer *outRingBuffer = new RingBuffer(outputname, true, false);
    nvr->SetRingBuffer(outRingBuffer);
    nvr->WriteHeader();
    nvr->StreamAllocate();

    playing = true;

    pthread_mutex_init(&text_buflock, NULL);

    disablevideo = true;

    InitVideo();

    for (int i = 0; i < MAXTBUFFER; i++)
        txtbuffers[i].buffer = new unsigned char[text_size];

    ClearAfterSeek();

    VideoFrame frame;
    frame.codec = FMT_YV12;
    frame.width = video_width;
    frame.height = video_height;
    frame.size = video_width * video_height * 3 / 2;

    FIFOWriter::FIFOWriter *fifow = NULL;

    if (fifodir != NULL)
    {
        QString audfifo = fifodir + QString("/audout");
        QString vidfifo = fifodir + QString("/vidout");
        int audio_size = audio_samplerate * audio_bits * audio_channels / 8;
        // framecontrol is true if we want to enforce fifo sync.
        if (framecontrol)
            VERBOSE(VB_GENERAL, "Enforcing sync on fifos");
        fifow = new FIFOWriter::FIFOWriter(2, framecontrol);

        if (!fifow->FIFOInit(0, QString("video"), vidfifo, frame.size, 50) ||
            !fifow->FIFOInit(1, QString("audio"), audfifo, audio_size, 25))
        {
           cerr << "Error initializing fifo writer.  Aborting" << endl;
           delete fifow;
           delete outRingBuffer;
           delete nvr;
           unlink(outputname);
           return REENCODE_ERROR;
        }
        VERBOSE(VB_GENERAL, QString("Video %1x%2@%3fps Audio rate: %4")
                                   .arg(video_width).arg(video_height)
                                   .arg(video_frame_rate)
                                   .arg(audio_samplerate));
        VERBOSE(VB_GENERAL, "Created fifos. Waiting for connection.");
    }

    bool forceKeyFrames = (fifow == NULL) ? framecontrol : false;

    if (vidsetting == decoder->GetEncodingType() && !forceKeyFrames &&
        fifow == NULL && !deleteMap.isEmpty() && honorCutList)
    {
        decoder->SetRawVideoState(true);
        VERBOSE(VB_GENERAL, "Reencoding video in 'raw' mode");
    }

    decoder->setExactSeeks(true);
    decoder->GetFrame(0);
    int rawaudio = decoder->GetRawAudioState();

    QMap<long long, int>::Iterator i;
    QMap<long long, int>::Iterator last = deleteMap.end();
    bool writekeyframe = true;
   
    int num_keyframes = 0;

    int did_ff = 0; 
    if (honorCutList && !deleteMap.isEmpty())
    {
        i = deleteMap.begin();
        if (i.key() == 1 && i.data() == 1)
        {
            while ((i.data() == 1) && (i != last))
            {
                decoder->DoFastForward(i.key());
                ++i;
            }
        }
    }

    frame.frameNumber = 0;
    long lastKeyFrame = 0;
    long totalAudio = 0;
    int dropvideo = 0;
    float rateTimeConv = audio_samplerate * audio_bits * audio_channels / 
                         8000.0;
    float vidFrameTime = 1000.0 / video_frame_rate;
    int wait_recover = 0;

    while (!eof)
    {
        decoder->GetFrame(0);
        frame.frameNumber++;

        if (eof)
            break;

        VideoFrame *lastDecode = videoOutput->GetLastDecodedFrame();
        frame.buf = lastDecode->buf;
        frame.timecode = lastDecode->timecode;

        if ((!deleteMap.isEmpty()) && honorCutList) 
        {
            if (framesPlayed >= i.key()) 
            {
                while((i.data() == 1) && (i != last))
                {
                    QString msg = QString("Fast-Forwarding from %1")
                                  .arg((int)i.key());
                    i++;
                    msg += QString(" to %1").arg((int)i.key());
                    VERBOSE(VB_GENERAL, msg);
                    decoder->DoFastForward(i.key());
                    lastDecode = videoOutput->GetLastDecodedFrame();
                    frame.buf = lastDecode->buf;
                    frame.timecode = lastDecode->timecode;
                    did_ff = 1;
                }
                while((i.data() == 0) && (i != last))
                {
                    i++;
                }
            }
        }

        if (eof)
            break;

        if (fifow)
        {
            totalAudio += arb->audiobuffer_len;
            int audbufTime = (int)(totalAudio / rateTimeConv);
            int auddelta = arb->last_audiotime - audbufTime;
            int vidTime = (int) (frame.frameNumber * vidFrameTime + 0.5);
            int viddelta = frame.timecode - vidTime;
            int delta = viddelta - auddelta;
            if (abs(delta) < 500 && abs(delta) >= vidFrameTime)
            {
               QString msg = QString("Audio is %1ms %2 video at # %3")
                          .arg(abs(delta))
                          .arg(((delta > 0) ? "ahead of" : "behind"))
                          .arg((int)frame.frameNumber);
                VERBOSE(VB_GENERAL, msg);
                dropvideo = (delta > 0) ? 1 : -1;
                wait_recover = 0;
            }
            else if (delta >= 500 && delta < 10000)
            {
                if (wait_recover == 0)
                {
                    dropvideo = 5;
                    wait_recover = 6;
                }
                else if (wait_recover == 1)
                {
                    // Video is badly lagging.  Try to catch up.
                    int count = 0;
                    while (delta > vidFrameTime)
                    {
                        fifow->FIFOWrite(0, frame.buf, frame.size);
                        count++;
                        delta -= (int)vidFrameTime;
                    }
                    QString msg = QString("Added %1 blank video frames")
                                  .arg(count);
                    frame.frameNumber += count;
                    dropvideo = 0;
                    wait_recover = 0;
                }
                else
                    wait_recover--;
            }
            else
            {
                dropvideo = 0;
                wait_recover = 0;
            }

            // int buflen = (int)(arb->audiobuffer_len / rateTimeConv);
            // cout << frame.frameNumber << ": video time: " << frame.timecode
            //      << " audio time: " << arb->last_audiotime << " buf: "
            //      << buflen << " exp: "
            //      << audbufTime << " delta: " << delta << endl;
            if (arb->audiobuffer_len)
                fifow->FIFOWrite(1, arb->audiobuffer, arb->audiobuffer_len);
            if (dropvideo < 0)
            {
                dropvideo++;
                frame.frameNumber--;
            }
            else
            {
                fifow->FIFOWrite(0, frame.buf, frame.size);
                if (dropvideo)
                {
                    fifow->FIFOWrite(0, frame.buf, frame.size);
                    frame.frameNumber++;
                    dropvideo--;
                }
            }
            videoOutput->DoneDisplayingFrame();
            audioOutput->Reset();
            rtxt = wtxt;
        }
        else if (rawaudio)
        {
            // Encoding from NuppelVideo to NuppelVideo with MP3 audio
            // So let's not decode/reencode audio
            if (!decoder->GetRawAudioState()) 
            {
                // The Raw state changed during decode.  This is not good
                if (fifow)
                    delete fifow;
                delete outRingBuffer;
                delete nvr;
                unlink(outputname);
                return REENCODE_ERROR;
            }

            if (forceKeyFrames) 
                writekeyframe = true;
            else
            {
                writekeyframe = decoder->isLastFrameKey();
                if (writekeyframe)
                {
                    // Currently, we don't create new sync frames,
                    // (though we do create new 'I' frames), so we mark
                    // the key-frames before deciding whether we need a
                    // new 'I' frame.

                    //need to correct the frame# and timecode here
                    // Question:  Is it necessary to change the timecodes?
                    decoder->UpdateFrameNumber(frame.frameNumber);
                    nvr->UpdateSeekTable(num_keyframes, false);
                    ReencoderAddKFA(&kfa_table, frame.frameNumber,lastKeyFrame,
                                    num_keyframes);
                    num_keyframes++;
                    lastKeyFrame = frame.frameNumber;

                    if (did_ff)
                        did_ff = 0;
                }
                else if (did_ff == 1 && decoder->GetRawVideoState())
                {
                    // Create a new 'I' frame if we just processed a cut.
                    writekeyframe = true;
                    did_ff++;
                }
            }

            if (!did_ff && decoder->GetRawVideoState())
                decoder->WriteStoredData(outRingBuffer, true);
            else
            {
                decoder->WriteStoredData(outRingBuffer, false);
                nvr->WriteVideo(&frame, true, writekeyframe);
            }
            audioOutput->Reset();
            rtxt = wtxt;
        } 
        else 
        {
            // audio is fully decoded, so we need to reencode it
            audioframesize = arb->audiobuffer_len;
            if (audioframesize > 0)
            {
                nvr->SetOption("audioframesize", audioframesize);
                int starttime = audioOutput->GetAudiotime();
                nvr->WriteAudio(arb->audiobuffer, audioFrame++, starttime);
                arb->audiobuffer_len = 0;
            }

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
                nvr->WriteText(inpos, txtbuffers[rtxt].len, 
                               txtbuffers[rtxt].timecode, pagenr);
                rtxt = (rtxt + 1) % MAXTBUFFER;
            }

            if (forceKeyFrames)
                nvr->WriteVideo(&frame, true, true);
            else
                nvr->WriteVideo(&frame);
        }

        if (QDateTime::currentDateTime() > curtime) 
        {
            if (honorCutList && 
                m_playbackinfo->CheckMarkupFlag(MARK_UPDATED_CUT, m_db)) 
            {
                if (fifow)
                    delete fifow;
                delete outRingBuffer;
                delete nvr;
                unlink(outputname);
                return REENCODE_CUTLIST_CHANGE;
            }

            if (chkTranscodeDB)
            {
                QString query = QString("SELECT * FROM transcoding WHERE "
                        "chanid = '%1' AND starttime = '%2' AND "
                        "((status & %3) = %4) AND hostname = '%5';")
                        .arg(m_playbackinfo->chanid)
                        .arg(m_playbackinfo->startts.toString("yyyyMMddhhmmss"))
                        .arg(TRANSCODE_STOP)
                        .arg(TRANSCODE_STOP)
                        .arg(gContext->GetHostName());
                db_lock.lock();
                MythContext::KickDatabase(m_db);
                QSqlQuery result = m_db->exec(query);
                db_lock.unlock();
                if (result.isActive() && result.numRowsAffected() > 0)
                {
                    if (fifow)
                        delete fifow;
                    delete outRingBuffer;
                    delete nvr;
                    unlink(outputname);
                    return REENCODE_ERROR;
                }
            }
            curtime = QDateTime::currentDateTime();
            curtime = curtime.addSecs(60);
        }
    }

    nvr->WriteSeekTable();
    if (!kfa_table.isEmpty())
        nvr->WriteKeyFrameAdjustTable(&kfa_table);
    delete outRingBuffer;
    delete nvr;
    if (fifow) 
    {
        fifow->FIFODrain();
        delete fifow;
        unlink(outputname);
    }
    return REENCODE_OK;
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

    if (OpenFile() < 0)
        return(0);

    db_lock.lock();
    m_playbackinfo->SetMarkupFlag(MARK_PROCESSING, true, m_db);
    db_lock.unlock();

    playing = true;

    pthread_mutex_init(&text_buflock, NULL);

    disablevideo = true;
    InitVideo();

    for (int i = 0; i < MAXTBUFFER; i++)
        txtbuffers[i].buffer = new unsigned char[text_size];

    ClearAfterSeek();

    if (!(commercialskipmethod & 0x01))
        commDetect->SetBlankFrameDetection(false);
    if (commercialskipmethod & 0x02)
        commDetect->SetSceneChangeDetection(true);

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

int NuppelVideoPlayer::calcSliderPos(float offset, QString &desc)
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

        played += (int)(offset * video_frame_rate);
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

    float secsplayed = ((float)framesPlayed / video_frame_rate) + offset;
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
                eof = 1;
            else
            {
                if (osd)
                {
                    int skipped_seconds = (int)((commBreakIter.key() -
                            framesPlayed) / video_frame_rate);
                    QString comm_msg = QString(QObject::tr("Skip %1 seconds"))
                                      .arg(skipped_seconds);
                    QString desc;
                    int spos = calcSliderPos(skipped_seconds, desc);
                    osd->StartPause(spos, false, comm_msg, desc, 1);
                }

                JumpToFrame(commBreakIter.key());
            }

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
                    int spos = calcSliderPos(0, desc);

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

    if (hascommbreaktable)
    {
        SetCommBreakIter();

        if ((commBreakIter == commBreakMap.begin()) &&
            (direction < 0))
        {
            int skipped_seconds = (int)((0 - framesPlayed) / video_frame_rate);

            QString comm_msg = QString(QObject::tr("Start of program."));
            QString desc;
            int spos = calcSliderPos(skipped_seconds, desc);
            osd->StartPause(spos, false, comm_msg, desc, 1);

            JumpToFrame(0);
            return true;
        }

        if ((commBreakIter == commBreakMap.end()) &&
            (direction > 0))
        {
            QString comm_msg = QString(QObject::tr("At End, can not Skip."));
            QString desc;
            int spos = calcSliderPos(0,desc);
            osd->StartPause(spos, false, comm_msg, desc, 1);
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
                    int spos = calcSliderPos(skipped_seconds, desc);
                    osd->StartPause(spos, false, comm_msg, desc, 1);

                    JumpToFrame(0);
                    return true;
                }
                else
                    commBreakIter--;
            }
        }

        if (osd)
        {
            int skipped_seconds = (int)((commBreakIter.key() -
                    framesPlayed) / video_frame_rate);
            QString comm_msg = QString(QObject::tr("Skip %1 seconds"))
                                  .arg(skipped_seconds);
            QString desc;
            int spos = calcSliderPos(skipped_seconds, desc);
            osd->StartPause(spos, false, comm_msg, desc, 1);
        }
        JumpToFrame(commBreakIter.key());
        commBreakIter++;

        GetFrame(1, true);
        while (LastFrameIsBlank())
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
