#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <qstringlist.h>
#include <qsqldatabase.h>
#include <qurl.h>
#include <qmap.h>

#include <iostream>
using namespace std;

#include "NuppelVideoPlayer.h"
#include "NuppelVideoRecorder.h"
#include "recordingprofile.h"
#include "XJ.h"
#include "osdtypes.h"
#include "remoteutil.h"
#include "programinfo.h"
#include "mythcontext.h"

#include "decoderbase.h"
#include "nuppeldecoder.h"
#include "avformatdecoder.h"

extern "C" {
#include "../libvbitext/vbi.h"
#include "vsync.h"
}

#include "remoteencoder.h"

#define wsUp            0x52 + 256
#define wsDown          0x54 + 256
#define wsLeft          0x51 + 256
#define wsRight         0x53 + 256
#define wsEscape        0x1b + 256
#define wsZero          0xb0 + 256
#define wsOne           0xb1 + 256
#define wsTwo           0xb2 + 256
#define wsThree         0xb3 + 256
#define wsFour          0xb4 + 256
#define wsFive          0xb5 + 256
#define wsSix           0xb6 + 256
#define wsSeven         0xb7 + 256
#define wsEight         0xb8 + 256
#define wsNine          0xb9 + 256
#define wsEnter         0x8d + 256
#define wsReturn        0x0d + 256

NuppelVideoPlayer::NuppelVideoPlayer(QSqlDatabase *ldb,
                                     ProgramInfo *info)
{
    m_db = ldb;
    m_playbackinfo = NULL;

    if (info)
        m_playbackinfo = new ProgramInfo(*info);

    playing = false;
    audiofd = -1;
    filename = "output.nuv";

    vbimode = ' ';
    vbipagenr = 0x08880000;
    video_height = 0;
    video_width = 0;
    video_size = 0;
    text_size = 0;

    decoder = NULL;

    bookmarkseek = 0;

    eof = 0;

    keyframedist = 30;
    usepre = 3;

    lastaudiolen = 0;
    wpos = rpos = 0;
    waud = raud = 0;
    wtxt = rtxt = 0;

    embedid = 0;
    embx = emby = embw = embh = -1;

    nvr_enc = NULL;

    paused = 0;

    audiodevice = "/dev/dsp";

    ringBuffer = NULL;
    weMadeBuffer = false;
    osd = NULL;

    commDetect = NULL;

    audio_bits = 16;
    audio_channels = 2;
    audio_samplerate = 44100;
    audio_bytes_per_sample = audio_channels * audio_bits / 8;
    audio_buffer_unused = 0;

    editmode = false;
    resetvideo = false;

    hasFullPositionMap = false;

    totalLength = 0;
    totalFrames = 0;

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

    eventvalid = false;

    timedisplay = NULL;
    seekamount = 30;
    seekamountpos = 4;
    deleteframe = 0;
    hasdeletetable = false;
    hasblanktable = false;
    hascommbreaktable = false;

    dialogname = "";

    for (int i = 0; i <= MAXVBUFFER; i++)
        vbuffer[i] = NULL;

    for (int i = 0; i <= MAXTBUFFER; i++)
    {
        txtbuffers[i].len = 0;
        txtbuffers[i].timecode = 0;
        txtbuffers[i].type = 0;
        txtbuffers[i].buffer = NULL;
    }

    own_vidbufs = false;

    killvideo = false;
    pausevideo = false;

    killaudio = false;
    pauseaudio = false;

    cc = false;
    lastccrow = 0;

    numbadioctls = 0;
    numlowbuffer = 0;

    pthread_mutex_init(&eventLock, NULL);
}

