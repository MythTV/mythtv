#ifndef DECODERBASE_H_
#define DECODERBASE_H_

class RingBuffer;
class NuppelVideoPlayer;
class RemoteEncoder;
class MythSqlDatabase;
class ProgramInfo;

class DecoderBase
{
  public:
    DecoderBase(NuppelVideoPlayer *parent, MythSqlDatabase *db,
                ProgramInfo *pginfo) 
    { 
        m_parent = parent; 
        m_db = db;
        m_playbackinfo = pginfo;

        exactseeks = false;
        livetv = false;
        watchingrecording = false;
        nvr_enc = NULL;
        lowbuffers = false; 
    }
    virtual ~DecoderBase() { }

    virtual void Reset(void) = 0;

    virtual int OpenFile(RingBuffer *rbuffer, bool novideo,
                         char testbuf[2048]) = 0;

    void setExactSeeks(bool exact) { exactseeks = exact; }
    void setLiveTVMode(bool live) { livetv = live; }
    virtual void setWatchingRecording(bool mode) { watchingrecording = mode; }
    void setRecorder(RemoteEncoder *recorder) { nvr_enc = recorder; }

    void setLowBuffers(void) { lowbuffers = true; }

    virtual void GetFrame(int onlyvideo) = 0;
    
    virtual bool DoRewind(long long desiredFrame) = 0;
    virtual bool DoFastForward(long long desiredFrame) = 0;

    virtual bool isLastFrameKey() = 0;
    virtual void WriteStoredData(RingBuffer *rb, bool storevid) = 0;
    virtual void SetRawAudioState(bool state) = 0;
    virtual bool GetRawAudioState() = 0;
    virtual void SetRawVideoState(bool state) = 0;
    virtual bool GetRawVideoState() = 0;

    virtual long UpdateStoredFrameNum(long frame) = 0;

    virtual void SetPositionMap() = 0;

    virtual QString GetEncodingType(void) = 0;

    virtual void UpdateFramesPlayed(void) { };

    virtual void SetPixelFormat(const int) { }

  protected:
    NuppelVideoPlayer *m_parent;
    MythSqlDatabase *m_db;
    ProgramInfo *m_playbackinfo;

    RingBuffer *ringBuffer;

    bool exactseeks;
    bool livetv;
    bool watchingrecording;
    RemoteEncoder *nvr_enc;

    bool lowbuffers;
};

#endif
