// C++ headers
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdio>                      // for SEEK_SET, SEEK_CUR
#include <cstring>
#include <sys/types.h>
#include <vector>                       // for vector
using namespace std;

// Qt headers
#include <QMutex>
#include <QMap>                         // for QMap<>::iterator, QMap

// MythTV headers
#include "mythconfig.h"
#include "nuppeldecoder.h"
#include "mythplayer.h"
#include "mythlogging.h"
#include "programinfo.h"
#include "mythavutil.h"
#include "fourcc.h"
#include "RTjpegN.h"
#include "audiooutput.h"                // for RTjpeg, RTJ_YUV420
#include "audiooutputsettings.h"        // for ::FORMAT_NONE, ::FORMAT_S16, etc
#include "audioplayer.h"                // for AudioPlayer
#include "cc608reader.h"                // for CC608Reader

#include "lzo/lzo1x.h"

extern "C" {
#if HAVE_BIGENDIAN
#include "bswap.h"
#endif
#include "libavutil/opt.h"
}

#define LOC QString("NVD: ")

NuppelDecoder::NuppelDecoder(MythPlayer *parent,
                             const ProgramInfo &pginfo)
    : DecoderBase(parent, pginfo)
{
    // initialize structures
    memset(&m_fileHeader, 0, sizeof(rtfileheader));
    memset(&m_frameHeader, 0, sizeof(rtframeheader));
    memset(&m_extraData, 0, sizeof(extendeddata));
    m_planes[0] = m_planes[1] = m_planes[2] = nullptr;
    m_audioSamples = (uint8_t *)av_mallocz(AudioOutput::kMaxSizeBuffer);

    // set parent class variables
    m_positionMapType = MARK_KEYFRAME;
    m_lastKey = 0;
    m_framesPlayed = 0;
    m_getRawFrames = false;
    m_getRawVideo = false;

    m_rtjd = new RTjpeg();
    int format = RTJ_YUV420;
    m_rtjd->SetFormat(&format);

    if (lzo_init() != LZO_E_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, "NuppelDecoder: lzo_init() failed, aborting");
        m_errored = true;
        return;
    }
}

NuppelDecoder::~NuppelDecoder()
{
    delete m_rtjd;
    delete [] m_ffmpegExtraData;
    if (m_buf)
        av_freep(&m_buf);
    if (m_buf2)
        av_freep(&m_buf2);
    if (m_strm)
        av_freep(&m_strm);

    av_freep(&m_audioSamples);

    while (!m_storedData.empty())
    {
        delete m_storedData.front();
        m_storedData.pop_front();
    }
    CloseAVCodecVideo();
    CloseAVCodecAudio();
}

bool NuppelDecoder::CanHandle(char testbuf[kDecoderProbeBufferSize],
                              int /*testbufsize*/)
{
    return strncmp(testbuf, "NuppelVideo", 11) == 0 ||
           strncmp(testbuf, "MythTVVideo", 11) == 0;
}

MythCodecID NuppelDecoder::GetVideoCodecID(void) const
{
    MythCodecID value = kCodec_NONE;
    if (m_mpaVidCodec)
    {
        if (QString(m_mpaVidCodec->name) == "mpeg4")
            value = kCodec_NUV_MPEG4;
    }
    else if (m_usingExtraData && m_extraData.video_fourcc == FOURCC_DIVX)
        value = kCodec_NUV_MPEG4;
    else
        value = kCodec_NUV_RTjpeg;
    return (value);
}

QString NuppelDecoder::GetRawEncodingType(void)
{
    if (m_mpaVidCtx)
        return ff_codec_id_string(m_mpaVidCtx->codec_id);
    return QString();
}

bool NuppelDecoder::ReadFileheader(struct rtfileheader *fh)
{
    if (m_ringBuffer->Read(fh, FILEHEADERSIZE) != FILEHEADERSIZE)
        return false; // NOLINT(readability-simplify-boolean-expr)

#if HAVE_BIGENDIAN
    fh->width         = bswap_32(fh->width);
    fh->height        = bswap_32(fh->height);
    fh->desiredwidth  = bswap_32(fh->desiredwidth);
    fh->desiredheight = bswap_32(fh->desiredheight);
    fh->aspect        = bswap_dbl(fh->aspect);
    fh->fps           = bswap_dbl(fh->fps);
    fh->videoblocks   = bswap_32(fh->videoblocks);
    fh->audioblocks   = bswap_32(fh->audioblocks);
    fh->textsblocks   = bswap_32(fh->textsblocks);
    fh->keyframedist  = bswap_32(fh->keyframedist);
#endif

    return true;
}

bool NuppelDecoder::ReadFrameheader(struct rtframeheader *fh)
{
    if (m_ringBuffer->Read(fh, FRAMEHEADERSIZE) != FRAMEHEADERSIZE)
        return false; // NOLINT(readability-simplify-boolean-expr)

#if HAVE_BIGENDIAN
    fh->timecode     = bswap_32(fh->timecode);
    fh->packetlength = bswap_32(fh->packetlength);
#endif

    return true;
}

