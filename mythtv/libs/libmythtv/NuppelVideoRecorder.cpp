#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

#include <qstringlist.h>

#include <iostream>
using namespace std;

#include "NuppelVideoRecorder.h"

#ifndef MJPIOC_S_PARAMS
#include "videodev_mjpeg.h"
#endif

#define KEYFRAMEDIST   30

pthread_mutex_t avcodeclock = PTHREAD_MUTEX_INITIALIZER;

NuppelVideoRecorder::NuppelVideoRecorder(void)
{
    sfilename = "output.nuv";
    encoding = false;
    fd = 0;
    lf = tf = 0;
    M1 = 0, M2 = 0, Q = 255;
    pid = pid2 = 0;
    inputchannel = 1;
    compression = 1;
    compressaudio = 1;
    ntsc = 1;
    rawmode = 0;
    usebttv = 1;
    w = 352;
    h = 240;

    mp3quality = 3;
    gf = NULL;
    rtjc = NULL;
    strm = NULL;   
    mp3buf = NULL;

    act_video_encode = 0;
    act_video_buffer = 0;
    act_audio_encode = 0;
    act_audio_buffer = 0;
    video_buffer_count = 0;
    audio_buffer_count = 0;
    video_buffer_size = 0;
    audio_buffer_size = 0;

    audiodevice = "/dev/dsp";
    videodevice = "/dev/video";

    childrenLive = false;

    recording = false;

    ringBuffer = NULL;
    weMadeBuffer = false;
    paused = false;
    pausewritethread = false;   
 
    keyframedist = KEYFRAMEDIST;

    audiobytes = 0;
    audio_bits = 16;
    audio_channels = 2;
    audio_samplerate = 44100;
    audio_bytes_per_sample = audio_channels * audio_bits / 8;

    picture_format = PIX_FMT_YUV420P;

    avcodec_init();
    avcodec_register_all();

    mpa_codec = 0;
    mpa_ctx = NULL;

    codec = "rtjpeg";
    useavcodec = false;

    targetbitrate = 2200;
    scalebitrate = 1;
    maxquality = 2;
    minquality = 31;
    qualdiff = 3;

    oldtc = 0;
    startnum = 0;
    frameofgop = 0;
    lasttimecode = 0;
    audio_behind = 0;

    extendeddataOffset = 0;
    seektable = new vector<struct seektable_entry>;

    pip = false;
    hardware_encode = false;
    hmjpg_quality = 80;
    hmjpg_decimation = 2;
	
    videoFilterList = "";
}

NuppelVideoRecorder::~NuppelVideoRecorder(void)
{
    if (weMadeBuffer)
        delete ringBuffer;
    if (rtjc)
        delete rtjc;
    if (mp3buf)
        delete [] mp3buf;
    if (gf)
        lame_close(gf);  
    if (strm)
        delete [] strm;
    if (fd > 0)
        close(fd);
    if (seektable)
    {
        seektable->clear();
        delete seektable;  
    }  

    while (videobuffer.size() > 0)
    {
        struct vidbuffertype *vb = videobuffer.back();
        delete [] vb->buffer;
        delete vb;
        videobuffer.pop_back();
    }
    while (audiobuffer.size() > 0)
    {
        struct audbuffertype *ab = audiobuffer.back();
        delete [] ab->buffer;
        delete ab;
        audiobuffer.pop_back();
    }

    if (mpa_codec)
        avcodec_close(mpa_ctx);

    if (mpa_ctx)
        free(mpa_ctx);
    mpa_ctx = NULL;

    if (videoFilters.size() > 0)
    {
        filters_cleanup(&videoFilters[0], videoFilters.size());
        videoFilters.clear();
    }
}

void NuppelVideoRecorder::SetTVFormat(QString tvformat)
{
    if (tvformat.lower() == "ntsc" || tvformat.lower() == "ntsc-jp")
        ntsc = 1;
    else 
        ntsc = 0;
}

bool NuppelVideoRecorder::SetupAVCodec(void)
{
    if (mpa_codec)
        avcodec_close(mpa_ctx);
    
    if (mpa_ctx)
        free(mpa_ctx);
    mpa_ctx = NULL;

    mpa_codec = avcodec_find_encoder_by_name(codec.ascii());

    if (!mpa_codec)
    {
        cout << "error finding codec\n";
        return false;
    }

    mpa_ctx = avcodec_alloc_context();
   
    if (picture_format == PIX_FMT_YUV420P)
    { 
        mpa_ctx->pix_fmt = PIX_FMT_YUV420P;
    
        mpa_picture.linesize[0] = w;
        mpa_picture.linesize[1] = w / 2;
        mpa_picture.linesize[2] = w / 2;
    }
    else if (picture_format == PIX_FMT_YUV422P)
    {
        mpa_ctx->pix_fmt = PIX_FMT_YUV422P;

        mpa_picture.linesize[0] = w;
        mpa_picture.linesize[1] = w / 2;
        mpa_picture.linesize[2] = w / 2;
    }   
    else
        cerr << "Unknown picture format: " << picture_format << endl;

    mpa_ctx->width = w;
    mpa_ctx->height = h;

    int usebitrate = targetbitrate * 1000;
    if (scalebitrate)
    {
        float diff = (w * h) / (640.0 * 480.0);
        usebitrate = (int)(diff * usebitrate);
    }

    if (targetbitrate == -1)
        usebitrate = -1;

    mpa_ctx->frame_rate = (int)(video_frame_rate * FRAME_RATE_BASE);
    mpa_ctx->bit_rate = usebitrate;
    mpa_ctx->bit_rate_tolerance = usebitrate * 100;
    mpa_ctx->qmin = maxquality;
    mpa_ctx->qmax = minquality;
    mpa_ctx->max_qdiff = qualdiff;

    mpa_ctx->qblur = 0.5;
    mpa_ctx->max_b_frames = 0;
    mpa_ctx->b_quant_factor = 0;
    mpa_ctx->rc_strategy = 2;
    mpa_ctx->b_frame_strategy = 0;
    mpa_ctx->gop_size = 30;
    mpa_ctx->key_frame = -1;
    mpa_ctx->rc_max_rate = 0;
    mpa_ctx->rc_min_rate = 0;
    mpa_ctx->rc_buffer_size = 0;
    mpa_ctx->rc_buffer_aggressivity = 1.0;
    mpa_ctx->rc_override_count = 0;
    mpa_ctx->rc_initial_cplx = 0;
    mpa_ctx->dct_algo = FF_DCT_AUTO;
    mpa_ctx->idct_algo = FF_IDCT_AUTO;
    mpa_ctx->prediction_method = FF_PRED_LEFT;
    if (codec.lower() == "huffyuv")
        mpa_ctx->strict_std_compliance = -1;
 
    if (avcodec_open(mpa_ctx, mpa_codec) < 0)
    {
        cerr << "Unable to open FFMPEG/" <<  codec << " codec\n" << endl;
        return false;
    }

    return true;
}

