#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

#include <iostream>
#include <pthread.h>
using namespace std;

#include "nuppeldecoder.h"
#include "NuppelVideoPlayer.h"
#include "remoteencoder.h"
#include "mythcontext.h"

#include "minilzo.h"

extern pthread_mutex_t avcodeclock;

NuppelDecoder::NuppelDecoder(NuppelVideoPlayer *parent, QSqlDatabase *db,
                             ProgramInfo *pginfo)
             : DecoderBase(parent)
{
    m_db = db;
    m_playbackinfo = pginfo;

    ffmpeg_extradata = NULL;
    ffmpeg_extradatasize = 0;

    usingextradata = false;
    memset(&extradata, 0, sizeof(extendeddata));

    hasFullPositionMap = false;
    positionMap = new QMap<long long, long long>;

    hasKeyFrameAdjustMap = false;
    keyFrameAdjustMap = new QMap<long long, int>;

    totalLength = 0;
    totalFrames = 0;

    gf = lame_init();
    lame_set_decode_only(gf, 1);
    lame_decode_init();
    lame_init_params(gf);

    int i;
    rtjd = new RTjpeg();
    i = RTJ_YUV420;
    rtjd->SetFormat(&i);

    if (lzo_init() != LZO_E_OK)
    {
        cerr << "lzo_init() failed, exiting\n";
    }

    avcodec_init();
    avcodec_register_all();

    mpa_codec = 0;
    mpa_ctx = NULL;
    mpa_pic = NULL;
    directrendering = false;
    directbuf = NULL;

    buf = NULL;
    buf2 = NULL;
    strm = NULL;

    lastct = '1';

    audio_samplerate = 44100;

    lastKey = 0;
    framesPlayed = 0;

    getrawframes = false;
    getrawvideo = false;
}

NuppelDecoder::~NuppelDecoder()
{
    if (gf)
        lame_close(gf);
    if (rtjd)
        delete rtjd;
    if (ffmpeg_extradata)
        delete [] ffmpeg_extradata;
    if (positionMap)
        delete positionMap;
    if (keyFrameAdjustMap)
        delete keyFrameAdjustMap;
    if (buf)
        delete [] buf;
    if (buf2)
        delete [] buf2;
    if (strm)
        delete [] strm;

    CloseAVCodec();
}

bool NuppelDecoder::CanHandle(char testbuf[2048])
{
    if (!strncmp(testbuf, "NuppelVideo", 11) ||
        !strncmp(testbuf, "MythTVVideo", 11))
        return true;
    return false;
}

QString NuppelDecoder::GetEncodingType(void)
{
    QString value = "Unknown";
    if (mpa_codec)
    {
        if (QString(mpa_codec->name) == "mpeg4")
            value = "MPEG-4";
    }
    else if (usingextradata &&
             extradata.video_fourcc == MKTAG('D', 'I', 'V', 'X'))
        value = "MPEG-4";
    else
        value = "RTjpeg";
    return (value);
}

