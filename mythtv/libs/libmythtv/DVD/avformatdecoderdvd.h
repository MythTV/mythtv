#ifndef AVFORMATDECODERDVD_H
#define AVFORMATDECODERDVD_H

#include <QList>
#include "avformatdecoder.h"

#define INVALID_LBA 0xbfffffff

class MythDVDContext;

class AvFormatDecoderDVD : public AvFormatDecoder
{
  public:
    AvFormatDecoderDVD(MythPlayer *parent, const ProgramInfo &pginfo,
                       PlayerFlags flags)
        : AvFormatDecoder(parent, pginfo, flags) {}
    ~AvFormatDecoderDVD() override;
    void Reset(bool reset_video_data, bool seek_reset, bool reset_file) override; // AvFormatDecoder
    void UpdateFramesPlayed(void) override; // AvFormatDecoder
    bool GetFrame(DecodeType Type, bool &Retry) override; // AvFormatDecoder

  protected:
    int  ReadPacket(AVFormatContext *ctx, AVPacket *pkt, bool &storePacket) override; // AvFormatDecoder
    bool ProcessVideoPacket(AVStream *stream, AVPacket *pkt, bool &Retry) override; // AvFormatDecoder
    bool ProcessVideoFrame(AVStream *stream, AVFrame *mpa_pic) override; // AvFormatDecoder
    bool ProcessDataPacket(AVStream *curstream, AVPacket *pkt,
                           DecodeType decodetype) override; // AvFormatDecoder

  private:
    bool DoRewindSeek(long long desiredFrame) override; // AvFormatDecoder
    void DoFastForwardSeek(long long desiredFrame, bool &needflush) override; // AvFormatDecoder
    void StreamChangeCheck(void) override; // AvFormatDecoder
    void PostProcessTracks(void) override; // AvFormatDecoder
    int GetAudioLanguage(uint audio_index, uint stream_index) override; // AvFormatDecoder
    AudioTrackType GetAudioTrackType(uint stream_index) override; // AvFormatDecoder

    void CheckContext(int64_t pts);
    void ReleaseLastVideoPkt();
    static void ReleaseContext(MythDVDContext *&context);

    long long DVDFindPosition(long long desiredFrame);

    MythDVDContext*        m_curContext      {nullptr};
    QList<MythDVDContext*> m_contextList;
    AVPacket*              m_lastVideoPkt    {nullptr};
    uint32_t               m_lbaLastVideoPkt {INVALID_LBA};
    int                    m_framesReq       {0};
    MythDVDContext*        m_returnContext   {nullptr};
};

#endif // AVFORMATDECODERDVD_H