void NuppelVideoRecorder::Initialize(void)
{
    int videomegs;
    int audiomegs = 2;

    if (AudioInit() != 0)   
    {
        cerr << "Could not detect audio blocksize\n";
    }
 
    if (compressaudio)
    {
        gf = lame_init();
	lame_set_bWriteVbrTag(gf, 0);
	lame_set_quality(gf, mp3quality);
	lame_set_compression_ratio(gf, 11);
        lame_set_mode(gf, audio_channels == 2 ? STEREO : MONO);
        lame_set_num_channels(gf, audio_channels);
        lame_set_out_samplerate(gf, audio_samplerate);
        lame_set_in_samplerate(gf, audio_samplerate);
	lame_init_params(gf);

        if (audio_bits != 16) 
        {
            cerr << "lame support requires 16bit audio\n";
            compressaudio = false;
        }
    }

    if (codec == "hardware-mjpeg")
    {
        codec = "mjpeg";
        hardware_encode = true;
        if (ntsc)
        {
            switch (hmjpg_decimation)
            {
                case 2: w = 352; h = 240; break;
                case 4: w = 176; h = 120; break;
                default: w = 720; h = 480; break;
            }
        }
        else
        {
            switch (hmjpg_decimation)
            {
                case 2: w = 352; h = 288; break;
                case 4: w = 176; h = 144; break;
                default: w = 720; h = 576; break;
            }
        }
    }

    if (picture_format == PIX_FMT_YUV422P)
        video_buffer_size = w * h * 2;
    else
        video_buffer_size = w * h * 3 / 2;

    if (w >= 480 || h > 288) 
        videomegs = 20;
    else
        videomegs = 12;

    video_buffer_count = (videomegs * 1000 * 1000) / video_buffer_size;

    if (audio_buffer_size != 0)
        audio_buffer_count = (audiomegs * 1000 * 1000) / audio_buffer_size;
    else
        audio_buffer_count = 0;

    mp3buf_size = (int)(1.25 * 8192 + 7200);
    mp3buf = new char[mp3buf_size];

    if (!ringBuffer)
    {
        cerr << "Warning: Old ringbuf creation\n";
        ringBuffer = new RingBuffer(NULL, sfilename, true);
	weMadeBuffer = true;
	livetv = false;
    }
    else
        livetv = ringBuffer->LiveMode();

    audiobytes = 0;

    InitBuffers();
    InitFilters();
}

int NuppelVideoRecorder::AudioInit(void)
{
    int afmt, afd;
    int frag, blocksize = 4096;

    if (-1 == (afd = open(audiodevice.ascii(), O_RDONLY)))
    {
        cerr << "Cannot open DSP '" << audiodevice << "', dying.\n";
        return 1;
    }
  
    //ioctl(afd, SNDCTL_DSP_RESET, 0);
   
    frag = (8 << 16) | (10); //8 buffers, 1024 bytes each
    ioctl(afd, SNDCTL_DSP_SETFRAGMENT, &frag);

    afmt = AFMT_S16_LE;
    ioctl(afd, SNDCTL_DSP_SETFMT, &afmt);
    if (afmt != AFMT_S16_LE) 
    {
        cerr << "Can't get 16 bit DSP, exiting\n";
        return 1;
    }

    if (ioctl(afd, SNDCTL_DSP_SAMPLESIZE, &audio_bits) < 0 ||
        ioctl(afd, SNDCTL_DSP_CHANNELS, &audio_channels) < 0 ||
        ioctl(afd, SNDCTL_DSP_SPEED, &audio_samplerate) < 0)
    {
        cerr << "recorder: " << audiodevice 
             << ": error setting audio input device to "
             << audio_samplerate << "kHz/" 
             << audio_bits << "bits/"
             << audio_channels << "channel\n";
        return 1;
    }

    audio_bytes_per_sample = audio_channels * audio_bits / 8;

    if (-1 == ioctl(afd, SNDCTL_DSP_GETBLKSIZE, &blocksize)) 
    {
        cerr << "Can't get DSP blocksize, exiting\n";
        return(1);
    }
    blocksize *= 4;

    close(afd);
  
    audio_buffer_size = blocksize;

    return(0); // everything is ok
}

void NuppelVideoRecorder::InitFilters(void)
{
    QStringList filters = QStringList::split(",", videoFilterList);
    for (QStringList::Iterator i = filters.begin(); i != filters.end(); i++)
    {
        VideoFilter *filter = load_videoFilter((char *)((*i).ascii()), 
                                               NULL);
        if (filter != NULL)
            videoFilters.push_back(filter);
    }
}

void NuppelVideoRecorder::InitBuffers(void)
{
    for (int i = 0; i < video_buffer_count; i++)
    {
        vidbuffertype *vidbuf = new vidbuffertype;
        vidbuf->buffer = new unsigned char[video_buffer_size];
        vidbuf->sample = 0;
        vidbuf->freeToEncode = 0;
        vidbuf->freeToBuffer = 1;
        vidbuf->bufferlen = 0;
       
        videobuffer.push_back(vidbuf);
    }

    for (int i = 0; i < audio_buffer_count; i++)
    {
        audbuffertype *audbuf = new audbuffertype;
        audbuf->buffer = new unsigned char[audio_buffer_size];
        audbuf->sample = 0;
        audbuf->freeToEncode = 0;
        audbuf->freeToBuffer = 1;
      
        audiobuffer.push_back(audbuf);
    }
}

