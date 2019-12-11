// Std C headers
#include <cstdio>
#include <cstdlib>
#include <cerrno>

// POSIX headers
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

// C++ headers
#include <algorithm>
#include <iostream>
using namespace std;

#include <linux/videodev2.h>

// MythTV headers
#include "v4lchannel.h"
#include "frequencies.h"
#include "tv_rec.h"
#include "mythdb.h"
#include "channelutil.h"
#include "cardutil.h"

#define DEBUG_ATTRIB 1

#define LOC      QString("V4LChannel[%1](%2): ") \
                 .arg(m_inputid).arg(GetDevice())

static int format_to_mode(const QString &fmt);
static QString mode_to_format(int mode);

V4LChannel::~V4LChannel(void)
{
    V4LChannel::Close();
}

bool V4LChannel::Init(QString &startchannel, bool setchan)
{
    if (setchan)
    {
        SetFormat(gCoreContext->GetSetting("TVFormat"));
        SetDefaultFreqTable(gCoreContext->GetSetting("FreqTable"));
    }
    return ChannelBase::Init(startchannel, setchan);
}

bool V4LChannel::Open(void)
{
#if FAKE_VIDEO
    return true;
#endif
    if (m_videofd >= 0)
        return true;

    QByteArray ascii_device = m_device.toLatin1();
    m_videofd = open(ascii_device.constData(), O_RDWR);
    if (m_videofd < 0)
    {
         LOG(VB_GENERAL, LOG_ERR, LOC + "Can't open video device." + ENO);
         return false;
    }

    uint32_t version;
    uint32_t capabilities;
    if (!CardUtil::GetV4LInfo(m_videofd, m_device_name, m_driver_name,
                              version, capabilities))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to query capabilities." + ENO);
        Close();
        return false;
    }

    m_has_stream_io  = ((capabilities & V4L2_CAP_STREAMING)          != 0U);
    m_has_std_io     = ((capabilities & V4L2_CAP_READWRITE)          != 0U);
    m_has_async_io   = ((capabilities & V4L2_CAP_ASYNCIO)            != 0U);
    m_has_tuner      = ((capabilities & V4L2_CAP_TUNER)              != 0U);
    m_has_sliced_vbi = ((capabilities & V4L2_CAP_SLICED_VBI_CAPTURE) != 0U);

    if (m_driver_name == "bttv" || m_driver_name == "cx8800" || m_driver_name == "cx88_blackbird"
        || m_driver_name == "saa7164")
        m_has_stream_io = false; // driver workaround, see #9825, #10519 and #12336

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Device name '%1' driver '%2'.")
            .arg(m_device_name).arg(m_driver_name));

    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("v4l2: stream io: %2 std io: %3 async io: %4 "
                "tuner %5 sliced vbi %6")
            .arg(m_has_stream_io).arg(m_has_std_io).arg(m_has_async_io)
            .arg(m_has_tuner).arg(m_has_sliced_vbi));

    if (!InitializeInputs())
    {
        Close();
        return false;
    }

    SetFormat("Default");

    return true;
}

void V4LChannel::Close(void)
{
    if (m_videofd >= 0)
        close(m_videofd);
    m_videofd = -1;
}

void V4LChannel::SetFd(int fd)
{
    if (fd != m_videofd)
        Close();
    m_videofd = (fd >= 0) ? fd : -1;
}

static int format_to_mode(const QString &fmt)
{
    if (fmt == "PAL-BG")
        return V4L2_STD_PAL_BG;
    if (fmt == "PAL-D")
        return V4L2_STD_PAL_D;
    if (fmt == "PAL-DK")
        return V4L2_STD_PAL_DK;
    if (fmt == "PAL-I")
        return V4L2_STD_PAL_I;
    if (fmt == "PAL-60")
        return V4L2_STD_PAL_60;
    if (fmt == "SECAM")
        return V4L2_STD_SECAM;
    if (fmt == "SECAM-D")
        return V4L2_STD_SECAM_D;
    if (fmt == "SECAM-DK")
        return V4L2_STD_SECAM_DK;
    if (fmt == "PAL-NC")
        return V4L2_STD_PAL_Nc;
    if (fmt == "PAL-M")
        return V4L2_STD_PAL_M;
    if (fmt == "PAL-N")
        return V4L2_STD_PAL_N;
    if (fmt == "NTSC-JP")
        return V4L2_STD_NTSC_M_JP;
        // generics...
    if (fmt.startsWith("NTSC"))
        return V4L2_STD_NTSC;
    if (fmt.startsWith("ATSC"))
        return V4L2_STD_NTSC; // We've dropped V4L ATSC support...
    if (fmt.startsWith("PAL"))
        return V4L2_STD_PAL;
    return V4L2_STD_NTSC;
}

