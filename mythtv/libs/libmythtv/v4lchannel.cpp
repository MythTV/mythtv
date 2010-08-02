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

#include <linux/videodev.h>
#include <linux/videodev2.h>

// MythTV headers
#include "v4lchannel.h"
#include "frequencies.h"
#include "tv_rec.h"
#include "mythdb.h"
#include "channelutil.h"
#include "cardutil.h"

#define DEBUG_ATTRIB 1

#define LOC QString("Channel(%1): ").arg(device)
#define LOC_WARN QString("Channel(%1) Warning: ").arg(device)
#define LOC_ERR QString("Channel(%1) Error: ").arg(device)

static int format_to_mode(const QString& fmt, int v4l_version);
static QString mode_to_format(int mode, int v4l_version);

V4LChannel::V4LChannel(TVRec *parent, const QString &videodevice)
    : DTVChannel(parent),
      device(videodevice),          videofd(-1),
      device_name(QString::null),   driver_name(QString::null),
      curList(NULL),                totalChannels(0),
      currentFormat(""),            is_dtv(false),
      usingv4l2(false),             defaultFreqTable(1)
{
}

V4LChannel::~V4LChannel(void)
{
    Close();
}

bool V4LChannel::Init(QString &inputname, QString &startchannel, bool setchan)
{
    if (setchan)
    {
        SetFormat(gCoreContext->GetSetting("TVFormat"));
        SetDefaultFreqTable(gCoreContext->GetSetting("FreqTable"));
    }
    return ChannelBase::Init(inputname, startchannel, setchan);
}

bool V4LChannel::Open(void)
{
#if FAKE_VIDEO
    return true;
#endif
    if (videofd >= 0)
        return true;

    QByteArray ascii_device = device.toAscii();
    videofd = open(ascii_device.constData(), O_RDWR);
    if (videofd < 0)
    {
         VERBOSE(VB_IMPORTANT,
                 QString("Channel(%1)::Open(): Can't open video device, "
                         "error \"%2\"").arg(device).arg(strerror(errno)));
         return false;
    }

    usingv4l2 = CardUtil::hasV4L2(videofd);
    CardUtil::GetV4LInfo(videofd, device_name, driver_name);
    VERBOSE(VB_CHANNEL, LOC + QString("Device name '%1' driver '%2'.")
            .arg(device_name).arg(driver_name));

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
    if (videofd >= 0)
        close(videofd);
    videofd = -1;
}

void V4LChannel::SetFd(int fd)
{
    if (fd != videofd)
        Close();
    videofd = (fd >= 0) ? fd : -1;
}

static int format_to_mode(const QString &fmt, int v4l_version)
{
    if (2 == v4l_version)
    {
        if (fmt == "PAL-BG")
            return V4L2_STD_PAL_BG;
        else if (fmt == "PAL-D")
            return V4L2_STD_PAL_D;
        else if (fmt == "PAL-DK")
            return V4L2_STD_PAL_DK;
        else if (fmt == "PAL-I")
            return V4L2_STD_PAL_I;
        else if (fmt == "PAL-60")
            return V4L2_STD_PAL_60;
        else if (fmt == "SECAM")
            return V4L2_STD_SECAM;
        else if (fmt == "SECAM-D")
            return V4L2_STD_SECAM_D;
        else if (fmt == "SECAM-DK")
            return V4L2_STD_SECAM_DK;
        else if (fmt == "PAL-NC")
            return V4L2_STD_PAL_Nc;
        else if (fmt == "PAL-M")
            return V4L2_STD_PAL_M;
        else if (fmt == "PAL-N")
            return V4L2_STD_PAL_N;
        else if (fmt == "NTSC-JP")
            return V4L2_STD_NTSC_M_JP;
        // generics...
        else if (fmt.left(4) == "NTSC")
            return V4L2_STD_NTSC;
        else if (fmt.left(4) == "ATSC")
            return V4L2_STD_NTSC; // We've dropped V4L ATSC support...
        else if (fmt.left(3) == "PAL")
            return V4L2_STD_PAL;
        return V4L2_STD_NTSC;
    }
    else if (1 == v4l_version)
    {
        if (fmt == "NTSC-JP")
            return 6;
        else if (fmt.left(5) == "SECAM")
            return VIDEO_MODE_SECAM;
        else if (fmt == "PAL-NC")
            return 3;
        else if (fmt == "PAL-M")
            return 4;
        else if (fmt == "PAL-N")
            return 5;
        // generics...
        else if (fmt.left(3) == "PAL")
            return VIDEO_MODE_PAL;
        else if (fmt.left(4) == "NTSC")
            return VIDEO_MODE_NTSC;
        else if (fmt.left(4) == "ATSC")
            return VIDEO_MODE_NTSC; // We've dropped V4L ATSC support...
        return VIDEO_MODE_NTSC;
    }

    VERBOSE(VB_IMPORTANT,
            "format_to_mode() does not recognize V4L" << v4l_version);

    return V4L2_STD_NTSC; // assume V4L version 2
}