int NuppelDecoder::OpenFile(RingBuffer *rbuffer, bool novideo,
                            char testbuf[kDecoderProbeBufferSize],
                            int /*testbufsize*/)
{
    (void)testbuf;

    m_ringBuffer = rbuffer;
    m_disableVideo = novideo;
    m_tracks[kTrackTypeVideo].clear();
    StreamInfo si(0, 0, 0, 0, 0);
    m_tracks[kTrackTypeVideo].push_back(si);
    m_selectedTrack[kTrackTypeVideo] = si;

    struct rtframeheader frameheader {};
    long long startpos = 0;
    int foundit = 0;
    char *space;

    if (!ReadFileheader(&m_fileHeader))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Error reading file: %1").arg(m_ringBuffer->GetFilename()));
        return -1;
    }

    while ((QString(m_fileHeader.finfo) != "NuppelVideo") &&
           (QString(m_fileHeader.finfo) != "MythTVVideo"))
    {
        m_ringBuffer->Seek(startpos, SEEK_SET);
        char dummychar;
        m_ringBuffer->Read(&dummychar, 1);

        startpos = m_ringBuffer->GetReadPosition();

        if (!ReadFileheader(&m_fileHeader))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Error reading file: %1")
                    .arg(m_ringBuffer->GetFilename()));
            return -1;
        }

        if (startpos > 20000)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Bad file: '%1'")
                    .arg(m_ringBuffer->GetFilename()));
            return -1;
        }
    }

    if (m_fileHeader.aspect > .999 && m_fileHeader.aspect < 1.001)
        m_fileHeader.aspect = 4.0 / 3;
    m_currentAspect = m_fileHeader.aspect;

    GetPlayer()->SetKeyframeDistance(m_fileHeader.keyframedist);
    GetPlayer()->SetVideoParams(m_fileHeader.width, m_fileHeader.height,
                                m_fileHeader.fps, m_currentAspect, false, 16);

    m_videoWidth = m_fileHeader.width;
    m_videoHeight = m_fileHeader.height;
    m_videoSize = m_videoHeight * m_videoWidth * 3 / 2;
    m_keyframeDist = m_fileHeader.keyframedist;
    m_videoFrameRate = m_fileHeader.fps;

    if (!ReadFrameheader(&frameheader))
    {
        LOG(VB_GENERAL, LOG_ERR, "File not big enough for a header");
        return -1;
    }
    if (frameheader.frametype != 'D')
    {
        LOG(VB_GENERAL, LOG_ERR, "Illegal file format");
        return -1;
    }

    space = new char[m_videoSize];

    if (frameheader.comptype == 'F')
    {
        m_ffmpegExtraDataSize = frameheader.packetlength;
        if (m_ffmpegExtraDataSize > 0)
        {
            m_ffmpegExtraData = new uint8_t[m_ffmpegExtraDataSize];
            if (frameheader.packetlength != m_ringBuffer->Read(m_ffmpegExtraData,
                                                     frameheader.packetlength))
            {
                LOG(VB_GENERAL, LOG_ERR,
                    "File not big enough for first frame data");
                delete [] m_ffmpegExtraData;
                m_ffmpegExtraData = nullptr;
                delete [] space;
                return -1;
            }
        }
    }
    else
    {
        if (frameheader.packetlength != m_ringBuffer->Read(space,
                                                     frameheader.packetlength))
        {
            LOG(VB_GENERAL, LOG_ERR,
                "File not big enough for first frame data");
            delete [] space;
            return -1;
        }
    }

    if ((m_videoHeight & 1) == 1)
    {
        m_videoHeight--;
        LOG(VB_GENERAL, LOG_ERR,
            QString("Incompatible video height, reducing to %1")
                .arg( m_videoHeight));
    }

    startpos = m_ringBuffer->GetReadPosition();

    ReadFrameheader(&frameheader);

    if (frameheader.frametype == 'X')
    {
        if (frameheader.packetlength != EXTENDEDSIZE)
        {
            LOG(VB_GENERAL, LOG_ERR, "Corrupt file.  Bad extended frame.");
        }
        else
        {
            m_ringBuffer->Read(&m_extraData, frameheader.packetlength);
#if HAVE_BIGENDIAN
            struct extendeddata *ed = &m_extradata;
            ed->version                 = bswap_32(ed->version);
            ed->video_fourcc            = bswap_32(ed->video_fourcc);
            ed->audio_fourcc            = bswap_32(ed->audio_fourcc);
            ed->audio_sample_rate       = bswap_32(ed->audio_sample_rate);
            ed->audio_bitsPerSample     = bswap_32(ed->audio_BitsPerSample);
            ed->audio_channels          = bswap_32(ed->audio_channels);
            ed->audio_compression_ratio = bswap_32(ed->audio_compression_ratio);
            ed->audio_quality           = bswap_32(ed->audio_quality);
            ed->rtjpeg_quality          = bswap_32(ed->rtjpeg_quality);
            ed->rtjpeg_luma_filter      = bswap_32(ed->rtjpeg_luma_filter);
            ed->rtjpeg_chroma_filter    = bswap_32(ed->rtjpeg_chroma_filter);
            ed->lavc_bitrate            = bswap_32(ed->lavc_bitrate);
            ed->lavc_qmin               = bswap_32(ed->lavc_qmin);
            ed->lavc_qmax               = bswap_32(ed->lavc_qmax);
            ed->lavc_maxqdiff           = bswap_32(ed->lavc_maxqdiff);
            ed->seektable_offset        = bswap_64(ed->seektable_offset);
            ed->keyframeadjust_offset   = bswap_64(ed->keyframeadjust_offset);
#endif
            m_usingExtraData = true;
            ReadFrameheader(&frameheader);
        }
    }

    if (m_usingExtraData && m_extraData.seektable_offset > 0)
    {
        long long currentpos = m_ringBuffer->GetReadPosition();
        struct rtframeheader seek_frameheader {};

        int seekret = m_ringBuffer->Seek(m_extraData.seektable_offset, SEEK_SET);
        if (seekret == -1)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("NuppelDecoder::OpenFile(): seek error (%1)")
                    .arg(strerror(errno)));
        }

        ReadFrameheader(&seek_frameheader);

        if (seek_frameheader.frametype != 'Q')
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Invalid seektable (frametype %1)")
                    .arg((int)seek_frameheader.frametype));
        }
        else
        {
            if (seek_frameheader.packetlength > 0)
            {
                char *seekbuf = new char[seek_frameheader.packetlength];
                m_ringBuffer->Read(seekbuf, seek_frameheader.packetlength);

                int numentries = seek_frameheader.packetlength /
                                 sizeof(struct seektable_entry);
                struct seektable_entry ste {0, 0};
                int offset = 0;

                m_positionMapLock.lock();

                m_positionMap.clear();
                m_positionMap.reserve(numentries);

                for (int z = 0; z < numentries; z++)
                {
                    memcpy(&ste, seekbuf + offset,
                           sizeof(struct seektable_entry));
#if HAVE_BIGENDIAN
                    ste.file_offset     = bswap_64(ste.file_offset);
                    ste.keyframe_number = bswap_32(ste.keyframe_number);
#endif
                    offset += sizeof(struct seektable_entry);

                    PosMapEntry e = {ste.keyframe_number,
                                     ste.keyframe_number * m_keyframeDist,
                                     ste.file_offset};
                    m_positionMap.push_back(e);
                    uint64_t frame_num = ste.keyframe_number * m_keyframeDist;
                    m_frameToDurMap[frame_num] =
                        frame_num * 1000 / m_videoFrameRate;
                    m_durToFrameMap[m_frameToDurMap[frame_num]] = frame_num;
                }
                m_hasFullPositionMap = true;
                m_totalLength = (int)((ste.keyframe_number * m_keyframeDist * 1.0) /
                                     m_videoFrameRate);
                m_totalFrames = (long long)ste.keyframe_number * m_keyframeDist;

                m_positionMapLock.unlock();

                GetPlayer()->SetFileLength(m_totalLength, m_totalFrames);

                delete [] seekbuf;
            }
            else
                LOG(VB_GENERAL, LOG_ERR, "0 length seek table");
        }

        m_ringBuffer->Seek(currentpos, SEEK_SET);
    }

    if (m_usingExtraData && m_extraData.keyframeadjust_offset > 0 &&
        m_hasFullPositionMap)
    {
        long long currentpos = m_ringBuffer->GetReadPosition();
        struct rtframeheader kfa_frameheader {};

        int kfa_ret = m_ringBuffer->Seek(m_extraData.keyframeadjust_offset,
                                       SEEK_SET);
        if (kfa_ret == -1)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("NuppelDecoder::OpenFile(): keyframeadjust (%1)")
                    .arg(strerror(errno)));
        }

        m_ringBuffer->Read(&kfa_frameheader, FRAMEHEADERSIZE);

        if (kfa_frameheader.frametype != 'K')
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Invalid key frame adjust table (frametype %1)")
                    .arg((int)kfa_frameheader.frametype));
        }
        else
        {
            if (kfa_frameheader.packetlength > 0)
            {
                char *kfa_buf = new char[kfa_frameheader.packetlength];
                m_ringBuffer->Read(kfa_buf, kfa_frameheader.packetlength);

                int numentries = kfa_frameheader.packetlength /
                                 sizeof(struct kfatable_entry);
                struct kfatable_entry kfate {};
                int offset = 0;
                int adjust = 0;
                QMap<long long, int> keyFrameAdjustMap;

                for (int z = 0; z < numentries; z++)
                {
                    memcpy(&kfate, kfa_buf + offset,
                           sizeof(struct kfatable_entry));
#if HAVE_BIGENDIAN
                    kfate.adjust          = bswap_32(kfate.adjust);
                    kfate.keyframe_number = bswap_32(kfate.keyframe_number);
#endif
                    offset += sizeof(struct kfatable_entry);

                    keyFrameAdjustMap[kfate.keyframe_number] = kfate.adjust;
                    adjust += kfate.adjust;
                }
                m_hasKeyFrameAdjustTable = true;

                m_totalLength -= (int)(adjust / m_videoFrameRate);
                m_totalFrames -= adjust;
                GetPlayer()->SetFileLength(m_totalLength, m_totalFrames);

                adjust = 0;

                {
                    QMutexLocker locker(&m_positionMapLock);
                    for (size_t i = 0; i < m_positionMap.size(); i++)
                    {
                        long long adj = m_positionMap[i].adjFrame;

                        if (keyFrameAdjustMap.contains(adj))
                            adjust += keyFrameAdjustMap[adj];

                        m_positionMap[i].adjFrame -= adjust;
                    }
                }

                delete [] kfa_buf;
            }
            else
                LOG(VB_GENERAL, LOG_ERR, "0 length key frame adjust table");
        }

        m_ringBuffer->Seek(currentpos, SEEK_SET);
    }

    while (frameheader.frametype != 'A' && frameheader.frametype != 'V' &&
           frameheader.frametype != 'S' && frameheader.frametype != 'T' &&
           frameheader.frametype != 'R')
    {
        m_ringBuffer->Seek(startpos, SEEK_SET);

        char dummychar;
        m_ringBuffer->Read(&dummychar, 1);

        startpos = m_ringBuffer->GetReadPosition();

        if (!ReadFrameheader(&frameheader))
        {
            delete [] space;
            return -1;
        }

        if (startpos > 20000)
        {
            delete [] space;
            return -1;
        }
    }

    foundit = 0;

    m_effDsp = m_audioSamplerate * 100;
    m_audio->SetEffDsp(m_effDsp);

    if (m_usingExtraData)
    {
        m_effDsp = m_extraData.audio_sample_rate * 100;
        m_audio->SetEffDsp(m_effDsp);
        m_audioSamplerate = m_extraData.audio_sample_rate;
#if HAVE_BIGENDIAN
        // Why only if using extradata?
        m_audio_bits_per_sample = m_extraData.audio_bits_per_sample;
#endif
        AudioFormat format = FORMAT_NONE;
        switch (m_extraData.audio_bits_per_sample)
        {
            case 8:  format = FORMAT_U8;  break;
            case 16: format = FORMAT_S16; break;
            case 24: format = FORMAT_S24; break;
            case 32: format = FORMAT_S32; break;
        }

        m_audio->SetAudioParams(format, m_extraData.audio_channels,
                                m_extraData.audio_channels,
                                AV_CODEC_ID_NONE, m_extraData.audio_sample_rate,
                                false /* AC3/DTS pass through */);
        m_audio->ReinitAudio();
        foundit = 1;
    }

    while (!foundit)
    {
        if (frameheader.frametype == 'S')
        {
            if (frameheader.comptype == 'A')
            {
                m_effDsp = frameheader.timecode;
                if (m_effDsp > 0)
                {
                    m_audio->SetEffDsp(m_effDsp);
                    foundit = 1;
                    continue;
                }
            }
        }
        if (frameheader.frametype != 'R' && frameheader.packetlength != 0)
        {
            if (frameheader.packetlength != m_ringBuffer->Read(space,
                                                 frameheader.packetlength))
            {
                foundit = 1;
                continue;
            }
        }

        long long startpos2 = m_ringBuffer->GetReadPosition();

        foundit = !ReadFrameheader(&frameheader);

        bool framesearch = false;

        while (frameheader.frametype != 'A' && frameheader.frametype != 'V' &&
               frameheader.frametype != 'S' && frameheader.frametype != 'T' &&
               frameheader.frametype != 'R' && frameheader.frametype != 'X')
        {
            if (!framesearch)
                LOG(VB_GENERAL, LOG_INFO, "Searching for frame header.");

            framesearch = true;

            m_ringBuffer->Seek(startpos2, SEEK_SET);

            char dummychar;
            m_ringBuffer->Read(&dummychar, 1);

            startpos2 = m_ringBuffer->GetReadPosition();

            foundit = !ReadFrameheader(&frameheader);
            if (foundit)
                break;
        }
    }

    delete [] space;

    m_setReadAhead = false;

    // mpeg4 encodes are small enough that this shouldn't matter
    if (m_usingExtraData && m_extraData.video_fourcc == FOURCC_DIVX)
        m_setReadAhead = true;

    m_bitrate = 0;
    unsigned min_bitrate = 1000;
    if (m_usingExtraData && m_extraData.video_fourcc == FOURCC_DIVX)
    {
        // Use video bitrate, ignore negligible audio bitrate
        m_bitrate = m_extraData.lavc_bitrate / 1000;
    }
    m_bitrate = max(m_bitrate, min_bitrate); // set minimum 1 Mb/s to be safe
    LOG(VB_PLAYBACK, LOG_INFO,
        QString("Setting bitrate to %1 Kb/s").arg(m_bitrate));

    m_ringBuffer->UpdateRawBitrate(GetRawBitrate());

    m_videoSizeTotal = 0;
    m_videoFramesRead = 0;

    m_ringBuffer->Seek(startpos, SEEK_SET);

    m_buf = (unsigned char*)av_malloc(m_videoSize);
    m_strm = (unsigned char*)av_malloc(m_videoSize * 2);

    if (m_hasFullPositionMap)
        return 1;

    if (SyncPositionMap())
        return 1;

    return 0;
}

