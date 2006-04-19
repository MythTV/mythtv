// -*- Mode: c++ -*-
/**
 *  DummyDTVRecorder -- 
 *  Copyright (c) 2005 by Daniel Thor Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */
#ifndef DUMMYDTVRECORDER_H
#define DUMMYDTVRECORDER_H

#include <sys/time.h>
#include "dtvrecorder.h"

class DummyDTVRecorder: public DTVRecorder
{
  public:
    DummyDTVRecorder(
        TVRec *rec,
        bool tsmode = true,        RingBuffer *rbuffer = NULL,
        uint desired_width = 1920, uint desired_height = 1088,
        double desired_frame_rate = 29.97, uint bits_per_sec = 20000000,
        uint non_buf_frames = 0, bool autoStart = true);

    ~DummyDTVRecorder();

    void SetOptionsFromProfile(RecordingProfile*, const QString&,
                               const QString&, const QString&) {;}

    void SetDesiredAttributes(uint width, uint height, double frame_rate);

    void StartRecording(void);
    bool Open(void);

    void StartRecordingThread(unsigned long long required_frames = 0);
    void StopRecordingThread(void);

  private:
    void Close(void);
    int ProcessData(unsigned char *buffer, int len);
    void FinishRecording(void);
    static void *RecordingThread(void*);

    // Settable
    bool       _tsmode;
    uint       _desired_width;
    uint       _desired_height;
    double     _desired_frame_rate;
    uint       _desired_bitrate;
    uint       _non_buf_frames;

    // Internal
    uint      _packets_in_frame;
    pthread_t _rec_thread;
    struct timeval _next_frame_time;
    struct timeval _next_5th_frame_time;
};

#endif // DUMMYDTVRECORDER_H
