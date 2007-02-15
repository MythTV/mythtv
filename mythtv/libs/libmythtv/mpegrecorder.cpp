// -*- Mode: c++ -*-

// C headers
#include <ctime>

// POSIX headers
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

// System headers
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <algorithm>
using namespace std;

// Qt headers
#include <qregexp.h>

// avlib headers
extern "C" {
#include "../libavcodec/avcodec.h"
}

// MythTV headers
#include "mpegrecorder.h"
#include "RingBuffer.h"
#include "mythcontext.h"
#include "programinfo.h"
#include "recordingprofile.h"
#include "tv_rec.h"
#include "videodev_myth.h"
#include "util.h"
#include "cardutil.h"

// ivtv header
extern "C" {
#include "ivtv_myth.h"
}

#define IVTV_KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

#define LOC QString("MPEGRec(%1): ").arg(videodevice)
#define LOC_WARN QString("MPEGRec(%1) Warning: ").arg(videodevice)
#define LOC_ERR QString("MPEGRec(%1) Error: ").arg(videodevice)

const int MpegRecorder::audRateL1[] =
{
    32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0
};

const int MpegRecorder::audRateL2[] =
{
    32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0
};

const int MpegRecorder::audRateL3[] =
{
    32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0
};

const char* MpegRecorder::streamType[] =
{
    "MPEG-2 PS", "MPEG-2 TS",     "MPEG-1 VCD",    "PES AV",
    "",          "PES V",          "",             "PES A",
    "",          "",              "DVD",           "VCD",
    "SVCD",      "DVD-Special 1", "DVD-Special 2", 0
};

const char* MpegRecorder::aspectRatio[] =
{
    "Square", "4:3", "16:9", "2.21:1", 0
};

const unsigned int MpegRecorder::kBuildBufferMaxSize = 1024 * 1024;

MpegRecorder::MpegRecorder(TVRec *rec) :
    RecorderBase(rec),
    // Debugging variables
    deviceIsMpegFile(false),
    bufferSize(4096),
    // Driver info
    card(QString::null),      driver(QString::null),
    version(0),               usingv4l2(false),
    has_buggy_vbi(true),      has_v4l2_vbi(false),
    // State
    recording(false),         encoding(false),
    errored(false),
    // Pausing state
    cleartimeonpause(false),
    // Number of frames written
    framesWritten(0),
    // Encoding info
    width(720),               height(480),
    bitrate(4500),            maxbitrate(6000),
    streamtype(0),            aspectratio(2),
    audtype(2),               audsamplerate(48000),
    audbitratel1(14),         audbitratel2(14),
    audbitratel3(10),
    audvolume(80),            language(0),
    // Input file descriptors
    chanfd(-1),               readfd(-1),
    // Keyframe tracking inforamtion
    keyframedist(15),         gopset(false),
    leftovers(0),             lastpackheaderpos(0),
    lastseqstart(0),          numgops(0),
    // Position map support
    positionMapLock(false),
    // buffer used for ...
    buildbuffer(new unsigned char[kBuildBufferMaxSize + 1]),
    buildbuffersize(0)
{
}

MpegRecorder::~MpegRecorder()
{
    TeardownAll();
    delete [] buildbuffer;
}

void MpegRecorder::TeardownAll(void)
{
    if (chanfd >= 0)
    {
        close(chanfd);
        chanfd = -1;
    }
    if (readfd >= 0)
    {
        close(readfd);
        readfd = -1;
    }
}

static int find_index(const int *audio_rate, int value)
{
    for (uint i = 0; audio_rate[i] != 0; i++)
    {
        if (audio_rate[i] == value)
            return i;
    }

    return -1;
}