static QString mode_to_format(int mode, int v4l_version)
{
    if (2 == v4l_version)
    {
        if (mode == V4L2_STD_NTSC)
            return "NTSC";
        else if (mode == V4L2_STD_NTSC_M_JP)
            return "NTSC-JP";
        else if (mode == V4L2_STD_PAL)
            return "PAL";
        else if (mode == V4L2_STD_PAL_60)
            return "PAL-60";
        else if (mode == V4L2_STD_PAL_BG)
            return "PAL-BG";
        else if (mode == V4L2_STD_PAL_D)
            return "PAL-D";
        else if (mode == V4L2_STD_PAL_DK)
            return "PAL-DK";
        else if (mode == V4L2_STD_PAL_I)
            return "PAL-I";
        else if (mode == V4L2_STD_PAL_M)
            return "PAL-M";
        else if (mode == V4L2_STD_PAL_N)
            return "PAL-N";
        else if (mode == V4L2_STD_PAL_Nc)
            return "PAL-NC";
        else if (mode == V4L2_STD_SECAM)
            return "SECAM";
        else if (mode == V4L2_STD_SECAM_D)
            return "SECAM-D";
        // generic..
        else if ((V4L2_STD_NTSC_M      == mode) ||
                 (V4L2_STD_NTSC_443    == mode) ||
                 (V4L2_STD_NTSC_M_KR   == mode))
            return "NTSC";
        else if ((V4L2_STD_PAL_B       == mode) ||
                 (V4L2_STD_PAL_B1      == mode) ||
                 (V4L2_STD_PAL_G       == mode) ||
                 (V4L2_STD_PAL_H       == mode) ||
                 (V4L2_STD_PAL_D1      == mode) ||
                 (V4L2_STD_PAL_K       == mode))
            return "PAL";
        else if ((V4L2_STD_SECAM_B     == mode) ||
                 (V4L2_STD_SECAM_DK    == mode) ||
                 (V4L2_STD_SECAM_G     == mode) ||
                 (V4L2_STD_SECAM_H     == mode) ||
                 (V4L2_STD_SECAM_K     == mode) ||
                 (V4L2_STD_SECAM_K1    == mode) ||
                 (V4L2_STD_SECAM_L     == mode) ||
                 (V4L2_STD_SECAM_LC    == mode))
            return "SECAM";
        else if ((V4L2_STD_ATSC        == mode) ||
                 (V4L2_STD_ATSC_8_VSB  == mode) ||
                 (V4L2_STD_ATSC_16_VSB == mode))
        {
            // We've dropped V4L ATSC support, but this still needs to be
            // returned here so we will change the mode if the device is
            // in ATSC mode already.
            return "ATSC";
        }
    }
    else if (1 == v4l_version)
    {
        if (mode == VIDEO_MODE_NTSC)
            return "NTSC";
        else if (mode == VIDEO_MODE_PAL)
            return "PAL";
        else if (mode == VIDEO_MODE_SECAM)
            return "SECAM";
        else if (mode == 3)
            return "PAL-NC";
        else if (mode == 5)
            return "PAL-N";
        else if (mode == 6)
            return "NTSC-JP";
    }
    else
    {
        VERBOSE(VB_IMPORTANT,
                "mode_to_format() does not recognize V4L" << v4l_version);
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
    if (!ChannelBase::InitializeInputs())
        return false;

    // Get global TVFormat setting
    QString fmt = gCoreContext->GetSetting("TVFormat");
    VERBOSE(VB_CHANNEL, QString("Global TVFormat Setting '%1'").arg(fmt));
    int videomode_v4l1 = format_to_mode(fmt.toUpper(), 1);
    int videomode_v4l2 = format_to_mode(fmt.toUpper(), 2);

    bool ok = false;
    InputNames v4l_inputs = CardUtil::ProbeV4LVideoInputs(videofd, ok);

    // Insert info from hardware
    uint valid_cnt = 0;
    InputMap::const_iterator it;
    for (it = m_inputs.begin(); it != m_inputs.end(); ++it)
    {
        InputNames::const_iterator v4l_it = v4l_inputs.begin();
        for (; v4l_it != v4l_inputs.end(); ++v4l_it)
        {
            if (*v4l_it == (*it)->name)
            {
                (*it)->inputNumV4L   = v4l_it.key();
                (*it)->videoModeV4L1 = videomode_v4l1;
                (*it)->videoModeV4L2 = videomode_v4l2;
                valid_cnt++;
            }
        }
    }

    // print em
    for (it = m_inputs.begin(); it != m_inputs.end(); ++it)
    {
        VERBOSE(VB_CHANNEL, LOC + QString("Input #%1: '%2' schan(%3) "
                                          "tun(%4) v4l1(%5) v4l2(%6)")
                .arg(it.key()).arg((*it)->name).arg((*it)->startChanNum)
                .arg((*it)->tuneToChannel)
                .arg(mode_to_format((*it)->videoModeV4L1,1))
                .arg(mode_to_format((*it)->videoModeV4L2,2)));
    }

    return valid_cnt;
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

    int inputNum = m_currentInputID;
    if (m_currentInputID < 0)
        inputNum = GetNextInputNum();

    QString fmt = format;
    if ((fmt == "Default") || format.isEmpty())
    {
        InputMap::const_iterator it = m_inputs.find(inputNum);
        if (it != m_inputs.end())
            fmt = mode_to_format((*it)->videoModeV4L2, 2);
    }

    VERBOSE(VB_CHANNEL, LOC + QString("SetFormat(%1) fmt(%2) input(%3)")
            .arg(format).arg(fmt).arg(inputNum));

    if ((fmt == currentFormat) || SetInputAndFormat(inputNum, fmt))
    {
        currentFormat = fmt;
        is_dtv        = (fmt == "ATSC");
    }
}

int V4LChannel::SetDefaultFreqTable(const QString &name)
{
    defaultFreqTable = SetFreqTable(name);
    return defaultFreqTable;
}

void V4LChannel::SetFreqTable(const int index)
{
    curList = chanlists[index].list;
    totalChannels = chanlists[index].count;
}

int V4LChannel::SetFreqTable(const QString &name)
{
    int i = 0;
    char *listname = (char *)chanlists[i].name;

    curList = NULL;
    while (listname != NULL)
    {
        if (name == listname)
        {
            SetFreqTable(i);
            return i;
        }
        i++;
        listname = (char *)chanlists[i].name;
    }

    VERBOSE(VB_CHANNEL, QString("Channel(%1)::SetFreqTable(): Invalid "
                                "frequency table name %2, using %3.").
            arg(device).arg(name).arg((char *)chanlists[1].name));
    SetFreqTable(1);
    return 1;
}

int V4LChannel::GetCurrentChannelNum(const QString &channame)
{
    for (int i = 0; i < totalChannels; i++)
    {
        if (channame == curList[i].name)
            return i;
    }

    VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("GetCurrentChannelNum(%1): "
                    "Failed to find Channel").arg(channame));

    return -1;
}

