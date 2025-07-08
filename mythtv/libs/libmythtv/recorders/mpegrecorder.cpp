// -*- Mode: c++ -*-

// C++ headers
#include <algorithm>
#include <chrono> // for milliseconds
#include <cinttypes>
#include <fcntl.h>
#include <thread> // for sleep_for
#include <unistd.h>
#include <vector>

// System headers
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/poll.h>

#include <linux/videodev2.h>

#include "libmythbase/mythconfig.h"

// MythTV headers
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/programinfo.h"

#include "cardutil.h"
#include "io/mythmediabuffer.h"
#include "mpegrecorder.h"
#include "recordingprofile.h"
#include "tv_rec.h"

#define LOC QString("MPEGRec[%1](%2): ") \
            .arg(m_tvrec ? m_tvrec->GetInputId() : -1).arg(m_videodevice)

const std::array<const int,14> MpegRecorder::kAudRateL1
{
    32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448
};

const std::array<const int,14> MpegRecorder::kAudRateL2
{
    32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384
};

const std::array<const int,14> MpegRecorder::kAudRateL3
{
    32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320
};

const std::array<const std::string,15> MpegRecorder::kStreamType
{
    "MPEG-2 PS", "MPEG-2 TS",     "MPEG-1 VCD",    "PES AV",
    "",          "PES V",          "",             "PES A",
    "",          "",              "DVD",           "VCD",
    "SVCD",      "DVD-Special 1", "DVD-Special 2"
};

const std::array<const std::string,4> MpegRecorder::kAspectRatio
{
    "Square", "4:3", "16:9", "2.21:1"
};

void MpegRecorder::TeardownAll(void)
{
    StopRecording();

    if (m_chanfd >= 0)
    {
        close(m_chanfd);
        m_chanfd = -1;
    }
    if (m_readfd >= 0)
    {
        close(m_readfd);
        m_readfd = -1;
    }

    if (m_deviceReadBuffer)
    {
        if (m_deviceReadBuffer->IsRunning())
            m_deviceReadBuffer->Stop();

        delete m_deviceReadBuffer;
        m_deviceReadBuffer = nullptr;
    }

}

static int find_index(const std::array<const int,14> &audio_rate, int value)
{
    for (uint i = 0; i < audio_rate.size(); i++)
    {
        if (audio_rate[i] == value)
            return i;
    }

    return -1;
}

void MpegRecorder::SetOption(const QString &opt, int value)
{
    if (opt == "width")
        m_width = value;
    else if (opt == "height")
        m_height = value;
    else if (opt == "mpeg2bitrate")
        m_bitrate = value;
    else if (opt == "mpeg2maxbitrate")
        m_maxBitrate = value;
    else if (opt == "samplerate")
        m_audSampleRate = value;
    else if (opt == "mpeg2audbitratel1")
    {
        int index = find_index(kAudRateL1, value);
        if (index >= 0)
            m_audBitrateL1 = index + 1;
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Audiorate(L1): " +
                QString("%1 is invalid").arg(value));
        }
    }
    else if (opt == "mpeg2audbitratel2")
    {
        int index = find_index(kAudRateL2, value);
        if (index >= 0)
            m_audBitrateL2 = index + 1;
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Audiorate(L2): " +
                    QString("%1 is invalid").arg(value));
        }
    }
    else if (opt == "mpeg2audbitratel3")
    {
        int index = find_index(kAudRateL3, value);
        if (index >= 0)
            m_audBitrateL3 = index + 1;
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Audiorate(L2): " +
                    QString("%1 is invalid").arg(value));
        }
    }
    else if (opt == "mpeg2audvolume")
    {
        m_audVolume = value;
    }
    else if (opt.endsWith("_mpeg4avgbitrate"))
    {
        if (opt.startsWith("low"))
            m_lowMpeg4AvgBitrate = value;
        else if (opt.startsWith("medium"))
            m_mediumMpeg4AvgBitrate = value;
        else if (opt.startsWith("high"))
            m_highMpeg4AvgBitrate = value;
        else
            V4LRecorder::SetOption(opt, value);
    }
    else if (opt.endsWith("_mpeg4peakbitrate"))
    {
        if (opt.startsWith("low"))
            m_lowMpeg4PeakBitrate = value;
        else if (opt.startsWith("medium"))
            m_mediumMpeg4PeakBitrate = value;
        else if (opt.startsWith("high"))
            m_highMpeg4PeakBitrate = value;
        else
            V4LRecorder::SetOption(opt, value);
    }
    else
    {
        V4LRecorder::SetOption(opt, value);
    }
}

