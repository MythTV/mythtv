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

// Qt headers
#include <qsqldatabase.h>

// MythTV headers
#include "videodev_myth.h"
#include "channel.h"
#include "frequencies.h"
#include "tv.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "channelutil.h"
#include "cardutil.h"

#define LOC QString("Channel(%1): ").arg(device)
#define LOC_ERR QString("Channel(%1) Error: ").arg(device)

static int format_to_mode(const QString& fmt, int v4l_version);
static QString mode_to_format(int mode, int v4l_version);

/** \class Channel
 *  \brief Class implementing ChannelBase interface to tuning hardware
 *         when using Video4Linux based drivers.
 */

Channel::Channel(TVRec *parent, const QString &videodevice)
    : ChannelBase(parent), device(videodevice), videofd(-1),
      curList(NULL), totalChannels(0),
      currentFormat(""), is_dtv(false), usingv4l2(false),
      defaultFreqTable(1)
{
}

Channel::~Channel(void)
{
    Close();
}

bool Channel::Open(void)
{
#if FAKE_VIDEO
    return true;
#endif
    if (videofd >= 0)
        return true;

    videofd = open(device.ascii(), O_RDWR);
    if (videofd < 0)
    {
         VERBOSE(VB_IMPORTANT,
                 QString("Channel(%1)::Open(): Can't open video device, "
                         "error \"%2\"").arg(device).arg(strerror(errno)));
         return false;
    }

    usingv4l2 = CardUtil::hasV4L2(videofd);

    if (!InitializeInputs())
    {
        Close();
        return false;
    }

    SetFormat("Default");

    return true;
}

void Channel::Close(void)
{
    if (videofd >= 0)
        close(videofd);
    videofd = -1;
}

void Channel::SetFd(int fd)
{
    if (fd != videofd)
        Close();
    videofd = (fd >= 0) ? fd : -1;
}

static int format_to_mode(const QString& fmt, int v4l_version)
{
    if (v4l_version==2)
    {
        if (fmt == "NTSC")
            return V4L2_STD_NTSC;
        else if (fmt == "ATSC")
            return V4L2_STD_ATSC_8_VSB;
        else if (fmt == "PAL")
            return V4L2_STD_PAL;
        else if (fmt == "PAL-BG")
            return V4L2_STD_PAL_BG;
        else if (fmt == "PAL-DK")
            return V4L2_STD_PAL_DK;
        else if (fmt == "PAL-I")
            return V4L2_STD_PAL_I;
        else if (fmt == "PAL-60")
            return V4L2_STD_PAL_60;
        else if (fmt == "SECAM")
            return V4L2_STD_SECAM;
        else if (fmt == "PAL-NC")
            return V4L2_STD_PAL_Nc;
        else if (fmt == "PAL-M")
            return V4L2_STD_PAL_M;
        else if (fmt == "PAL-N")
            return V4L2_STD_PAL_N;
        else if (fmt == "NTSC-JP")
            return V4L2_STD_NTSC_M_JP;
        return V4L2_STD_NTSC;
    }
    if (v4l_version==1)
    {
        if (fmt == "NTSC")
            return VIDEO_MODE_NTSC;
        else if (fmt == "ATSC")
            return VIDEO_MODE_ATSC;
        else if (fmt == "PAL")
            return VIDEO_MODE_PAL;
        else if (fmt == "PAL-BG")
            return VIDEO_MODE_PAL;
        else if (fmt == "PAL-DK")
            return VIDEO_MODE_PAL;
        else if (fmt == "PAL-I")
            return VIDEO_MODE_PAL;
        else if (fmt == "PAL-60")
            return VIDEO_MODE_PAL;
        else if (fmt == "SECAM")
            return VIDEO_MODE_SECAM;
        else if (fmt == "PAL-NC")
            return 3;
        else if (fmt == "PAL-M")
            return 4;
        else if (fmt == "PAL-N")
            return 5;
        else if (fmt == "NTSC-JP")
            return 6;
    }
    return VIDEO_MODE_NTSC;
}

