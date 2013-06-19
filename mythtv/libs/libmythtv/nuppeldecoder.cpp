// C headers
#include <cerrno>
#include <cassert>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// C++ headers
#include <algorithm>
#include <iostream>
using namespace std;

// Qt headers
#include <QMutex>

// MythTV headers
#include "mythconfig.h"
#include "nuppeldecoder.h"
#include "mythplayer.h"
#include "remoteencoder.h"
#include "mythlogging.h"
#include "myth_imgconvert.h"
#include "programinfo.h"
#include "audiooutpututil.h"

#include "minilzo.h"

extern "C" {
#if HAVE_BIGENDIAN
#include "bswap.h"
#endif
#include "libavutil/opt.h"
}

#define LOC QString("NVD: ")

NuppelDecoder::NuppelDecoder(MythPlayer *parent,
                             const ProgramInfo &pginfo)
    : DecoderBase(parent, pginfo),
      rtjd(0), video_width(0), video_height(0), video_size(0),
      video_frame_rate(0.0f), audio_samplerate(44100),
#if HAVE_BIGENDIAN
      audio_bits_per_sample(0),
#endif
      ffmpeg_extradatasize(0), ffmpeg_extradata(0), usingextradata(false),
      disablevideo(false), totalLength(0), totalFrames(0), effdsp(0),
      directframe(NULL),            decoded_video_frame(NULL),
      mpa_vidcodec(0), mpa_vidctx(0), mpa_audcodec(0), mpa_audctx(0),
      directrendering(false),
      lastct('1'), strm(0), buf(0), buf2(0),
      videosizetotal(0), videoframesread(0), setreadahead(false)
{
    // initialize structures
    memset(&fileheader, 0, sizeof(rtfileheader));
    memset(&frameheader, 0, sizeof(rtframeheader));
    memset(&extradata, 0, sizeof(extendeddata));
    memset(&tmppicture, 0, sizeof(AVPicture));
    planes[0] = planes[1] = planes[2] = 0;
    m_audioSamples = (uint8_t *)av_mallocz(AVCODEC_MAX_AUDIO_FRAME_SIZE *
                                           sizeof(int32_t));

    // set parent class variables
    positionMapType = MARK_KEYFRAME;
    lastKey = 0;
    framesPlayed = 0;
    getrawframes = false;
    getrawvideo = false;

    rtjd = new RTjpeg();
    int format = RTJ_YUV420;
    rtjd->SetFormat(&format);

    {
        QMutexLocker locker(avcodeclock);
        avcodec_register_all();
    }

    if (lzo_init() != LZO_E_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, "NuppelDecoder: lzo_init() failed, aborting");
        errored = true;
        return;
    }
}

NuppelDecoder::~NuppelDecoder()
{
    if (rtjd)
        delete rtjd;
    if (ffmpeg_extradata)
        delete [] ffmpeg_extradata;
    if (buf)
        delete [] buf;
    if (buf2)
        delete [] buf2;
    if (strm_buf)
        delete [] strm_buf;

    av_free(m_audioSamples);

    while (!StoredData.empty())
    {
        delete StoredData.front();
        StoredData.pop_front();
    }
    CloseAVCodecVideo();
    CloseAVCodecAudio();
}

bool NuppelDecoder::CanHandle(char testbuf[kDecoderProbeBufferSize],
                              int)
{
    if (!strncmp(testbuf, "NuppelVideo", 11) ||
        !strncmp(testbuf, "MythTVVideo", 11))
        return true;
    return false;
}

MythCodecID NuppelDecoder::GetVideoCodecID(void) const
{
    MythCodecID value = kCodec_NONE;
    if (mpa_vidcodec)
    {
        if (QString(mpa_vidcodec->name) == "mpeg4")
            value = kCodec_NUV_MPEG4;
    }
    else if (usingextradata && extradata.video_fourcc == FOURCC_DIVX)
        value = kCodec_NUV_MPEG4;
    else
        value = kCodec_NUV_RTjpeg;
    return (value);
}

QString NuppelDecoder::GetRawEncodingType(void)
{
    if (mpa_vidctx)
        return ff_codec_id_string(mpa_vidctx->codec_id);
    return QString();
}