void MpegRecorder::SetOption(const QString &opt, const QString &value)
{
    std::string value_ss = value.toStdString();
    if (opt == "mpeg2streamtype")
    {
        bool found = false;
        for (size_t i = 0; i < kStreamType.size(); i++)
        {
            if (kStreamType[i] == value_ss)
            {
                if (kStreamType[i] == "MPEG-2 TS")
                {
                     m_containerFormat = formatMPEG2_TS;
                }
                else if (kStreamType[i] == "MPEG-2 PS")
                {
                     m_containerFormat = formatMPEG2_PS;
                }
                else
                {
                    // TODO Expand AVContainer to include other types in
                    //      streamType
                    m_containerFormat = formatUnknown;
                }
                m_streamType = i;
                found = true;
                break;
            }
        }

        if (!found)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "MPEG2 stream type: " +
                    QString("%1 is invalid").arg(value));
        }
    }
    else if (opt == "mpeg2language")
    {
        bool ok = false;
        m_language = value.toInt(&ok); // on failure language will be 0
        if (!ok)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "MPEG2 language (stereo) flag " +
                    QString("'%1' is invalid").arg(value));
        }
    }
    else if (opt == "mpeg2aspectratio")
    {
        bool found = false;
        for (size_t i = 0; i < kAspectRatio.size(); i++)
        {
            if (kAspectRatio[i] == value_ss)
            {
                m_aspectRatio = i + 1;
                found = true;
                break;
            }
        }

        if (!found)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "MPEG2 Aspect-ratio: " +
                    QString("%1 is invalid").arg(value));
        }
    }
    else if (opt == "mpeg2audtype")
    {
        if (value == "Layer I")
            m_audType = V4L2_MPEG_AUDIO_ENCODING_LAYER_1 + 1;
        else if (value == "Layer II")
            m_audType = V4L2_MPEG_AUDIO_ENCODING_LAYER_2 + 1;
        else if (value == "Layer III")
            m_audType = V4L2_MPEG_AUDIO_ENCODING_LAYER_3 + 1;
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "MPEG2 audio layer: " +
                    QString("%1 is invalid").arg(value));
        }
    }
    else if (opt == "audiocodec")
    {
        if (value == "AAC Hardware Encoder")
            m_audType = V4L2_MPEG_AUDIO_ENCODING_AAC + 1;
        else if (value == "AC3 Hardware Encoder")
            m_audType = V4L2_MPEG_AUDIO_ENCODING_AC3 + 1;
    }
    else
    {
        V4LRecorder::SetOption(opt, value);
    }
}

void MpegRecorder::SetOptionsFromProfile(RecordingProfile *profile,
                                         const QString &videodev,
                                         [[maybe_unused]] const QString &audiodev,
                                         [[maybe_unused]] const QString &vbidev)
{
    if (videodev.startsWith("file:", Qt::CaseInsensitive))
    {
        m_deviceIsMpegFile = true;
        m_bufferSize = 64000;
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
    const StandardSetting *setting = profile->byName(name);
    if (setting)
        SetOption(name, setting->getValue().toInt());
}

// same as the base class, it just doesn't complain if an option is missing
void MpegRecorder::SetStrOption(RecordingProfile *profile, const QString &name)
{
    const StandardSetting *setting = profile->byName(name);
    if (setting)
        SetOption(name, setting->getValue());
}

bool MpegRecorder::OpenMpegFileAsInput(void)
{
    QByteArray vdevice = m_videodevice.toLatin1();
    m_chanfd = m_readfd = open(vdevice.constData(), O_RDONLY);

    if (m_readfd < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Can't open MPEG File '%1'")
                .arg(m_videodevice) + ENO);
        m_error = LOC + QString("Can't open MPEG File '%1'")
                 .arg(m_videodevice) + ENO;
        return false;
    }
    return true;
}

bool MpegRecorder::OpenV4L2DeviceAsInput(void)
{
    // open implicitly starts encoding, so we need the lock..
    QMutexLocker locker(&m_startStopEncodingLock);

    QByteArray vdevice = m_videodevice.toLatin1();
    m_chanfd = open(vdevice.constData(), O_RDWR);
    if (m_chanfd < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Can't open video device. " + ENO);
        m_error = LOC + "Can't open video device. " + ENO;
        return false;
    }

    m_bufferSize = 4096;

    bool supports_tuner = false;
    bool supports_audio = false;
    uint32_t capabilities = 0;
    if (CardUtil::GetV4LInfo(m_chanfd, m_card, m_driver, m_version, capabilities))
    {
        m_supportsSlicedVbi   = ((capabilities & V4L2_CAP_SLICED_VBI_CAPTURE) != 0U);
        supports_tuner        = ((capabilities & V4L2_CAP_TUNER) != 0U);
        supports_audio        = ((capabilities & V4L2_CAP_AUDIO) != 0U);
        /// Determine hacks needed for specific drivers & driver versions
        if (m_driver == "hdpvr")
        {
            m_bufferSize = 1500 * TSPacket::kSize;
            m_useIForKeyframe = false;
        }
    }

    if (!(capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "V4L version 1, unsupported");
        m_error = LOC + "V4L version 1, unsupported";
        close(m_chanfd);
        m_chanfd = -1;
        return false;
    }

    if (!SetVideoCaptureFormat(m_chanfd))
    {
        close(m_chanfd);
        m_chanfd = -1;
        return false;
    }

    if (supports_tuner)
        SetLanguageMode(m_chanfd);    // we don't care if this fails...

    if (supports_audio)
        SetRecordingVolume(m_chanfd); // we don't care if this fails...

    if (!SetV4L2DeviceOptions(m_chanfd))
    {
        close(m_chanfd);
        m_chanfd = -1;
        return false;
    }

    SetVBIOptions(m_chanfd); // we don't care if this fails...

    m_readfd = open(vdevice.constData(), O_RDWR | O_NONBLOCK);

    if (m_readfd < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Can't open video device." + ENO);
        m_error = LOC + "Can't open video device." + ENO;
        close(m_chanfd);
        m_chanfd = -1;
        return false;
    }

    if (m_deviceReadBuffer)
    {
        if (m_deviceReadBuffer->IsRunning())
            m_deviceReadBuffer->Stop();

        delete m_deviceReadBuffer;
        m_deviceReadBuffer = nullptr;
    }

    m_deviceReadBuffer = new DeviceReadBuffer(this);

    if (!m_deviceReadBuffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to allocate DRB buffer");
        m_error = "Failed to allocate DRB buffer";
        close(m_chanfd);
        m_chanfd = -1;
        close(m_readfd);
        m_readfd = -1;
        return false;
    }

    if (!m_deviceReadBuffer->Setup(vdevice.constData(), m_readfd))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to setup DRB buffer");
        m_error = "Failed to setup DRB buffer";
        close(m_chanfd);
        m_chanfd = -1;
        close(m_readfd);
        m_readfd = -1;
        return false;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "DRB ready");

    if (m_vbiFd >= 0)
        m_vbiThread = new VBIThread(this);

    return true;
}


