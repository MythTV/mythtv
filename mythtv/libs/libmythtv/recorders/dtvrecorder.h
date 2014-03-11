// -*- Mode: c++ -*-
/**
 *  DTVRecorder -- base class for Digital Television recorders
 *  Copyright (c) 2003-2004 by Brandon Beattie, Doug Larrick,
 *    Jason Hoos, and Daniel Thor Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef DTVRECORDER_H
#define DTVRECORDER_H

#include <vector>
using namespace std;

#include <QAtomicInt>
#include <QString>

#include "streamlisteners.h"
#include "recorderbase.h"
#include "H264Parser.h"

class MPEGStreamData;
class TSPacket;
class QTime;

class DTVRecorder :
    public RecorderBase,
    public MPEGStreamListener,
    public MPEGSingleProgramStreamListener,
    public DVBMainStreamListener,
    public ATSCMainStreamListener,
    public TSPacketListener,
    public TSPacketListenerAV
{
  public:
    DTVRecorder(TVRec *rec);
    virtual ~DTVRecorder();

    virtual void SetOption(const QString &opt, const QString &value);
    virtual void SetOption(const QString &name, int value);
    virtual void SetOptionsFromProfile(
        RecordingProfile *profile, const QString &videodev,
        const QString&, const QString&);

    bool IsErrored(void) { return !_error.isEmpty(); }

    long long GetFramesWritten(void) { return _frames_written_count; }

    void SetVideoFilters(QString &/*filters*/) {;}
    void Initialize(void) {;}
    int GetVideoFd(void) { return _stream_fd; }

    virtual void SetStreamData(MPEGStreamData* sd);
    MPEGStreamData *GetStreamData(void) const { return _stream_data; }

    virtual void Reset(void);
    virtual void ClearStatistics(void);
    virtual RecordingQuality *GetRecordingQuality(const RecordingInfo*) const;

    // MPEG Stream Listener
    void HandlePAT(const ProgramAssociationTable*);
    void HandleCAT(const ConditionalAccessTable*) {}
    void HandlePMT(uint pid, const ProgramMapTable*);
    void HandleEncryptionStatus(uint /*pnum*/, bool /*encrypted*/) { }

    // MPEG Single Program Stream Listener
    void HandleSingleProgramPAT(ProgramAssociationTable *pat, bool insert);
    void HandleSingleProgramPMT(ProgramMapTable *pmt, bool insert);

    // ATSC Main
    void HandleSTT(const SystemTimeTable*) { UpdateCAMTimeOffset(); }
    void HandleVCT(uint /*tsid*/, const VirtualChannelTable*) {}
    void HandleMGT(const MasterGuideTable*) {}

    // DVBMainStreamListener
    void HandleTDT(const TimeDateTable*) { UpdateCAMTimeOffset(); }
    void HandleNIT(const NetworkInformationTable*) {}
    void HandleSDT(uint /*tsid*/, const ServiceDescriptionTable*) {}

    // TSPacketListener
    bool ProcessTSPacket(const TSPacket &tspacket);

    // TSPacketListenerAV
    bool ProcessVideoTSPacket(const TSPacket& tspacket);
    bool ProcessAudioTSPacket(const TSPacket& tspacket);

    // Common audio/visual processing
    bool ProcessAVTSPacket(const TSPacket &tspacket);

  protected:
    virtual void InitStreamData(void);

    void FinishRecording(void);
    void ResetForNewFile(void);

    void HandleKeyframe(int64_t extra);
    void HandleTimestamps(int stream_id, int64_t pts, int64_t dts);
    void UpdateFramesWritten(void);

    void BufferedWrite(const TSPacket &tspacket, bool insert = false);

    // MPEG TS "audio only" support
    bool FindAudioKeyframes(const TSPacket *tspacket);

    // MPEG2 TS support
    bool FindMPEG2Keyframes(const TSPacket* tspacket);

    // MPEG4 AVC / H.264 TS support
    bool FindH264Keyframes(const TSPacket* tspacket);
    void HandleH264Keyframe(void);

    // MPEG2 PS support (Hauppauge PVR-x50/PVR-500)
    void FindPSKeyFrames(const uint8_t *buffer, uint len);

    // For handling other (non audio/video) packets
    bool FindOtherKeyframes(const TSPacket *tspacket);

    inline bool CheckCC(uint pid, uint cc);

    virtual QString GetSIStandard(void) const { return "mpeg"; }
    virtual void SetCAMPMT(const ProgramMapTable*) {}
    virtual void UpdateCAMTimeOffset(void) {}

    // file handle for stream
    int _stream_fd;

    QString _recording_type;

    // used for scanning pes headers for keyframes
    QTime     _audio_timer;
    uint32_t  _start_code;
    int       _first_keyframe;
    unsigned long long _last_gop_seen;
    unsigned long long _last_seq_seen;
    unsigned long long _last_keyframe_seen;
    unsigned int _audio_bytes_remaining;
    unsigned int _video_bytes_remaining;
    unsigned int _other_bytes_remaining;

    // MPEG2 parser information
    int _progressive_sequence;
    int _repeat_pict;

    // H.264 support
    bool _pes_synced;
    bool _seen_sps;
    H264Parser m_h264_parser;

    /// Wait for the a GOP/SEQ-start before sending data
    bool _wait_for_keyframe_option;

    bool _has_written_other_keyframe;

    // state tracking variables
    /// non-empty iff irrecoverable recording error detected
    QString _error;

    MPEGStreamData          *_stream_data;

    // keyframe finding buffer
    bool                  _buffer_packets;
    vector<unsigned char> _payload_buffer;

    // general recorder stuff
    mutable QMutex           _pid_lock;
    ProgramAssociationTable *_input_pat; ///< PAT on input side
    ProgramMapTable         *_input_pmt; ///< PMT on input side
    bool                     _has_no_av;

    // TS recorder stuff
    bool          _record_mpts;
    unsigned char _stream_id[0x1fff + 1];
    unsigned char _pid_status[0x1fff + 1];
    unsigned char _continuity_counter[0x1fff + 1];
    vector<TSPacket> _scratch;

    // Statistics
    bool          _use_pts; // vs use dts
    uint64_t      _ts_count[256];
    int64_t       _ts_last[256];
    int64_t       _ts_first[256];
    QDateTime     _ts_first_dt[256];
    mutable QAtomicInt _packet_count;
    mutable QAtomicInt _continuity_error_count;
    unsigned long long _frames_seen_count;
    unsigned long long _frames_written_count;
    double _total_duration; // usec
    // Calculate _total_duration as
    // _td_base + (_td_tick_count * _td_tick_framerate / 2)
    double _td_base;
    uint64_t _td_tick_count;
    FrameRate _td_tick_framerate;

    // constants
    /// If the number of regular frames detected since the last
    /// detected keyframe exceeds this value, then we begin marking
    /// random regular frames as keyframes.
    static const uint kMaxKeyFrameDistance;
    static const unsigned char kPayloadStartSeen = 0x2;
};

inline bool DTVRecorder::CheckCC(uint pid, uint new_cnt)
{
    bool ok = ((((_continuity_counter[pid] + 1) & 0xf) == new_cnt) ||
               (_continuity_counter[pid] == new_cnt) ||
               (_continuity_counter[pid] == 0xFF));

    _continuity_counter[pid] = new_cnt & 0xf;

    return ok;
}

#endif // DTVRECORDER_H
