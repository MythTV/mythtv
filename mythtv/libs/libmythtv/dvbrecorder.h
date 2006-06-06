// -*- Mode: c++ -*-
/*
 *  Copyright (C) Kenneth Aafloy 2003
 *  
 *  Copyright notice is in dvbrecorder.cpp of the MythTV project.
 */

#ifndef DVBRECORDER_H
#define DVBRECORDER_H

// C++ includes
#include <vector>
using namespace std;

// Qt includes
#include <qmutex.h>

#include "dtvrecorder.h"
#include "tspacket.h"
#include "DeviceReadBuffer.h"

class DVBChannel;
class MPEGStreamData;
class ProgramAssociationTable;
class ProgramMapTable;
class TSPacket;

class PIDInfo
{
  public:
    PIDInfo() :
        filter_fd(-1),      continuityCount(0xFF),
        streamType(0),      pesType(-1),
        isEncrypted(false), payloadStartSeen(false) {;}

    int    filter_fd;         ///< Input filter file descriptor
    uint   continuityCount;   ///< last Continuity Count (sentinel 0xFF)
    uint   streamType;        ///< StreamID
    int    pesType;           ///< PESStreamID
    bool   isEncrypted;       ///< true if PID is marked as encrypted
    bool   payloadStartSeen;  ///< true if payload start packet seen on PID

    inline void Close(void);
    inline bool CheckCC(uint cc);
};
typedef QMap<uint,PIDInfo*> PIDInfoMap;
typedef vector<uint>        uint_vec_t;

/** \class DVBRecorder
 *  \brief This is a specialization of DTVRecorder used to
 *         handle streams from DVB drivers.
 *
 *  \sa DTVRecorder, HDTVRecorder
 */
class DVBRecorder : public DTVRecorder,
                    private ReaderPausedCB,
                    public MPEGStreamListener
{
  public:
    DVBRecorder(TVRec *rec, DVBChannel* dvbchannel);
   ~DVBRecorder();

    void SetOption(const QString &name, int value);

    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev);

    void StartRecording(void);
    void Reset(void);
    void StopRecording(void);

    bool Open(void);
    bool IsOpen(void) const { return _stream_fd >= 0; }
    void Close(void);

    void HandlePAT(const ProgramAssociationTable*);
    void HandleCAT(const ConditionalAccessTable*) {}
    void HandlePMT(uint pid, const ProgramMapTable*);

    void SetStreamData(MPEGStreamData*);
    MPEGStreamData* GetStreamData(void) { return _stream_data; }

  private:
    void TeardownAll(void);

    uint ProcessDataTS(unsigned char *buffer, uint len);
    bool ProcessTSPacket(const TSPacket& tspacket);
    void ProcessTSPacket2(const TSPacket& tspacket);

    bool AdjustFilters(void);
    bool AdjustEITPIDs(void);
    void CloseFilters(void);
    void OpenFilter(uint pid, int pes_type, uint mpeg_stream_type);
    int  OpenFilterFd(uint pid, int pes_type, uint stream_type);

    void SetOutputPAT(ProgramAssociationTable*);
    void SetOutputPMT(ProgramMapTable*);

    void CreatePAT(void);
    void CreatePMT(void);
    void WritePATPMT(void);

    void DebugTSHeader(unsigned char* buffer, int len);

    void ReaderPaused(int fd);
    bool PauseAndWait(int timeout = 100);

    DeviceReadBuffer *_drb;

    void GetTimeStamp(const TSPacket& tspacket);
    void CreateVideoFrame(void);

    void StartDummyVideo(void);
    static void *run_dummy_video(void *param);
    void RunDummyVideo(void);
    void StopDummyVideo(void);

  private:
    // Options set in SetOption()
    int             _card_number_option;

    // DVB stuff
    DVBChannel*     dvbchannel;

    // general recorder stuff
    /// Set when we want to generate a new filter set
    MPEGStreamData *_stream_data;
    bool            _reset_pid_filters;
    QMutex          _pid_lock;
    PIDInfoMap      _pid_infos;
    uint_vec_t      _eit_pids;

    /// PAT on input side
    ProgramAssociationTable *_input_pat;
    /// PMT on input side
    ProgramMapTable         *_input_pmt;

    // TS recorder stuff
    ProgramAssociationTable *_pat;
    ProgramMapTable         *_pmt;
    uint            _pmt_pid;  ///< PID for rewritten PMT
    uint            _next_pat_version;
    uint            _next_pmt_version;
    int             _ts_packets_until_psip_sync;

    // Fake video for streams without video
    uint            _audio_header_pos;
    uint            _video_header_pos;
    uint            _audio_pid;
    int64_t         _video_time_stamp;
    int64_t         _synch_time_stamp;
    int64_t         _audio_time_stamp;
    int64_t         _new_time_stamp;
    uint            _ts_change_count;
    int             _video_stream_fd;
    double          _frames_per_sec;
    uint            _dummy_output_video_pid;
    pthread_t       _video_thread;
    bool            _stop_dummy;
    bool            _dummy_stopped;
    QWaitCondition  _wait_time;
    QWaitCondition  _wait_stop;
    QMutex          _ts_lock;
    uint            _video_cc;

    // Statistics
    mutable uint        _continuity_error_count;
    mutable uint        _stream_overflow_count;
    mutable uint        _bad_packet_count;

    // Constants
    static const int TSPACKETS_BETWEEN_PSIP_SYNC;
    static const int POLL_INTERVAL;
    static const int POLL_WARNING_TIMEOUT;
};

inline void PIDInfo::Close(void)
{
    if (filter_fd >= 0)
        close(filter_fd);
}

inline bool PIDInfo::CheckCC(uint new_cnt)
{
    if (continuityCount == 0xFF)
    {
        continuityCount = new_cnt;
        return true;
    }

    continuityCount = (continuityCount+1) & 0xf;
    if (continuityCount == new_cnt)
        return true;
    
    continuityCount = new_cnt;
    return false;
}

#endif
