#ifndef NUPPELVIDEOPLAYER
#define NUPPELVIDEOPLAYER

#include <map>
#include <vector>
#include <qstring.h>

#ifdef MMX
#undef MMX
#define MMXBLAH
#endif
#include <lame/lame.h>
#ifdef MMXBLAH
#define MMX
#endif

#include "RTjpegN.h"
#include "format.h"
#include "RingBuffer.h"
#include "osd.h"
#include "jitterometer.h"

extern "C" {
#include "../libavcodec/avcodec.h"
#include "filter.h"
}
using namespace std;

#define MAXVBUFFER 21
#define AUDBUFSIZE 512000

class NuppelVideoRecorder;
class NuppelVideoPlayer;
class XvVideoOutput;

class NuppelVideoPlayer
{
 public:
    NuppelVideoPlayer();
   ~NuppelVideoPlayer();

    void SetAsPIP(void) { disableaudio = disablevideo = true; }

    void SetAudioDevice(QString device) { audiodevice = device; }
    void SetFileName(QString lfilename) { filename = lfilename; }

    void SetExactSeeks(bool exact) { exactseeks = exact; }

    void StartPlaying(void);
    void StopPlaying(void) { killplayer = true; }
    
    bool IsPlaying(void) { return playing; }

    void SetRingBuffer(RingBuffer *rbuf) { ringBuffer = rbuf; }

    void SetAudioSampleRate(int rate) { audio_samplerate = rate; }

    bool TogglePause(void) { if (paused) Unpause(); else Pause(); 
                             return paused; 
                           }
    void Pause(void) { paused = true; actuallypaused = false; 
                       audio_actually_paused = false; 
                       video_actually_paused = false; }
    void Unpause(void) { paused = false; }
    bool GetPause(void);
   
    bool ToggleEdit(void) { editmode = !editmode; return editmode; }
    void AdvanceOneFrame(void) { advancedecoder = true; } 
 
    void FastForward(float seconds) 
                                { fftime = (int)(seconds * video_frame_rate); }
    void Rewind(float seconds) 
                            { rewindtime = (int)(seconds * video_frame_rate); }

    void ResetPlaying(void) { resetplaying = true; actuallyreset = false; }
    bool ResetYet(void) { return actuallyreset; }

    float GetFrameRate(void) { return video_frame_rate; } 
    long long GetFramesPlayed(void) { return framesPlayed; }
    void ResetFramesPlayed(void) { framesPlayed = 0; }

    void SetRecorder(NuppelVideoRecorder *nvcr) { nvr = nvcr; }

    OSD *GetOSD(void) { return osd; }

    void SetOSDFontName(QString filename, QString prefix) 
                      { osdfilename = filename; osdprefix = prefix; }
    void SetOSDThemeName(QString themename) { osdtheme = themename; }

    // don't use this on something you're playing
    char *GetScreenGrab(int secondsin, int &buflen, int &vw, int &vh);

    void SetLength(int len) { totalLength = len; }

    void ReencodeFile(char *inputname, char *outputname);

    unsigned char *GetCurrentFrame(int &w, int &h);

    void SetPipPlayer(NuppelVideoPlayer *pip) { setpipplayer = pip; 
                                                needsetpipplayer = true; }
    bool PipPlayerSet(void) { return !needsetpipplayer; }
 
    void SetVideoFilters(QString &filters) { videoFilterList = filters; }

    int CheckEvents(void); 

    void SetWatchingRecording(bool mode) { watchingrecording = mode; }
    void SetBookmark(void);

    void ToggleFullScreen(void);

 protected:
    void OutputAudioLoop(void);
    void OutputVideoLoop(void);

    static void *kickoffOutputAudioLoop(void *player);
    static void *kickoffOutputVideoLoop(void *player);
    
 private:
    void InitSound(void);
    void WriteAudio(unsigned char *aubuf, int size);

    void InitFilters(void);
    int InitSubs(void);
    
    int OpenFile(bool skipDsp = false);
    int CloseFile(void);

    int GetAudiotime(void);
    void SetAudiotime(void);
    unsigned char *DecodeFrame(struct rtframeheader *frameheader,
                               unsigned char *strm);
    void GetFrame(int onlyvideo);
    