void V4LChannel::SaveCachedPids(const pid_cache_t &pid_cache) const
{
    int chanid = GetChanID();
    if (chanid > 0)
        DTVChannel::SaveCachedPids(chanid, pid_cache);
}

void V4LChannel::GetCachedPids(pid_cache_t &pid_cache) const
{
    int chanid = GetChanID();
    if (chanid > 0)
        DTVChannel::GetCachedPids(chanid, pid_cache);
}

bool V4LChannel::SetChannelByString(const QString &channum)
{
    QString loc = LOC + QString("SetChannelByString(%1)").arg(channum);
    QString loc_err = loc + ", Error: ";
    VERBOSE(VB_CHANNEL, loc);

    if (!Open())
    {
        VERBOSE(VB_IMPORTANT, loc_err + "Channel object "
                "will not open, can not change channels.");

        return false;
    }

    QString inputName;
    if (!CheckChannel(channum, inputName))
    {
        VERBOSE(VB_IMPORTANT, loc_err +
                "CheckChannel failed.\n\t\t\tPlease verify the channel "
                "in the 'mythtv-setup' Channel Editor.");

        return false;
    }

    // If CheckChannel filled in the inputName then we need to
    // change inputs and return, since the act of changing
    // inputs will change the channel as well.
    if (!inputName.isEmpty())
        return ChannelBase::SelectInput(inputName, channum, false);

    ClearDTVInfo();

    InputMap::const_iterator it = m_inputs.find(m_currentInputID);
    if (it == m_inputs.end())
        return false;

    uint mplexid_restriction;
    if (!IsInputAvailable(m_currentInputID, mplexid_restriction))
        return false;

    // Fetch tuning data from the database.
    QString tvformat, modulation, freqtable, freqid, dtv_si_std;
    int finetune;
    uint64_t frequency;
    int mpeg_prog_num;
    uint atsc_major, atsc_minor, mplexid, tsid, netid;

    if (!ChannelUtil::GetChannelData(
        (*it)->sourceid, channum,
        tvformat, modulation, freqtable, freqid,
        finetune, frequency,
        dtv_si_std, mpeg_prog_num, atsc_major, atsc_minor, tsid, netid,
        mplexid, m_commfree))
    {
        return false;
    }

    if (mplexid_restriction && (mplexid != mplexid_restriction))
        return false;

    // If the frequency is zeroed out, don't use it directly.
    bool ok = (frequency > 0);

    if (!ok)
    {
        frequency = (freqid.toInt(&ok) + finetune) * 1000;
        mplexid = 0;
    }
    bool isFrequency = ok && (frequency > 10000000);

    // If we are tuning to a freqid, rather than an actual frequency,
    // we need to set the frequency table to use.
    if (!isFrequency)
    {
        if (freqtable == "default" || freqtable.isEmpty())
            SetFreqTable(defaultFreqTable);
        else
            SetFreqTable(freqtable);
    }

    // Set NTSC, PAL, ATSC, etc.
    SetFormat(tvformat);

    // If a tuneToChannel is set make sure we're still on it
    if (!(*it)->tuneToChannel.isEmpty() && (*it)->tuneToChannel != "Undefined")
        TuneTo((*it)->tuneToChannel, 0);

    // Tune to proper frequency
    if ((*it)->externalChanger.isEmpty())
    {
        if ((*it)->name.contains("composite", Qt::CaseInsensitive) ||
            (*it)->name.contains("s-video", Qt::CaseInsensitive))
        {
            VERBOSE(VB_GENERAL, LOC_WARN + "You have not set "
                    "an external channel changing"
                    "\n\t\t\tscript for a composite or s-video "
                    "input. Channel changing will do nothing.");
        }
        else if (isFrequency)
        {
            if (!Tune(frequency, "", (is_dtv) ? "8vsb" : "analog", dtv_si_std))
            {
                return false;
            }
        }
        else
        {
            if (!TuneTo(freqid, finetune))
                return false;
        }
    }
    else if (!ChangeExternalChannel(freqid))
        return false;

    // Set the current channum to the new channel's channum
    QString tmp = channum; tmp.detach();
    m_curchannelname = tmp;

    // Setup filters & recording picture attributes for framegrabing recorders
    // now that the new curchannelname has been established.
    if (m_pParent)
        m_pParent->SetVideoFiltersForChannel(GetCurrentSourceID(), channum);
    InitPictureAttributes();

    // Set the major and minor channel for any additional multiplex tuning
    SetDTVInfo(atsc_major, atsc_minor, netid, tsid, mpeg_prog_num);

    // Set this as the future start channel for this source
    QString tmpX = m_curchannelname; tmpX.detach();
    m_inputs[m_currentInputID]->startChanNum = tmpX;

    return true;
}

