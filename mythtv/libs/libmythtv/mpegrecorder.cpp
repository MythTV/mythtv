// -*- Mode: c++ -*-

// C headers
#include <ctime>

// C++ headers
#include <algorithm>
#include <vector>
using namespace std;

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
#include <sys/poll.h>

#include <linux/videodev2.h>

#include "mythconfig.h"

// avlib headers
extern "C" {
#include "libavcodec/avcodec.h"
}

// MythTV headers
#include "mpegrecorder.h"
#include "ringbuffer.h"
#include "mythcorecontext.h"
#include "programinfo.h"
#include "recordingprofile.h"
#include "tv_rec.h"
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

MpegRecorder::MpegRecorder(TVRec *rec) :
    DTVRecorder(rec),
    // Debugging variables
    deviceIsMpegFile(false),      bufferSize(0),
    // Driver info
    card(QString::null),          driver(QString::null),
    version(0),                   usingv4l2(false),
    has_buggy_vbi(true),          has_v4l2_vbi(false),
    requires_special_pause(false),
    // State
    recording(false),             encoding(false),
    start_stop_encoding_lock(QMutex::Recursive),
    recording_wait_lock(),        recording_wait(),
    // Pausing state
    cleartimeonpause(false),
    // Encoding info
    width(720),                   height(480),
    bitrate(4500),                maxbitrate(6000),
    streamtype(0),                aspectratio(2),
    audtype(2),                   audsamplerate(48000),
    audbitratel1(14),             audbitratel2(14),
    audbitratel3(10),
    audvolume(80),                language(0),
    low_mpeg4avgbitrate(4500),    low_mpeg4peakbitrate(6000),
    medium_mpeg4avgbitrate(9000), medium_mpeg4peakbitrate(13500),
    high_mpeg4avgbitrate(13500),  high_mpeg4peakbitrate(20200),
    // Input file descriptors
    chanfd(-1),                   readfd(-1),
    _device_read_buffer(NULL),
    // Statistics
    _continuity_error_count(0),   _stream_overflow_count(0),
    _bad_packet_count(0)
{
    memset(_stream_id, 0, sizeof(_stream_id));
    memset(_pid_status, 0, sizeof(_pid_status));
    memset(_continuity_counter, 0, sizeof(_continuity_counter));
}

MpegRecorder::~MpegRecorder()
{
    TeardownAll();
}

