// Based upon CDVDVideoCodecVDA from the xbmc project, originally written by
// Scott Davilla (davilla@xbmc.org) and released under the GPLv2

#include "mythlogging.h"
#define LOC QString("VDADec: ")
#define ERR QString("VDADec error: ")

#include "mythframe.h"
#include "myth_imgconvert.h"
#include "util-osx-cocoa.h"
#include "privatedecoder_vda.h"
#ifdef USING_QUARTZ_VIDEO
#undef CodecType
#import  "QuickTime/ImageCompression.h"
#endif
#include "H264Parser.h"

#include <CoreServices/CoreServices.h>

extern "C" {
#include "libavformat/avformat.h"
}
VDALibrary *gVDALib = NULL;

VDALibrary* VDALibrary::GetVDALibrary(void)
{
    static QMutex vda_create_lock;
    QMutexLocker locker(&vda_create_lock);
    if (gVDALib)
        return gVDALib;
    gVDALib = new VDALibrary();
    if (gVDALib)
    {
        if (gVDALib->IsValid())
            return gVDALib;
        delete gVDALib;
    }
    gVDALib = NULL;
    return gVDALib;
}

VDALibrary::VDALibrary(void)
  : decoderCreate(NULL),  decoderDecode(NULL), decoderFlush(NULL),
    decoderDestroy(NULL), decoderConfigWidth(NULL),
    decoderConfigHeight(NULL), decoderConfigSourceFmt(NULL),
    decoderConfigAVCCData(NULL), m_lib(NULL), m_valid(false)
{
    m_lib = new QLibrary(VDA_DECODER_PATH);
    if (m_lib)
    {
        decoderCreate  = (MYTH_VDADECODERCREATE)
                            m_lib->resolve("VDADecoderCreate");
        decoderDecode  = (MYTH_VDADECODERDECODE)
                            m_lib->resolve("VDADecoderDecode");
        decoderFlush   = (MYTH_VDADECODERFLUSH)
                            m_lib->resolve("VDADecoderFlush");
        decoderDestroy = (MYTH_VDADECODERDESTROY)
                            m_lib->resolve("VDADecoderDestroy");
        decoderConfigWidth = (CFStringRef*)
                            m_lib->resolve("kVDADecoderConfiguration_Width");
        decoderConfigHeight = (CFStringRef*)
                            m_lib->resolve("kVDADecoderConfiguration_Height");
        decoderConfigSourceFmt = (CFStringRef*)
                            m_lib->resolve("kVDADecoderConfiguration_SourceFormat");
        decoderConfigAVCCData = (CFStringRef*)
                            m_lib->resolve("kVDADecoderConfiguration_avcCData");
    }

    if (decoderCreate && decoderDecode && decoderFlush && decoderDestroy &&
        decoderConfigHeight && decoderConfigWidth && decoderConfigSourceFmt &&
        decoderConfigAVCCData && m_lib->isLoaded())
    {
        m_valid = true;
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            "Loaded VideoDecodeAcceleration library.");
    }
    else
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Failed to load VideoDecodeAcceleration library.");
}

#define INIT_ST OSStatus vda_st; bool ok = true
#define CHECK_ST \
    ok &= (vda_st == kVDADecoderNoErr); \
    if (!ok) \
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Error at %1:%2 (#%3, %4)") \
              .arg(__FILE__).arg( __LINE__).arg(vda_st) \
              .arg(vda_err_to_string(vda_st)))

QString vda_err_to_string(OSStatus err)
{
    switch (err)
    {
        case kVDADecoderHardwareNotSupportedErr:
            return "Hardware not supported";
        case kVDADecoderFormatNotSupportedErr:
            return "Format not supported";
        case kVDADecoderConfigurationError:
            return "Configuration error";
        case kVDADecoderDecoderFailedErr:
            return "Decoder failed";
        case paramErr:
            return "Parameter error";
    }
    return "Unknown error";
}