void MpegRecorder::SetOption(const QString &opt, int value)
{
    if (opt == "width")
        width = value;
    else if (opt == "height")
        height = value;
    else if (opt == "mpeg2bitrate")
        bitrate = value;
    else if (opt == "mpeg2maxbitrate")
        maxbitrate = value;
    else if (opt == "samplerate")
        audsamplerate = value;
    else if (opt == "mpeg2audbitratel1")
    {
        int index = find_index(audRateL1, value);
        if (index >= 0)
            audbitratel1 = index + 1;
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Audiorate(L1): " +
                    QString("%1 is invalid").arg(value));
        }
    }
    else if (opt == "mpeg2audbitratel2")
    {
        int index = find_index(audRateL2, value);
        if (index >= 0)
            audbitratel2 = index + 1;
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Audiorate(L2): " +
                    QString("%1 is invalid").arg(value));
        }
    }
    else if (opt == "mpeg2audbitratel3")
    {
        int index = find_index(audRateL3, value);
        if (index >= 0)
            audbitratel3 = index + 1;
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Audiorate(L2): " +
                    QString("%1 is invalid").arg(value));
        }
    }
    else if (opt == "mpeg2audvolume")
        audvolume = value;
    else
        RecorderBase::SetOption(opt, value);
}

void MpegRecorder::SetOption(const QString &opt, const QString &value)
{
    if (opt == "mpeg2streamtype")
    {
        bool found = false;
        for (unsigned int i = 0; i < sizeof(streamType) / sizeof(char*); i++)
        {
            if (QString(streamType[i]) == value)
            {
                streamtype = i;
                found = true;
                break;
            }
        }

        if (!found)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "MPEG2 stream type: " +
                    QString("%1 is invalid").arg(value));
        }
    }
    else if (opt == "mpeg2language")
    {
        bool ok = false;
        language = value.toInt(&ok); // on failure language will be 0
        if (!ok)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "MPEG2 language (stereo) flag " +
                    QString("'%1' is invalid").arg(value));
        }
    }
    else if (opt == "mpeg2aspectratio")
    {
        bool found = false;
        for (int i = 0; aspectRatio[i] != 0; i++)
        {
            if (QString(aspectRatio[i]) == value)
            {
                aspectratio = i + 1;
                found = true;
                break;
            }
        }

        if (!found)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "MPEG2 Aspect-ratio: " +
                    QString("%1 is invalid").arg(value));
        }
    }
    else if (opt == "mpeg2audtype")
    {
        if (value == "Layer I")
            audtype = 1;
        else if (value == "Layer II")
            audtype = 2;
        else if (value == "Layer III")
            audtype = 3;
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "MPEG2 audio layer: " +
                    QString("%1 is invalid").arg(value));
        }
    }
    else
    {
        RecorderBase::SetOption(opt, value);
    }
}

void MpegRecorder::SetOptionsFromProfile(RecordingProfile *profile, 
                                         const QString &videodev, 
                                         const QString &audiodev,
                                         const QString &vbidev)
{
    (void)audiodev;
    (void)vbidev;

    if (videodev.lower().left(5) == "file:")
    {
        deviceIsMpegFile = true;
        bufferSize = 64000;
        QString newVideoDev = videodev;
        newVideoDev.replace(QRegExp("^file:", false), QString(""));
        SetOption("videodevice", newVideoDev);
    }
    else
    {
        SetOption("videodevice", videodev);
    }

    SetOption("tvformat", gContext->GetSetting("TVFormat"));
    SetOption("vbiformat", gContext->GetSetting("VbiFormat"));

    SetIntOption(profile, "mpeg2bitrate");
    SetIntOption(profile, "mpeg2maxbitrate");
    SetStrOption(profile, "mpeg2streamtype");
    SetStrOption(profile, "mpeg2aspectratio");
    SetStrOption(profile, "mpeg2language");

    SetIntOption(profile, "samplerate");
    SetStrOption(profile, "mpeg2audtype");
    SetIntOption(profile, "mpeg2audbitratel1");
    SetIntOption(profile, "mpeg2audbitratel2");
    SetIntOption(profile, "mpeg2audbitratel3");
    SetIntOption(profile, "mpeg2audvolume");

    SetIntOption(profile, "width");
    SetIntOption(profile, "height");
}

bool MpegRecorder::OpenMpegFileAsInput(void)
{
    chanfd = readfd = open(videodevice.ascii(), O_RDONLY);

    if (readfd < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("Can't open MPEG File '%1'")
                .arg(videodevice) + ENO);

        return false;
    }
    return true;
}

