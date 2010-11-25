// -*- Mode: c++ -*-
/**
 *  DTVRecorder -- base class for Digital Television recorders
 *  Copyright (c) 2003-2004 by Brandon Beattie, Doug Larrick,
 *    Jason Hoos, and Daniel Thor Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef DTVRECORDER_H
#define DTVRECORDER_H

#include <vector>
using namespace std;

#include <QString>

#include "recorderbase.h"
#include "H264Parser.h"

class MPEGStreamData;
class TSPacket;
class QTime;

class DTVRecorder: public RecorderBase
{
  public:
    DTVRecorder(TVRec *rec);
    virtual ~DTVRecorder();

    virtual void SetOption(const QString &opt, const QString &value);
    virtual void SetOption(const QString &name, int value);

    virtual void StopRecording(void) { _request_recording = false; }
    bool IsRecording(void) { return _recording; }
    bool IsErrored(void) { return _error; }

    long long GetFramesWritten(void) { return _frames_written_count; }

    void SetVideoFilters(QString &/*filters*/) {;}
    void Initialize(void) {;}
    int GetVideoFd(void) { return _stream_fd; }

    virtual void SetNextRecording(const ProgramInfo*, RingBuffer*);
    virtual void SetStreamData(void) = 0;
    void SetStreamData(MPEGStreamData* sd);
    MPEGStreamData *GetStreamData(void) const { return _stream_data; }

    virtual void Reset();

  protected:
    void FinishRecording(void);
    void ResetForNewFile(void);

    void HandleKeyframe(uint64_t frameNum, int64_t extra = 0);

    void BufferedWrite(const TSPacket &tspacket);

    // MPEG TS "audio only" support
    bool FindAudioKeyframes(const TSPacket *tspacket);

    // MPEG2 TS support
    bool FindMPEG2Keyframes(const TSPacket* tspacket);

    // MPEG4 AVC / H.264 TS support
    bool FindH264Keyframes(const TSPacket* tspacket);
    void HandleH264Keyframe(void);

    // MPEG2 PS support (Hauppauge PVR-x50/PVR-500)
    void FindPSKeyFrames(const uint8_t *buffer, uint len);

    // For handling other (non audio/video) packets
    bool FindOtherKeyframes(const TSPacket *tspacket);

    // file handle for stream
    int _stream_fd;

    QString _recording_type;

    // used for scanning pes headers for keyframes
    QTime     _audio_timer;
    uint32_t  _start_code;
    int       _first_keyframe;
    unsigned long long _last_gop_seen;
    unsigned long long _last_seq_seen;
    unsigned long long _last_keyframe_seen;
    unsigned int _audio_bytes_remaining;
    unsigned int _video_bytes_remaining;
    unsigned int _other_bytes_remaining;

    // H.264 support
    bool _pes_synced;
    bool _seen_sps;
    H264Parser m_h264_parser;

    /// True if API call has requested a recording be [re]started
    bool _request_recording;
    /// Wait for the a GOP/SEQ-start before sending data
    bool _wait_for_keyframe_option;

    bool _has_written_other_keyframe;

    // state tracking variables
    /// True iff recording is actually being performed
    bool _recording;
    /// True iff irrecoverable recording error detected
    bool _error;

    MPEGStreamData          *_stream_data;

    // packet buffer
    unsigned char* _buffer;
    int            _buffer_size;

    // keyframe finding buffer
    bool                  _buffer_packets;
    vector<unsigned char> _payload_buffer;

    // statistics
    unsigned long long _frames_seen_count;
    unsigned long long _frames_written_count;

    // constants
    /// If the number of regular frames detected since the last
    /// detected keyframe exceeds this value, then we begin marking
    /// random regular frames as keyframes.
    static const uint kMaxKeyFrameDistance;
};

#endif // DTVRECORDER_H