////////////////////////////////////////////////////////////////////////////////
// TODO: refactor this so as not to need these ffmpeg routines.
// These are not exposed in ffmpeg's API so we dupe them here.
// AVC helper functions for muxers,
//  * Copyright (c) 2006 Baptiste Coudurier <baptiste.coudurier@smartjog.com>
// This is part of FFmpeg
//  * License as published by the Free Software Foundation; either
//  * version 2.1 of the License, or (at your option) any later version.
#define VDA_RB16(x)                          \
  ((((const uint8_t*)(x))[0] <<  8) |        \
   ((const uint8_t*)(x)) [1])

#define VDA_RB24(x)                          \
  ((((const uint8_t*)(x))[0] << 16) |        \
   (((const uint8_t*)(x))[1] <<  8) |        \
   ((const uint8_t*)(x))[2])

#define VDA_RB32(x)                          \
  ((((const uint8_t*)(x))[0] << 24) |        \
   (((const uint8_t*)(x))[1] << 16) |        \
   (((const uint8_t*)(x))[2] <<  8) |        \
   ((const uint8_t*)(x))[3])

#define VDA_WB32(p, d) { \
  ((uint8_t*)(p))[3] = (d); \
  ((uint8_t*)(p))[2] = (d) >> 8; \
  ((uint8_t*)(p))[1] = (d) >> 16; \
  ((uint8_t*)(p))[0] = (d) >> 24; }

