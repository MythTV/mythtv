#ifndef HDTVRECORDER_H_
#define HDTVRECORDER_H_

#include "recorderbase.h"
#include "tspacket.h"

struct AVFormatContext;
struct AVPacket;
class ATSCStreamData;

class HDTVRecorder : public RecorderBase
{
  friend class ATSCStreamData;
  friend class TSPacketProcessor;
  public:
    HDTVRecorder();
   ~HDTVRecorder();

    void SetOption(const QString &opt, int value);
    void SetOption(const QString &name, const QString &value)
                       { RecorderBase::SetOption(name, value); }
    void SetVideoFilters(QString &filters);

    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev,
                               const QString &audiodev,
                               const QString &vbidev, int ispip);

    void Initialize(void);
    void StartRecording(void);
    void StopRecording(void);
    void Reset(void);

    void Pause(bool clear = true);
    void Unpause(void);
    bool GetPause(void);
    void WaitForPause(void);
    bool IsRecording(void);
    bool IsErrored(void) { return false; }

    long long GetFramesWritten(void);

    bool Open(void);
    int GetVideoFd(void);

    long long GetKeyframePosition(long long desired);
    void GetBlankFrameMap(QMap<long long, int> &blank_frame_map);

    void ChannelNameChanged(const QString& new_freqid);

  private:
    bool SetupRecording();
    void FinishRecording();

    int ProcessData(unsigned char *buffer, int len);
    bool ProcessTSPacket(const TSPacket& tspacket);
    void HandleVideo(const TSPacket* tspacket);
    void HandleAudio(const TSPacket* tspacket);
    void FindKeyframes(const TSPacket* tspacket);
    void HandleGOP();

    int ResyncStream(unsigned char *buffer, int curr_pos, int len);

    void WritePAT();
    void WritePMT();

    ATSCStreamData* StreamData() { return _atsc_stream_data; }
    const ATSCStreamData* StreamData() const { return _atsc_stream_data; }

    int _atsc_stream_fd;
    ATSCStreamData* _atsc_stream_data;
    unsigned char *_buffer;
    unsigned int _buffer_size;
    bool _error;

    // Wait for the a GOP before sending data, this prevents resolution changes from crashing ffmpeg
    bool _wait_for_gop;

    // API call is requesting action
    bool _request_recording;
    bool _request_pause;
    // recording or pause is actually being performed
    bool _recording;
    bool _paused;

    // statistics
    long long _tspacket_count;
    long long _nullpacket_count;
    long long _resync_count;
    long long _frames_written_count;
    long long _frames_seen_count;

     // used for scanning pes header for group of pictures header
    int _frame_of_first_gop;
    int _position_within_gop_header;
    bool _scanning_pes_header_for_gop;
    bool _gop_seen;

    QMap<long long, long long> _position_map;
    QMap<long long, long long> _position_map_delta;
};

#endif
