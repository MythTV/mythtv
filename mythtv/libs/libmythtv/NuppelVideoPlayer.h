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

using namespace std;

#define MAXVBUFFER 20
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

    void SetOSDFontName(char *filename) { osdfilename = filename; }
    
    void SetInfoText(const string &text, const string &subtitle,
                     const string &desc, const string &category,
                     const string &start, const string &end, int secs);
    void SetChannelText(const string &text, int secs);

    void ShowLastOSD(int secs);
    void TurnOffOSD(void) { osd->TurnOff(); }
   
    bool OSDVisible(void) { if (osd) return osd->Visible(); else return false; }
    
 private:
    void InitSound(void);
    void WriteAudio(unsigned char *aubuf, int size);

    int InitSubs(void);
    
    int OpenFile(bool skipDsp = false);
    int CloseFile(void);

    int ComputeByteShift(void);
    unsigned char *DecodeFrame(struct rtframeheader *frameheader,
                               unsigned char *strm);
    unsigned char *GetFrame(int *timecode, int onlyvideo, 
                            unsigned char **audiodata, int *alen);
    
    long long CalcMaxPausePosition(void);
    void CalcMaxFFTime();
   
    bool DoFastForward();
    bool DoRewind();
   
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
    int vbuffer_numvalid;	/* number of slots containing valid data */
    int bufstat[MAXVBUFFER];	/* slot status. 0=unused, 1=valid data */
    int timecodes[MAXVBUFFER];	/* timecode for each slot */
    unsigned char *vbuffer[MAXVBUFFER];	/* decompressed video data */

    /* Audio circular buffer */
    unsigned char audiobuffer[AUDBUFSIZE];	/* buffer */
    unsigned char tmpaudio[AUDBUFSIZE];		/* temporary space */
    int audiolen;		/* number of valid bytes */
    int raud, waud;		/* read and write positions */
    int audiotimecode;		/* timecode of audio most recently placed into
				   buffer */

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

    string osdfilename;
    OSD *osd;
};

#endif
