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
class TeletextViewer;

const int kDecoderProbeBufferSize = 65536;

class StreamInfo
{
  public:
    StreamInfo() : av_stream_index(-1), language(-2), language_index(0),
        stream_id(-1), easy_reader(false), wide_aspect_ratio(false) {}
    StreamInfo(int a, int b, uint c, int d, bool e = false, bool f = false) :
        av_stream_index(a), language(b), language_index(c), stream_id(d),
        easy_reader(e), wide_aspect_ratio(f) {}

  public:
    int  av_stream_index;
    int  language; ///< ISO639 canonical language key
    uint language_index;
    int  stream_id;
    bool easy_reader;
    bool wide_aspect_ratio;

    bool operator<(const StreamInfo& b) const
        { return (this->stream_id < b.stream_id) ; }
};
typedef vector<StreamInfo> sinfo_vec_t;

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

    void setTranscoding(bool value) { transcoding = value; };
                                                          
    bool IsErrored() { return errored; }

    void SetWaitForChange(void);
    bool GetWaitForChange(void);
    void SetReadAdjust(long long adjust);

    // DVD public stuff
    void ChangeDVDTrack(bool ffw);
    long long DVDFindPosition(long long desiredFrame);
    long long DVDCurrentFrameNumber(void);

    // Audio/Subtitle/EIA-608/EIA-708 stream selection
    virtual QStringList GetTracks(uint type) const;
    virtual uint GetTrackCount(uint type) const
        { return tracks[type].size(); }

    virtual QString GetTrackDesc(uint type, uint trackNo) const;
    virtual int  SetTrack(uint type, int trackNo);
    int          GetTrack(uint type) const { return currentTrack[type]; }
    StreamInfo   GetTrackInfo(uint type, uint trackNo) const;
    inline  int  IncrementTrack(uint type);
    inline  int  DecrementTrack(uint type);
    inline  int  ChangeTrack(uint type, int dir);
    virtual bool InsertTrack(uint type, const StreamInfo&);

    virtual int  GetTeletextDecoderType(void) const { return -1; }
    virtual void SetTeletextDecoderViewer(TeletextViewer*) {;}

    // MHEG/MHI stuff
    virtual void ITVReset(const QRect& /*total*/, const QRect& /*visible*/) {}
    virtual bool ITVUpdate(bool /*visible*/) { return false; }
    virtual bool ITVHandleAction(const QString& /*action*/) { return false; }
    virtual void ITVRestart(uint /*chanid*/, bool /*livetv*/) {}

    virtual bool SetAudioByComponentTag(int) { return false; }
    virtual bool SetVideoByComponentTag(int) { return false; }

  protected:
    virtual int  AutoSelectTrack(uint type);
    inline  void AutoSelectTracks(void);
    inline  void ResetTracks(void);

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

    bool errored;

    bool waitingForChange;
    long long readAdjust;
    bool justAfterChange;

    // Audio/Subtitle/EIA-608/EIA-708 stream selection
    int         currentTrack[kTrackTypeCount];
    sinfo_vec_t tracks[kTrackTypeCount];
    StreamInfo  wantedTrack[kTrackTypeCount];
    StreamInfo  selectedTrack[(uint)kTrackTypeCount];
    /// language preferences for auto-selection of streams
    vector<int> languagePreference;
};

inline int DecoderBase::IncrementTrack(uint type)
{
    int next_track = -1;
    int size = tracks[type].size();
    if (size)
        next_track = (max(-1, currentTrack[type]) + 1) % size;
    return SetTrack(type, next_track);
}

inline int DecoderBase::DecrementTrack(uint type)
{
    int next_track = -1;
    int size = tracks[type].size();
    if (size)
        next_track = (max(+0, currentTrack[type]) + size - 1) % size;
    return SetTrack(type, next_track);
}

inline int DecoderBase::ChangeTrack(uint type, int dir)
{
    if (dir > 0)
        return IncrementTrack(type); 
    else
        return DecrementTrack(type);
}

inline void DecoderBase::AutoSelectTracks(void)
{
    for (uint i = 0; i < kTrackTypeCount; i++)
        AutoSelectTrack(i);
}

inline void DecoderBase::ResetTracks(void)
{
    for (uint i = 0; i < kTrackTypeCount; i++)
        currentTrack[i] = -1;
}

#endif