static QString mode_to_format(int mode, int v4l_version)
{
    if (v4l_version==2)
    {
        if (mode == V4L2_STD_NTSC)
            return "NTSC";
        else if (mode == V4L2_STD_ATSC_8_VSB)
            return "ATSC";
        else if (mode == V4L2_STD_PAL)
            return "PAL";
        else if (mode == V4L2_STD_PAL_BG)
            return "PAL-BG";
        else if (mode == V4L2_STD_PAL_DK)
            return "PAL-DK";
        else if (mode == V4L2_STD_PAL_I)
            return "PAL-I";
        else if (mode == V4L2_STD_PAL_60)
            return "PAL-60";
        else if (mode == V4L2_STD_SECAM)
            return "SECAM";
        else if (mode == V4L2_STD_PAL_Nc)
            return "PAL-NC";
        else if (mode == V4L2_STD_PAL_M)
            return "PAL-M";
        else if (mode == V4L2_STD_PAL_N)
            return "PAL-N";
        else if (mode == V4L2_STD_NTSC_M_JP)
            return "NTSC-JP";
        return "Unknown";
    }
    if (v4l_version==1)
    {
        if (mode == VIDEO_MODE_NTSC)
            return "NTSC";
        else if (mode == VIDEO_MODE_ATSC)
            return "ATSC";
        else if (mode == VIDEO_MODE_PAL)
            return "PAL";
        else if (mode == VIDEO_MODE_SECAM)
            return "SECAM";
        else if (mode == 3)
            return "PAL-NC";
        else if (mode == 4)
            return "PAL-M";
        else if (mode == 5)
            return "PAL-N";
        else if (mode == 6)
            return "NTSC-JP";
    }
    return "Unknown";
}

/** \fn Channel::InitializeInputs(void)
 *  This enumerates the inputs, converts the format
 *  string to something the hardware understands, and
 *  if the parent pointer is valid retrieves the
 *  channels from the database.
 */
bool Channel::InitializeInputs(void)
{
    // Get Inputs from DB
    if (!ChannelBase::InitializeInputs())
        return false;

    // Get global TVFormat setting
    QString fmt = gContext->GetSetting("TVFormat");
    VERBOSE(VB_CHANNEL, QString("Global TVFormat Setting '%1'").arg(fmt));
    int videomode_v4l1 = format_to_mode(fmt.upper(), 1);
    int videomode_v4l2 = format_to_mode(fmt.upper(), 2);

    bool ok = false;
    InputNames v4l_inputs = CardUtil::probeV4LInputs(videofd, ok);

    // Insert info from hardware
    uint valid_cnt = 0;
    InputMap::const_iterator it;
    for (it = inputs.begin(); it != inputs.end(); ++it)
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
    for (it = inputs.begin(); it != inputs.end(); ++it)
    {
        VERBOSE(VB_CHANNEL, LOC + QString("Input #%1: '%2' schan(%3) "
                                          "tun(%4) v4l1(%5) v4l2(%6)")
                .arg(it.key()).arg((*it)->name).arg((*it)->startChanNum)
                .arg((*it)->tuneToChannel)
                .arg(mode_to_format((*it)->videoModeV4L1,1))
                .arg(mode_to_format((*it)->videoModeV4L1,2)));
    }

    return valid_cnt;
}

/** \fn Channel::SetFormat(const QString &format)
 *  \brief Initializes tuner and modulator variables
 *
 *  \param format One of twelve formats:
 *                "NTSC",   "NTSC-JP", "ATSC",
 *                "SECAM",
 *                "PAL",    "PAL-BG",  "PAL-DK",   "PAL-I",
 *                "PAL-60", "PAL-NC",  "PAL-M", or "PAL-N"
 */
