#ifndef NUPPELVIDEOPLAYER
#define NUPPELVIDEOPLAYER

#include <qstring.h>
#include <qmutex.h>
#include <qwaitcondition.h>
#include <qptrqueue.h>
#include <sys/time.h>

#include "RingBuffer.h"
#include "osd.h"
#include "jitterometer.h"
#include "recordingprofile.h"
#include "videooutbase.h"

extern "C" {
#include "filter.h"
}
using namespace std;

#define MAXTBUFFER 21

class VideoOutput;
class OSDSet;
class RemoteEncoder;
class MythSqlDatabase;
class ProgramInfo;
class DecoderBase;
class AudioOutput;
class FilterManager;
class FilterChain;
class VideoSync;

struct TextContainer
{
    int timecode;
    char type;
    int len;
    unsigned char *buffer;
};

typedef  void (*StatusCallback)(int, void*);

class NuppelVideoPlayer
{
 public:
    NuppelVideoPlayer(ProgramInfo *info = NULL);
   ~NuppelVideoPlayer();

    friend class CommDetect;

    void SetParentWidget(QWidget *widget) { parentWidget = widget; }

    void SetAsPIP(void) { disableaudio = disablevideo = true; }
    void SetNoAudio(void) { disableaudio = true; }
    void SetNoVideo(void) { disablevideo = true; }

    void SetAudioDevice(QString device) { audiodevice = device; }
    void SetFileName(QString lfilename) { filename = lfilename; }

    void SetExactSeeks(bool exact) { exactseeks = exact; }
    void SetAutoCommercialSkip(int autoskip);
    void SetTryUnflaggedSkip(bool tryskip) { tryunflaggedskip = tryskip; };
    void SetCommercialSkipMethod(int method) { commercialskipmethod = method; }

    int OpenFile(bool skipDsp = false);
    void StartPlaying(void);
    void StopPlaying(void) { killplayer = true; decoder_thread_alive = false; }

    bool IsPlaying(void) { return playing; }
    bool IsDecoderThreadAlive(void) { return decoder_thread_alive; }

    void SetRingBuffer(RingBuffer *rbuf) { ringBuffer = rbuf; }

    void SetAudioSampleRate(int rate) { audio_samplerate = rate; }

    void SetAudioStretchFactor(float factor) { audio_stretchfactor = factor; }

    void Pause(bool waitvideo = true);
    bool Play(float speed = 1.0, bool normal = true,
              bool unpauseaudio = true);
    bool GetPause(void);
    int GetFFRewSkip(void) { return ffrew_skip; }
    bool AtNormalSpeed(void) { return next_normal_speed; }

    bool FastForward(float seconds);
    bool Rewind(float seconds);

    void SkipCommercials(int direction);

    void ResetPlaying(void);

    int GetVideoWidth(void) { return video_width; }
    int GetVideoHeight(void) { return video_height; }
    float GetVideoAspect(void) { return video_aspect; }
    float GetFrameRate(void) { return video_frame_rate; }
    long long GetTotalFrameCount(void) { return totalFrames; }
    long long GetFramesPlayed(void) { return framesPlayed; }
    int GetSecondsBehind(void);

    void SetRecorder(RemoteEncoder *recorder);

    OSD *GetOSD(void) { return osd; }

    // don't use this on something you're playing
    char *GetScreenGrab(int secondsin, int &buflen, int &vw, int &vh);

    void SetLength(int len) { totalLength = len; }
    int GetLength(void) { return totalLength; }

    QString GetEncodingType(void);
    void SetAudioOutput (AudioOutput *ao) { audioOutput = ao; }
    void FlushTxtBuffers(void) { rtxt = wtxt; }
    bool WriteStoredData(RingBuffer *outRingBuffer, bool writevideo,
                         long timecodeOffset);
    long UpdateStoredFrameNum(long curFrameNum);
    void InitForTranscode(bool copyaudio, bool copyvideo);
    bool TranscodeGetNextFrame(QMap<long long, int>::Iterator &dm_iter,
                               int *did_ff, bool *is_key, bool honorCutList);
    void TranscodeWriteText(void (*func)(void *, unsigned char *, int, int, int), void *ptr);

    int FlagCommercials(bool showPercentage, bool fullSpeed,
                        bool inJobQueue);
    bool RebuildSeekTable(bool showPercentage = true, StatusCallback cb = NULL,
                          void* cbData = NULL);

    VideoFrame *GetCurrentFrame(int &w, int &h);
    void ReleaseCurrentFrame(VideoFrame *frame);

    void SetPipPlayer(NuppelVideoPlayer *pip) { setpipplayer = pip;
                                                needsetpipplayer = true; }
    bool PipPlayerSet(void) { return !needsetpipplayer; }

    void SetVideoFilters(QString &filters) { videoFilterList = filters; }

    void SetWatchingRecording(bool mode);
    void SetBookmark(void);
    void ClearBookmark(void);
    long long GetBookmark(void);

    void ToggleCC(char mode, int arg);

    // edit mode stuff
    bool EnableEdit(void);
    bool DoKeypress(QKeyEvent *e);
    bool GetEditMode(void) { return editmode; }

    void EmbedInWidget(WId wid, int x, int y, int w, int h);
    void StopEmbedding(void);

    // decoder stuff..
    void ForceVideoOutputType(VideoOutputType type);

    void SetKeyframeDistance(int keyframedistance);
    void SetVideoParams(int width, int height, double fps,
                        int keyframedistance, float aspect = 1.33333,
                        FrameScanType scan = kScan_Ignore, bool reinit = false);
    void SetAudioParams(int bps, int channels, int samplerate);
    void SetEffDsp(int dsprate);
    void SetFileLength(int total, int frames);

