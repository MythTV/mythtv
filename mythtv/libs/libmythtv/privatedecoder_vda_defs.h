#ifndef PRIVATEDECODER_VDA_DEFS_H
#define PRIVATEDECODER_VDA_DEFS_H

#define VDA_DECODER_PATH "/System/Library/Frameworks/VideoDecodeAcceleration.framework/Versions/Current/VideoDecodeAcceleration"

enum
{
    kVDADecodeInfo_Asynchronous = 1UL << 0,
    kVDADecodeInfo_FrameDropped = 1UL << 1
};

enum
{
    kVDADecoderDecodeFlags_DontEmitFrame = 1 << 0
};

enum
{
    kVDADecoderFlush_EmitFrames = 1 << 0		
};

enum
{
    kVDADecoderNoErr                   = 0,
    kVDADecoderHardwareNotSupportedErr = -12470,
    kVDADecoderFormatNotSupportedErr   = -12471,
    kVDADecoderConfigurationError      = -12472,
    kVDADecoderDecoderFailedErr        = -12473,
};

typedef struct OpaqueVDADecoder* VDADecoder;

typedef void (*VDADecoderOutputCallback)
    (void *decompressionOutputRefCon,
     CFDictionaryRef frameInfo,
     OSStatus status,
     uint32_t infoFlags,
     CVImageBufferRef imageBuffer);
  
typedef OSStatus (*MYTH_VDADECODERCREATE)
    (CFDictionaryRef decoderConfiguration,
     CFDictionaryRef destinationImageBufferAttributes,
     VDADecoderOutputCallback *outputCallback,
     void *decoderOutputCallbackRefcon,
     VDADecoder *decoderOut);
typedef OSStatus (*MYTH_VDADECODERDECODE)
    (VDADecoder decoder,
     uint32_t decodeFlags,
     CFTypeRef compressedBuffer,
     CFDictionaryRef frameInfo);
typedef OSStatus (*MYTH_VDADECODERFLUSH)
    (VDADecoder decoder, uint32_t flushFlags);
typedef OSStatus (*MYTH_VDADECODERDESTROY)
    (VDADecoder decoder);
     
#endif // PRIVATEDECODER_VDA_DEFS_H