int NuppelDecoder::OpenFile(RingBuffer *rbuffer, bool novideo, 
                            char testbuf[2048])
{
    (void)testbuf;

    ringBuffer = rbuffer;
    disablevideo = novideo;

    struct rtframeheader frameheader;
    long long startpos = 0;
    int foundit = 0;
    char ftype;
    char *space;

    if (ringBuffer->Read(&fileheader, FILEHEADERSIZE) != FILEHEADERSIZE)
    {
        cerr << "Error reading file: " << ringBuffer->GetFilename() << endl;
        return -1;
    }

    while ((QString(fileheader.finfo) != "NuppelVideo") &&
           (QString(fileheader.finfo) != "MythTVVideo"))
    {
        ringBuffer->Seek(startpos, SEEK_SET);
        char dummychar;
        ringBuffer->Read(&dummychar, 1);

        startpos = ringBuffer->GetTotalReadPosition();

        if (ringBuffer->Read(&fileheader, FILEHEADERSIZE) != FILEHEADERSIZE)
        {
            cerr << "Error reading file: " << ringBuffer->GetFilename() << endl;
            return -1;
        }

        if (startpos > 20000)
        {
            cerr << "Bad file: " << ringBuffer->GetFilename().ascii() << endl;
            return -1;
        }
    }

    m_parent->SetVideoParams(fileheader.width, fileheader.height,
                             fileheader.fps, fileheader.keyframedist);

    video_width = fileheader.width;
    video_height = fileheader.height;
    video_size = video_height * video_width * 3 / 2;
    keyframedist = fileheader.keyframedist;
    video_frame_rate = fileheader.fps;

    if (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader, FRAMEHEADERSIZE))
    {
        cerr << "File not big enough for a header\n";
        return -1;
    }
    if (frameheader.frametype != 'D')
    {
        cerr << "Illegal file format\n";
        return -1;
    }

    space = new char[video_size];

    if (frameheader.comptype == 'F')
    {
        ffmpeg_extradatasize = frameheader.packetlength;
        if (ffmpeg_extradatasize > 0)
        {
            ffmpeg_extradata = new char[ffmpeg_extradatasize];
            if (frameheader.packetlength != ringBuffer->Read(ffmpeg_extradata,
                                                     frameheader.packetlength))
            {
                cerr << "File not big enough for first frame data\n";
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
            cerr << "File not big enough for first frame data\n";
            delete [] space;
            return -1;
        }
    }

    if ((video_height & 1) == 1)
    {
        video_height--;
        cerr << "Incompatible video height, reducing to " << video_height
             << endl;
    }

    startpos = ringBuffer->GetTotalReadPosition();

    ringBuffer->Read(&frameheader, FRAMEHEADERSIZE);

    if (frameheader.frametype == 'X')
    {
        if (frameheader.packetlength != EXTENDEDSIZE)
        {
            cerr << "Corrupt file.  Bad extended frame.\n";
        }
        else
        {
            ringBuffer->Read(&extradata, frameheader.packetlength);
            usingextradata = true;
            ringBuffer->Read(&frameheader, FRAMEHEADERSIZE);
        }
    }

    if (usingextradata && extradata.seektable_offset > 0)
    {
        long long currentpos = ringBuffer->GetTotalReadPosition();
        struct rtframeheader seek_frameheader;

        int seekret = ringBuffer->Seek(extradata.seektable_offset, SEEK_SET);
        if (seekret == -1) {
            perror("seek");
        }

        ringBuffer->Read(&seek_frameheader, FRAMEHEADERSIZE);

        if (seek_frameheader.frametype != 'Q')
        {
            cerr << "Invalid seektable (frametype "
                 << (int)seek_frameheader.frametype << ")\n";
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

                for (int z = 0; z < numentries; z++)
                {
                    memcpy(&ste, seekbuf + offset,
                           sizeof(struct seektable_entry));
                    offset += sizeof(struct seektable_entry);

                    (*positionMap)[ste.keyframe_number] = ste.file_offset;
                }
                hasFullPositionMap = true;
                totalLength = (int)((ste.keyframe_number * keyframedist * 1.0) /
                                     video_frame_rate);
                totalFrames = (long long)ste.keyframe_number * keyframedist;
                m_parent->SetFileLength(totalLength, totalFrames);

                delete [] seekbuf;
            }
            else
                cerr << "0 length seek table\n";
        }

        ringBuffer->Seek(currentpos, SEEK_SET);
    }

    if (usingextradata && extradata.keyframeadjust_offset > 0 &&
        hasFullPositionMap && !disablevideo)
    {
        long long currentpos = ringBuffer->GetTotalReadPosition();
        struct rtframeheader kfa_frameheader;

        int kfa_ret = ringBuffer->Seek(extradata.keyframeadjust_offset, 
                                       SEEK_SET);
        if (kfa_ret == -1) {
            perror("keyframeadjust");
        }

        ringBuffer->Read(&kfa_frameheader, FRAMEHEADERSIZE);

        if (kfa_frameheader.frametype != 'K')
        {
            cerr << "Invalid key frame adjust table (frametype "
                 << (int)kfa_frameheader.frametype << ")\n";
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

                for (int z = 0; z < numentries; z++)
                {
                    memcpy(&kfate, kfa_buf + offset,
                           sizeof(struct kfatable_entry));
                    offset += sizeof(struct kfatable_entry);

                    (*keyFrameAdjustMap)[kfate.keyframe_number] = kfate.adjust;
                    adjust += kfate.adjust;
                }
                hasKeyFrameAdjustMap = true;

                totalLength -= (int)(adjust / video_frame_rate);
                totalFrames -= adjust;
                m_parent->SetFileLength(totalLength, totalFrames);

                delete [] kfa_buf;
            }
            else
                cerr << "0 length key frame adjust table\n";
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

        startpos = ringBuffer->GetTotalReadPosition();

        if (ringBuffer->Read(&frameheader, FRAMEHEADERSIZE) != FRAMEHEADERSIZE)
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
    m_parent->SetEffDsp(effdsp);

    if (usingextradata)
    {
        effdsp = extradata.audio_sample_rate * 100;
        m_parent->SetEffDsp(effdsp);
        audio_samplerate = extradata.audio_sample_rate;
        m_parent->SetAudioParams(extradata.audio_bits_per_sample,
                                 extradata.audio_channels, 
                                 extradata.audio_sample_rate);
        foundit = 1;
    }

    while (!foundit)
    {
        ftype = ' ';
        if (frameheader.frametype == 'S')
        {
            if (frameheader.comptype == 'A')
            {
                effdsp = frameheader.timecode;
                if (effdsp > 0)
                {
                    m_parent->SetEffDsp(effdsp);
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

        long long startpos2 = ringBuffer->GetTotalReadPosition();

        foundit = (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader,
                                                       FRAMEHEADERSIZE));

        bool framesearch = false;

        while (frameheader.frametype != 'A' && frameheader.frametype != 'V' &&
               frameheader.frametype != 'S' && frameheader.frametype != 'T' &&
               frameheader.frametype != 'R' && frameheader.frametype != 'X')
        {
            if (!framesearch)
                VERBOSE(VB_IMPORTANT, "Searching for frame header.");

            framesearch = true;

            ringBuffer->Seek(startpos2, SEEK_SET);

            char dummychar;
            ringBuffer->Read(&dummychar, 1);

            startpos2 = ringBuffer->GetTotalReadPosition();

            foundit = (FRAMEHEADERSIZE != ringBuffer->Read(&frameheader,
                                                           FRAMEHEADERSIZE));
            if (foundit)
                break;
        }
    }

    delete [] space;

    setreadahead = false;

    // mpeg4 encodes are small enough that this shouldn't matter
    if (usingextradata && extradata.video_fourcc == MKTAG('D', 'I', 'V', 'X'))
        setreadahead = true;

    ringBuffer->CalcReadAheadThresh(0);

    videosizetotal = 0;
    videoframesread = 0;

    ringBuffer->Seek(startpos, SEEK_SET);

    buf = new unsigned char[video_size];
    strm = new unsigned char[video_size * 2];

    if (hasFullPositionMap)
        return 1;

    if (m_playbackinfo && m_db && positionMap)
    {
        m_playbackinfo->GetPositionMap(*positionMap, MARK_KEYFRAME, m_db);
        if (positionMap->size() && !livetv && !watchingrecording)
        {
            QMap<long long, long long>::Iterator it;
            long long max_keyframe = -1;
            for (it = positionMap->begin(); it != positionMap->end(); ++it)
                if (it.key() > max_keyframe)
                    max_keyframe = it.key();

            if (max_keyframe >= 0)
            {
                totalLength = (int)((max_keyframe * keyframedist * 1.0) /
                                     video_frame_rate);
                totalFrames = (long long)max_keyframe * keyframedist;
                m_parent->SetFileLength(totalLength, totalFrames);
            }
            hasFullPositionMap = true;
            return 1;
        }
    }

    return 0;
}

void NuppelDecoder::Reset(void)
{
    delete positionMap;
    positionMap = new QMap<long long, long long>;

    if (mpa_codec)
        avcodec_flush_buffers(mpa_ctx);

    framesPlayed = 0;
}

int get_nuppel_buffer(struct AVCodecContext *c, AVFrame *pic)
{
    NuppelDecoder *nd = (NuppelDecoder *)(c->opaque);

    int width = c->width;
    int height = c->height;

    pic->data[0] = nd->directbuf;
    pic->data[1] = pic->data[0] + width * height;
    pic->data[2] = pic->data[1] + width * height / 4;

    pic->linesize[0] = width;
    pic->linesize[1] = width / 2;
    pic->linesize[2] = width / 2;

    pic->opaque = nd;
    pic->type = FF_BUFFER_TYPE_USER;

    pic->age = 256 * 256 * 256 * 64;

    return 1;
}

void release_nuppel_buffer(struct AVCodecContext *c, AVFrame *pic)
{
    (void)c;
    assert(pic->type == FF_BUFFER_TYPE_USER);

    int i;
    for (i = 0; i < 4; i++)
        pic->data[i] = NULL;
}

bool NuppelDecoder::InitAVCodec(int codec)
{
    if (mpa_codec)
        CloseAVCodec();

    if (usingextradata)
    {
        switch(extradata.video_fourcc)
        {
            case MKTAG('D', 'I', 'V', 'X'): codec = CODEC_ID_MPEG4; break;
            case MKTAG('W', 'M', 'V', '1'): codec = CODEC_ID_WMV1; break;
            case MKTAG('D', 'I', 'V', '3'): codec = CODEC_ID_MSMPEG4V3; break;
            case MKTAG('M', 'P', '4', '2'): codec = CODEC_ID_MSMPEG4V2; break;
            case MKTAG('M', 'P', 'G', '4'): codec = CODEC_ID_MSMPEG4V1; break;
            case MKTAG('M', 'J', 'P', 'G'): codec = CODEC_ID_MJPEG; break;
            case MKTAG('H', '2', '6', '3'): codec = CODEC_ID_H263; break;
            case MKTAG('I', '2', '6', '3'): codec = CODEC_ID_H263I; break;
            case MKTAG('M', 'P', 'E', 'G'): codec = CODEC_ID_MPEG1VIDEO; break;
            case MKTAG('H', 'F', 'Y', 'U'): codec = CODEC_ID_HUFFYUV; break;
            default: codec = -1;
        }
    }
    mpa_codec = avcodec_find_decoder((enum CodecID)codec);

    if (!mpa_codec)
    {
        cerr << "couldn't find codec " << codec;
        if (usingextradata)
            cerr << " (" << extradata.video_fourcc << ")";
        cerr << endl;
        return false;
    }

    if (mpa_codec->capabilities & CODEC_CAP_DR1 && codec != CODEC_ID_MJPEG)
        directrendering = true;

    if (mpa_ctx)
        free(mpa_ctx);

    if (mpa_pic)
        free(mpa_pic);

    mpa_pic = avcodec_alloc_frame();

    mpa_ctx = avcodec_alloc_context();

    mpa_ctx->codec_id = (enum CodecID)codec;
    mpa_ctx->width = video_width;
    mpa_ctx->height = video_height;
    mpa_ctx->error_resilience = 2;
    mpa_ctx->bits_per_sample = 12;

    if (directrendering)
    {
        mpa_ctx->flags |= CODEC_FLAG_EMU_EDGE;
        mpa_ctx->draw_horiz_band = NULL;
        mpa_ctx->get_buffer = get_nuppel_buffer;
        mpa_ctx->release_buffer = release_nuppel_buffer;
        mpa_ctx->opaque = (void *)this;
    }
    if (ffmpeg_extradatasize > 0)
    {
        mpa_ctx->flags |= CODEC_FLAG_EXTERN_HUFF;
        mpa_ctx->extradata = ffmpeg_extradata;
        mpa_ctx->extradata_size = ffmpeg_extradatasize;
    }

    if (avcodec_open(mpa_ctx, mpa_codec) < 0)
    {
        cerr << "Couldn't find lavc codec\n";
        return false;
    }

    return true;
}

void NuppelDecoder::CloseAVCodec(void)
{
    if (!mpa_codec)
        return;

    avcodec_close(mpa_ctx);

    if (mpa_ctx)
        free(mpa_ctx);
    if (mpa_pic)
        free(mpa_pic);
    mpa_ctx = NULL;
    mpa_pic = NULL;
}

bool NuppelDecoder::DecodeFrame(struct rtframeheader *frameheader,
                                unsigned char *lstrm, VideoFrame *frame)
{
    int r;
    unsigned int out_len;
    int compoff = 0;

    unsigned char *outbuf = frame->buf;

    if (!buf2)
    {
        buf2 = new unsigned char[video_size];
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
                memcpy(outbuf, buf2, video_size);
                break;
            case '1': case '2':
            default:
                memcpy(outbuf, buf, video_size);
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
            cerr << "minilzo: can't decompress illegal data\n";
        }
    }

    if (frameheader->comptype == '0')
    {
        memcpy(outbuf, lstrm, video_size);
        return true;
    }

    if (frameheader->comptype == '3')
    {
        memcpy(outbuf, buf2, video_size);
        return true;
    }

    if (frameheader->comptype == '2' || frameheader->comptype == '1')
    {
        if (compoff)
            rtjd->Decompress((int8_t*)lstrm, planes);
        else
            rtjd->Decompress((int8_t*)buf2, planes);

        memcpy(outbuf, buf, video_size);
    }
    else
    {
        if (!mpa_codec)
            InitAVCodec(frameheader->comptype - '3');

        int gotpicture = 0;
        pthread_mutex_lock(&avcodeclock);
        // if directrendering, writes into buf
        directbuf = outbuf;
        int ret = avcodec_decode_video(mpa_ctx, mpa_pic, &gotpicture,
                                       lstrm, frameheader->packetlength);
        pthread_mutex_unlock(&avcodeclock);
        if (ret < 0)
        {
            VERBOSE(VB_PLAYBACK, QString("decoding error: %1 back from avcodec")
                                 .arg(ret));
            return false;
        }

        if (!gotpicture)
        {
            return false;
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
        for (int i = 0; i < 4; i++)
        {
            mpa_pic_tmp.data[i] = mpa_pic->data[i];
            mpa_pic_tmp.linesize[i] = mpa_pic->linesize[i];
        }

        img_convert(&tmppicture, PIX_FMT_YUV420P, &mpa_pic_tmp,
                    mpa_ctx->pix_fmt, video_width, video_height);
    }

    return true;
}

bool NuppelDecoder::isValidFrametype(char type)
{
    switch (type)
    {
        case 'A': case 'V': case 'S': case 'T': case 'R': case 'X':
        case 'M': case 'D':
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

    StoredData.append(new RawDataList(frameheader, strmcpy));
}

void NuppelDecoder::UpdateFrameNumber(long framenum)
{
    for (RawDataList *data = StoredData.first(); data; data = StoredData.next())
    {
        if (data->frameheader.frametype == 'S' &&
            data->frameheader.comptype == 'V')
        {
            data->frameheader.timecode = framenum;
        }
    }
}

void NuppelDecoder::WriteStoredData(RingBuffer *rb, bool storevid)
{
    RawDataList *data;
    while(! StoredData.isEmpty()) {
        data = StoredData.first();
        if (storevid || data->frameheader.frametype != 'V')
        {
            rb->Write(&(data->frameheader), FRAMEHEADERSIZE);
            if (data->packet)
                rb->Write(data->packet, data->frameheader.packetlength);
        }
        StoredData.removeFirst();
        delete data;
    }
}

// avignore = 0  : get audio and video
//          = 1  : video only
//          = -1 : neither, just parse

void NuppelDecoder::GetFrame(int avignore)
{
    bool gotvideo = false;
    bool ret = false;
    int seeklen = 0;

    while (!gotvideo)
    {
        long long currentposition = ringBuffer->GetTotalReadPosition();

        if ((ringBuffer->Read(&frameheader, FRAMEHEADERSIZE) != FRAMEHEADERSIZE)
            || (frameheader.frametype == 'Q') || (frameheader.frametype == 'K'))
        {
            m_parent->SetEof();
            return;
        }

        bool framesearch = false;

        while (!isValidFrametype(frameheader.frametype))
        {
            if (!framesearch)
                VERBOSE(VB_IMPORTANT, "Searching for frame header.");

            framesearch = true;

            ringBuffer->Seek((long long)seeklen-FRAMEHEADERSIZE, SEEK_CUR);

            if (ringBuffer->Read(&frameheader, FRAMEHEADERSIZE)
                != FRAMEHEADERSIZE)
            {
                m_parent->SetEof();
                return;
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
                m_parent->SetEof();
                return;
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
                if (frameheader.timecode > 0 && frameheader.timecode < 5500000)
                {
                    effdsp = frameheader.timecode;
                    m_parent->SetEffDsp(effdsp);
                }
            }
            else if (frameheader.comptype == 'V')
            {
                lastKey = frameheader.timecode;
                framesPlayed = frameheader.timecode - 1;
                if (!hasFullPositionMap)
                    (*positionMap)[lastKey / keyframedist] = currentposition;
            }
            if (getrawframes)
                StoreRawData(NULL);
        }

        if (frameheader.packetlength > 0)
        {
            if (frameheader.packetlength > 10485760) // arbitrary 10MB limit
            {
                cerr << "Broken packet: " << frameheader.frametype
                     << " " << frameheader.packetlength << endl;
                m_parent->SetEof();
                return;
            }
            if (ringBuffer->Read(strm, frameheader.packetlength) !=
                frameheader.packetlength)
            {
                m_parent->SetEof();
                return;
            }
        }
        else
            continue;

        if (frameheader.frametype == 'V')
        {
            if (avignore == -1)
            {
                framesPlayed++;
                gotvideo = 1;
                continue;
            }

            VideoFrame *buf = m_parent->GetNextVideoFrame();

            ret = DecodeFrame(&frameheader, strm, buf);
            if (!ret)
            {
                m_parent->DiscardVideoFrame(buf);
                continue;
            }

            m_parent->ReleaseNextVideoFrame(buf, frameheader.timecode);
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

                    float bps = videosizetotal * 8.0 / 1024 * video_frame_rate;
                    bps = bps * 3 / 2;

                    ringBuffer->CalcReadAheadThresh((int)bps);
                    setreadahead = true;
                }
            }
            continue;
        }

        if (frameheader.frametype=='A' && avignore == 0)
        {
            if (frameheader.comptype=='3')
            {
                if (getrawframes)
                    StoreRawData(strm);

                int lameret = 0;
                short int pcmlbuffer[audio_samplerate * 4];
                short int pcmrbuffer[audio_samplerate * 4];
                int packetlen = frameheader.packetlength;

                do
                {
                    lameret = lame_decode(strm, packetlen, pcmlbuffer,
                                          pcmrbuffer);

                    if (lameret > 0)
                    {
                        m_parent->AddAudioData(pcmlbuffer, pcmrbuffer,
                                               lameret, frameheader.timecode);
                    }
                    else if (lameret < 0)
                    {
                        cerr << "lame decode error: " << lameret 
                             << ", exiting\n";
                        exit(-1);
                    }
                    packetlen = 0;
                } while (lameret > 0);
            }
            else
            {
                getrawframes = 0;
                m_parent->AddAudioData((char *)strm, frameheader.packetlength, 
                                       frameheader.timecode);
            }
        }

        if (frameheader.frametype == 'T' && avignore >= 0)
        {
            if (getrawframes)
                StoreRawData(strm);

            m_parent->AddTextData((char *)strm, frameheader.packetlength,
                                  frameheader.timecode, frameheader.comptype);
        }
    }

    m_parent->SetFramesPlayed(framesPlayed);
}

int NuppelDecoder::GetKeyIndex(int keyFrame)
{
    if (hasKeyFrameAdjustMap)
    {
        int index = 0;
        int curFrame = 0;
        QMap <long long, int>::Iterator keyiter = NULL;
        keyiter = (*keyFrameAdjustMap).begin();
        while (curFrame < keyFrame)
        {
            curFrame += keyframedist;
            index++;
            if (keyiter.key() == curFrame)
            {
                 curFrame -= keyiter.data();
                 if (keyiter != (*keyFrameAdjustMap).end())
                     keyiter++;
            }
        }
        return (index);
    }
    else
       return (lastKey / keyframedist);
}

bool NuppelDecoder::DoRewind(long long desiredFrame)
{
    long long storelastKey = lastKey;
    int keyIndex;

    if (hasKeyFrameAdjustMap)
    {
        keyIndex = GetKeyIndex(lastKey);
        QMap <long long, int>::Iterator keyiter = NULL;
        keyiter = (*keyFrameAdjustMap).find(lastKey);
        if (keyiter != (*keyFrameAdjustMap).end())
            lastKey += keyiter.data();

        while (lastKey > desiredFrame)
        {
            lastKey -= keyframedist;
            keyIndex--;
            keyiter = (*keyFrameAdjustMap).find(lastKey);
            if (keyiter != (*keyFrameAdjustMap).end())
                lastKey += keyiter.data();
        }
    }
    else
    {
        while (lastKey > desiredFrame)
        {
            lastKey -= keyframedist;
        }
        keyIndex = lastKey / keyframedist;
    }

    if (lastKey < 1)
        lastKey = 1;

    int normalframes = desiredFrame - lastKey;
    long long keyPos = (*positionMap)[keyIndex];
    long long curPosition = ringBuffer->GetTotalReadPosition();
    long long diff = keyPos - curPosition;

    while (ringBuffer->GetFreeSpaceWithReadChange(diff) < 0)
    {
        lastKey += keyframedist;
        keyIndex++;
        keyPos = (*positionMap)[keyIndex];
        if (keyPos == 0)
            continue;
        diff = keyPos - curPosition;
        normalframes = 0;
        if (lastKey > storelastKey)
        {
            lastKey = storelastKey;
            diff = 0;
            normalframes = 0;
            return false;
        }
    }

    int iter = 0;

    while (keyPos == 0 && lastKey > 1 && iter < 4)
    {
        VERBOSE(VB_PLAYBACK, QString("No keyframe in position map for %1")
                                     .arg((int)lastKey));

        lastKey -= keyframedist;
        keyIndex--;
        keyPos = (*positionMap)[keyIndex];
        diff = keyPos - curPosition;
        normalframes += keyframedist;

        if (keyPos != 0)
           VERBOSE(VB_PLAYBACK, QString("Using seek position: %1")
                                       .arg((int)lastKey));

        iter++;
    }

    if (keyPos == 0)
    {
        VERBOSE(VB_GENERAL, QString("Unknown seek position: %1")
                                    .arg((int)lastKey));

        lastKey = storelastKey;
        diff = 0;
        normalframes = 0;

        return false;
    }

    ringBuffer->Seek(diff, SEEK_CUR);
    framesPlayed = lastKey - 1;

    int fileend = 0;

    normalframes = desiredFrame - framesPlayed;

    if (!exactseeks)
        normalframes = 0;

    if (mpa_codec)
        avcodec_flush_buffers(mpa_ctx);

    // disbale frame storage
    int oldrawstate = getrawframes;
    getrawframes = 0;

    while (normalframes > 0 && !fileend)
    {
        if (normalframes != 1)
            GetFrame(1);
        else
        {
            getrawframes = oldrawstate;
            GetFrame(0);
        }

        fileend = m_parent->GetEof();
        normalframes--;
    }

    getrawframes = oldrawstate;

    return true;
}

bool NuppelDecoder::DoFastForward(long long desiredFrame)
{
    // disable frame storage
    int oldrawstate = getrawframes;
    getrawframes = 0;

    long long number = desiredFrame - framesPlayed;
    long long desiredKey = lastKey;
    int lastKeyIndex = GetKeyIndex(lastKey);
    int desiredIndex = lastKeyIndex;

    if (hasKeyFrameAdjustMap)
    {
        QMap <long long, int>::Iterator keyiter = NULL;
        keyiter = (*keyFrameAdjustMap).begin();
        while (keyiter.key() < lastKey && keyiter != (*keyFrameAdjustMap).end())
            keyiter++;
        long long prevKey = desiredKey;
        while (desiredKey <= desiredFrame)
        {
            prevKey = desiredKey;
            desiredKey += keyframedist;
            desiredIndex++;
            if (desiredKey == keyiter.key())
            {
                desiredKey -= keyiter.data();
                keyiter++;
            }
        }
        desiredKey = prevKey;
        desiredIndex--;
    }
    else
    {
        while (desiredKey <= desiredFrame)
            desiredKey += keyframedist;
        desiredKey -= keyframedist;
        desiredIndex = desiredKey / keyframedist;
    }

    int normalframes = desiredFrame - desiredKey;
    int fileend = 0;

    if (desiredKey == lastKey)
        normalframes = number;

    long long keyPos = -1;

    long long tmpKey = desiredKey;
    int tmpIndex = desiredIndex;

    while (keyPos == -1 && tmpKey > lastKey)
    {
        if (positionMap->find(tmpIndex) != positionMap->end())
        {
            keyPos = (*positionMap)[tmpIndex];
        }
        else if (livetv || (watchingrecording && nvr_enc &&
                            nvr_enc->IsValidRecorder()))
        {
            for (int i = lastKeyIndex; i <= tmpIndex; i++)
            {
                if (positionMap->find(i) == positionMap->end())
                    nvr_enc->FillPositionMap(i, tmpIndex, *positionMap);
            }
            keyPos = (*positionMap)[tmpIndex];
        }

        if (keyPos == -1)
        {
            VERBOSE(VB_PLAYBACK, QString("No keyframe in position map for %1")
                    .arg((int)tmpKey));

            tmpKey -= keyframedist;
            tmpIndex--;
        }
        else
        {
            desiredKey = tmpKey;
            desiredIndex = tmpIndex;
        }
    }

    bool needflush = false;

    if (keyPos != -1)
    {
        lastKey = desiredKey;
        long long diff = keyPos - ringBuffer->GetTotalReadPosition();

        ringBuffer->Seek(diff, SEEK_CUR);
        framesPlayed = lastKey - 1;
        needflush = true;
    }
    else if (!livetv && !watchingrecording)
    {
        if (lastKey < desiredKey)
        {
            VERBOSE(VB_IMPORTANT, "Did not find keyframe position.");

            VERBOSE(VB_PLAYBACK, QString("lastKey: %1 desiredKey: %2")
                    .arg((int)lastKey).arg((int)desiredKey));
        }

        while (lastKey < desiredKey && !fileend)
        {
            GetFrame(-1);
            needflush = true;
            fileend = m_parent->GetEof();
        }

        if (needflush)
        {
            lastKey = desiredKey;
            keyPos = (*positionMap)[desiredKey / keyframedist];
            long long diff = keyPos - ringBuffer->GetTotalReadPosition();

            ringBuffer->Seek(diff, SEEK_CUR);

            framesPlayed = lastKey - 1;
        }
    }

    normalframes = desiredFrame - framesPlayed;

    if (!exactseeks)
        normalframes = 0;

    if (mpa_codec && needflush)
        avcodec_flush_buffers(mpa_ctx);

    while (normalframes > 0 && !fileend)
    {
        if (normalframes != 1)
            GetFrame(1);
        else
        {
            getrawframes = oldrawstate;
            GetFrame(false);
        }
        fileend = m_parent->GetEof();
        normalframes--;
    }

    getrawframes = oldrawstate;

    return true;
}

void NuppelDecoder::SetPositionMap(void)
{
    if (m_playbackinfo && m_db && positionMap)
        m_playbackinfo->SetPositionMap(*positionMap, MARK_KEYFRAME, m_db);
}

