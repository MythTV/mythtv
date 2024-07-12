#ifndef MYTHV4L2M2MCONTEXT_H
#define MYTHV4L2M2MCONTEXT_H

// MythTV
#include "mythdrmprimecontext.h"

using V4L2Profiles = QList<MythCodecContext::CodecProfile>;
using V4L2Mapping  = QPair<const uint32_t, const MythCodecContext::CodecProfile>;

class MythV4L2M2MContext : public MythDRMPRIMEContext
{
  public:
    MythV4L2M2MContext(DecoderBase *Parent, MythCodecID CodecID);
   ~MythV4L2M2MContext() override = default;
    static MythCodecID GetSupportedCodec (AVCodecContext **Context,
                                          const AVCodec **Codec,
                                          const QString  &Decoder,
                                          AVStream       *Stream,
                                          uint            StreamType);
    void        InitVideoCodec           (AVCodecContext *Context, bool SelectedStream, bool &DirectRendering) override;
    bool        RetrieveFrame            (AVCodecContext *Context, MythVideoFrame *Frame, AVFrame *AvFrame) override;
    void        SetDecoderOptions        (AVCodecContext* Context, const AVCodec* Codec) override;
    int         HwDecoderInit            (AVCodecContext *Context) override;
    bool        DecoderWillResetOnFlush  () override;
    static bool GetBuffer                (AVCodecContext *Context, MythVideoFrame *Frame, AVFrame *AvFrame, int/*Flags*/);
    static bool HaveV4L2Codecs           (bool Reinit = false);
    static void GetDecoderList           (QStringList &Decoders);

    static enum AVPixelFormat GetV4L2RequestFormat(AVCodecContext *Context, const AVPixelFormat *PixFmt);
    static int  InitialiseV4L2RequestContext(AVCodecContext *Context);

  protected:
    static const V4L2Profiles& GetStandardProfiles();
    static const V4L2Profiles& GetRequestProfiles();

  private:
    static V4L2Profiles GetProfiles(const std::vector<V4L2Mapping> &Profiles);

    bool m_request { false };
};

#endif