static const uint8_t *avc_find_startcode_internal(const uint8_t *p, const uint8_t *end)
{
    const uint8_t *a = p + 4 - ((intptr_t)p & 3);

    for (end -= 3; p < a && p < end; p++)
    {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    for (end -= 3; p < end; p += 4)
    {
        uint32_t x = *(const uint32_t*)p;
        if ((x - 0x01010101) & (~x) & 0x80808080) // generic
        {
            if (p[1] == 0)
            {
                if (p[0] == 0 && p[2] == 1)
                    return p;
                if (p[2] == 0 && p[3] == 1)
                    return p+1;
            }
            if (p[3] == 0)
            {
                if (p[2] == 0 && p[4] == 1)
                    return p+2;
                if (p[4] == 0 && p[5] == 1)
                    return p+3;
            }
        }
    }

    for (end += 3; p < end; p++)
    {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    return end + 3;
}

const uint8_t *avc_find_startcode(const uint8_t *p, const uint8_t *end)
{
    const uint8_t *out= avc_find_startcode_internal(p, end);
    if (p<out && out<end && !out[-1])
        out--;
    return out;
}

const int avc_parse_nal_units(AVIOContext *pb, const uint8_t *buf_in, int size)
{
    const uint8_t *p = buf_in;
    const uint8_t *end = p + size;
    const uint8_t *nal_start, *nal_end;

    size = 0;
    nal_start = avc_find_startcode(p, end);
    while (nal_start < end)
    {
        while (!*(nal_start++));
        nal_end = avc_find_startcode(nal_start, end);
        avio_wb32(pb, nal_end - nal_start);
        avio_write(pb, nal_start, nal_end - nal_start);
        size += 4 + nal_end - nal_start;
        nal_start = nal_end;
    }
    return size;
}

const int avc_parse_nal_units_buf(const uint8_t *buf_in,
                                  uint8_t **buf, int *size)
{
    AVIOContext *pb;
    int ret = avio_open_dyn_buf(&pb);
    if (ret < 0)
        return ret;

    avc_parse_nal_units(pb, buf_in, *size);

    av_freep(buf);
    *size = avio_close_dyn_buf(pb, buf);
    return 0;
}

const int isom_write_avcc(AVIOContext *pb, const uint8_t *data, int len)
{
    // extradata from bytestream h264, convert to avcC atom data for bitstream
    if (len > 6)
    {
        /* check for h264 start code */
        if (VDA_RB32(data) == 0x00000001 || VDA_RB24(data) == 0x000001)
        {
            uint8_t *buf=NULL, *end, *start;
            uint32_t sps_size=0, pps_size=0;
            uint8_t *sps=0, *pps=0;

            int ret = avc_parse_nal_units_buf(data, &buf, &len);
            if (ret < 0)
                return ret;
            start = buf;
            end = buf + len;

            /* look for sps and pps */
            while (buf < end)
            {
                unsigned int size;
                uint8_t nal_type;
                size = VDA_RB32(buf);
                nal_type = buf[4] & 0x1f;
                if (nal_type == 7) /* SPS */
                {
                    sps = buf + 4;
                    sps_size = size;

                    //parse_sps(sps+1, sps_size-1);
                }
                else if (nal_type == 8) /* PPS */
                {
                    pps = buf + 4;
                    pps_size = size;
                }
                buf += size + 4;
            }
            if (!sps)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid data (sps)");
                return -1;
            }

            avio_w8(pb, 1); /* version */
            avio_w8(pb, sps[1]); /* profile */
            avio_w8(pb, sps[2]); /* profile compat */
            avio_w8(pb, sps[3]); /* level */
            avio_w8(pb, 0xff); /* 6 bits reserved (111111) + 2 bits nal size length - 1 (11) */
            avio_w8(pb, 0xe1); /* 3 bits reserved (111) + 5 bits number of sps (00001) */

            avio_wb16(pb, sps_size);
            avio_write(pb, sps, sps_size);
            if (pps)
            {
                avio_w8(pb, 1); /* number of pps */
                avio_wb16(pb, pps_size);
                avio_write(pb, pps, pps_size);
            }
            av_free(start);
        }
        else
        {
            avio_write(pb, data, len);
        }
    }
    return 0;
}

void PrivateDecoderVDA::GetDecoders(render_opts &opts)
{
    opts.decoders->append("vda");
    (*opts.equiv_decoders)["vda"].append("nuppel");
    (*opts.equiv_decoders)["vda"].append("ffmpeg");
    (*opts.equiv_decoders)["vda"].append("dummy");
}

PrivateDecoderVDA::PrivateDecoderVDA()
  : PrivateDecoder(), m_lib(NULL), m_decoder(NULL), m_size(QSize()),
    m_frame_lock(QMutex::Recursive), m_frames_decoded(0), m_annexb(false),
    m_slice_count(0), m_convert_3byteTo4byteNALSize(false), m_max_ref_frames(0)
{
}

PrivateDecoderVDA::~PrivateDecoderVDA()
{
    Reset();
    if (m_decoder)
    {
        INIT_ST;
        vda_st = m_lib->decoderDestroy((VDADecoder)m_decoder);
        CHECK_ST;
    }
    m_decoder = NULL;
}

bool PrivateDecoderVDA::Init(const QString &decoder,
                             PlayerFlags flags,
                             AVCodecContext *avctx)
{
    if ((decoder != "vda") || (avctx->codec_id != AV_CODEC_ID_H264) ||
        !(flags & kDecodeAllowEXT) || !avctx)
        return false;

    m_lib = VDALibrary::GetVDALibrary();
    if (!m_lib)
        return false;

    uint8_t *extradata = avctx->extradata;
    int extrasize = avctx->extradata_size;
    if (!extradata || extrasize < 7)
        return false;

    CFDataRef avc_cdata = NULL;
    if (extradata[0] != 1)
    {
        if (extradata[0] == 0 && extradata[1] == 0 && extradata[2] == 0 &&
            extradata[3] == 1)
        {
            // video content is from x264 or from bytestream h264 (AnnexB format)
            // NAL reformating to bitstream format needed
            AVIOContext *pb;
            if (avio_open_dyn_buf(&pb) < 0)
            {
                return false;
            }

            m_annexb = true;
            isom_write_avcc(pb, extradata, extrasize);
            // unhook from ffmpeg's extradata
            extradata = NULL;
            // extract the avcC atom data into extradata then write it into avcCData for VDADecoder
            extrasize = avio_close_dyn_buf(pb, &extradata);
            // CFDataCreate makes a copy of extradata contents
            avc_cdata = CFDataCreate(kCFAllocatorDefault,
                                    (const uint8_t*)extradata, extrasize);
            // done with the converted extradata, we MUST free using av_free
            av_free(extradata);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid avcC atom data");
            return false;
        }
    }
    else
    {
        if (extradata[4] == 0xFE)
        {
            // video content is from so silly encoder that think 3 byte NAL sizes
            // are valid, setup to convert 3 byte NAL sizes to 4 byte.
            extradata[4] = 0xFF;
            m_convert_3byteTo4byteNALSize = true;
        }
        // CFDataCreate makes a copy of extradata contents
        avc_cdata = CFDataCreate(kCFAllocatorDefault, (const uint8_t*)extradata,
                                 extrasize);
    }

    OSType format = 'avc1';

    // check the avcC atom's sps for number of reference frames and
    // bail if interlaced, VDA does not handle interlaced h264.
    uint32_t avcc_len = CFDataGetLength(avc_cdata);
    if (avcc_len < 8)
    {
        // avcc atoms with length less than 8 are borked.
        CFRelease(avc_cdata);
        return false;
    }
    bool interlaced = false;
    uint8_t *spc = (uint8_t*)CFDataGetBytePtr(avc_cdata) + 6;
    uint32_t sps_size = VDA_RB16(spc);
    if (sps_size)
    {
        H264Parser *h264_parser = new H264Parser();
        h264_parser->parse_SPS(spc+3, sps_size-1,
                              interlaced, m_max_ref_frames);
        delete h264_parser;
    }
    else
    {
        m_max_ref_frames = avctx->refs;
    }

    bool isMountainLion = false;
    SInt32 majorVersion, minorVersion;

    Gestalt(gestaltSystemVersionMajor, &majorVersion);
    Gestalt(gestaltSystemVersionMinor, &minorVersion);
    if (majorVersion >= 10 && minorVersion >= 8)
    {
        isMountainLion = true;
    }

    if (!isMountainLion && interlaced)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Possible interlaced content. Aborting");
        CFRelease(avc_cdata);
        return false;
    }
    if (m_max_ref_frames == 0)
    {
        m_max_ref_frames = 2;
    }

    if (avctx->profile == FF_PROFILE_H264_MAIN && avctx->level == 32 &&
        m_max_ref_frames > 4)
    {
        // Main@L3.2, VDA cannot handle greater than 4 reference frames
        LOG(VB_GENERAL, LOG_ERR,
            LOC + "Main@L3.2 detected, VDA cannot decode.");
        CFRelease(avc_cdata);
        return false;
    }

    int32_t width  = avctx->coded_width;
    int32_t height = avctx->coded_height;
    m_size         = QSize(width, height);
    m_slice_count  = avctx->slice_count;

    int mbs = ceil((double)width / 16.0f);
    if (((mbs == 49)  || (mbs == 54 ) || (mbs == 59 ) || (mbs == 64) ||
         (mbs == 113) || (mbs == 118) || (mbs == 123) || (mbs == 128)))
    {
        LOG(VB_PLAYBACK, LOG_WARNING, LOC +
            QString("Warning: VDA decoding may not be supported for this "
                    "video stream (width %1)").arg(width));
    }

    CFMutableDictionaryRef destinationImageBufferAttributes =
        CFDictionaryCreateMutable(kCFAllocatorDefault, 1,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks);
#ifdef USING_QUARTZ_VIDEO
    OSType cvPixelFormatType = k422YpCbCr8PixelFormat;
#else
    OSType cvPixelFormatType = kCVPixelFormatType_422YpCbCr8;
#endif
    CFNumberRef pixelFormat  = CFNumberCreate(kCFAllocatorDefault,
                                              kCFNumberSInt32Type,
                                              &cvPixelFormatType);
    CFDictionarySetValue(destinationImageBufferAttributes,
                         kCVPixelBufferPixelFormatTypeKey,
                         pixelFormat);

    //CFDictionaryRef emptyDictionary =
    //    CFDictionaryCreate(kCFAllocatorDefault, NULL, NULL, 0,
    //                       &kCFTypeDictionaryKeyCallBacks,
    //                       &kCFTypeDictionaryValueCallBacks);
    //CFDictionarySetValue(destinationImageBufferAttributes,
    //                     kCVPixelBufferIOSurfacePropertiesKey,
    //                     emptyDictionary);

    CFMutableDictionaryRef decoderConfig =
                            CFDictionaryCreateMutable(
                                            kCFAllocatorDefault, 4,
                                            &kCFTypeDictionaryKeyCallBacks,
                                            &kCFTypeDictionaryValueCallBacks);
    CFNumberRef avc_width  = CFNumberCreate(kCFAllocatorDefault,
                                            kCFNumberSInt32Type, &width);
    CFNumberRef avc_height = CFNumberCreate(kCFAllocatorDefault,
                                            kCFNumberSInt32Type, &height);
    CFNumberRef avc_format = CFNumberCreate(kCFAllocatorDefault,
                                            kCFNumberSInt32Type, &format);

    CFDictionarySetValue(decoderConfig, *m_lib->decoderConfigHeight,    avc_height);
    CFDictionarySetValue(decoderConfig, *m_lib->decoderConfigWidth,     avc_width);
    CFDictionarySetValue(decoderConfig, *m_lib->decoderConfigSourceFmt, avc_format);
    CFDictionarySetValue(decoderConfig, *m_lib->decoderConfigAVCCData,  avc_cdata);
    CFRelease(avc_width);
    CFRelease(avc_height);
    CFRelease(avc_format);
    CFRelease(avc_cdata);

    INIT_ST;
    vda_st = m_lib->decoderCreate(decoderConfig, destinationImageBufferAttributes,
                                  (VDADecoderOutputCallback*)VDADecoderCallback,
                                  this, (VDADecoder*)&m_decoder);
    CHECK_ST;
    CFRelease(decoderConfig);
    CFRelease(destinationImageBufferAttributes);
    //CFRelease(emptyDictionary);
    if (ok)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Created VDA decoder: Size %1x%2 Ref Frames %3 "
                    "Slices %5 AnnexB %6")
            .arg(width).arg(height).arg(m_max_ref_frames)
            .arg(m_slice_count).arg(m_annexb ? "Yes" : "No"));
    }
    m_max_ref_frames = std::min(m_max_ref_frames, 5);
    return ok;
}