static QString mode_to_format(int mode)
{
    if (mode == V4L2_STD_NTSC)
        return "NTSC";
    if (mode == V4L2_STD_NTSC_M_JP)
        return "NTSC-JP";
    if (mode == V4L2_STD_PAL)
        return "PAL";
    if (mode == V4L2_STD_PAL_60)
        return "PAL-60";
    if (mode == V4L2_STD_PAL_BG)
        return "PAL-BG";
    if (mode == V4L2_STD_PAL_D)
        return "PAL-D";
    if (mode == V4L2_STD_PAL_DK)
        return "PAL-DK";
    if (mode == V4L2_STD_PAL_I)
        return "PAL-I";
    if (mode == V4L2_STD_PAL_M)
        return "PAL-M";
    if (mode == V4L2_STD_PAL_N)
        return "PAL-N";
    if (mode == V4L2_STD_PAL_Nc)
        return "PAL-NC";
    if (mode == V4L2_STD_SECAM)
        return "SECAM";
    if (mode == V4L2_STD_SECAM_D)
        return "SECAM-D";
    // generic..
    if ((V4L2_STD_NTSC_M      == mode) ||
        (V4L2_STD_NTSC_443    == mode) ||
        (V4L2_STD_NTSC_M_KR   == mode))
        return "NTSC";
    if ((V4L2_STD_PAL_B       == mode) ||
        (V4L2_STD_PAL_B1      == mode) ||
        (V4L2_STD_PAL_G       == mode) ||
        (V4L2_STD_PAL_H       == mode) ||
        (V4L2_STD_PAL_D1      == mode) ||
        (V4L2_STD_PAL_K       == mode))
        return "PAL";
    if ((V4L2_STD_SECAM_B     == mode) ||
        (V4L2_STD_SECAM_DK    == mode) ||
        (V4L2_STD_SECAM_G     == mode) ||
        (V4L2_STD_SECAM_H     == mode) ||
        (V4L2_STD_SECAM_K     == mode) ||
        (V4L2_STD_SECAM_K1    == mode) ||
        (V4L2_STD_SECAM_L     == mode) ||
        (V4L2_STD_SECAM_LC    == mode))
        return "SECAM";
    if ((V4L2_STD_ATSC        == mode) ||
        (V4L2_STD_ATSC_8_VSB  == mode) ||
        (V4L2_STD_ATSC_16_VSB == mode))
    {
        // We've dropped V4L ATSC support, but this still needs to be
        // returned here so we will change the mode if the device is
        // in ATSC mode already.
        return "ATSC";
    }

    return "Unknown";
}

/** \fn V4LChannel::InitializeInputs(void)
 *  This enumerates the inputs, converts the format
 *  string to something the hardware understands, and
 *  if the parent pointer is valid retrieves the
 *  channels from the database.
 */
bool V4LChannel::InitializeInputs(void)
{
    // Get Inputs from DB
    if (!ChannelBase::InitializeInput())
        return false;

    // Get global TVFormat setting
    QString fmt = gCoreContext->GetSetting("TVFormat");
    LOG(VB_CHANNEL, LOG_INFO, QString("Global TVFormat Setting '%1'").arg(fmt));
    int videomode_v4l2 = format_to_mode(fmt.toUpper());

    bool ok = false;
    InputNames v4l_inputs = CardUtil::ProbeV4LVideoInputs(m_videofd, ok);

    // Insert info from hardware
    uint valid_cnt = 0;
    InputNames::const_iterator v4l_it = v4l_inputs.begin();
    for (; v4l_it != v4l_inputs.end(); ++v4l_it)
    {
        if (*v4l_it == m_name)
        {
            m_inputNumV4L   = v4l_it.key();
            m_videoModeV4L2 = videomode_v4l2;
            valid_cnt++;
        }
    }

    // print it
    LOG(VB_CHANNEL, LOG_INFO, LOC +
        QString("Input #%1: '%2' schan(%3) tun(%4) v4l2(%6)")
        .arg(m_inputid).arg(m_name).arg(m_startChanNum)
        .arg(m_tuneToChannel)
        .arg(mode_to_format(m_videoModeV4L2)));

    return valid_cnt != 0U;
}

