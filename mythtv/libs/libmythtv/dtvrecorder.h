// -*- Mode: c++ -*-
/**
 *  DTVRecorder -- base class for DVBRecorder and HDTVRecorder
 *  Copyright (c) 2003-2004 by Brandon Beattie, Doug Larrick,
 *    Jason Hoos, and Daniel Thor Kristjansson
 *  Device ringbuffer added by John Poet
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef DTVRECORDER_H
#define DTVRECORDER_H

#include "recorderbase.h"

class TSPacket;

class DTVRecorder: public RecorderBase
{
  public:
    enum {report_loops = 20000};

    DTVRecorder(void);
    ~DTVRecorder(void);

    void SetOption(const QString &opt, const QString &value)
    {
        RecorderBase::SetOption(opt, value);
    }
    void SetOption(const QString &name, int value);

    void StopRecording(void);
    bool IsRecording(void) { return _recording; }
    bool IsErrored(void) { return _error; }

    void Pause(bool /*clear*/);
    void Unpause(void) { _request_pause = false; }
    bool GetPause(void);
    void WaitForPause(void);

    long long GetKeyframePosition(long long desired);
    long long GetFramesWritten(void) { return _frames_written_count; }

    void SetVideoFilters(QString &/*filters*/) {;}
    void Initialize(void) {;}
    int GetVideoFd(void) { return _stream_fd; }

    void GetBlankFrameMap(QMap<long long, int> &blank_frame_map);
    void Reset();
  protected:
    void FinishRecording(void);
    void FindKeyframes(const TSPacket* tspacket);
    void HandleKeyframe();

    static void *boot_ringbuffer(void *);
    void fill_ringbuffer(void);
    int ringbuf_read(unsigned char *buffer, size_t count);

    size_t loop;

    // used for scanning pes header for group of pictures header
    int _first_keyframe;
    int _position_within_gop_header;
    bool _scanning_pes_header_for_gop;
    bool _keyframe_seen;
    long long _last_keyframe_seen, _last_gop_seen, _last_seq_seen;

    // file handle for stream
    int _stream_fd;

    // irrecoverable recording error detected
    bool _error;

    // API call is requesting action
    bool _request_recording;
    bool _request_pause; // Pause recording: for channel changes, and to stop recording
    bool _wait_for_keyframe_option; // Wait for the a GOP/SEQ-start before sending data
    // recording or pause is actually being performed
    bool _recording;
    bool _paused;
    bool _wait_for_keyframe;

    // packet buffer
    unsigned char* _buffer;
    int  _buffer_size;

    // statistics
    long long _frames_seen_count;
    long long _frames_written_count;

    // position maps for seeking
    QMap<long long, long long> _position_map;
    QMap<long long, long long> _position_map_delta;

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

#endif // DTVRECORDER_H
