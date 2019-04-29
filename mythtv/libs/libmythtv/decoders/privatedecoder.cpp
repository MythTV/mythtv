#include "privatedecoder.h"

#ifdef USING_OPENMAX
#include "privatedecoder_omx.h"
#endif

void PrivateDecoder::GetDecoders(render_opts &opts)
{
#ifdef USING_OPENMAX
    PrivateDecoderOMX::GetDecoders(opts);
#endif

#if !(USING_OPENMAX)
    Q_UNUSED(opts);
#endif
}

PrivateDecoder* PrivateDecoder::Create(const QString &decoder,
                                       PlayerFlags flags,
                                       AVCodecContext *avctx)
{
#ifdef USING_OPENMAX
    PrivateDecoderOMX *omx = new PrivateDecoderOMX;
    if (omx && omx->Init(decoder, flags, avctx))
        return omx;
    delete omx;
#endif

#if !(USING_OPENMAX)
    Q_UNUSED(decoder);
    Q_UNUSED(flags);
    Q_UNUSED(avctx);
#endif

    return nullptr;
}
