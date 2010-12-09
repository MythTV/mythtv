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
                                       bool no_hardware_decode,
                                       AVCodecContext *avctx)
{
#if defined(Q_OS_MACX)
    PrivateDecoderVDA *vda = new PrivateDecoderVDA();
    if (vda && vda->Init(decoder, no_hardware_decode, avctx))
        return vda;
    delete vda;
#endif

#ifdef USING_CRYSTALHD
    PrivateDecoderCrystalHD *chd = new PrivateDecoderCrystalHD();
    if (chd && chd->Init(decoder, no_hardware_decode, avctx))
        return chd;
    delete chd;
#endif

    return NULL;
}
