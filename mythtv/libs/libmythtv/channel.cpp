#include <cstdio>
#include <cstdlib>
#include <cerrno>

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <qsqldatabase.h>

#include "videodev_myth.h"
#include "channel.h"
#include "frequencies.h"
#include "tv.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "channelutil.h"
#include "videosource.h" // for CardUtil

#include <iostream>
using namespace std;

/** \class Channel
 *  \brief Class implementing ChannelBase interface to tuning hardware
 *         when using Video4Linux based drivers.
 */

Channel::Channel(TVRec *parent, const QString &videodevice)
    : ChannelBase(parent), device(videodevice), videofd(-1),
      curList(NULL), totalChannels(0), usingv4l2(false),
      videomode_v4l1(VIDEO_MODE_NTSC), videomode_v4l2(V4L2_STD_NTSC),
      currentFormat(""), defaultFreqTable(1)
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

    struct v4l2_capability vcap;
    memset(&vcap, 0, sizeof(vcap));
    if (ioctl(videofd, VIDIOC_QUERYCAP, &vcap) < 0)
        usingv4l2 = false;
    else
    {
        if (vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
            usingv4l2 = true;
    }

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

/** \fn Channel::SetFormat(const QString &format)
 *  \brief Initializes tuner and modulator variables
 *
 *  This enumerates the inputs, converts the format
 *  string to something the hardware understands, and
 *  if the parent pointer is valid retrieves the
 *  channels from the database.
 *
 *  \param format One of eight formats, 
 *                "NTSC", "NTSC-JP", "ATSC", "SECAM",
 *                "PAL", "PAL-NC", "PAL-M" or "PAL-N".
 */
void Channel::SetFormat(const QString &format)
{
    if (!Open())
        return;

    if (currentFormat == format)
        return;

    currentFormat = format;
  
    QString fmt;
    if (format == "Default")
        fmt = gContext->GetSetting("TVFormat");
    else
        fmt = format;

    videomode_v4l1 = format_to_mode(fmt, 1);
    videomode_v4l2 = format_to_mode(fmt, 2);
 
    if (usingv4l2)
    {
        struct v4l2_input vin;
        bzero(&vin, sizeof(vin));

        vin.index = 0;

        while (ioctl(videofd, VIDIOC_ENUMINPUT, &vin) >= 0)
        {
            VERBOSE(VB_CHANNEL, QString("Channel(%1):SetFormat(): Probed "
                                        "input: %2, name = %3")
                    .arg(device).arg(vin.index).arg((char*)vin.name));
            channelnames[vin.index] = (char *)vin.name;
            inputChannel[vin.index] = "";
            inputTuneTo[vin.index] = "";
            externalChanger[vin.index] = "";
            sourceid[vin.index] = "";
            vin.index++;

            capchannels = vin.index;
        }
    }
    else
    {
        struct video_tuner tuner;
        bzero(&tuner, sizeof(tuner));

        ioctl(videofd, VIDIOCGTUNER, &tuner);
        tuner.mode = videomode_v4l1;
        ioctl(videofd, VIDIOCSTUNER, &tuner);

        struct video_capability vidcap;
        bzero(&vidcap, sizeof(vidcap));
        ioctl(videofd, VIDIOCGCAP, &vidcap);

        capchannels = vidcap.channels;
        for (int i = 0; i < vidcap.channels; i++)
        {
            struct video_channel test;
            bzero(&test, sizeof(test));

            test.channel = i;
            ioctl(videofd, VIDIOCGCHAN, &test);

            VERBOSE(VB_CHANNEL, QString("Channel(%1):SetFormat(): Probed "
                                        "input: %2, name = %3")
                    .arg(device).arg(i).arg((char *)test.name));
            channelnames[i] = test.name;
            inputChannel[i] = "";
            inputTuneTo[i] = "";
            externalChanger[i] = "";
            sourceid[i] = "";
        }

        struct video_channel vc;
        bzero(&vc, sizeof(vc));

        vc.channel = currentcapchannel;
        ioctl(videofd, VIDIOCGCHAN, &vc);
        vc.norm = videomode_v4l1;
        ioctl(videofd, VIDIOCSCHAN, &vc);
    }

    if (pParent)
    {
        pParent->RetrieveInputChannels(inputChannel, inputTuneTo,
                                       externalChanger, sourceid);
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

int Channel::GetChanID() const
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare(
        QString(
            "SELECT chanid FROM channel "
            "WHERE channum = \"%1\" AND channel.sourceid = \"%2\"")
        .arg(curchannelname).arg(sourceid[currentcapchannel]));

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("fetching chanid", query);
        return -1;
    }

    if (query.size() <= 0)
        return -1;

    query.next();
    return query.value(0).toInt();
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
            VERBOSE(VB_IMPORTANT, "Error, couldn't find any available channels."
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
    if (pParent && !pParent->CheckChannel(this, chan, inputName))
    {
        VERBOSE(VB_IMPORTANT, QString(
                    "Channel(%1): CheckChannel failed. Please verify "
                    "channel \"%2\" in the \"mythtv-setup\" Channel Editor.").
                arg(device).arg(chan));
        return false;
    }

    SetCachedATSCInfo("");

    MSqlQuery query(MSqlQuery::InitCon());

    QString thequery = QString(
        "SELECT finetune, freqid, tvformat, freqtable, atscsrcid, commfree, mplexid "
        "FROM channel, videosource "
        "WHERE videosource.sourceid = channel.sourceid AND "
        "      channum = '%1' AND channel.sourceid = '%2'")
        .arg(chan).arg(sourceid[currentcapchannel]);

    query.prepare(thequery);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("fetchtuningparams", query);
    if (query.size() <= 0)
    {
        VERBOSE(VB_IMPORTANT, QString(
                    "Channel(%1): CheckChannel failed because it could not\n"
                    "\t\t\tfind channel number '%2' in DB for source '%3'.")
                .arg(device).arg(chan).arg(sourceid[currentcapchannel]));
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
    QString atsc_chan = (atscsrcid < 256) ?
        chan : QString("%1_%2").arg(atscsrcid >> 8).arg(atscsrcid & 0xff);

    // If CheckChannel filled in the inputName then we need to change inputs
    // and return, since the act of changing inputs will change the channel as well.
    if (!inputName.isEmpty())
    {
        ChannelBase::SwitchToInput(inputName, chan);
        SetCachedATSCInfo(atsc_chan);
        return true;
    }

    QString modulation;
    int frequency = ChannelUtil::GetTuningParams(mplexid, modulation);
    bool ok = (frequency > 0);

    if (!ok)
    {
        frequency = freqid.toInt(&ok) * 1000 + finetune;
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

    if (tvformat.isNull() || tvformat.isEmpty())
        tvformat = "Default";
    SetFormat(tvformat);

    curchannelname = chan;

    if (pParent)
        pParent->SetVideoFiltersForChannel(this, chan);

    SetContrast();
    SetColour();
    SetBrightness();
    SetHue();

    inputChannel[currentcapchannel] = curchannelname;

    // Tune
    if (externalChanger[currentcapchannel].isEmpty())
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

    SetCachedATSCInfo(atsc_chan);

    return true;
}

bool Channel::TuneTo(const QString &channum, int finetune)
{
    int i = GetCurrentChannelNum(channum);
    VERBOSE(VB_CHANNEL, QString("Channel(%1)::TuneTo(%2): curList[%3].freq(%4)")
            .arg(device).arg(channum).arg(i)
            .arg((i != -1) ? curList[i].freq : -1));

    if (i == -1)
    {
        VERBOSE(VB_IMPORTANT, QString("Channel(%1)::TuneTo(%2): Error, "
                                      "failed to find channel.")
                .arg(device).arg(channum));
        return false;
    }

    int frequency = curList[i].freq * 1000 + finetune;

    return Tune(frequency);
}

/** \fn Channel::Tune(uint frequency, QString inputname, QString modulation)
 *  \brief Tunes to a specifix frequency (Hz) on a particular input, using
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

    if (currentFormat == "")
        SetFormat("Default");
    int inputnum = GetInputByName(inputname);

    if ((inputnum >= 0) && (GetCurrentInputNum() != inputnum))
        SwitchToInput(inputnum, false);
    else if (GetCurrentInputNum() < 0)
        SwitchToInput(0, false);

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

        vf.frequency = (isTunerCapLow) ?
            ((int)(frequency / 62.5)) : (frequency / 62500);

        if (modulation.lower() != "analog")
        {
/*
            vf.type = V4L2_TUNER_DIGITAL_TV;
            if (ioctl(videofd, VIDIOC_S_FREQUENCY, &vf)>=0)
                return true;
*/
        }

        vf.tuner = 0; // use first tuner
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

int Channel::GetCardID() const
{
    if (pParent)
        return pParent->GetCaptureCardNum();
    else
        return CardUtil::GetCardID(GetDevice());
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
    (void) modulation; // we don't actually use this yet.

    if (!Tune(frequency, inputname))
        return false;

    return true;
}

void Channel::SwitchToInput(int newcapchannel, bool setstarting)
{
    bool usingv4l1 = !usingv4l2;

    VERBOSE(VB_CHANNEL, QString("Channel(%1)::SwitchToInput(in %2%3)")
            .arg(device).arg(newcapchannel)
            .arg(setstarting ? ", set ch" : ""));

    int ioctlval = 0;
    if (usingv4l2)
    {
        ioctlval = ioctl(videofd, VIDIOC_S_INPUT, &newcapchannel);
        if (ioctlval < 0)
            VERBOSE(VB_IMPORTANT,
                    QString("Channel(%1)::SwitchToInput(in %2%3): Error %4 "
                            "while setting input (v2): %5").arg(device)
                    .arg(newcapchannel).arg(setstarting?", set ch":"")
                    .arg(ioctlval).arg(strerror(errno)));

        VERBOSE(VB_CHANNEL, 
                QString("Channel(%1)::SwitchToInput() setting video mode to %2")
                .arg(device).arg(mode_to_format(videomode_v4l2, 2)));
        ioctlval = ioctl(videofd, VIDIOC_S_STD, &videomode_v4l2);
        if (ioctlval < 0)
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Channel(%1)::SwitchToInput(in %2%3): Error %4 "
                            "while setting video mode (v2), \"%5\", trying v4l v1")
                    .arg(device).arg(newcapchannel)
                    .arg(setstarting ? ", set ch" : "")
                    .arg(ioctlval).arg(strerror(errno)));

            // Fall through to try v4l version 1, pcHDTV
            // drivers don't work with VIDIOC_S_STD ioctl.
            usingv4l1 = true;
        }
    }

    if (usingv4l1)
    {
        struct video_channel set;
        memset(&set, 0, sizeof(set));
        ioctl(videofd, VIDIOCGCHAN, &set); // read in old settings
        set.channel = newcapchannel;
        set.norm = videomode_v4l1;
        VERBOSE(VB_CHANNEL, 
                QString("Channel(%1)::SwitchToInput() setting video mode to %2")
                .arg(device).arg(mode_to_format(videomode_v4l1, 1)));
        ioctlval = ioctl(videofd, VIDIOCSCHAN, &set); // set new settings
        if (ioctlval < 0)
            VERBOSE(VB_IMPORTANT,
                    QString("Channel(%1)::SwitchToInput(in %2%3): "
                            "Error %4 while setting video mode (v1): %5")
                    .arg(device).arg(newcapchannel)
                    .arg(setstarting ? ", set ch" : "")
                    .arg(ioctlval).arg(strerror(errno)));
        else if (usingv4l2)
            VERBOSE(VB_IMPORTANT,
                    QString("Channel(%1)::SwitchToInput(in %2%3): "
                            "Setting video mode with v4l version 1 worked")
                    .arg(device).arg(newcapchannel)
                    .arg(setstarting ? ", set ch" : ""));
    }

    currentcapchannel = newcapchannel;
    curchannelname = "";

    if (inputTuneTo[currentcapchannel] != "Undefined")
        TuneTo(inputTuneTo[currentcapchannel], 0);

    if (setstarting && !inputChannel[currentcapchannel].isEmpty())
        SetChannelByString(inputChannel[currentcapchannel]);
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
    if (!pParent)
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
    if (!pParent)
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

    if (current_value < 0) // Couldn't get from database
    {
        if (up)
        {
            newvalue = card_value + 655;
            newvalue = (newvalue > 65535) ? (65535) : (newvalue);
        }
        else
        {
            newvalue = card_value - 655;
            newvalue = (newvalue < 0) ? (0) : (newvalue);
        }
    }
    else
    {
        if (up)
        {
            newvalue = current_value + 655;
            newvalue = (newvalue > 65535) ? (65535) : (newvalue);
        }
        else
        {
            newvalue = current_value - 655;
            newvalue = (newvalue < 0) ? (0) : (newvalue);
        }

        pParent->SetChannelValue(channel_field, newvalue, this, curchannelname);
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
        ctrl.value = (int)((qctrl.maximum - qctrl.minimum) / 65535.0 * 
                           newvalue + qctrl.minimum);
        ctrl.value = ctrl.value > qctrl.maximum
                        ? qctrl.maximum
                            : ctrl.value < qctrl.minimum
                                ? qctrl.minimum
                                    : ctrl.value;
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

