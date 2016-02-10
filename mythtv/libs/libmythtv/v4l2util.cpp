#include "v4l2util.h"
#include "mythlogging.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <QRegularExpression>

#define v4l2_ioctl(_FD_, _REQUEST_, _DATA_) ioctl(_FD_, _REQUEST_, _DATA_)
#define LOC      QString("V4L2(%1): ").arg(m_device_name)

V4L2util::V4L2util(void)
    : m_fd(-1),
      m_vbi_fd(-1),
      m_capabilities(0),
      m_have_query_ext_ctrl(false),
      m_version(0)
{
}

V4L2util::V4L2util(const QString& dev_name)
    : m_fd(-1),
      m_vbi_fd(-1),
      m_capabilities(0),
      m_have_query_ext_ctrl(false)
{
    Open(dev_name);
}

V4L2util::V4L2util(const QString& dev_name, const QString& vbi_dev_name)
    : m_fd(0),
      m_vbi_fd(-1),
      m_capabilities(0),
      m_have_query_ext_ctrl(false)
{
    Open(dev_name, vbi_dev_name);
}

bool V4L2util::Open(const QString& dev_name, const QString& vbi_dev_name)
{
    if (m_fd >= 0 && dev_name == m_device_name)
        return true;

    Close();

    m_fd = open(dev_name.toLatin1().constData(), O_RDWR);
    if (m_fd < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Could not open '%1': ").arg(dev_name) + ENO);
        return false;
    }
    m_device_name = dev_name;

    struct v4l2_query_ext_ctrl qc = {
        V4L2_CTRL_FLAG_NEXT_CTRL | V4L2_CTRL_FLAG_NEXT_COMPOUND
    };

    m_have_query_ext_ctrl = (v4l2_ioctl(m_fd, VIDIOC_QUERY_EXT_CTRL, &qc) == 0);

    m_card_name = m_driver_name = QString::null;
    m_version = 0;
    m_capabilities = 0;

    struct v4l2_capability capability = {0};
    if (ioctl(m_fd, VIDIOC_QUERYCAP, &capability) >= 0)
    {
        m_card_name    = QString::fromLatin1((const char*)capability.card);
        m_driver_name  = QString::fromLatin1((const char*)capability.driver);
        m_version      = capability.version;
        m_capabilities = capability.capabilities;
    }
    else
    {
        Close();
        return false;
    }

    if (!m_driver_name.isEmpty())
        m_driver_name.remove( QRegExp("\\[[0-9]\\]$") );

    OpenVBI(vbi_dev_name);

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Opened");
    return true;
}

void V4L2util::Close()
{
    if (m_fd >= 0)
    {
        close(m_fd);
        LOG(VB_CHANNEL, LOG_INFO, LOC + "Closed");
    }
    m_fd = -1;
    m_options.clear();
}

bool V4L2util::HasStreaming(void) const
{
    if (m_capabilities ^ V4L2_CAP_STREAMING)
        return false;

    struct v4l2_requestbuffers reqbuf;

    if (-1 == ioctl (m_fd, VIDIOC_REQBUFS, &reqbuf))
    {
        if (errno == EINVAL)
        {
            LOG(VB_CHANNEL, LOG_INFO, LOC +
                "Video capturing or mmap-streaming is not supported");
        }
        else
        {
            LOG(VB_CHANNEL, LOG_WARNING, LOC + "VIDIOC_REQBUFS" + ENO);
        }
        return false;
    }

    return true;
}

bool V4L2util::HasSlicedVBI(void) const
{
    return m_capabilities & V4L2_CAP_SLICED_VBI_CAPTURE;
}

void V4L2util::bitmask_toString(QString& result, uint32_t flags,
                                uint32_t mask, const QString& desc)
{
    if (flags& mask)
    {
        if (!result.isEmpty())
            result += '|';
        result += desc;
    }
}

QString V4L2util::ctrlflags_toString(uint32_t flags)
{
    QString result;

    bitmask_toString(result, flags, V4L2_CTRL_FLAG_DISABLED,
                     "disabled");
    bitmask_toString(result, flags, V4L2_CTRL_FLAG_GRABBED,
                     "grabbed");
    bitmask_toString(result, flags, V4L2_CTRL_FLAG_READ_ONLY,
                     "read-only");
    bitmask_toString(result, flags, V4L2_CTRL_FLAG_UPDATE,
                     "update");
    bitmask_toString(result, flags, V4L2_CTRL_FLAG_INACTIVE,
                     "inactive");
    bitmask_toString(result, flags, V4L2_CTRL_FLAG_SLIDER,
                     "slider");
    bitmask_toString(result, flags, V4L2_CTRL_FLAG_WRITE_ONLY,
                     "write-only");
    bitmask_toString(result, flags, V4L2_CTRL_FLAG_VOLATILE,
                     "volatile");
    bitmask_toString(result, flags, V4L2_CTRL_FLAG_HAS_PAYLOAD,
                     "has-payload");
    bitmask_toString(result, flags, V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
                     "execute-on-write");

    return result;
}