void Channel::SetFormat(const QString &format)
{
    if (!Open())
        return;

    int inputNum = currentInputID;
    if (currentInputID < 0)
        inputNum = GetNextInputNum();

    QString fmt = format;
    if ((fmt == "Default") || format.isEmpty())
    {
        InputMap::const_iterator it = inputs.find(inputNum);
        if (it != inputs.end())
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

int Channel::SetDefaultFreqTable(const QString &name)
{
    defaultFreqTable = SetFreqTable(name);
    return defaultFreqTable;
}

void Channel::SetFreqTable(const int index)
{
    curList = chanlists[index].list;
    totalChannels = chanlists[index].count;
}

int Channel::SetFreqTable(const QString &name)
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

int Channel::GetCurrentChannelNum(const QString &channame)
{
    // remove part after '-' for (HDTV subchannels)
    QString real_channame = channame;
    int pos = channame.find('-');
    if (pos != -1)
        real_channame.truncate(pos);

    for (int i = 0; i < totalChannels; i++)
    {
        if (real_channame == curList[i].name)
            return i;
    }
    VERBOSE(VB_IMPORTANT,
            QString("Channel::GetCurrentChannelNum(%1): "
                    "Failed to find Channel '%2'")
            .arg(channame).arg(real_channame));

    return -1;
}

void Channel::SaveCachedPids(const pid_cache_t &pid_cache) const
{
    int chanid = GetChanID();
    if (chanid >= 0)
        ChannelBase::SaveCachedPids(chanid, pid_cache);
}

void Channel::GetCachedPids(pid_cache_t &pid_cache) const
{
    int chanid = GetChanID();
    if (chanid >= 0)
        ChannelBase::GetCachedPids(chanid, pid_cache);
}

bool Channel::SetChannelByDirection(ChannelChangeDirection dir)
{
    if (ChannelBase::SetChannelByDirection(dir))
        return true;

    if ((CHANNEL_DIRECTION_UP != dir) && (CHANNEL_DIRECTION_DOWN != dir))
        return false;

    QString nextchan;
    bool finished = false;
    int chancount = 0;
    int curchannel = GetCurrentChannelNum(curchannelname);
    int incrDir = (CHANNEL_DIRECTION_UP == dir) ? 1 : -1;

    while (!finished)
    {
        curchannel += incrDir;
        curchannel = (curchannel < 0) ? totalChannels - 1 : curchannel;
        curchannel = (curchannel > totalChannels) ? 0 : curchannel;
        chancount++;

        nextchan = curList[curchannel].name;
        finished = SetChannelByString(nextchan);

        if (chancount > totalChannels)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Couldn't find any available channels."
                    "\n\t\t\tYour database is most likely setup incorrectly.");
            break;
        }
    }
    return finished;
}

bool Channel::SetChannelByString(const QString &chan)
{
    VERBOSE(VB_CHANNEL, QString("Channel(%1)::SetChannelByString(%2)")
            .arg(device).arg(chan));
    
    if (!Open())
    {
        VERBOSE(VB_IMPORTANT, QString(
                    "Channel(%1)::SetChannelByString(): Channel object "
                    "wasn't open, can't change channels").arg(device));
        return false;
    }

    QString inputName;
    if (!CheckChannel(chan, inputName))
    {
        VERBOSE(VB_IMPORTANT, LOC + "CheckChannel failed. " +
                QString("Please verify channel '%1'").arg(chan) +
                " in the \"mythtv-setup\" Channel Editor.");
        return false;
    }

    // If CheckChannel filled in the inputName then we need to
    // change inputs and return, since the act of changing
    // inputs will change the channel as well.
    if (!inputName.isEmpty())
        return ChannelBase::SwitchToInput(inputName, chan);

    SetCachedATSCInfo("");

    InputMap::const_iterator it = inputs.find(currentInputID);
    if (it == inputs.end())
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    QString thequery = QString(
        "SELECT finetune, freqid, tvformat, freqtable, "
        "       atscsrcid, commfree, mplexid "
        "FROM channel, videosource "
        "WHERE videosource.sourceid = channel.sourceid AND "
        "      channum = '%1' AND channel.sourceid = '%2'")
        .arg(chan).arg((*it)->sourceid);

    query.prepare(thequery);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("fetchtuningparams", query);
    if (query.size() <= 0)
    {
        VERBOSE(VB_IMPORTANT, QString(
                    "Channel(%1): CheckChannel failed because it could not\n"
                    "\t\t\tfind channel number '%2' in DB for source '%3'.")
                .arg(device).arg(chan).arg((*it)->sourceid));
        return false;
    }
    query.next();

    int finetune      = query.value(0).toInt();
    QString freqid    = query.value(1).toString();
    QString tvformat  = query.value(2).toString();
    QString freqtable = query.value(3).toString();
    uint atscsrcid    = query.value(4).toInt();
    commfree          = query.value(5).toBool();
    uint mplexid      = query.value(6).toInt();

    QString modulation;
    int frequency = ChannelUtil::GetTuningParams(mplexid, modulation);
    bool ok = (frequency > 0);

    if (!ok)
    {
        frequency = (freqid.toInt(&ok) + finetune) * 1000;
        mplexid = 0;
    }
    bool isFrequency = ok && (frequency > 10000000);

    if (!isFrequency)
    {
        if (freqtable == "default" || freqtable.isNull() || freqtable.isEmpty())
            SetFreqTable(defaultFreqTable);
        else
            SetFreqTable(freqtable);
    }

    SetFormat(tvformat);

    curchannelname = chan;

    if (pParent)
        pParent->SetVideoFiltersForChannel(this, chan);

    SetContrast();
    SetColour();
    SetBrightness();
    SetHue();

    inputs[currentInputID]->startChanNum = curchannelname;

    // Tune
    if ((*it)->externalChanger.isEmpty())
    {
        if (isFrequency)
        {
            if (!Tune(frequency))
                return false;
        }
        else
        {
            if (!TuneTo(freqid, finetune))
                return false;
        }
    }
    else if (!ChangeExternalChannel(freqid))
        return false;

    QString min_maj = QString("%1_0").arg(chan);
    if (atscsrcid > 255)
        min_maj = QString("%1_%2")
            .arg(atscsrcid >> 8).arg(atscsrcid & 0xff);

    SetCachedATSCInfo(min_maj);

    return true;
}

