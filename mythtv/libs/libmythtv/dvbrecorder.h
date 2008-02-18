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
#include <deque>
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
class DVBStreamHandler;

typedef vector<uint>        uint_vec_t;

/** \class DVBRecorder
 *  \brief This is a specialization of DTVRecorder used to
 *         handle streams from DVB drivers.
 *
 *  \sa DTVRecorder
 */
class DVBRecorder :
    public DTVRecorder,
    public MPEGStreamListener,
    public MPEGSingleProgramStreamListener,
    public DVBMainStreamListener,
    public ATSCMainStreamListener,
    public TSPacketListener,
    public TSPacketListenerAV,
    private ReaderPausedCB
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
    void ResetForNewFile(void);
    void StopRecording(void);

    bool Open(void);
    bool IsOpen(void) const { return _stream_fd >= 0; }
    void Close(void);

    void HandlePAT(const ProgramAssociationTable*);
    void HandleCAT(const ConditionalAccessTable*) {}
    void HandlePMT(uint pid, const ProgramMapTable*);
    void HandleEncryptionStatus(uint /*pnum*/, bool /*encrypted*/) { }

    // MPEG Single Program Stream Listener
    void HandleSingleProgramPAT(ProgramAssociationTable *pat);
    void HandleSingleProgramPMT(ProgramMapTable*);

    // ATSC Main
    void HandleSTT(const SystemTimeTable*);
    void HandleVCT(uint /*tsid*/, const VirtualChannelTable*) {}
    void HandleMGT(const MasterGuideTable*) {}

    // DVBMainStreamListener
    void HandleTDT(const TimeDateTable*);
    void HandleNIT(const NetworkInformationTable*) {}
    void HandleSDT(uint /*tsid*/, const ServiceDescriptionTable*) {}

    // TSPacketListener
    bool ProcessTSPacket(const TSPacket& tspacket);

    // TSPacketListenerAV
    bool ProcessVideoTSPacket(const TSPacket& tspacket);
    bool ProcessAudioTSPacket(const TSPacket& tspacket);

    // Common audio/visual processing
    bool ProcessAVTSPacket(const TSPacket &tspacket);

    void SetStreamData(MPEGStreamData*);
    MPEGStreamData* GetStreamData(void) { return _stream_data; }

    void BufferedWrite(const TSPacket &tspacket);

  private:
    void TeardownAll(void);

    inline bool CheckCC(uint pid, uint cc);

    void ReaderPaused(int fd);
    bool PauseAndWait(int timeout = 100);

  private:
    // Options set in SetOption()
    int             _card_number_option;

    // DVB stuff
    DVBChannel       *dvbchannel;
    DVBStreamHandler *_stream_handler;

    // general recorder stuff
    MPEGStreamData *_stream_data;
    mutable QMutex  _pid_lock;
    ProgramAssociationTable *_input_pat; ///< PAT on input side
    ProgramMapTable         *_input_pmt; ///< PMT on input side
    bool                     _has_no_av;

    // TS recorder stuff
    unsigned char   _stream_id[0x1fff];
    unsigned char   _pid_status[0x1fff];
    unsigned char   _continuity_counter[0x1fff];

    // Statistics
    mutable uint        _continuity_error_count;
    mutable uint        _stream_overflow_count;
    mutable uint        _bad_packet_count;

    // Constants
    static const int TSPACKETS_BETWEEN_PSIP_SYNC;
    static const int POLL_INTERVAL;
    static const int POLL_WARNING_TIMEOUT;

    static const unsigned char kPayloadStartSeen = 0x2;
};


inline bool DVBRecorder::CheckCC(uint pid, uint new_cnt)
{
    bool ok = ((((_continuity_counter[pid] + 1) & 0xf) == new_cnt) ||
               (_continuity_counter[pid] == 0xFF));

    _continuity_counter[pid] = new_cnt & 0xf;

    return ok;
}

#endif