    VideoFrame *GetNextVideoFrame(void);
    void ReleaseNextVideoFrame(VideoFrame *buffer, long long timecode);
    void DiscardVideoFrame(VideoFrame *buffer);

    void DrawSlice(VideoFrame *frame, int x, int y, int w, int h);

    bool GetRawAudioState(void);
    void AddAudioData(char *buffer, int len, long long timecode);
    void AddAudioData(short int *lbuffer, short int *rbuffer, int samples,
                      long long timecode);

    void AddTextData(char *buffer, int len, long long timecode, char type);

    void SetEof(void) { eof = 1; }
    int GetEof(void) { return eof; }
    void SetFramesPlayed(long long played) { framesPlayed = played; }

    VideoOutput *getVideoOutput(void) { return videoOutput; }
    AudioOutput *getAudioOutput(void) { return audioOutput; }

    VideoSync *getVideoSync() const { return videosync; }

    void StopVideoSync(void);

    int calcSliderPos(QString &desc);

    bool GetLimitKeyRepeat(void) { return limitKeyRepeat; }

    void ReinitOSD(void);
    void ReinitVideo(void);
    void ReinitAudio(void);

    void ToggleLetterbox(int letterboxMode = -1);
    void Zoom(int direction);
    int GetLetterbox(void);

    void ExposeEvent(void);

    void incCurrentAudioTrack();
    void decCurrentAudioTrack();
    bool setCurrentAudioTrack(int trackNo);
    int getCurrentAudioTrack();

    long long CalcMaxFFTime(long long ff);

    bool IsErrored() { return errored; }

 protected:
    void OutputVideoLoop(void);
    void IvtvVideoLoop(void);

    static void *kickoffOutputVideoLoop(void *player);

    VideoOutputType forceVideoOutput;

 private:
    bool InitVideo(void);

    void InitFilters(void);

    bool GetVideoPause(void);
    void PauseVideo(bool wait = true);
    void UnpauseVideo(void);

    void setPrebuffering(bool prebuffer);

    bool DecodeFrame(struct rtframeheader *frameheader,
                     unsigned char *strm, unsigned char *outbuf);
    bool GetFrame(int onlyvideo, bool unsafe = false);

    void DoPause();
    void DoPlay();
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

    void ShowText(void);
    void ResetCC(void);
    void UpdateCC(unsigned char *inpos);

    void UpdateTimeDisplay(void);
    void UpdateSeekAmount(bool up);
    void UpdateEditSlider(void);
    void AddMark(long long frames, int type);
    void DeleteMark(long long frames);
    void ReverseMark(long long frames);
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

    float WarpFactor(void);

    FrameScanType detectInterlace(FrameScanType newScan, FrameScanType scan,
                                  float fps, int video_height);

    QString filename;

    /* rtjpeg_plugin stuff */
    int eof;
    int video_width;
    int video_height;
    int postfilt_width;
    int postfilt_height;
    int video_size;
    double video_frame_rate;
    float video_aspect;
    FrameScanType m_scan;
    bool m_double_framerate;
    bool m_can_double;

    int filesize;
    int startpos;

    char vbimode;
    int vbipagenr;
    int text_size;

    /* Video circular buffer */
    bool prebuffering;  /* don't play until done prebuffering */
    QMutex prebuffering_lock;
    QWaitCondition prebuffering_wait;

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
    float audio_stretchfactor;

    AudioOutput *audioOutput;

    bool paused, pausevideo;
    bool actuallypaused, video_actually_paused;
    QWaitCondition decoderThreadPaused, videoThreadPaused;

    bool cc;
    unsigned char ccmode;

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

    class CommDetect *commDetect;

    int totalLength;
    long long totalFrames;

    OSD *osd;

    bool disablevideo;
    bool disableaudio;

    NuppelVideoPlayer *pipplayer;
    NuppelVideoPlayer *setpipplayer;
    bool needsetpipplayer;

    QString videoFilterList;
    FilterChain *videoFilters;
    FilterManager *FiltMan;

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

    WId embedid;
    int embx, emby, embw, embh;

    ProgramInfo *m_playbackinfo;

    long long bookmarkseek;

    int consecutive_blanks;
    int skipcommercials;
    int autocommercialskip;
    int commercialskipmethod;
    int commrewindamount;
    int commnotifyamount;
    bool tryunflaggedskip;

    QString ccline;
    int cccol;
    int ccrow;

    DecoderBase *decoder;

    /* avsync stuff */
    int lastaudiotime;
    int delay;
    int avsync_delay;
    int avsync_adjustment;
    int avsync_avg;
    int avsync_oldavg;
    int refreshrate;

    QMutex decoder_lock;
    int frame_interval; // always adjusted for play_speed
    float play_speed;
    bool normal_speed;
    int ffrew_skip;
    float next_play_speed;
    bool next_normal_speed;
    int videobuf_retries;

    float warpfactor;
    float warpfactor_avg;
    int vsynctol;
    short int *warplbuff;
    short int *warprbuff;
    int warpbuffsize;
 
    int prevtc;

    bool delay_clipping;
    struct timeval nexttrigger, now;
    VideoSync *videosync;

    bool lastsync;

    Jitterometer *output_jmeter;

    void InitAVSync(void);
    void AVSync(void);
    void ShutdownAVSync(void);

    bool usevideotimebase;

    bool limitKeyRepeat;

    QWidget *parentWidget;

    QMutex vidExitLock;

    QMutex videofiltersLock;

    bool errored;

    int m_DeintSetting;
};

#endif