bool Channel::TuneTo(const QString &channum, int finetune)
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

    return Tune(frequency);
}

/** \fn Channel::Tune(uint frequency, QString inputname, QString modulation)
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
bool Channel::Tune(uint frequency, QString inputname, QString modulation)
{
    VERBOSE(VB_CHANNEL, QString("Channel(%1)::Tune(%2, %3, %4)")
            .arg(device).arg(frequency).arg(inputname).arg(modulation));
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

        if (modulation.lower() == "digital")
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

    return true;
}

bool Channel::IsTuned() const
{
    if (usingv4l2)
    {
        struct v4l2_tuner tuner;

        memset(&tuner,0,sizeof(tuner));
        if (-1 == ::ioctl(videofd,VIDIOC_G_TUNER,&tuner,0))
            return false;
        return tuner.signal ? true : false;
    }
    else 
    {
        struct video_tuner tuner;
        memset(&tuner,0,sizeof(tuner));
        if (-1 == ::ioctl(videofd,VIDIOCGTUNER,&tuner,0))
             return false;
        return tuner.signal ? true : false;
    }
}

/** \fn Channel::TuneMultiplex(uint mplexid)
 *  \brief To be used by the siscan
 *
 *   mplexid is how the db indexes each transport
 */
bool Channel::TuneMultiplex(uint mplexid)
{
    VERBOSE(VB_CHANNEL, QString("Channel(%1)::TuneMultiplex(%2)")
            .arg(device).arg(mplexid));

    MSqlQuery query(MSqlQuery::InitCon());

    int cardid = GetCardID();
    if (cardid < 0)
        return false;

    // Query for our tuning params
    QString thequery(
        "SELECT frequency, inputname, modulation "
        "FROM dtv_multiplex, cardinput, capturecard "
        "WHERE dtv_multiplex.sourceid = cardinput.sourceid AND "
        "      cardinput.cardid = capturecard.cardid AND ");

    thequery += QString("mplexid = '%1' AND cardinput.cardid = '%2'")
        .arg(mplexid).arg(cardid);

    query.prepare(thequery);
    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError(
            QString("Channel(%1)::TuneMultiplex(): Error, could not find "
                    "tuning parameters for transport %1.")
            .arg(device).arg(mplexid), query);
        return false;
    }
    if (query.size() <= 0)
    {
        VERBOSE(VB_IMPORTANT, QString(
                    "Channel(%1)::TuneMultiplex(): Error, could not find "
                    "tuning parameters for transport %1.")
                .arg(device).arg(mplexid));
        return false;
    }
    query.next();

    uint    frequency  = query.value(0).toInt();
    QString inputname  = query.value(1).toString();
    QString modulation = query.value(2).toString();

    if (!Tune(frequency, inputname, modulation))
        return false;

    return true;
}