void release_nuppel_buffer(void *opaque, uint8_t *data)
{
    auto *frame = (VideoFrame*)data;
    auto *nd = (NuppelDecoder*)opaque;
    if (nd && nd->GetPlayer())
        nd->GetPlayer()->DeLimboFrame(frame);
}

int get_nuppel_buffer(struct AVCodecContext *c, AVFrame *pic, int /*flags*/)
{
    auto *nd = (NuppelDecoder *)(c->opaque);

    int i;

    for (i = 0; i < 3; i++)
    {
        pic->data[i] = nd->m_directFrame->buf + nd->m_directFrame->offsets[i];
        pic->linesize[i] = nd->m_directFrame->pitches[i];
    }

    pic->opaque = nd->m_directFrame;

    // Set release method
    AVBufferRef *buffer =
        av_buffer_create((uint8_t*)nd->m_directFrame, 0, release_nuppel_buffer, nd, 0);
    pic->buf[0] = buffer;

    return 0;
}

bool NuppelDecoder::InitAVCodecVideo(int codec)
{
    if (m_mpaVidCodec)
        CloseAVCodecVideo();

    if (m_usingExtraData)
    {
        switch(m_extraData.video_fourcc)
        {
            case FOURCC_DIVX: codec = AV_CODEC_ID_MPEG4;      break;
            case FOURCC_WMV1: codec = AV_CODEC_ID_WMV1;       break;
            case FOURCC_DIV3: codec = AV_CODEC_ID_MSMPEG4V3;  break;
            case FOURCC_MP42: codec = AV_CODEC_ID_MSMPEG4V2;  break;
            case FOURCC_MPG4: codec = AV_CODEC_ID_MSMPEG4V1;  break;
            case FOURCC_MJPG: codec = AV_CODEC_ID_MJPEG;      break;
            case FOURCC_H263: codec = AV_CODEC_ID_H263;       break;
            case FOURCC_H264: codec = AV_CODEC_ID_H264;       break;
            case FOURCC_I263: codec = AV_CODEC_ID_H263I;      break;
            case FOURCC_MPEG: codec = AV_CODEC_ID_MPEG1VIDEO; break;
            case FOURCC_MPG2: codec = AV_CODEC_ID_MPEG2VIDEO; break;
            case FOURCC_HFYU: codec = AV_CODEC_ID_HUFFYUV;    break;
            default: codec = -1;
        }
    }
    m_mpaVidCodec = avcodec_find_decoder((enum AVCodecID)codec);

    if (!m_mpaVidCodec)
    {
        if (m_usingExtraData)
            LOG(VB_GENERAL, LOG_ERR,
                QString("couldn't find video codec (%1)")
                    .arg(m_extraData.video_fourcc));
        else
            LOG(VB_GENERAL, LOG_ERR, "couldn't find video codec");
        return false;
    }

    if (m_mpaVidCodec->capabilities & AV_CODEC_CAP_DR1 && codec != AV_CODEC_ID_MJPEG)
        m_directRendering = true;

    if (m_mpaVidCtx)
        avcodec_free_context(&m_mpaVidCtx);

    m_mpaVidCtx = avcodec_alloc_context3(nullptr);

    m_mpaVidCtx->codec_id = (enum AVCodecID)codec;
    m_mpaVidCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    m_mpaVidCtx->width = m_videoWidth;
    m_mpaVidCtx->height = m_videoHeight;
    m_mpaVidCtx->err_recognition = AV_EF_CRCCHECK | AV_EF_BITSTREAM |
                                   AV_EF_BUFFER;
    m_mpaVidCtx->bits_per_coded_sample = 12;

    if (m_directRendering)
    {
        // m_mpaVidCtx->flags |= CODEC_FLAG_EMU_EDGE;
        m_mpaVidCtx->draw_horiz_band = nullptr;
        m_mpaVidCtx->get_buffer2 = get_nuppel_buffer;
        m_mpaVidCtx->opaque = (void *)this;
    }
    if (m_ffmpegExtraDataSize > 0)
    {
        av_opt_set_int(m_mpaVidCtx, "extern_huff", 1, 0);
        m_mpaVidCtx->extradata = m_ffmpegExtraData;
        m_mpaVidCtx->extradata_size = m_ffmpegExtraDataSize;
    }

    QMutexLocker locker(avcodeclock);
    if (avcodec_open2(m_mpaVidCtx, m_mpaVidCodec, nullptr) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Couldn't find lavc video codec");
        return false;
    }

    return true;
}

