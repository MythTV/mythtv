// -*- Mode: c++ -*-
/**
 *  HDTVRecorder
 *  Copyright (c) 2003-2004 by Brandon Beattie, Doug Larrick, 
 *    Jason Hoos, and Daniel Thor Kristjansson
 *  Device ringbuffer added by John Poet
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef HDTVRECORDER_H_
#define HDTVRECORDER_H_

#include "dtvrecorder.h"
#include "tsstats.h"

struct AVFormatContext;
struct AVPacket;
class ATSCStreamData;

/** \class HDTVRecorder
 *  \brief This is a specialization of DTVRecorder used to 
 *         handle streams from bttv drivers, such as the
 *         vendor drivers for the the HD-2000 and HD-3000.
 *
 *  \sa DTVRecorder, DVBRecorder
 */
class HDTVRecorder : public DTVRecorder
{
    friend class ATSCStreamData;
    friend class TSPacketProcessor;
  public:
    enum {report_loops = 20000};

    HDTVRecorder();
   ~HDTVRecorder();

    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev, int ispip);

    void StartRecording(void);
    void StopRecording(void);

    void Pause(bool /*clear*/);
    bool GetPause(void);
    void WaitForPause(void);
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

    static void *boot_ringbuffer(void *);
    void fill_ringbuffer(void);
    int ringbuf_read(unsigned char *buffer, size_t count);


    ATSCStreamData* StreamData() { return _atsc_stream_data; }
    const ATSCStreamData* StreamData() const { return _atsc_stream_data; }

    ATSCStreamData* _atsc_stream_data;

    // statistics
    TSStats _ts_stats;
    long long _resync_count;
    size_t loop;

    // Data for managing the device ringbuffer
    struct {
        pthread_t        thread;
        pthread_mutex_t  lock;
        pthread_mutex_t  lock_stats;
        bool             run;
        bool             eof;
        bool             error;
        bool             request_pause;
        bool             paused;
        size_t           size;
        size_t           used;
        size_t           max_used;
        size_t           avg_used;
        size_t           avg_cnt;
        size_t           dev_read_size;
        size_t           min_read;
        unsigned char  * buffer;
        unsigned char  * readPtr;
        unsigned char  * writePtr;
        unsigned char  * endPtr;
    } ringbuf;
};

#endif
