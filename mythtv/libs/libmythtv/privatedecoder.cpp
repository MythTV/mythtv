#include "privatedecoder.h"

#if defined(Q_OS_MACX)
#include "privatedecoder_vda.h"
#endif

#ifdef USING_CRYSTALHD
#include "privatedecoder_crystalhd.h"
#endif

void PrivateDecoder::GetDecoders(render_opts &opts)
{
#if defined(Q_OS_MACX)
    PrivateDecoderVDA::GetDecoders(opts);
#endif

#ifdef USING_CRYSTALHD
    PrivateDecoderCrystalHD::GetDecoders(opts);
#endif
}

PrivateDecoder* PrivateDecoder::Create(const QString &decoder,
                                       PlayerFlags flags,
                                       AVCodecContext *avctx)
{
#if defined(Q_OS_MACX)
    PrivateDecoderVDA *vda = new PrivateDecoderVDA();
    if (vda && vda->Init(decoder, flags, avctx))
        return vda;
    delete vda;
#endif

#ifdef USING_CRYSTALHD
    PrivateDecoderCrystalHD *chd = new PrivateDecoderCrystalHD();
    if (chd && chd->Init(decoder, flags, avctx))
        return chd;
    delete chd;
#endif

    return NULL;
}