void MpegRecorder::TeardownAll(void)
{
    StopRecording();

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
    else if (opt.right(16) == "_mpeg4avgbitrate")
    {
        if (opt.left(3) == "low")
            low_mpeg4avgbitrate = value;
        else if (opt.left(6) == "medium")
            medium_mpeg4avgbitrate = value;
        else if (opt.left(4) == "high")
            high_mpeg4avgbitrate = value;
        else
            RecorderBase::SetOption(opt, value);
    }
    else if (opt.right(17) == "_mpeg4peakbitrate")
    {
        if (opt.left(3) == "low")
            low_mpeg4peakbitrate = value;
        else if (opt.left(6) == "medium")
            medium_mpeg4peakbitrate = value;
        else if (opt.left(4) == "high")
            high_mpeg4peakbitrate = value;
        else
            RecorderBase::SetOption(opt, value);
    }
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
    else if (opt == "audiocodec")
    {
        if (value == "AAC Hardware Encoder")
            audtype = 4;
        else if (value == "AC3 Hardware Encoder")
            audtype = 5;
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

    if (videodev.toLower().left(5) == "file:")
    {
        deviceIsMpegFile = true;
        bufferSize = 64000;
        QString newVideoDev = videodev;
        if (newVideoDev.startsWith("file:", Qt::CaseInsensitive))
            newVideoDev = newVideoDev.remove(0,5);
        SetOption("videodevice", newVideoDev);
    }
    else
    {
        SetOption("videodevice", videodev);
    }

    SetOption("audiodevice", audiodev);

    SetOption("tvformat", gCoreContext->GetSetting("TVFormat"));
    SetOption("vbiformat", gCoreContext->GetSetting("VbiFormat"));

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

    SetIntOption(profile, "low_mpeg4avgbitrate");
    SetIntOption(profile, "low_mpeg4peakbitrate");
    SetIntOption(profile, "medium_mpeg4avgbitrate");
    SetIntOption(profile, "medium_mpeg4peakbitrate");
    SetIntOption(profile, "high_mpeg4avgbitrate");
    SetIntOption(profile, "high_mpeg4peakbitrate");

    SetStrOption(profile, "audiocodec");
}

// same as the base class, it just doesn't complain if an option is missing
void MpegRecorder::SetIntOption(RecordingProfile *profile, const QString &name)
{
    const Setting *setting = profile->byName(name);
    if (setting)
        SetOption(name, setting->getValue().toInt());
}

// same as the base class, it just doesn't complain if an option is missing
void MpegRecorder::SetStrOption(RecordingProfile *profile, const QString &name)
{
    const Setting *setting = profile->byName(name);
    if (setting)
        SetOption(name, setting->getValue());
}

bool MpegRecorder::OpenMpegFileAsInput(void)
{
    QByteArray vdevice = videodevice.toAscii();
    chanfd = readfd = open(vdevice.constData(), O_RDONLY);

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
    // open implicitly starts encoding, so we need the lock..
    QMutexLocker locker(&start_stop_encoding_lock);

    QByteArray vdevice = videodevice.toAscii();
    chanfd = open(vdevice.constData(), O_RDWR);
    if (chanfd < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Can't open video device. " + ENO);
        return false;
    }

    if (CardUtil::GetV4LInfo(chanfd, card, driver, version))
    {
        if (driver == "ivtv")
        {
            bufferSize    = 4096;
            usingv4l2     = (version >= IVTV_KERNEL_VERSION(0, 8, 0));
            has_v4l2_vbi  = (version >= IVTV_KERNEL_VERSION(0, 3, 8));
            has_buggy_vbi = true;
            requires_special_pause =
                (version >= IVTV_KERNEL_VERSION(0, 10, 0));
        }
        else if (driver == "pvrusb2")
        {
            bufferSize    = 4096;
            usingv4l2     = true;
            has_v4l2_vbi  = true;
            has_buggy_vbi = true;
            requires_special_pause = false;
        }
        else if (driver == "hdpvr")
        {
            bufferSize = 1500 * TSPacket::SIZE;
            usingv4l2 = true;
            requires_special_pause = true;

            memset(_stream_id, 0,  sizeof(_stream_id));
            memset(_pid_status, 0, sizeof(_pid_status));
            memset(_continuity_counter, 0xff, sizeof(_continuity_counter));

            m_h264_parser.use_I_forKeyframes(false);
        }
        else
        {
            bufferSize    = 4096;
            usingv4l2     = has_v4l2_vbi = true;
            has_buggy_vbi = requires_special_pause = false;
        }
    }

    VERBOSE(VB_RECORD, LOC + QString("usingv4l2(%1) has_v4l2_vbi(%2) "
                                     "has_buggy_vbi(%3)")
            .arg(usingv4l2).arg(has_v4l2_vbi).arg(has_buggy_vbi));


    if ((driver != "hdpvr") && !SetFormat(chanfd))
        return false;

    if (driver != "hdpvr")
    {
        SetLanguageMode(chanfd);        // we don't care if this fails...
        SetRecordingVolume(chanfd); // we don't care if this fails...
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

    readfd = open(vdevice.constData(), O_RDWR | O_NONBLOCK);

    if (readfd < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Can't open video device." + ENO);
        return false;
    }

    if (_device_read_buffer)
    {
        if (_device_read_buffer->IsRunning())
            _device_read_buffer->Stop();

        delete _device_read_buffer;
        _device_read_buffer = NULL;
    }

    _device_read_buffer = new DeviceReadBuffer(this);

    if (!_device_read_buffer)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to allocate DRB buffer");
        _error = true;
        return false;
    }

    if (!_device_read_buffer->Setup(vdevice.constData(), readfd))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to allocate DRB buffer");
        _error = true;
        return false;
    }

    VERBOSE(VB_RECORD, LOC + "DRB ready");

    return true;
}


bool MpegRecorder::SetFormat(int chanfd)
{
    struct v4l2_format vfmt;
    memset(&vfmt, 0, sizeof(vfmt));

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

    return true;
}