QString Channel::GetFormatForChannel(QString channum, QString inputname)
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
        MythContext::DBError("SwitchToInput:find format", query);
    else if (query.next())
        fmt = query.value(0).toString();
    return fmt;
}

bool Channel::SetInputAndFormat(int inputNum, QString newFmt)
{
    InputMap::const_iterator it = inputs.find(inputNum);
    if (it == inputs.end() || (*it)->inputNumV4L < 0)
        return false;

    int inputNumV4L = (*it)->inputNumV4L;
    bool usingv4l1 = !usingv4l2;
    bool ok = true;

    if (usingv4l2)
    {
        int ioctlval = ioctl(videofd, VIDIOC_S_INPUT, &inputNumV4L);
        if (ioctlval < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + QString(
                        "SetInputAndFormat(%1, %2) "
                        "\n\t\t\twhile setting input (v4l v2)")
                    .arg(inputNum).arg(newFmt) + ENO);
            ok = false;
        }

        VERBOSE(VB_CHANNEL, LOC + QString(
                    "SetInputAndFormat(%1, %2) v4l v2")
                .arg(inputNum).arg(newFmt));

        v4l2_std_id vid_mode = format_to_mode(newFmt, 2);
        ioctlval = ioctl(videofd, VIDIOC_S_STD, &vid_mode);
        if (ioctlval < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + QString(
                        "SetInputAndFormat(%1, %2) "
                        "\n\t\t\twhile setting format (v4l v2)")
                    .arg(inputNum).arg(newFmt) + ENO);

            // Fall through to try v4l version 1, pcHDTV 1.4 (for HD-2000)
            // drivers don't work with VIDIOC_S_STD ioctl.
            usingv4l1 = true;
            ok = false;
        }
    }

    if (usingv4l1)
    {
        VERBOSE(VB_CHANNEL, LOC + QString(
                    "SetInputAndFormat(%1, %2) v4l v1")
                .arg(inputNum).arg(newFmt));

        // read in old settings
        struct video_channel set;
        bzero(&set, sizeof(set));
        ioctl(videofd, VIDIOCGCHAN, &set);

        // set new settings
        set.channel = inputNumV4L;
        set.norm    = format_to_mode(newFmt, 1);
        int ioctlval = ioctl(videofd, VIDIOCSCHAN, &set);

        if (ioctlval < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + QString(
                        "SetInputAndFormat(%1, %2) "
                        "\n\t\t\twhile setting format (v4l v1)")
                    .arg(inputNum).arg(newFmt) + ENO);
            ok = false;
        }
        else if (usingv4l2)
        {
            VERBOSE(VB_IMPORTANT, LOC + QString(
                        "SetInputAndFormat(%1, %2) "
                        "\n\t\t\tSetting video mode with v4l version 1 worked")
                    .arg(inputNum).arg(newFmt));
            ok = true;
        }
    }
    return ok;
}

bool Channel::SwitchToInput(int inputnum, bool setstarting)
{
    InputMap::const_iterator it = inputs.find(inputnum);
    if (it == inputs.end())
        return false;

    QString tuneFreqId = (*it)->tuneToChannel;
    QString channum    = (*it)->startChanNum;
    QString inputname  = (*it)->name;

    VERBOSE(VB_CHANNEL, QString("Channel(%1)::SwitchToInput(in %2, '%3')")
            .arg(device).arg(inputnum)
            .arg(setstarting ? channum : QString("")));

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
    currentInputID = inputnum;
    curchannelname    = ""; // this will be set by SetChannelByString

    if (setstarting && !tuneFreqId.isEmpty() && tuneFreqId != "Undefined")
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

unsigned short *Channel::GetV4L1Field(int attrib, struct video_picture &vid_pic)
{
    switch (attrib)
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
            VERBOSE(VB_IMPORTANT,
                    QString("Channel(%1)::SetColourAttribute(): "
                            "invalid attribute argument: %2\n").
                    arg(device).arg(attrib));
    }
    return NULL;
}