QString V4L2util::queryctrl_toString(int type)
{
    switch (type)
    {
        case V4L2_CTRL_TYPE_INTEGER:
          return "int";
        case V4L2_CTRL_TYPE_INTEGER64:
          return "int64";
        case V4L2_CTRL_TYPE_STRING:
          return "str";
        case V4L2_CTRL_TYPE_BOOLEAN:
          return "bool";
        case V4L2_CTRL_TYPE_MENU:
          return "menu";
        case V4L2_CTRL_TYPE_INTEGER_MENU:
          return "intmenu";
        case V4L2_CTRL_TYPE_BUTTON:
          return "button";
        case V4L2_CTRL_TYPE_BITMASK:
          return "bitmask";
        case V4L2_CTRL_TYPE_U8:
          return "u8";
        case V4L2_CTRL_TYPE_U16:
          return "u16";
        case V4L2_CTRL_TYPE_U32:
          return "u32";
        default:
          return "unknown";
    }
}

void V4L2util::log_qctrl(struct v4l2_queryctrl& queryctrl,
                         DriverOption& drv_opt, QString& msg)
{
    struct v4l2_querymenu qmenu;
    QString  nameStr((char *)queryctrl.name);
    int      idx;

    memset(&qmenu, 0, sizeof(qmenu));
    qmenu.id = queryctrl.id;

    // Replace non-printable with _
    nameStr.replace(QRegularExpression("[^a-zA-Z\\d\\s]"), "_");

    drv_opt.name          = nameStr;
    drv_opt.minimum       = queryctrl.minimum;
    drv_opt.maximum       = queryctrl.maximum;
    drv_opt.step          = queryctrl.step;
    drv_opt.default_value = queryctrl.default_value;;

    if (nameStr == "Stream Type")
        drv_opt.category = DriverOption::STREAM_TYPE;
    else if (nameStr == "Video Encoding")
        drv_opt.category = DriverOption::VIDEO_ENCODING;
    else if (nameStr == "Video Aspect")
        drv_opt.category = DriverOption::VIDEO_ASPECT;
    else if (nameStr == "Video B Frames")
        drv_opt.category = DriverOption::VIDEO_B_FRAMES;
    else if (nameStr == "Video GOP Size")
        drv_opt.category = DriverOption::VIDEO_GOP_SIZE;
    else if (nameStr == "Video Bitrate Mode")
        drv_opt.category = DriverOption::VIDEO_BITRATE_MODE;
    else if (nameStr == "Video Bitrate")
        drv_opt.category = DriverOption::VIDEO_BITRATE;
    else if (nameStr == "Video Peak Bitrate")
        drv_opt.category = DriverOption::VIDEO_BITRATE_PEAK;
    else if (nameStr == "Audio Encoding")
        drv_opt.category = DriverOption::AUDIO_ENCODING;
    else if (nameStr == "Audio Bitrate Mode")
        drv_opt.category = DriverOption::AUDIO_BITRATE_MODE;
    else if (nameStr == "Audio Bitrate")
        drv_opt.category = DriverOption::AUDIO_BITRATE;
    else if (nameStr == "Brightness")
        drv_opt.category = DriverOption::BRIGHTNESS;
    else if (nameStr == "Contrast")
        drv_opt.category = DriverOption::CONTRAST;
    else if (nameStr == "Saturation")
        drv_opt.category = DriverOption::SATURATION;
    else if (nameStr == "Hue")
        drv_opt.category = DriverOption::HUE;
    else if (nameStr == "Sharpness")
        drv_opt.category = DriverOption::SHARPNESS;
    else if (nameStr == "Volume")
        drv_opt.category = DriverOption::VOLUME;
    else
        drv_opt.category = DriverOption::UNKNOWN_CAT;

    switch (queryctrl.type)
    {
        case V4L2_CTRL_TYPE_INTEGER:
        case V4L2_CTRL_TYPE_INTEGER64:
        case V4L2_CTRL_TYPE_U8:
        case V4L2_CTRL_TYPE_U16:
        case V4L2_CTRL_TYPE_U32:
          msg = QString("%1 : min=%2 max=%3 step=%4 default=%5")
                .arg(QString("%1 (%2)").arg(nameStr)
                     .arg(queryctrl_toString(queryctrl.type)), 31, QChar(' '))
                .arg(queryctrl.minimum)
                .arg(queryctrl.maximum)
                .arg(queryctrl.step)
                .arg(queryctrl.default_value);
          drv_opt.type = DriverOption::INTEGER;
          break;
        case V4L2_CTRL_TYPE_STRING:
          msg =  QString("%1 : min=%2 max=%3 step=%4")
                .arg(QString("%1 (%2)").arg(nameStr)
                     .arg(queryctrl_toString(queryctrl.type)), 31, QChar(' '))
                 .arg(queryctrl.minimum)
                 .arg(queryctrl.maximum)
                 .arg(queryctrl.step);
          drv_opt.type = DriverOption::STRING;
          break;
        case V4L2_CTRL_TYPE_BOOLEAN:
          msg = QString("%1 : default=%2")
                .arg(QString("%1 (%2)").arg(nameStr)
                     .arg(queryctrl_toString(queryctrl.type)), 31, QChar(' '))
                .arg(queryctrl.default_value);
          drv_opt.type = DriverOption::BOOLEAN;
          break;
        case V4L2_CTRL_TYPE_MENU:
        case V4L2_CTRL_TYPE_INTEGER_MENU:
        {
            msg = QString("%1 : min=%3 max=%4 default=%5")
                .arg(QString("%1 (%2)").arg(nameStr)
                     .arg(queryctrl_toString(queryctrl.type)), 31, QChar(' '))
                  .arg(queryctrl.minimum)
                  .arg(queryctrl.maximum)
                  .arg(queryctrl.default_value);
#if 0
            struct v4l2_querymenu querymenu = { 0, };
            memset (&querymenu, 0, sizeof (querymenu));
            querymenu.id = queryctrl.id;

            for (querymenu.index = queryctrl.minimum;
                 static_cast<int>(querymenu.index) <= queryctrl.maximum;
                 ++querymenu.index)
            {
                drv_opt.menu.clear();
                if (0 == ioctl(m_fd, VIDIOC_QUERYMENU, &querymenu))
                {
                    msg += QString(" menu>%1").arg((char *)querymenu.name);
                    drv_opt.menu[querymenu.index] =
                        QString((char *)querymenu.name);
                }
            }
#endif
            drv_opt.type = DriverOption::MENU;
            break;
        }
        case V4L2_CTRL_TYPE_BUTTON:
          msg = QString("%1 :")
                .arg(QString("%1 (%2)").arg(nameStr)
                     .arg(queryctrl_toString(queryctrl.type)), 31, QChar(' '));
          drv_opt.type = DriverOption::BUTTON;
                break;
        case V4L2_CTRL_TYPE_BITMASK:
          msg = QString("%1 : max=0x%2 default=0x%3")
                .arg(QString("%1 (%2)").arg(nameStr)
                     .arg(queryctrl_toString(queryctrl.type)), 31, QChar(' '))
                .arg(queryctrl.maximum, 8, 16, QChar(' '))
                .arg(queryctrl.default_value, 8, 16, QChar(' '));
          drv_opt.type = DriverOption::BITMASK;
          break;

        default:
          msg = QString("%1 : type=%2")
                .arg(QString("%1 (%2)").arg(nameStr)
                     .arg(queryctrl_toString(queryctrl.type)), 31, QChar(' '))
                .arg(queryctrl.type);
          drv_opt.type = DriverOption::UNKNOWN_TYPE;
          break;
    }

    if (queryctrl.flags)
        msg += QString(" flags=%1").arg(ctrlflags_toString(queryctrl.flags));

    if (queryctrl.type == V4L2_CTRL_TYPE_MENU ||
        queryctrl.type == V4L2_CTRL_TYPE_INTEGER_MENU)
    {
        for (idx = queryctrl.minimum; idx <= queryctrl.maximum; ++idx)
        {
            qmenu.index = idx;
            if (v4l2_ioctl(m_fd, VIDIOC_QUERYMENU, &qmenu))
                continue;

            drv_opt.menu[idx] = QString((char *)qmenu.name);
            if (queryctrl.type == V4L2_CTRL_TYPE_MENU)
                msg += QString("\t\t%1: %2").arg(idx).arg((char *)qmenu.name);
            else
                msg += QString("\t\t%1: %2 (0x%3)")
                       .arg(idx).arg(qmenu.value)
                       .arg(qmenu.value, 0, 16, QChar('0'));
        }
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC + msg);
}

