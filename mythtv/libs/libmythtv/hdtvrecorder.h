// -*- Mode: c++ -*-
/**
 *  DTVRecorder -- base class for DVBRecorder and HDTVRecorder
 *  Copyright (c) 2003-2004 by Brandon Beattie, Doug Larrick, 
 *    Jason Hoos, and Daniel Thor Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef HDTVRECORDER_H_
#define HDTVRECORDER_H_

#include "dtvrecorder.h"
#include "tsstats.h"

struct AVFormatContext;
struct AVPacket;
class ATSCStreamData;

class HDTVRecorder : public DTVRecorder
{
  friend class ATSCStreamData;
  friend class TSPacketProcessor;
  public:
    HDTVRecorder();
   ~HDTVRecorder();

    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev, int ispip);

    void StartRecording(void);
    void Reset(void);

    bool Open(void);

    void ChannelNameChanged(const QString& new_freqid);

  private:
    int ProcessData(unsigned char *buffer, int len);
    bool ProcessTSPacket(const TSPacket& tspacket);
    void HandleVideo(const TSPacket* tspacket);
    void HandleAudio(const TSPacket* tspacket);

    int ResyncStream(unsigned char *buffer, int curr_pos, int len);

    void WritePAT();
    void WritePMT();

    ATSCStreamData* StreamData() { return _atsc_stream_data; }
    const ATSCStreamData* StreamData() const { return _atsc_stream_data; }

    ATSCStreamData* _atsc_stream_data;

    // statistics
    TSStats _ts_stats;
    long long _resync_count;
};

#endif