void Channel::SetColourAttribute(int attrib, const char *name)
{
    if (!pParent || is_dtv)
        return;

    QString field_name = name;
    int field = pParent->GetChannelValue(field_name, this, curchannelname);

    if (usingv4l2)
    {
        struct v4l2_control ctrl;
        struct v4l2_queryctrl qctrl;
        memset(&ctrl, 0, sizeof(ctrl));
        memset(&qctrl, 0, sizeof(qctrl));

        if (field != -1)
        {
            ctrl.id = qctrl.id = attrib;
            if (ioctl(videofd, VIDIOC_QUERYCTRL, &qctrl) < 0)
            {
                VERBOSE(VB_IMPORTANT,
                        QString("Channel(%1)::SetColourAttribute(): "
                                "failed to query controls, error: %2").
                        arg(device).arg(strerror(errno)));
                return;
            }
            ctrl.value = (int)((qctrl.maximum - qctrl.minimum) 
                               / 65535.0 * field + qctrl.minimum);
            ctrl.value = ctrl.value > qctrl.maximum
                              ? qctrl.maximum
                                  : ctrl.value < qctrl.minimum
                                       ? qctrl.minimum
                                            : ctrl.value;
            if (ioctl(videofd, VIDIOC_S_CTRL, &ctrl) < 0)
            {
                VERBOSE(VB_IMPORTANT,
                        QString("Channel(%1)::SetColourAttribute(): "
                                "failed to set controls, error: %2").
                        arg(device).arg(strerror(errno)));
                return;
            }
        }
    }
    else
    {
        unsigned short *setfield;
        struct video_picture vid_pic;
        memset(&vid_pic, 0, sizeof(vid_pic));

        if (ioctl(videofd, VIDIOCGPICT, &vid_pic) < 0)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Channel(%1)::SetColourAttribute(): failed "
                            "to get picture controls, error: %2").
                    arg(device).arg(strerror(errno)));
            return;
        }
        setfield = GetV4L1Field(attrib, vid_pic);
        if (field != -1 && setfield)
        {
            *setfield = field;
            if (ioctl(videofd, VIDIOCSPICT, &vid_pic) < 0)
            {
                VERBOSE(VB_IMPORTANT,
                        QString("Channel(%1)::SetColourAttribute(): failed "
                                "to set picture controls, error: %2").
                        arg(device).arg(strerror(errno)));
                return;
            }
        }
    }
    return;
}

void Channel::SetContrast(void)
{
    SetColourAttribute(V4L2_CID_CONTRAST, "contrast");
    return;
}

void Channel::SetBrightness()
{
    SetColourAttribute(V4L2_CID_BRIGHTNESS, "brightness");
    return;
}

void Channel::SetColour()
{
    SetColourAttribute(V4L2_CID_SATURATION, "colour");
    return;
}

void Channel::SetHue(void)
{
    SetColourAttribute(V4L2_CID_HUE, "hue");
    return;
}