bool MpegRecorder::SetVideoCaptureFormat(int chanfd)
{
    if (m_driver == "hdpvr")
        return true;

    struct v4l2_format vfmt {};

    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(chanfd, VIDIOC_G_FMT, &vfmt) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Error getting format" + ENO);
        m_error = LOC + "Error getting format" + ENO;
        return false;
    }

    vfmt.fmt.pix.width = m_width;
    vfmt.fmt.pix.height = m_height;

    if (ioctl(chanfd, VIDIOC_S_FMT, &vfmt) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Error setting format" + ENO);
        m_error = LOC + "Error setting format" + ENO;
        return false;
    }

    return true;
}

/// Set audio language mode
bool MpegRecorder::SetLanguageMode(int chanfd)
{
    struct v4l2_tuner vt {};
    if (ioctl(chanfd, VIDIOC_G_TUNER, &vt) < 0)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Unable to get audio mode" + ENO);
        return false;
    }

    switch (m_language)
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
    if ((2 == m_language) && (1 == audio_layer))
    {
        LOG(VB_GENERAL, LOG_WARNING,
            "Dual audio mode incompatible with Layer I audio."
            "\n\t\t\tFalling back to Main Language");
        vt.audmode = V4L2_TUNER_MODE_LANG1;
        success = false;
    }

    if (ioctl(chanfd, VIDIOC_S_TUNER, &vt) < 0)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Unable to set audio mode" + ENO);
        success = false;
    }

    return success;
}

bool MpegRecorder::SetRecordingVolume(int chanfd)
{
    // Get volume min/max values
    struct v4l2_queryctrl qctrl {};
    qctrl.id = V4L2_CID_AUDIO_VOLUME;
    if ((ioctl(chanfd, VIDIOC_QUERYCTRL, &qctrl) < 0) ||
        (qctrl.flags & V4L2_CTRL_FLAG_DISABLED))
    {
        LOG(VB_CHANNEL, LOG_WARNING,
            LOC + "Audio volume control not supported.");
        return false;
    }

    // calculate volume in card units.
    int range = qctrl.maximum - qctrl.minimum;
    int value = (int) ((range * m_audVolume * 0.01F) + qctrl.minimum);
    int ctrl_volume = std::clamp(value, qctrl.minimum, qctrl.maximum);

    // Set recording volume
    struct v4l2_control ctrl {V4L2_CID_AUDIO_VOLUME, ctrl_volume};

    if (ioctl(chanfd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Unable to set recording volume" + ENO + "\n\t\t\t" +
            "If you are using an AverMedia M179 card this is normal.");
        return false;
    }

    return true;
}

uint MpegRecorder::GetFilteredStreamType(void) const
{
    uint st = (uint) m_streamType;

    if (m_driver == "ivtv")
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

    if (st != (uint) m_streamType)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Stream type '%1'\n\t\t\t"
                    "is not supported by %2 driver, using '%3' instead.")
                .arg(QString::fromStdString(kStreamType[m_streamType]),
                     m_driver,
                     QString::fromStdString(kStreamType[st])));
    }

    return st;
}

uint MpegRecorder::GetFilteredAudioSampleRate(void) const
{
    uint sr = (uint) m_audSampleRate;

    sr = (m_driver == "ivtv") ? 48000 : sr; // only 48kHz works properly.

    if (sr != (uint) m_audSampleRate)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Audio sample rate %1 Hz\n\t\t\t"
                    "is not supported by %2 driver, using %3 Hz instead.")
                .arg(m_audSampleRate).arg(m_driver).arg(sr));
    }

    switch (sr)
    {
        case 32000: return V4L2_MPEG_AUDIO_SAMPLING_FREQ_32000;
        case 44100: return V4L2_MPEG_AUDIO_SAMPLING_FREQ_44100;
        case 48000:
        default:    return V4L2_MPEG_AUDIO_SAMPLING_FREQ_48000;
    }
}

uint MpegRecorder::GetFilteredAudioLayer(void) const
{
    uint layer = (uint) m_audType;

    layer = std::clamp(layer, 1U, 3U);

    layer = (m_driver == "ivtv") ? 2 : layer;

    if (layer != (uint) m_audType)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("MPEG layer %1 does not work properly\n\t\t\t"
                    "with %2 driver. Using MPEG layer %3 audio instead.")
                .arg(m_audType).arg(m_driver).arg(layer));
    }

    return layer;
}

