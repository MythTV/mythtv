#ifndef NUPPELVIDEOPLAYER
#define NUPPELVIDEOPLAYER

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
#define MAXTBUFFER 11
#define AUDBUFSIZE 512000

#define COMMERCIAL_SKIP_OFF     0
#define COMMERCIAL_SKIP_BLANKS  1

class NuppelVideoPlayer;
class XvVideoOutput;
class OSDSet;
class RemoteEncoder;
class QSqlDatabase;
class ProgramInfo;

class NuppelVideoPlayer
{
 public:
    NuppelVideoPlayer(QSqlDatabase *ldb = NULL, ProgramInfo *info = NULL);
   ~NuppelVideoPlayer();

    void SetAsPIP(void) { disableaudio = disablevideo = true; }
    void SetNoAudio(void) { disableaudio = true; }

    void SetAudioDevice(QString device) { audiodevice = device; }
    void SetFileName(QString lfilename) { filename = lfilename; }

    void SetExactSeeks(bool exact) { exactseeks = exact; }
    void SetAutoCommercialSkip(int autoskip) { autocommercialskip = autoskip; }
    void SetCommercialSkipMethod(int method) { commercialskipmethod = method; }

    void StartPlaying(void);
    void StopPlaying(void) { killplayer = true; }
    
    bool IsPlaying(void) { return playing; }

    void SetRingBuffer(RingBuffer *rbuf) { ringBuffer = rbuf; }

    void SetAudioSampleRate(int rate) { audio_samplerate = rate; }

    bool TogglePause(void) { if (paused) Unpause(); else Pause(); 
                             return paused; 
                           }
    void Pause(void);
    void Unpause(void); 
    bool GetPause(void);
   
    void FastForward(float seconds);
    void Rewind(float seconds);

    void SkipCommercials(int direction);

    void ResetPlaying(void) { resetplaying = true; actuallyreset = false; }
    bool ResetYet(void) { return actuallyreset; }

    float GetFrameRate(void) { return video_frame_rate; } 
    long long GetFramesPlayed(void) { return framesPlayed; }
    void ResetFramesPlayed(void) { framesPlayed = 0; }

    void SetRecorder(RemoteEncoder *recorder) { nvr_enc = recorder; }

    OSD *GetOSD(void) { return osd; }

    void SetOSDFontName(QString filename, QString osdccfont, QString prefix) 
    { osdfontname = filename; osdccfontname = osdccfont; osdprefix = prefix; }

    void SetOSDThemeName(QString themename) { osdtheme = themename; }

    // don't use this on something you're playing
    char *GetScreenGrab(int secondsin, int &buflen, int &vw, int &vh);

    void SetLength(int len) { totalLength = len; }
    int GetLength(void) { return totalLength; }

    void ReencodeFile(char *inputname, char *outputname);

    int FlagCommercials(int show_percentage = false);

    unsigned char *GetCurrentFrame(int &w, int &h);

    void SetPipPlayer(NuppelVideoPlayer *pip) { setpipplayer = pip; 
                                                needsetpipplayer = true; }
    bool PipPlayerSet(void) { return !needsetpipplayer; }
 
    void SetVideoFilters(QString &filters) { videoFilterList = filters; }

    int CheckEvents(void); 

    void SetWatchingRecording(bool mode) { watchingrecording = mode; }
    void SetBookmark(void);
    long long GetBookmark(void);

    void ToggleFullScreen(void);

    void ToggleCC(void);

    // edit mode stuff
    bool EnableEdit(void);
    void DoKeypress(int keypress);

    bool GetEditMode(void) { return editmode; }

    void EmbedInWidget(unsigned long wid, int x, int y, int w, int h);
    void StopEmbedding(void);

 protected:
    void OutputAudioLoop(void);
    void OutputVideoLoop(void);

    static void *kickoffOutputAudioLoop(void *player);
    static void *kickoffOutputVideoLoop(void *player);
    
 private:
    void InitSound(void);
    void InitVideo(void);
    void WriteAudio(unsigned char *aubuf, int size);

    void InitFilters(void);
    int InitSubs(void);
   
    bool GetVideoPause(void);
    void PauseVideo(void);
    void UnpauseVideo(void);

    bool GetAudioPause(void);
    void PauseAudio(void);
    void UnpauseAudio(void);
 
    int OpenFile(bool skipDsp = false);
    int CloseFile(void);

    int GetAudiotime(void);
    void SetAudiotime(void);
    bool DecodeFrame(struct rtframeheader *frameheader,
                     unsigned char *strm, unsigned char *outbuf);
    void GetFrame(int onlyvideo, bool unsafe = false);

    void ReduceJitter(struct timeval *nexttrigger);
    
    long long CalcMaxFFTime(long long ff);
   
    bool DoFastForward();
    bool DoRewind();
   
    void ClearAfterSeek(); // caller should not hold any locks

    int GetStatusbarPos(void);

    bool FrameIsBlank(int vposition);
    bool LastFrameIsBlank(void) { return FrameIsBlank(vpos); }
    int SkipTooCloseToEnd(int frames);
    void SkipCommercialsByBlanks(void);
    bool DoSkipCommercials(int direction);
    void AutoCommercialSkip(void);

    void JumpToFrame(long long frame);
    void JumpToNetFrame(long long net) { JumpToFrame(framesPlayed + net); }
 