void NuppelVideoRecorder::StartRecording(void)
{
    if (lzo_init() != LZO_E_OK)
    {
        cerr << "lzo_init() failed, exiting\n";
        return;
    }

    strm = new signed char[w * h * 2 + 10];

    if (ntsc)
        video_frame_rate = 29.97;
    else
        video_frame_rate = 25.0;

    if (codec.lower() == "rtjpeg")
        useavcodec = false;
    else
        useavcodec = true;

    if (useavcodec)
        useavcodec = SetupAVCodec();

    if (!useavcodec)
    {
        picture_format = PIX_FMT_YUV420P;

        int setval;
        rtjc = new RTjpeg();
        setval = RTJ_YUV420;
        rtjc->SetFormat(&setval);
        rtjc->SetSize(&w, &h);
        rtjc->SetQuality(&Q);
        setval = 2;
        rtjc->SetIntra(&setval, &M1, &M2);
    }

    if (CreateNuppelFile() != 0)
    {
        cerr << "Cannot open '" << ringBuffer->GetFilename() << "' for "
             << "writing, exiting\n";
        return;
    }

    if (childrenLive)
    {
        cerr << "Error: children are already alive\n";
        return;
    }

    if (SpawnChildren() < 0)
    {
        cerr << "Couldn't spawn children\n";
        return;
    }

    // save the start time
    gettimeofday(&stm, &tzone);

    if (getuid() == 0)
        nice(-10);

    fd = open(videodevice.ascii(), O_RDWR);
    if (fd <= 0)
    {
        cerr << "Can't open video device: " << videodevice << endl;
        perror("open video:");
        KillChildren();
        return;
    }

#ifdef V4L2
    struct v4l2_capability vcap;
    struct v4l2_format     vfmt;
    struct v4l2_buffer     vbuf;
    struct v4l2_requestbuffers vrbuf;

    memset(&vcap, 0, sizeof(vcap));
    memset(&vfmt, 0, sizeof(vfmt));
    memset(&vbuf, 0, sizeof(vbuf));
    memset(&vrbuf, 0, sizeof(vrbuf));

    if (ioctl(fd, VIDIOC_QUERYCAP, &vcap) < 0)
    {
        perror("videoc_querycap:");
        return;
    }

    if (vcap.type != V4L2_TYPE_CAPTURE)
    {
        printf("Not a capture device\n");
        exit(0);
    }

    if (!(vcap.flags & V4L2_FLAG_STREAMING) ||
        !(vcap.flags & V4L2_FLAG_SELECT))
    {
        printf("Won't work with the streaming interface, failing\n");
        exit(0);
    }

    vfmt.type = V4L2_BUF_TYPE_CAPTURE;
    vfmt.fmt.pix.width = w;
    vfmt.fmt.pix.height = h;
    vfmt.fmt.pix.depth = 12;
    vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    vfmt.fmt.pix.flags = V4L2_FMT_FLAG_INTERLACED;

    if (ioctl(fd, VIDIOC_S_FMT, &vfmt) < 0)
    {
        printf("Unable to set desired format\n");
        exit(0);
    }

    int numbuffers = 2;
    
    vrbuf.type = V4L2_BUF_TYPE_CAPTURE;
    vrbuf.count = numbuffers;

    if (ioctl(fd, VIDIOC_REQBUFS, &vrbuf) < 0)
    {
        printf("Not able to get any capture buffers\n");
        exit(0);
    }

    unsigned char *buffers[numbuffers];
    int bufferlen[numbuffers];

    for (int i = 0; i < numbuffers; i++)
    {
        vbuf.type = V4L2_BUF_TYPE_CAPTURE;
        vbuf.index = i;

        if (ioctl(fd, VIDIOC_QUERYBUF, &vbuf) < 0)
        {
            printf("unable to query capture buffer %d\n", i);
            exit(0);
        }

        buffers[i] = (unsigned char *)mmap(NULL, vbuf.length,
                                           PROT_READ|PROT_WRITE, MAP_SHARED,
                                           fd, vbuf.offset);
        bufferlen[i] = vbuf.length;
    }

    for (int i = 0; i < numbuffers; i++)
    {
	memset(buffers[i], 0, bufferlen[i]);
        vbuf.type = V4L2_BUF_TYPE_CAPTURE;
        vbuf.index = i;
        ioctl(fd, VIDIOC_QBUF, &vbuf);
    }

    int turnon = V4L2_BUF_TYPE_CAPTURE;
    ioctl(fd, VIDIOC_STREAMON, &turnon);

    struct timeval tv;
    fd_set rdset;
    int frame = 0;

    encoding = true;
    recording = true;

    struct timeval startt, nowt;
    while (encoding) {
        if (paused)
        {
            mainpaused = true;
            usleep(50);
            gettimeofday(&stm, &tzone);
            continue;
        }
again:
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        FD_ZERO(&rdset);
        FD_SET(fd, &rdset);

        switch (select(fd+1, &rdset, NULL, NULL, &tv))
        {
            case -1:
                  if (errno == EINTR)
                      goto again;
                  perror("select");
                  continue;
            case 0:
                  printf("select timeout\n");
                  continue;
           default: break;
        }

        memset(&vbuf, 0, sizeof(vbuf));
        vbuf.type = V4L2_BUF_TYPE_CAPTURE;
        ioctl(fd, VIDIOC_DQBUF, &vbuf);

        frame = vbuf.index;
        BufferIt(buffers[frame], video_buffer_size);
	
        vbuf.type = V4L2_BUF_TYPE_CAPTURE;
        ioctl(fd, VIDIOC_QBUF, &vbuf);
    }

    ioctl(fd, VIDIOC_STREAMOFF, &turnon);

    munmap(buffers[0], bufferlen[frame]);
    munmap(buffers[1], bufferlen[frame]);

#else
    
    struct video_capability vc;
    struct video_mmap mm;
    struct video_mbuf vm;
    struct video_channel vchan;
    struct video_audio va;
    struct video_tuner vt;

    memset(&mm, 0, sizeof(mm));
    memset(&vm, 0, sizeof(vm));
    memset(&vchan, 0, sizeof(vchan));
    memset(&va, 0, sizeof(va));
    memset(&vt, 0, sizeof(vt));
    memset(&vc, 0, sizeof(vc));

    if (ioctl(fd, VIDIOCGCAP, &vc) < 0)
    {
        perror("VIDIOCGCAP:");
        KillChildren(); 
        return;
    }

    if ((vc.type & VID_TYPE_MJPEG_ENCODER) && hardware_encode)
    {
        DoMJPEG();
        return;
    }

    if (ioctl(fd, VIDIOCGMBUF, &vm) < 0)
    {
        perror("VIDOCGMBUF:");
        KillChildren();
        return;
    }

    if (vm.frames < 2)
    {
        fprintf(stderr, "need a minimum of 2 capture buffers\n");
        KillChildren();
        return;
    }

    int frame;

    unsigned char *buf = (unsigned char *)mmap(0, vm.size, 
                                               PROT_READ|PROT_WRITE, 
                                               MAP_SHARED, 
                                               fd, 0);
    if (buf <= 0)
    {
        perror("mmap");
        KillChildren();
        return;
    }

    int channel = 0;
    int volume = -1;

    if (ioctl(fd, VIDIOCGCHAN, &vchan) < 0) 
        perror("VIDIOCGCHAN");

    // if channel has a audio then activate it
    if ((vchan.flags & VIDEO_VC_AUDIO)==VIDEO_VC_AUDIO) {
        //if (!quiet) 
        //    fprintf(stderr, "%s\n", "unmuting tv-audio");
        // audio hack, to enable audio from tvcard, in case we use a tuner
        va.audio = 0; // use audio channel 0
        if (ioctl(fd, VIDIOCGAUDIO, &va)<0) 
            perror("VIDIOCGAUDIO"); 
        origaudio = va;
        //if (!quiet) 
        //    fprintf(stderr, "audio volume was '%d'\n", va.volume);
        va.audio = 0;
        va.flags &= ~VIDEO_AUDIO_MUTE; // now this really has to work

        if ((volume==-1 && va.volume<32768) || volume!=-1) 
        {
            if (volume==-1) 
                va.volume = 32768;            // no more silence 8-)
            else 
                va.volume = volume;
            if (!quiet) 
                fprintf(stderr, "audio volume set to '%d'\n", va.volume);
        }
        if (ioctl(fd, VIDIOCSAUDIO, &va)<0) 
            perror("VIDIOCSAUDIO");
    } 
    else 
    {
        if (!quiet) 
            fprintf(stderr, "channel '%d' has no tuner (composite)\n", channel);
    }

    mm.height = h;
    mm.width  = w;
    if (picture_format == PIX_FMT_YUV422P)
        mm.format = VIDEO_PALETTE_YUV422P;
    else
        mm.format = VIDEO_PALETTE_YUV420P;  

    mm.frame  = 0;
    if (ioctl(fd, VIDIOCMCAPTURE, &mm)<0) 
        perror("VIDIOCMCAPTUREi0");
    mm.frame  = 1;
    if (ioctl(fd, VIDIOCMCAPTURE, &mm)<0) 
        perror("VIDIOCMCAPTUREi1");
    
    encoding = true;
    recording = true;

    while (encoding) {
        if (paused)
        {
           mainpaused = true;
           usleep(50);
           gettimeofday(&stm, &tzone);
           continue;
        }
        frame = 0;
        mm.frame = 0;
        if (ioctl(fd, VIDIOCSYNC, &frame)<0) 
            perror("VIDIOCSYNC0");
        else 
        {
            BufferIt(buf+vm.offsets[0], video_buffer_size);
            //memset(buf+vm.offsets[0], 0, video_buffer_size);
            if (ioctl(fd, VIDIOCMCAPTURE, &mm)<0) 
                perror("VIDIOCMCAPTURE0");
        }
        frame = 1;
        mm.frame = 1;
        if (ioctl(fd, VIDIOCSYNC, &frame)<0) 
            perror("VIDIOCSYNC1");
        else 
        {
            BufferIt(buf+vm.offsets[1], video_buffer_size);
            //memset(buf+vm.offsets[1], 0, video_buffer_size);
            if (ioctl(fd, VIDIOCMCAPTURE, &mm)<0) 
                perror("VIDIOCMCAPTURE1");
        }
    }

    munmap(buf, vm.size);
#endif
    KillChildren();

    if (!livetv)
        WriteSeekTable(false);

    recording = false;
    close(fd);
}

