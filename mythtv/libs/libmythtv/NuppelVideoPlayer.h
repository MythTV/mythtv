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

using namespace std;

#define MAXVBUFFER 20

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

    bool TogglePause(void) { return (paused = !paused); }
    void Pause(void) { paused = true; actuallypaused = false; }
    void Unpause(void) { paused = false; }
    bool GetPause(void) { return actuallypaused; }
    
    void FastForward(float seconds) { fftime = seconds; }
    void Rewind(float seconds) { rewindtime = seconds; }
    void ResetPlaying(void) { resetplaying = true; actuallyreset = false; }
    bool ResetYet(void) { return actuallyreset; }

    float GetFrameRate(void) { return video_frame_rate; } 
    long long GetFramesPlayed(void) { return framesPlayed; }
    long long GetFramesSkipped(void) { return framesSkipped; }

    void SetRecorder(NuppelVideoRecorder *nvcr) { nvr = nvcr; }

    void SetInfoText(char *text, int secs);

 private:
    void InitSound(void);
    void WriteAudio(unsigned char *aubuf, int size);

    int InitSubs(void);
    
    int OpenFile(bool skipDsp = false);
    int CloseFile(void);

    unsigned char *DecodeFrame(struct rtframeheader *frameheader,
                               unsigned char *strm);
    unsigned char *GetFrame(int *timecode, int onlyvideo, 
                            unsigned char **audiodata, int *alen);
    
    long long CalcMaxPausePosition(void);
    void CalcMaxFFTime();
   
    void DoFastForward();
    void DoRewind();
   
    void ClearAfterSeek();
    
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
    int effdsp;
    int filesize;
    int startpos;
    int audiodelay;

    int lastaudiolen;
    unsigned char *strm;
    struct rtframeheader frameheader;
    int wpos;
    int rpos;
    int bufstat[MAXVBUFFER];
    int timecodes[MAXVBUFFER];
    unsigned char *vbuffer[MAXVBUFFER];
    unsigned char audiobuffer[512000];
    unsigned char tmpaudio[512000];
    int audiolen;
    int fafterseek;
    int audiotimecode;

    int weseeked;

    struct rtfileheader fileheader;
    lame_global_flags *gf;

    RTjpeg *rtjd;
    uint8_t *planes[3];

    int paused;
    bool actuallypaused;

    bool playing;

    RingBuffer *ringBuffer;
    bool weMadeBuffer; 
    bool killplayer;

    long long framesPlayed;
    long long framesSkipped;
    
    bool livetv;

    map<long long, long long> *positionMap;
    long long lastKey;
    
    float rewindtime, fftime;
    NuppelVideoRecorder *nvr;

    bool resetplaying;
    bool actuallyreset;
};

#endif