    int audiolen(bool use_lock); // number of valid bytes in audio buffer
    int audiofree(bool use_lock); // number of free bytes in audio buffer
    int vbuffer_numvalid(void); // number of valid slots in video buffer
    int vbuffer_numfree(void); // number of free slots in the video buffer
    int tbuffer_numvalid(void); // number of valid slots in the text buffer
    int tbuffer_numfree(void); // number of free slots in the text buffer
    inline int getSpaceOnSoundcard(void);

    void ResetNexttrigger(struct timeval *tv);

    void ShowPip(unsigned char *xvidbuf);
    void ShowText();

    void UpdateTimeDisplay(void);
    void UpdateSeekAmount(bool up);
    void UpdateEditSlider(void);
    void AddMark(long long frames, int type);
    void DeleteMark(long long frames);
    void HandleSelect(void);
    void HandleResponse(void);
    void HandleArbSeek(bool right);
    bool IsInDelete(long long testframe);
    void SaveCutList(void);
    void LoadCutList(void);
    void LoadBlankList(void);
    void LoadCommBreakList(void);
    void DisableEdit(void);
    void SetDeleteIter(void);
    void SetBlankIter(void);
    void SetCommBreakIter(void);

    bool isValidFrametype(char type);

    int audiofd;

    QString filename;
    QString audiodevice;
    
    /* rtjpeg_plugin stuff */
    int eof;
    int video_width;
    int video_height;
    int video_size;
    double video_frame_rate;
    unsigned char *buf;
    unsigned char *buf2;
    unsigned char *directbuf;
    char lastct;
    int effdsp; // from the recorded stream
    int audio_channels;
    int audio_bits;
    int audio_bytes_per_sample;
    int audio_samplerate;
    int audio_buffer_unused;
    int filesize;
    int startpos;

    int lastaudiolen;
    unsigned char *strm;
    struct rtframeheader frameheader;

    char vbimode;
    int vbipagenr;
    int text_size;

    /* Video circular buffer */
    int wpos;		/* next slot to write */
    int rpos;		/* next slot to read */
    int usepre;		/* number of slots to keep full */
    bool prebuffering;	/* don't play until done prebuffering */ 
    int timecodes[MAXVBUFFER];	/* timecode for each slot */
    unsigned char *vbuffer[MAXVBUFFER+1];	/* decompressed video data */

    /* Audio circular buffer */
    unsigned char audiobuffer[AUDBUFSIZE];	/* buffer */
    unsigned char tmpaudio[AUDBUFSIZE];		/* temporary space */
    int raud, waud;		/* read and write positions */
    int audbuf_timecode;	/* timecode of audio most recently placed into
				   buffer */
    /* Text circular buffer */
    int wtxt;          /* next slot to write */
    int rtxt;          /* next slot to read */
    int txttimecodes[MAXTBUFFER];      /* timecode for each slot */
    char txttype[MAXTBUFFER];  /* texttype for each slot */
    unsigned char *tbuffer[MAXTBUFFER+1];      /* decoded subtitles */

    pthread_mutex_t audio_buflock; /* adjustments to audiotimecode, waud, and
                                      raud can only be made while holding this
                                      lock */
    pthread_mutex_t video_buflock; /* adjustments to rpos and wpos can only
                                      be made while holding this lock */
    pthread_mutex_t text_buflock;  /* adjustments to rtxt and wtxt can only
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

    bool paused, pausevideo, pauseaudio;
    bool actuallypaused, audio_actually_paused, video_actually_paused;
    bool cc;

    bool playing;

    RingBuffer *ringBuffer;
    bool weMadeBuffer; 
    bool killplayer;
    bool killaudio;
    bool killvideo;

    long long framesPlayed;
    
    bool livetv;
    bool watchingrecording;
    bool editmode;
    bool resetvideo;

    QMap<long long, long long> *positionMap;
    long long lastKey;
    
    long long rewindtime, fftime;
    RemoteEncoder *nvr_enc;

    bool resetplaying;
    bool actuallyreset;

    int totalLength;
    long long totalFrames;

    QString osdfontname;
    QString osdccfontname;
    QString osdprefix;
    QString osdtheme;
    OSD *osd;

    bool InitAVCodec(int codec);
    void CloseAVCodec(void);

    AVCodec *mpa_codec;
    AVCodecContext *mpa_ctx;
    AVFrame *mpa_pic;
    AVPicture tmppicture;
    AVPicture mpa_pic_tmp;

    bool directrendering;
    friend int get_buffer(struct AVCodecContext *c, AVFrame *pic);
    friend void release_buffer(struct AVCodecContext *c, AVFrame *pic);

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

    int seekamount;
    int seekamountpos;
    OSDSet *timedisplay;

    QMap<long long, int> deleteMap;
    QMap<long long, int>::Iterator deleteIter;
    QMap<long long, int> blankMap;
    QMap<long long, int>::Iterator blankIter;
    QMap<long long, int> commBreakMap;
    QMap<long long, int>::Iterator commBreakIter;
    QString dialogname;
    int dialogtype;
    long long deleteframe;
    bool hasdeletetable;
    bool hasblanktable;
    bool hascommbreaktable;

    int ffmpeg_extradatasize;
    char *ffmpeg_extradata;

    bool own_vidbufs;

    unsigned int embedid;
    int embx, emby, embw, embh;

    QSqlDatabase *m_db;
    ProgramInfo *m_playbackinfo;

    long long bookmarkseek;

    int numbadioctls;
    int numlowbuffer;

    int consecutive_blanks;
    int vpos;
    int skipcommercials;
    int autocommercialskip;
    int commercialskipmethod;
    int commercialskipeverywhere;

    QString cclines[4];
    int ccindent[4];
    int lastccrow;
};

#endif