uint MpegRecorder::GetFilteredAudioBitRate(uint audio_layer) const
{
    if (2 == audio_layer)
        return std::max(m_audBitrateL2, 10);
    if (3 == audio_layer)
        return  m_audBitrateL3;
    return std::max(m_audBitrateL1, 6);
}

static int streamtype_ivtv_to_v4l2(int st)
{
    switch (st)
    {
        case 0:  return V4L2_MPEG_STREAM_TYPE_MPEG2_PS;
        case 1:  return V4L2_MPEG_STREAM_TYPE_MPEG2_TS;
        case 2:  return V4L2_MPEG_STREAM_TYPE_MPEG1_VCD;
        case 3:                                          /* PES A/V    */
        case 5:                                          /* PES V      */
        case 7:                                          /* PES A      */
          return V4L2_MPEG_STREAM_TYPE_MPEG2_PS;
        case 10: return V4L2_MPEG_STREAM_TYPE_MPEG2_DVD;
        case 11: return V4L2_MPEG_STREAM_TYPE_MPEG1_VCD; /* VCD */
        case 12: return V4L2_MPEG_STREAM_TYPE_MPEG2_SVCD;
        case 13:                                         /* DVD-Special 1 */
        case 14:                                         /* DVD-Special 2 */
            return V4L2_MPEG_STREAM_TYPE_MPEG2_DVD;
        default: return V4L2_MPEG_STREAM_TYPE_MPEG2_TS;
    }
}

static void add_ext_ctrl(std::vector<struct v4l2_ext_control> &ctrl_list,
                         uint32_t id, int32_t value)
{
    struct v4l2_ext_control tmp_ctrl {};
    tmp_ctrl.id    = id;
    tmp_ctrl.value = value;
    ctrl_list.push_back(tmp_ctrl);
}

static void set_ctrls(int fd, std::vector<struct v4l2_ext_control> &ext_ctrls)
{
    static QMutex s_controlDescriptionLock;
    static QMap<uint32_t,QString> s_controlDescription;

    s_controlDescriptionLock.lock();
    if (s_controlDescription.isEmpty())
    {
        s_controlDescription[V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ] =
            "Audio Sampling Frequency";
        s_controlDescription[V4L2_CID_MPEG_VIDEO_ASPECT] =
            "Video Aspect ratio";
        s_controlDescription[V4L2_CID_MPEG_AUDIO_ENCODING] =
            "Audio Encoding";
        s_controlDescription[V4L2_CID_MPEG_AUDIO_L2_BITRATE] =
            "Audio L2 Bitrate";
        s_controlDescription[V4L2_CID_MPEG_VIDEO_BITRATE_PEAK] =
            "Video Peak Bitrate";
        s_controlDescription[V4L2_CID_MPEG_VIDEO_BITRATE] =
            "Video Average Bitrate";
        s_controlDescription[V4L2_CID_MPEG_STREAM_TYPE] =
            "MPEG Stream type";
        s_controlDescription[V4L2_CID_MPEG_VIDEO_BITRATE_MODE] =
            "MPEG Bitrate mode";
    }
    s_controlDescriptionLock.unlock();

    for (auto & ext_ctrl : ext_ctrls)
    {
        struct v4l2_ext_controls ctrls {};

        int value = ext_ctrl.value;

        ctrls.ctrl_class  = V4L2_CTRL_CLASS_MPEG;
        ctrls.count       = 1;
        ctrls.controls    = &ext_ctrl;

        if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls) < 0)
        {
            QMutexLocker locker(&s_controlDescriptionLock);
            LOG(VB_GENERAL, LOG_ERR, QString("mpegrecorder.cpp:set_ctrls(): ") +
                QString("Could not set %1 to %2")
                    .arg(s_controlDescription[ext_ctrl.id]).arg(value) +
                    ENO);
        }
    }
}

bool MpegRecorder::SetV4L2DeviceOptions(int chanfd)
{
    std::vector<struct v4l2_ext_control> ext_ctrls;

    // Set controls
    if (m_driver != "hdpvr")
    {
        if (!m_driver.startsWith("saa7164"))
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
                     m_aspectRatio - 1);

        add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_STREAM_TYPE,
                     streamtype_ivtv_to_v4l2(GetFilteredStreamType()));

    }
    else
    {
        m_maxBitrate = m_highMpeg4PeakBitrate;
        m_bitrate    = m_highMpeg4AvgBitrate;
    }
    m_maxBitrate = std::max(m_maxBitrate, m_bitrate);

    if (m_driver == "hdpvr" || m_driver.startsWith("saa7164"))
    {
        add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
                     (m_maxBitrate == m_bitrate) ?
                     V4L2_MPEG_VIDEO_BITRATE_MODE_CBR :
                     V4L2_MPEG_VIDEO_BITRATE_MODE_VBR);
    }

    add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_VIDEO_BITRATE,
                 m_bitrate * 1000);

    add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_VIDEO_BITRATE_PEAK,
                 m_maxBitrate * 1000);

    set_ctrls(chanfd, ext_ctrls);

    bool ok = false;
    int audioinput = m_audioDeviceName.toUInt(&ok);
    if (ok)
    {
        struct v4l2_audio ain {};
        ain.index = audioinput;
        if (ioctl(chanfd, VIDIOC_ENUMAUDIO, &ain) < 0)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Unable to get audio input.");
        }
        else
        {
            ain.index = audioinput;
            if (ioctl(chanfd, VIDIOC_S_AUDIO, &ain) < 0)
            {
                LOG(VB_GENERAL, LOG_WARNING,
                    LOC + "Unable to set audio input.");
            }
        }
    }

    // query supported audio codecs if spdif is not used
    if (m_driver == "hdpvr" && audioinput != 2)
    {
        struct v4l2_queryctrl qctrl {};
        qctrl.id = V4L2_CID_MPEG_AUDIO_ENCODING;

        if (!ioctl(chanfd, VIDIOC_QUERYCTRL, &qctrl))
        {
            uint audio_enc = std::clamp(m_audType-1, qctrl.minimum, qctrl.maximum);
            add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_AUDIO_ENCODING, audio_enc);
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                "Unable to get supported audio codecs." + ENO);
        }
    }

    return true;
}