bool MpegRecorder::OpenV4L2DeviceAsInput(void)
{
    chanfd = open(videodevice.ascii(), O_RDWR);
    if (chanfd < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Can't open video device. " + ENO);
        return false;
    }

    if (CardUtil::GetV4LInfo(chanfd, card, driver, version))
    {
        if (driver == "ivtv")
        {
            usingv4l2     = (version >= IVTV_KERNEL_VERSION(0, 8, 0));
            has_v4l2_vbi  = (version >= IVTV_KERNEL_VERSION(0, 3, 8));
            has_buggy_vbi = (version <  IVTV_KERNEL_VERSION(0, 10, 0));
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "\n\nNot ivtv driver??\n\n");
            usingv4l2 = has_v4l2_vbi = true;
            has_buggy_vbi = false;
        }
    }

    VERBOSE(VB_RECORD, LOC + QString("usingv4l2(%1) has_v4l2_vbi(%2) "
                                     "has_buggy_vbi(%3)")
            .arg(usingv4l2).arg(has_v4l2_vbi).arg(has_buggy_vbi));

    struct v4l2_format vfmt;
    bzero(&vfmt, sizeof(vfmt));

    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(chanfd, VIDIOC_G_FMT, &vfmt) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Error getting format" + ENO);
        return false;
    }

    vfmt.fmt.pix.width = width;
    vfmt.fmt.pix.height = height;

    if (ioctl(chanfd, VIDIOC_S_FMT, &vfmt) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Error setting format" + ENO);
        return false;
    }

    // Set audio language mode
    bool do_audmode_set = true;
    struct v4l2_tuner vt;
    bzero(&vt, sizeof(struct v4l2_tuner));
    if (ioctl(chanfd, VIDIOC_G_TUNER, &vt) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Unable to get audio mode" + ENO);
        do_audmode_set = false;
    }

    static const int v4l2_lang[3] =
    {
        V4L2_TUNER_MODE_LANG1,
        V4L2_TUNER_MODE_LANG2,
        V4L2_TUNER_MODE_STEREO,
    };
    language = (language > 2) ? 0 : language;
    vt.audmode = v4l2_lang[language];

    int audio_layer = GetFilteredAudioLayer();
    if (do_audmode_set && (2 == language) && (1 == audio_layer))
    {
        VERBOSE(VB_GENERAL, "Dual audio mode incompatible with Layer I audio."
                "\n\t\t\tFalling back to Main Language");
        vt.audmode = V4L2_TUNER_MODE_LANG1;
    }

    if (do_audmode_set && ioctl(chanfd, VIDIOC_S_TUNER, &vt) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Unable to set audio mode" + ENO);
    }

    // Set recording volume
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_AUDIO_VOLUME;
    ctrl.value = 65536 / 100 *audvolume;

    if (ioctl(chanfd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "Unable to set recording volume" + ENO + "\n\t\t\t" +
                "If you are using an AverMedia M179 card this is normal.");
    }

    bool ok = true;
    if (usingv4l2)
        ok = SetV4L2DeviceOptions(chanfd);
    else
    {
        ok = SetIVTVDeviceOptions(chanfd);
        if (!ok)
            usingv4l2 = ok = SetV4L2DeviceOptions(chanfd);
    }

    if (!ok)
        return false;

    SetVBIOptions(chanfd);

    readfd = open(videodevice.ascii(), O_RDWR | O_NONBLOCK);
    if (readfd < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Can't open video device." + ENO);
        return false;
    }

    return true;
}

uint MpegRecorder::GetFilteredStreamType(void) const
{
    uint st = (uint) streamtype;

    if (driver == "ivtv")
    {
        switch (st)
        {
            case 2:  st = 2;  break;
            case 10:
            case 13:
            case 14: st = 10; break;
            case 11: st = 11; break;
            case 12: st = 12; break;
            default: st = 0;  break;   
        }
    }

    if (st != (uint) streamtype)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Stream type '%1'\n\t\t\t"
                        "is not supported by %2 driver, using '%3' instead.")
                .arg(streamType[streamtype]).arg(driver).arg(streamType[st]));
    }

    return st;
}