/** \fn V4LChannel::SetFormat(const QString &format)
 *  \brief Initializes tuner and modulator variables
 *
 *  \param format One of twelve formats:
 *                "NTSC",   "NTSC-JP", "ATSC",
 *                "SECAM",
 *                "PAL",    "PAL-BG",  "PAL-DK",   "PAL-I",
 *                "PAL-60", "PAL-NC",  "PAL-M", or "PAL-N"
 */
void V4LChannel::SetFormat(const QString &format)
{
    if (!Open())
        return;

    int inputNum = m_inputid;

    QString fmt = format;
    if ((fmt == "Default") || format.isEmpty())
    {
        if (m_inputid)
            fmt = mode_to_format(m_videoModeV4L2);
    }

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("SetFormat(%1) fmt(%2) input(%3)")
            .arg(format).arg(fmt).arg(inputNum));

    if (!SetInputAndFormat(inputNum, fmt))
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to set format." + ENO);

}

int V4LChannel::SetDefaultFreqTable(const QString &name)
{
    m_defaultFreqTable = SetFreqTable(name);
    return m_defaultFreqTable;
}

void V4LChannel::SetFreqTable(const int index)
{
    m_curList = chanlists[index].list;
    m_totalChannels = chanlists[index].count;
}

int V4LChannel::SetFreqTable(const QString &tablename)
{
    const QString& name = tablename;
    bool use_default = (name.toLower() == "default" || name.isEmpty());

    int i = 0;
    char *listname = (char *)chanlists[i].name;

    m_curList = nullptr;
    while (listname != nullptr)
    {
        if (use_default)
        {
            if (i == m_defaultFreqTable)
            {
                SetFreqTable(i);
                return i;
            }
        }
        else if (name == listname)
        {
            SetFreqTable(i);
            return i;
        }
        i++;
        listname = (char *)chanlists[i].name;
    }

    LOG(VB_CHANNEL, LOG_ERR,
        QString("Channel(%1)::SetFreqTable(): Invalid "
                "frequency table name %2, using %3.").
            arg(m_device).arg(name).arg((char *)chanlists[1].name));
    SetFreqTable(1);
    return 1;
}

int V4LChannel::GetCurrentChannelNum(const QString &channame)
{
    for (int i = 0; i < m_totalChannels; i++)
    {
        if (channame == m_curList[i].name)
            return i;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC +
        QString("GetCurrentChannelNum(%1): Failed to find Channel")
            .arg(channame));

    return -1;
}

bool V4LChannel::Tune(const QString &freqid, int finetune)
{
    int i = GetCurrentChannelNum(freqid);
    LOG(VB_CHANNEL, LOG_INFO,
        QString("Channel(%1)::Tune(%2): curList[%3].freq(%4)")
            .arg(m_device).arg(freqid).arg(i)
            .arg((i != -1) ? m_curList[i].freq : -1));

    if (i == -1)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Channel(%1)::Tune(%2): Error, failed to find channel.")
                .arg(m_device).arg(freqid));
        return false;
    }

    int frequency = (m_curList[i].freq + finetune) * 1000;

    return Tune(frequency);
}

bool V4LChannel::Tune(const DTVMultiplex &tuning)
{
    return Tune(tuning.m_frequency - 1750000); // to visual carrier
}

/** \brief Tunes to a specific frequency (Hz) on a particular input
 *
 *  \note This function always uses modulator zero.
 *  \note Unlike digital tuning functions this accepts the visual carrier
 *        frequency and not the center frequency.
 *
 *  \param frequency Frequency in Hz, this is divided by 62.5 kHz or 62.5 Hz
 *                   depending on the modulator and sent to the hardware.
 */