bool MpegRecorder::SetVBIOptions(int chanfd)
{
    if (VBIMode::None == m_vbiMode)
        return true;

    if (m_driver == "hdpvr")
        return true;

#ifdef V4L2_CAP_SLICED_VBI_CAPTURE
    if (m_supportsSlicedVbi)
    {
        int fd = 0;

        if (OpenVBIDevice() >= 0)
            fd = m_vbiFd;
        else
            fd = chanfd;

        struct v4l2_format vbifmt {};
        vbifmt.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
        vbifmt.fmt.sliced.service_set |= (VBIMode::PAL_TT == m_vbiMode) ?
            V4L2_SLICED_VBI_625 : V4L2_SLICED_VBI_525;

        if (ioctl(fd, VIDIOC_S_FMT, &vbifmt) < 0)
        {
            if (m_vbiFd >= 0)
            {
                fd = chanfd; // Retry with video device instead
                if (ioctl(fd, VIDIOC_S_FMT, &vbifmt) < 0)
                {
                    LOG(VB_GENERAL, LOG_WARNING, LOC +
                        "Unable to enable VBI embedding (/dev/vbiX)" + ENO);
                    return false;
                }
            }
            else
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC +
                    "Unable to enable VBI embedding (/dev/videoX)" + ENO);
                return false;
            }
        }

        if (ioctl(fd, VIDIOC_G_FMT, &vbifmt) >= 0)
        {
            LOG(VB_RECORD, LOG_INFO,
                LOC + QString("VBI service: %1, io size: %2")
                    .arg(vbifmt.fmt.sliced.service_set)
                    .arg(vbifmt.fmt.sliced.io_size));

            struct v4l2_ext_control vbi_ctrl {};
            vbi_ctrl.id      = V4L2_CID_MPEG_STREAM_VBI_FMT;
            vbi_ctrl.value   = V4L2_MPEG_STREAM_VBI_FMT_IVTV;

            struct v4l2_ext_controls ctrls {};
            ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
            ctrls.count      = 1;
            ctrls.controls   = &vbi_ctrl;

            if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls) < 0)
            {
                LOG(VB_GENERAL, LOG_WARNING, LOC +
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
    ResetForNewFile();
    return (m_deviceIsMpegFile) ? OpenMpegFileAsInput() : OpenV4L2DeviceAsInput();
}

void MpegRecorder::run(void)
{
    if (!Open())
    {
        if (m_error.isEmpty())
            m_error = "Failed to open V4L device";
        return;
    }

    bool has_select = true;

#if defined(__FreeBSD__)
    // HACK. FreeBSD PVR150/500 driver doesn't currently support select()
    has_select = false;
#endif

    if (m_driver == "hdpvr")
    {
        int progNum = 1;
        auto *sd = new MPEGStreamData(progNum,
                                      m_tvrec ? m_tvrec->GetInputId() : -1,
                                      true);
        sd->SetRecordingType(m_recordingType);
        SetStreamData(sd);

        m_streamData->AddAVListener(this);
        m_streamData->AddWritingListener(this);

        // Make sure the first things in the file are a PAT & PMT
        HandleSingleProgramPAT(m_streamData->PATSingleProgram(), true);
        HandleSingleProgramPMT(m_streamData->PMTSingleProgram(), true);
        m_waitForKeyframeOption = true;
    }

    {
        QMutexLocker locker(&m_pauseLock);
        m_requestRecording = true;
        m_requestHelper = true;
        m_recording = true;
        m_recordingWait.wakeAll();
    }

    auto *buffer = new unsigned char[m_bufferSize + 1];
    int len = 0;
    int remainder = 0;

    bool      good_data = false;
    bool      gap = false;
    QDateTime gap_start;

    MythTimer elapsedTimer;
    float elapsed = NAN;
    long long bytesRead = 0;

    bool ok { false };
    // Bytes per second, but env var is BITS PER SECOND
    int dummyBPS = qEnvironmentVariableIntValue("DUMMYBPS", &ok) / 8;
    if (ok)
    {
        LOG(VB_GENERAL, LOG_INFO,
            LOC + QString("Throttling dummy recorder to %1 bits per second")
                .arg(dummyBPS * 8));
    }

    struct timeval tv {};
    fd_set rdset;

    if (m_deviceIsMpegFile)
        elapsedTimer.start();
    else if (m_deviceReadBuffer)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Initial startup of recorder");
        StartEncoding();
    }

    QByteArray vdevice = m_videodevice.toLatin1();
    while (IsRecordingRequested() && !IsErrored())
    {
        if (PauseAndWait(100ms))
            continue;

        if (m_deviceIsMpegFile)
        {
            if (dummyBPS && bytesRead)
            {
                elapsed = (elapsedTimer.elapsed().count() / 1000.0F) + 1;
                while ((bytesRead / elapsed) > dummyBPS)
                {
                    std::this_thread::sleep_for(50ms);
                    elapsed = (elapsedTimer.elapsed().count() / 1000.0F) + 1;
                }
            }
            else if (GetFramesWritten())
            {
                elapsed = (elapsedTimer.elapsed().count() / 1000.0F) + 1;
                while ((GetFramesWritten() / elapsed) > 30)
                {
                    std::this_thread::sleep_for(50ms);
                    elapsed = (elapsedTimer.elapsed().count() / 1000.0F) + 1;
                }
            }
        }

        if (m_deviceReadBuffer)
        {
            len = m_deviceReadBuffer->Read
                  (&(buffer[remainder]), m_bufferSize - remainder);

            // Check for DRB errors
            if (m_deviceReadBuffer->IsErrored())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Device error detected");

                if (good_data)
                {
                    if (gap)
                    {
                        /* Already processing a gap, which means
                         * restarting the encoding didn't work! */
                        SetRecordingStatus(RecStatus::Failing, __FILE__, __LINE__);
                    }
                    else
                    {
                        gap = true;
                    }
                }

                if (!RestartEncoding())
                    SetRecordingStatus(RecStatus::Failing, __FILE__, __LINE__);
            }
            else if (m_deviceReadBuffer->IsEOF() &&
                     IsRecordingRequested())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Device EOF detected");
                m_error = "Device EOF detected";
            }
            else
            {
                // If we have seen good data, but now have a gap, note it
                if (good_data)
                {
                    if (gap)
                    {
                        QMutexLocker locker(&m_statisticsLock);
                        QDateTime gap_end(MythDate::current());

                        m_recordingGaps.push_back(RecordingGap
                                                (gap_start, gap_end));
                        LOG(VB_RECORD, LOG_DEBUG,
                            LOC + QString("Inserted gap %1 dur %2")
                            .arg(m_recordingGaps.back().toString())
                            .arg(gap_start.secsTo(gap_end)));
                        gap = false;
                    }
                    else
                    {
                        gap_start = MythDate::current();
                    }
                }
                else
                {
                    good_data = true;
                }
            }
        }
        else if (m_readfd < 0)
        {
            continue;
        }
        else
        {
            if (has_select)
            {
                tv.tv_sec = 5;
                tv.tv_usec = 0;
                FD_ZERO(&rdset); // NOLINT(readability-isolate-declaration)
                FD_SET(m_readfd, &rdset);

                switch (select(m_readfd + 1, &rdset, nullptr, nullptr, &tv))
                {
                    case -1:
                        if (errno == EINTR)
                            continue;

                        LOG(VB_GENERAL, LOG_ERR, LOC + "Select error" + ENO);
                        continue;

                    case 0:
                        LOG(VB_GENERAL, LOG_ERR, LOC + "select timeout - "
                                "driver has stopped responding");

                        if (close(m_readfd) != 0)
                        {
                            LOG(VB_GENERAL, LOG_ERR, LOC + "Close error" + ENO);
                        }

                        // Force card to be reopened on next iteration..
                        m_readfd = -1;
                        continue;

                    default:
                        break;
                }
            }

            len = read(m_readfd, &(buffer[remainder]), m_bufferSize - remainder);

            if (len < 0 && !has_select)
            {
                QMutexLocker locker(&m_pauseLock);
                if (m_requestRecording && !m_requestPause)
                    m_unpauseWait.wait(&m_pauseLock, 25);
                continue;
            }

            if ((len == 0) && (m_deviceIsMpegFile))
            {
                close(m_readfd);
                m_readfd = open(vdevice.constData(), O_RDONLY);

                if (m_readfd >= 0)
                {
                    len = read(m_readfd,
                               &(buffer[remainder]), m_bufferSize - remainder);
                }

                if (len <= 0)
                {
                    m_error = "Failed to read from video file";
                    continue;
                }
            }
            else if (len < 0 && errno != EAGAIN)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("error reading from: %1")
                        .arg(m_videodevice) + ENO);
                continue;
            }
        }

        if (len > 0)
        {
            bytesRead += len;
            len += remainder;

            if (m_driver == "hdpvr")
            {
                remainder = m_streamData->ProcessData(buffer, len);
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

    LOG(VB_RECORD, LOG_INFO, LOC + "run finishing up");

    StopEncoding();

    {
        QMutexLocker locker(&m_pauseLock);
        m_requestHelper = false;
    }

    if (m_vbiThread)
    {
        m_vbiThread->wait();
        delete m_vbiThread;
        m_vbiThread = nullptr;
        CloseVBIDevice();
    }

    FinishRecording();

    delete[] buffer;

    if (m_driver == "hdpvr")
    {
        m_streamData->RemoveWritingListener(this);
        m_streamData->RemoveAVListener(this);
        SetStreamData(nullptr);
    }

    QMutexLocker locker(&m_pauseLock);
    m_requestRecording = false;
    m_recording = false;
    m_recordingWait.wakeAll();
}

bool MpegRecorder::ProcessTSPacket(const TSPacket &tspacket_real)
{
    const uint pid = tspacket_real.PID();

    TSPacket *tspacket_fake = nullptr;
    if ((m_driver == "hdpvr") && (pid == 0x1001)) // PCRPID for HD-PVR
    {
        tspacket_fake = tspacket_real.CreateClone();
        uint cc = (m_continuityCounter[pid] == 0xFF) ?
            0 : (m_continuityCounter[pid] + 1) & 0xf;
        tspacket_fake->SetContinuityCounter(cc);
    }

    const TSPacket &tspacket = (tspacket_fake)
        ? *tspacket_fake : tspacket_real;

    bool ret = DTVRecorder::ProcessTSPacket(tspacket);

    delete tspacket_fake;

    return ret;
}

void MpegRecorder::Reset(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Reset(void)");
    ResetForNewFile();

    if (m_h2645Parser != nullptr)
        m_h2645Parser->Reset();

    m_startCode = 0xffffffff;

    if (m_curRecording)
    {
        m_curRecording->ClearPositionMap(MARK_GOP_BYFRAME);
    }
    if (m_streamData)
        m_streamData->Reset(m_streamData->DesiredProgram());
}

void MpegRecorder::Pause(bool clear)
{
    QMutexLocker locker(&m_pauseLock);
    m_clearTimeOnPause = clear;
    m_paused = false;
    m_requestPause = true;
}

bool MpegRecorder::PauseAndWait(std::chrono::milliseconds timeout)
{
    QMutexLocker locker(&m_pauseLock);
    if (m_requestPause)
    {
        if (!IsPaused(true))
        {
            LOG(VB_RECORD, LOG_INFO, LOC + "PauseAndWait pause");

            StopEncoding();

            m_paused = true;
            m_pauseWait.wakeAll();

            if (m_tvrec)
                m_tvrec->RecorderPaused();
        }

        m_unpauseWait.wait(&m_pauseLock, timeout.count());
    }

    if (!m_requestPause && IsPaused(true))
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "PauseAndWait unpause");

        if (m_driver == "hdpvr")
        {
            // HD-PVR will sometimes reset to defaults
            SetV4L2DeviceOptions(m_chanfd);
        }

        StartEncoding();

        if (m_streamData)
            m_streamData->Reset(m_streamData->DesiredProgram());

        m_paused = false;
    }

    return IsPaused(true);
}