void NuppelVideoRecorder::DoMJPEG(void)
{
    struct mjpeg_params bparm;

    if (ioctl(fd, MJPIOC_G_PARAMS, &bparm) < 0)
    {
        perror("MJPIOC_G_PARAMS:");
        return;
    }

    //bparm.input = 2;
    //bparm.norm = 1;
    bparm.quality = hmjpg_quality;
    bparm.decimation = hmjpg_decimation;

    for (int n = 0; n < 14; n++)
         bparm.APP_data[n] = 0;

    if (ioctl(fd, MJPIOC_S_PARAMS, &bparm) < 0)
    {
        perror("MJPIOC_S_PARAMS:");
        return;
    }

    struct mjpeg_requestbuffers breq;

    breq.count = 64;
    breq.size = 256 * 1024;

    if (ioctl(fd, MJPIOC_REQBUFS, &breq) < 0)
    {
        perror("MJPIOC_REQBUFS:");
        return;
    }

    uint8_t *MJPG_buff = (uint8_t *)mmap(0, breq.count * breq.size, 
                                         PROT_READ|PROT_WRITE, MAP_SHARED, fd, 
                                         0);

    if (MJPG_buff == MAP_FAILED)
    {
        cerr << "error mapping mjpeg buffers\n";
        return;
    } 
  
    struct mjpeg_sync bsync;
 
    for (unsigned int count = 0; count < breq.count; count++)
    {
        if (ioctl(fd, MJPIOC_QBUF_CAPT, &count) < 0)
            perror("MJPIOC_QBUF_CAPT:");
    }

    encoding = true;
    recording = true;

    while (encoding)
    {
        if (paused)
        {
           mainpaused = true;
           usleep(50);
           gettimeofday(&stm, &tzone);
           continue;
        }
        if (ioctl(fd, MJPIOC_SYNC, &bsync) < 0)
            encoding = false;

        BufferIt((unsigned char *)(MJPG_buff + bsync.frame * breq.size),
                 bsync.length);

        if (ioctl(fd, MJPIOC_QBUF_CAPT, &(bsync.frame)) < 0)
            encoding = false;
    }

    munmap(MJPG_buff, breq.count * breq.size);
    KillChildren();
	        
    if (!livetv)
        WriteSeekTable(false);
		    
    recording = false;
    close(fd);
}

void NuppelVideoRecorder::TransitionToFile(const QString &lfilename)
{
    pausewritethread = true;
    while (!actuallypaused)
	usleep(50);

    ringBuffer->TransitionToFile(lfilename);
    WriteHeader(true);

    pausewritethread = false;
}

void NuppelVideoRecorder::TransitionToRing(void)
{
    pausewritethread = true;
    while (!actuallypaused)
        usleep(50);

    WriteSeekTable(true);
    ringBuffer->TransitionToRing();

    pausewritethread = false;
}

int NuppelVideoRecorder::SpawnChildren(void)
{
    int result;

    childrenLive = true;
    
    result = pthread_create(&write_tid, NULL, 
		            NuppelVideoRecorder::WriteThread, this);

    if (result)
    {
        cerr << "Couldn't spawn writer thread, exiting\n";
	return -1;
    }

    result = pthread_create(&audio_tid, NULL,
                            NuppelVideoRecorder::AudioThread, this);

    if (result)
    {
        cerr << "Couldn't spawn audio thread, exiting\n";
	return -1;
    }

    return 0;
}

void NuppelVideoRecorder::KillChildren(void)
{
    childrenLive = false;
   
#ifndef V4L2
    if (ioctl(fd, VIDIOCSAUDIO, &origaudio) < 0)
        perror("VIDIOCSAUDIO");
#endif

    pthread_join(write_tid, NULL);
    pthread_join(audio_tid, NULL);
}

void NuppelVideoRecorder::BufferIt(unsigned char *buf, int len)
{
    int act;
    long tcres;
    int fn;
    struct timeval now;

    act = act_video_buffer;
 
    if (!videobuffer[act]->freeToBuffer) {
        return;
    }

    gettimeofday(&now, &tzone);
   
    tcres = (now.tv_sec-stm.tv_sec)*1000 + now.tv_usec/1000 - stm.tv_usec/1000;

    usebttv = 0;
    // here is the non preferable timecode - drop algorithm - fallback
    if (!usebttv) 
    {
        if (tf==0)
            tf = 2;
        else 
        {
            fn = tcres - oldtc;

     // the difference should be less than 1,5*timeperframe or we have 
     // missed at least one frame, this code might be inaccurate!
     
            if (ntsc)
                fn = (fn+16)/33;
            else 
                fn = (fn+20)/40;
            if (fn<1)
                fn=1;
            tf += 2*fn; // two fields
        }
    }

    oldtc = tcres;

    if (!videobuffer[act]->freeToBuffer) 
    {
	printf("DROPPED frame due to full buffer in the recorder.\n");
        return; // we can't buffer the current frame
    }
	
    videobuffer[act]->sample = tf;

    // record the time at the start of this frame.
    // 'tcres' is at the end of the frame, so subtract the right # of ms
    videobuffer[act]->timecode = (ntsc) ? (tcres - 33) : (tcres - 40);

    memcpy(videobuffer[act]->buffer, buf, len);
    videobuffer[act]->bufferlen = len;

    videobuffer[act]->freeToBuffer = 0;
    act_video_buffer++;
    if (act_video_buffer >= video_buffer_count) 
        act_video_buffer = 0; // cycle to begin of buffer
    videobuffer[act]->freeToEncode = 1; // set last to prevent race
    return;
}