bool V4LChannel::Tune(uint64_t frequency)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("Tune(%1)").arg(frequency));

    int ioctlval = 0;

    uint mplexid_restriction;
    uint chanid_restriction;
    if (!IsInputAvailable(mplexid_restriction, chanid_restriction))
        return false;

    // Video4Linux version 2 tuning
    bool isTunerCapLow = false;
    struct v4l2_modulator mod {};
    mod.index = 0;
    ioctlval = ioctl(m_videofd, VIDIOC_G_MODULATOR, &mod);
    if (ioctlval >= 0)
    {
        isTunerCapLow = ((mod.capability & V4L2_TUNER_CAP_LOW) != 0U);
        LOG(VB_CHANNEL, LOG_INFO,
            QString("  name: %1").arg((char *)mod.name));
        LOG(VB_CHANNEL, LOG_INFO, QString("CapLow: %1").arg(isTunerCapLow));
    }

    struct v4l2_frequency vf {};
    vf.tuner = 0; // use first tuner
    vf.frequency = (isTunerCapLow) ?
        ((int)(frequency / 62.5)) : (frequency / 62500);

    vf.type = V4L2_TUNER_ANALOG_TV;

    ioctlval = ioctl(m_videofd, VIDIOC_S_FREQUENCY, &vf);
    if (ioctlval < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Channel(%1)::Tune(): Error %2 "
                    "while setting frequency (v2): %3")
            .arg(m_device).arg(ioctlval).arg(strerror(errno)));
        return false;
    }
    ioctlval = ioctl(m_videofd, VIDIOC_G_FREQUENCY, &vf);

    if (ioctlval >= 0)
    {
        LOG(VB_CHANNEL, LOG_INFO,
            QString("Channel(%1)::Tune(): Frequency is now %2")
            .arg(m_device).arg(vf.frequency * 62500));
    }

    return true;
}

/** \fn V4LChannel::Retune(void)
 *  \brief Retunes to last tuned frequency.
 *
 *  NOTE: This only works for V4L2 and only for analog tuning.
 */
bool V4LChannel::Retune(void)
{
    struct v4l2_frequency vf {};
    vf.tuner = 0; // use first tuner
    vf.type = V4L2_TUNER_ANALOG_TV;

    // Get the last tuned frequency
    int ioctlval = ioctl(m_videofd, VIDIOC_G_FREQUENCY, &vf);
    if (ioctlval < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Retune failed (1)" + ENO);
        return false;
    }

    // Set the last tuned frequency again...
    ioctlval = ioctl(m_videofd, VIDIOC_S_FREQUENCY, &vf);
    if (ioctlval < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Retune failed (2)" + ENO);
        return false;
    }

    return true;
}

QString V4LChannel::GetFormatForChannel(const QString& channum, const QString& inputname)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT tvformat "
        "FROM channel, capturecard "
        "WHERE deleted              IS NULL      AND "
        "      channum              = :CHANNUM   AND "
        "      inputname            = :INPUTNAME AND "
        "      capturecard.cardid   = :INPUTID   AND "
        "      capturecard.sourceid = channel.sourceid");
    query.bindValue(":CHANNUM",   channum);
    query.bindValue(":INPUTNAME", inputname);
    query.bindValue(":INPUTID",    m_inputid);

    QString fmt;
    if (!query.exec() || !query.isActive())
        MythDB::DBError("GetFormatForChannel:find format", query);
    else if (query.next())
        fmt = query.value(0).toString();
    return fmt;
}

bool V4LChannel::SetInputAndFormat(int inputNum, const QString& newFmt)
{
    if (!m_inputid || m_inputNumV4L < 0)
        return false;

    int inputNumV4L = m_inputNumV4L;
    bool ok = true;

    QString msg =
        QString("SetInputAndFormat(%1, %2) ").arg(inputNum).arg(newFmt);

    struct v4l2_input input {};
    int ioctlval = ioctl(m_videofd, VIDIOC_G_INPUT, &input);
    bool input_switch = (0 != ioctlval || (uint)inputNumV4L != input.index);

    const v4l2_std_id new_vid_mode = format_to_mode(newFmt);
    v4l2_std_id cur_vid_mode;
    ioctlval = ioctl(m_videofd, VIDIOC_G_STD, &cur_vid_mode);
    bool mode_switch = (0 != ioctlval || new_vid_mode != cur_vid_mode);
    bool needs_switch = input_switch || mode_switch;

    LOG(VB_GENERAL, LOG_INFO, LOC + msg + "(v4l v2) " +
        QString("input_switch: %1 mode_switch: %2")
        .arg(input_switch).arg(mode_switch));

    // ConvertX (wis-go7007) requires streaming to be disabled
    // before an input switch, do this if CAP_STREAMING is set.
    bool streamingDisabled = false;
    int  streamType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (needs_switch && m_has_stream_io)
    {
        ioctlval = ioctl(m_videofd, VIDIOC_STREAMOFF, &streamType);
        if (ioctlval < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + msg +
                "\n\t\t\twhile disabling streaming (v4l v2)" + ENO);
            ok = false;
        }
        else
        {
            streamingDisabled = true;
        }
    }

    if (input_switch)
    {
        // Send the input switch ioctl.
        ioctlval = ioctl(m_videofd, VIDIOC_S_INPUT, &inputNumV4L);
        if (ioctlval < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + msg +
                "\n\t\t\twhile setting input (v4l v2)" + ENO);

            ok = false;
        }
    }

    if (mode_switch)
    {
        ioctlval = ioctl(m_videofd, VIDIOC_S_STD, &new_vid_mode);
        if (ioctlval < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + msg +
                "\n\t\t\twhile setting format (v4l v2)" + ENO);

            ok = false;
        }
    }

    if (streamingDisabled)
    {
        ioctlval = ioctl(m_videofd, VIDIOC_STREAMON, &streamType);
        if (ioctlval < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + msg +
                "\n\t\t\twhile reenabling streaming (v4l v2)" + ENO);

            ok = false;
        }
    }

    return ok;
}

