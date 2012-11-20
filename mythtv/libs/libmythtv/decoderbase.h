#ifndef DECODERBASE_H_
#define DECODERBASE_H_

#include <stdint.h>

#include <vector>
using namespace std;

#include "ringbuffer.h"
#include "remoteencoder.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "programinfo.h"
#include "mythcodecid.h"

class RingBuffer;
class TeletextViewer;
class MythPlayer;
class AudioPlayer;

const int kDecoderProbeBufferSize = 256 * 1024;

/// Track types
typedef enum TrackTypes
{
    kTrackTypeUnknown = 0,
    kTrackTypeAudio,
    kTrackTypeVideo,
    kTrackTypeSubtitle,
    kTrackTypeCC608,
    kTrackTypeCC708,
    kTrackTypeTeletextCaptions,
    kTrackTypeTeletextMenu,
    kTrackTypeRawText,
    kTrackTypeAttachment,
    kTrackTypeCount,

    kTrackTypeTextSubtitle,
} TrackType;
QString toString(TrackType type);
int to_track_type(const QString &str);

typedef enum DecodeTypes
{
    kDecodeNothing = 0x00, // Demux and preprocess only.
    kDecodeVideo   = 0x01,
    kDecodeAudio   = 0x02,
    kDecodeAV      = 0x03,
} DecodeType;

typedef enum AudioTrackType
{
    kAudioTypeNormal = 0,
    kAudioTypeAudioDescription,
    kAudioTypeCommentary
} AudioTrackType;

class StreamInfo
{
  public:
    StreamInfo() :
        av_stream_index(-1), av_substream_index(-1),
        language(-2), language_index(0),
        stream_id(-1), easy_reader(false),
        wide_aspect_ratio(false), orig_num_channels(2), forced(false),
        audio_type(kAudioTypeNormal)  {}
    StreamInfo(int a, int b, uint c, int d, int e, bool f = false,
               bool g = false, bool h = false,
               AudioTrackType i = kAudioTypeNormal) :
        av_stream_index(a), av_substream_index(-1),
        language(b), language_index(c), stream_id(d),
        easy_reader(f), wide_aspect_ratio(g), orig_num_channels(e), forced(h),
        audio_type(i) {}
    StreamInfo(int a, int b, uint c, int d, int e, int f,
               bool g = false, bool h = false, bool i = false,
               AudioTrackType j = kAudioTypeNormal) :
        av_stream_index(a), av_substream_index(e),
        language(b), language_index(c), stream_id(d),
        easy_reader(g), wide_aspect_ratio(h), orig_num_channels(f), forced(i),
        audio_type(j) {}

  public:
    int  av_stream_index;
    /// -1 for no substream, 0 for first dual audio stream, 1 for second dual
    int  av_substream_index;
    int  language; ///< ISO639 canonical language key
    uint language_index;
    int  stream_id;
    bool easy_reader;
    bool wide_aspect_ratio;
    int  orig_num_channels;
    bool forced;
    AudioTrackType audio_type;

    bool operator<(const StreamInfo& b) const
    {
        return (this->stream_id < b.stream_id);
    }
};
typedef vector<StreamInfo> sinfo_vec_t;

class DecoderBase
{
  public:
    DecoderBase(MythPlayer *parent, const ProgramInfo &pginfo);
    virtual ~DecoderBase();

    virtual void Reset(bool reset_video_data, bool seek_reset, bool reset_file);

    virtual int OpenFile(RingBuffer *rbuffer, bool novideo,
                         char testbuf[kDecoderProbeBufferSize],
                         int testbufsize = kDecoderProbeBufferSize) = 0;

    virtual void SetEof(bool eof)  { ateof = eof;  }
    bool         GetEof(void) const { return ateof; }

    void SetSeekSnap(uint64_t snap)  { seeksnap = snap; }
    uint64_t GetSeekSnap(void) const { return seeksnap;  }
    void SetLiveTVMode(bool live)  { livetv = live;      }

    // Must be done while player is paused.
    void SetProgramInfo(const ProgramInfo &pginfo);

    void SetLowBuffers(bool low) { lowbuffers = low; }
    /// Disables AC3/DTS pass through
    virtual void SetDisablePassThrough(bool disable) { (void)disable; }

    virtual void SetWatchingRecording(bool mode);
    /// Demux, preprocess and possibly decode a frame of video/audio.
    virtual bool GetFrame(DecodeType) = 0;
    MythPlayer *GetPlayer() { return m_parent; }

    virtual int GetNumChapters(void)                      { return 0; }
    virtual int GetCurrentChapter(long long framesPlayed) { return 0; }
    virtual void GetChapterTimes(QList<long long> &times) { return;   }
    virtual long long GetChapter(int chapter)             { return framesPlayed; }
    virtual bool DoRewind(long long desiredFrame, bool doflush = true);
    virtual bool DoFastForward(long long desiredFrame, bool doflush = true);

    float GetVideoAspect(void) const { return current_aspect; }

    virtual int64_t NormalizeVideoTimecode(int64_t timecode) { return timecode; }

