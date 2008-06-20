// -*- Mode: c++ -*-

#ifndef MPEGRECORDER_H_
#define MPEGRECORDER_H_

#include "dtvrecorder.h"
#include "tspacket.h"
#include "mpegstreamdata.h"

struct AVFormatContext;
struct AVPacket;

class MpegRecorder : public DTVRecorder,
                     public MPEGSingleProgramStreamListener,
                     public TSPacketListener,
                     public TSPacketListenerAV
{
  public:
    MpegRecorder(TVRec*);
   ~MpegRecorder();
    void TeardownAll(void);

    void SetOption(const QString &opt, int value);
    void SetOption(const QString &name, const QString &value);
    void SetVideoFilters(QString&) {}

    void SetOptionsFromProfile(RecordingProfile *profile,
                               const QString &videodev, 
                               const QString &audiodev,
                               const QString &vbidev);

    void Initialize(void) {}
    void StartRecording(void);
    void StopRecording(void);
    void Reset(void);

    void Pause(bool clear = true);
    bool PauseAndWait(int timeout = 100);

    bool IsRecording(void) { return recording; }

    bool Open(void);
    int GetVideoFd(void) { return chanfd; }

    // TS
    virtual void SetStreamData(MPEGStreamData*);
    virtual MPEGStreamData *GetStreamData(void) { return _stream_data; }

    // TSPacketListener
    bool ProcessTSPacket(const TSPacket &tspacket);

    // TSPacketListenerAV
    bool ProcessVideoTSPacket(const TSPacket &tspacket);
    bool ProcessAudioTSPacket(const TSPacket &tspacket);
    bool ProcessAVTSPacket(const TSPacket &tspacket);

    // Implements MPEGSingleProgramStreamListener
    void HandleSingleProgramPAT(ProgramAssociationTable *pat);
    void HandleSingleProgramPMT(ProgramMapTable *pmt);

  private:
    bool OpenMpegFileAsInput(void);
    bool OpenV4L2DeviceAsInput(void);
    bool SetIVTVDeviceOptions(int chanfd);
    bool SetV4L2DeviceOptions(int chanfd);
    bool SetFormat(int chanfd);
    bool SetLanguageMode(int chanfd);
    bool SetRecordingVolume(int chanfd);
    bool SetVBIOptions(int chanfd);
    uint GetFilteredStreamType(void) const;
    uint GetFilteredAudioSampleRate(void) const;
    uint GetFilteredAudioLayer(void) const;
    uint GetFilteredAudioBitRate(uint audio_layer) const;

    void ResetForNewFile(void);

    inline bool CheckCC(uint pid, uint cc);

    bool deviceIsMpegFile;
    int bufferSize;

    // Driver info
    QString  card;
    QString  driver;
    uint32_t version;
    bool     usingv4l2;
    bool     has_buggy_vbi;
    bool     has_v4l2_vbi;
    bool     requires_special_pause;

    // State
    bool recording;
    bool encoding;

    // Pausing state
    bool cleartimeonpause;

    // Encoding info
    int width, height;
    int bitrate, maxbitrate, streamtype, aspectratio;
    int audtype, audsamplerate, audbitratel1, audbitratel2, audbitratel3;
    int audvolume;
    unsigned int language; ///< 0 is Main Lang; 1 is SAP Lang; 2 is Dual

    // Input file descriptors
    int chanfd;
    int readfd;

    static const int   audRateL1[];
    static const int   audRateL2[];
    static const int   audRateL3[];
    static const char *streamType[];
    static const char *aspectRatio[];
    static const unsigned int kBuildBufferMaxSize;

    // TS
    MPEGStreamData *_stream_data;
    unsigned char   _stream_id[0x1fff];
    unsigned char   _pid_status[0x1fff];
    unsigned char   _continuity_counter[0x1fff];
    static const unsigned char kPayloadStartSeen = 0x2;

    // Statistics
    mutable uint        _continuity_error_count;
    mutable uint        _stream_overflow_count;
    mutable uint        _bad_packet_count;
};

inline bool MpegRecorder::CheckCC(uint pid, uint new_cnt)
{
    bool ok = ((((_continuity_counter[pid] + 1) & 0xf) == new_cnt) ||
               (_continuity_counter[pid] == 0xFF));

    _continuity_counter[pid] = new_cnt & 0xf;

    return ok;
}

#endif