NuppelVideoPlayer::~NuppelVideoPlayer(void)
{
    if (m_playbackinfo)
        delete m_playbackinfo;
    if (weMadeBuffer)
        delete ringBuffer;
    if (osd)
        delete osd;
    if (commDetect)
        delete commDetect;

    if (own_vidbufs)
    {
        for (int i = 0; i <= MAXVBUFFER; i++)
        {
            if (vbuffer[i])
                delete [] vbuffer[i];
        }
    }

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

void NuppelVideoPlayer::Pause(void)
{
    actuallypaused = false;

    // pause the decoder thread first, as it's usually waiting for the video
    // thread.  Make sure that it's done pausing before continuing on.
    paused = true;
    while (!actuallypaused)
        usleep(50);

    PauseVideo();
    PauseAudio();

    if (ringBuffer)
        ringBuffer->Pause();
}

void NuppelVideoPlayer::Unpause(void)
{
    paused = false;
    UnpauseVideo();
    UnpauseAudio();

    if (ringBuffer)
        ringBuffer->Unpause();
}

bool NuppelVideoPlayer::GetPause(void)
{
    bool rbpaused = true;
    if (ringBuffer)
        rbpaused = ringBuffer->isPaused();

    if (disableaudio)
    {
        return (actuallypaused && rbpaused && GetVideoPause());
    }
    return (actuallypaused && rbpaused && GetAudioPause() && GetVideoPause());
}

inline bool NuppelVideoPlayer::GetVideoPause(void)
{
    return video_actually_paused;
}

void NuppelVideoPlayer::PauseVideo(void)
{
    video_actually_paused = false;
    pausevideo = true;

    while (!video_actually_paused)
        usleep(50);
}

void NuppelVideoPlayer::UnpauseVideo(void)
{
    pausevideo = false;
}

inline bool NuppelVideoPlayer::GetAudioPause(void)
{
    return audio_actually_paused;
}

void NuppelVideoPlayer::PauseAudio(void)
{
    audio_actually_paused = false;
    pauseaudio = true;
}

void NuppelVideoPlayer::UnpauseAudio(void)
{
    pauseaudio = false;
}

void NuppelVideoPlayer::InitVideo(void)
{
    char name[] = "MythTV"; 
    videoOutput = new XvVideoOutput();
    videoOutput->Init(video_width, video_height, name, name, MAXVBUFFER + 1, 
                      vbuffer, embedid);
    if (embedid > 0)
    {
        videoOutput->EmbedInWidget(embedid, embx, emby, embw, embh);
    }
}

void NuppelVideoPlayer::InitSound(void)
{
    int caps;

    if (disableaudio)
    {
        audiofd = -1;
        return;
    }

    QTime curtime = QTime::currentTime();
    curtime = curtime.addSecs(2);

    audiofd = -1;

    while (QTime::currentTime() < curtime && audiofd == -1)
    {
        audiofd = open(audiodevice.ascii(), O_WRONLY | O_NONBLOCK);
        if (audiofd < 0 && errno != EAGAIN && errno != EINTR)
        {
            if (errno == EBUSY)
            {
                cerr << "ERROR: something is currently using: " << audiodevice
                     << "\nFix this, then run mythfrontend again\n";
                exit(0);
            }
            perror("open");
        }
        if (audiofd < 0)
            usleep(50);
    }

    if (audiofd == -1)
    {
        cerr << "player: Can't open audio device: " << audiodevice << endl;
        disableaudio = true;
        return;
    }

    fcntl(audiofd, F_SETFL, fcntl(audiofd, F_GETFL) & ~O_NONBLOCK);

    if (ioctl(audiofd, SNDCTL_DSP_SAMPLESIZE, &audio_bits) < 0 ||
        ioctl(audiofd, SNDCTL_DSP_CHANNELS, &audio_channels) < 0 ||
        ioctl(audiofd, SNDCTL_DSP_SPEED, &audio_samplerate) < 0)
    {
        cerr << "player: " << audiodevice 
             << ": error setting audio output device to "
             << audio_samplerate << "kHz/" 
             << audio_bits << "bits/"
             << audio_channels << "channel\n";
        close(audiofd);
        audiofd = -1;
        return;
    }

    audio_bytes_per_sample = audio_channels * audio_bits / 8;

    audio_buf_info info;
    ioctl(audiofd, SNDCTL_DSP_GETOSPACE, &info);

    audio_buffer_unused = info.bytes - audio_bytes_per_sample * 
                          audio_samplerate / 10;
    if(audio_buffer_unused < 0)
       audio_buffer_unused = 0;

    if (!gContext->GetNumSetting("AggressiveSoundcardBuffer", 0))
        audio_buffer_unused = 0;

    if (ioctl(audiofd, SNDCTL_DSP_GETCAPS, &caps) >= 0 && 
        !(caps & DSP_CAP_REALTIME))
    {
        cerr << "audio device cannot report buffer state accurately,\n"
             << "audio/video sync will be bad, continuing anyway\n";
    }
}

void NuppelVideoPlayer::WriteAudio(unsigned char *aubuf, int size)
{
    if (audiofd <= 0)
        return;

    unsigned char *tmpbuf;
    int written = 0, lw = 0;

    tmpbuf = aubuf;

    while ((written < size) && 
           ((lw = write(audiofd, tmpbuf, size - written)) > 0))
    {
        if (lw == -1)
        {
            cerr << "Error writing to audio device, exiting\n";
            close(audiofd);
            audiofd = -1;
            return;
        }
        written += lw;
        tmpbuf += lw;
    }
}

void NuppelVideoPlayer::SetVideoParams(int width, int height, double fps, 
                                       int keyframedistance)
{
    if (width > 0)
        video_width = width; 
    if (height > 0)
        video_height = height;
    if (fps > 0)
        video_frame_rate = fps;

    video_size = video_height * video_width * 3 / 2;
    keyframedist = keyframedistance;
}

void NuppelVideoPlayer::SetAudioParams(int bps, int channels, int samplerate)
{
    audio_bits = bps;
    audio_channels = channels;
    audio_samplerate = samplerate;
    audio_bytes_per_sample = audio_channels * audio_bits / 8;
}

void NuppelVideoPlayer::SetEffDsp(int dsprate)
{
    effdsp = dsprate;
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

int NuppelVideoPlayer::audiolen(bool use_lock)
{
    /* Thread safe, returns the number of valid bytes in the audio buffer */
    int ret;
    
    if (use_lock) 
        pthread_mutex_lock(&audio_buflock);

    if (waud >= raud)
        ret = waud - raud;
    else
        ret = AUDBUFSIZE - (raud - waud);

    if (use_lock)
        pthread_mutex_unlock(&audio_buflock);

    return ret;
}

int NuppelVideoPlayer::audiofree(bool use_lock)
{
    return AUDBUFSIZE - audiolen(use_lock) - 1;
    /* There is one wasted byte in the buffer. The case where waud = raud is
       interpreted as an empty buffer, so the fullest the buffer can ever
       be is AUDBUFSIZE - 1. */
}

int NuppelVideoPlayer::vbuffer_numvalid(void)
{
    /* thread safe, returns number of valid slots in the video buffer */
    int ret;
    pthread_mutex_lock(&video_buflock);

    if (wpos >= rpos)
        ret = wpos - rpos;
    else
        ret = MAXVBUFFER - (rpos - wpos);

    pthread_mutex_unlock(&video_buflock);
    return ret;
}

int NuppelVideoPlayer::vbuffer_numfree(void)
{
    return MAXVBUFFER - vbuffer_numvalid() - 1;
    /* There's one wasted slot, because the case when rpos = wpos is 
       interpreted as an empty buffer. So the fullest the buffer can be is
       MAXVBUFFER - 1. */
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

int NuppelVideoPlayer::GetAudiotime(void)
{
    /* Returns the current timecode of audio leaving the soundcard, based
       on the 'audiotime' computed earlier, and the delay since it was computed.

       This is a little roundabout...

       The reason is that computing 'audiotime' requires acquiring the audio 
       lock, which the video thread should not do. So, we call 'SetAudioTime()'
       from the audio thread, and then call this from the video thread. */
    int ret;
    struct timeval now;

    if (audiotime == 0)
        return 0;

    pthread_mutex_lock(&avsync_lock);

    gettimeofday(&now, NULL);

    ret = audiotime;
 
    ret += (now.tv_sec - audiotime_updated.tv_sec) * 1000;
    ret += (now.tv_usec - audiotime_updated.tv_usec) / 1000;

    pthread_mutex_unlock(&avsync_lock);
    return ret;
}

void NuppelVideoPlayer::SetAudiotime(void)
{
    if (audbuf_timecode == 0)
        return;

    int soundcard_buffer = 0;
    int totalbuffer;

    /* We want to calculate 'audiotime', which is the timestamp of the audio
       which is leaving the sound card at this instant.

       We use these variables:

       'effdsp' is samples/sec, multiplied by 100.
       Bytes per sample is assumed to be 4.

       'audiotimecode' is the timecode of the audio that has just been 
       written into the buffer.

       'totalbuffer' is the total # of bytes in our audio buffer, and the
       sound card's buffer.

       'ms/byte' is given by '25000/effdsp'...
     */

    pthread_mutex_lock(&audio_buflock);
    pthread_mutex_lock(&avsync_lock);
 
    ioctl(audiofd, SNDCTL_DSP_GETODELAY, &soundcard_buffer); // bytes
    totalbuffer = audiolen(false) + soundcard_buffer;
               
    audiotime = audbuf_timecode - (int)(totalbuffer * 100000.0 /
                                        (audio_bytes_per_sample * effdsp));
 
    gettimeofday(&audiotime_updated, NULL);

    pthread_mutex_unlock(&avsync_lock);
    pthread_mutex_unlock(&audio_buflock);
}

unsigned char *NuppelVideoPlayer::GetNextVideoFrame(void)
{
    pthread_mutex_lock(&video_buflock);

    return vbuffer[wpos];
}

void NuppelVideoPlayer::ReleaseNextVideoFrame(bool good, long long timecode)
{
    if (!good)
        pthread_mutex_unlock(&video_buflock);
    else
    {
        timecodes[wpos] = timecode;
        vpos = wpos;
        wpos = (wpos + 1) % MAXVBUFFER;

        pthread_mutex_unlock(&video_buflock);
    }
}

void NuppelVideoPlayer::AddAudioData(short int *lbuffer, short int *rbuffer,
                                     int samples, long long timecode)
{
    pthread_mutex_lock(&audio_buflock);

    int new_audio_samps = 0;
    int itemp = 0;
    int afree = audiofree(false);

    if (samples * audio_bytes_per_sample > afree)
    {
        cout << "Audio buffer overflow, audio data lost!\n";
        samples = afree / audio_bytes_per_sample;
    }

    short int *saudbuffer = (short int *)audiobuffer;

    for (itemp = 0; itemp < samples; itemp++)
    {
        saudbuffer[waud / 2] = lbuffer[itemp];
        if (audio_channels == 2)
            saudbuffer[waud / 2 + 1] = rbuffer[itemp];

        waud += audio_bytes_per_sample;
        if (waud >= AUDBUFSIZE)
            waud -= AUDBUFSIZE;
    }
    new_audio_samps += samples;

    audbuf_timecode = timecode + (int)((new_audio_samps * 100000.0) / effdsp);
    lastaudiolen = audiolen(false);

    pthread_mutex_unlock(&audio_buflock);
}    

void NuppelVideoPlayer::AddAudioData(char *buffer, int len, long long timecode)
{
    int afree = audiofree(true);

    if (len > afree)
    {
        cout << "Audio buffer overflow, audio data lost!\n";
        len = afree;
    }

    pthread_mutex_lock(&audio_buflock);

    int bdiff = AUDBUFSIZE - waud;
    if (bdiff < len)
    {
        memcpy(audiobuffer + waud, buffer, bdiff);
        memcpy(audiobuffer, buffer + bdiff, len - bdiff);
    }
    else
        memcpy(audiobuffer + waud, buffer, len);

    waud = (waud + len) % AUDBUFSIZE;

    lastaudiolen = audiolen(false);

    /* we want the time at the end -- but the file format stores
       time at the start of the chunk. */
    audbuf_timecode = timecode + (int)((len*100000.0) / 
                                       (audio_bytes_per_sample * effdsp));

    pthread_mutex_unlock(&audio_buflock);
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
    while (vbuffer_numfree() == 0 && !unsafe)
    {
        //cout << "waiting for video buffer to drain.\n";
        prebuffering = false;
        usleep(2000);
    }

    decoder->GetFrame(onlyvideo);

    if (vbuffer_numvalid() >= usepre)
        prebuffering = false;
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
    nexttrigger->tv_usec += (int)(1000000 / video_frame_rate);
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

unsigned char *NuppelVideoPlayer::GetCurrentFrame(int &w, int &h)
{
    w = video_width;
    h = video_height;
    return vbuffer[rpos];
}

void NuppelVideoPlayer::ShowPip(unsigned char *xvidbuf)
{
    int pipw, piph;

    unsigned char *pipimage = pipplayer->GetCurrentFrame(pipw, piph);

    if (!pipimage)
        return;

    int xoff = 50;
    int yoff = 50;
    
    for (int i = 0; i < piph; i++)
    {
        memcpy(xvidbuf + (i + yoff) * video_width + xoff,
               pipimage + i * pipw, pipw);
    }

    xoff /= 2;
    yoff /= 2;

    unsigned char *uptr = xvidbuf + video_width * video_height;
    unsigned char *vptr = xvidbuf + video_width * video_height * 5 / 4;
    int vidw = video_width / 2;

    unsigned char *pipuptr = pipimage + pipw * piph;
    unsigned char *pipvptr = pipimage + pipw * piph * 5 / 4;
    pipw /= 2;

    for (int i = 0; i < piph / 2; i ++)
    {
        memcpy(uptr + (i + yoff) * vidw + xoff, pipuptr + i * pipw, pipw);
        memcpy(vptr + (i + yoff) * vidw + xoff, pipvptr + i * pipw, pipw);
    }
}

int NuppelVideoPlayer::CheckEvents(void)
{
    int ret = 0;
    if (videoOutput && eventvalid)
    {
        pthread_mutex_lock(&eventLock);
        if (videoOutput && eventvalid)
            ret = videoOutput->CheckEvents();
        pthread_mutex_unlock(&eventLock);
    } 
    return ret;
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

void NuppelVideoPlayer::ToggleFullScreen(void)
{
    if (videoOutput)
        videoOutput->ToggleFullScreen();
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
    // check if subtitles need to be updated on the OSD
    if (tbuffer_numvalid() && txtbuffers[rtxt].timecode && 
        (txtbuffers[rtxt].timecode <= timecodes[rpos]))
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

void NuppelVideoPlayer::OutputVideoLoop(void)
{
    int laudiotime;
    int delay;
    int avsync_delay = 0;
    int avsync_avg = 0;

    int straddle_avoid = 2000;

    bool hasvsync = false;
    bool hasvgasync = false;

    struct timeval nexttrigger;

    bool reducejitter = gContext->GetNumSetting("ReduceJitter", 0);

    // Jitterometer *output_jmeter = new Jitterometer("video_output", 100);

    if (!disablevideo)
    {
        eventvalid = true;
    }

    int frame_interval = (int)(1000000/video_frame_rate);

    Frame frame;
    
    frame.codec = CODEC_YUV;
    frame.width = video_width;
    frame.height = video_height;
    frame.bpp = -1;
    frame.frameNumber = framesPlayed;

    int pause_rpos = 0;
    unsigned char *pause_buf = new unsigned char[video_size];

    gettimeofday(&nexttrigger, NULL);

    QString timing_type = "next trigger";

    if (reducejitter)
    {
        int ret = vsync_init();

        if (ret > 0)
        {
            hasvsync = true;

            if ( ret == 1 )
                timing_type = "nVidia polling";

            else if ( ret == 2 )
                timing_type = "DRM vblank";
        }

        /*
        // still to be implemented
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

    cout << "Video timing method: " << timing_type << endl;

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
            {
                pause_rpos = rpos;
                memcpy(pause_buf, vbuffer[pause_rpos], video_size);
            }
            if (resetvideo)
            {
                pause_rpos = vpos;
                memcpy(pause_buf, vbuffer[pause_rpos], video_size);
                resetvideo = false;
            }

            video_actually_paused = true;

            //printf("video waiting for unpause\n");
            usleep(5000);

            if (!disablevideo)
            {
                memcpy(vbuffer[MAXVBUFFER], pause_buf, video_size);
                frame.buf = vbuffer[MAXVBUFFER];
                if (videoFilters.size() > 0)
                    process_video_filters(&frame, &videoFilters[0],
                                          videoFilters.size());
                if (pipplayer)
                    ShowPip(vbuffer[MAXVBUFFER]);
                osd->Display(vbuffer[MAXVBUFFER]);
                videoOutput->PrepareFrame(vbuffer[MAXVBUFFER], video_width, 
                                  video_height);
                videoOutput->Show();
                ResetNexttrigger(&nexttrigger);
            }
            continue;
        }
        video_actually_paused = false;
        resetvideo = false;

        if (prebuffering)
        {
            //printf("prebuffering...\n");
            usleep(200);
            ResetNexttrigger(&nexttrigger);
            continue;
        }

        if (vbuffer_numvalid() < 2)
        {
           prebuffering = true;
           continue;
        }

        /* if we get here, we're actually going to do video output */

        if (!disablevideo)
        {
            frame.buf = vbuffer[rpos];
            if (videoFilters.size() > 0)
                process_video_filters(&frame, &videoFilters[0],
                                      videoFilters.size());

            if (pipplayer)
                ShowPip(vbuffer[rpos]);

            if (cc)
                ShowText();

            osd->Display(vbuffer[rpos]);
        }

        /* Delay until it is time to show the next frame */
        delay = UpdateDelay(&nexttrigger);

        /* If delay is sometwhat more than a frame or < 0ms, 
           we clip it to these amounts and reset nexttrigger */
        if ( delay > frame_interval * 2)
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

        if (disablevideo)
        {
            if (delay > 0)
                usleep(delay);
        }
        else
        {
            videoOutput->PrepareFrame(vbuffer[rpos], video_width,
                                      video_height);

            delay = UpdateDelay(&nexttrigger);

            if (hasvsync)
            {
                if (delay > 10000)
                {
                    sched_yield();
                    delay = UpdateDelay(&nexttrigger);
                }

                while (delay + straddle_avoid > 0)
                {
                    vsync_wait_for_retrace();
                    delay = UpdateDelay(&nexttrigger);

                    /* If fps and refresh rate are equal and delay is near 0,
                       delay could fall on either side of 0 causing jitter. */

                    if ( delay + straddle_avoid > 0 &&
                         delay + straddle_avoid < 1000 )
                    {
                        straddle_avoid = 0 - straddle_avoid;
                        // cerr << "Flip straddle " << straddle_avoid << endl;
                    }
                }
            }
            else
            {
                if (reducejitter)
                    ReduceJitter(&nexttrigger);
                else
                {
		    if (delay > 0)
                        usleep(delay);
                }
            }

            // delay = UpdateDelay(&nexttrigger);
            // cerr << timecodes[rpos] << " "
            //     << delay * 100 / frame_interval <<  endl;

            /* This should be the best possile time to update the frame */
            videoOutput->Show();

        }

        /* a/v sync assumes that when 'Show' returns, that is the instant
           the frame has become visible on screen */

        //if (output_jmeter->RecordCycleTime())
        //    cout << avsync_delay  / 1000 << endl;


        /* The value of nexttrigger is perfect -- we calculated it to
           be exactly one frame time after the previous frame, 
           Show frames at the frame rate as long as audio is in sync */

        nexttrigger.tv_usec += frame_interval;
        
        if (audiofd > 0)
        {
            laudiotime = GetAudiotime(); // ms, same scale as timecodes
            
            if (laudiotime != 0) // laudiotime = 0 after a seek
            {
                /* The time at the start of this frame (ie, now) is
                   given by timecodes[rpos] */

                avsync_delay = (timecodes[rpos] - laudiotime) * 1000; // uSecs

                avsync_avg = (avsync_avg + (avsync_delay * 3)) / 4;

                /* If the audio time codes and video diverge, shift 
                   the video by one interlaced field (1/2 frame) */

                if (avsync_avg > frame_interval)
                {
                    nexttrigger.tv_usec += frame_interval / 2; // re-syncing
                    // cerr << timecodes[rpos] << " added field\n";
                }
                if (avsync_avg < 0 - frame_interval)
                {
                    nexttrigger.tv_usec -= frame_interval / 2; // re-syncing
                    // cerr << timecodes[rpos] << " dropped field\n";
                }
            }
        }

        NormalizeTimeval(&nexttrigger);

        /* update rpos */
        pthread_mutex_lock(&video_buflock);
        if (rpos != wpos) // if a seek occurred, rpos == wpos, in this case do
                          // nothing
            rpos = (rpos + 1) % MAXVBUFFER;
        pthread_mutex_unlock(&video_buflock);

        sched_yield();
    }

    delete [] pause_buf;
 
    if (!disablevideo)
    {
        pthread_mutex_lock(&eventLock);
        delete videoOutput;
        videoOutput = NULL;
        eventvalid = false;
        pthread_mutex_unlock(&eventLock);
    }

    if (hasvsync)
        vsync_shutdown();

    if (hasvgasync)
        vgasync_cleanup();
}

inline int NuppelVideoPlayer::getSpaceOnSoundcard(void)
{
    audio_buf_info info;
    int space = 0;

    ioctl(audiofd, SNDCTL_DSP_GETOSPACE, &info);
    space = info.bytes - audio_buffer_unused;

    if (space < 0)
    {
        numbadioctls++;
        if (numbadioctls > 2 || space < -5000)
        {
            cerr << "Your soundcard is not reporting free space correctly.\n"
                 << "Falling back to old method...\n";
            audio_buffer_unused = 0;
            space = info.bytes;
        }
    }
    else
        numbadioctls = 0;

    return space;
}

void NuppelVideoPlayer::OutputAudioLoop(void)
{
    int bytesperframe;
    int space_on_soundcard;
    unsigned char zeros[1024];
 
    bzero(zeros, 1024);

    while (!eof && !killaudio)
    {
	if (audiofd <= 0) 
	    break;

	if (pauseaudio)
	{
            audio_actually_paused = true;
            //usleep(50);
            audiotime = 0; // mark 'audiotime' as invalid.

            space_on_soundcard = getSpaceOnSoundcard();
            if (1024 < space_on_soundcard)
            {
                WriteAudio(zeros, 1024);
            }
            else
            {
             //printf("waiting for space to write 1024 zeros on soundcard which has %d bytes free\n",space_on_soundcard);
                usleep(50);
            }

            continue;
	}    
	
        if (prebuffering)
        {
	    audiotime = 0; // mark 'audiotime' as invalid

            space_on_soundcard = getSpaceOnSoundcard();
            if (1024 < space_on_soundcard)
            {
                WriteAudio(zeros, 1024);
            }
            else
            {
             //printf("waiting for space to write 1024 zeros on soundcard which has %d bytes free\n",space_on_soundcard);
                usleep(50);
            }

	    //printf("audio thread waiting for prebuffer\n");
	    continue;
        }

        SetAudiotime(); // once per loop, calculate stuff for a/v sync

        /* do audio output */
	
        /* approximate # of audio bytes for each frame. */
        bytesperframe = audio_bytes_per_sample * 
                        (int)(effdsp / 100.0 / video_frame_rate + 0.5);
	
        // wait for the buffer to fill with enough to play
        if (bytesperframe >= audiolen(true))
        { 
            //printf("audio thread waiting for buffer to fill\n");
            usleep(200);
            continue;
        }
	
        // wait for there to be free space on the sound card so we can write
        // without blocking.  We don't want to block while holding audio_buflock
	
        space_on_soundcard = getSpaceOnSoundcard();
        if (bytesperframe > space_on_soundcard)
        {
            //printf("waiting for space to write %d bytes on soundcard whish has %d bytes free\n", bytesperframe, space_on_soundcard);
            numlowbuffer++;
            if (numlowbuffer > 5 && audio_buffer_unused)
            {
                cerr << "dropping back audio_buffer_unused\n";
                audio_buffer_unused /= 2;
            }

            usleep(200);
            continue;
        }
        else
            numlowbuffer = 0;

        pthread_mutex_lock(&audio_buflock); // begin critical section

        // re-check audiolen() in case things changed.
        // for example, ClearAfterSeek() might have run
        if (bytesperframe < audiolen(false))
        {
            int bdiff = AUDBUFSIZE - raud;
            if (bytesperframe > bdiff)
            {
                WriteAudio(audiobuffer + raud, bdiff);
                WriteAudio(audiobuffer, bytesperframe - bdiff);
            }
            else
            {
                WriteAudio(audiobuffer + raud, bytesperframe);
            }

            /* update raud */
            raud = (raud + bytesperframe) % AUDBUFSIZE;
        }
        pthread_mutex_unlock(&audio_buflock); // end critical section
    }
    //ioctl(audiofd, SNDCTL_DSP_RESET, NULL);
}

void *NuppelVideoPlayer::kickoffOutputAudioLoop(void *player)
{
    ((NuppelVideoPlayer *)player)->OutputAudioLoop();
    return NULL;
}

void *NuppelVideoPlayer::kickoffOutputVideoLoop(void *player)
{
    ((NuppelVideoPlayer *)player)->OutputVideoLoop();
    return NULL;
}

void NuppelVideoPlayer::FastForward(float seconds)
{
    if (fftime == 0)
        fftime = (int)(seconds * video_frame_rate);
}

void NuppelVideoPlayer::Rewind(float seconds)
{
    if (rewindtime == 0)
        rewindtime = (int)(seconds * video_frame_rate);
}

void NuppelVideoPlayer::SkipCommercials(int direction)
{
    if (skipcommercials == 0)
        skipcommercials = direction;
}

void NuppelVideoPlayer::StartPlaying(void)
{
    consecutive_blanks = 0;

    killplayer = false;

    framesPlayed = 0;

    if (OpenFile() < 0)
        return;

    InitSound();
    InitFilters();

    if (!disablevideo)
    {
        InitVideo();
        int dispx = 0, dispy = 0, dispw = video_width, disph = video_height;
        videoOutput->GetDrawSize(dispx, dispy, dispw, disph);
        osd = new OSD(video_width, video_height, (int)ceil(video_frame_rate),
                      osdfontname, osdccfontname, osdprefix, osdtheme,
                      dispx, dispy, dispw, disph);
    }
    else
        own_vidbufs = true;

    playing = true;
  
    audbuf_timecode = 0;
    audiotime = 0;
    gettimeofday(&audiotime_updated, NULL);

    rewindtime = fftime = 0;
    skipcommercials = 0;

    resetplaying = false;
    
    pthread_mutex_init(&audio_buflock, NULL);
    pthread_mutex_init(&video_buflock, NULL);
    pthread_mutex_init(&text_buflock, NULL);
    pthread_mutex_init(&avsync_lock, NULL);

    if (own_vidbufs)
    {
        // initialize and purge buffers
        for (int i = 0; i <= MAXVBUFFER; i++)
            vbuffer[i] = new unsigned char[video_size];
    }

    for (int i = 0; i < MAXTBUFFER; i++)
        txtbuffers[i].buffer = new unsigned char[text_size];

    wpos = 0;
    ClearAfterSeek();

    /* This thread will fill the video and audio buffers, it does all CPU
       intensive operations. We fork two other threads which do nothing but
       write to the audio and video output devices.  These should use a 
       minimum of CPU. */

    pthread_t output_audio, output_video;
    pthread_create(&output_audio, NULL, kickoffOutputAudioLoop, this);
    pthread_create(&output_video, NULL, kickoffOutputVideoLoop, this);

    int pausecheck = 0;

    if (bookmarkseek > 30)
    {
        GetFrame(audiofd <= 0);

        bool seeks = exactseeks;

        decoder->setExactSeeks(false);

        fftime = bookmarkseek;
        DoFastForward();
        fftime = 0;

        decoder->setExactSeeks(seeks);
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

    while (!eof && !killplayer)
    {
	if (resetplaying)
	{
            ClearAfterSeek();

	    framesPlayed = 0;

            decoder->Reset();

	    resetplaying = false;
	    actuallyreset = true;
        }
	    
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
                
                GetFrame(audiofd <= 0);
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

                GetFrame(audiofd <= 0);
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

        GetFrame(audiofd <= 0);

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

    killvideo = killaudio = true;

    // these threads will also exit when killplayer or eof is true
    pthread_join(output_video, NULL);
    pthread_join(output_audio, NULL);

    close(audiofd);
    playing = false;
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

    decoder->DoRewind(desiredFrame);

    ClearAfterSeek();
    return true;
}

long long NuppelVideoPlayer::CalcMaxFFTime(long long ff)
{
    long long maxtime = (long long)(1.0 * video_frame_rate);
    if (watchingrecording && nvr_enc && nvr_enc->IsValidRecorder())
        maxtime = (long long)(3.0 * video_frame_rate);
    
    long long ret = ff;

    if (livetv || (watchingrecording && nvr_enc && nvr_enc->IsValidRecorder()))
    {
        long long behind = nvr_enc->GetFramesWritten() - framesPlayed;
        if (behind < maxtime) // if we're close, do nothing
            ret = 0;
        else if (behind - ff <= maxtime)
            ret = behind - maxtime;
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

    decoder->DoFastForward(desiredFrame);

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
    /* caller to this function should not hold any locks, we acquire all three
       right here */

    pthread_mutex_lock(&audio_buflock);
    pthread_mutex_lock(&video_buflock);
    pthread_mutex_lock(&avsync_lock);

    for (int i = 0; i < MAXVBUFFER; i++)
    {
        timecodes[i] = 0;
    }
    vpos = wpos;
    rpos = wpos;

    for (int i = 0; i < MAXTBUFFER; i++)
    {
        txtbuffers[i].timecode = 0;
    }
    wtxt = 0;
    rtxt = 0;
    raud = waud = 0;
    audbuf_timecode = 0;
    audiotime = 0;
    gettimeofday(&audiotime_updated, NULL);
    prebuffering = true;

    if (osd)
        osd->ClearAllCCText();

    //if (audiofd)
    //    ioctl(audiofd, SNDCTL_DSP_RESET, NULL);

    pthread_mutex_unlock(&avsync_lock);
    pthread_mutex_unlock(&video_buflock);
    pthread_mutex_unlock(&audio_buflock);

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
            case wsUp: osd->DialogUp(dialogname); break;
            case wsDown: osd->DialogDown(dialogname); break;
            case ' ': case wsEnter: case wsReturn: 
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
        case ' ': case wsEnter: case wsReturn:
        {
            HandleSelect();
            break;
        }
        case wsLeft: case 'a': case 'A': 
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
        case wsRight: case 'd': case 'D':
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
        case wsUp: 
        {
            UpdateSeekAmount(true);
            UpdateTimeDisplay();
            break;
        }
        case wsDown:
        {
            UpdateSeekAmount(false);
            UpdateTimeDisplay();
            break; 
        }
        case 'c': case 'C':
        {
            QMap<long long, int>::Iterator it;
            for (it = deleteMap.begin(); it != deleteMap.end(); ++it)
                osd->HideEditArrow(it.key(), it.data());

            deleteMap.clear();
            UpdateEditSlider();
            break;
        }
        case 'z': case 'Z':
        {
            if (hascommbreaktable)
            {
                QMap<long long, int>::Iterator it;
                for (it = commBreakMap.begin(); it != commBreakMap.end(); ++it)
                {
                    if (!deleteMap.contains(it.key()))
                        if (it.data() == MARK_COMM_START)
                            AddMark(it.key(), 1);
                        else
                            AddMark(it.key(), 0);
                }
            }
            break;
        }
        case wsEscape: case 'e': case 'E': case 'm': case 'M':
        {
            DisableEdit();
            break;
        }
        default: break;
    }

    decoder->setExactSeeks(exactstore);
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
        case 0: text = "cut point"; seekamount = -2; break;
        case 1: text = "keyframe"; seekamount = -1; break;
        case 2: text = "1 frame"; seekamount = 1; break;
        case 3: text = "0.5 seconds"; seekamount = fps / 2; break;
        case 4: text = "1 second"; seekamount = fps; break;
        case 5: text = "5 seconds"; seekamount = fps * 5; break;
        case 6: text = "20 seconds"; seekamount = fps * 20; break;
        case 7: text = "1 minute"; seekamount = fps * 60; break;
        case 8: text = "5 minutes"; seekamount = fps * 300; break;
        case 9: text = "10 minutes"; seekamount = fps * 600; break;
        default: text = "error"; seekamount = fps; break;
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

    QString cutmarker = "";
    if (IsInDelete(framesPlayed))
        cutmarker = "cut";

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
        QString message = "You are close to an existing cut point.  Would you "
                          "like to:";
        QString option1 = "Delete this cut point";
        QString option2 = "Move this cut point to the current position";
        QString option3 = "Flip directions - delete to the ";
        if (direction == 0)
            option3 += "right";
        else
            option3 += "left";
        QString option4 = "Cancel";

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
        QString message = "Insert a new cut point?";
        QString option1 = "Delete before this frame";
        QString option2 = "Delete after this frame";
        QString option3 = "Cancel";

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
        if (result == 1)
            DeleteMark(deleteframe);
        if (result == 2)
        {
            DeleteMark(deleteframe);
            AddMark(framesPlayed, type);
        }
        else if (result == 3)
        {
            DeleteMark(deleteframe);
            AddMark(deleteframe, 1 - type);
        }
    }
    else if (dialogtype == 1)
    {
        if (result == 1)
            AddMark(framesPlayed, 0);
        else if (result == 2)
            AddMark(framesPlayed, 1);
    }
}

void NuppelVideoPlayer::UpdateEditSlider(void)
{
    osd->DoEditSlider(deleteMap, framesPlayed, totalFrames);
}

void NuppelVideoPlayer::AddMark(long long frames, int type)
{
    deleteMap[frames] = type;
    osd->ShowEditArrow(frames, totalFrames, type);
    UpdateEditSlider();
}

void NuppelVideoPlayer::DeleteMark(long long frames)
{
    osd->HideEditArrow(frames, deleteMap[frames]);
    deleteMap.remove(frames);
    UpdateEditSlider();
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

    pthread_mutex_init(&audio_buflock, NULL);
    pthread_mutex_init(&video_buflock, NULL);
    pthread_mutex_init(&text_buflock, NULL);
    pthread_mutex_init(&avsync_lock, NULL);

    own_vidbufs = true;

    for (int i = 0; i <= MAXVBUFFER; i++)
        vbuffer[i] = new unsigned char[video_size];

    for (int i = 0; i < MAXTBUFFER; i++)
        txtbuffers[i].buffer = new unsigned char[text_size];

    wpos = 0;
    ClearAfterSeek();

    unsigned char *frame = (unsigned char *)decoder->GetScreenGrab(secondsin); 

    if (!frame)
    {
        bufflen = 0;
        vw = vh = 0;
        return NULL;
    }

    AVPicture orig, retbuf;
    avpicture_fill(&orig, frame, PIX_FMT_YUV420P, video_width, video_height);
 
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

bool NuppelVideoPlayer::ReencodeFile(char *inputname, char *outputname,
                                     RecordingProfile &profile)
{ 
    NuppelVideoRecorder *nvr;

    inputname = inputname;
    outputname = outputname;

    filename = inputname;
     
    int audioframesize = 4096 * 4;
    int audioFrame = 0;
    // Input setup
    nvr = new NuppelVideoRecorder;
    ringBuffer = new RingBuffer(filename, false, false);

    if (OpenFile(false) < 0)
    {
        delete nvr;
        return false;
    }

    // Recorder setup
    nvr->SetFrameRate(video_frame_rate);

    // this is ripped from tv_rec SetupRecording. It'd be nice to consolodate
    nvr->SetOption("width", video_width);
    nvr->SetOption("height", video_height);

    nvr->SetOption("tvformat", gContext->GetSetting("TVFormat"));
    nvr->SetOption("vbiformat", gContext->GetSetting("VbiFormat"));

    QString setting = profile.byName("videocodec")->getValue();
    if (setting == "MPEG-4") 
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
    else if (setting == "RTjpeg") 
    {
        nvr->SetOption("codec", "rtjpeg");
        SetProfileOption(profile, "rtjpegquality");
        SetProfileOption(profile, "rtjpegchromafilter");
        SetProfileOption(profile, "rtjpeglumafilter");
    } 
    else 
    {
        cerr << "Unknown video codec: " << setting << endl;
    }

    setting = profile.byName("audiocodec")->getValue();
    if (setting == "MP3") 
    {
        nvr->SetOption("audiocompression", 1);
        SetProfileOption(profile, "mp3quality");
        SetProfileOption(profile, "samplerate");
        decoder->SetRawFrameState(true);
    } 
    else if (setting == "Uncompressed") 
    {
        nvr->SetOption("audiocompression", 0);
    } 
    else 
    {
        cerr << "Unknown audio codec: " << setting << endl;
    }

    nvr->AudioInit(true);

    RingBuffer *outRingBuffer = new RingBuffer(outputname, true, false);
    nvr->SetRingBuffer(outRingBuffer);
    nvr->WriteHeader(false);
    nvr->StreamAllocate();

    playing = true;

    pthread_mutex_init(&audio_buflock, NULL);
    pthread_mutex_init(&video_buflock, NULL);
    pthread_mutex_init(&text_buflock, NULL);
    pthread_mutex_init(&avsync_lock, NULL);

    own_vidbufs = true;

    for (int i = 0; i <= MAXVBUFFER; i++)
        vbuffer[i] = new unsigned char[video_size];

    for (int i = 0; i < MAXTBUFFER; i++)
        txtbuffers[i].buffer = new unsigned char[text_size];

    wpos = 0;
    ClearAfterSeek();

    Frame frame;
    frame.codec = CODEC_YUV;
    frame.width = video_width;
    frame.height = video_height;
    frame.len = video_width * video_height * 3 / 2;

    decoder->GetFrame(0);
    int tryraw = decoder->GetRawFrameState();
    while (!eof)
    {
        frame.buf = vbuffer[vpos];
        frame.timecode = timecodes[vpos];
        frame.frameNumber = framesPlayed;

        if (tryraw)
        {
            // Encoding from NuppelVideo to NuppelVideo with MP3 audio
            // So let's not decode/reencode audio
            if (!decoder->GetRawFrameState()) 
            {
                // The Raw stae changed during decode.  This is not good
                delete outRingBuffer;
                delete nvr;
                unlink(outputname);
                return false;
            }

            decoder->WriteStoredData(outRingBuffer);
            nvr->WriteVideo(&frame, true, decoder->isLastFrameKey());
            raud = waud;
            rtxt = wtxt;
        } 
        else 
        {
            // audio is fully decoded, so we need to reencode it
            if ((audioframesize = audiolen(false))) 
            {
                nvr->SetOption("audioframesize", audioframesize);
                int starttime = audbuf_timecode - 
                                (int)((audioframesize * 100000.0)
                                / (audio_bytes_per_sample * effdsp));
                nvr->WriteAudio(audiobuffer + raud, audioFrame++, starttime);
                /* update raud */
                raud = (raud + audioframesize) % AUDBUFSIZE;
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

            nvr->WriteVideo(&frame);
        }
    
        decoder->GetFrame(0);
    }

    if (!tryraw)
        nvr->WriteSeekTable(false);
    delete outRingBuffer;
    delete nvr;
    return true;
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

    pthread_mutex_init(&audio_buflock, NULL);
    pthread_mutex_init(&video_buflock, NULL);
    pthread_mutex_init(&text_buflock, NULL);
    pthread_mutex_init(&avsync_lock, NULL);

    own_vidbufs = true;

    for (int i = 0; i <= MAXVBUFFER; i++)
        vbuffer[i] = new unsigned char[video_size];

    for (int i = 0; i < MAXTBUFFER; i++)
        txtbuffers[i].buffer = new unsigned char[text_size];

    wpos = 0;
    ClearAfterSeek();

    if ( ! (commercialskipmethod & 0x01))
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

        commDetect->ProcessNextFrame(vbuffer[vpos], framesPlayed);

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

bool NuppelVideoPlayer::FrameIsBlank(int vposition)
{
    commDetect->ProcessNextFrame(vbuffer[vposition]);

    return(commDetect->FrameIsBlank());
}

void NuppelVideoPlayer::AutoCommercialSkip(void)
{
    if (hascommbreaktable)
    {
        if (commBreakIter.data() == MARK_COMM_END)
            commBreakIter++;

        if ((commBreakIter.data() == MARK_COMM_START) && 
            (framesPlayed >= commBreakIter.key()))
        {
            ++commBreakIter;
            if (commBreakIter.key() == totalFrames)
                eof = 1;
            else
            {
                if (osd)
                {
                    int skipped_seconds = (int)((commBreakIter.key() -
                            framesPlayed) / video_frame_rate);
                    QString comm_msg = QString("Auto-Skip %1 seconds")
                                      .arg(skipped_seconds);
                    int spos = GetStatusbarPos();
                    osd->StartPause(spos, false, "SKIP", comm_msg, 1);
                }

                JumpToFrame(commBreakIter.key());
            }

            GetFrame(1, true);
            while (LastFrameIsBlank())
                GetFrame(1, true);

            ++commBreakIter;
        }
        return;
    }

    if (autocommercialskip == COMMERCIAL_SKIP_BLANKS)
    {
        if (LastFrameIsBlank())
        {
            PauseVideo();

            consecutive_blanks++;
            if (consecutive_blanks >= 3)
            {
                int saved_position = framesPlayed;
                JumpToNetFrame((long long int)(30 * video_frame_rate - 3));

                // search a 10-frame window for another blank
                int tries = 10;

                GetFrame(1, true);
                while ((tries > 0) && (!LastFrameIsBlank()))
                {
                     GetFrame(1, true);
                     tries--;
                }

                if (tries)
                {
                    // found another blank
 
                    consecutive_blanks = 0;
                    UnpauseVideo();
                    return;
                }
                else
                {
                    // no blank found so exit auto-skip
                    JumpToFrame(saved_position + 1);
                    UnpauseVideo();
                    return;
                }
            }

            UnpauseVideo();
        }
        else if (consecutive_blanks)
        {
            consecutive_blanks = 0;
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
                    int spos = GetStatusbarPos();

                    blank_seq_found = 1;

                    commercials_found++;
                    comm_msg = QString("Found %1 sec. commercial")
                                      .arg(*comm_length);
       
                    osd->StartPause(spos, false, "SKIP", comm_msg, 5);
                }
                break;
            }

            comm_length++;
        }
    } while (blank_seq_found);

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
            QString comm_msg = QString("At Start of program.");
            int spos = GetStatusbarPos();
            osd->StartPause(spos, false, "SKIP", comm_msg, 1);

            JumpToFrame(0);
            return true;
        }

        if ((commBreakIter == commBreakMap.end()) &&
            (direction > 0))
        {
            QString comm_msg = QString("At End, can not Skip.");
            int spos = GetStatusbarPos();
            osd->StartPause(spos, false, "SKIP", comm_msg, 1);
            return false;
        }

        if (direction < 0)
        {
            commBreakIter--;

            int skipped_seconds = (int)((commBreakIter.key() -
                    framesPlayed) / video_frame_rate);

            if (skipped_seconds > -3)
            {
                if (commBreakIter == commBreakMap.begin())
                {
                    QString comm_msg = QString("Start of program.");
                    int spos = GetStatusbarPos();
                    osd->StartPause(spos, false, "SKIP", comm_msg, 1);

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
            QString comm_msg = QString("Auto-Skip %1 seconds")
                                  .arg(skipped_seconds);
            int spos = GetStatusbarPos();
            osd->StartPause(spos, false, "SKIP", comm_msg, 1);
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