bool V4LChannel::TuneTo(const QString &channum, int finetune)
{
    int i = GetCurrentChannelNum(channum);
    VERBOSE(VB_CHANNEL, QString("Channel(%1)::TuneTo(%2): "
                                "curList[%3].freq(%4)")
            .arg(device).arg(channum).arg(i)
            .arg((i != -1) ? curList[i].freq : -1));

    if (i == -1)
    {
        VERBOSE(VB_IMPORTANT, QString("Channel(%1)::TuneTo(%2): Error, "
                                      "failed to find channel.")
                .arg(device).arg(channum));
        return false;
    }

    int frequency = (curList[i].freq + finetune) * 1000;

    return Tune(frequency, "", "analog", "analog");
}

bool V4LChannel::Tune(const DTVMultiplex &tuning, QString inputname)
{
    return Tune(tuning.frequency - 1750000, // to visual carrier
                inputname, tuning.modulation.toString(), tuning.sistandard);
}

/** \fn V4LChannel::Tune(uint,QString,QString,QString)
 *  \brief Tunes to a specific frequency (Hz) on a particular input, using
 *         the specified modulation.
 *
 *  Note: This function always uses modulator zero.
 *
 *  \param frequency Frequency in Hz, this is divided by 62.5 kHz or 62.5 Hz
 *                   depending on the modulator and sent to the hardware.
 *  \param inputname Name of the input (Television, Antenna 1, etc.)
 *  \param modulation "radio", "analog", or "digital"
 */
bool V4LChannel::Tune(uint frequency, QString inputname,
                   QString modulation, QString si_std)
{
    VERBOSE(VB_CHANNEL, LOC + QString("Tune(%1, %2, %3, %4)")
            .arg(frequency).arg(inputname).arg(modulation).arg(si_std));

    int ioctlval = 0;

    if (modulation == "8vsb")
        SetFormat("ATSC");
    modulation = (is_dtv) ? "digital" : modulation;

    int inputnum = GetInputByName(inputname);

    bool ok = true;
    if ((inputnum >= 0) && (GetCurrentInputNum() != inputnum))
        ok = SwitchToInput(inputnum, false);
    else if (GetCurrentInputNum() < 0)
        ok = SwitchToInput(0, false);

    if (!ok)
        return false;

    // If the frequency is a center frequency and not
    // a visual carrier frequency, convert it.
    int offset = frequency % 1000000;
    offset = (offset > 500000) ? 1000000 - offset : offset;
    bool is_visual_carrier = (offset > 150000) && (offset < 350000);
    if (!is_visual_carrier && currentFormat == "ATSC")
    {
        VERBOSE(VB_CHANNEL,  QString("Channel(%1): ").arg(device) +
                QString("Converting frequency from center frequency "
                        "(%1 Hz) to visual carrier frequency (%2 Hz).")
                .arg(frequency).arg(frequency - 1750000));
        frequency -= 1750000; // convert to visual carrier
    }

    // Video4Linux version 2 tuning
    if (usingv4l2)
    {
        bool isTunerCapLow = false;
        struct v4l2_modulator mod;
        bzero(&mod, sizeof(mod));
        mod.index = 0;
        ioctlval = ioctl(videofd, VIDIOC_G_MODULATOR, &mod);
        if (ioctlval >= 0)
        {
            isTunerCapLow = (mod.capability & V4L2_TUNER_CAP_LOW);
            VERBOSE(VB_CHANNEL, "  name: "<<mod.name);
            VERBOSE(VB_CHANNEL, "CapLow: "<<isTunerCapLow);
        }

        struct v4l2_frequency vf;
        bzero(&vf, sizeof(vf));

        vf.tuner = 0; // use first tuner
        vf.frequency = (isTunerCapLow) ?
            ((int)(frequency / 62.5)) : (frequency / 62500);

        if (modulation.toLower() == "digital")
        {
            VERBOSE(VB_CHANNEL, "using digital modulation");
            vf.type = V4L2_TUNER_DIGITAL_TV;
            if (ioctl(videofd, VIDIOC_S_FREQUENCY, &vf)>=0)
                return true;
            VERBOSE(VB_CHANNEL, "digital modulation failed");
        }

        vf.type = V4L2_TUNER_ANALOG_TV;

        ioctlval = ioctl(videofd, VIDIOC_S_FREQUENCY, &vf);
        if (ioctlval < 0)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Channel(%1)::Tune(): Error %2 "
                            "while setting frequency (v2): %3")
                    .arg(device).arg(ioctlval).arg(strerror(errno)));
            return false;
        }
        ioctlval = ioctl(videofd, VIDIOC_G_FREQUENCY, &vf);

        if (ioctlval >= 0)
        {
            VERBOSE(VB_CHANNEL, QString(
                        "Channel(%1)::Tune(): Frequency is now %2")
                    .arg(device).arg(vf.frequency * 62500));
        }

        return true;
    }

    // Video4Linux version 1 tuning
    uint freq = frequency / 62500;
    ioctlval = ioctl(videofd, VIDIOCSFREQ, &freq);
    if (ioctlval < 0)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Channel(%1)::Tune(): Error %2 "
                        "while setting frequency (v1): %3")
                .arg(device).arg(ioctlval).arg(strerror(errno)));
        return false;
    }

    SetSIStandard(si_std);

    return true;
}