uint MpegRecorder::GetFilteredAudioSampleRate(void) const
{
    uint sr = (uint) audsamplerate;

    sr = (driver == "ivtv") ? 48000 : sr; // only 48kHz works properly.

    if (sr != (uint) audsamplerate)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Audio sample rate %1 Hz\n\t\t\t"
                        "is not supported by %2 driver, using %3 Hz instead.")
                .arg(audsamplerate).arg(driver).arg(sr));
    }

    switch (sr)
    {
        case 32000: return V4L2_MPEG_AUDIO_SAMPLING_FREQ_32000;
        case 44100: return V4L2_MPEG_AUDIO_SAMPLING_FREQ_44100;
        case 48000: return V4L2_MPEG_AUDIO_SAMPLING_FREQ_48000;
        default:    return V4L2_MPEG_AUDIO_SAMPLING_FREQ_48000;
    }
}

uint MpegRecorder::GetFilteredAudioLayer(void) const
{
    uint layer = (uint) audtype;

    layer = max(min(layer, 3U), 1U);

    layer = (driver == "ivtv") ? 2 : layer;

    if (layer != (uint) audtype)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("MPEG layer %1 does not work properly\n\t\t\t"
                        "with %2 driver. Using MPEG layer %3 audio instead.")
                .arg(audtype).arg(driver).arg(layer));
    }

    return layer;
}

uint MpegRecorder::GetFilteredAudioBitRate(uint audio_layer) const
{
    return ((2 == audio_layer) ? max(audbitratel2, 10) :
            ((3 == audio_layer) ? audbitratel3 : max(audbitratel1, 6)));
}

bool MpegRecorder::SetIVTVDeviceOptions(int chanfd)
{
    struct ivtv_ioctl_codec ivtvcodec;
    bzero(&ivtvcodec, sizeof(ivtvcodec));

    if (ioctl(chanfd, IVTV_IOC_G_CODEC, &ivtvcodec) < 0)
    {
        // Downgrade to VB_RECORD warning when ioctl isn't supported,
        // unless the driver is completely missitng it probably
        // supports the Linux v2.6.18 v4l2 mpeg encoder API.
        VERBOSE((22 == errno) ? VB_RECORD : VB_IMPORTANT,
                ((22 == errno) ? LOC_WARN : LOC_ERR) +
                "Error getting codec params using old IVTV ioctl" + ENO);
        return false;
    }

    uint audio_rate  = GetFilteredAudioSampleRate();
    uint audio_layer = GetFilteredAudioLayer();
    uint audbitrate  = GetFilteredAudioBitRate(audio_layer);

    ivtvcodec.audio_bitmask  = audio_rate | (audio_layer << 2);
    ivtvcodec.audio_bitmask |= audbitrate << 4;
    ivtvcodec.aspect         = aspectratio;
    ivtvcodec.bitrate        = min(bitrate, maxbitrate) * 1000;
    ivtvcodec.bitrate_peak   = maxbitrate * 1000;
    ivtvcodec.framerate      = ntsc_framerate ? 0 : 1; // 1->25fps, 0->30fps
    ivtvcodec.stream_type    = GetFilteredStreamType();

    if (ioctl(chanfd, IVTV_IOC_S_CODEC, &ivtvcodec) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Error setting codec params" + ENO);
        return false;
    }

    keyframedist = (ivtvcodec.framerate) ? 12 : keyframedist;

    return true;
}

static int streamtype_ivtv_to_v4l2(int st)
{
    switch (st)
    {
        case 0:  return V4L2_MPEG_STREAM_TYPE_MPEG2_PS;
        case 1:  return V4L2_MPEG_STREAM_TYPE_MPEG2_TS;
        case 2:  return V4L2_MPEG_STREAM_TYPE_MPEG1_VCD;
        case 3:  return V4L2_MPEG_STREAM_TYPE_MPEG2_PS;  /* PES A/V    */
        case 5:  return V4L2_MPEG_STREAM_TYPE_MPEG2_PS;  /* PES V      */
        case 7:  return V4L2_MPEG_STREAM_TYPE_MPEG2_PS;  /* PES A      */
        case 10: return V4L2_MPEG_STREAM_TYPE_MPEG2_DVD;
        case 11: return V4L2_MPEG_STREAM_TYPE_MPEG1_VCD; /* VCD */
        case 12: return V4L2_MPEG_STREAM_TYPE_MPEG2_SVCD;
        case 13: return V4L2_MPEG_STREAM_TYPE_MPEG2_DVD; /* DVD-Special 1 */
        case 14: return V4L2_MPEG_STREAM_TYPE_MPEG2_DVD; /* DVD-Special 2 */
        default: return V4L2_MPEG_STREAM_TYPE_MPEG2_TS;
    }
}

