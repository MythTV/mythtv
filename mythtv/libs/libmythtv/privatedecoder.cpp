#include "privatedecoder.h"

#ifdef USING_OPENMAX
#include "privatedecoder_omx.h"
#endif

#ifdef USING_CRYSTALHD
#include "privatedecoder_crystalhd.h"
#endif

void PrivateDecoder::GetDecoders(render_opts &opts)
{
#ifdef USING_OPENMAX
    PrivateDecoderOMX::GetDecoders(opts);
#endif

#ifdef USING_CRYSTALHD
    PrivateDecoderCrystalHD::GetDecoders(opts);
#endif

#if !(defined(Q_OS_MACX) || USING_OPENMAX || USING_CRYSTALHD)
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

#ifdef USING_CRYSTALHD
    PrivateDecoderCrystalHD *chd = new PrivateDecoderCrystalHD();
    if (chd && chd->Init(decoder, flags, avctx))
        return chd;
    delete chd;
#endif

#if !(defined(Q_OS_MACX) || USING_OPENMAX || USING_CRYSTALHD)
    Q_UNUSED(decoder);
    Q_UNUSED(flags);
    Q_UNUSED(avctx);
#endif

    return nullptr;
}