bool NuppelDecoder::ReadFileheader(struct rtfileheader *fh)
{
    if (ringBuffer->Read(fh, FILEHEADERSIZE) != FILEHEADERSIZE)
        return false;

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
    if (ringBuffer->Read(fh, FRAMEHEADERSIZE) != FRAMEHEADERSIZE)
        return false;

#if HAVE_BIGENDIAN
    fh->timecode     = bswap_32(fh->timecode);
    fh->packetlength = bswap_32(fh->packetlength);
#endif

    return true;
}

int NuppelDecoder::OpenFile(RingBuffer *rbuffer, bool novideo,
                            char testbuf[kDecoderProbeBufferSize],
                            int)
{
    (void)testbuf;

    ringBuffer = rbuffer;
    disablevideo = novideo;
    tracks[kTrackTypeVideo].clear();
    StreamInfo si(0, 0, 0, 0, 0);
    tracks[kTrackTypeVideo].push_back(si);
    selectedTrack[kTrackTypeVideo] = si;

    struct rtframeheader frameheader;
    long long startpos = 0;
    int foundit = 0;
    char *space;

    if (!ReadFileheader(&fileheader))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Error reading file: %1").arg(ringBuffer->GetFilename()));
        return -1;
    }

    while ((QString(fileheader.finfo) != "NuppelVideo") &&
           (QString(fileheader.finfo) != "MythTVVideo"))
    {
        ringBuffer->Seek(startpos, SEEK_SET);
        char dummychar;
        ringBuffer->Read(&dummychar, 1);

        startpos = ringBuffer->GetReadPosition();

        if (!ReadFileheader(&fileheader))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Error reading file: %1")
                    .arg(ringBuffer->GetFilename()));
            return -1;
        }

        if (startpos > 20000)
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Bad file: '%1'")
                    .arg(ringBuffer->GetFilename()));
            return -1;
        }
    }

    if (fileheader.aspect > .999 && fileheader.aspect < 1.001)
        fileheader.aspect = 4.0 / 3;
    current_aspect = fileheader.aspect;

    GetPlayer()->SetKeyframeDistance(fileheader.keyframedist);
    GetPlayer()->SetVideoParams(fileheader.width, fileheader.height,
                                fileheader.fps);

    video_width = fileheader.width;
    video_height = fileheader.height;
    video_size = video_height * video_width * 3 / 2;
    keyframedist = fileheader.keyframedist;
    video_frame_rate = fileheader.fps;

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

    space = new char[video_size];

    if (frameheader.comptype == 'F')
    {
        ffmpeg_extradatasize = frameheader.packetlength;
        if (ffmpeg_extradatasize > 0)
        {
            ffmpeg_extradata = new uint8_t[ffmpeg_extradatasize];
            if (frameheader.packetlength != ringBuffer->Read(ffmpeg_extradata,
                                                     frameheader.packetlength))
            {
                LOG(VB_GENERAL, LOG_ERR,
                    "File not big enough for first frame data");
                delete [] ffmpeg_extradata;
                ffmpeg_extradata = NULL;
                delete [] space;
                return -1;
            }
        }
    }
    else
    {
        if (frameheader.packetlength != ringBuffer->Read(space,
                                                     frameheader.packetlength))
        {
            LOG(VB_GENERAL, LOG_ERR,
                "File not big enough for first frame data");
            delete [] space;
            return -1;
        }
    }

    if ((video_height & 1) == 1)
    {
        video_height--;
        LOG(VB_GENERAL, LOG_ERR,
            QString("Incompatible video height, reducing to %1")
                .arg( video_height));
    }

    startpos = ringBuffer->GetReadPosition();

    ReadFrameheader(&frameheader);

    if (frameheader.frametype == 'X')
    {
        if (frameheader.packetlength != EXTENDEDSIZE)
        {
            LOG(VB_GENERAL, LOG_ERR, "Corrupt file.  Bad extended frame.");
        }
        else
        {
            ringBuffer->Read(&extradata, frameheader.packetlength);
#if HAVE_BIGENDIAN
            struct extendeddata *ed = &extradata;
            ed->version                 = bswap_32(ed->version);
            ed->video_fourcc            = bswap_32(ed->video_fourcc);
            ed->audio_fourcc            = bswap_32(ed->audio_fourcc);
            ed->audio_sample_rate       = bswap_32(ed->audio_sample_rate);
            ed->audio_bits_per_sample   = bswap_32(ed->audio_bits_per_sample);
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
            usingextradata = true;
            ReadFrameheader(&frameheader);
        }
    }

    if (usingextradata && extradata.seektable_offset > 0)
    {
        long long currentpos = ringBuffer->GetReadPosition();
        struct rtframeheader seek_frameheader;

        int seekret = ringBuffer->Seek(extradata.seektable_offset, SEEK_SET);
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
                ringBuffer->Read(seekbuf, seek_frameheader.packetlength);

                int numentries = seek_frameheader.packetlength /
                                 sizeof(struct seektable_entry);
                struct seektable_entry ste;
                int offset = 0;

                ste.file_offset = 0;
                ste.keyframe_number = 0;

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
                                     ste.keyframe_number * keyframedist,
                                     ste.file_offset};
                    m_positionMap.push_back(e);
                    uint64_t frame_num = ste.keyframe_number * keyframedist;
                    m_frameToDurMap[frame_num] =
                        frame_num * 1000 / video_frame_rate;
                    m_durToFrameMap[m_frameToDurMap[frame_num]] = frame_num;
                }
                hasFullPositionMap = true;
                totalLength = (int)((ste.keyframe_number * keyframedist * 1.0) /
                                     video_frame_rate);
                totalFrames = (long long)ste.keyframe_number * keyframedist;

                m_positionMapLock.unlock();

                GetPlayer()->SetFileLength(totalLength, totalFrames);

                delete [] seekbuf;
            }
            else
                LOG(VB_GENERAL, LOG_ERR, "0 length seek table");
        }

        ringBuffer->Seek(currentpos, SEEK_SET);
    }

    if (usingextradata && extradata.keyframeadjust_offset > 0 &&
        hasFullPositionMap)
    {
        long long currentpos = ringBuffer->GetReadPosition();
        struct rtframeheader kfa_frameheader;

        int kfa_ret = ringBuffer->Seek(extradata.keyframeadjust_offset,
                                       SEEK_SET);
        if (kfa_ret == -1)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("NuppelDecoder::OpenFile(): keyframeadjust (%1)")
                    .arg(strerror(errno)));
        }

        ringBuffer->Read(&kfa_frameheader, FRAMEHEADERSIZE);

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
                ringBuffer->Read(kfa_buf, kfa_frameheader.packetlength);

                int numentries = kfa_frameheader.packetlength /
                                 sizeof(struct kfatable_entry);
                struct kfatable_entry kfate;
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
                hasKeyFrameAdjustTable = true;

                totalLength -= (int)(adjust / video_frame_rate);
                totalFrames -= adjust;
                GetPlayer()->SetFileLength(totalLength, totalFrames);

                adjust = 0;

                {
                    QMutexLocker locker(&m_positionMapLock);
                    for (uint i = 0; i < m_positionMap.size(); i++)
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

        ringBuffer->Seek(currentpos, SEEK_SET);
    }

    while (frameheader.frametype != 'A' && frameheader.frametype != 'V' &&
           frameheader.frametype != 'S' && frameheader.frametype != 'T' &&
           frameheader.frametype != 'R')
    {
        ringBuffer->Seek(startpos, SEEK_SET);

        char dummychar;
        ringBuffer->Read(&dummychar, 1);

        startpos = ringBuffer->GetReadPosition();

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

    effdsp = audio_samplerate * 100;
    m_audio->SetEffDsp(effdsp);

    if (usingextradata)
    {
        effdsp = extradata.audio_sample_rate * 100;
        m_audio->SetEffDsp(effdsp);
        audio_samplerate = extradata.audio_sample_rate;
#if HAVE_BIGENDIAN
        // Why only if using extradata?
        audio_bits_per_sample = extradata.audio_bits_per_sample;
#endif
        AudioFormat format = FORMAT_NONE;
        switch (extradata.audio_bits_per_sample)
        {
            case 8:  format = FORMAT_U8;  break;
            case 16: format = FORMAT_S16; break;
            case 24: format = FORMAT_S24; break;
            case 32: format = FORMAT_S32; break;
        }

        m_audio->SetAudioParams(format, extradata.audio_channels,
                                extradata.audio_channels,
                                AV_CODEC_ID_NONE, extradata.audio_sample_rate,
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
                effdsp = frameheader.timecode;
                if (effdsp > 0)
                {
                    m_audio->SetEffDsp(effdsp);
                    foundit = 1;
                    continue;
                }
            }
        }
        if (frameheader.frametype != 'R' && frameheader.packetlength != 0)
        {
            if (frameheader.packetlength != ringBuffer->Read(space,
                                                 frameheader.packetlength))
            {
                foundit = 1;
                continue;
            }
        }

        long long startpos2 = ringBuffer->GetReadPosition();

        foundit = !ReadFrameheader(&frameheader);

        bool framesearch = false;

        while (frameheader.frametype != 'A' && frameheader.frametype != 'V' &&
               frameheader.frametype != 'S' && frameheader.frametype != 'T' &&
               frameheader.frametype != 'R' && frameheader.frametype != 'X')
        {
            if (!framesearch)
                LOG(VB_GENERAL, LOG_INFO, "Searching for frame header.");

            framesearch = true;

            ringBuffer->Seek(startpos2, SEEK_SET);

            char dummychar;
            ringBuffer->Read(&dummychar, 1);

            startpos2 = ringBuffer->GetReadPosition();

            foundit = !ReadFrameheader(&frameheader);
            if (foundit)
                break;
        }
    }

    delete [] space;

    setreadahead = false;

    // mpeg4 encodes are small enough that this shouldn't matter
    if (usingextradata && extradata.video_fourcc == FOURCC_DIVX)
        setreadahead = true;

    bitrate = 0;
    unsigned min_bitrate = 1000;
    if (usingextradata && extradata.video_fourcc == FOURCC_DIVX)
    {
        // Use video bitrate, ignore negligible audio bitrate
        bitrate = extradata.lavc_bitrate / 1000;
    }
    bitrate = max(bitrate, min_bitrate); // set minimum 1 Mb/s to be safe
    LOG(VB_PLAYBACK, LOG_INFO,
        QString("Setting bitrate to %1 Kb/s").arg(bitrate));

    ringBuffer->UpdateRawBitrate(GetRawBitrate());

    videosizetotal = 0;
    videoframesread = 0;

    ringBuffer->Seek(startpos, SEEK_SET);

    buf = new unsigned char[video_size];
    strm_buf = new unsigned char[video_size * 2 + 16];
    strm = (unsigned char*) (((long)strm_buf + 15) & ~0xf);

    if (hasFullPositionMap)
        return 1;

    if (SyncPositionMap())
        return 1;

    return 0;
}