/** \fn V4LChannel::Retune(void)
 *  \brief Retunes to last tuned frequency.
 *
 *  NOTE: This only works for V4L2 and only for analog tuning.
 */
bool V4LChannel::Retune(void)
{
    if (usingv4l2)
    {
        struct v4l2_frequency vf;
        bzero(&vf, sizeof(vf));

        vf.tuner = 0; // use first tuner
        vf.type = V4L2_TUNER_ANALOG_TV;

        // Get the last tuned frequency
        int ioctlval = ioctl(videofd, VIDIOC_G_FREQUENCY, &vf);
        if (ioctlval < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Retune failed (1)" + ENO);
            return false;
        }

        // Set the last tuned frequency again...
        ioctlval = ioctl(videofd, VIDIOC_S_FREQUENCY, &vf);
        if (ioctlval < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Retune failed (2)" + ENO);
            return false;
        }

        return true;
    }

    return false;
}

// documented in dtvchannel.h
bool V4LChannel::TuneMultiplex(uint mplexid, QString inputname)
{
    VERBOSE(VB_CHANNEL, LOC + QString("TuneMultiplex(%1)").arg(mplexid));

    QString  modulation;
    QString  si_std;
    uint64_t frequency;
    uint     transportid;
    uint     dvb_networkid;

    if (!ChannelUtil::GetTuningParams(
            mplexid, modulation, frequency,
            transportid, dvb_networkid, si_std))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "TuneMultiplex(): " +
                QString("Could not find tuning parameters for multiplex %1.")
                .arg(mplexid));

        return false;
    }

    if (!Tune(frequency, inputname, modulation, si_std))
        return false;

    return true;
}

QString V4LChannel::GetFormatForChannel(QString channum, QString inputname)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT tvformat "
        "FROM channel, cardinput "
        "WHERE channum            = :CHANNUM   AND "
        "      inputname          = :INPUTNAME AND "
        "      cardinput.cardid   = :CARDID    AND "
        "      cardinput.sourceid = channel.sourceid");
    query.bindValue(":CHANNUM",   channum);
    query.bindValue(":INPUTNAME", inputname);
    query.bindValue(":CARDID",    GetCardID());

    QString fmt = QString::null;
    if (!query.exec() || !query.isActive())
        MythDB::DBError("SwitchToInput:find format", query);
    else if (query.next())
        fmt = query.value(0).toString();
    return fmt;
}

bool V4LChannel::SetInputAndFormat(int inputNum, QString newFmt)
{
    InputMap::const_iterator it = m_inputs.find(inputNum);
    if (it == m_inputs.end() || (*it)->inputNumV4L < 0)
        return false;

    int inputNumV4L = (*it)->inputNumV4L;
    bool usingv4l1 = !usingv4l2;
    bool ok = true;

    QString msg =
        QString("SetInputAndFormat(%1, %2) ").arg(inputNum).arg(newFmt);

    if (usingv4l2)
    {
        VERBOSE(VB_CHANNEL, LOC + msg + "(v4l v2)");

        int ioctlval = ioctl(videofd, VIDIOC_S_INPUT, &inputNumV4L);

        // ConvertX (wis-go7007) requires streaming to be disabled
        // before an input switch, do this if initial switch failed.
        bool streamingDisabled = false;
        int  streamType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if ((ioctlval < 0) && (errno == EBUSY))
        {
            ioctlval = ioctl(videofd, VIDIOC_STREAMOFF, &streamType);
            if (ioctlval < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + msg +
                        "\n\t\t\twhile disabling streaming (v4l v2)" + ENO);

                ok = false;
                ioctlval = 0;
            }
            else
            {
                streamingDisabled = true;

                // Resend the input switch ioctl.
                ioctlval = ioctl(videofd, VIDIOC_S_INPUT, &inputNumV4L);
            }
        }

        if (ioctlval < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + msg +
                    "\n\t\t\twhile setting input (v4l v2)" + ENO);

            ok = false;
        }

        v4l2_std_id vid_mode = format_to_mode(newFmt, 2);
        ioctlval = ioctl(videofd, VIDIOC_S_STD, &vid_mode);
        if (ioctlval < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + msg +
                    "\n\t\t\twhile setting format (v4l v2)" + ENO);

            ok = false;
        }

        // ConvertX (wis-go7007) requires streaming to be disabled
        // before an input switch, here we try to re-enable streaming.
        if (streamingDisabled)
        {
            ioctlval = ioctl(videofd, VIDIOC_STREAMON, &streamType);
            if (ioctlval < 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + msg +
                        "\n\t\t\twhile reenabling streaming (v4l v2)" + ENO);

                ok = false;
            }
        }
    }

    if (usingv4l1)
    {
        VERBOSE(VB_CHANNEL, LOC + msg + "(v4l v1)");

        // read in old settings
        struct video_channel set;
        bzero(&set, sizeof(set));
        ioctl(videofd, VIDIOCGCHAN, &set);

        // set new settings
        set.channel = inputNumV4L;
        set.norm    = format_to_mode(newFmt, 1);
        int ioctlval = ioctl(videofd, VIDIOCSCHAN, &set);

        ok = (ioctlval >= 0);
        if (!ok)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + msg +
                    "\n\t\t\twhile setting format (v4l v1)" + ENO);
        }
        else if (usingv4l2)
        {
            VERBOSE(VB_IMPORTANT, LOC + msg +
                    "\n\t\t\tSetting video mode with v4l version 1 worked");
        }
    }
    return ok;
}