void NuppelDecoder::CloseAVCodecVideo(void)
{
    QMutexLocker locker(avcodeclock);

    if (m_mpaVidCodec && m_mpaVidCtx)
        avcodec_free_context(&m_mpaVidCtx);
}

bool NuppelDecoder::InitAVCodecAudio(int codec)
{
    if (m_mpaAudCodec)
        CloseAVCodecAudio();

    if (m_usingExtraData)
    {
        switch(m_extraData.audio_fourcc)
        {
            case FOURCC_LAME: codec = AV_CODEC_ID_MP3;        break;
            case FOURCC_AC3 : codec = AV_CODEC_ID_AC3;        break;
            default: codec = -1;
        }
    }
    m_mpaAudCodec = avcodec_find_decoder((enum AVCodecID)codec);

    if (!m_mpaAudCodec)
    {
        if (m_usingExtraData)
            LOG(VB_GENERAL, LOG_ERR, QString("couldn't find audio codec (%1)")
                    .arg(m_extraData.audio_fourcc));
        else
            LOG(VB_GENERAL, LOG_ERR, "couldn't find audio codec");
        return false;
    }

    if (m_mpaAudCtx)
        avcodec_free_context(&m_mpaAudCtx);

    m_mpaAudCtx = avcodec_alloc_context3(nullptr);

    m_mpaAudCtx->codec_id = (enum AVCodecID)codec;
    m_mpaAudCtx->codec_type = AVMEDIA_TYPE_AUDIO;

    QMutexLocker locker(avcodeclock);
    if (avcodec_open2(m_mpaAudCtx, m_mpaAudCodec, nullptr) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Couldn't find lavc audio codec");
        return false;
    }

    return true;
}