/// Set audio language mode
bool MpegRecorder::SetLanguageMode(int chanfd)
{
    struct v4l2_tuner vt;
    memset(&vt, 0, sizeof(struct v4l2_tuner));
    if (ioctl(chanfd, VIDIOC_G_TUNER, &vt) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Unable to get audio mode" + ENO);
        return false;
    }

    switch (language)
    {
        case 0:
            vt.audmode = V4L2_TUNER_MODE_LANG1;
            break;
        case 1:
            vt.audmode = V4L2_TUNER_MODE_LANG2;
            break;
        case 2:
            if (usingv4l2)
                vt.audmode = V4L2_TUNER_MODE_LANG1_LANG2;
            else
                vt.audmode = V4L2_TUNER_MODE_STEREO;
            break;
        default:
            vt.audmode = V4L2_TUNER_MODE_LANG1;
    }

    int audio_layer = GetFilteredAudioLayer();
    bool success = true;
    if ((2 == language) && (1 == audio_layer))
    {
        VERBOSE(VB_GENERAL, "Dual audio mode incompatible with Layer I audio."
                "\n\t\t\tFalling back to Main Language");
        vt.audmode = V4L2_TUNER_MODE_LANG1;
        success = false;
    }

    if (ioctl(chanfd, VIDIOC_S_TUNER, &vt) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Unable to set audio mode" + ENO);
        success = false;
    }

    return success;
}

bool MpegRecorder::SetRecordingVolume(int chanfd)
{
    // Get volume min/max values
    struct v4l2_queryctrl qctrl;
    qctrl.id = V4L2_CID_AUDIO_VOLUME;
    if (ioctl(chanfd, VIDIOC_QUERYCTRL, &qctrl) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "Unable to get recording volume parameters(max/min)" + ENO +
                "\n\t\t\tusing default range [0,65535].");
        qctrl.maximum = 65535;
        qctrl.minimum = 0;
    }

    // calculate volume in card units.
    int range = qctrl.maximum - qctrl.minimum;
    int value = (int) ((range * audvolume * 0.01f) + qctrl.minimum);
    int ctrl_volume = min(qctrl.maximum, max(qctrl.minimum, value));

    // Set recording volume
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_AUDIO_VOLUME;
    ctrl.value = ctrl_volume;

    if (ioctl(chanfd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "Unable to set recording volume" + ENO + "\n\t\t\t" +
                "If you are using an AverMedia M179 card this is normal.");
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
    memset(&ivtvcodec, 0, sizeof(ivtvcodec));

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

static void add_ext_ctrl(vector<struct v4l2_ext_control> &ctrl_list,
                         uint32_t id, int32_t value)
{
    struct v4l2_ext_control tmp_ctrl;
    memset(&tmp_ctrl, 0, sizeof(struct v4l2_ext_control));
    tmp_ctrl.id    = id;
    tmp_ctrl.value = value;
    ctrl_list.push_back(tmp_ctrl);
}

static void set_ctrls(int fd, vector<struct v4l2_ext_control> &ext_ctrls)
{
    static QMutex control_description_lock;
    static QMap<uint32_t,QString> control_description;

    control_description_lock.lock();
    if (control_description.isEmpty())
    {
        control_description[V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ] =
            "Audio Sampling Frequency";
        control_description[V4L2_CID_MPEG_VIDEO_ASPECT] =
            "Video Aspect ratio";
        control_description[V4L2_CID_MPEG_AUDIO_ENCODING] =
            "Audio Encoding";
        control_description[V4L2_CID_MPEG_AUDIO_L2_BITRATE] =
            "Audio L2 Bitrate";
        control_description[V4L2_CID_MPEG_VIDEO_BITRATE_PEAK] =
            "Video Peak Bitrate";
        control_description[V4L2_CID_MPEG_VIDEO_BITRATE] =
            "Video Average Bitrate";
        control_description[V4L2_CID_MPEG_STREAM_TYPE] =
            "MPEG Stream type";
        control_description[V4L2_CID_MPEG_VIDEO_BITRATE_MODE] =
            "MPEG Bitrate mode";
    }
    control_description_lock.unlock();

    for (uint i = 0; i < ext_ctrls.size(); i++)
    {
        struct v4l2_ext_controls ctrls;
        memset(&ctrls, 0, sizeof(struct v4l2_ext_controls));

        int value = ext_ctrls[i].value;

        ctrls.ctrl_class  = V4L2_CTRL_CLASS_MPEG;
        ctrls.count       = 1;
        ctrls.controls    = &ext_ctrls[i];

        if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls) < 0)
        {
            QMutexLocker locker(&control_description_lock);
            VERBOSE(VB_IMPORTANT, QString("mpegrecorder.cpp:set_ctrls(): ") +
                    QString("Could not set %1 to %2")
                    .arg(control_description[ext_ctrls[i].id]).arg(value) +
                    ENO);
        }
    }
}