bool MpegRecorder::SetV4L2DeviceOptions(int chanfd)
{
    static const uint kNumControls = 7;
    struct v4l2_ext_controls ctrls;
    struct v4l2_ext_control ext_ctrl[kNumControls];
    QString control_description[kNumControls] =
    {
        "Audio Sampling Frequency",
        "Video Aspect ratio",
        "Audio Encoding",
        "Audio L2 Bitrate",
        "Video Average Bitrate",
        "Video Peak Bitrate",
        "MPEG Stream type",
    };

    // Set controls
    bzero(&ctrls,    sizeof(struct v4l2_ext_controls));
    bzero(&ext_ctrl, sizeof(struct v4l2_ext_control) * kNumControls);

    uint audio_layer = GetFilteredAudioLayer();
    uint audbitrate  = GetFilteredAudioBitRate(audio_layer);

    ext_ctrl[0].id    = V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ;
    ext_ctrl[0].value = GetFilteredAudioSampleRate();

    ext_ctrl[1].id    = V4L2_CID_MPEG_VIDEO_ASPECT;
    ext_ctrl[1].value = aspectratio - 1;

    ext_ctrl[2].id    = V4L2_CID_MPEG_AUDIO_ENCODING;
    ext_ctrl[2].value = audio_layer - 1;

    ext_ctrl[3].id    = V4L2_CID_MPEG_AUDIO_L2_BITRATE;
    ext_ctrl[3].value = audbitrate - 1;

    ext_ctrl[4].id    = V4L2_CID_MPEG_VIDEO_BITRATE;
    ext_ctrl[4].value = (bitrate = min(bitrate, maxbitrate)) * 1000;

    ext_ctrl[5].id    = V4L2_CID_MPEG_VIDEO_BITRATE_PEAK;
    ext_ctrl[5].value = maxbitrate * 1000;

    ext_ctrl[6].id    = V4L2_CID_MPEG_STREAM_TYPE;
    ext_ctrl[6].value = streamtype_ivtv_to_v4l2(GetFilteredStreamType());

    for (uint i = 0; i < kNumControls; i++)
    {
        int value = ext_ctrl[i].value;

        ctrls.ctrl_class  = V4L2_CTRL_CLASS_MPEG;
        ctrls.count       = 1;
        ctrls.controls    = ext_ctrl + i;

        if (ioctl(chanfd, VIDIOC_S_EXT_CTRLS, &ctrls) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Could not set %1 to %2")
                    .arg(control_description[i]).arg(value) + ENO);
        }
    }

    // Get controls
    ext_ctrl[0].id    = V4L2_CID_MPEG_VIDEO_GOP_SIZE;
    ext_ctrl[0].value = 0;

    ctrls.ctrl_class  = V4L2_CTRL_CLASS_MPEG;
    ctrls.count       = 1;
    ctrls.controls    = ext_ctrl;

    if (ioctl(chanfd, VIDIOC_G_EXT_CTRLS, &ctrls) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Unable to get "
                "V4L2_CID_MPEG_VIDEO_GOP_SIZE, defaulting to 12" + ENO);
        ext_ctrl[0].value = 12;
    }

    keyframedist = ext_ctrl[0].value;

    return true;
}

bool MpegRecorder::SetVBIOptions(int chanfd)
{
    if (!vbimode)
        return true;

    if (has_buggy_vbi)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "VBI recording with broken drivers."
                "\n\t\t\tUpgrade to ivtv 0.10.0 if you experience problems.");
    }

    /****************************************************************/
    /** First tell driver which services we want to capture.       **/

