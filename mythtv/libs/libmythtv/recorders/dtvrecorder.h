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

    void SetOption(const QString &name, const QString &value) override; // RecorderBase
    void SetOption(const QString &name, int value) override; // RecorderBase
    void SetOptionsFromProfile(
        RecordingProfile *profile, const QString &videodev,
        const QString&, const QString&) override; // RecorderBase

    bool IsErrored(void) override // RecorderBase
        { return !m_error.isEmpty(); }

    long long GetFramesWritten(void) override // RecorderBase
        { return m_frames_written_count; }

    void SetVideoFilters(QString &/*filters*/) override {;} // RecorderBase
    void Initialize(void) override {;} // RecorderBase
    int GetVideoFd(void) override // RecorderBase
        { return m_stream_fd; }

    virtual void SetStreamData(MPEGStreamData* data);
    MPEGStreamData *GetStreamData(void) const { return m_stream_data; }

    void Reset(void) override; // RecorderBase
    void ClearStatistics(void) override; // RecorderBase
    RecordingQuality *GetRecordingQuality(const RecordingInfo*) const override; // RecorderBase

    // MPEG Stream Listener
    void HandlePAT(const ProgramAssociationTable*) override; // MPEGStreamListener
    void HandleCAT(const ConditionalAccessTable*) override {} // MPEGStreamListener
    void HandlePMT(uint progNum, const ProgramMapTable*) override; // MPEGStreamListener
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
    int                      m_stream_fd                  {-1};

    QString                  m_recording_type             {"all"};

    // used for scanning pes headers for keyframes
    QTime                    m_audio_timer;
    uint32_t                 m_start_code                 {0xffffffff};
    int                      m_first_keyframe             {-1};
    unsigned long long       m_last_gop_seen              {0};
    unsigned long long       m_last_seq_seen              {0};
    unsigned long long       m_last_keyframe_seen         {0};
    unsigned int             m_audio_bytes_remaining      {0};
    unsigned int             m_video_bytes_remaining      {0};
    unsigned int             m_other_bytes_remaining      {0};

    // MPEG2 parser information
    int                      m_progressive_sequence       {0};
    int                      m_repeat_pict                {0};

    // H.264 support
    bool                     m_pes_synced                 {false};
    bool                     m_seen_sps                   {false};
    H264Parser               m_h264_parser;

    /// Wait for the a GOP/SEQ-start before sending data
    bool                     m_wait_for_keyframe_option   {true};

    bool                     m_has_written_other_keyframe {false};

    // state tracking variables
    /// non-empty iff irrecoverable recording error detected
    QString                  m_error;

    MPEGStreamData          *m_stream_data                {nullptr};

    // keyframe finding buffer
    bool                     m_buffer_packets             {false};
    vector<unsigned char>    m_payload_buffer;

    // general recorder stuff
    mutable QMutex           m_pid_lock                   {QMutex::Recursive};
                             /// PAT on input side
    ProgramAssociationTable *m_input_pat                  {nullptr};
                             /// PMT on input side
    ProgramMapTable         *m_input_pmt                  {nullptr};
    bool                     m_has_no_av                  {false};

    // TS recorder stuff
    bool                     m_record_mpts                {false};
    bool                     m_record_mpts_only           {false};
    unsigned char            m_stream_id[0x1fff + 1] {0};
    unsigned char            m_pid_status[0x1fff + 1] {0};
    unsigned char            m_continuity_counter[0x1fff + 1] {0};
    vector<TSPacket>         m_scratch;

    // Statistics
    int                      m_minimum_recording_quality  {95};
    bool                     m_use_pts                    {false}; // vs use dts
    uint64_t                 m_ts_count[256]              {0};
    int64_t                  m_ts_last[256];
    int64_t                  m_ts_first[256];
    QDateTime                m_ts_first_dt[256];
    mutable QAtomicInt       m_packet_count               {0};
    mutable QAtomicInt       m_continuity_error_count     {0};
    unsigned long long       m_frames_seen_count          {0};
    unsigned long long       m_frames_written_count       {0};
    double                   m_total_duration             {0.0}; // usec
    // Calculate m_total_duration as
    // m_td_base + (m_td_tick_count * m_td_tick_framerate / 2)
    double                   m_td_base                    {0.0};
    uint64_t                 m_td_tick_count              {0};
    FrameRate                m_td_tick_framerate          {0};

    // Music Choice
    // Comcast Music Choice uses 3 frames every 6 seconds and no key frames
    bool                     m_music_choice               {false};

    // constants
    /// If the number of regular frames detected since the last
    /// detected keyframe exceeds this value, then we begin marking
    /// random regular frames as keyframes.
    static const uint          kMaxKeyFrameDistance;
    static const unsigned char kPayloadStartSeen = 0x2;
};

inline bool DTVRecorder::CheckCC(uint pid, uint new_cnt)
{
    bool ok = ((((m_continuity_counter[pid] + 1) & 0xf) == new_cnt) ||
               (m_continuity_counter[pid] == new_cnt) ||
               (m_continuity_counter[pid] == 0xFF));

    m_continuity_counter[pid] = new_cnt & 0xf;

    return ok;
}

#endif // DTVRECORDER_H
