#ifndef DECODERBASE_H_
#define DECODERBASE_H_

class RingBuffer;
class NuppelVideoPlayer;
class RemoteEncoder;

class DecoderBase
{
  public:
    DecoderBase(NuppelVideoPlayer *parent) { m_parent = parent; 
                                             exactseeks = false;
                                             livetv = false;
                                             watchingrecording = false;
                                             nvr_enc = NULL; }
    virtual ~DecoderBase() { }

    virtual void Reset(void) = 0;

    virtual int OpenFile(RingBuffer *rbuffer, bool novideo) = 0;

    void setExactSeeks(bool exact) { exactseeks = exact; }
    void setLiveTVMode(bool live) { livetv = live; }
    void setWatchingRecording(bool mode) { watchingrecording = mode; }
    void setRecorder(RemoteEncoder *recorder) { nvr_enc = recorder; }

    virtual void GetFrame(int onlyvideo) = 0;
    
    virtual bool DoRewind(long long desiredFrame) = 0;
    virtual bool DoFastForward(long long desiredFrame) = 0;

    virtual char *GetScreenGrab(int secondsin) = 0; 

  protected:
    NuppelVideoPlayer *m_parent;
    RingBuffer *ringBuffer;

    bool exactseeks;
    bool livetv;
    bool watchingrecording;
    RemoteEncoder *nvr_enc;
};

#endif