#ifdef IVTV_IOC_S_VBI_EMBED
    // used for ivtv driver versions 0.2.0-0.3.7
    if (!has_v4l2_vbi)
    {
        struct ivtv_sliced_vbi_format vbifmt;
        bzero(&vbifmt, sizeof(struct ivtv_sliced_vbi_format));
        vbifmt.service_set = (1 == vbimode) ? VBI_TYPE_TELETEXT : VBI_TYPE_CC;

        if (ioctl(chanfd, IVTV_IOC_S_VBI_MODE, &vbifmt) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    "Can't enable VBI recording (1)" + ENO);

            return false;
        }

        if (ioctl(chanfd, IVTV_IOC_G_VBI_MODE, &vbifmt) >= 0)
        {
            VERBOSE(VB_RECORD, LOC + QString(
                        "VBI service:%1, packet size:%2, io size:%3")
                    .arg(vbifmt.service_set).arg(vbifmt.packet_size)
                    .arg(vbifmt.io_size));
        }
    }
#endif // IVTV_IOC_S_VBI_EMBED

#ifdef V4L2_CAP_SLICED_VBI_CAPTURE
    // used for ivtv driver versions 0.3.8+
    if (has_v4l2_vbi)
    {
        struct v4l2_format vbifmt;
        bzero(&vbifmt, sizeof(struct v4l2_format));
        vbifmt.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
        vbifmt.fmt.sliced.service_set |= (1 == vbimode) ?
            V4L2_SLICED_VBI_625 : V4L2_SLICED_VBI_525;

        if (ioctl(chanfd, VIDIOC_S_FMT, &vbifmt) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    "Can't enable VBI recording (3)" + ENO);

            return false;
        }

        if (ioctl(chanfd, VIDIOC_G_FMT, &vbifmt) >= 0)
        {
            VERBOSE(VB_RECORD, LOC + QString("VBI service: %1, io size: %2")
                    .arg(vbifmt.fmt.sliced.service_set)
                    .arg(vbifmt.fmt.sliced.io_size));
        }
    }
#endif // V4L2_CAP_SLICED_VBI_CAPTURE

    /****************************************************************/
    /** Second, tell driver to embed the captions in the stream.   **/

#ifdef IVTV_IOC_S_VBI_EMBED
    // used for ivtv driver versions 0.2.0-0.7.x
    if (!usingv4l2)
    {
        int embedon = 1;
        if (ioctl(chanfd, IVTV_IOC_S_VBI_EMBED, &embedon) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    "Can't enable VBI recording (4)" + ENO);

            return false;
        }
    }
#endif // IVTV_IOC_S_VBI_EMBED

#ifdef V4L2_CAP_SLICED_VBI_CAPTURE
    // used for ivtv driver versions 0.8.0+
    if (usingv4l2)
    {
        struct v4l2_ext_control vbi_ctrl;
        vbi_ctrl.id      = V4L2_CID_MPEG_STREAM_VBI_FMT;
        vbi_ctrl.value   = V4L2_MPEG_STREAM_VBI_FMT_IVTV;

        struct v4l2_ext_controls ctrls;
        bzero(&ctrls, sizeof(struct v4l2_ext_controls));
        ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
        ctrls.count      = 1;
        ctrls.controls   = &vbi_ctrl;

        if (ioctl(chanfd, VIDIOC_S_EXT_CTRLS, &ctrls) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    "Can't enable VBI recording (5)" + ENO);

            return false;
        }
    }
#endif // V4L2_CAP_SLICED_VBI_CAPTURE

    return true;
}

bool MpegRecorder::Open(void)
{
    if (deviceIsMpegFile)
        return OpenMpegFileAsInput();
    else
        return OpenV4L2DeviceAsInput();
}

