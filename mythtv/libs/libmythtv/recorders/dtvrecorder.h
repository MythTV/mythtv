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

#include <QAtomicInt>
#include <QString>

#include "libmythtv/mpeg/H2645Parser.h"
#include "libmythtv/mpeg/streamlisteners.h"
#include "libmythtv/recorders/recorderbase.h"
#include "libmythtv/scantype.h"

class MPEGStreamData;
class TSPacket;
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
    ~DTVRecorder() override;

    void SetOption(const QString &name, const QString &value) override; // RecorderBase
    void SetOption(const QString &name, int value) override; // RecorderBase
    void SetOptionsFromProfile(
        RecordingProfile *profile, const QString &videodev,
        const QString &audiodev, const QString &vbidev) override; // RecorderBase

    bool IsErrored(void) override // RecorderBase
        { return !m_error.isEmpty(); }

    long long GetFramesWritten(void) override // RecorderBase
        { return m_framesWrittenCount; }

    void SetVideoFilters(QString &/*filters*/) override {;} // RecorderBase
    void Initialize(void) override {;} // RecorderBase
    int GetVideoFd(void) override // RecorderBase
        { return m_streamFd; }

    virtual void SetStreamData(MPEGStreamData* data);
    MPEGStreamData *GetStreamData(void) const { return m_streamData; }

    void Reset(void) override; // RecorderBase
    void ClearStatistics(void) override; // RecorderBase
    RecordingQuality *GetRecordingQuality(const RecordingInfo *r) const override; // RecorderBase

    // MPEG Stream Listener
    void HandlePAT(const ProgramAssociationTable *_pat) override; // MPEGStreamListener
    void HandleCAT(const ConditionalAccessTable */*cat*/) override {} // MPEGStreamListener
    void HandlePMT(uint progNum, const ProgramMapTable *_pmt) override; // MPEGStreamListener
    void HandleEncryptionStatus(uint /*pnum*/, bool /*encrypted*/) override { } // MPEGStreamListener

    // MPEG Single Program Stream Listener
    void HandleSingleProgramPAT(ProgramAssociationTable *pat, bool insert) override; // MPEGSingleProgramStreamListener
    void HandleSingleProgramPMT(ProgramMapTable *pmt, bool insert) override; // MPEGSingleProgramStreamListener

    // ATSC Main
    void HandleSTT(const SystemTimeTable */*stt*/) override { UpdateCAMTimeOffset(); } // ATSCMainStreamListener
    void HandleVCT(uint /*tsid*/, const VirtualChannelTable */*vct*/) override {} // ATSCMainStreamListener
    void HandleMGT(const MasterGuideTable */*mgt*/) override {} // ATSCMainStreamListener

    // DVBMainStreamListener
    void HandleTDT(const TimeDateTable */*tdt*/) override { UpdateCAMTimeOffset(); } // DVBMainStreamListener
    void HandleNIT(const NetworkInformationTable */*nit*/) override {} // DVBMainStreamListener
    void HandleSDT(uint /*tsid*/, const ServiceDescriptionTable */*sdt*/) override {} // DVBMainStreamListener

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
    bool FindH2645Keyframes(const TSPacket* tspacket);
    void HandleH2645Keyframe(void);

    // MPEG2 PS support (Hauppauge PVR-x50/PVR-500)
    void FindPSKeyFrames(const uint8_t *buffer, uint len) override; // PSStreamListener

    // For handling other (non audio/video) packets
    bool FindOtherKeyframes(const TSPacket *tspacket);

    inline bool CheckCC(uint pid, uint new_cnt);

    virtual QString GetSIStandard(void) const { return "mpeg"; }
    virtual void SetCAMPMT(const ProgramMapTable */*pmt*/) {}
    virtual void UpdateCAMTimeOffset(void) {}

    // file handle for stream
    int                      m_streamFd                   {-1};

    QString                  m_recordingType              {"all"};

    // used for scanning pes headers for keyframes
    MythTimer                m_pesTimer;
    QElapsedTimer            m_audioTimer;
    uint32_t                 m_startCode                  {0xffffffff};
    int                      m_firstKeyframe              {-1};
    unsigned long long       m_lastGopSeen                {0};
    unsigned long long       m_lastSeqSeen                {0};
    unsigned long long       m_lastKeyframeSeen           {0};
    unsigned int             m_audioBytesRemaining        {0};
    unsigned int             m_videoBytesRemaining        {0};
    unsigned int             m_otherBytesRemaining        {0};

    // MPEG2 parser information
    int                      m_progressiveSequence        {0};
    int                      m_repeatPict                 {0};

    // H.264 support
    bool                     m_pesSynced                  {false};
    bool                     m_seenSps                    {false};
    H2645Parser             *m_h2645Parser                {nullptr};

    /// Wait for the a GOP/SEQ-start before sending data
    bool                     m_waitForKeyframeOption      {true};

    bool                     m_hasWrittenOtherKeyframe    {false};

    // state tracking variables
    /// non-empty iff irrecoverable recording error detected
    QString                  m_error;

    MPEGStreamData          *m_streamData                 {nullptr};

    // keyframe finding buffer
    bool                     m_bufferPackets              {false};
    std::vector<unsigned char> m_payloadBuffer;

    // general recorder stuff
    mutable QRecursiveMutex  m_pidLock;
                             /// PAT on input side
    ProgramAssociationTable *m_inputPat                   {nullptr};
                             /// PMT on input side
    ProgramMapTable         *m_inputPmt                   {nullptr};
    bool                     m_hasNoAV                    {false};

    // TS recorder stuff
    bool                     m_recordMpts                 {false};
    bool                     m_recordMptsOnly             {false};
    MythTimer                m_recordMptsTimer;
    std::array<uint8_t,0x1fff + 1> m_streamId             {0};
    std::array<uint8_t,0x1fff + 1> m_pidStatus            {0};
    std::array<uint8_t,0x1fff + 1> m_continuityCounter    {0};
    std::vector<TSPacket>    m_scratch;

    // Statistics
    int                      m_minimumRecordingQuality    {95};
    bool                     m_usePts                     {false}; // vs use dts
    std::array<uint64_t,256> m_tsCount                    {0};
    std::array<int64_t,256>  m_tsLast                     {};
    std::array<int64_t,256>  m_tsFirst                    {};
    std::array<QDateTime,256>m_tsFirstDt                  {};
    mutable QAtomicInt       m_packetCount                {0};
    mutable QAtomicInt       m_continuityErrorCount       {0};
    unsigned long long       m_framesSeenCount            {0};
    unsigned long long       m_framesWrittenCount         {0};
    /// @brief Total milliseconds that have passed since the start of the recording.
    double                   m_totalDuration              {0.0};
    /// @brief Milliseconds from the start to m_tdTickCount = 0.
    double                   m_tdBase                     {0.0};
    /** @brief Count of the number of equivalent interlaced fields that have passed
               since m_tdBase.

    @note This needs to be divied by 2 to get the number of @e frames corresponding to
          m_tdTickFramerate.
    */
    uint64_t                 m_tdTickCount                {0};
    MythAVRational           m_tdTickFramerate            {0};
    SCAN_t                   m_scanType                   {SCAN_t::UNKNOWN_SCAN};

    // Music Choice
    // Comcast Music Choice uses 3 frames every 6 seconds and no key frames
    bool                     m_musicChoice                {false};

    bool                     m_useIForKeyframe            {true};

    // constants
    /// If the number of regular frames detected since the last
    /// detected keyframe exceeds this value, then we begin marking
    /// random regular frames as keyframes.
    static const uint          kMaxKeyFrameDistance;
    static const unsigned char kPayloadStartSeen = 0x2;
};

inline bool DTVRecorder::CheckCC(uint pid, uint new_cnt)
{
    bool ok = ((((m_continuityCounter[pid] + 1) & 0xf) == new_cnt) ||
               (m_continuityCounter[pid] == new_cnt) ||
               (m_continuityCounter[pid] == 0xFF));

    m_continuityCounter[pid] = new_cnt & 0xf;

    return ok;
}

#endif // DTVRECORDER_H