bool V4L2util::log_control(struct v4l2_queryctrl& qctrl, DriverOption& drv_opt,
                           QString& msg)
{
    struct v4l2_control ctrl;
    struct v4l2_ext_control ext_ctrl;
    struct v4l2_ext_controls ctrls;

    memset(&ctrl, 0, sizeof(ctrl));
    memset(&ext_ctrl, 0, sizeof(ext_ctrl));
    memset(&ctrls, 0, sizeof(ctrls));
    if (qctrl.flags& V4L2_CTRL_FLAG_DISABLED)
    {
        msg += QString("'%1' Disabled").arg((char *)qctrl.name);
        return true;
    }

    if (qctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS)
    {
        msg += QString("'%1' V4L2_CTRL_TYPE_CTRL_CLASS").arg((char *)qctrl.name);
        return true;
    }

    ext_ctrl.id = qctrl.id;
    if ((qctrl.flags& V4L2_CTRL_FLAG_WRITE_ONLY) ||
        qctrl.type == V4L2_CTRL_TYPE_BUTTON)
    {
        log_qctrl(qctrl, drv_opt, msg);
        return true;
    }

    if (qctrl.type >= V4L2_CTRL_COMPOUND_TYPES)
    {
        log_qctrl(qctrl, drv_opt, msg);
        return true;
    }

    ctrls.ctrl_class = V4L2_CTRL_ID2CLASS(qctrl.id);
    ctrls.count = 1;
    ctrls.controls = &ext_ctrl;
    if (qctrl.type == V4L2_CTRL_TYPE_INTEGER64 ||
        qctrl.type == V4L2_CTRL_TYPE_STRING ||
        (V4L2_CTRL_ID2CLASS(qctrl.id) != V4L2_CTRL_CLASS_USER &&
         qctrl.id < V4L2_CID_PRIVATE_BASE))
    {
        if (qctrl.type == V4L2_CTRL_TYPE_STRING)
        {
            ext_ctrl.size = qctrl.maximum + 1;
            ext_ctrl.string = (char *)malloc(ext_ctrl.size);
            ext_ctrl.string[0] = 0;
        }
        if (v4l2_ioctl(m_fd, VIDIOC_G_EXT_CTRLS, &ctrls))
        {
            LOG(VB_CHANNEL, LOG_WARNING, LOC +
                QString("Failed to get ext_ctr %1: ")
                .arg((char *)qctrl.name) + ENO);
            return false;
        }
    }
    else {
        ctrl.id = qctrl.id;
        if (v4l2_ioctl(m_fd, VIDIOC_G_CTRL, &ctrl))
        {
            LOG(VB_CHANNEL, LOG_WARNING, LOC +
                QString("Failed to get ctrl %1: ")
                .arg((char *)qctrl.name) + ENO);
            return false;
        }
        ext_ctrl.value = ctrl.value;
    }
    log_qctrl(qctrl, drv_opt, msg);

    if (qctrl.type == V4L2_CTRL_TYPE_STRING)
        free(ext_ctrl.string);
    return true;
}