void MpegRecorder::StartRecording(void)
{
    if (!Open())
    {
	errored = true;
	return;
    }

    if (!SetupRecording())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to setup recording.");
	errored = true;
	return;
    }

    encoding = true;
    recording = true;
    unsigned char *buffer = new unsigned char[bufferSize + 1];
    int ret;

    MythTimer elapsedTimer;
    float elapsed;

    struct timeval tv;
    fd_set rdset;

    if (deviceIsMpegFile)
        elapsedTimer.start();

    while (encoding)
    {
        if (PauseAndWait(100))
            continue;

        if ((deviceIsMpegFile) && (framesWritten))
        {
            elapsed = (elapsedTimer.elapsed() / 1000.0) + 1;
            while ((framesWritten / elapsed) > 30)
            {
                usleep(50000);
                elapsed = (elapsedTimer.elapsed() / 1000.0) + 1;
            }
        }

        if (readfd < 0)
            readfd = open(videodevice.ascii(), O_RDWR);

        tv.tv_sec = 5;
        tv.tv_usec = 0;
        FD_ZERO(&rdset);
        FD_SET(readfd, &rdset);

#if defined(__FreeBSD__)
        // HACK. FreeBSD PVR150/500 driver doesn't currently support select()
#else
        switch (select(readfd + 1, &rdset, NULL, NULL, &tv))
        {
            case -1:
                if (errno == EINTR)
                    continue;

                VERBOSE(VB_IMPORTANT, LOC_ERR + "Select error" + ENO);
                continue;

            case 0:
                VERBOSE(VB_IMPORTANT, LOC_ERR + "select timeout - "
                        "ivtv driver has stopped responding");

                if (close(readfd) != 0)
                {
                    VERBOSE(VB_IMPORTANT, LOC_ERR + "Close error" + ENO);
                }

                readfd = -1; // Force PVR card to be reopened on next iteration
                continue;

           default: break;
        }
#endif

        ret = read(readfd, buffer, bufferSize);

        if ((ret == 0) &&
            (deviceIsMpegFile))
        {
            close(readfd);
            readfd = open(videodevice.ascii(), O_RDONLY);

            if (readfd >= 0)
                ret = read(readfd, buffer, bufferSize);
            if (ret <= 0)
            {
                encoding = false;
                continue;
            }
        }
        else if (ret < 0 && errno != EAGAIN)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + QString("error reading from: %1")
                    .arg(videodevice) + ENO);

            continue;
        }
        else if (ret > 0)
        {
            ProcessData(buffer, ret);
        }
    }

    FinishRecording();

    delete[] buffer;
    recording = false;
}

bool MpegRecorder::SetupRecording(void)
{
    leftovers = 0xFFFFFFFF;
    numgops = 0;
    lastseqstart = 0;
    return true;
}

void MpegRecorder::FinishRecording(void)
{
    ringBuffer->WriterFlush();

    if (curRecording)
    {
        curRecording->SetFilesize(ringBuffer->GetRealFileSize());
        SavePositionMap(true);
    }
    positionMapLock.lock();
    positionMap.clear();
    positionMapDelta.clear();
    positionMapLock.unlock();
}

#define PACK_HEADER   0x000001BA
#define GOP_START     0x000001B8
#define SEQ_START     0x000001B3
#define SLICE_MIN     0x00000101
#define SLICE_MAX     0x000001af

void MpegRecorder::ProcessData(unsigned char *buffer, int len)
{
    unsigned char *bufptr = buffer, *bufstart = buffer;
    unsigned int state = leftovers, v = 0;
    int leftlen = len;

    while (bufptr < buffer + len)
    {
        v = *bufptr++;
        if (state == 0x000001)
        {
            state = ((state << 8) | v) & 0xFFFFFF;
            
            if (state == PACK_HEADER)
            {
                long long startpos = ringBuffer->GetWritePosition();
                startpos += buildbuffersize + bufptr - bufstart - 4;
                lastpackheaderpos = startpos;

                int curpos = bufptr - bufstart - 4;
                if (curpos < 0)
                {
                    // header was split
                    buildbuffersize += curpos;
                    if (buildbuffersize > 0)
                        ringBuffer->Write(buildbuffer, buildbuffersize);

                    buildbuffersize = 4;
                    memcpy(buildbuffer, &state, 4);

                    leftlen = leftlen - curpos + 4;
                    bufstart = bufptr;
                }
                else
                {
                    // header was entirely in this packet
                    memcpy(buildbuffer + buildbuffersize, bufstart, curpos);
                    buildbuffersize += curpos;
                    bufstart += curpos;
                    leftlen -= curpos;

                    if (buildbuffersize > 0)
                        ringBuffer->Write(buildbuffer, buildbuffersize);

                    buildbuffersize = 0;
                }
            }

            if (state == SEQ_START)
            {
                lastseqstart = lastpackheaderpos;
            }

            if (state == GOP_START && lastseqstart == lastpackheaderpos)
            {
                framesWritten = numgops * keyframedist;
                numgops++;
                HandleKeyframe();
            }
        }
        else
            state = ((state << 8) | v) & 0xFFFFFF;
    }

    leftovers = state;

    if (buildbuffersize + leftlen > kBuildBufferMaxSize)
    {
        ringBuffer->Write(buildbuffer, buildbuffersize);
        buildbuffersize = 0;
    }

    // copy remaining..
    memcpy(buildbuffer + buildbuffersize, bufstart, leftlen);
    buildbuffersize += leftlen;
}

