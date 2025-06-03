#ifndef AUDIOOUTPUTUTIL_H_
#define AUDIOOUTPUTUTIL_H_

#include "audiooutputsettings.h"


/**
 * All toFloat variants require 16 byte aligned input and output buffers on x86 for SSE optimised operation
 * The SSE code processes 16 bytes at a time and leaves any remainder for the C
 * - there is no remainder in practice
 */
class MPUBLIC AudioOutputUtil
{
 public:
    static int DecodeAudio(AVCodecContext *ctx,
                           uint8_t *buffer, int &data_size,
                           const AVPacket *pkt);
};

#endif
