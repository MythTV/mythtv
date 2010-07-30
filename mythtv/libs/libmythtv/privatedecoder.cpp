#include "privatedecoder.h"
#include "privatedecoder_mpeg2.h"

void PrivateDecoder::GetDecoders(render_opts &opts)
{
    PrivateDecoderMPEG2::GetDecoders(opts);
}

PrivateDecoder* PrivateDecoder::Create(const QString &decoder,
                                       bool no_hardware_decode,
                                       CodecID codec_id,
                                       void* extradata,
                                       int extradata_size)
{
    PrivateDecoder *test = NULL;
    test = new PrivateDecoderMPEG2();
    if (test && test->Init(decoder, no_hardware_decode, codec_id,
                           extradata, extradata_size))
        return test;
    delete test;
    return NULL;
}