int get_nuppel_buffer(struct AVCodecContext *c, AVFrame *pic)
{
    NuppelDecoder *nd = (NuppelDecoder *)(c->opaque);

    int i;

    for (i = 0; i < 3; i++)
    {
        pic->data[i] = nd->directframe->buf + nd->directframe->offsets[i];
        pic->linesize[i] = nd->directframe->pitches[i];
    }

    pic->opaque = nd->directframe;
    pic->type = FF_BUFFER_TYPE_USER;

    return 1;
}

void release_nuppel_buffer(struct AVCodecContext *c, AVFrame *pic)
{
    (void)c;
    assert(pic->type == FF_BUFFER_TYPE_USER);

    NuppelDecoder *nd = (NuppelDecoder *)(c->opaque);
    if (nd && nd->GetPlayer())
        nd->GetPlayer()->DeLimboFrame((VideoFrame*)pic->opaque);

    int i;
    for (i = 0; i < 4; i++)
        pic->data[i] = NULL;
}

bool NuppelDecoder::InitAVCodecVideo(int codec)
{
    if (mpa_vidcodec)
        CloseAVCodecVideo();

    if (usingextradata)
    {
        switch(extradata.video_fourcc)
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
    mpa_vidcodec = avcodec_find_decoder((enum CodecID)codec);

    if (!mpa_vidcodec)
    {
        if (usingextradata)
            LOG(VB_GENERAL, LOG_ERR,
                QString("couldn't find video codec (%1)")
                    .arg(extradata.video_fourcc));
        else
            LOG(VB_GENERAL, LOG_ERR, "couldn't find video codec");
        return false;
    }

    if (mpa_vidcodec->capabilities & CODEC_CAP_DR1 && codec != AV_CODEC_ID_MJPEG)
        directrendering = true;

    if (mpa_vidctx)
        av_free(mpa_vidctx);

    mpa_vidctx = avcodec_alloc_context3(NULL);

    mpa_vidctx->codec_id = (enum CodecID)codec;
    mpa_vidctx->codec_type = AVMEDIA_TYPE_VIDEO;
    mpa_vidctx->width = video_width;
    mpa_vidctx->height = video_height;
    mpa_vidctx->err_recognition = AV_EF_CRCCHECK | AV_EF_BITSTREAM |
                                  AV_EF_BUFFER;
    mpa_vidctx->bits_per_coded_sample = 12;

    if (directrendering)
    {
        mpa_vidctx->flags |= CODEC_FLAG_EMU_EDGE;
        mpa_vidctx->draw_horiz_band = NULL;
        mpa_vidctx->get_buffer = get_nuppel_buffer;
        mpa_vidctx->release_buffer = release_nuppel_buffer;
        mpa_vidctx->opaque = (void *)this;
    }
    if (ffmpeg_extradatasize > 0)
    {
        av_opt_set_int(mpa_vidctx, "extern_huff", 1, 0);
        mpa_vidctx->extradata = ffmpeg_extradata;
        mpa_vidctx->extradata_size = ffmpeg_extradatasize;
    }

    QMutexLocker locker(avcodeclock);
    if (avcodec_open2(mpa_vidctx, mpa_vidcodec, NULL) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Couldn't find lavc video codec");
        return false;
    }

    return true;
}

void NuppelDecoder::CloseAVCodecVideo(void)
{
    QMutexLocker locker(avcodeclock);

    if (mpa_vidcodec)
    {
        avcodec_close(mpa_vidctx);

        if (mpa_vidctx)
        {
            av_free(mpa_vidctx);
            mpa_vidctx = NULL;
        }
    }
}

bool NuppelDecoder::InitAVCodecAudio(int codec)
{
    if (mpa_audcodec)
        CloseAVCodecAudio();

    if (usingextradata)
    {
        switch(extradata.audio_fourcc)
        {
            case FOURCC_LAME: codec = AV_CODEC_ID_MP3;        break;
            case FOURCC_AC3 : codec = AV_CODEC_ID_AC3;        break;
            default: codec = -1;
        }
    }
    mpa_audcodec = avcodec_find_decoder((enum CodecID)codec);

    if (!mpa_audcodec)
    {
        if (usingextradata)
            LOG(VB_GENERAL, LOG_ERR, QString("couldn't find audio codec (%1)")
                    .arg(extradata.audio_fourcc));
        else
            LOG(VB_GENERAL, LOG_ERR, "couldn't find audio codec");
        return false;
    }

    if (mpa_audctx)
        av_free(mpa_audctx);

    mpa_audctx = avcodec_alloc_context3(NULL);

    mpa_audctx->codec_id = (enum CodecID)codec;
    mpa_audctx->codec_type = AVMEDIA_TYPE_AUDIO;

    QMutexLocker locker(avcodeclock);
    if (avcodec_open2(mpa_audctx, mpa_audcodec, NULL) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Couldn't find lavc audio codec");
        return false;
    }

    return true;
}

void NuppelDecoder::CloseAVCodecAudio(void)
{
    QMutexLocker locker(avcodeclock);

    if (mpa_audcodec)
    {
        avcodec_close(mpa_audctx);

        if (mpa_audctx)
        {
            av_free(mpa_audctx);
            mpa_audctx = NULL;
        }
    }
}

static void CopyToVideo(unsigned char *buf, int video_width,
                        int video_height, VideoFrame *frame)
{
    uint ysize = video_width * video_height;
    uint uvsize = ysize >> 2;

    unsigned char *planes[3];
    planes[0] = buf;
    planes[1] = planes[0] + ysize;
    planes[2] = planes[1] + uvsize;

    memcpy(frame->buf + frame->offsets[0], planes[0], ysize);
    memcpy(frame->buf + frame->offsets[1], planes[1], uvsize);
    memcpy(frame->buf + frame->offsets[2], planes[2], uvsize);
}

bool NuppelDecoder::DecodeFrame(struct rtframeheader *frameheader,
                                unsigned char *lstrm, VideoFrame *frame)
{
    int r;
    lzo_uint out_len;
    int compoff = 0;
    AVPacket pkt;

    unsigned char *outbuf = frame->buf;
    directframe = frame;

    if (!buf2)
    {
        buf2 = new unsigned char[video_size + 64];
        planes[0] = buf;
        planes[1] = planes[0] + video_width * video_height;
        planes[2] = planes[1] + (video_width * video_height) / 4;
    }

    if (frameheader->comptype == 'N') {
        memset(outbuf, 0, video_width * video_height);
        memset(outbuf + video_width * video_height, 127,
               (video_width * video_height)/2);
        return true;
    }

    if (frameheader->comptype == 'L') {
        switch(lastct) {
            case '0': case '3':
                CopyToVideo(buf2, video_width, video_height, frame);
                break;
            case '1': case '2':
            default:
                CopyToVideo(buf, video_width, video_height, frame);
                break;
        }
        return true;
    }

    compoff = 1;
    if (frameheader->comptype == '2' || frameheader->comptype == '3')
        compoff=0;

    lastct = frameheader->comptype;

    if (!compoff)
    {
        r = lzo1x_decompress(lstrm, frameheader->packetlength, buf2, &out_len,
                              NULL);
        if (r != LZO_E_OK)
        {
            LOG(VB_GENERAL, LOG_ERR, "minilzo: can't decompress illegal data");
        }
    }

    if (frameheader->comptype == '0')
    {
        CopyToVideo(lstrm, video_width, video_height, frame);
        return true;
    }

    if (frameheader->comptype == '3')
    {
        CopyToVideo(buf2, video_width, video_height, frame);
        return true;
    }

    if (frameheader->comptype == '2' || frameheader->comptype == '1')
    {
        if (compoff)
            rtjd->Decompress((int8_t*)lstrm, planes);
        else
            rtjd->Decompress((int8_t*)buf2, planes);

        CopyToVideo(buf, video_width, video_height, frame);
    }
    else
    {
        if (!mpa_vidcodec)
            InitAVCodecVideo(frameheader->comptype - '3');

        AVFrame mpa_pic;
        av_init_packet(&pkt);
        pkt.data = lstrm;
        pkt.size = frameheader->packetlength;

        {
            QMutexLocker locker(avcodeclock);
            // if directrendering, writes into buf
            int gotpicture = 0;
            int ret = avcodec_decode_video2(mpa_vidctx, &mpa_pic, &gotpicture,
                                            &pkt);
            directframe = NULL;
            if (ret < 0)
            {
                LOG(VB_PLAYBACK, LOG_ERR, LOC +
                    QString("avcodec_decode_video returned: %1").arg(ret));
                return false;
            }
            else if (!gotpicture)
            {
                return false;
            }
        }

/* XXX: Broken
        if (mpa_pic->qscale_table != NULL && mpa_pic->qstride > 0)
        {
            int tablesize = mpa_pic->qstride * ((video_height + 15) / 16);

            if (frame->qstride != mpa_pic->qstride ||
                frame->qscale_table == NULL)
            {
                frame->qstride = mpa_pic->qstride;

                if (frame->qscale_table)
                    delete [] frame->qscale_table;

                frame->qscale_table = new unsigned char[tablesize];
            }

            memcpy(frame->qscale_table, mpa_pic->qscale_table, tablesize);
        }
*/

        if (directrendering)
            return true;

        avpicture_fill(&tmppicture, outbuf, PIX_FMT_YUV420P, video_width,
                       video_height);

        myth_sws_img_convert(
            &tmppicture, PIX_FMT_YUV420P, (AVPicture *)&mpa_pic,
                    mpa_vidctx->pix_fmt, video_width, video_height);
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
        strmcpy = new unsigned char[frameheader.packetlength];
        memcpy(strmcpy, newstrm, frameheader.packetlength);
    }
    else
        strmcpy = NULL;

    StoredData.push_back(new RawDataList(frameheader, strmcpy));
}

// The return value is the number of bytes in StoredData before the 'SV' frame
long NuppelDecoder::UpdateStoredFrameNum(long framenum)
{
    long sync_offset = 0;

    list<RawDataList*>::iterator it = StoredData.begin();
    for ( ; it != StoredData.end(); ++it)
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
    RawDataList *data;
    while (!StoredData.empty())
    {
        data = StoredData.front();

        if (data->frameheader.frametype != 'S')
            data->frameheader.timecode -= timecodeOffset;

        if (storevid || data->frameheader.frametype != 'V')
        {
            rb->Write(&(data->frameheader), FRAMEHEADERSIZE);
            if (data->packet)
                rb->Write(data->packet, data->frameheader.packetlength);
        }
        StoredData.pop_front();
        delete data;
    }
}

void NuppelDecoder::ClearStoredData()
{
    RawDataList *data;
    while (!StoredData.empty())
    {
        data = StoredData.front();
        StoredData.pop_front();
        delete data;
    }
}

bool NuppelDecoder::GetFrame(DecodeType decodetype)
{
    bool gotvideo = false;
    bool ret = false;
    int seeklen = 0;
    AVPacket pkt;

    decoded_video_frame = NULL;

    while (!gotvideo)
    {
        long long currentposition = ringBuffer->GetReadPosition();
        if (waitingForChange && currentposition + 4 >= readAdjust)
        {
            FileChanged();
            currentposition = ringBuffer->GetReadPosition();
        }

        if (!ReadFrameheader(&frameheader))
        {
            SetEof(true);
            return false;
        }


        if (!ringBuffer->LiveMode() &&
            ((frameheader.frametype == 'Q') || (frameheader.frametype == 'K')))
        {
            SetEof(true);
            return false;
        }

        bool framesearch = false;

        while (!isValidFrametype(frameheader.frametype))
        {
            if (!framesearch)
                LOG(VB_GENERAL, LOG_INFO, "Searching for frame header.");

            framesearch = true;

            ringBuffer->Seek((long long)seeklen-FRAMEHEADERSIZE, SEEK_CUR);

            if (!ReadFrameheader(&frameheader))
            {
                SetEof(true);
                return false;
            }
            seeklen = 1;
        }

        if (frameheader.frametype == 'M')
        {
            int sizetoskip = sizeof(rtfileheader) - sizeof(rtframeheader);
            char *dummy = new char[sizetoskip + 1];

            if (ringBuffer->Read(dummy, sizetoskip) != sizetoskip)
            {
                delete [] dummy;
                SetEof(true);
                return false;
            }

            delete [] dummy;
            continue;
        }

        if (frameheader.frametype == 'R')
        {
            if (getrawframes)
                StoreRawData(NULL);
            continue; // the R-frame has no data packet
        }

        if (frameheader.frametype == 'S')
        {
            if (frameheader.comptype == 'A')
            {
                if (frameheader.timecode > 2000000 &&
                    frameheader.timecode < 5500000)
                {
                    effdsp = frameheader.timecode;
                    m_audio->SetEffDsp(effdsp);
                }
            }
            else if (frameheader.comptype == 'V')
            {
                lastKey = frameheader.timecode;
                framesPlayed = (frameheader.timecode > 0 ?
                                frameheader.timecode - 1 : 0);

                if (!hasFullPositionMap)
                {
                    long long last_index = 0;
                    long long this_index = lastKey / keyframedist;

                    QMutexLocker locker(&m_positionMapLock);
                    if (!m_positionMap.empty())
                        last_index = m_positionMap.back().index;

                    if (this_index > last_index)
                    {
                        PosMapEntry e = {this_index, lastKey, currentposition};
                        m_positionMap.push_back(e);
                        m_frameToDurMap[lastKey] =
                            lastKey * 1000 / video_frame_rate;
                        m_durToFrameMap[m_frameToDurMap[lastKey]] = lastKey;
                    }
                }
            }
            if (getrawframes)
                StoreRawData(NULL);
        }

        if (frameheader.packetlength > 0)
        {
            if (frameheader.packetlength > 10485760) // arbitrary 10MB limit
            {
                LOG(VB_GENERAL, LOG_ERR, QString("Broken packet: %1 %2")
                        .arg(frameheader.frametype)
                        .arg(frameheader.packetlength));
                SetEof(true);
                return false;
            }
            if (ringBuffer->Read(strm, frameheader.packetlength) !=
                frameheader.packetlength)
            {
                SetEof(true);
                return false;
            }
        }
        else
            continue;

        if (frameheader.frametype == 'V')
        {
            if (!(kDecodeVideo & decodetype))
            {
                framesPlayed++;
                gotvideo = 1;
                continue;
            }

            VideoFrame *buf = GetPlayer()->GetNextVideoFrame();
            if (!buf)
                continue;

            ret = DecodeFrame(&frameheader, strm, buf);
            if (!ret)
            {
                GetPlayer()->DiscardVideoFrame(buf);
                continue;
            }

            buf->aspect = current_aspect;
            buf->frameNumber = framesPlayed;
            buf->dummy = 0;
            GetPlayer()->ReleaseNextVideoFrame(buf, frameheader.timecode);

            // We need to make the frame available ourselves
            // if we are not using ffmpeg/avlib.
            if (directframe)
                GetPlayer()->DeLimboFrame(buf);

            decoded_video_frame = buf;
            gotvideo = 1;
            if (getrawframes && getrawvideo)
                StoreRawData(strm);
            framesPlayed++;

            if (!setreadahead)
            {
                videosizetotal += frameheader.packetlength;
                videoframesread++;

                if (videoframesread > 15)
                {
                    videosizetotal /= videoframesread;

                    float bps = (videosizetotal * 8.0f / 1024.0f *
                                 video_frame_rate);
                    bitrate = (uint) (bps * 1.5f);

                    ringBuffer->UpdateRawBitrate(GetRawBitrate());
                    setreadahead = true;
                }
            }
            continue;
        }

        if (frameheader.frametype=='A' && (kDecodeAudio & decodetype))
        {
            if ((frameheader.comptype == '3') || (frameheader.comptype == 'A'))
            {
                if (getrawframes)
                    StoreRawData(strm);

                if (!mpa_audcodec)
                {
                    if (frameheader.comptype == '3')
                        InitAVCodecAudio(AV_CODEC_ID_MP3);
                    else if (frameheader.comptype == 'A')
                        InitAVCodecAudio(AV_CODEC_ID_AC3);
                    else
                    {
                        LOG(VB_GENERAL, LOG_ERR, LOC + QString("GetFrame: "
                                "Unknown audio comptype of '%1', skipping")
                                .arg(frameheader.comptype));
                        return false;
                    }
                }

                av_init_packet(&pkt);
                pkt.data = strm;
                pkt.size = frameheader.packetlength;
                int ret = 0;

                QMutexLocker locker(avcodeclock);

                while (pkt.size > 0)
                {
                    int data_size = 0;

                    ret = m_audio->DecodeAudio(mpa_audctx, m_audioSamples,
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
                                          frameheader.timecode, 0);
                }
            }
            else
            {
                getrawframes = 0;
#if HAVE_BIGENDIAN
                // Why endian correct the audio buffer here?
                // Don't big-endian clients have to do it in audiooutBlah.cpp?
                if (audio_bits_per_sample == 16) {
                    // swap bytes
                    for (int i = 0; i < (frameheader.packetlength & ~1); i+=2) {
                        char tmp;
                        tmp = strm[i+1];
                        strm[i+1] = strm[i];
                        strm[i] = tmp;
                    }
                }
#endif
                LOG(VB_PLAYBACK, LOG_DEBUG, QString("A audio timecode %1")
                                              .arg(frameheader.timecode));
                m_audio->AddAudioData((char *)strm, frameheader.packetlength,
                                      frameheader.timecode, 0);
            }
        }

        if (frameheader.frametype == 'T' && (kDecodeVideo & decodetype))
        {
            if (getrawframes)
                StoreRawData(strm);

            GetPlayer()->GetCC608Reader()->AddTextData(strm, frameheader.packetlength,
                                  frameheader.timecode, frameheader.comptype);
        }

        if (frameheader.frametype == 'S' && frameheader.comptype == 'M')
        {
            unsigned char *eop = strm + frameheader.packetlength;
            unsigned char *cur = strm;

            struct rtfileheader tmphead;
            struct rtfileheader *fh = &tmphead;

            memcpy(fh, cur, min((int)sizeof(*fh), frameheader.packetlength));

            while (QString(fh->finfo) != "MythTVVideo" &&
                   cur + frameheader.packetlength <= eop)
            {
                cur++;
                memcpy(fh, cur, min((int)sizeof(*fh), frameheader.packetlength));
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

                fileheader = *fh;

                if (fileheader.aspect > .999 && fileheader.aspect < 1.001)
                    fileheader.aspect = 4.0 / 3;
                current_aspect = fileheader.aspect;

                GetPlayer()->SetKeyframeDistance(fileheader.keyframedist);
                GetPlayer()->SetVideoParams(fileheader.width, fileheader.height,
                                            fileheader.fps);
            }
        }
    }

    framesRead = framesPlayed;

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

    DecoderBase::SeekReset(newKey, skipFrames, doFlush, discardFrames);

    if (mpa_vidcodec && doFlush)
        avcodec_flush_buffers(mpa_vidctx);

    if (discardFrames)
        GetPlayer()->DiscardVideoFrames(doFlush);

    for (;(skipFrames > 0) && !ateof; skipFrames--)
    {
        GetFrame(kDecodeAV);
        if (decoded_video_frame)
            GetPlayer()->DiscardVideoFrame(decoded_video_frame);
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