void NuppelVideoRecorder::WriteHeader(bool todumpfile)
{
    struct rtfileheader fileheader;
    struct rtframeheader frameheader;
    static unsigned long int tbls[128];
    static const char finfo[12] = "MythTVVideo";
    static const char vers[5]   = "0.07";
    
    memset(&fileheader, 0, sizeof(fileheader));
    memcpy(fileheader.finfo, finfo, sizeof(fileheader.finfo));
    memcpy(fileheader.version, vers, sizeof(fileheader.version));
    fileheader.width  = w;
    fileheader.height = h;
    fileheader.desiredwidth  = 0;
    fileheader.desiredheight = 0;
    fileheader.pimode = 'P';
    fileheader.aspect = 1.0;
    if (ntsc)
        fileheader.fps = 29.97;
    else
        fileheader.fps = 25.0;
    video_frame_rate = fileheader.fps;
    fileheader.videoblocks = -1;
    fileheader.audioblocks = -1;
    fileheader.textsblocks = 0;
    fileheader.keyframedist = KEYFRAMEDIST;

    if (todumpfile)
        ringBuffer->WriteToDumpFile(&fileheader, FILEHEADERSIZE);
    else
        ringBuffer->Write(&fileheader, FILEHEADERSIZE);

    memset(&frameheader, 0, sizeof(frameheader));
    frameheader.frametype = 'D'; // compressor data

    if (useavcodec)
    {
        frameheader.comptype = 'F';
        frameheader.packetlength = mpa_ctx->extradata_size;

        if (todumpfile)
            ringBuffer->WriteToDumpFile(&frameheader, FRAMEHEADERSIZE);
        else
            ringBuffer->Write(&frameheader, FRAMEHEADERSIZE);

        if (todumpfile)
            ringBuffer->WriteToDumpFile(mpa_ctx->extradata, 
                                        frameheader.packetlength);
        else
            ringBuffer->Write(mpa_ctx->extradata, frameheader.packetlength);
    }
    else
    {
        frameheader.comptype = 'R'; // compressor data for RTjpeg
        frameheader.packetlength = sizeof(tbls);

        // compression configuration header
        if (todumpfile)
            ringBuffer->WriteToDumpFile(&frameheader, FRAMEHEADERSIZE);
        else
            ringBuffer->Write(&frameheader, FRAMEHEADERSIZE);

        memset(tbls, 0, sizeof(tbls));
        if (todumpfile)
            ringBuffer->WriteToDumpFile(tbls, sizeof(tbls));
        else
            ringBuffer->Write(tbls, sizeof(tbls));
    }

    memset(&frameheader, 0, sizeof(frameheader));
    frameheader.frametype = 'X'; // extended data
    frameheader.packetlength = sizeof(extendeddata);

    // extended data header
    if (todumpfile)
        ringBuffer->WriteToDumpFile(&frameheader, FRAMEHEADERSIZE);
    else
        ringBuffer->Write(&frameheader, FRAMEHEADERSIZE);
    
    struct extendeddata moredata;
    memset(&moredata, 0, sizeof(extendeddata));
    
    moredata.version = 1;
    if (useavcodec)
    {
        int vidfcc = 0;
        switch(mpa_codec->id)
        {
            case CODEC_ID_MPEG4: vidfcc = MKTAG('D', 'I', 'V', 'X'); break;
            case CODEC_ID_WMV1: vidfcc = MKTAG('W', 'M', 'V', '1'); break;
            case CODEC_ID_MSMPEG4V3: vidfcc = MKTAG('D', 'I', 'V', '3'); break;
            case CODEC_ID_MSMPEG4V2: vidfcc = MKTAG('M', 'P', '4', '2'); break;
            case CODEC_ID_MSMPEG4V1: vidfcc = MKTAG('M', 'P', 'G', '4'); break;
            case CODEC_ID_MJPEG: vidfcc = MKTAG('M', 'J', 'P', 'G'); break;
            case CODEC_ID_H263: vidfcc = MKTAG('H', '2', '6', '3'); break;
            case CODEC_ID_H263P: vidfcc = MKTAG('H', '2', '6', '3'); break;
            case CODEC_ID_H263I: vidfcc = MKTAG('I', '2', '6', '3'); break;
            case CODEC_ID_MPEG1VIDEO: vidfcc = MKTAG('M', 'P', 'E', 'G'); break;
            case CODEC_ID_HUFFYUV: vidfcc = MKTAG('H', 'F', 'Y', 'U'); break;
            default: break;
        }
        moredata.video_fourcc = vidfcc;
        moredata.lavc_bitrate = mpa_ctx->bit_rate;
        moredata.lavc_qmin = mpa_ctx->qmin;
        moredata.lavc_qmax = mpa_ctx->qmax;
        moredata.lavc_maxqdiff = mpa_ctx->max_qdiff;
    }
    else
    {
        moredata.video_fourcc = MKTAG('R', 'J', 'P', 'G');
        moredata.rtjpeg_quality = Q;
        moredata.rtjpeg_luma_filter = M1;
        moredata.rtjpeg_chroma_filter = M2;
    }

    if (compressaudio)
    {
        moredata.audio_fourcc = MKTAG('L', 'A', 'M', 'E');
        moredata.audio_compression_ratio = 11;
        moredata.audio_quality = mp3quality;
    }
    else
    {
        moredata.audio_fourcc = MKTAG('R', 'A', 'W', 'A');
    }

    moredata.audio_sample_rate = audio_samplerate;
    moredata.audio_channels = audio_channels;
    moredata.audio_bits_per_sample = audio_bits;

    extendeddataOffset = ringBuffer->GetFileWritePosition();

    if (todumpfile)
        ringBuffer->WriteToDumpFile(&moredata, sizeof(moredata));
    else
        ringBuffer->Write(&moredata, sizeof(moredata));

    last_block = 0;
    lf = 0; // that resets framenumber so that seeking in the
            // continues parts works too
}

void NuppelVideoRecorder::WriteSeekTable(bool todumpfile)
{
    int numentries = seektable->size();

    struct rtframeheader frameheader;
    memset(&frameheader, 0, sizeof(frameheader));
    frameheader.frametype = 'Q'; // SeekTable
    frameheader.packetlength = sizeof(struct seektable_entry) * numentries;

    long long currentpos = ringBuffer->GetFileWritePosition();

    if (todumpfile)
        ringBuffer->WriteToDumpFile(&frameheader, sizeof(frameheader));
    else
        ringBuffer->Write(&frameheader, sizeof(frameheader));    

    char *seekbuf = new char[frameheader.packetlength];
    int offset = 0;

    vector<struct seektable_entry>::iterator i = seektable->begin();
    for (; i != seektable->end(); i++)
    {
        memcpy(seekbuf + offset, (const void *)&(*i), 
               sizeof(struct seektable_entry));
        offset += sizeof(struct seektable_entry);
    }

    if (todumpfile)
        ringBuffer->WriteToDumpFile(seekbuf, frameheader.packetlength);
    else
        ringBuffer->Write(seekbuf, frameheader.packetlength);

    ringBuffer->WriterSeek(extendeddataOffset + 
                           offsetof(struct extendeddata, seektable_offset),
                           SEEK_SET);

    if (todumpfile)
        ringBuffer->WriteToDumpFile(&currentpos, sizeof(long long));
    else
        ringBuffer->Write(&currentpos, sizeof(long long));
}