bool MpegRecorder::SetV4L2DeviceOptions(int chanfd)
{
    vector<struct v4l2_ext_control> ext_ctrls;

    // Set controls
    if (driver != "hdpvr")
    {
        if (driver != "saa7164")
        {
            add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ,
                         GetFilteredAudioSampleRate());

            uint audio_layer = GetFilteredAudioLayer();
            add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_AUDIO_ENCODING,
                         audio_layer - 1);

            uint audbitrate  = GetFilteredAudioBitRate(audio_layer);
            add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_AUDIO_L2_BITRATE,
                         audbitrate - 1);
        }

        add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_VIDEO_ASPECT,
                     aspectratio - 1);

        add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_STREAM_TYPE,
                     streamtype_ivtv_to_v4l2(GetFilteredStreamType()));

    }
    else
    {
        maxbitrate = high_mpeg4peakbitrate;
        bitrate    = high_mpeg4avgbitrate;
    }
    maxbitrate = std::max(maxbitrate, bitrate);

    if (driver == "hdpvr" || driver == "saa7164")
    {
        add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
                     (maxbitrate == bitrate) ?
                     V4L2_MPEG_VIDEO_BITRATE_MODE_CBR :
                     V4L2_MPEG_VIDEO_BITRATE_MODE_VBR);
    }

    add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_VIDEO_BITRATE,
                 bitrate * 1000);

    add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_VIDEO_BITRATE_PEAK,
                 maxbitrate * 1000);

    set_ctrls(chanfd, ext_ctrls);

    bool ok;
    int audioinput = audiodevice.toUInt(&ok);
    if (ok)
    {
        struct v4l2_audio ain;
        memset(&ain, 0, sizeof(ain));
        ain.index = audioinput;
        if (ioctl(chanfd, VIDIOC_ENUMAUDIO, &ain) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    "Unable to get audio input.");
        }
        else
        {
            ain.index = audioinput;
            if (ioctl(chanfd, VIDIOC_S_AUDIO, &ain) < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_WARN +
                        "Unable to set audio input.");
            }
        }
    }

    // query supported audio codecs if spdif is not used
    if (driver == "hdpvr" && audioinput != 2)
    {
        struct v4l2_queryctrl qctrl;
        qctrl.id = V4L2_CID_MPEG_AUDIO_ENCODING;

        if (!ioctl(chanfd, VIDIOC_QUERYCTRL, &qctrl))
        {
            uint audio_enc = max(min(audtype-1, qctrl.maximum), qctrl.minimum);
            add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_AUDIO_ENCODING, audio_enc);
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    "Unable to get supported audio codecs." + ENO);
        }
    }

    return true;
}