void NuppelDecoder::CloseAVCodecAudio(void)
{
    QMutexLocker locker(avcodeclock);

    if (m_mpaAudCodec && m_mpaAudCtx)
        avcodec_free_context(&m_mpaAudCtx);
}

static void CopyToVideo(unsigned char *buf, int video_width,
                        int video_height, VideoFrame *frame)
{
    (void)video_width;
    (void)video_height;
    copybuffer(frame, buf, frame->width);
}

bool NuppelDecoder::DecodeFrame(struct rtframeheader *frameheader,
                                unsigned char *lstrm, VideoFrame *frame)
{
    lzo_uint out_len;
    int compoff = 0;

    m_directFrame = frame;

    if (!m_buf2)
    {
        m_buf2 = (unsigned char*)av_malloc(m_videoSize + 64);
        m_planes[0] = m_buf;
        m_planes[1] = m_planes[0] + m_videoWidth * m_videoHeight;
        m_planes[2] = m_planes[1] + (m_videoWidth * m_videoHeight) / 4;
    }

    if (frameheader->comptype == 'N')
    {
        memset(frame->buf, 0, frame->pitches[0] * m_videoHeight);
        memset(frame->buf + frame->offsets[1], 127,
               frame->pitches[1] * frame->height / 2);
        memset(frame->buf + frame->offsets[2], 127,
               frame->pitches[2] * frame->height / 2);
        return true;
    }

    if (frameheader->comptype == 'L')
    {
        switch(m_lastCt)
        {
            case '0': case '3':
                CopyToVideo(m_buf2, m_videoWidth, m_videoHeight, frame);
                break;
            case '1': case '2':
            default:
                CopyToVideo(m_buf, m_videoWidth, m_videoHeight, frame);
                break;
        }
        return true;
    }

    compoff = 1;
    if (frameheader->comptype == '2' || frameheader->comptype == '3')
        compoff=0;

    m_lastCt = frameheader->comptype;

    if (!compoff)
    {
        int r = lzo1x_decompress(lstrm, frameheader->packetlength, m_buf2, &out_len,
                              nullptr);
        if (r != LZO_E_OK)
        {
            LOG(VB_GENERAL, LOG_ERR, "minilzo: can't decompress illegal data");
        }
    }

    if (frameheader->comptype == '0')
    {
        CopyToVideo(lstrm, m_videoWidth, m_videoHeight, frame);
        return true;
    }

    if (frameheader->comptype == '3')
    {
        CopyToVideo(m_buf2, m_videoWidth, m_videoHeight, frame);
        return true;
    }

    if (frameheader->comptype == '2' || frameheader->comptype == '1')
    {
        if (compoff)
            m_rtjd->Decompress((int8_t*)lstrm, m_planes);
        else
            m_rtjd->Decompress((int8_t*)m_buf2, m_planes);

        CopyToVideo(m_buf, m_videoWidth, m_videoHeight, frame);
    }
    else
    {
        if (!m_mpaVidCodec)
            InitAVCodecVideo(frameheader->comptype - '3');

        if (!m_mpaVidCtx)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "NULL mpa_vidctx");
            return false;
        }

        MythAVFrame mpa_pic;
        if (!mpa_pic)
            return false;
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data = lstrm;
        pkt.size = frameheader->packetlength;

        {
            QMutexLocker locker(avcodeclock);
            // if directrendering, writes into buf
            bool gotpicture = false;
            //  SUGGESTION
            //  Now that avcodec_decode_video2 is deprecated and replaced
            //  by 2 calls (receive frame and send packet), this could be optimized
            //  into separate routines or separate threads.
            //  Also now that it always consumes a whole buffer some code
            //  in the caller may be able to be optimized.
            int ret = avcodec_receive_frame(m_mpaVidCtx, mpa_pic);
            if (ret == 0)
                gotpicture = true;
            if (ret == AVERROR(EAGAIN))
                ret = 0;
            if (ret == 0)
                ret = avcodec_send_packet(m_mpaVidCtx, &pkt);
            m_directFrame = nullptr;
            // The code assumes that there is always space to add a new
            // packet. This seems risky but has always worked.
            // It should actually check if (ret == AVERROR(EAGAIN)) and then keep
            // the packet around and try it again after processing the frame
            // received here.
            if (ret < 0)
            {
                char error[AV_ERROR_MAX_STRING_SIZE];
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    QString("video decode error: %1 (%2)")
                    .arg(av_make_error_string(error, sizeof(error), ret))
                    .arg(gotpicture));
            }
            if (!gotpicture)
            {
                return false;
            }
        }

