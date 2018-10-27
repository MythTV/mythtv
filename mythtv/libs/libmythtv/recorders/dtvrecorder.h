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
class StreamID;

class DTVRecorder :
    public RecorderBase,
    public MPEGStreamListener,
    public MPEGSingleProgramStreamListener,
    public DVBMainStreamListener,
    public ATSCMainStreamListener,
    public TSPacketListener,
    public TSPacketListenerAV,
    public PSStreamListener
{
  public:
    explicit DTVRecorder(TVRec *rec);
    virtual ~DTVRecorder();

    void SetOption(const QString &opt, const QString &value) override; // RecorderBase
    void SetOption(const QString &name, int value) override; // RecorderBase
    void SetOptionsFromProfile(
        RecordingProfile *profile, const QString &videodev,
        const QString&, const QString&) override; // RecorderBase

    bool IsErrored(void) override // RecorderBase
        { return !_error.isEmpty(); }

    long long GetFramesWritten(void) override // RecorderBase
        { return _frames_written_count; }

    void SetVideoFilters(QString &/*filters*/) override {;} // RecorderBase
    void Initialize(void) override {;} // RecorderBase
    int GetVideoFd(void) override // RecorderBase
        { return _stream_fd; }

    virtual void SetStreamData(MPEGStreamData* sd);
    MPEGStreamData *GetStreamData(void) const { return _stream_data; }

    void Reset(void) override; // RecorderBase
    void ClearStatistics(void) override; // RecorderBase
    RecordingQuality *GetRecordingQuality(const RecordingInfo*) const override; // RecorderBase

    // MPEG Stream Listener
    void HandlePAT(const ProgramAssociationTable*) override; // MPEGStreamListener
    void HandleCAT(const ConditionalAccessTable*) override {} // MPEGStreamListener
    void HandlePMT(uint pid, const ProgramMapTable*) override; // MPEGStreamListener
    void HandleEncryptionStatus(uint /*pnum*/, bool /*encrypted*/) override { } // MPEGStreamListener

    // MPEG Single Program Stream Listener
    void HandleSingleProgramPAT(ProgramAssociationTable *pat, bool insert) override; // MPEGSingleProgramStreamListener
    void HandleSingleProgramPMT(ProgramMapTable *pmt, bool insert) override; // MPEGSingleProgramStreamListener

    // ATSC Main
    void HandleSTT(const SystemTimeTable*) override { UpdateCAMTimeOffset(); } // ATSCMainStreamListener
    void HandleVCT(uint /*tsid*/, const VirtualChannelTable*) override {} // ATSCMainStreamListener
    void HandleMGT(const MasterGuideTable*) override {} // ATSCMainStreamListener

    // DVBMainStreamListener
    void HandleTDT(const TimeDateTable*) override { UpdateCAMTimeOffset(); } // DVBMainStreamListener
    void HandleNIT(const NetworkInformationTable*) override {} // DVBMainStreamListener
    void HandleSDT(uint /*tsid*/, const ServiceDescriptionTable*) override {} // DVBMainStreamListener

    // TSPacketListener
    bool ProcessTSPacket(const TSPacket &tspacket) override; // TSPacketListener

    // TSPacketListenerAV
    bool ProcessVideoTSPacket(const TSPacket& tspacket) override; // TSPacketListenerAV
    bool ProcessAudioTSPacket(const TSPacket& tspacket) override; // TSPacketListenerAV

    // Common audio/visual processing
    bool ProcessAVTSPacket(const TSPacket &tspacket);

  protected:
    virtual void InitStreamData(void);

    void FinishRecording(void) override; // RecorderBase
    void ResetForNewFile(void) override; // RecorderBase

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
    void FindPSKeyFrames(const uint8_t *buffer, uint len) override; // PSStreamListener

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
    int           _record_mpts;
    bool          _record_mpts_only;
    unsigned char _stream_id[0x1fff + 1];
    unsigned char _pid_status[0x1fff + 1];
    unsigned char _continuity_counter[0x1fff + 1];
    vector<TSPacket> _scratch;

    // Statistics
    int           _minimum_recording_quality;
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

    // Music Choice
    // Comcast Music Choice uses 3 frames every 6 seconds and no key frames
    bool music_choice;

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