bool MpegRecorder::SetVBIOptions(int chanfd)
{
    if (!vbimode)
        return true;

    if (driver == "hdpvr" || driver == "saa7164")
        return true;

    if (has_buggy_vbi)
    {
        cout<<" *********************** WARNING ***********************"<<endl;
        cout<<" ivtv drivers prior to 0.10.0 can cause lockups when    "<<endl;
        cout<<" reading VBI. Drivers between 0.10.5 and 1.0.3+ do not  "<<endl;
        cout<<" properly capture VBI data on PVR-250 and PVR-350 cards."<<endl;
        cout<<endl;
    }

    /****************************************************************/
    /** First tell driver which services we want to capture.       **/

#ifdef IVTV_IOC_S_VBI_EMBED
    // used for ivtv driver versions 0.2.0-0.3.7
    if (!has_v4l2_vbi)
    {
        struct ivtv_sliced_vbi_format vbifmt;
        memset(&vbifmt, 0, sizeof(struct ivtv_sliced_vbi_format));
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
        memset(&vbifmt, 0, sizeof(struct v4l2_format));
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
        memset(&ctrls, 0, sizeof(struct v4l2_ext_controls));
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
        _error = true;
        return;
    }

    bool has_select = true;

#if defined(__FreeBSD__)
    // HACK. FreeBSD PVR150/500 driver doesn't currently support select()
    has_select = false;
#endif

    _start_code = 0xffffffff;
    _last_gop_seen = 0;
    _frames_written_count = 0;

    if (driver == "hdpvr")
    {
        int progNum = 1;
        MPEGStreamData *sd = new MPEGStreamData(progNum, true);
        sd->SetRecordingType(_recording_type);
        DTVRecorder::SetStreamData(sd);

        _stream_data->AddAVListener(this);
        _stream_data->AddWritingListener(this);

        // Make sure the first things in the file are a PAT & PMT
        _wait_for_keyframe_option = false;
        HandleSingleProgramPAT(_stream_data->PATSingleProgram());
        HandleSingleProgramPMT(_stream_data->PMTSingleProgram());
        _wait_for_keyframe_option = true;
    }

    {
        QMutexLocker locker(&recording_wait_lock);
        encoding = true;
        recording = true;
    }

    unsigned char *buffer = new unsigned char[bufferSize + 1];
    int len;
    int remainder = 0;

    MythTimer elapsedTimer;
    float elapsed;
    long long bytesRead = 0;
    int dummyBPS = 0;  // Bytes per second, but env var is BITS PER SECOND

    if (getenv("DUMMYBPS"))
    {
        dummyBPS = atoi(getenv("DUMMYBPS")) / 8;
        VERBOSE(VB_IMPORTANT, LOC + QString("Throttling dummy recorder to %1 "
                "bits per second").arg(dummyBPS * 8));
    }

    struct timeval tv;
    fd_set rdset;

    if (deviceIsMpegFile)
        elapsedTimer.start();
    else if (_device_read_buffer)
    {
        VERBOSE(VB_RECORD, LOC + "Initial startup of recorder");

        if (requires_special_pause && !StartEncoding(readfd))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to start recording");
            _error = true;
        }
        else
            _device_read_buffer->Start();
    }

    QByteArray vdevice = videodevice.toAscii();
    while (encoding && !_error)
    {
        if (PauseAndWait(100))
            continue;

        if (deviceIsMpegFile)
        {
            if (dummyBPS && bytesRead)
            {
                elapsed = (elapsedTimer.elapsed() / 1000.0) + 1;
                while ((bytesRead / elapsed) > dummyBPS)
                {
                    usleep(50000);
                    elapsed = (elapsedTimer.elapsed() / 1000.0) + 1;
                }
            }
            else if (GetFramesWritten())
            {
                elapsed = (elapsedTimer.elapsed() / 1000.0) + 1;
                while ((GetFramesWritten() / elapsed) > 30)
                {
                    usleep(50000);
                    elapsed = (elapsedTimer.elapsed() / 1000.0) + 1;
                }
            }
        }
        else
        {
            if (readfd < 0)
            {
                if (!Open())
                {
                    _error = true;
                    break;
                }

                if (readfd < 0)
                {
                    VERBOSE(VB_IMPORTANT, LOC_ERR +
                            QString("Failed to open device '%1'")
                            .arg(videodevice));
                    continue;
                }
            }
        }

        if (_device_read_buffer)
        {
            len = _device_read_buffer->Read(
                    &(buffer[remainder]), bufferSize - remainder);

            // Check for DRB errors
            if (_device_read_buffer->IsErrored())
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Device error detected");

                RestartEncoding();
            }
            else if (_device_read_buffer->IsEOF())
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Device EOF detected");
                _error = true;
            }
        }
        else
        {
            if (has_select)
            {
                tv.tv_sec = 5;
                tv.tv_usec = 0;
                FD_ZERO(&rdset);
                FD_SET(readfd, &rdset);

                switch (select(readfd + 1, &rdset, NULL, NULL, &tv))
                {
                    case -1:
                        if (errno == EINTR)
                            continue;

                        VERBOSE(VB_IMPORTANT, LOC_ERR + "Select error" + ENO);
                        continue;

                    case 0:
                        VERBOSE(VB_IMPORTANT, LOC_ERR + "select timeout - "
                                "driver has stopped responding");

                        if (close(readfd) != 0)
                        {
                            VERBOSE(VB_IMPORTANT, LOC_ERR +
                                    "Close error" + ENO);
                        }

                        // Force card to be reopened on next iteration..
                        readfd = -1;
                        continue;

                    default:
                        break;
                }
            }

            len = read(readfd, &(buffer[remainder]), bufferSize - remainder);

            if (len < 0 && !has_select)
            {
                usleep(25 * 1000);
                continue;
            }

            if ((len == 0) && (deviceIsMpegFile))
            {
                close(readfd);
                readfd = open(vdevice.constData(), O_RDONLY);

                if (readfd >= 0)
                {
                    len = read(readfd,
                               &(buffer[remainder]), bufferSize - remainder);
                }

                if (len <= 0)
                {
                    encoding = false;
                    continue;
                }
            }
            else if (len < 0 && errno != EAGAIN)
            {
                VERBOSE(VB_IMPORTANT,
                        LOC_ERR + QString("error reading from: %1")
                        .arg(videodevice) + ENO);
                continue;
            }
        }

        if (len > 0)
        {
            len += remainder;
            bytesRead += len;

            if (driver == "hdpvr") {
                remainder = _stream_data->ProcessData(buffer, len);
                int start_remain = len - remainder;
                if (remainder && (start_remain >= remainder))
                    memcpy(buffer, buffer+start_remain, remainder);
                else if (remainder)
                    memmove(buffer, buffer+start_remain, remainder);
            }
            else
            {
                FindPSKeyFrames(buffer, len);
            }
        }
    }

    VERBOSE(VB_RECORD, LOC + "StartRecording finishing up");

    if (_device_read_buffer)
    {
        if (_device_read_buffer->IsRunning())
            _device_read_buffer->Stop();

        delete _device_read_buffer;
        _device_read_buffer = NULL;
    }
    if (requires_special_pause)
        StopEncoding(readfd);

    FinishRecording();

    delete[] buffer;
    DTVRecorder::SetStreamData(NULL);
    encoding = false;

    QMutexLocker locker(&recording_wait_lock);
    recording = false;
    recording_wait.wakeAll();
}