bool PrivateDecoderVDA::Reset(void)
{
    if (m_lib && m_decoder)
        m_lib->decoderFlush((VDADecoder)m_decoder, 0 /*dont emit*/);

    m_frames_decoded = 0;
    m_frame_lock.lock();
    while (!m_decoded_frames.empty())
        PopDecodedFrame();
    m_frame_lock.unlock();
    return true;
}

void PrivateDecoderVDA::PopDecodedFrame(void)
{
    QMutexLocker lock(&m_frame_lock);
    if (m_decoded_frames.isEmpty())
        return;
    CVPixelBufferRelease(m_decoded_frames.last().buffer);
    m_decoded_frames.removeLast();
}

bool PrivateDecoderVDA::HasBufferedFrames(void)
{
    m_frame_lock.lock();
    bool result = m_decoded_frames.size() > 0;
    m_frame_lock.unlock();
    return result;
}

int  PrivateDecoderVDA::GetFrame(AVStream *stream,
                                 AVFrame *picture,
                                 int *got_picture_ptr,
                                 AVPacket *pkt)
{
    if (!pkt)

    CocoaAutoReleasePool pool;
    int result = -1;
    if (!m_lib || !stream)
        return result;

    AVCodecContext *avctx = stream->codec;
    if (!avctx)
        return result;

    if (pkt)
    {
        CFDataRef avc_demux;
        CFDictionaryRef params;
        if (m_annexb)
        {
            // convert demuxer packet from bytestream (AnnexB) to bitstream
            AVIOContext *pb;
            int demuxer_bytes;
            uint8_t *demuxer_content;

            if(avio_open_dyn_buf(&pb) < 0)
            {
                return result;
            }
            demuxer_bytes = avc_parse_nal_units(pb, pkt->data, pkt->size);
            demuxer_bytes = avio_close_dyn_buf(pb, &demuxer_content);
            avc_demux = CFDataCreate(kCFAllocatorDefault, demuxer_content, demuxer_bytes);
            av_free(demuxer_content);
        }
        else if (m_convert_3byteTo4byteNALSize)
        {
            // convert demuxer packet from 3 byte NAL sizes to 4 byte
            AVIOContext *pb;
            if (avio_open_dyn_buf(&pb) < 0)
            {
                return result;
            }

            uint32_t nal_size;
            uint8_t *end = pkt->data + pkt->size;
            uint8_t *nal_start = pkt->data;
            while (nal_start < end)
            {
                nal_size = VDA_RB24(nal_start);
                avio_wb32(pb, nal_size);
                nal_start += 3;
                avio_write(pb, nal_start, nal_size);
                nal_start += nal_size;
            }

            uint8_t *demuxer_content;
            int demuxer_bytes = avio_close_dyn_buf(pb, &demuxer_content);
            avc_demux = CFDataCreate(kCFAllocatorDefault, demuxer_content, demuxer_bytes);
            av_free(demuxer_content);
        }
        else
        {
            avc_demux = CFDataCreate(kCFAllocatorDefault, pkt->data, pkt->size);
        }

        CFStringRef keys[4] = { CFSTR("FRAME_PTS"),
                                CFSTR("FRAME_INTERLACED"), CFSTR("FRAME_TFF"),
                                CFSTR("FRAME_REPEAT") };
        CFNumberRef values[5];
        values[0] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type,
                                   &pkt->pts);
        values[1] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt8Type,
                                   &picture->interlaced_frame);
        values[2] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt8Type,
                                   &picture->top_field_first);
        values[3] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt8Type,
                                   &picture->repeat_pict);
        params = CFDictionaryCreate(kCFAllocatorDefault, (const void **)&keys,
                                    (const void **)&values, 4,
                                    &kCFTypeDictionaryKeyCallBacks,
                                    &kCFTypeDictionaryValueCallBacks);

        INIT_ST;
        vda_st = m_lib->decoderDecode((VDADecoder)m_decoder, 0, avc_demux, params);
        CHECK_ST;
        if (vda_st == kVDADecoderNoErr)
            result = pkt->size;
        CFRelease(avc_demux);
        CFRelease(params);
    }

    if (m_decoded_frames.size() < m_max_ref_frames)
        return result;

    *got_picture_ptr = 1;
    m_frame_lock.lock();
    VDAFrame vdaframe = m_decoded_frames.takeLast();
    m_frame_lock.unlock();

    if (avctx->get_buffer2(avctx, picture, 0) < 0)
        return -1;

    picture->reordered_opaque = vdaframe.pts;
    picture->interlaced_frame = vdaframe.interlaced_frame;
    picture->top_field_first  = vdaframe.top_field_first;
    picture->repeat_pict      = vdaframe.repeat_pict;
    VideoFrame *frame         = (VideoFrame*)picture->opaque;

    PixelFormat in_fmt  = PIX_FMT_NONE;
    PixelFormat out_fmt = PIX_FMT_NONE;
    if (vdaframe.format == 'BGRA')
        in_fmt = PIX_FMT_BGRA;
    else if (vdaframe.format == '2vuy')
        in_fmt = PIX_FMT_UYVY422;

    if (frame->codec == FMT_YV12)
        out_fmt = PIX_FMT_YUV420P;

    if (out_fmt != PIX_FMT_NONE && in_fmt != PIX_FMT_NONE && frame->buf)
    {
        CVPixelBufferLockBaseAddress(vdaframe.buffer, 0);
        uint8_t* base = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(vdaframe.buffer, 0);
        AVPicture img_in, img_out;
        avpicture_fill(&img_out, (uint8_t *)frame->buf, out_fmt,
                       frame->width, frame->height);
        avpicture_fill(&img_in, base, in_fmt,
                       frame->width, frame->height);
        myth_sws_img_convert(&img_out, out_fmt, &img_in, in_fmt,
                       frame->width, frame->height);
        CVPixelBufferUnlockBaseAddress(vdaframe.buffer, 0);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to convert decoded frame.");
    }

    CVPixelBufferRelease(vdaframe.buffer);
    return result;
}

