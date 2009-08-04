/** -*- Mode: c++ -*-
 *  HDHRRecorder
 *  Copyright (c) 2006-2009 by Silicondust Engineering Ltd.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef HDHOMERUNRECORDER_H_
#define HDHOMERUNRECORDER_H_

// Qt includes
#include <QMutex>

#include "dtvrecorder.h"
#include "streamlisteners.h"
#include "eitscanner.h"

class HDHRChannel;
class ProgramMapTable;
class MPEGStreamData;
class HDHRStreamHandler;

typedef vector<uint>        uint_vec_t;

class HDHRRecorder : public DTVRecorder,
                     public DVBMainStreamListener,
                     public ATSCMainStreamListener,
                     public MPEGStreamListener,
                     public MPEGSingleProgramStreamListener,
                     public TSPacketListener,
                     public TSPacketListenerAV
{
  public:
    HDHRRecorder(TVRec *rec, HDHRChannel *channel);
    ~HDHRRecorder();

    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev);

    void StartRecording(void);
    void ResetForNewFile(void);
    void StopRecording(void);

    bool Open(void);
    bool IsOpen(void) const { return _stream_handler; }
    void Close(void);

    // MPEG Stream Listener
    void HandlePAT(const ProgramAssociationTable*);
    void HandleCAT(const ConditionalAccessTable*) {}
    void HandlePMT(uint pid, const ProgramMapTable*);
    void HandleEncryptionStatus(uint /*pnum*/, bool /*encrypted*/) { }

    // MPEG Single Program Stream Listener
    void HandleSingleProgramPAT(ProgramAssociationTable *pat);
    void HandleSingleProgramPMT(ProgramMapTable *pmt);

    // ATSC Main
    void HandleSTT(const SystemTimeTable*) {}
    void HandleVCT(uint /*tsid*/, const VirtualChannelTable*) {}
    void HandleMGT(const MasterGuideTable*) {}

    // DVBMainStreamListener
    void HandleTDT(const TimeDateTable*) {}
    void HandleNIT(const NetworkInformationTable*) {}
    void HandleSDT(uint /*tsid*/, const ServiceDescriptionTable*) {}

    // TSPacketListener
    bool ProcessTSPacket(const TSPacket &tspacket);

    // TSPacketListenerAV
    bool ProcessVideoTSPacket(const TSPacket& tspacket);
    bool ProcessAudioTSPacket(const TSPacket& tspacket);

    // Common audio/visual processing
    bool ProcessAVTSPacket(const TSPacket &tspacket);

    void SetStreamData(MPEGStreamData*);
    MPEGStreamData *GetStreamData(void) { return _stream_data; }
    ATSCStreamData *GetATSCStreamData(void);

    void BufferedWrite(const TSPacket &tspacket);

  private:
    void TeardownAll(void);

    void ReaderPaused(int fd);
    bool PauseAndWait(int timeout = 100);

  private:
    HDHRChannel              *_channel;
    HDHRStreamHandler        *_stream_handler;

    // general recorder stuff
    MPEGStreamData          *_stream_data;
    mutable QMutex           _pid_lock;
    ProgramAssociationTable *_input_pat; ///< PAT on input side
    ProgramMapTable         *_input_pmt; ///< PMT on input side
    bool                     _has_no_av;

    // TS recorder stuff
    unsigned char _stream_id[0x1fff + 1];
    unsigned char _pid_status[0x1fff + 1];
    unsigned char _continuity_counter[0x1fff + 1];
    vector<TSPacket> _scratch;

    // Constants
    static const int TSPACKETS_BETWEEN_PSIP_SYNC;
    static const int POLL_INTERVAL;
    static const int POLL_WARNING_TIMEOUT;

    static const unsigned char kPayloadStartSeen = 0x2;
};

#endif