int NuppelVideoRecorder::CreateNuppelFile(void)
{
    framesWritten = 0;
    
    if (!ringBuffer)
    {
        cerr << "Error: no ringbuffer, recorder wasn't initialized.\n";
        return -1;
    }

    if (!ringBuffer->IsOpen())
    {
        cerr << "Ringbuffer isn't open\n";
        return -1;
    }

    WriteHeader();

    return 0;
}

void NuppelVideoRecorder::Reset(void)
{
    framesWritten = 0;
    lf = 0;
    last_block = 0;

    for (int i = 0; i < video_buffer_count; i++)
    {
        vidbuffertype *vidbuf = videobuffer[i];
        vidbuf->sample = 0;
	vidbuf->timecode = 0;
        vidbuf->freeToEncode = 0;
        vidbuf->freeToBuffer = 1;
    }

    for (int i = 0; i < audio_buffer_count; i++)
    {
        audbuffertype *audbuf = audiobuffer[i];
        audbuf->sample = 0;
	audbuf->timecode = 0;
        audbuf->freeToEncode = 0;
        audbuf->freeToBuffer = 1;
    }

    act_video_encode = 0;
    act_video_buffer = 0;
    act_audio_encode = 0;
    act_audio_buffer = 0;
    act_audio_sample = 0;

    audiobytes = 0;
    effectivedsp = 0;
}

void *NuppelVideoRecorder::WriteThread(void *param)
{
    NuppelVideoRecorder *nvr = (NuppelVideoRecorder *)param;

    nvr->doWriteThread();

    return NULL;
}

void *NuppelVideoRecorder::AudioThread(void *param)
{
    NuppelVideoRecorder *nvr = (NuppelVideoRecorder *)param;

    nvr->doAudioThread();

    return NULL;
}

void NuppelVideoRecorder::doAudioThread(void)
{
    int afmt = 0, trigger = 0;
    int afd = 0, act = 0, lastread = 0;
    int frag = 0, blocksize = 0;
    unsigned char *buffer;
    audio_buf_info ispace;
    struct timeval anow;

    act_audio_sample = 0;

    if (-1 == (afd = open(audiodevice.ascii(), O_RDONLY))) 
    {
        cerr << "Cannot open DSP '" << audiodevice << "', exiting";
        return;
    }

    //ioctl(afd, SNDCTL_DSP_RESET, 0);

    frag = (8 << 16) | (10); //8 buffers, 1024 bytes each
    ioctl(afd, SNDCTL_DSP_SETFRAGMENT, &frag);

    afmt = AFMT_S16_LE;
    ioctl(afd, SNDCTL_DSP_SETFMT, &afmt);
    if (afmt != AFMT_S16_LE) 
    {
        cerr << "Can't get 16 bit DSP, exiting";
        close(afd);
        return;
    }

    if (ioctl(afd, SNDCTL_DSP_SAMPLESIZE, &audio_bits) < 0 ||
        ioctl(afd, SNDCTL_DSP_CHANNELS, &audio_channels) < 0 ||
        ioctl(afd, SNDCTL_DSP_SPEED, &audio_samplerate) < 0)
    {
        cerr << "recorder: " << audiodevice 
             << ": error setting audio input device to "
             << audio_samplerate << "kHz/" 
             << audio_bits << "bits/"
             << audio_channels << "channel\n";
        close(afd);
        return;
    }

    audio_bytes_per_sample = audio_channels * audio_bits / 8;

    if (-1 == ioctl(afd, SNDCTL_DSP_GETBLKSIZE,  &blocksize)) 
    {
        cerr << "Can't get DSP blocksize, exiting";
        close(afd);
        return;
    }

    blocksize *= 4;  // allways read 4*blocksize

    if (blocksize != audio_buffer_size) 
    {
        cerr << "warning: audio blocksize = '" << blocksize << "'"
             << " audio_buffer_size='" << audio_buffer_size << "'\n";
    }

    buffer = new unsigned char[audio_buffer_size];

    /* trigger record */
    trigger = ~PCM_ENABLE_INPUT;
    ioctl(afd,SNDCTL_DSP_SETTRIGGER,&trigger);

    trigger = PCM_ENABLE_INPUT;
    ioctl(afd,SNDCTL_DSP_SETTRIGGER,&trigger);

    audiopaused = false;
    while (childrenLive) {
	if (paused)
	{
            audiopaused = true;
            usleep(50);
	    act = act_audio_buffer;
            continue;
        }

        if (audio_buffer_size != (lastread = read(afd, buffer,
                                                  audio_buffer_size))) 
        {
            cerr << "only read " << lastread << " from " << audio_buffer_size
                 << " bytes from '" << audiodevice << "'\n";
            perror("read audio");
        }

	/* record the current time */
	/* Don't assume that the sound device's record buffer is empty
	   (like we used to.) Measure to see how much stuff is in there,
	   and correct for it when calculating the timestamp */
        gettimeofday(&anow, &tzone);
	ioctl( afd, SNDCTL_DSP_GETISPACE, &ispace );

        act = act_audio_buffer;

        if (!audiobuffer[act]->freeToBuffer) 
        {
            cerr << "ran out of free AUDIO buffers :-(\n";
            act_audio_sample++;
            continue;
        }

        audiobuffer[act]->sample = act_audio_sample;

	/* calculate timecode. First compute the difference
	   between now and stm (start time) */
        audiobuffer[act]->timecode = 
	    (anow.tv_sec-stm.tv_sec)*1000 + 
	    anow.tv_usec/1000 - stm.tv_usec/1000;
	/* We want the timestamp to point to the start of this
	   audio chunk. So, subtract off the length of the chunk
	   and the length of audio still in the capture buffer. */
	audiobuffer[act]->timecode -= (int)( 
		( ispace.fragments * ispace.fragsize + audio_buffer_size )
		* 1000.0 / ( audio_samplerate * audio_bytes_per_sample )  );

        memcpy(audiobuffer[act]->buffer, buffer, audio_buffer_size);

        audiobuffer[act]->freeToBuffer = 0;
        act_audio_buffer++;
        if (act_audio_buffer >= audio_buffer_count) 
            act_audio_buffer = 0; 
        audiobuffer[act]->freeToEncode = 1; 

        act_audio_sample++; 
    }

    delete [] buffer;
    close(afd);
}