/* XXX: Broken
        if (mpa_pic->qscale_table != nullptr && mpa_pic->qstride > 0)
        {
            int tablesize = mpa_pic->qstride * ((m_videoHeight + 15) / 16);

            if (frame->qstride != mpa_pic->qstride ||
                frame->qscale_table == nullptr)
            {
                frame->qstride = mpa_pic->qstride;

                if (frame->qscale_table)
                    delete [] frame->qscale_table;

                frame->qscale_table = new unsigned char[tablesize];
            }

            memcpy(frame->qscale_table, mpa_pic->qscale_table, tablesize);
        }
*/

        if (m_directRendering)
            return true;

        AVFrame *tmp = mpa_pic;
        m_copyFrame.Copy(frame, tmp, m_mpaVidCtx->pix_fmt);
    }

    return true;
}

bool NuppelDecoder::isValidFrametype(char type)
{
    switch (type)
    {
        case 'A': case 'V': case 'S': case 'T': case 'R': case 'X':
        case 'M': case 'D': case 'Q': case 'K':
            return true;
        default:
            return false;
    }

    return false;
}

void NuppelDecoder::StoreRawData(unsigned char *newstrm)
{
    unsigned char *strmcpy;
    if (newstrm)
    {
        strmcpy = new unsigned char[m_frameHeader.packetlength];
        memcpy(strmcpy, newstrm, m_frameHeader.packetlength);
    }
    else
        strmcpy = nullptr;

    m_storedData.push_back(new RawDataList(m_frameHeader, strmcpy));
}

// The return value is the number of bytes in storedData before the 'SV' frame
long NuppelDecoder::UpdateStoredFrameNum(long framenum)
{
    long sync_offset = 0;

    auto it = m_storedData.begin();
    for ( ; it != m_storedData.end(); ++it)
    {
        RawDataList *data = *it;
        if (data->frameheader.frametype == 'S' &&
            data->frameheader.comptype == 'V')
        {
            data->frameheader.timecode = framenum;
            return sync_offset;
        }
        sync_offset += FRAMEHEADERSIZE;
        if (data->packet)
            sync_offset += data->frameheader.packetlength;
    }
    return 0;
}

void NuppelDecoder::WriteStoredData(RingBuffer *rb, bool storevid,
                                    long timecodeOffset)
{
    while (!m_storedData.empty())
    {
        RawDataList *data = m_storedData.front();

        if (data->frameheader.frametype != 'S')
            data->frameheader.timecode -= timecodeOffset;

        if (storevid || data->frameheader.frametype != 'V')
        {
            rb->Write(&(data->frameheader), FRAMEHEADERSIZE);
            if (data->packet)
                rb->Write(data->packet, data->frameheader.packetlength);
        }
        m_storedData.pop_front();
        delete data;
    }
}