    virtual bool IsLastFrameKey(void) const = 0;
    virtual bool IsCodecMPEG(void) const { return false; }
    virtual void WriteStoredData(RingBuffer *rb, bool storevid,
                                 long timecodeOffset) = 0;
    virtual void ClearStoredData(void) { return; }
    virtual void SetRawAudioState(bool state) { getrawframes = state; }
    virtual bool GetRawAudioState(void) const { return getrawframes; }
    virtual void SetRawVideoState(bool state) { getrawvideo = state; }
    virtual bool GetRawVideoState(void) const { return getrawvideo; }

    virtual long UpdateStoredFrameNum(long frame) = 0;

    virtual double  GetFPS(void) const { return fps; }
    /// Returns the estimated bitrate if the video were played at normal speed.
    uint GetRawBitrate(void) const { return bitrate; }

    virtual void UpdateFramesPlayed(void);
    long long GetFramesRead(void) const { return framesRead; }
    long long GetFramesPlayed(void) const { return framesPlayed; }

    virtual QString GetCodecDecoderName(void) const = 0;
    virtual QString GetRawEncodingType(void) { return QString(); }
    virtual MythCodecID GetVideoCodecID(void) const = 0;
    virtual void *GetVideoCodecPrivate(void) { return NULL; }

    virtual void ResetPosMap(void);
    virtual bool SyncPositionMap(void);
    virtual bool PosMapFromDb(void);
    virtual bool PosMapFromEnc(void);

    virtual bool FindPosition(long long desired_value, bool search_adjusted,
                              int &lower_bound, int &upper_bound);

    uint64_t SavePositionMapDelta(uint64_t first_frame, uint64_t last_frame);
    virtual void SeekReset(long long newkey, uint skipFrames,
                           bool doFlush, bool discardFrames);

    void SetTranscoding(bool value) { transcoding = value; }

    bool IsErrored() const { return errored; }

    void SetWaitForChange(void);
    bool GetWaitForChange(void) const;
    void SetReadAdjust(long long adjust);

    // Audio/Subtitle/EIA-608/EIA-708 stream selection
    void SetDecodeAllSubtitles(bool val) { decodeAllSubtitles = val; }
    virtual QStringList GetTracks(uint type) const;
    virtual uint GetTrackCount(uint type) const
        { return tracks[type].size(); }

    virtual int  GetTrackLanguageIndex(uint type, uint trackNo) const;
    virtual QString GetTrackDesc(uint type, uint trackNo) const;
    virtual int  SetTrack(uint type, int trackNo);
    int          GetTrack(uint type) const { return currentTrack[type]; }
    StreamInfo   GetTrackInfo(uint type, uint trackNo) const;
    inline  int  IncrementTrack(uint type);
    inline  int  DecrementTrack(uint type);
    inline  int  ChangeTrack(uint type, int dir);
    virtual bool InsertTrack(uint type, const StreamInfo&);
    inline int   NextTrack(uint type);

    virtual int  GetTeletextDecoderType(void) const { return -1; }

    virtual QString GetXDS(const QString&) const { return QString::null; }
    virtual QByteArray GetSubHeader(uint trackNo) const { return QByteArray(); }
    virtual void GetAttachmentData(uint trackNo, QByteArray &filename,
                                   QByteArray &data) {}

    // MHEG/MHI stuff
    virtual bool SetAudioByComponentTag(int) { return false; }
    virtual bool SetVideoByComponentTag(int) { return false; }

    void SaveTotalDuration(void);
    void ResetTotalDuration(void) { totalDuration = 0; }
    void SaveTotalFrames(void);
    bool GetVideoInverted(void) const { return video_inverted; }

  protected:
    virtual int  AutoSelectTrack(uint type);
    inline  void AutoSelectTracks(void);
    inline  void ResetTracks(void);

    void FileChanged(void);

    virtual bool DoRewindSeek(long long desiredFrame);
    virtual void DoFastForwardSeek(long long desiredFrame, bool &needflush);

    long long ConditionallyUpdatePosMap(long long desiredFrame);
    long long GetLastFrameInPosMap(void) const;
    unsigned long GetPositionMapSize(void) const;

    typedef struct posmapentry
    {
        long long index;    // frame or keyframe number
        long long adjFrame; // keyFrameAdjustTable adjusted frame number
        long long pos;      // position in stream
    } PosMapEntry;
    long long GetKey(const PosMapEntry &entry) const;

    MythPlayer *m_parent;
    ProgramInfo *m_playbackinfo;
    AudioPlayer *m_audio;
    RingBuffer *ringBuffer;

    int current_width;
    int current_height;
    float current_aspect;
    double fps;
    uint bitrate;

    long long framesPlayed;
    long long framesRead;
    int64_t totalDuration;
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

    mutable QMutex m_positionMapLock;
    vector<PosMapEntry> m_positionMap;
    bool dontSyncPositionMap;

    uint64_t seeksnap;
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
    bool video_inverted;

    // Audio/Subtitle/EIA-608/EIA-708 stream selection
    bool        decodeAllSubtitles;
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

inline int DecoderBase::NextTrack(uint type)
{
    int next_track = -1;
    int size = tracks[type].size();
    if (size)
        next_track = (max(0, currentTrack[type]) + 1) % size;
    return next_track;
}
#endif