bool MpegRecorder::ProcessTSPacket(const TSPacket &tspacket_real)
{
    const uint pid = tspacket_real.PID();

    TSPacket *tspacket_fake = NULL;
    if ((driver == "hdpvr") && (pid == 0x1001)) // PCRPID for HD-PVR
    {
        tspacket_fake = tspacket_real.CreateClone();
        uint cc = (_continuity_counter[pid] == 0xFF) ?
            0 : (_continuity_counter[pid] + 1) & 0xf;
        tspacket_fake->SetContinuityCounter(cc);
    }

    const TSPacket *tspacket = (tspacket_fake) ?
        tspacket_fake : &tspacket_real;

    // Check continuity counter
    if ((pid != 0x1fff) && !CheckCC(pid, tspacket->ContinuityCounter()))
    {
        VERBOSE(VB_RECORD, LOC +
                QString("PID 0x%1 discontinuity detected").arg(pid,0,16));
        _continuity_error_count++;
    }

    // Only write the packet
    // if audio/video key-frames have been found
    if (!(_wait_for_keyframe_option && _first_keyframe < 0))
    {
        _buffer_packets = true;

        BufferedWrite(*tspacket);
    }

    if (tspacket_fake)
        delete tspacket_fake;

    return true;
}

bool MpegRecorder::ProcessVideoTSPacket(const TSPacket &tspacket)
{
    if (!ringBuffer)
        return true;

    _buffer_packets = !FindH264Keyframes(&tspacket);
    if (!_seen_sps)
        return true;

    return ProcessAVTSPacket(tspacket);
}

bool MpegRecorder::ProcessAudioTSPacket(const TSPacket &tspacket)
{
    if (!ringBuffer)
        return true;

    _buffer_packets = !FindAudioKeyframes(&tspacket);
    return ProcessAVTSPacket(tspacket);
}

/// Common code for processing either audio or video packets
bool MpegRecorder::ProcessAVTSPacket(const TSPacket &tspacket)
{
    const uint pid = tspacket.PID();

    // Check continuity counter
    if ((pid != 0x1fff) && !CheckCC(pid, tspacket.ContinuityCounter()))
    {
        VERBOSE(VB_RECORD, LOC +
                QString("PID 0x%1 discontinuity detected").arg(pid,0,16));
        _continuity_error_count++;
    }

    // Sync recording start to first keyframe
    if (_wait_for_keyframe_option && _first_keyframe < 0)
        return true;

    // Sync streams to the first Payload Unit Start Indicator
    // _after_ first keyframe iff _wait_for_keyframe_option is true
    if (!(_pid_status[pid] & kPayloadStartSeen) && tspacket.HasPayload())
    {
        if (!tspacket.PayloadStart())
            return true; // not payload start - drop packet

        VERBOSE(VB_RECORD,
                QString("PID 0x%1 Found Payload Start").arg(pid,0,16));

        _pid_status[pid] |= kPayloadStartSeen;
    }

    BufferedWrite(tspacket);

    return true;
}

void MpegRecorder::StopRecording(void)
{
    QMutexLocker locker(&recording_wait_lock);
    if (recording)
    {
        encoding = false; // force exit from StartRecording() while loop
        recording_wait.wait(&recording_wait_lock);
    }
}

void MpegRecorder::ResetForNewFile(void)
{
    DTVRecorder::ResetForNewFile();

    memset(_stream_id, 0,  sizeof(_stream_id));
    memset(_pid_status, 0, sizeof(_pid_status));
    memset(_continuity_counter, 0xff, sizeof(_continuity_counter));

    if (driver == "hdpvr")
    {
        m_h264_parser.Reset();
        _wait_for_keyframe_option = true;
    }
}

