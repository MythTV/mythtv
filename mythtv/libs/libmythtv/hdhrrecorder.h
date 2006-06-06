/**
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

class HDHRRecorder : public DTVRecorder,
                     public MPEGSingleProgramStreamListener,
                     public ATSCMainStreamListener
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
    ATSCStreamData *GetATSCStreamData(void) { return _atsc_stream_data; }

    // MPEG Single Program
    void HandleSingleProgramPAT(ProgramAssociationTable *pat);
    void HandleSingleProgramPMT(ProgramMapTable *pmt);

    // ATSC
    void HandleSTT(const SystemTimeTable*) {}
    void HandleMGT(const MasterGuideTable *mgt);
    void HandleVCT(uint, const VirtualChannelTable*) {}

  private:
    void AdjustEITPIDs(void);
    void ProcessTSData(const unsigned char *buffer, int len);
    bool ProcessTSPacket(const TSPacket& tspacket);
    void TeardownAll(void);
    
  private:
    HDHRChannel                   *_channel;
    struct hdhomerun_video_sock_t *_video_socket;
    ATSCStreamData                *_atsc_stream_data;
    ProgramMapTable               *_pmt;
    ProgramMapTable               *_pmt_copy;

    mutable QMutex                 _lock;
};

#endif
