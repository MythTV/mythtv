// -*- Mode: c++ -*-
/**
 *  DTVRecorder -- base class for DVBRecorder and HDTVRecorder
 *  Copyright (c) 2003-2004 by Brandon Beattie, Doug Larrick, 
 *    Jason Hoos, and Daniel Thor Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef DTVRECORDER_H
#define DTVRECORDER_H

#include <vector>
using namespace std;

#include "recorderbase.h"

class TSPacket;

class DTVRecorder: public RecorderBase
{
    Q_OBJECT
  public:
    DTVRecorder(TVRec *rec, const char *name = "DTVRecorder");
    ~DTVRecorder();

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
    void ResetForNewFile(void);

    bool FindKeyframes(const TSPacket* tspacket);
    void HandleKeyframe();
    void SavePositionMap(bool force);

    void BufferedWrite(const TSPacket &tspacket);

    // file handle for stream
    int _stream_fd;

    // used for scanning pes headers for keyframes
    bool _keyframe_seen;
    int  _position_within_gop_header;
    long long _first_keyframe;
    long long _last_keyframe_seen;
    long long _last_gop_seen;
    long long _last_seq_seen;

    /// True if API call has requested a recording be [re]started
    bool _request_recording;
    /// Wait for the a GOP/SEQ-start before sending data
    bool _wait_for_keyframe_option;
    bool _wait_for_keyframe;

    // state tracking variables
    /// True iff recording is actually being performed
    bool _recording;
    /// True iff irrecoverable recording error detected
    bool _error;

    // packet buffer
    unsigned char* _buffer;
    int            _buffer_size;

    // keyframe finding buffer
    bool                  _buffer_packets;
    vector<unsigned char> _payload_buffer;

    // statistics
    long long _frames_seen_count;
    long long _frames_written_count;

    // position maps for seeking
    QMutex                     _position_map_lock;
    QMap<long long, long long> _position_map;
    QMap<long long, long long> _position_map_delta;

    // constants
    /// If the number of regular frames detected since the last
    /// detected keyframe exceeds this value, then we begin marking
    /// random regular frames as keyframes.
    static const uint kMaxKeyFrameDistance;
};

#endif // DTVRECORDER_H