    long long CalcMaxFFTime(long long ff);
   
    bool DoFastForward();
    bool DoRewind();
   
    void ClearAfterSeek(); // caller should not hold any locks
    
    int audiolen(bool use_lock); // number of valid bytes in audio buffer
    int audiofree(bool use_lock); // number of free bytes in audio buffer
    int vbuffer_numvalid(void); // number of valid slots in video buffer
    int vbuffer_numfree(void); // number of free slots in the video buffer

    void ResetNexttrigger(struct timeval *tv);

    void ShowPip(unsigned char *xvidbuf);

    int audiofd;

    QString filename;
    QString audiodevice;
    
    /* rtjpeg_plugin stuff */
    int eof;
    int video_width;
    int video_height;
    double video_frame_rate;
    unsigned char *buf;
    unsigned char *buf2;
    char lastct;
    int effdsp; // from the recorded stream
    int audio_samplerate; // rate to tell the output device
    int filesize;
    int startpos;

    int lastaudiolen;
    unsigned char *strm;
    struct rtframeheader frameheader;

    /* Video circular buffer */
    int wpos;		/* next slot to write */
    int rpos;		/* next slot to read */
    int usepre;		/* number of slots to keep full */
    bool prebuffering;	/* don't play until done prebuffering */ 
    int timecodes[MAXVBUFFER];	/* timecode for each slot */
    unsigned char *vbuffer[MAXVBUFFER];	/* decompressed video data */

    /* Audio circular buffer */
    unsigned char audiobuffer[AUDBUFSIZE];	/* buffer */
    unsigned char tmpaudio[AUDBUFSIZE];		/* temporary space */
    int raud, waud;		/* read and write positions */
    int audbuf_timecode;	/* timecode of audio most recently placed into
				   buffer */

    pthread_mutex_t audio_buflock; /* adjustments to audiotimecode, waud, and
                                      raud can only be made while holding this
                                      lock */
    pthread_mutex_t video_buflock; /* adjustments to rpos and wpos can only
                                      be made while holding this lock */

    /* A/V Sync state */
  
    pthread_mutex_t avsync_lock; /* must hold avsync_lock to read or write
                                    'audiotime' and 'audiotime_updated' */
    int audiotime; // timecode of audio leaving the soundcard (same units as
                   //                                          timecodes) ...
    struct timeval audiotime_updated; // ... which was last updated at this time

    /* Locking note:
       A thread needing multiple locks must acquire in this order:
  
       1. audio_buflock
       2. video_buflock
       3. avsync_lock

       and release in reverse order. This should avoid deadlock situations. */

    int weseeked;

    struct rtfileheader fileheader;
    lame_global_flags *gf;

    RTjpeg *rtjd;
    uint8_t *planes[3];

    int paused;
    bool actuallypaused, audio_actually_paused, video_actually_paused;

    bool playing;

    RingBuffer *ringBuffer;
    bool weMadeBuffer; 
    bool killplayer;

    long long framesPlayed;
    
    bool livetv;
    bool watchingrecording;
    bool editmode;
    bool advancevideo;
    bool resetvideo;
    bool advancedecoder;

    map<long long, long long> *positionMap;
    long long lastKey;
    
    long long rewindtime, fftime;
    NuppelVideoRecorder *nvr;

    bool resetplaying;
    bool actuallyreset;

    int totalLength;

    QString osdfilename;
    QString osdprefix;
    QString osdtheme;
    OSD *osd;

    bool InitAVCodec(int codec);
    void CloseAVCodec(void);

    AVCodec *mpa_codec;
    AVCodecContext *mpa_ctx;
    AVPicture mpa_picture;

    bool disablevideo;
    bool disableaudio;

    NuppelVideoPlayer *pipplayer;
    NuppelVideoPlayer *setpipplayer;
    bool needsetpipplayer;

    QString videoFilterList;
    vector<VideoFilter *> videoFilters;

    struct extendeddata extradata;
    bool usingextradata;

    bool haspositionmap;
    int keyframedist;

    bool exactseeks;

    XvVideoOutput *videoOutput;
    pthread_mutex_t eventLock;
    bool eventvalid;
};

#endif