// Some drivers don't set 'default' options, so make some assumptions
void V4L2util::SetDefaultOptions(DriverOption::Options& options)
{
    if (!options.contains(DriverOption::VIDEO_ENCODING))
    {
        DriverOption drv_opt;
        drv_opt.category = DriverOption::VIDEO_ENCODING;
        drv_opt.name = "Video Encoding";
        drv_opt.minimum = drv_opt.maximum = drv_opt.default_value =
                          V4L2_MPEG_VIDEO_ENCODING_MPEG_2;
        drv_opt.menu[drv_opt.default_value] = "MPEG-2 Video";
        options[drv_opt.category] = drv_opt;
    }

    if (!options.contains(DriverOption::AUDIO_ENCODING))
    {
        DriverOption drv_opt;

        // V4L2_CID_MPEG_AUDIO_ENCODING
        drv_opt.category = DriverOption::AUDIO_ENCODING;
        drv_opt.name = "Audio Encoding";
        drv_opt.minimum = drv_opt.maximum = drv_opt.default_value =
                          V4L2_MPEG_AUDIO_ENCODING_LAYER_2;
        drv_opt.menu[drv_opt.default_value] = "MPEG-1/2 Layer II encoding";
        options[drv_opt.category] = drv_opt;

        drv_opt.category = DriverOption::AUDIO_BITRATE;
        drv_opt.name = "Audio Bitrate";
        drv_opt.minimum = drv_opt.maximum = drv_opt.default_value =
                          V4L2_MPEG_AUDIO_ENCODING_LAYER_2;
        drv_opt.menu[drv_opt.default_value] = "MPEG-1/2 Layer II encoding";
        options[drv_opt.category] = drv_opt;

        // V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ
        drv_opt.category = DriverOption::AUDIO_SAMPLERATE;
        drv_opt.name = "MPEG Audio sampling frequency";
        drv_opt.minimum = drv_opt.maximum = drv_opt.default_value =
                          V4L2_MPEG_AUDIO_SAMPLING_FREQ_48000;
        drv_opt.menu[drv_opt.default_value] = "48 kHz";
        options[drv_opt.category] = drv_opt;

        // VIDIOC_S_TUNER
        drv_opt.category = DriverOption::AUDIO_LANGUAGE;
        drv_opt.name = "Tuner Audio Modes";
        drv_opt.minimum = drv_opt.maximum = drv_opt.default_value =
                          V4L2_TUNER_MODE_STEREO;
        drv_opt.menu[drv_opt.default_value] = "Play stereo audio";
        options[drv_opt.category] = drv_opt;
    }

    DriverOption::Options::iterator Iopt = options.begin();
    for ( ; Iopt != options.end(); ++Iopt)
    {
        // If the driver provides a menu of options, use it to set limits
        if (!(*Iopt).menu.isEmpty())
        {
            int minimum = INT_MAX;
            int maximum = -1;

            DriverOption::menu_t::iterator Imenu = (*Iopt).menu.begin();
            for ( ; Imenu != (*Iopt).menu.end(); ++Imenu)
            {
                if (Imenu.key() < minimum) minimum = Imenu.key();
                if (Imenu.key() > maximum) maximum = Imenu.key();
            }
            if ((*Iopt).minimum != minimum)
            {
                LOG(VB_CHANNEL, LOG_INFO, LOC +
                    QString("%1 menu options overrides minimum from %2 to %3")
                    .arg((*Iopt).name).arg((*Iopt).minimum).arg(minimum));
                (*Iopt).minimum = minimum;
            }
            if ((*Iopt).maximum != maximum)
            {
                LOG(VB_CHANNEL, LOG_INFO, LOC +
                    QString("%1 menu options overrides maximum from %2 to %3")
                    .arg((*Iopt).name).arg((*Iopt).maximum).arg(maximum));
                (*Iopt).maximum = maximum;
            }
        }
    }
}

bool V4L2util::GetFormats(QStringList& formats)
{
    struct v4l2_fmtdesc vid_fmtdesc;
    memset(&vid_fmtdesc, 0, sizeof(vid_fmtdesc));
    const char *flags[] = {"uncompressed", "compressed"};

    vid_fmtdesc.index = 0;
    vid_fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while(ioctl(m_fd, VIDIOC_ENUM_FMT, &vid_fmtdesc) == 0)
    {
        formats << QString("%1 (%2)").arg((char *)vid_fmtdesc.description)
                                    .arg((char *)flags[vid_fmtdesc.flags]);

        /* Convert the pixelformat attributes from FourCC into 'human readab
           fprintf(stdout, "  pixelformat  :%c%c%c%c\\n",
           vid_fmtdesc.pixelformat& 0xFF, (vid_fmtdesc.pixelformat >>
           (vid_fmtdesc.pixelformat >> 16)& 0xFF, (vid_fmtdesc.pixelfo
        */

        vid_fmtdesc.index++;
    }

    LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("GetFormats: %1")
        .arg(formats.join(",")));

    return true;
}