static int get_v4l2_attribute(const QString &db_col_name)
{
    if ("brightness" == db_col_name)
        return V4L2_CID_BRIGHTNESS;
    if ("contrast" == db_col_name)
        return V4L2_CID_CONTRAST;
    if ("colour" == db_col_name)
        return V4L2_CID_SATURATION;
    if ("hue" == db_col_name)
        return V4L2_CID_HUE;
    return -1;
}

bool V4LChannel::InitPictureAttribute(const QString &db_col_name)
{
    if (!m_pParent)
        return false;

    int v4l2_attrib = get_v4l2_attribute(db_col_name);
    if (v4l2_attrib == -1)
        return false;

    int cfield = ChannelUtil::GetChannelValueInt(
        db_col_name, GetSourceID(), m_curchannelname);
    int sfield = CardUtil::GetValueInt(
        db_col_name, m_inputid);

    if ((cfield == -1) || (sfield == -1))
        return false;

    QString loc = LOC +
        QString("InitPictureAttribute(%1): ").arg(db_col_name, 10);

    struct v4l2_control ctrl {};
    struct v4l2_queryctrl qctrl {};

    ctrl.id = qctrl.id = v4l2_attrib;
    if (ioctl(m_videofd, VIDIOC_QUERYCTRL, &qctrl) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, loc + "failed to query controls." + ENO);
        return false;
    }

    float new_range = qctrl.maximum - qctrl.minimum;
    float old_range = 65535 - 0;
    float scl_range = new_range / old_range;
    float dfl       = (qctrl.default_value - qctrl.minimum) / new_range;
    int   norm_dfl  = (0x10000 + (int)(dfl * old_range) - 32768) & 0xFFFF;

    if (m_pict_attr_default.find(db_col_name) == m_pict_attr_default.end())
    {
        if (m_device_name == "pcHDTV HD3000 HDTV")
        {
            m_pict_attr_default["brightness"] = 9830;
            m_pict_attr_default["contrast"]   = 39322;
            m_pict_attr_default["colour"]     = 45875;
            m_pict_attr_default["hue"]        = 0;
        }
        else
        {
            m_pict_attr_default[db_col_name] = norm_dfl;
        }
    }

    int dfield = m_pict_attr_default[db_col_name];
    int field  = (cfield + sfield + dfield) & 0xFFFF;
    int value0 = (int) ((scl_range * field) + qctrl.minimum);
    int value1 = min(value0, qctrl.maximum);
    ctrl.value = max(value1, qctrl.minimum);

#if DEBUG_ATTRIB
    LOG(VB_CHANNEL, LOG_DEBUG, loc + QString(" %1\n\t\t\t"
                                             "[%2,%3] dflt(%4, %5, %6)")
        .arg(value0).arg(qctrl.minimum, 5).arg(qctrl.maximum, 5)
        .arg(qctrl.default_value, 5).arg(dfl, 4, 'f', 2)
        .arg(norm_dfl));
#endif

    if (ioctl(m_videofd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, loc + "failed to set controls" + ENO);
        return false;
    }

    return true;
}

bool V4LChannel::InitPictureAttributes(void)
{
    return (InitPictureAttribute("brightness") &&
            InitPictureAttribute("contrast")   &&
            InitPictureAttribute("colour")     &&
            InitPictureAttribute("hue"));
}

int V4LChannel::GetPictureAttribute(PictureAttribute attr) const
{
    QString db_col_name = toDBString(attr);
    if (db_col_name.isEmpty())
        return -1;

    int cfield = ChannelUtil::GetChannelValueInt(
        db_col_name, GetSourceID(), m_curchannelname);
    int sfield = CardUtil::GetValueInt(
        db_col_name, m_inputid);
    int dfield = 0;

    if (m_pict_attr_default.find(db_col_name) != m_pict_attr_default.end())
        dfield = m_pict_attr_default[db_col_name];

    int val = (cfield + sfield + dfield) & 0xFFFF;

#if DEBUG_ATTRIB
    LOG(VB_CHANNEL, LOG_DEBUG,
        QString("GetPictureAttribute(%1) -> cdb %2 rdb %3 d %4 -> %5")
            .arg(db_col_name).arg(cfield).arg(sfield)
            .arg(dfield).arg(val));
#endif

    return val;
}