void NuppelVideoRecorder::doWriteThread(void)
{
    int videofirst;

    actuallypaused = false;
    while (childrenLive)
    {
	if (pausewritethread)
	{
            actuallypaused = true;
            usleep(50);
            continue;
	}
	
        if (!videobuffer[act_video_encode]->freeToEncode && 
            (!audio_buffer_count || 
             !audiobuffer[act_audio_encode]->freeToEncode))
        {
            usleep(1000);
            continue;
        }

        if (videobuffer[act_video_encode]->freeToEncode)
        {
            if (audio_buffer_count && 
                audiobuffer[act_audio_encode]->freeToEncode) 
            {
                videofirst = (videobuffer[act_video_encode]->timecode <=
                              audiobuffer[act_audio_encode]->timecode);
            }
            else
                videofirst = 1;
        }
        else
            videofirst = 0;

        if (videofirst)
        {
            if (videobuffer[act_video_encode]->freeToEncode)
            {
                WriteVideo(videobuffer[act_video_encode]->buffer,
                           videobuffer[act_video_encode]->bufferlen,
                           videobuffer[act_video_encode]->sample,
                           videobuffer[act_video_encode]->timecode);
                videobuffer[act_video_encode]->sample = 0;
                videobuffer[act_video_encode]->freeToEncode = 0;
                videobuffer[act_video_encode]->freeToBuffer = 1;
                act_video_encode++;
                if (act_video_encode >= video_buffer_count)
                    act_video_encode = 0;
            }
        }
        else
        {
            if (audio_buffer_count && 
                audiobuffer[act_audio_encode]->freeToEncode)
            {
                WriteAudio(audiobuffer[act_audio_encode]->buffer,
                           audiobuffer[act_audio_encode]->sample,
                           audiobuffer[act_audio_encode]->timecode);
                audiobuffer[act_audio_encode]->sample = 0;
                audiobuffer[act_audio_encode]->freeToEncode = 0;
                audiobuffer[act_audio_encode]->freeToBuffer = 1; 
                act_audio_encode++;
                if (act_audio_encode >= audio_buffer_count) 
                    act_audio_encode = 0; 
            }
        }
    }
}

long long NuppelVideoRecorder::GetKeyframePosition(long long desired)
{
    long long ret = -1;

    vector<struct seektable_entry>::iterator i = seektable->begin();
    for (; i != seektable->end(); i++)
    {
        if ((*i).keyframe_number == desired)
        {
            ret = (*i).file_offset;
            break;
        }        
    }

    return ret;
}

void NuppelVideoRecorder::WriteVideo(unsigned char *buf, int len, int fnum, 
                                     int timecode)
{
    int tmp = 0, r = 0, out_len = OUT_LEN;
    struct rtframeheader frameheader;
    int xaa, freecount = 0, compressthis;
    int raw = 0;
    int timeperframe = 40;
    uint8_t *planes[3];
    Frame frame;

    memset(&frameheader, 0, sizeof(frameheader));

    planes[0] = buf;
    planes[1] = planes[0] + w * h;
    if (picture_format == PIX_FMT_YUV422P)
        planes[2] = planes[1] + (w * h) / 2;
    else
        planes[2] = planes[1] + (w * h) / 4;
    compressthis = compression;

    frame.codec = CODEC_YUV;
    frame.width = w;
    frame.height = h;
    frame.bpp = -1;
    frame.frameNumber = fnum;
    frame.buf = buf;

    if (lf == 0) 
    { // this will be triggered every new file
        lf = fnum;
        startnum = fnum;
	lasttimecode = 0;
	frameofgop = 0;
    }

    // count free buffers -- FIXME this can be done with less CPU time!!
    for (xaa = 0; xaa < video_buffer_count; xaa++) 
    {
        if (videobuffer[xaa]->freeToBuffer) 
            freecount++;
    }

    if ( freecount < (video_buffer_count / 3) ) 
	compressthis = 0; // speed up the encode process
    
    if (freecount < 5 || rawmode)
	raw = 1; // speed up the encode process
    
    if(raw==1 || compressthis==0) {
	if(ringBuffer->IsIOBound())
	{
	    /* need to compress, the disk can't handle any more bandwidth*/
	    raw=0;
	    compressthis=1;
	}
    }

    // see if it's time for a seeker header, sync information and a keyframe
    frameheader.keyframe  = frameofgop;             // no keyframe defaulted

    bool wantkeyframe = false;

    if (((fnum-startnum)>>1) % keyframedist == 0) {
        frameheader.keyframe=0;
        frameofgop=0;
        ringBuffer->Write("RTjjjjjjjjjjjjjjjjjjjjjjjj", FRAMEHEADERSIZE);

        long long position = ringBuffer->GetFileWritePosition();
        struct seektable_entry ste;
        ste.file_offset = position;
        ste.keyframe_number = ((fnum - startnum) >> 1) / keyframedist;

        seektable->push_back(ste);

        frameheader.frametype    = 'S';           // sync frame
        frameheader.comptype     = 'V';           // video sync information
        frameheader.filters      = 0;             // no filters applied
        frameheader.packetlength = 0;             // no data packet
        frameheader.timecode     = (fnum-startnum)>>1;  
        // write video sync info
        ringBuffer->Write(&frameheader, FRAMEHEADERSIZE);
        frameheader.frametype    = 'S';           // sync frame
        frameheader.comptype     = 'A';           // video sync information
        frameheader.filters      = 0;             // no filters applied
        frameheader.packetlength = 0;             // no data packet
        frameheader.timecode     = effectivedsp;  // effective dsp frequency
        // write audio sync info
        ringBuffer->Write(&frameheader, FRAMEHEADERSIZE);

        wantkeyframe = true;
        sync();
    }

    if (videoFilters.size() > 0)
        process_video_filters(&frame, &videoFilters[0], videoFilters.size());

    if (useavcodec)
    {
        mpa_picture.data[0] = planes[0];
        mpa_picture.data[1] = planes[1];
        mpa_picture.data[2] = planes[2];

        if (wantkeyframe)
            mpa_ctx->force_type = I_TYPE;
        else
            mpa_ctx->force_type = 0;

	if (!hardware_encode)
	{
#ifdef EXTRA_LOCKING
            pthread_mutex_lock(&avcodeclock);
#endif
            tmp = avcodec_encode_video(mpa_ctx, (unsigned char *)strm, 
                                       video_buffer_size, &mpa_picture); 
#ifdef EXTRA_LOCKING
            pthread_mutex_unlock(&avcodeclock);
#endif
	}
    }
    else
    {
        if (!raw) 
        {
            if (wantkeyframe)
                rtjc->SetNextKey();
            tmp = rtjc->Compress(strm, planes);
        }
        else 
            tmp = video_buffer_size;

        // here is lzo compression afterwards
        if (compressthis) {
            if (raw) 
                r = lzo1x_1_compress((unsigned char*)buf, video_buffer_size, 
                                     out, (lzo_uint *)&out_len, wrkmem);
            else
                r = lzo1x_1_compress((unsigned char *)strm, tmp, out,
                                     (lzo_uint *)&out_len, wrkmem);
            if (r != LZO_E_OK) 
            {
                cerr << "lzo compression failed\n";
                return;
            }
        }
    }

    dropped = (((fnum-lf)>>1) - 1); // should be += 0 ;-)
    
    if (dropped>0)
    {
//	printf("recorder dropped = '%ld' fnum = '%d' lf = '%d'\n", dropped, fnum, lf);
	if (ntsc)
	    timeperframe = 1000/30;
	else
	    timeperframe = 1000/25;
    }
   
    // if we have lost frames we insert "copied" frames until we have the
    // exact count because of that we should have no problems with audio 
    // sync, as long as we don't loose audio samples :-/
  
    while (0 && dropped > 0) 
    {
        frameheader.timecode = lasttimecode + timeperframe;
	/*
	  if(frameheader.timecode - lasttimecode > 49 ||
	  frameheader.timecode - lasttimecode < 16)
	  printf("Recorder timecode irregularity 2, last = '%d' cur = '%d'\n", lasttimecode, frameheader.timecode); */

        lasttimecode = frameheader.timecode;
        frameheader.keyframe  = frameofgop;             // no keyframe defaulted
        frameheader.packetlength =  0;   // no additional data needed
        frameheader.frametype    = 'V';  // last frame (or nullframe if first)
        frameheader.comptype    = 'L';
        ringBuffer->Write(&frameheader, FRAMEHEADERSIZE);
        // we don't calculate sizes for lost frames for compression computation
        dropped--;
        frameofgop++;
    }

    frameheader.frametype = 'V'; // video frame
    frameheader.timecode  = timecode;
    /*
      if(frameheader.timecode - lasttimecode > 49 ||
      frameheader.timecode - lasttimecode < 16)
      printf("Recorder timecode irregularity 1, last = '%d' cur = '%d' fnum = '%d' lf = '%d'\n", lasttimecode, frameheader.timecode, fnum, lf); */
    lasttimecode = frameheader.timecode;
    frameheader.filters   = 0;             // no filters applied

    // compr ends here
    if (useavcodec)
    {
        if (mpa_codec->id == CODEC_ID_RAWVIDEO)
        {
            frameheader.comptype = '0';
            frameheader.packetlength = video_buffer_size;
            ringBuffer->Write(&frameheader, FRAMEHEADERSIZE);
            ringBuffer->Write(buf, video_buffer_size);
        }
        else if (hardware_encode)
        {
            frameheader.comptype = '4';
            frameheader.packetlength = len;
            ringBuffer->Write(&frameheader, FRAMEHEADERSIZE);
            ringBuffer->Write(buf, len);
        }
        else
        {
            frameheader.comptype = '4';
            frameheader.packetlength = tmp;
            ringBuffer->Write(&frameheader, FRAMEHEADERSIZE);
            ringBuffer->Write(strm, tmp);
        }
    }
    else if (compressthis == 0 || (tmp < out_len)) 
    {
        if (!raw) 
        {
            frameheader.comptype  = '1'; // video compression: RTjpeg only
            frameheader.packetlength = tmp;
            ringBuffer->Write(&frameheader, FRAMEHEADERSIZE);
            ringBuffer->Write(strm, tmp);
        } 
        else 
        {
            frameheader.comptype  = '0'; // raw YUV420
            frameheader.packetlength = video_buffer_size;
            ringBuffer->Write(&frameheader, FRAMEHEADERSIZE);
            ringBuffer->Write(buf, video_buffer_size); // we write buf directly
        }
    } 
    else 
    {
        if (!raw) 
            frameheader.comptype  = '2'; // video compression: RTjpeg with lzo
        else
            frameheader.comptype  = '3'; // raw YUV420 with lzo
        frameheader.packetlength = out_len;
        ringBuffer->Write(&frameheader, FRAMEHEADERSIZE);
        ringBuffer->Write(out, out_len);
    }

    frameofgop++;
    framesWritten++;
   
    // now we reset the last frame number so that we can find out
    // how many frames we didn't get next time
    lf = fnum;
}