void MpegRecorder::Reset(void)
{
    VERBOSE(VB_RECORD, LOC + "Reset(void)");
    ResetForNewFile();

    _start_code = 0xffffffff;

    if (curRecording)
    {
        curRecording->ClearPositionMap(MARK_GOP_BYFRAME);
    }
    if (_stream_data)
        _stream_data->Reset(_stream_data->DesiredProgram());
}

void MpegRecorder::Pause(bool clear)
{
    cleartimeonpause = clear;
    paused = false;
    request_pause = true;
}

bool MpegRecorder::PauseAndWait(int timeout)
{
    if (request_pause)
    {
        QMutex waitlock;
        waitlock.lock();

        if (!paused)
        {
            VERBOSE(VB_RECORD, LOC + "PauseAndWait pause");

            if (_device_read_buffer)
            {
                QMutex drb_lock;
                drb_lock.lock();
                _device_read_buffer->SetRequestPause(true);
                _device_read_buffer->WaitForPaused(4000);
            }

            // Some drivers require streaming to be disabled before
            // an input switch and other channel format setting.
            if (requires_special_pause)
                StopEncoding(readfd);

            paused = true;
            pauseWait.wakeAll();

            if (tvrec)
                tvrec->RecorderPaused();
        }

        unpauseWait.wait(&waitlock, timeout);
    }

    if (!request_pause && paused)
    {
        VERBOSE(VB_RECORD, LOC + "PauseAndWait unpause");

        if (driver == "hdpvr")
        {
            m_h264_parser.Reset();
            _wait_for_keyframe_option = true;
            _seen_sps = false;
        }

        // Some drivers require streaming to be disabled before
        // an input switch and other channel format setting.
        if (requires_special_pause)
            StartEncoding(readfd);

        if (_device_read_buffer)
            _device_read_buffer->SetRequestPause(false);

        if (_stream_data)
            _stream_data->Reset(_stream_data->DesiredProgram());

        paused = false;
    }
    return paused;
}

void MpegRecorder::RestartEncoding(void)
{
    VERBOSE(VB_RECORD, LOC + "RestartEncoding");

    _device_read_buffer->Stop();

    QMutexLocker locker(&start_stop_encoding_lock);

    if (requires_special_pause)
        StopEncoding(readfd);

    // Make sure the next things in the file are a PAT & PMT
    if (_stream_data &&
        _stream_data->PATSingleProgram() &&
        _stream_data->PMTSingleProgram())
    {
        _wait_for_keyframe_option = false;
        HandleSingleProgramPAT(_stream_data->PATSingleProgram());
        HandleSingleProgramPMT(_stream_data->PMTSingleProgram());
    }

    if (requires_special_pause && !StartEncoding(readfd))
    {
        if (0 != close(readfd))
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Close error" + ENO);

        readfd = -1;
        return;
    }

    _device_read_buffer->Start();
}

bool MpegRecorder::StartEncoding(int fd)
{
    QMutexLocker locker(&start_stop_encoding_lock);

    struct v4l2_encoder_cmd command;
    memset(&command, 0, sizeof(struct v4l2_encoder_cmd));
    command.cmd = V4L2_ENC_CMD_START;

    if (driver == "hdpvr")
        HandleResolutionChanges();

    VERBOSE(VB_RECORD, LOC + "StartEncoding");

    if (ioctl(fd, VIDIOC_ENCODER_CMD, &command) == 0)
    {
        if (driver == "hdpvr")
        {
            m_h264_parser.Reset();
            _wait_for_keyframe_option = true;
            _seen_sps = false;
        }

        VERBOSE(VB_RECORD, LOC + "Encoding started");
        return true;
    }

    VERBOSE(VB_IMPORTANT, LOC_ERR + "StartEncoding failed" + ENO);
    return false;
}

bool MpegRecorder::StopEncoding(int fd)
{
    QMutexLocker locker(&start_stop_encoding_lock);

    struct v4l2_encoder_cmd command;
    memset(&command, 0, sizeof(struct v4l2_encoder_cmd));
    command.cmd = V4L2_ENC_CMD_STOP;

    VERBOSE(VB_RECORD, LOC + "StopEncoding");

    if (ioctl(fd, VIDIOC_ENCODER_CMD, &command) == 0)
    {
        VERBOSE(VB_RECORD, LOC + "Encoding stopped");
        return true;
    }

    VERBOSE(VB_IMPORTANT, LOC_ERR + "StopEncoding failed" + ENO);
    return false;
}