bool MpegRecorder::RestartEncoding(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "RestartEncoding");

    QMutexLocker locker(&m_startStopEncodingLock);

    StopEncoding();

    // Make sure the next things in the file are a PAT & PMT
    if (m_streamData &&
        m_streamData->PATSingleProgram() &&
        m_streamData->PMTSingleProgram())
    {
        m_payloadBuffer.clear();  // No reason to keep part of a frame
        HandleSingleProgramPAT(m_streamData->PATSingleProgram(), true);
        HandleSingleProgramPMT(m_streamData->PMTSingleProgram(), true);
    }

    if (m_driver == "hdpvr") // HD-PVR will sometimes reset to defaults
        SetV4L2DeviceOptions(m_chanfd);

    return StartEncoding();
}

bool MpegRecorder::StartEncoding(void)
{
    QMutexLocker locker(&m_startStopEncodingLock);

    LOG(VB_RECORD, LOG_INFO, LOC + "StartEncoding");

    if (m_readfd < 0)
    {
        m_readfd = open(m_videodevice.toLatin1().constData(), O_RDWR | O_NONBLOCK);
        if (m_readfd < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "StartEncoding: Can't open video device." + ENO);
            m_error = "Failed to start recording";
            return false;
        }
    }

    if (m_h2645Parser != nullptr)
        m_h2645Parser->Reset();

    bool good_res = true;
    if (m_driver == "hdpvr")
    {
        m_waitForKeyframeOption = true;
        m_seenSps = false;
        good_res = HandleResolutionChanges();
    }

    // (at least) with the 3.10 kernel, the V4L2_ENC_CMD_START does
    // not reliably start the data flow from a HD-PVR.  A read() seems
    // to work, though.

    int idx = 1;
    for ( ; idx < 50; ++idx)
    {
        uint8_t dummy = 0;
        int len = read(m_readfd, &dummy, 0);
        if (len == 0)
            break;
        if (idx == 20)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "StartEncoding: read failing, re-opening device: " + ENO);
            close(m_readfd);
            std::this_thread::sleep_for(2ms);
            m_readfd = open(m_videodevice.toLatin1().constData(),
                          O_RDWR | O_NONBLOCK);
            if (m_readfd < 0)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "StartEncoding: Can't open video device." + ENO);
                m_error = "Failed to start recording";
                return false;
            }
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("StartEncoding: read failed, retry in %1 msec:")
                .arg(100 * idx) + ENO);
            std::this_thread::sleep_for(idx * 100us);
        }
    }
    if (idx == 50)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "StartEncoding: read from video device failed." + ENO);
        m_error = "Failed to start recording";
        close(m_readfd);
        m_readfd = -1;
        return false;
    }
    if (idx > 0)
    {
        LOG(VB_RECORD, LOG_WARNING, LOC +
            QString("%1 read attempts required to start encoding").arg(idx));
    }

    if (!good_res)   // Try again
        HandleResolutionChanges();

    if (m_deviceReadBuffer)
    {
        m_deviceReadBuffer->Reset(m_videodevice.toLatin1().constData(), m_readfd);
        m_deviceReadBuffer->SetRequestPause(false);
        m_deviceReadBuffer->Start();
    }

    return true;
}