static int get_v4l2_attribute_value(int videofd, int v4l2_attrib)
{
    struct v4l2_control ctrl {};
    struct v4l2_queryctrl qctrl {};

    ctrl.id = qctrl.id = v4l2_attrib;
    if (ioctl(videofd, VIDIOC_QUERYCTRL, &qctrl) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "get_v4l2_attribute_value: failed to query controls (1)" + ENO);
        return -1;
    }

    if (ioctl(videofd, VIDIOC_G_CTRL, &ctrl) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "get_v4l2_attribute_value: failed to get controls (2)" + ENO);
        return -1;
    }

    float mult = 65535.0 / (qctrl.maximum - qctrl.minimum);
    return min(max((int)(mult * (ctrl.value - qctrl.minimum)), 0), 65525);
}

static int set_v4l2_attribute_value(int videofd, int v4l2_attrib, int newvalue)
{
    struct v4l2_control ctrl {};
    struct v4l2_queryctrl qctrl {};

    ctrl.id = qctrl.id = v4l2_attrib;
    if (ioctl(videofd, VIDIOC_QUERYCTRL, &qctrl) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "set_v4l2_attribute_value: failed to query control" + ENO);
        return -1;
    }

    float mult = (qctrl.maximum - qctrl.minimum) / 65535.0;
    ctrl.value = (int)(mult * newvalue + qctrl.minimum);
    ctrl.value = min(ctrl.value, qctrl.maximum);
    ctrl.value = max(ctrl.value, qctrl.minimum);

    if (ioctl(videofd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "set_v4l2_attribute_value: failed to set control" + ENO);
        return -1;
    }

    return 0;
}

int V4LChannel::ChangePictureAttribute(
    PictureAdjustType type, PictureAttribute attr, bool up)
{
    if (!m_pParent)
        return -1;

    QString db_col_name = toDBString(attr);
    if (db_col_name.isEmpty())
        return -1;

    int v4l2_attrib = get_v4l2_attribute(db_col_name);
    if (v4l2_attrib == -1)
        return -1;

    // get the old attribute value from the hardware, this is
    // just a sanity check on whether this attribute exists
    if (get_v4l2_attribute_value(m_videofd, v4l2_attrib) < 0)
        return -1;

    int old_value = GetPictureAttribute(attr);
    int new_value = old_value + ((up) ? 655 : -655);

    // make sure we are within bounds (wrap around for hue)
    if (V4L2_CID_HUE == v4l2_attrib)
        new_value &= 0xffff;
    new_value = min(max(new_value, 0), 65535);

#if DEBUG_ATTRIB
    LOG(VB_CHANNEL, LOG_DEBUG,
        QString("ChangePictureAttribute(%1,%2,%3) cur %4 -> new %5")
            .arg(type).arg(db_col_name).arg(up)
            .arg(old_value).arg(new_value));
#endif

    // actually set the new attribute value on the hardware
    if (set_v4l2_attribute_value(m_videofd, v4l2_attrib, new_value) < 0)
        return -1;

    // tell the DB about the new attribute value
    if (kAdjustingPicture_Channel == type)
    {
        int adj_value = ChannelUtil::GetChannelValueInt(
            db_col_name, GetSourceID(), m_curchannelname);

        int tmp = new_value - old_value + adj_value;
        tmp = (tmp < 0)      ? tmp + 0x10000 : tmp;
        tmp = (tmp > 0xffff) ? tmp - 0x10000 : tmp;
        ChannelUtil::SetChannelValue(db_col_name, QString::number(tmp),
                                     GetSourceID(), m_curchannelname);
    }
    else if (kAdjustingPicture_Recording == type)
    {
        int adj_value = CardUtil::GetValueInt(
            db_col_name, m_inputid);

        int tmp = new_value - old_value + adj_value;
        tmp = (tmp < 0)      ? tmp + 0x10000 : tmp;
        tmp = (tmp > 0xffff) ? tmp - 0x10000 : tmp;
        CardUtil::SetValue(db_col_name, m_inputid, tmp);
    }

    return new_value;
}