int Channel::ChangeColourAttribute(int attrib, const char *name, bool up)
{
    if (!pParent || is_dtv)
        return -1;

    int newvalue;    // The int should have ample space to avoid overflow
                     // in the case that we're just over or under 65535

    QString channel_field = name;
    int current_value = pParent->GetChannelValue(channel_field, this, 
                                                 curchannelname);

    int card_value;

    if (usingv4l2)
    {
        struct v4l2_control ctrl;
        struct v4l2_queryctrl qctrl;
        memset(&ctrl, 0, sizeof(ctrl));
        memset(&qctrl, 0, sizeof(qctrl));

        ctrl.id = qctrl.id = attrib;
        if (ioctl(videofd, VIDIOC_QUERYCTRL, &qctrl) < 0)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Channel(%1)::ChangeColourAttribute(): "
                            "failed to query controls (1), error: %2").
                    arg(device).arg(strerror(errno)));
            return -1;
        }

        if (ioctl(videofd, VIDIOC_G_CTRL, &ctrl) < 0)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Channel(%1)::ChangeColourAttribute(): "
                            "failed to get control, error: %2").
                    arg(device).arg(strerror(errno)));
            return -1;
        }
        card_value = (int)(65535.0 / (qctrl.maximum - qctrl.minimum) * 
                           ctrl.value);
    }
    else
    {
        unsigned short *setfield;
        struct video_picture vid_pic;
        memset(&vid_pic, 0, sizeof(vid_pic));

        if (ioctl(videofd, VIDIOCGPICT, &vid_pic) < 0)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Channel(%1)::ChangeColourAttribute(): "
                            "failed to get picture control (1), error: %2").
                    arg(device).arg(strerror(errno)));
            return -1;
        }

        setfield = GetV4L1Field(attrib, vid_pic);
        if (!setfield)
        {
            return -1;
        }

        card_value = *setfield;
    }

    newvalue  = (current_value < 0) ? card_value : current_value;
    newvalue += (up) ? 655 : -655;

    if (V4L2_CID_HUE == attrib)
    {
        // wrap around for hue
        newvalue = (newvalue > 65535) ? newvalue - 65535 : newvalue;
        newvalue = (newvalue < 0)     ? newvalue + 65535 : newvalue;
    }

    // make sure we are within bounds
    newvalue  = min(newvalue, 65535);
    newvalue  = max(newvalue, 0);

    if (current_value >= 0)
    {
        // tell the DB about the new attributes
        pParent->SetChannelValue(channel_field, newvalue,
                                 this, curchannelname);
    }

    if (usingv4l2)
    {
        struct v4l2_control ctrl;
        struct v4l2_queryctrl qctrl;
        memset(&ctrl, 0, sizeof(ctrl));
        memset(&qctrl, 0, sizeof(qctrl));

        ctrl.id = qctrl.id = attrib;
        if (ioctl(videofd, VIDIOC_QUERYCTRL, &qctrl) < 0)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Channel(%1)::ChangeColourAttribute(): "
                            "failed to query controls (2), error: %2").
                    arg(device).arg(strerror(errno)));
            return -1;
        }
        float mult = (qctrl.maximum - qctrl.minimum) / 65535.0;
        ctrl.value = (int)(mult * newvalue + qctrl.minimum);
        ctrl.value = min(ctrl.value, qctrl.maximum);
        ctrl.value = max(ctrl.value, qctrl.minimum);

        if (ioctl(videofd, VIDIOC_S_CTRL, &ctrl) < 0)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Channel(%1)::ChangeColourAttribute(): "
                            "failed to set control, error: %2").
                    arg(device).arg(strerror(errno)));
            return -1;
        }
    }
    else
    {
        unsigned short *setfield;
        struct video_picture vid_pic;
        memset(&vid_pic, 0, sizeof(vid_pic));

        if (ioctl(videofd, VIDIOCGPICT, &vid_pic) < 0)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Channel(%1)::ChangeColourAttribute(): "
                            "failed to get picture control (2), error: %2").
                    arg(device).arg(strerror(errno)));
            return -1;
        }
        setfield = GetV4L1Field(attrib, vid_pic);
        if (newvalue != -1 && setfield)
        {
            *setfield = newvalue;
            if (ioctl(videofd, VIDIOCSPICT, &vid_pic) < 0)
            {
                VERBOSE(VB_IMPORTANT,
                        QString("Channel(%1)::ChangeColourAttribute(): "
                                "failed to set picture control, error: %2").
                        arg(device).arg(strerror(errno)));
                return -1;
            }
        }
        else
        {
            // ???
            return -1;
        }
    }

    return newvalue / 655;
}

int Channel::ChangeContrast(bool up)
{
    return ChangeColourAttribute(V4L2_CID_CONTRAST, "contrast", up);
}

int Channel::ChangeBrightness(bool up)
{
    return ChangeColourAttribute(V4L2_CID_BRIGHTNESS, "brightness", up);
}

int Channel::ChangeColour(bool up)
{
    return ChangeColourAttribute(V4L2_CID_SATURATION, "colour", up);
}

int Channel::ChangeHue(bool up)
{
    return ChangeColourAttribute(V4L2_CID_HUE, "hue", up);
}