bool V4L2util::GetOptions(DriverOption::Options& options)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Options");

    if (!m_options.isEmpty())
    {
        options = m_options;
        return true;
    }

    struct v4l2_queryctrl qctrl = { 0, };

    qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    while (0 == ioctl (m_fd, VIDIOC_QUERYCTRL, &qctrl))
    {
        QString      msg;
        DriverOption drv_opt;

        log_control(qctrl, drv_opt, msg);
        m_options[drv_opt.category] = drv_opt;

        qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    SetDefaultOptions(m_options);
    options = m_options;
    return true;
}

int V4L2util::GetOptionValue(DriverOption::category_t cat, const QString& desc)
{
    if (m_options.isEmpty())
        GetOptions(m_options);

    if (!m_options.contains(cat))
    {
        LOG(VB_CHANNEL, LOG_WARNING, LOC +
            QString("Driver does not support option."));
        return -1;
    }

    DriverOption drv_opt = m_options.value(cat);
    DriverOption::menu_t::iterator Imenu = drv_opt.menu.begin();
    for ( ; Imenu != drv_opt.menu.end(); ++Imenu)
    {
        if ((*Imenu) == desc)
        {
            LOG(VB_CHANNEL, LOG_INFO, LOC + QString("GetOptionValue '%1' = %2")
                .arg(desc).arg(Imenu.key()));
            return Imenu.key();
        }
    }

    LOG(VB_CHANNEL, LOG_WARNING, LOC +
        QString("'%1' not found in driver options menu.").arg(desc));
    return -1;
}

bool V4L2util::GetVideoStandard(QString& name) const
{
    v4l2_std_id std_id;
    struct v4l2_standard standard = { 0, };

    if (-1 == ioctl (m_fd, VIDIOC_G_STD, &std_id))
    {
        /* Note when VIDIOC_ENUMSTD always returns EINVAL this
           is no video device or it falls under the USB exception,
           and VIDIOC_G_STD returning EINVAL is no error. */
        LOG(VB_CHANNEL, LOG_WARNING, LOC +
            "GetVideoStandard: Failed to detect signal." + ENO);
        return false;
    }

    standard.index = 0;

    while (0 == ioctl (m_fd, VIDIOC_ENUMSTD, &standard))
    {
        if (standard.id & std_id)
        {
            name = (char *)standard.name;
            return true;
        }

        ++standard.index;
    }

    /* EINVAL indicates the end of the enumeration, which cannot be
       empty unless this device falls under the USB exception. */
    if (errno == EINVAL || standard.index == 0)
    {
        LOG(VB_CHANNEL, LOG_WARNING, LOC +
            "GetVideoStandard: Failed to find signal." + ENO);
    }

    return false;
}

int V4L2util::GetSignalStrength(void) const
{
    return -1;   // Does not work

    struct v4l2_tuner tuner = { 0, };

    if (ioctl(m_fd, VIDIOC_G_TUNER, &tuner, 0) != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, "GetSignalStrength() : "
            "Failed to probe signal (v4l2)" + ENO);
        return -1;
    }

    tuner.signal /= 655.35; // Set to 0-100 range

    LOG(VB_RECORD, LOG_INFO, LOC + QString("GetSignalStrength() : "
                                           "(%1\%)")
        .arg(tuner.signal));
    return tuner.signal;
}

bool V4L2util::GetResolution(int& width, int& height) const
{
    struct v4l2_format vfmt = { 0, };

    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(m_fd, VIDIOC_G_FMT, &vfmt) != 0)
    {
        LOG(VB_CHANNEL, LOG_WARNING, LOC +
            "Failed to determine resolution: " + ENO);
        width = height = -1;
        return false;
    }

    width  = vfmt.fmt.pix.width;
    height = vfmt.fmt.pix.height;
    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("Resolution: %1x%2").arg(width).arg(height));
    return true;
}

bool V4L2util::HasTuner(void) const
{
    return m_capabilities & V4L2_CAP_TUNER;
}

bool V4L2util::HasAudioSupport(void) const
{
    return m_capabilities & V4L2_CAP_AUDIO;
}

bool V4L2util::IsEncoder(void) const
{
    struct v4l2_queryctrl qctrl = { 0, };

    qctrl.id = V4L2_CTRL_CLASS_MPEG | V4L2_CTRL_FLAG_NEXT_CTRL;
    return (0 == ioctl (m_fd, VIDIOC_QUERYCTRL, &qctrl) &&
            V4L2_CTRL_ID2CLASS (qctrl.id) == V4L2_CTRL_CLASS_MPEG);
}

bool V4L2util::UserAdjustableResolution(void) const
{
    // I have not been able to come up with a way of querying the
    // driver to answer this question.

    if (m_driver_name == "hdpvr")
        return false;
    return true;
}

int V4L2util::GetExtControl(int request, const QString& ctrl_desc) const
{
    struct v4l2_ext_control  ctrl = { 0, };
    struct v4l2_ext_controls ctrls = { 0, };

    ctrl.id     = request;

    ctrls.count      = 1;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
    ctrls.controls   = &ctrl;

    if (ioctl(m_fd, VIDIOC_G_EXT_CTRLS, &ctrls) != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to retrieve current %1 value.")
            .arg(ctrl_desc) + ENO);
        return -1;
    }

    return ctrls.controls->value;
}