bool V4LChannel::SwitchToInput(int inputnum, bool setstarting)
{
    InputMap::const_iterator it = m_inputs.find(inputnum);
    if (it == m_inputs.end())
        return false;

    QString tuneFreqId = (*it)->tuneToChannel;
    QString channum    = (*it)->startChanNum;
    QString inputname  = (*it)->name;

    VERBOSE(VB_CHANNEL, QString("Channel(%1)::SwitchToInput(in %2, '%3')")
            .arg(device).arg(inputnum)
            .arg(setstarting ? channum : QString("")));

    uint mplexid_restriction;
    if (!IsInputAvailable(inputnum, mplexid_restriction))
        return false;

    QString newFmt = mode_to_format((*it)->videoModeV4L2, 2);

    // If we are setting a channel, get its video mode...
    bool chanValid  = (channum != "Undefined") && !channum.isEmpty();
    if (setstarting && chanValid)
    {
        QString tmp = GetFormatForChannel(channum, inputname);
        if (tmp != "Default" && !tmp.isEmpty())
            newFmt = tmp;
    }

    bool ok = SetInputAndFormat(inputnum, newFmt);

    // Try to set ATSC mode if NTSC fails
    if (!ok && newFmt == "NTSC")
        ok = SetInputAndFormat(inputnum, "ATSC");

    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC + "SetInputAndFormat() failed");
        return false;
    }

    currentFormat     = newFmt;
    is_dtv            = newFmt == "ATSC";
    m_currentInputID = inputnum;
    m_curchannelname    = ""; // this will be set by SetChannelByString

    if (!tuneFreqId.isEmpty() && tuneFreqId != "Undefined")
        ok = TuneTo(tuneFreqId, 0);

    if (!ok)
        return false;

    if (setstarting && chanValid)
        ok = SetChannelByString(channum);
    else if (setstarting && !chanValid)
    {
        VERBOSE(VB_IMPORTANT, LOC +
                QString("SwitchToInput(in %2, set ch): ").arg(inputnum) +
                QString("\n\t\t\tDefault channel '%1' is not valid.")
                .arg(channum));
        ok = false;
    }

    return ok;
}

static unsigned short *get_v4l1_field(
    int v4l2_attrib, struct video_picture &vid_pic)
{
    switch (v4l2_attrib)
    {
        case V4L2_CID_CONTRAST:
            return &vid_pic.contrast;
        case V4L2_CID_BRIGHTNESS:
            return &vid_pic.brightness;
        case V4L2_CID_SATURATION:
            return &vid_pic.colour;
        case V4L2_CID_HUE:
            return &vid_pic.hue;
        default:
            VERBOSE(VB_IMPORTANT, "get_v4l1_field: "
                    "invalid attribute argument "<<v4l2_attrib);
    }
    return NULL;
}

static int get_v4l2_attribute(const QString &db_col_name)
{
    if ("brightness" == db_col_name)
        return V4L2_CID_BRIGHTNESS;
    else if ("contrast" == db_col_name)
        return V4L2_CID_CONTRAST;
    else if ("colour" == db_col_name)
        return V4L2_CID_SATURATION;
    else if ("hue" == db_col_name)
        return V4L2_CID_HUE;
    return -1;
}

