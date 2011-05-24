// -*- Mode: c++ -*-

// C headers
#include <ctime>

// C++ headers
#include <algorithm>
#include <vector>
using namespace std;

// POSIX headers
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
    V4LRecorder(rec),
    // Debugging variables
    deviceIsMpegFile(false),      bufferSize(0),
    // Driver info
    card(QString::null),          driver(QString::null),
    version(0),
    supports_sliced_vbi(false),
    // State
    start_stop_encoding_lock(QMutex::Recursive),
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
    _device_read_buffer(NULL)
{
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
            V4LRecorder::SetOption(opt, value);
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
            V4LRecorder::SetOption(opt, value);
    }
    else
        V4LRecorder::SetOption(opt, value);
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
        V4LRecorder::SetOption(opt, value);
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
    SetOption("vbidevice", vbidev);
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

    bufferSize = 4096;

    bool supports_tuner = false, supports_audio = false;
    uint32_t capabilities = 0;
    if (CardUtil::GetV4LInfo(chanfd, card, driver, version, capabilities))
    {
        supports_sliced_vbi = !!(capabilities & V4L2_CAP_SLICED_VBI_CAPTURE);
        supports_tuner      = !!(capabilities & V4L2_CAP_TUNER);
        supports_audio      = !!(capabilities & V4L2_CAP_AUDIO);
        /// Determine hacks needed for specific drivers & driver versions
        if (driver == "hdpvr")
        {
            bufferSize = 1500 * TSPacket::kSize;
            m_h264_parser.use_I_forKeyframes(false);
        }
    }

    if (!(capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "V4L version 1, unsupported");
        close(chanfd);
        chanfd = -1;
        return false;
    }

    if (!SetVideoCaptureFormat(chanfd))
    {
        close(chanfd);
        chanfd = -1;
        return false;
    }

    if (supports_tuner)
        SetLanguageMode(chanfd);    // we don't care if this fails...

    if (supports_audio)
        SetRecordingVolume(chanfd); // we don't care if this fails...

    if (!SetV4L2DeviceOptions(chanfd))
        return false;

    SetVBIOptions(chanfd); // we don't care if this fails...

    readfd = open(vdevice.constData(), O_RDWR | O_NONBLOCK);

    if (readfd < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Can't open video device." + ENO);
        close(chanfd);
        chanfd = -1;
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
        close(chanfd);
        chanfd = -1;
        close(readfd);
        readfd = -1;
        return false;
    }

    if (!_device_read_buffer->Setup(vdevice.constData(), readfd))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to allocate DRB buffer");
        _error = true;
        close(chanfd);
        chanfd = -1;
        close(readfd);
        readfd = -1;
        return false;
    }

    VERBOSE(VB_RECORD, LOC + "DRB ready");

    if (vbi_fd >= 0)
        vbi_thread = new VBIThread(this);

    return true;
}


bool MpegRecorder::SetVideoCaptureFormat(int chanfd)
{
    if (driver == "hdpvr")
        return true;

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
            vt.audmode = V4L2_TUNER_MODE_LANG1_LANG2;
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
    memset(&qctrl, 0 , sizeof(struct v4l2_queryctrl));
    qctrl.id = V4L2_CID_AUDIO_VOLUME;
    if ((ioctl(chanfd, VIDIOC_QUERYCTRL, &qctrl) < 0) ||
        (qctrl.flags & V4L2_CTRL_FLAG_DISABLED))
    {
        VERBOSE(VB_CHANNEL, LOC_WARN + "Audio volume control not supported.");
        return false;
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
        if (driver.left(7) != "saa7164")
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

    if (driver == "hdpvr" || driver.left(7) == "saa7164")
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
    if (VBIMode::None == vbimode)
        return true;

    if (driver == "hdpvr")
        return true;

#ifdef V4L2_CAP_SLICED_VBI_CAPTURE
    if (supports_sliced_vbi)
    {
        struct v4l2_format vbifmt;
        memset(&vbifmt, 0, sizeof(struct v4l2_format));
        vbifmt.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
        vbifmt.fmt.sliced.service_set |= (VBIMode::PAL_TT == vbimode) ?
            V4L2_SLICED_VBI_625 : V4L2_SLICED_VBI_525;

        if (ioctl(chanfd, VIDIOC_S_FMT, &vbifmt) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    "Unable to enable VBI embedding" + ENO);
        }
        else if (ioctl(chanfd, VIDIOC_G_FMT, &vbifmt) >= 0)
        {
            VERBOSE(VB_RECORD, LOC + QString("VBI service: %1, io size: %2")
                    .arg(vbifmt.fmt.sliced.service_set)
                    .arg(vbifmt.fmt.sliced.io_size));

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
                        "Unable to set VBI embedding format" + ENO);
            }
            else
            {
                return true;
            }
        }
    }
#endif // V4L2_CAP_SLICED_VBI_CAPTURE

    return OpenVBIDevice() >= 0;
}