void MpegRecorder::StopRecording(void)
{
    encoding = false;
}

void MpegRecorder::ResetForNewFile(void)
{
    errored = false;
    framesWritten = 0;
    numgops = 0;
    lastseqstart = lastpackheaderpos = 0;

    positionMap.clear();
    positionMapDelta.clear();
}

void MpegRecorder::Reset(void)
{
    ResetForNewFile();

    leftovers = 0xFFFFFFFF;
    buildbuffersize = 0;

    if (curRecording)
        curRecording->ClearPositionMap(MARK_GOP_START);
}

void MpegRecorder::Pause(bool clear)
{
    cleartimeonpause = clear;
    paused = false;
    request_pause = true;
}

long long MpegRecorder::GetKeyframePosition(long long desired)
{
    QMutexLocker locker(&positionMapLock);
    long long ret = -1;

    if (positionMap.find(desired) != positionMap.end())
        ret = positionMap[desired];

    return ret;
}

// documented in recorderbase.h
void MpegRecorder::SetNextRecording(const ProgramInfo *progInf, RingBuffer *rb)
{
    // First we do some of the time consuming stuff we can do now
    SavePositionMap(true);
    ringBuffer->WriterFlush();

    // Then we set the next info
    {
        QMutexLocker locker(&nextRingBufferLock);
        nextRecording = NULL;
        if (progInf)
            nextRecording = new ProgramInfo(*progInf);
        nextRingBuffer = rb;
    }
}

/** \fn MpegRecorder::HandleKeyframe(void)
 *  \brief This save the current frame to the position maps
 *         and handles ringbuffer switching.
 */
void MpegRecorder::HandleKeyframe(void)
{
    // Add key frame to position map
    bool save_map = false;
    positionMapLock.lock();
    if (!positionMap.contains(numgops))
    {
        positionMapDelta[numgops] = lastpackheaderpos;
        positionMap[numgops]      = lastpackheaderpos;
        save_map = true;
    }
    positionMapLock.unlock();
    // Save the position map delta, but don't force a save.
    if (save_map)
        SavePositionMap(false);

    // Perform ringbuffer switch if needed.
    CheckForRingBufferSwitch();
}

/** \fn MpegRecorder::SavePositionMap(bool)
 *  \brief This saves the postition map delta to the database if force
 *         is true or there are 30 frames in the map or there are five
 *         frames in the map with less than 30 frames in the non-delta
 *         position map.
 *  \param force If true this forces a DB sync.
 */
void MpegRecorder::SavePositionMap(bool force)
{
    QMutexLocker locker(&positionMapLock);

    // save on every 5th key frame if in the first few frames of a recording
    force |= (positionMap.size() < 30) && (positionMap.size() % 5 == 1);
    // save every 30th key frame later on
    force |= positionMapDelta.size() >= 30;

    if (curRecording && force)
    {
        curRecording->SetPositionMapDelta(positionMapDelta,
                                          MARK_GOP_START);
        curRecording->SetFilesize(lastpackheaderpos);
        positionMapDelta.clear();
    }
}
