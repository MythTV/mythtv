#ifndef NUPPELVIDEOPLAYER
#define NUPPELVIDEOPLAYER

#include <string>
#include <map>

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

using namespace std;

#define MAXVBUFFER 21
#define AUDBUFSIZE 512000

class NuppelVideoRecorder;

class NuppelVideoPlayer
{
 public:
    NuppelVideoPlayer();
   ~NuppelVideoPlayer();

    void SetAudioDevice(char *device) { audiodevice = device; }
    void SetFileName(char *lfilename) { filename = lfilename; }

    void StartPlaying(void);
    void StopPlaying(void) { killplayer = true; }
    
    bool IsPlaying(void) { return playing; }

    void SetRingBuffer(RingBuffer *rbuf) { ringBuffer = rbuf; }

    void SetDeinterlace(bool set) { deinterlace = set; }
    bool GetDeinterlace(void) { return deinterlace; }

    void SetAudioSampleRate(int rate) { audio_samplerate = rate; }

    bool TogglePause(void) { return (paused = !paused); }
    void Pause(void) { paused = true; actuallypaused = false; 
                       audio_actually_paused = false; 
                       video_actually_paused = false; }
    void Unpause(void) { paused = false; }
    bool GetPause(void) { return (actuallypaused && audio_actually_paused &&
                                  video_actually_paused); }
    
    void FastForward(float seconds) { fftime = seconds; }
    void Rewind(float seconds) { rewindtime = seconds; }
    void ResetPlaying(void) { resetplaying = true; actuallyreset = false; }
    bool ResetYet(void) { return actuallyreset; }

    float GetFrameRate(void) { return video_frame_rate; } 
    long long GetFramesPlayed(void) { return framesPlayed; }

    void SetRecorder(NuppelVideoRecorder *nvcr) { nvr = nvcr; }

    void SetOSDFontName(char *filename) { osdfilename = filename; }
    
    void SetInfoText(const string &text, const string &subtitle,
                     const string &desc, const string &category,
                     const string &start, const string &end, int secs);
    void SetChannelText(const string &text, int secs);

    void ShowLastOSD(int secs);
    void TurnOffOSD(void) { osd->TurnOff(); }
   
    bool OSDVisible(void) { if (osd) return osd->Visible(); else return false; }

 protected:
    void OutputAudioLoop(void);
    void OutputVideoLoop(void);

    static void *kickoffOutputAudioLoop(void *player);
    static void *kickoffOutputVideoLoop(void *player);
    
 private:
    void InitSound(void);
    void WriteAudio(unsigned char *aubuf, int size);

    int InitSubs(void);
    
    int OpenFile(bool skipDsp = false);
    int CloseFile(void);

    int GetAudiotime(void);
    void SetAudiotime(void);
    unsigned char *DecodeFrame(struct rtframeheader *frameheader,
                               unsigned char *strm);
    void GetFrame(int onlyvideo);
    
    long long CalcMaxPausePosition(void);
    void CalcMaxFFTime();
   
    bool DoFastForward();
    bool DoRewind();
   
    void ClearAfterSeek(); // caller should not hold any locks
    
    int audiolen(bool use_lock); // number of valid bytes in audio buffer
    int audiofree(bool use_lock); // number of free bytes in audio buffer
    int vbuffer_numvalid(void); // number of valid slots in video buffer
    int vbuffer_numfree(void); // number of free slots in the video buffer
 
    int deinterlace;
    
    int audiofd;

    string filename;
    string audiodevice;
    
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

    map<long long, long long> *positionMap;
    long long lastKey;
    
    float rewindtime, fftime;
    NuppelVideoRecorder *nvr;

    bool resetplaying;
    bool actuallyreset;

    string osdfilename;
    OSD *osd;
};

#endif
