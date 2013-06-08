#ifndef AVFORMATDECODERDVD_H
#define AVFORMATDECODERDVD_H

#include <QList>
#include "avformatdecoder.h"

class MythDVDContext;

class AvFormatDecoderDVD : public AvFormatDecoder
{
  public:
    AvFormatDecoderDVD(MythPlayer *parent, const ProgramInfo &pginfo,
                       PlayerFlags flags);
    virtual ~AvFormatDecoderDVD();
    virtual void Reset(bool reset_video_data, bool seek_reset, bool reset_file);
    virtual void UpdateFramesPlayed(void);
    virtual bool GetFrame(DecodeType decodetype); // DecoderBase

  protected:
    virtual int  ReadPacket(AVFormatContext *ctx, AVPacket *pkt);
    virtual bool ProcessVideoPacket(AVStream *stream, AVPacket *pkt);
    virtual bool ProcessVideoFrame(AVStream *stream, AVFrame *mpa_pic);
    virtual bool ProcessDataPacket(AVStream *curstream, AVPacket *pkt,
                                   DecodeType decodetype);

  private:
    virtual bool DoRewindSeek(long long desiredFrame);
    virtual void DoFastForwardSeek(long long desiredFrame, bool &needflush);
    virtual void StreamChangeCheck(void);
    virtual void PostProcessTracks(void);
    virtual int GetAudioLanguage(uint audio_index, uint stream_index);
    virtual AudioTrackType GetAudioTrackType(uint stream_index);

    void CheckContext(int64_t pts);
    void ReleaseLastVideoPkt();
    void ReleaseContext(MythDVDContext *&context);

    long long DVDFindPosition(long long desiredFrame);

    MythDVDContext*        m_curContext;
    QList<MythDVDContext*> m_contextList;
    AVPacket*              m_lastVideoPkt;
    uint32_t               m_lbaLastVideoPkt;
    int                    m_framesReq;
    MythDVDContext*        m_returnContext;
};

#endif // AVFORMATDECODERDVD_H
