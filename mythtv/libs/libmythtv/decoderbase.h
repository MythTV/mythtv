#ifndef DECODERBASE_H_
#define DECODERBASE_H_

#include "RingBuffer.h"
#include "NuppelVideoPlayer.h"
#include "remoteencoder.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "programinfo.h"

class RingBuffer;

class DecoderBase
{
  public:
    DecoderBase(NuppelVideoPlayer *parent, ProgramInfo *pginfo);
    virtual ~DecoderBase() { }

    virtual void Reset(void);

    virtual int OpenFile(RingBuffer *rbuffer, bool novideo,
                         char testbuf[2048]) = 0;

    void setExactSeeks(bool exact) { exactseeks = exact; }
    void setLiveTVMode(bool live) { livetv = live; }
    void setRecorder(RemoteEncoder *recorder) { nvr_enc = recorder; }

    void SetLowBuffers(bool low) { lowbuffers = low; }

    virtual void setWatchingRecording(bool mode);
    virtual bool GetFrame(int onlyvideo) = 0;
    
    virtual bool DoRewind(long long desiredFrame, bool doflush = true);
    virtual bool DoFastForward(long long desiredFrame, bool doflush = true);

    virtual bool isLastFrameKey() = 0;
    virtual void WriteStoredData(RingBuffer *rb, bool storevid,
                                 long timecodeOffset) = 0;
    virtual void ClearStoredData(void) { return; };
    virtual void SetRawAudioState(bool state) { getrawframes = state; }
    virtual bool GetRawAudioState(void) { return getrawframes; }
    virtual void SetRawVideoState(bool state) { getrawvideo = state; }
    virtual bool GetRawVideoState(void) { return getrawvideo; }

    virtual long UpdateStoredFrameNum(long frame) = 0;

    virtual QString GetEncodingType(void) = 0;

    virtual void UpdateFramesPlayed(void);
    long long GetFramesRead(void) { return framesRead; };

    virtual void SetPixelFormat(const int) { }
    virtual bool IsXvMCCompatible() { return false; }
    virtual void SetMPEG2Codec(const int) { }

    virtual bool SyncPositionMap(void);
    virtual bool PosMapFromDb(void);
    virtual bool PosMapFromEnc(void);

    virtual bool FindPosition(long long desired_value, bool search_adjusted,
                              int &lower_bound, int &upper_bound);

    virtual void SetPositionMap(void);
    virtual void SeekReset(long long newKey = 0, int skipFrames = 0,
                           bool needFlush = false) 
                          { (void)newKey; (void)skipFrames; (void)needFlush; }

    const int getCurrentAudioTrack() const { return currentAudioTrack;}
    virtual void incCurrentAudioTrack(){}
    virtual void decCurrentAudioTrack(){}
    virtual bool setCurrentAudioTrack(int){ return false;}

    void setTranscoding(bool value) { transcoding = value; };
                                                          
    bool IsErrored() { return errored; }
  protected:
    typedef struct posmapentry
    {
        long long index;    // frame or keyframe number
        long long adjFrame; // keyFrameAdjustTable adjusted frame number
        long long pos;      // position in stream
    } PosMapEntry;

    NuppelVideoPlayer *m_parent;
    ProgramInfo *m_playbackinfo;

    RingBuffer *ringBuffer;

    int current_width;
    int current_height;
    float current_aspect;

    long long framesPlayed;
    long long framesRead;
    long long lastKey;

    bool ateof;
    bool exitafterdecoded;
    bool transcoding;

    bool hasFullPositionMap;
    bool recordingHasPositionMap;
    bool posmapStarted;
    MarkTypes positionMapType;
 
    QValueVector<PosMapEntry> m_positionMap;

    int keyframedist;

    double fps;

    bool exactseeks;
    bool livetv;
    bool watchingrecording;
    RemoteEncoder *nvr_enc;

    bool hasKeyFrameAdjustTable;

    bool lowbuffers;

    bool getrawframes;
    bool getrawvideo;

    int currentAudioTrack;
    bool errored;
};

#endif
