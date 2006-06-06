/** -*- Mode: c++ -*-
 *  HDHRRecorder
 *  Copyright (c) 2006 by Silicondust Engineering Ltd.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef HDHOMERUNRECORDER_H_
#define HDHOMERUNRECORDER_H_

#include <qmutex.h>
#include "dtvrecorder.h"
#include "streamlisteners.h"
#include "eitscanner.h"

class HDHRChannel;
class ProgramMapTable;

typedef vector<uint>        uint_vec_t;

class HDHRRecorder : public DTVRecorder,
                     public MPEGStreamListener,
                     public MPEGSingleProgramStreamListener
{
    friend class ATSCStreamData;

  public:
    HDHRRecorder(TVRec *rec, HDHRChannel *channel);
    ~HDHRRecorder();

    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev);

    bool Open(void);
    bool StartData(void);
    void Close(void);

    void StartRecording(void);

    void SetStreamData(MPEGStreamData*);
    MPEGStreamData *GetStreamData(void);
    ATSCStreamData *GetATSCStreamData(void) { return _stream_data; }

    // MPEG Stream Listener
    void HandlePAT(const ProgramAssociationTable*);
    void HandleCAT(const ConditionalAccessTable*) {}
    void HandlePMT(uint pid, const ProgramMapTable*);

    // MPEG Single Program Stream Listener
    void HandleSingleProgramPAT(ProgramAssociationTable *pat);
    void HandleSingleProgramPMT(ProgramMapTable *pmt);

    /*
    // ATSC
    void HandleSTT(const SystemTimeTable*) {}
    void HandleMGT(const MasterGuideTable *mgt);
    void HandleVCT(uint, const VirtualChannelTable*) {}
    */

  private:
    bool AdjustFilters(void);
    bool AdjustEITPIDs(void);

    void ProcessTSData(const unsigned char *buffer, int len);
    bool ProcessTSPacket(const TSPacket& tspacket);
    void TeardownAll(void);
    
  private:
    HDHRChannel                   *_channel;
    struct hdhomerun_video_sock_t *_video_socket;
    ATSCStreamData                *_stream_data;

    ProgramAssociationTable       *_input_pat;
    ProgramMapTable               *_input_pmt;
    bool                           _reset_pid_filters;
    uint_vec_t                     _eit_pids;
    mutable QMutex                 _pid_lock;
};

#endif