bool MpegRecorder::Open(void)
{
    memset(_stream_id,  0, sizeof(_stream_id));
    memset(_pid_status, 0, sizeof(_pid_status));
    memset(_continuity_counter, 0xff, sizeof(_continuity_counter));
    return (deviceIsMpegFile) ? OpenMpegFileAsInput() : OpenV4L2DeviceAsInput();
}

void MpegRecorder::StartRecording(void)
{
    if (!Open())
    {
        _error = "Failed to open V4L device";
        return;
    }

    bool has_select = true;

#if defined(__FreeBSD__)
    // HACK. FreeBSD PVR150/500 driver doesn't currently support select()
    has_select = false;
#endif

    _continuity_error_count = 0;
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
        QMutexLocker locker(&pauseLock);
        request_recording = true;
        recording = true;
        recordingWait.wakeAll();
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

        if (!StartEncoding(readfd))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to start recording");
            _error = true;
        }
        else
            _device_read_buffer->Start();
    }

    QByteArray vdevice = videodevice.toAscii();
    while (IsRecordingRequested() && !IsErrored())
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
            else if (_device_read_buffer->IsEOF() &&
                     IsRecordingRequested())
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
                QMutexLocker locker(&pauseLock);
                if (request_recording && !request_pause)
                    unpauseWait.wait(&pauseLock, 25);
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
                    _error = "Failed to read from video file";
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
            bytesRead += len;
            len += remainder;

            if (driver == "hdpvr")
            {
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

    StopEncoding(readfd);

    FinishRecording();

    delete[] buffer;

    if (driver == "hdpvr")
    {
        _stream_data->RemoveWritingListener(this);
        _stream_data->RemoveAVListener(this);
        DTVRecorder::SetStreamData(NULL);
    }

    QMutexLocker locker(&pauseLock);
    recording = false;
    recordingWait.wakeAll();
}

void MpegRecorder::StopRecording(void)
{
    if (_device_read_buffer && _device_read_buffer->IsRunning())
        _device_read_buffer->Stop();
    V4LRecorder::StopRecording();
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

    const TSPacket &tspacket = (tspacket_fake)
        ? *tspacket_fake : tspacket_real;

    bool ret = DTVRecorder::ProcessTSPacket(tspacket);

    if (tspacket_fake)
        delete tspacket_fake;

    return ret;
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
    QMutexLocker locker(&pauseLock);
    cleartimeonpause = clear;
    paused = false;
    request_pause = true;
}

bool MpegRecorder::PauseAndWait(int timeout)
{
    QMutexLocker locker(&pauseLock);
    if (request_pause)
    {
        if (!IsPaused(true))
        {
            VERBOSE(VB_RECORD, LOC + "PauseAndWait pause");

            if (_device_read_buffer)
            {
                _device_read_buffer->SetRequestPause(true);
                _device_read_buffer->WaitForPaused(4000);
            }

            StopEncoding(readfd);

            paused = true;
            pauseWait.wakeAll();

            if (tvrec)
                tvrec->RecorderPaused();
        }

        unpauseWait.wait(&pauseLock, timeout);
    }

    if (!request_pause && IsPaused(true))
    {
        VERBOSE(VB_RECORD, LOC + "PauseAndWait unpause");

        if (driver == "hdpvr")
        {
            m_h264_parser.Reset();
            _wait_for_keyframe_option = true;
            _seen_sps = false;
        }

        StartEncoding(readfd);

        if (_device_read_buffer)
            _device_read_buffer->SetRequestPause(false);

        if (_stream_data)
            _stream_data->Reset(_stream_data->DesiredProgram());

        paused = false;
    }

    return IsPaused(true);
}

void MpegRecorder::RestartEncoding(void)
{
    VERBOSE(VB_RECORD, LOC + "RestartEncoding");

    _device_read_buffer->Stop();

    QMutexLocker locker(&start_stop_encoding_lock);

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

    if (!StartEncoding(readfd))
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

    // Some drivers do not support this ioctl at all.  It is marked as 
    // "experimental" in the V4L2 API spec.  If we fail with EINVAL (which
    // happens if the ioctl isn't supported), treat it as a success, but
    // put a warning in the logs.  This should keep any driver without this
    // support (such as saa7164) recording without affecting those that do
    // use it.
    VERBOSE(VB_IMPORTANT, LOC_WARN + "StartEncoding failed" + ENO);
    return (errno == EINVAL);
}

bool MpegRecorder::StopEncoding(int fd)
{
    QMutexLocker locker(&start_stop_encoding_lock);

    struct v4l2_encoder_cmd command;
    memset(&command, 0, sizeof(struct v4l2_encoder_cmd));
    command.cmd   = V4L2_ENC_CMD_STOP;
    command.flags = V4L2_ENC_CMD_STOP_AT_GOP_END;

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

void MpegRecorder::FormatCC(uint code1, uint code2)
{
    VERBOSE(VB_VBI, LOC + QString("FormatCC(0x%1,0x%2)")
            .arg(code1,0,16).arg(code2,0,16));
    // TODO add to CC data vector

    // TODO find video frames in output and insert cc_data
    // as CEA-708 user_data packets containing CEA-608 captions
    ///at picture data level
}