void NuppelVideoRecorder::WriteAudio(unsigned char *buf, int fnum, int timecode)
{
    struct rtframeheader frameheader;
    double mt;
    double eff;
    double abytes;

    if (last_block == 0)
    {
        firsttc = -1;
    }

    if (last_block != 0) 
    {
        if (fnum != (last_block+1)) 
        {
            audio_behind = fnum - (last_block+1);
            printf("audio behind %d %d\n", last_block, fnum);
        }
    }

    frameheader.frametype = 'A'; // audio frame
    frameheader.timecode = timecode;

    if (firsttc == -1) 
    {
        firsttc = timecode;
        //fprintf(stderr, "first timecode=%d\n", firsttc);
    } 
    else 
    {
        timecode -= firsttc; // this is to avoid the lack between the beginning
                             // of recording and the first timestamp, maybe we
                             // can calculate the audio-video +-lack at the 
                            // beginning too
        abytes = (double)audiobytes; // - (double)audio_buffer_size; 
                                     // wrong guess ;-)
        // need seconds instead of msec's
        mt = (double)timecode;
        if (mt > 0.0) 
        {
            eff = (abytes / mt) * (100000.0 / audio_bytes_per_sample);
            effectivedsp = (int)eff;
        }
    }

    if (compressaudio) 
    {
        char mp3gapless[7200];
        int compressedsize = 0;
        int gaplesssize = 0;
        int lameret = 0;

        if(audio_channels == 2)
        {
            lameret = lame_encode_buffer_interleaved(gf, (short int *)buf,
                                                     audio_buffer_size / 
                                                     audio_bytes_per_sample,
                                                     (unsigned char *)mp3buf,
                                                     mp3buf_size);
        }
        else
        {
            lameret = lame_encode_buffer(gf, (short int *)buf, (short int *)buf,
                                         audio_buffer_size / 
                                         audio_bytes_per_sample,
                                         (unsigned char *)mp3buf,
                                         mp3buf_size);
        }

        if (lameret < 0)
        {
            cerr << "lame error, exiting\n";
            exit(-1); 
        }
        compressedsize = lameret;

        lameret = lame_encode_flush_nogap(gf, (unsigned char *)mp3gapless, 
                                          7200);
        if (lameret < 0)
        {
            cerr << "lame error, exiting\n";
            exit(-1);
        }
        gaplesssize = lameret;

        frameheader.comptype = '3'; // audio is compressed
        frameheader.packetlength = compressedsize + gaplesssize;

        if (frameheader.packetlength > 0)
        {
            ringBuffer->Write(&frameheader, FRAMEHEADERSIZE);
            ringBuffer->Write(mp3buf, compressedsize);
            ringBuffer->Write(mp3gapless, gaplesssize);
        }
        audiobytes += audio_buffer_size;
    } 
    else 
    {
        frameheader.comptype = '0'; // uncompressed audio
        frameheader.packetlength = audio_buffer_size;

        ringBuffer->Write(&frameheader, FRAMEHEADERSIZE);
        ringBuffer->Write(buf, audio_buffer_size);
        audiobytes += audio_buffer_size; // only audio no header!!
    }

    // this will probably never happen and if there would be a 
    // 'uncountable' video frame drop -> material==worthless
    if (audio_behind > 0) 
    {
        cerr << "audio behind\n";
        frameheader.frametype = 'A'; // audio frame
        frameheader.comptype  = 'N'; // output a nullframe with
        frameheader.packetlength = 0;
        ringBuffer->Write(&frameheader, FRAMEHEADERSIZE);
        audiobytes += audio_buffer_size;
        audio_behind--;
    }

    last_block = fnum;
}

