#ifndef NUPPELVIDEOPLAYER
#define NUPPELVIDEOPLAYER

#include <vector>
#include <qstring.h>
#include <qmutex.h>
#include <qwaitcondition.h>
#include <qptrqueue.h>

#include "RingBuffer.h"
#include "osd.h"
#include "jitterometer.h"
#include "recordingprofile.h"
#include "commercial_skip.h"
#include "videooutbase.h"

extern "C" {
#include "filter.h"
}
using namespace std;

#define MAXTBUFFER 21

#define REENCODE_OK              1
#define REENCODE_CUTLIST_CHANGE -1
#define REENCODE_ERROR           0

class VideoOutput;
class OSDSet;
class RemoteEncoder;
class QSqlDatabase;
class ProgramInfo;
class DecoderBase;
class AudioOutput;

struct TextContainer
{
    int timecode;
    char type;
    int len;
    unsigned char *buffer;
};

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
    void StopPlaying(void) { killplayer = true; decoder_thread_alive = false; }
    
    bool IsPlaying(void) { return playing; }
    bool IsDecoderThreadAlive(void) { return decoder_thread_alive; }

    void SetRingBuffer(RingBuffer *rbuf) { ringBuffer = rbuf; }

    void SetAudioSampleRate(int rate) { audio_samplerate = rate; }

    bool TogglePause(void) { if (paused) Unpause(); else Pause(); 
                             return paused; 
                           }
    void Pause(bool waitvideo = true);
    void Unpause(bool unpauseaudio = true); 
    bool GetPause(void);

    float GetPlaySpeed(void) { return play_speed; };
    void SetPlaySpeed(float speed);

    bool FastForward(float seconds);
    bool Rewind(float seconds);

    void SkipCommercials(int direction);

    void ResetPlaying(void);

    float GetFrameRate(void) { return video_frame_rate; } 
    long long GetFramesPlayed(void) { return framesPlayed; }

    void SetRecorder(RemoteEncoder *recorder);

    OSD *GetOSD(void) { return osd; }

    void SetOSDFontName(QString filename, QString osdccfont, QString prefix) 
    { osdfontname = filename; osdccfontname = osdccfont; osdprefix = prefix; }

    void SetOSDThemeName(QString themename) { osdtheme = themename; }

    // don't use this on something you're playing
    char *GetScreenGrab(int secondsin, int &buflen, int &vw, int &vh);

    void SetLength(int len) { totalLength = len; }
    int GetLength(void) { return totalLength; }

    int ReencodeFile(char *inputname, char *outputname,
                     RecordingProfile &profile, bool honorCutList = false,
                     bool framecontrol = false, bool chkTranscodeDB = false,
                     QString fifodir = NULL);

    int FlagCommercials(bool showPercentage = false, bool fullSpeed = false);

    VideoFrame *GetCurrentFrame(int &w, int &h);

    void SetPipPlayer(NuppelVideoPlayer *pip) { setpipplayer = pip; 
                                                needsetpipplayer = true; }
    bool PipPlayerSet(void) { return !needsetpipplayer; }
 
    void SetVideoFilters(QString &filters) { videoFilterList = filters; }

    void SetWatchingRecording(bool mode);
    void SetBookmark(void);
    void ClearBookmark(void);
    long long GetBookmark(void);

    void ToggleCC(void);

    // edit mode stuff
    bool EnableEdit(void);
    void DoKeypress(int keypress);

    bool GetEditMode(void) { return editmode; }

    void EmbedInWidget(unsigned long wid, int x, int y, int w, int h);
    void StopEmbedding(void);

    // decoder stuff..
    void ForceVideoOutputType(VideoOutputType type);

    void SetVideoParams(int width, int height, double fps, 
                        int keyframedistance, float aspect = 1.33333);
    void SetAudioParams(int bps, int channels, int samplerate);
    void SetEffDsp(int dsprate);
    void SetFileLength(int total, int frames);

    VideoFrame *GetNextVideoFrame(void);
    void ReleaseNextVideoFrame(VideoFrame *buffer, long long timecode);
    void DiscardVideoFrame(VideoFrame *buffer);

    void DrawSlice(VideoFrame *frame, int x, int y, int w, int h);

    void AddAudioData(char *buffer, int len, long long timecode);
    void AddAudioData(short int *lbuffer, short int *rbuffer, int samples,
                      long long timecode);

    void AddTextData(char *buffer, int len, long long timecode, char type);

    void SetEof(void) { eof = 1; }
    int GetEof(void) { return eof; }
    void SetFramesPlayed(long long played) { framesPlayed = played; }

    VideoOutput *getVideoOutput(void) { return videoOutput; }

    int calcSliderPos(float offset, QString &desc);

    bool GetLimitKeyRepeat(void) { return limitKeyRepeat; }

    void ReinitVideo(void);
    void ReinitAudio(void);

    void ToggleLetterbox(void);
    bool GetLetterbox(void);

    void ExposeEvent(void);

 protected:
    void OutputVideoLoop(void);
    void IvtvVideoLoop(void);

    static void *kickoffOutputVideoLoop(void *player);
   
    VideoOutputType forceVideoOutput;
 
 private:
    void InitVideo(void);

    void InitFilters(void);
   
    bool GetVideoPause(void);
    void PauseVideo(bool wait = true);
    void UnpauseVideo(void);

    void setPrebuffering(bool prebuffer);
 
    int OpenFile(bool skipDsp = false);

    bool DecodeFrame(struct rtframeheader *frameheader,
                     unsigned char *strm, unsigned char *outbuf);
    void GetFrame(int onlyvideo, bool unsafe = false);

    void ReduceJitter(struct timeval *nexttrigger);
    
    long long CalcMaxFFTime(long long ff);
   
    bool DoFastForward();
    bool DoRewind();
   
    void ClearAfterSeek(); // caller should not hold any locks

    int GetStatusbarPos(void);

    bool FrameIsBlank(VideoFrame *frame);
    bool LastFrameIsBlank(void);
    int SkipTooCloseToEnd(int frames);
    void SkipCommercialsByBlanks(void);
    bool DoSkipCommercials(int direction);
    void AutoCommercialSkip(void);

    void JumpToFrame(long long frame);
    void JumpToNetFrame(long long net) { JumpToFrame(framesPlayed + net); }
 
    int tbuffer_numvalid(void); // number of valid slots in the text buffer
    int tbuffer_numfree(void); // number of free slots in the text buffer

    void ResetNexttrigger(struct timeval *tv);

    int UpdateDelay(struct timeval *nexttrigger);

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

    void ReencoderAddKFA(QPtrList<struct kfatable_entry> *kfa_table,
                         long curframe, long lastkey, long num_keyframes);

    QString filename;
    
    /* rtjpeg_plugin stuff */
    int eof;
    int video_width;
    int video_height;
    int video_size;
    double video_frame_rate;
    float video_aspect;

    int filesize;
    int startpos;

    char vbimode;
    int vbipagenr;
    int text_size;

    /* Video circular buffer */
    bool prebuffering;	/* don't play until done prebuffering */ 

    /* Text circular buffer */
    int wtxt;          /* next slot to write */
    int rtxt;          /* next slot to read */
    struct TextContainer txtbuffers[MAXTBUFFER+1];

    QMutex text_buflock;  /* adjustments to rtxt and wtxt can only
                             be made while holding this lock */

    /* Audio stuff */
    QString audiodevice;

    int audio_channels;
    int audio_bits;
    int audio_samplerate;

    AudioOutput *audioOutput;

    bool paused, pausevideo;
    bool actuallypaused, video_actually_paused;
    QWaitCondition decoderThreadPaused, videoThreadPaused;
 
    bool cc;

    bool playing;
    bool decoder_thread_alive;

    RingBuffer *ringBuffer;
    bool weMadeBuffer; 
    bool killplayer;
    bool killvideo;

    long long framesPlayed;
    
    bool livetv;
    bool watchingrecording;
    bool editmode;
    bool resetvideo;

    long long rewindtime, fftime;
    RemoteEncoder *nvr_enc;

    CommDetect *commDetect;

    int totalLength;
    long long totalFrames;

    QString osdfontname;
    QString osdccfontname;
    QString osdprefix;
    QString osdtheme;
    OSD *osd;

    bool disablevideo;
    bool disableaudio;

    NuppelVideoPlayer *pipplayer;
    NuppelVideoPlayer *setpipplayer;
    bool needsetpipplayer;

    QString videoFilterList;
    vector<VideoFilter *> videoFilters;

    int keyframedist;

    bool exactseeks;

    VideoOutput *videoOutput;

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
    bool hasFullPositionMap;

    unsigned int embedid;
    int embx, emby, embw, embh;

    QSqlDatabase *m_db;
    QMutex db_lock;
    ProgramInfo *m_playbackinfo;

    long long bookmarkseek;

    int consecutive_blanks;
    int skipcommercials;
    int autocommercialskip;
    int commercialskipmethod;

    QString cclines[4];
    int ccindent[4];
    int lastccrow;

    DecoderBase *decoder;

    /* avsync stuff */
    int lastaudiotime;
    int delay;
    int avsync_delay;
    int avsync_avg;
    int refreshrate;
    int frame_interval; // always adjusted for play_speed
    float play_speed;  
    bool normal_speed;
 
    bool delay_clipping;
    struct timeval nexttrigger, now;

    bool lastsync;
    bool hasvsync;
    bool hasvgasync;

    Jitterometer *output_jmeter;

    void InitExAVSync(void);
    void OldAVSync(void);
    void ExAVSync(void);
    void ShutdownExAVSync(void);

    bool reducejitter;
    bool experimentalsync;

    bool limitKeyRepeat;
};

#endif
