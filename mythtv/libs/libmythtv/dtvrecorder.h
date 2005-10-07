// -*- Mode: c++ -*-
/**
 *  DTVRecorder -- base class for DVBRecorder and HDTVRecorder
 *  Copyright (c) 2003-2004 by Brandon Beattie, Doug Larrick, 
 *    Jason Hoos, and Daniel Thor Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef DTVRECORDER_H
#define DTVRECORDER_H

#include "recorderbase.h"

class TSPacket;

class DTVRecorder: public RecorderBase
{
    Q_OBJECT
  public:
    DTVRecorder::DTVRecorder(TVRec *rec, const char *name = "DTVRecorder") : 
        RecorderBase(rec, name),
        _first_keyframe(0), _position_within_gop_header(0),
        _keyframe_seen(false), _last_keyframe_seen(0), _last_gop_seen(0),
        _last_seq_seen(0), _stream_fd(-1), _error(false),
        _request_recording(false),
        _wait_for_keyframe_option(true),
        _recording(false), _wait_for_keyframe(true),
        _buffer(0), _buffer_size(0),
        _frames_seen_count(0), _frames_written_count(0) {;}

    void SetOption(const QString &opt, const QString &value)
    {
        RecorderBase::SetOption(opt, value);
    }
    virtual void SetOption(const QString &name, int value);

    virtual void StopRecording(void) { _request_recording = false; }
    bool IsRecording(void) { return _recording; }
    bool IsErrored(void) { return _error; }

    long long GetKeyframePosition(long long desired);
    long long GetFramesWritten(void) { return _frames_written_count; }

    void SetVideoFilters(QString &/*filters*/) {;}
    void Initialize(void) {;}
    int GetVideoFd(void) { return _stream_fd; }

    virtual void SetNextRecording(const ProgramInfo*, RingBuffer*);

    virtual void Reset();
  protected:
    void FinishRecording(void);
    void FindKeyframes(const TSPacket* tspacket);
    void HandleKeyframe();
    void SavePositionMap(bool force);

    // used for scanning pes header for group of pictures header
    int _first_keyframe;
    int _position_within_gop_header;
    bool _keyframe_seen;
    long long _last_keyframe_seen, _last_gop_seen, _last_seq_seen;

    // file handle for stream
    int _stream_fd;

    // irrecoverable recording error detected
    bool _error;

    // API call is requesting action
    bool _request_recording;
    /// Wait for the a GOP/SEQ-start before sending data
    bool _wait_for_keyframe_option;
    /// True iff recording is actually being performed
    bool _recording;
    bool _wait_for_keyframe;

    // packet buffer
    unsigned char* _buffer;
    int  _buffer_size;

    // statistics
    long long _frames_seen_count;
    long long _frames_written_count;

    // position maps for seeking
    QMutex                     _position_map_lock;
    QMap<long long, long long> _position_map;
    QMap<long long, long long> _position_map_delta;
};

#endif // DTVRECORDER_H
