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
#include "h264utils.h"

class MPEGStreamData;
class TSPacket;

class DTVRecorder: public RecorderBase
{
  public:
    DTVRecorder(TVRec *rec);
    ~DTVRecorder();

    virtual void SetOption(const QString &opt, const QString &value);
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
    virtual void SetStreamData(MPEGStreamData*) {}
    virtual MPEGStreamData *GetStreamData(void) { return NULL; }

    virtual void Reset();

  protected:
    void FinishRecording(void);
    void ResetForNewFile(void);

    void HandleKeyframe();
    void SavePositionMap(bool force);

    void BufferedWrite(const TSPacket &tspacket);

    // MPEG2 support
    bool FindMPEG2Keyframes(const TSPacket* tspacket);

    // MPEG4 AVC / H.264 support
    bool FindH264Keyframes(const TSPacket* tspacket);
    void HandleH264Keyframe(void);

    // file handle for stream
    int _stream_fd;

    QString _recording_type;

    // used for scanning pes headers for keyframes
    uint      _header_pos;
    int       _first_keyframe;
    unsigned long long _last_gop_seen;
    unsigned long long _last_seq_seen;
    unsigned long long _last_keyframe_seen;

    // H.264 support
    bool _pes_synced;
    bool _seen_sps;
    H264::KeyframeSequencer _h264_kf_seq;

    /// True if API call has requested a recording be [re]started
    bool _request_recording;
    /// Wait for the a GOP/SEQ-start before sending data
    bool _wait_for_keyframe_option;

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
    unsigned long long _frames_seen_count;
    unsigned long long _frames_written_count;

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