bool V4L2util::SetExtControl(int request, int value, const QString& ctrl_desc,
                             const QString& value_desc)
{
    int current_value = GetExtControl(request, ctrl_desc);

    if (current_value < 0)
        return false;
    if (current_value == value)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("%1 value is already %2 (%3).").arg(ctrl_desc)
            .arg(value_desc).arg(value));
        return true;
    }

    struct v4l2_ext_control  ctrl = { 0, };
    struct v4l2_ext_controls ctrls = { 0, };

    ctrl.id          = request;
    ctrl.value       = value;

    ctrls.count      = 1;
    ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
    ctrls.controls    = &ctrl;

    if (ioctl(m_fd, VIDIOC_S_EXT_CTRLS, &ctrls) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to set %1 value to %2 (%3).")
            .arg(ctrl_desc).arg(value_desc).arg(value) + ENO);
        return false;
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("%1 value set to %2 (%3).").arg(ctrl_desc)
        .arg(value_desc).arg(value));

    return true;
}

QString V4L2util::StreamTypeDesc(int value)
{
    switch (value)
    {
        case V4L2_MPEG_STREAM_TYPE_MPEG2_PS:
          return "MPEG-2 program stream";
        case V4L2_MPEG_STREAM_TYPE_MPEG2_TS:
          return "MPEG-2 transport stream";
        case V4L2_MPEG_STREAM_TYPE_MPEG1_SS:
          return "MPEG-1 system stream";
        case V4L2_MPEG_STREAM_TYPE_MPEG2_DVD:
          return "MPEG-2 DVD-compatible stream";
        case V4L2_MPEG_STREAM_TYPE_MPEG1_VCD:
          return "MPEG-1 VCD-compatible stream";
        case V4L2_MPEG_STREAM_TYPE_MPEG2_SVCD:
          return "MPEG-2 SVCD-compatible stream";
    }
    return "Unknown";
}

int V4L2util::GetStreamType(void) const
{
    int type;

    if (DriverName().startsWith("saa7164"))
    {
        // The saa7164 driver reports that it can do TS, but it doesn't work!
        type = V4L2_MPEG_STREAM_TYPE_MPEG2_PS;
    }
    else
        type = GetExtControl(V4L2_CID_MPEG_STREAM_TYPE, "Stream Type");

    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("MPEG Stream Type is currently set to %1 (%2)")
        .arg(StreamTypeDesc(type)).arg(type));

   return type;
}

bool V4L2util::SetStreamType(int value)
{
    QString desc;

    if (DriverName().startsWith("saa7164") ||
        DriverName().startsWith("ivtv"))
    {
        // The saa7164 driver reports that it can do TS, but it doesn't work!
        value = V4L2_MPEG_STREAM_TYPE_MPEG2_PS;
    }

    return SetExtControl(V4L2_CID_MPEG_STREAM_TYPE, value,
                         "MPEG Stream type", StreamTypeDesc(value));
}

// Video controls
bool V4L2util::SetVideoAspect(int value)
{
    QString desc;
    switch (value)
    {
        case V4L2_MPEG_VIDEO_ASPECT_1x1:
          desc = "Square";
          break;
        case V4L2_MPEG_VIDEO_ASPECT_4x3:
          desc = "4x3";
          break;
        case V4L2_MPEG_VIDEO_ASPECT_16x9:
          desc = "16x9";
          break;
        case V4L2_MPEG_VIDEO_ASPECT_221x100:
          desc = "221x100";
          break;
        default:
          desc = "Unknown";
    }

    return SetExtControl(V4L2_CID_MPEG_VIDEO_ASPECT, value,
                         "Video Aspect ratio", desc);
}

bool V4L2util::SetVideoBitrateMode(int value)
{
    QString desc;
    switch (value)
    {
        case V4L2_MPEG_VIDEO_BITRATE_MODE_VBR:
          desc = "VBR";
          break;
        case V4L2_MPEG_VIDEO_BITRATE_MODE_CBR:
          desc = "CBR";
          break;
    }

    return SetExtControl(V4L2_CID_MPEG_VIDEO_BITRATE_MODE, value,
                         "Video Bitrate Mode", desc);
}

bool V4L2util::SetVideoBitrate(int value)
{
    QString desc = QString("%1").arg(value);
    return SetExtControl(V4L2_CID_MPEG_VIDEO_BITRATE, value,
                         "Video Average Bitrate", desc);
}

bool V4L2util::SetVideoBitratePeak(int value)
{
    QString desc = QString("%1").arg(value);
    return SetExtControl(V4L2_CID_MPEG_VIDEO_BITRATE_PEAK, value,
                         "Video Peak Bitrate", desc);
}

bool V4L2util::SetResolution(uint32_t width, uint32_t height)
{
    struct v4l2_format vfmt;
    memset(&vfmt, 0, sizeof(vfmt));

    vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(m_fd, VIDIOC_G_FMT, &vfmt) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "SetResolution() -- Error getting format" + ENO);
        return false;
    }

    if ((vfmt.fmt.pix.width == width) && (vfmt.fmt.pix.height == height))
    {
        LOG(VB_RECORD, LOG_INFO, LOC + QString("Resolution is already %1x%2")
            .arg(width).arg(height));
        return true;
    }

    vfmt.fmt.pix.width  = width;
    vfmt.fmt.pix.height = height;

    if (ioctl(m_fd, VIDIOC_S_FMT, &vfmt) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "SetResolution() -- Error setting format" + ENO);
        return false;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + QString("Resolution set to %1x%2")
        .arg(width).arg(height));
    return true;
}