void MpegRecorder::StopEncoding(void)
{
    QMutexLocker locker(&m_startStopEncodingLock);

    LOG(VB_RECORD, LOG_INFO, LOC + "StopEncoding");

    if (m_readfd < 0)
        return;

    struct v4l2_encoder_cmd command {};
    command.cmd   = V4L2_ENC_CMD_STOP;
    command.flags = V4L2_ENC_CMD_STOP_AT_GOP_END;

    if (m_deviceReadBuffer)
        m_deviceReadBuffer->SetRequestPause(true);

    bool stopped = 0 == ioctl(m_readfd, VIDIOC_ENCODER_CMD, &command);
    if (stopped)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Encoding stopped");
    }
    else if (errno != ENOTTY && errno != EINVAL)
    {
        // Some drivers do not support this ioctl at all.  It is marked as
        // "experimental" in the V4L2 API spec. These drivers return EINVAL
        // in older kernels and ENOTTY in 3.1+

        LOG(VB_GENERAL, LOG_WARNING, LOC + "StopEncoding failed" + ENO);
    }

    if (m_deviceReadBuffer && m_deviceReadBuffer->IsRunning())
    {
        // allow last bits of data through..
        std::this_thread::sleep_for(20ms);
        m_deviceReadBuffer->Stop();
    }

    // close the fd so streamoff/streamon work in V4LChannel
    close(m_readfd);
    m_readfd = -1;
}