void MpegRecorder::SetStreamData(void)
{
    _stream_data->AddMPEGSPListener(this);
    _stream_data->SetDesiredProgram(1);
}

void MpegRecorder::HandleSingleProgramPAT(ProgramAssociationTable *pat)
{
    if (!pat)
    {
        VERBOSE(VB_RECORD, LOC + "HandleSingleProgramPAT(NULL)");
        return;
    }

    if (!ringBuffer)
        return;


    uint next_cc = (pat->tsheader()->ContinuityCounter()+1)&0xf;
    pat->tsheader()->SetContinuityCounter(next_cc);
    pat->GetAsTSPackets(_scratch, next_cc);

    for (uint i = 0; i < _scratch.size(); i++)
        DTVRecorder::BufferedWrite(_scratch[i]);
}

void MpegRecorder::HandleSingleProgramPMT(ProgramMapTable *pmt)
{
    if (!pmt)
    {
        return;
    }

    // collect stream types for H.264 (MPEG-4 AVC) keyframe detection
    for (uint i = 0; i < pmt->StreamCount(); i++)
        _stream_id[pmt->StreamPID(i)] = pmt->StreamType(i);

    if (!ringBuffer)
        return;

    uint next_cc = (pmt->tsheader()->ContinuityCounter()+1)&0xf;
    pmt->tsheader()->SetContinuityCounter(next_cc);
    pmt->GetAsTSPackets(_scratch, next_cc);

    for (uint i = 0; i < _scratch.size(); i++)
        DTVRecorder::BufferedWrite(_scratch[i]);
}

void MpegRecorder::SetBitrate(int bitrate, int maxbitrate,
                              const QString & reason)
{
    if (maxbitrate == bitrate)
    {
        VERBOSE(VB_RECORD, LOC + QString("%1 bitrate %2 kbps CBR")
                .arg(reason).arg(bitrate));
    }
    else
    {
        VERBOSE(VB_RECORD, LOC + QString("%1 bitrate %2/%3 kbps VBR")
                .arg(reason).arg(bitrate).arg(maxbitrate));
    }

    vector<struct v4l2_ext_control> ext_ctrls;
    add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
                 (maxbitrate == bitrate) ?
                 V4L2_MPEG_VIDEO_BITRATE_MODE_CBR :
                 V4L2_MPEG_VIDEO_BITRATE_MODE_VBR);

    add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_VIDEO_BITRATE,
                 bitrate * 1000);

    add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_VIDEO_BITRATE_PEAK,
                 maxbitrate * 1000);

    set_ctrls(readfd, ext_ctrls);
}

void MpegRecorder::HandleResolutionChanges(void)
{
    VERBOSE(VB_RECORD, LOC + "Checking Resolution");
    uint pix = 0;
    struct v4l2_format vfmt;
    memset(&vfmt, 0, sizeof(vfmt));
    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == ioctl(chanfd, VIDIOC_G_FMT, &vfmt))
    {
        VERBOSE(VB_RECORD, LOC + QString("Got Resolution %1x%2")
                .arg(vfmt.fmt.pix.width).arg(vfmt.fmt.pix.height));
        pix = vfmt.fmt.pix.width * vfmt.fmt.pix.height;
    }

    if (!pix)
    {
        VERBOSE(VB_RECORD, LOC + "Giving up detecting resolution");
        return; // nothing to do, we don't have a resolution yet
    }

    int old_max = maxbitrate, old_avg = bitrate;
    if (pix <= 768*568)
    {
        maxbitrate = low_mpeg4peakbitrate;
        bitrate    = low_mpeg4avgbitrate;
    }
    else if (pix >= 1920*1080)
    {
        maxbitrate = high_mpeg4peakbitrate;
        bitrate    = high_mpeg4avgbitrate;
    }
    else
    {
        maxbitrate = medium_mpeg4peakbitrate;
        bitrate    = medium_mpeg4avgbitrate;
    }
    maxbitrate = std::max(maxbitrate, bitrate);

    if ((old_max != maxbitrate) || (old_avg != bitrate))
    {
        if (old_max == old_avg)
        {
            VERBOSE(VB_RECORD, LOC +
                    QString("Old bitrate %1 CBR").arg(old_avg));
        }
        else
        {
            VERBOSE(VB_RECORD, LOC +
                    QString("Old bitrate %1/%2 VBR")
                    .arg(old_avg).arg(old_max));
        }

        SetBitrate(bitrate, maxbitrate, "New");
    }
}