// Audio controls
bool V4L2util::SetAudioInput(int value)
{
    struct v4l2_audio ain = { 0, };

    ain.index = value;
    if (ioctl(m_fd, VIDIOC_ENUMAUDIO, &ain) < 0)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Failed to retrieve audio input.") + ENO);
        return false;
    }

    ain.index = value;
    if (ioctl(m_fd, VIDIOC_S_AUDIO, &ain) < 0)
    {
        LOG(VB_GENERAL, LOG_WARNING,
            LOC + QString("Failed to set audio input to %1.").arg(value) + ENO);
        return false;
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Audio input set to %1.")
        .arg(value));
    return true;
}

bool V4L2util::SetAudioCodec(int value)
{
#if 0
    if (DriverName().startsWith("ivtv"))
        value = V4L2_MPEG_AUDIO_ENCODING_LAYER_2;
#endif

    QString desc;
    switch (value)
    {
        case V4L2_MPEG_AUDIO_ENCODING_LAYER_1:
          desc = "Layer I";
          break;
        case V4L2_MPEG_AUDIO_ENCODING_LAYER_2:
          desc = "Layer II";
          break;
        case V4L2_MPEG_AUDIO_ENCODING_LAYER_3:
          desc = "Layer III";
          break;
        case V4L2_MPEG_AUDIO_ENCODING_AAC:
          desc = "AAC";
          break;
        case V4L2_MPEG_AUDIO_ENCODING_AC3:
          desc = "AC3";
          break;
        default:
          desc = "Unknown";
          break;
    }

#if 0
    if (DriverName().startsWith("ivtv"))
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("Overriding AudioCodec for %1 to %2")
            .arg(DriverName()).arg(desc));
    }
#endif

    return SetExtControl(V4L2_CID_MPEG_AUDIO_ENCODING, value,
                         "Audio Codec", desc);
}


bool V4L2util::SetVolume(int volume)
{
    // Get volume min/max values
    struct v4l2_queryctrl qctrl;
    memset(&qctrl, 0 , sizeof(struct v4l2_queryctrl));
    qctrl.id = V4L2_CID_AUDIO_VOLUME;
    if ((ioctl(m_fd, VIDIOC_QUERYCTRL, &qctrl) < 0) ||
        (qctrl.flags & V4L2_CTRL_FLAG_DISABLED))
    {
        LOG(VB_CHANNEL, LOG_WARNING,
            LOC + "SetRecordingVolume() -- Audio volume control not supported.");
        return false;
    }

    // calculate volume in card units.
    int range = qctrl.maximum - qctrl.minimum;
    int value = (int) ((range * volume * 0.01f) + qctrl.minimum);
    int ctrl_volume = std::min(qctrl.maximum, std::max(qctrl.minimum, value));

    // Set recording volume
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_AUDIO_VOLUME;
    ctrl.value = ctrl_volume;

    if (ioctl(m_fd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "SetRecordingVolume() -- Failed to set recording volume" + ENO);
//            "If you are using an AverMedia M179 card this is normal."
        return false;
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "SetRecordingVolume() -- volume set.");
    return true;
}

bool V4L2util::SetLanguageMode(int mode)
{
    struct v4l2_tuner vt = { 0, };

    if (ioctl(m_fd, VIDIOC_G_TUNER, &vt) < 0)
    {
        LOG(VB_CHANNEL, LOG_WARNING, LOC +
            "SetLanguageMode() -- Failed to retrieve audio mode" + ENO);
        return false;
    }

    vt.audmode = mode;

    if (ioctl(m_fd, VIDIOC_S_TUNER, &vt) < 0)
    {
        LOG(VB_CHANNEL, LOG_WARNING, LOC +
            "SetLanguageMode -- Failed to set audio mode" + ENO);
        return false;
    }

    QString desc;
    switch (mode)
    {
        case V4L2_TUNER_MODE_MONO:
          desc = "Mono";
          break;
        case V4L2_TUNER_MODE_STEREO:
          desc = "Stereo";
          break;
#if 0
        case V4L2_TUNER_MODE_LANG2:
          desc = "Lang2";
          break;
#endif
        case V4L2_TUNER_MODE_SAP:
          desc = "SAP";
          break;
        case V4L2_TUNER_MODE_LANG1:
          desc = "LANG1";
          break;
        case V4L2_TUNER_MODE_LANG1_LANG2:
          desc = "LANG1&Lang2";
          break;
        default:
          desc = "Unknown";
          break;
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Language Mode set to %1 (%2)")
        .arg(desc).arg(mode));
    return true;
}

bool V4L2util::SetAudioSamplingRate(int value)
{
    QString desc;

#if 0
    if (DriverName().startsWith("ivtv"))
        value = V4L2_MPEG_AUDIO_SAMPLING_FREQ_48000;
#endif

    switch (value)
    {
        case V4L2_MPEG_AUDIO_SAMPLING_FREQ_44100:
          desc = "44.1kHz";
          break;
        case V4L2_MPEG_AUDIO_SAMPLING_FREQ_48000:
          desc = "48kHz";
          break;
        case V4L2_MPEG_AUDIO_SAMPLING_FREQ_32000:
          desc = "32kHz";
          break;
        default:
          desc = "Unknown";
    }

#if 0
    if (DriverName().startsWith("ivtv"))
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("Overriding sampling frequence for %1 to %2")
            .arg(DriverName()).arg(desc));
    }