bool V4LChannel::InitPictureAttribute(const QString db_col_name)
{
    if (!m_pParent || is_dtv)
        return false;

    int v4l2_attrib = get_v4l2_attribute(db_col_name);
    if (v4l2_attrib == -1)
        return false;

    int cfield = ChannelUtil::GetChannelValueInt(
        db_col_name, GetCurrentSourceID(), m_curchannelname);
    int sfield = CardUtil::GetValueInt(
        db_col_name, GetCardID());

    if ((cfield == -1) || (sfield == -1))
        return false;

    int field = (cfield + sfield) & 0xFFFF;

    QString loc = LOC +
        QString("InitPictureAttribute(%1): ").arg(db_col_name, 10);
    QString loc_err = LOC_ERR +
        QString("InitPictureAttribute(%1): ").arg(db_col_name, 10);

    if (usingv4l2)
    {
        struct v4l2_control ctrl;
        struct v4l2_queryctrl qctrl;
        bzero(&ctrl, sizeof(ctrl));
        bzero(&qctrl, sizeof(qctrl));

        ctrl.id = qctrl.id = v4l2_attrib;
        if (ioctl(videofd, VIDIOC_QUERYCTRL, &qctrl) < 0)
        {
            VERBOSE(VB_IMPORTANT, loc_err + "failed to query controls." + ENO);
            return false;
        }

        float new_range = qctrl.maximum - qctrl.minimum;
        float old_range = 65535 - 0;
        float scl_range = new_range / old_range;
        float dfl       = (qctrl.default_value - qctrl.minimum) / new_range;
        int   norm_dfl  = (0x10000 + (int)(dfl * old_range) - 32768) & 0xFFFF;

        if (pict_attr_default.find(db_col_name) == pict_attr_default.end())
        {
            if (device_name == "pcHDTV HD3000 HDTV")
            {
                pict_attr_default["brightness"] = 9830;
                pict_attr_default["contrast"]   = 39322;
                pict_attr_default["colour"]     = 45875;
                pict_attr_default["hue"]        = 0;
            }
            else
            {
                pict_attr_default[db_col_name] = norm_dfl;
            }
        }

        int dfield = pict_attr_default[db_col_name];
        field      = (cfield + sfield + dfield) & 0xFFFF;
        int value0 = (int) ((scl_range * field) + qctrl.minimum);
        int value1 = min(value0, (int)qctrl.maximum);
        ctrl.value = max(value1, (int)qctrl.minimum);

#if DEBUG_ATTRIB
        VERBOSE(VB_CHANNEL, loc + QString(" %1\n\t\t\t"
                                          "[%2,%3] dflt(%4, %5, %6)")
                .arg(value0).arg(qctrl.minimum, 5).arg(qctrl.maximum, 5)
                .arg(qctrl.default_value, 5).arg(dfl, 4, 'f', 2)
                .arg(norm_dfl));
#endif

        if (ioctl(videofd, VIDIOC_S_CTRL, &ctrl) < 0)
        {
            VERBOSE(VB_IMPORTANT, loc_err + "failed to set controls" + ENO);
            return false;
        }

        return true;
    }

    // V4L1
    unsigned short *setfield;
    struct video_picture vid_pic;
    bzero(&vid_pic, sizeof(vid_pic));

    if (ioctl(videofd, VIDIOCGPICT, &vid_pic) < 0)
    {
        VERBOSE(VB_IMPORTANT, loc_err + "failed to query controls." + ENO);
        return false;
    }
    setfield = get_v4l1_field(v4l2_attrib, vid_pic);

    if (!setfield)
        return false;

    *setfield = field;
    if (ioctl(videofd, VIDIOCSPICT, &vid_pic) < 0)
    {
        VERBOSE(VB_IMPORTANT, loc_err + "failed to set controls." + ENO);
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
        db_col_name, GetCurrentSourceID(), m_curchannelname);
    int sfield = CardUtil::GetValueInt(
        db_col_name, GetCardID());
    int dfield = 0;

    if (pict_attr_default.find(db_col_name) != pict_attr_default.end())
        dfield = pict_attr_default[db_col_name];

    int val = (cfield + sfield + dfield) & 0xFFFF;

#if DEBUG_ATTRIB
    VERBOSE(VB_CHANNEL, QString(
                "GetPictureAttribute(%1) -> cdb %2 rdb %3 d %4 -> %5")
            .arg(db_col_name).arg(cfield).arg(sfield)
            .arg(dfield).arg(val));
#endif

    return val;
}

static int get_v4l2_attribute_value(int videofd, int v4l2_attrib)
{
    struct v4l2_control ctrl;
    struct v4l2_queryctrl qctrl;
    bzero(&ctrl, sizeof(ctrl));
    bzero(&qctrl, sizeof(qctrl));

    ctrl.id = qctrl.id = v4l2_attrib;
    if (ioctl(videofd, VIDIOC_QUERYCTRL, &qctrl) < 0)
    {
        VERBOSE(VB_IMPORTANT, "get_v4l2_attribute_value: "
                "failed to query controls (1)" + ENO);
        return -1;
    }

    if (ioctl(videofd, VIDIOC_G_CTRL, &ctrl) < 0)
    {
        VERBOSE(VB_IMPORTANT, "get_v4l2_attribute_value: "
                "failed to get controls (2)" + ENO);
        return -1;
    }

    float mult = 65535.0 / (qctrl.maximum - qctrl.minimum);
    return min(max((int)(mult * (ctrl.value - qctrl.minimum)), 0), 65525);
}

static int get_v4l1_attribute_value(int videofd, int v4l2_attrib)
{
    struct video_picture vid_pic;
    bzero(&vid_pic, sizeof(vid_pic));

    if (ioctl(videofd, VIDIOCGPICT, &vid_pic) < 0)
    {
        VERBOSE(VB_IMPORTANT, "get_v4l1_attribute_value: "
                "failed to get picture control (1)" + ENO);
        return -1;
    }

    unsigned short *setfield = get_v4l1_field(v4l2_attrib, vid_pic);
    if (setfield)
        return *setfield;

    return -1;
}