void NuppelDecoder::ClearStoredData()
{
    while (!m_storedData.empty())
    {
        RawDataList *data = m_storedData.front();
        m_storedData.pop_front();
        delete data;
    }
}

bool NuppelDecoder::GetFrame(DecodeType decodetype, bool& /*Retry*/)
{
    bool gotvideo = false;
    int seeklen = 0;
    AVPacket pkt;

    m_decodedVideoFrame = nullptr;

    while (!gotvideo)
    {
        long long currentposition = m_ringBuffer->GetReadPosition();
        if (m_waitingForChange && currentposition + 4 >= m_readAdjust)
        {
            FileChanged();
            currentposition = m_ringBuffer->GetReadPosition();
        }

        if (!ReadFrameheader(&m_frameHeader))
        {
            SetEof(true);
            return false;
        }


        if (!m_ringBuffer->LiveMode() &&
            ((m_frameHeader.frametype == 'Q') || (m_frameHeader.frametype == 'K')))
        {
            SetEof(true);
            return false;
        }

        bool framesearch = false;

        while (!isValidFrametype(m_frameHeader.frametype))
        {
            if (!framesearch)
                LOG(VB_GENERAL, LOG_INFO, "Searching for frame header.");

            framesearch = true;

            m_ringBuffer->Seek((long long)seeklen-FRAMEHEADERSIZE, SEEK_CUR);

            if (!ReadFrameheader(&m_frameHeader))
            {
                SetEof(true);
                return false;
            }
            seeklen = 1;
        }

        if (m_frameHeader.frametype == 'M')
        {
            int sizetoskip = sizeof(rtfileheader) - sizeof(rtframeheader);
            char *dummy = new char[sizetoskip + 1];

            if (m_ringBuffer->Read(dummy, sizetoskip) != sizetoskip)
            {
                delete [] dummy;
                SetEof(true);
                return false;
            }

            delete [] dummy;
            continue;
        }

        if (m_frameHeader.frametype == 'R')
        {
            if (m_getRawFrames)
                StoreRawData(nullptr);
            continue; // the R-frame has no data packet
        }

        if (m_frameHeader.frametype == 'S')
        {
            if (m_frameHeader.comptype == 'A')
            {
                if (m_frameHeader.timecode > 2000000 &&
                    m_frameHeader.timecode < 5500000)
                {
                    m_effDsp = m_frameHeader.timecode;
                    m_audio->SetEffDsp(m_effDsp);
                }
            }
            else if (m_frameHeader.comptype == 'V')
            {
                m_lastKey = m_frameHeader.timecode;
                m_framesPlayed = (m_frameHeader.timecode > 0 ?
                                m_frameHeader.timecode - 1 : 0);

                if (!m_hasFullPositionMap)
                {
                    long long last_index = 0;
                    long long this_index = m_lastKey / m_keyframeDist;

                    QMutexLocker locker(&m_positionMapLock);
                    if (!m_positionMap.empty())
                        last_index = m_positionMap.back().index;

                    if (this_index > last_index)
                    {
                        PosMapEntry e = {this_index, m_lastKey, currentposition};
                        m_positionMap.push_back(e);
                        m_frameToDurMap[m_lastKey] =
                            m_lastKey * 1000 / m_videoFrameRate;
                        m_durToFrameMap[m_frameToDurMap[m_lastKey]] = m_lastKey;
                    }
                }
            }
            if (m_getRawFrames)
                StoreRawData(nullptr);
        }

        if (m_frameHeader.packetlength > 0)
        {
            if (m_frameHeader.packetlength > 10485760) // arbitrary 10MB limit
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Broken packet: %1 %2")
                        .arg(m_frameHeader.frametype)
                        .arg(m_frameHeader.packetlength));
                SetEof(true);
                return false;
            }
            if (m_ringBuffer->Read(m_strm, m_frameHeader.packetlength) !=
                m_frameHeader.packetlength)
            {
                SetEof(true);
                return false;
            }
        }
        else
            continue;

        if (m_frameHeader.frametype == 'V')
        {
            if (!(kDecodeVideo & decodetype))
            {
                m_framesPlayed++;
                gotvideo = true;
                continue;
            }

            VideoFrame *buf = GetPlayer()->GetNextVideoFrame();
            if (!buf)
                continue;

            bool ret = DecodeFrame(&m_frameHeader, m_strm, buf);
            if (!ret)
            {
                GetPlayer()->DiscardVideoFrame(buf);
                continue;
            }

            buf->aspect = m_currentAspect;
            buf->frameNumber = m_framesPlayed;
            buf->frameCounter = m_frameCounter++;
            buf->dummy = 0;
            GetPlayer()->ReleaseNextVideoFrame(buf, m_frameHeader.timecode);

            // We need to make the frame available ourselves
            // if we are not using ffmpeg/avlib.
            if (m_directFrame)
                GetPlayer()->DeLimboFrame(buf);

            m_decodedVideoFrame = buf;
            gotvideo = true;
            if (m_getRawFrames && m_getRawVideo)
                StoreRawData(m_strm);
            m_framesPlayed++;

            if (!m_setReadAhead)
            {
                m_videoSizeTotal += m_frameHeader.packetlength;
                m_videoFramesRead++;

                if (m_videoFramesRead > 15)
                {
                    m_videoSizeTotal /= m_videoFramesRead;

                    float bps = (m_videoSizeTotal * 8.0F / 1024.0F *
                                 static_cast<float>(m_videoFrameRate));
                    m_bitrate = (uint) (bps * 1.5F);

                    m_ringBuffer->UpdateRawBitrate(GetRawBitrate());
                    m_setReadAhead = true;
                }
            }
            continue;
        }

        if (m_frameHeader.frametype=='A' && (kDecodeAudio & decodetype))
        {
            if ((m_frameHeader.comptype == '3') || (m_frameHeader.comptype == 'A'))
            {
                if (m_getRawFrames)
                    StoreRawData(m_strm);

                if (!m_mpaAudCodec)
                {
                    if (m_frameHeader.comptype == '3')
                        InitAVCodecAudio(AV_CODEC_ID_MP3);
                    else if (m_frameHeader.comptype == 'A')
                        InitAVCodecAudio(AV_CODEC_ID_AC3);
                    else
                    {
                        LOG(VB_GENERAL, LOG_ERR, LOC + QString("GetFrame: "
                                "Unknown audio comptype of '%1', skipping")
                                .arg(m_frameHeader.comptype));
                        return false;
                    }
                }

                av_init_packet(&pkt);
                pkt.data = m_strm;
                pkt.size = m_frameHeader.packetlength;

                QMutexLocker locker(avcodeclock);

                while (pkt.size > 0 && m_audio->HasAudioOut())
                {
                    int data_size = 0;

                    int ret = m_audio->DecodeAudio(m_mpaAudCtx, m_audioSamples,
                                                   data_size, &pkt);
                    if (ret < 0)
                    {
                        LOG(VB_GENERAL, LOG_ERR, LOC + "Unknown audio decoding error");
                        return false;
                    }

                    pkt.size -= ret;
                    pkt.data += ret;
                    if (data_size <= 0)
                        continue;

                    m_audio->AddAudioData((char *)m_audioSamples, data_size,
                                          m_frameHeader.timecode, 0);
                }
            }
            else
            {
                m_getRawFrames = false;
#if HAVE_BIGENDIAN
                // Why endian correct the audio buffer here?
                // Don't big-endian clients have to do it in audiooutBlah.cpp?
                if (m_audio_bits_per_sample == 16) {
                    // swap bytes
                    for (int i = 0; i < (m_frameHeader.packetlength & ~1); i+=2) {
                        char tmp;
                        tmp = m_strm[i+1];
                        m_strm[i+1] = m_strm[i];
                        m_strm[i] = tmp;
                    }
                }
#endif
                LOG(VB_PLAYBACK, LOG_DEBUG, QString("A audio timecode %1")
                                              .arg(m_frameHeader.timecode));
                m_audio->AddAudioData((char *)m_strm, m_frameHeader.packetlength,
                                      m_frameHeader.timecode, 0);
            }
        }

        if (m_frameHeader.frametype == 'T' && (kDecodeVideo & decodetype))
        {
            if (m_getRawFrames)
                StoreRawData(m_strm);

            GetPlayer()->GetCC608Reader()->AddTextData(m_strm, m_frameHeader.packetlength,
                                  m_frameHeader.timecode, m_frameHeader.comptype);
        }

        if (m_frameHeader.frametype == 'S' && m_frameHeader.comptype == 'M')
        {
            unsigned char *eop = m_strm + m_frameHeader.packetlength;
            unsigned char *cur = m_strm;

            struct rtfileheader tmphead {};
            struct rtfileheader *fh = &tmphead;

            memcpy(fh, cur, min((int)sizeof(*fh), m_frameHeader.packetlength));

            while (QString(fh->finfo) != "MythTVVideo" &&
                   cur + m_frameHeader.packetlength <= eop)
            {
                cur++;
                memcpy(fh, cur, min((int)sizeof(*fh), m_frameHeader.packetlength));
            }

            if (QString(fh->finfo) == "MythTVVideo")
            {
#if HAVE_BIGENDIAN
                fh->width         = bswap_32(fh->width);
                fh->height        = bswap_32(fh->height);
                fh->desiredwidth  = bswap_32(fh->desiredwidth);
                fh->desiredheight = bswap_32(fh->desiredheight);
                fh->aspect        = bswap_dbl(fh->aspect);
                fh->fps           = bswap_dbl(fh->fps);
                fh->videoblocks   = bswap_32(fh->videoblocks);
                fh->audioblocks   = bswap_32(fh->audioblocks);
                fh->textsblocks   = bswap_32(fh->textsblocks);
                fh->keyframedist  = bswap_32(fh->keyframedist);
#endif

                m_fileHeader = *fh;

                if (m_fileHeader.aspect > .999 && m_fileHeader.aspect < 1.001)
                    m_fileHeader.aspect = 4.0 / 3;
                m_currentAspect = m_fileHeader.aspect;

                GetPlayer()->SetKeyframeDistance(m_fileHeader.keyframedist);
                GetPlayer()->SetVideoParams(m_fileHeader.width, m_fileHeader.height,
                                            m_fileHeader.fps, m_currentAspect, false, 2);
            }
        }
    }

    m_framesRead = m_framesPlayed;

    return true;
}

void NuppelDecoder::SeekReset(long long newKey, uint skipFrames,
                              bool doFlush, bool discardFrames)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("SeekReset(%1, %2, %3 flush, %4 discard)")
            .arg(newKey).arg(skipFrames)
            .arg((doFlush) ? "do" : "don't")
            .arg((discardFrames) ? "do" : "don't"));

    QMutexLocker locker(avcodeclock);

    DecoderBase::SeekReset(newKey, skipFrames, doFlush, discardFrames);

    if (m_mpaVidCodec && doFlush)
        avcodec_flush_buffers(m_mpaVidCtx);

    if (discardFrames)
        GetPlayer()->DiscardVideoFrames(doFlush, false);

    for (;(skipFrames > 0) && !m_atEof; skipFrames--)
    {
        bool retry = false;
        GetFrame(kDecodeAV, retry);
        if (m_decodedVideoFrame)
            GetPlayer()->DiscardVideoFrame(m_decodedVideoFrame);
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