#endif

    return SetExtControl(V4L2_CID_MPEG_AUDIO_SAMPLING_FREQ, value,
                         "Audio Sample Rate", desc);
}

bool V4L2util::SetAudioBitrateL2(int value)
{
    QString desc;
    switch (value)
    {
        case V4L2_MPEG_AUDIO_L2_BITRATE_32K:
          desc = "32K";
          break;
        case V4L2_MPEG_AUDIO_L2_BITRATE_48K:
          desc = "48K";
          break;
        case V4L2_MPEG_AUDIO_L2_BITRATE_56K:
          desc = "56K";
          break;
        case V4L2_MPEG_AUDIO_L2_BITRATE_64K:
          desc = "64K";
          break;
        case V4L2_MPEG_AUDIO_L2_BITRATE_80K:
          desc = "80K";
          break;
        case V4L2_MPEG_AUDIO_L2_BITRATE_96K:
          desc = "96K";
          break;
        case V4L2_MPEG_AUDIO_L2_BITRATE_112K:
          desc = "112K";
          break;
        case V4L2_MPEG_AUDIO_L2_BITRATE_128K:
          desc = "128K";
          break;
        case V4L2_MPEG_AUDIO_L2_BITRATE_160K:
          desc = "160K";
          break;
        case V4L2_MPEG_AUDIO_L2_BITRATE_192K:
          desc = "192K";
          break;
        case V4L2_MPEG_AUDIO_L2_BITRATE_224K:
          desc = "224K";
          break;
        case V4L2_MPEG_AUDIO_L2_BITRATE_256K:
          desc = "256K";
          break;
        case V4L2_MPEG_AUDIO_L2_BITRATE_320K:
          desc = "320K";
          break;
        case V4L2_MPEG_AUDIO_L2_BITRATE_384K:
          desc = "384K";
          break;
        default:
          desc = "Unknown";
    }

    return SetExtControl(V4L2_CID_MPEG_AUDIO_L2_BITRATE, value,
                         "Audio L2 Bitrate", desc);
}

// Actions
bool V4L2util::SetEncoderState(int mode, const QString& desc)
{
    struct v4l2_encoder_cmd command = { 0, };

    command.cmd   = mode;
    if (ioctl(m_fd, VIDIOC_ENCODER_CMD, &command) != 0 && errno != ENOTTY)
    {
        // Some drivers do not support this ioctl at all.  It is marked as
        // "experimental" in the V4L2 API spec. These drivers return EINVAL
        // in older kernels and ENOTTY in 3.1+
        LOG(VB_CHANNEL, LOG_WARNING, LOC +
            QString("SetEncoderState(%1) -- failed").arg(desc) + ENO);
        return false;
    }
    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("SetEncoderState(%1) -- success").arg(desc));
    return true;
}

bool V4L2util::StartEncoding(void)
{
    return SetEncoderState(V4L2_ENC_CMD_START, "Start");
}

bool V4L2util::StopEncoding(void)
{
    return SetEncoderState(V4L2_ENC_CMD_STOP, "Stop");
}

bool V4L2util::PauseEncoding(void)
{
    return SetEncoderState(V4L2_ENC_CMD_PAUSE, "Pause");
}

bool V4L2util::ResumeEncoding(void)
{
    return SetEncoderState(V4L2_ENC_CMD_RESUME, "Resume");
}

bool V4L2util::OpenVBI(const QString& vbi_dev_name)
{
    return false;
}

bool V4L2util::SetSlicedVBI(const VBIMode::vbimode_t& vbimode)
{
    struct v4l2_format vbifmt = { 0, };

    vbifmt.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
    vbifmt.fmt.sliced.service_set |= (VBIMode::PAL_TT == vbimode) ?
                                     V4L2_SLICED_VBI_625 : V4L2_SLICED_VBI_525;

    int fd = m_vbi_fd < 0 ? m_fd : m_vbi_fd;

    if (ioctl(fd, VIDIOC_S_FMT, &vbifmt) < 0)
    {
        LOG(VB_CHANNEL, LOG_WARNING, LOC + "ConfigureVBI() -- " +
            "Failed to enable VBI embedding (/dev/videoX)" + ENO);
        return false;
    }

    if (ioctl(fd, VIDIOC_G_FMT, &vbifmt) >= 0)
    {
        LOG(VB_RECORD, LOG_INFO,
            LOC + QString("VBI service: %1, io size: %2")
            .arg(vbifmt.fmt.sliced.service_set)
            .arg(vbifmt.fmt.sliced.io_size));

        // V4L2_MPEG_STREAM_VBI_FMT_NONE = 0,  /* No VBI in the MPEG stream */
        // V4L2_MPEG_STREAM_VBI_FMT_IVTV = 1,  /* VBI in private packets, IVTV form
        return SetExtControl(V4L2_CID_MPEG_STREAM_VBI_FMT,
                             V4L2_MPEG_STREAM_VBI_FMT_IVTV,
                             "MPEG Stream VBI format",
                             "VBI in private packets, IVTV form");

    }

    return false;
}
