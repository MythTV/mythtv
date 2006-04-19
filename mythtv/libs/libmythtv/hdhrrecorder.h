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

class HDHRRecorder : public DTVRecorder,
                     public MPEGSingleProgramStreamListener,
                     public ATSCMainStreamListener,
                     public EITSource,
                     public ATSCEITStreamListener
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

    void SetStreamData(ATSCStreamData*);
    ATSCStreamData* GetStreamData(void) { return _atsc_stream_data; }

    // MPEG Single Program
    void HandleSingleProgramPAT(ProgramAssociationTable *pat);
    void HandleSingleProgramPMT(ProgramMapTable *pmt);

    // ATSC
    void HandleSTT(const SystemTimeTable*);
    void HandleMGT(const MasterGuideTable *mgt);
    void HandleVCT(uint, const VirtualChannelTable*);

    // EIT Source
    void SetEITHelper(EITHelper*);
    void SetEITRate(float);

    // ATSC EIT
    void HandleEIT(uint pid, const EventInformationTable*);
    void HandleETT(uint pid, const ExtendedTextTable*);

  private:
    void AdjustEITRate(void);
    void ProcessTSData(const unsigned char *buffer, int len);
    bool ProcessTSPacket(const TSPacket& tspacket);
    void TeardownAll(void);
    
  private:
    HDHRChannel                   *_channel;
    struct hdhomerun_video_sock_t *_video_socket;
    ATSCStreamData                *_atsc_stream_data;

    QMap<uint,uint>                _eit_sourceid_to_channel;
    EITHelper                     *_eit_helper;
    float                          _eit_rate;
    float                          _eit_old_rate;
    mutable QMutex                 _lock;
};

#endif
