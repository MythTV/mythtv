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
    DummyDTVRecorder::DummyDTVRecorder(
        bool tsmode = true,        RingBuffer *rbuffer = NULL,
        uint desired_width = 1920, uint desired_height = 1088,
        double desired_frame_rate = 29.97,
        bool autoStart = true);

    DummyDTVRecorder::~DummyDTVRecorder();

    void SetOptionsFromProfile(RecordingProfile*, const QString&,
                               const QString&, const QString&, int) {;}

    void SetDesiredAttributes(uint width, uint height, double frame_rate);

    void StartRecording(void);
    bool Open(void);

    void StartRecordingThread(void);
    void StopRecordingThread(void);
  private:
    int ProcessData(unsigned char *buffer, int len);
    static void *RecordingThread(void*);

    // Settable
    bool       _tsmode;
    uint       _desired_width;
    uint       _desired_height;
    double     _desired_frame_rate;

    // Internal
    int       _stream_data;
    pthread_t _rec_thread;
    struct timeval _next_frame_time;
};

#endif // DUMMYDTVRECORDER_H