static int get_attribute_value(bool usingv4l2, int videofd, int v4l2_attrib)
{
    if (usingv4l2)
        return get_v4l2_attribute_value(videofd, v4l2_attrib);
    return get_v4l1_attribute_value(videofd, v4l2_attrib);
}

static int set_v4l2_attribute_value(int videofd, int v4l2_attrib, int newvalue)
{
    struct v4l2_control ctrl;
    struct v4l2_queryctrl qctrl;
    bzero(&ctrl, sizeof(ctrl));
    bzero(&qctrl, sizeof(qctrl));

    ctrl.id = qctrl.id = v4l2_attrib;
    if (ioctl(videofd, VIDIOC_QUERYCTRL, &qctrl) < 0)
    {
        VERBOSE(VB_IMPORTANT, "set_v4l2_attribute_value: "
                "failed to query control" + ENO);
        return -1;
    }

    float mult = (qctrl.maximum - qctrl.minimum) / 65535.0;
    ctrl.value = (int)(mult * newvalue + qctrl.minimum);
    ctrl.value = min(ctrl.value, qctrl.maximum);
    ctrl.value = max(ctrl.value, qctrl.minimum);

    if (ioctl(videofd, VIDIOC_S_CTRL, &ctrl) < 0)
    {
        VERBOSE(VB_IMPORTANT, "set_v4l2_attribute_value: "
                "failed to set control" + ENO);
        return -1;
    }

    return 0;
}

static int set_v4l1_attribute_value(int videofd, int v4l2_attrib, int newvalue)
{
    unsigned short *setfield;
    struct video_picture vid_pic;
    bzero(&vid_pic, sizeof(vid_pic));

    if (ioctl(videofd, VIDIOCGPICT, &vid_pic) < 0)
    {
        VERBOSE(VB_IMPORTANT, "set_v4l1_attribute_value: "
                "failed to get picture control." + ENO);
        return -1;
    }
    setfield = get_v4l1_field(v4l2_attrib, vid_pic);
    if (newvalue != -1 && setfield)
    {
        *setfield = newvalue;
        if (ioctl(videofd, VIDIOCSPICT, &vid_pic) < 0)
        {
            VERBOSE(VB_IMPORTANT, "set_v4l1_attribute_value: "
                    "failed to set picture control." + ENO);
            return -1;
        }
    }
    else
    {
        // ???
        return -1;
    }

    return 0;
}

static int set_attribute_value(bool usingv4l2, int videofd,
                               int v4l2_attrib, int newvalue)
{
    if (usingv4l2)
        return set_v4l2_attribute_value(videofd, v4l2_attrib, newvalue);
    return set_v4l1_attribute_value(videofd, v4l2_attrib, newvalue);
}

int V4LChannel::ChangePictureAttribute(
    PictureAdjustType type, PictureAttribute attr, bool up)
{
    if (!m_pParent || is_dtv)
        return -1;

    QString db_col_name = toDBString(attr);
    if (db_col_name.isEmpty())
        return -1;

    int v4l2_attrib = get_v4l2_attribute(db_col_name);
    if (v4l2_attrib == -1)
        return -1;

    // get the old attribute value from the hardware, this is
    // just a sanity check on whether this attribute exists
    if (get_attribute_value(usingv4l2, videofd, v4l2_attrib) < 0)
        return -1;

    int old_value = GetPictureAttribute(attr);
    int new_value = old_value + ((up) ? 655 : -655);

    // make sure we are within bounds (wrap around for hue)
    if (V4L2_CID_HUE == v4l2_attrib)
        new_value &= 0xffff;
    new_value = min(max(new_value, 0), 65535);

#if DEBUG_ATTRIB
    VERBOSE(VB_CHANNEL, QString(
                "ChangePictureAttribute(%1,%2,%3) cur %4 -> new %5")
            .arg(type).arg(db_col_name).arg(up)
            .arg(old_value).arg(new_value));
#endif

    // actually set the new attribute value on the hardware
    if (set_attribute_value(usingv4l2, videofd, v4l2_attrib, new_value) < 0)
        return -1;

    // tell the DB about the new attribute value
    if (kAdjustingPicture_Channel == type)
    {
        int adj_value = ChannelUtil::GetChannelValueInt(
            db_col_name, GetCurrentSourceID(), m_curchannelname);

        int tmp = new_value - old_value + adj_value;
        tmp = (tmp < 0)      ? tmp + 0x10000 : tmp;
        tmp = (tmp > 0xffff) ? tmp - 0x10000 : tmp;
        ChannelUtil::SetChannelValue(db_col_name, QString::number(tmp),
                                     GetCurrentSourceID(), m_curchannelname);
    }
    else if (kAdjustingPicture_Recording == type)
    {
        int adj_value = CardUtil::GetValueInt(
            db_col_name, GetCardID());

        int tmp = new_value - old_value + adj_value;
        tmp = (tmp < 0)      ? tmp + 0x10000 : tmp;
        tmp = (tmp > 0xffff) ? tmp - 0x10000 : tmp;
        CardUtil::SetValue(db_col_name, GetCardID(),
                           GetCurrentSourceID(), tmp);
    }

    return new_value;
}