void MpegRecorder::InitStreamData(void)
{
    m_streamData->AddMPEGSPListener(this);
    m_streamData->SetDesiredProgram(1);
}

void MpegRecorder::SetBitrate(int bitrate, int maxbitrate,
                              const QString & reason)
{
    if (maxbitrate == bitrate)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("%1 bitrate %2 kbps CBR")
                .arg(reason).arg(bitrate));
    }
    else
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("%1 bitrate %2/%3 kbps VBR")
                .arg(reason).arg(bitrate).arg(maxbitrate));
    }

    std::vector<struct v4l2_ext_control> ext_ctrls;
    add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
                 (maxbitrate == bitrate) ?
                 V4L2_MPEG_VIDEO_BITRATE_MODE_CBR :
                 V4L2_MPEG_VIDEO_BITRATE_MODE_VBR);

    add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_VIDEO_BITRATE,
                 bitrate * 1000);

    add_ext_ctrl(ext_ctrls, V4L2_CID_MPEG_VIDEO_BITRATE_PEAK,
                 maxbitrate * 1000);

    set_ctrls(m_readfd, ext_ctrls);
}

bool MpegRecorder::HandleResolutionChanges(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Checking Resolution");
    uint pix = 0;
    struct v4l2_format vfmt {};
    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == ioctl(m_chanfd, VIDIOC_G_FMT, &vfmt))
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("Got Resolution %1x%2")
                .arg(vfmt.fmt.pix.width).arg(vfmt.fmt.pix.height));
        pix = vfmt.fmt.pix.width * vfmt.fmt.pix.height;
    }

    if (!pix)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Giving up detecting resolution: " + ENO);
        return false; // nothing to do, we don't have a resolution yet
    }

    int old_max = m_maxBitrate;
    int old_avg = m_bitrate;
    if (pix <= 768*568)
    {
        m_maxBitrate = m_lowMpeg4PeakBitrate;
        m_bitrate    = m_lowMpeg4AvgBitrate;
    }
    else if (pix >= 1920*1080)
    {
        m_maxBitrate = m_highMpeg4PeakBitrate;
        m_bitrate    = m_highMpeg4AvgBitrate;
    }
    else
    {
        m_maxBitrate = m_mediumMpeg4PeakBitrate;
        m_bitrate    = m_mediumMpeg4AvgBitrate;
    }
    m_maxBitrate = std::max(m_maxBitrate, m_bitrate);

    if ((old_max != m_maxBitrate) || (old_avg != m_bitrate))
    {
        if (old_max == old_avg)
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("Old bitrate %1 CBR").arg(old_avg));
        }
        else
        {
            LOG(VB_RECORD, LOG_INFO,LOC +
                QString("Old bitrate %1/%2 VBR") .arg(old_avg).arg(old_max));
        }

        SetBitrate(m_bitrate, m_maxBitrate, "New");
    }

    return true;
}

void MpegRecorder::FormatCC(uint code1, uint code2)
{
    LOG(VB_VBI, LOG_INFO, LOC + QString("FormatCC(0x%1,0x%2)")
            .arg(code1,0,16).arg(code2,0,16));
    // TODO add to CC data vector

    // TODO find video frames in output and insert cc_data
    // as CEA-708 user_data packets containing CEA-608 captions
    ///at picture data level
}
