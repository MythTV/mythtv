#ifndef DECODERBASE_H_
#define DECODERBASE_H_

#include "RingBuffer.h"
#include "NuppelVideoPlayer.h"
#include "remoteencoder.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "programinfo.h"
#include "videooutbase.h" // for MythCodecID

class RingBuffer;

const int kDecoderProbeBufferSize = 65536;

class DecoderBase
{
  public:
    DecoderBase(NuppelVideoPlayer *parent, ProgramInfo *pginfo);
    virtual ~DecoderBase();

    virtual void Reset(void);

    virtual int OpenFile(RingBuffer *rbuffer, bool novideo,
                         char testbuf[kDecoderProbeBufferSize],
                         int testbufsize = kDecoderProbeBufferSize) = 0;

    void setExactSeeks(bool exact) { exactseeks = exact; }
    void setLiveTVMode(bool live) { livetv = live; }
    void setRecorder(RemoteEncoder *recorder) { nvr_enc = recorder; }

    // Must be done while player is paused.
    void SetProgramInfo(ProgramInfo *pginfo);

    void SetLowBuffers(bool low) { lowbuffers = low; }
    /// Disables AC3/DTS pass through
    virtual void SetDisablePassThrough(bool disable) { (void)disable; }

    virtual void setWatchingRecording(bool mode);
    virtual bool GetFrame(int onlyvideo) = 0;
    NuppelVideoPlayer *GetNVP() { return m_parent; }
    
    virtual bool DoRewind(long long desiredFrame, bool doflush = true);
    virtual bool DoFastForward(long long desiredFrame, bool doflush = true);

    virtual bool isLastFrameKey() = 0;
    virtual void WriteStoredData(RingBuffer *rb, bool storevid,
                                 long timecodeOffset) = 0;
    virtual void ClearStoredData(void) { return; };
    virtual void SetRawAudioState(bool state) { getrawframes = state; }
    virtual bool GetRawAudioState(void) const { return getrawframes; }
    virtual void SetRawVideoState(bool state) { getrawvideo = state; }
    virtual bool GetRawVideoState(void) const { return getrawvideo; }

    virtual long UpdateStoredFrameNum(long frame) = 0;

    virtual QString GetEncodingType(void) const = 0;
    virtual double  GetFPS(void) const { return fps; }

    virtual void UpdateFramesPlayed(void);
    long long GetFramesRead(void) const { return framesRead; };

    virtual MythCodecID GetVideoCodecID() const { return kCodec_NONE; }
    virtual bool SyncPositionMap(void);
    virtual bool PosMapFromDb(void);
    virtual bool PosMapFromEnc(void);

    virtual bool FindPosition(long long desired_value, bool search_adjusted,
                              int &lower_bound, int &upper_bound);

    virtual void SetPositionMap(void);
    virtual void SeekReset(long long newkey, uint skipFrames,
                           bool doFlush, bool discardFrames);

    const int getCurrentAudioTrack() const { return currentAudioTrack;}
    virtual void incCurrentAudioTrack(){}
    virtual void decCurrentAudioTrack(){}
    virtual bool setCurrentAudioTrack(int){ return false;}
    virtual QStringList listAudioTracks() const { return QStringList("Track 1"); }

    const int getCurrentSubtitleTrack() const { return currentSubtitleTrack;}
    virtual void incCurrentSubtitleTrack(){}
    virtual void decCurrentSubtitleTrack(){}
    virtual bool setCurrentSubtitleTrack(int){ return false;}
    virtual QStringList listSubtitleTracks() const { return QStringList(); }

    void setTranscoding(bool value) { transcoding = value; };
                                                          
    bool IsErrored() { return errored; }

    void SetWaitForChange(void);
    bool GetWaitForChange(void);
    void SetReadAdjust(long long adjust);

    // DVD public stuff
    void ChangeDVDTrack(bool ffw);
    long long DVDFindPosition(long long desiredFrame);
    long long DVDCurrentFrameNumber(void);

  protected:
    void FileChanged(void);

    bool DoRewindSeek(long long desiredFrame);
    void DoFastForwardSeek(long long desiredFrame, bool &needflush);

    long long GetLastFrameInPosMap(long long desiredFrame);

    typedef struct posmapentry
    {
        long long index;    // frame or keyframe number
        long long adjFrame; // keyFrameAdjustTable adjusted frame number
        long long pos;      // position in stream
    } PosMapEntry;
    long long GetKey(PosMapEntry &entry) const;

    NuppelVideoPlayer *m_parent;
    ProgramInfo *m_playbackinfo;

    RingBuffer *ringBuffer;
    RemoteEncoder *nvr_enc;

    int current_width;
    int current_height;
    float current_aspect;
    double fps;

    long long framesPlayed;
    long long framesRead;
    long long lastKey;
    int keyframedist;
    long long indexOffset;

    bool ateof;
    bool exitafterdecoded;
    bool transcoding;

    bool hasFullPositionMap;
    bool recordingHasPositionMap;
    bool posmapStarted;
    MarkTypes positionMapType;
 
    QValueVector<PosMapEntry> m_positionMap;

    bool exactseeks;
    bool livetv;
    bool watchingrecording;

    bool hasKeyFrameAdjustTable;

    bool lowbuffers;

    bool getrawframes;
    bool getrawvideo;

    int currentAudioTrack;
    int currentSubtitleTrack;
    bool errored;

    bool waitingForChange;
    long long readAdjust;
    bool justAfterChange;
};

#endif