void PrivateDecoderVDA::VDADecoderCallback(void *decompressionOutputRefCon,
                                           CFDictionaryRef frameInfo,
                                           OSStatus status,
                                           uint32_t infoFlags,
                                           CVImageBufferRef imageBuffer)
{
    CocoaAutoReleasePool pool;
    PrivateDecoderVDA *decoder = (PrivateDecoderVDA*)decompressionOutputRefCon;

    if (kVDADecodeInfo_FrameDropped & infoFlags)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Callback: Decoder dropped frame");
        return;
    }

    if (!imageBuffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Callback: decoder returned empty buffer.");
        return;
    }

    INIT_ST;
    vda_st = status;
    CHECK_ST;

    OSType format_type = CVPixelBufferGetPixelFormatType(imageBuffer);
    if ((format_type != '2vuy') && (format_type != 'BGRA'))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Callback: image buffer format unknown (%1)")
                .arg(format_type));
        return;
    }

    int64_t pts = AV_NOPTS_VALUE;
    int8_t interlaced = 0;
    int8_t topfirst   = 0;
    int8_t repeatpic  = 0;
    CFNumberRef ptsref = (CFNumberRef)CFDictionaryGetValue(frameInfo,
                                                   CFSTR("FRAME_PTS"));
    CFNumberRef intref = (CFNumberRef)CFDictionaryGetValue(frameInfo,
                                                   CFSTR("FRAME_INTERLACED"));
    CFNumberRef topref = (CFNumberRef)CFDictionaryGetValue(frameInfo,
                                                   CFSTR("FRAME_TFF"));
    CFNumberRef repref = (CFNumberRef)CFDictionaryGetValue(frameInfo,
                                                   CFSTR("FRAME_REPEAT"));

    if (ptsref)
    {
        CFNumberGetValue(ptsref, kCFNumberSInt64Type, &pts);
        CFRelease(ptsref);
    }
    if (intref)
    {
        CFNumberGetValue(intref, kCFNumberSInt8Type, &interlaced);
        CFRelease(intref);
    }
    if (topref)
    {
        CFNumberGetValue(topref, kCFNumberSInt8Type, &topfirst);
        CFRelease(topref);
    }
    if (repref)
    {
        CFNumberGetValue(repref, kCFNumberSInt8Type, &repeatpic);
        CFRelease(repref);
    }

    int64_t time =  (pts != (int64_t)AV_NOPTS_VALUE) ? pts : 0;
    {
        QMutexLocker lock(&decoder->m_frame_lock);
        bool found = false;
        int i = 0;
        for (; i < decoder->m_decoded_frames.size(); i++)
        {
            int64_t pts = decoder->m_decoded_frames[i].pts;
            if (pts != (int64_t)AV_NOPTS_VALUE && time > pts)
            {
                found = true;
                break;
            }
        }

        VDAFrame frame(CVPixelBufferRetain(imageBuffer), format_type,
                       pts, interlaced, topfirst, repeatpic);
        if (!found)
            i = decoder->m_decoded_frames.size();
        decoder->m_decoded_frames.insert(i, frame);
        decoder->m_frames_decoded++;
    }
}
