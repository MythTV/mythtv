#include <stdio.h>
#include <stdlib.h>
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

#include <iostream>
using namespace std;

Channel::Channel(TVRec *parent, const QString &videodevice)
       : ChannelBase(parent)
{
    device = videodevice;
    isopen = false;
    videofd = -1;
    curList = 0;
    totalChannels = 0;
    usingv4l2 = false;
    videomode = VIDEO_MODE_NTSC;
}

Channel::~Channel(void)
{
    Close();
}

bool Channel::Open(void)
{
    if (isopen)
        return true;

    videofd = open(device.ascii(), O_RDWR);
    if (videofd > 0)
        isopen = true;
    else
    {
         cerr << "Channel::Open(): Can't open: " << device << endl;
         perror(device.ascii());
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

    return isopen;
}

void Channel::Close(void)
{
    if (isopen)
        close(videofd);
    isopen = false;
    videofd = -1;
}

void Channel::SetFd(int fd)
{
    if (fd > 0)
    {
        videofd = fd;
        isopen = true;
    }
    else
    {
        videofd = -1;
        isopen = false;
    }
}

void Channel::SetFormat(const QString &format)
{
    if (!Open())
        return;
   
    if (usingv4l2)
    {
        struct v4l2_input vin;
        memset(&vin, 0, sizeof(vin));
        vin.index = 0;

        while (ioctl(videofd, VIDIOC_ENUMINPUT, &vin) >= 0)
        {
            cout << "Probed: " << device << " - " << vin.name << endl;
            channelnames[vin.index] = (char *)vin.name;
            inputChannel[vin.index] = "";
            inputTuneTo[vin.index] = "";
            externalChanger[vin.index] = "";
            vin.index++;

            capchannels = vin.index;
        }

        if (format == "NTSC")
            videomode = V4L2_STD_NTSC;
        else if (format == "PAL")
            videomode = V4L2_STD_PAL;
        else if (format == "SECAM")
            videomode = V4L2_STD_SECAM;
        else if (format == "PAL-NC")
            videomode = V4L2_STD_PAL_Nc;
        else if (format == "PAL-M")
            videomode = V4L2_STD_PAL_M;
        else if (format == "PAL-N")
            videomode = V4L2_STD_PAL_N;
        else if (format == "NTSC-JP")
            videomode = V4L2_STD_NTSC_M_JP;

        pParent->RetrieveInputChannels(inputChannel, inputTuneTo, 
                                       externalChanger);
        return;
    }
 
    int mode = VIDEO_MODE_AUTO;
    struct video_tuner tuner;

    memset(&tuner, 0, sizeof(tuner));

    ioctl(videofd, VIDIOCGTUNER, &tuner);
    
    if (format == "NTSC")
        mode = VIDEO_MODE_NTSC;
    else if (format == "PAL")
        mode = VIDEO_MODE_PAL;
    else if (format == "SECAM")
        mode = VIDEO_MODE_SECAM;
    else if (format == "PAL-NC")
        mode = 3;
    else if (format == "PAL-M")
        mode = 4;
    else if (format == "PAL-N")
        mode = 5;
    else if (format == "NTSC-JP")
        mode = 6;

    tuner.mode = mode;
    ioctl(videofd, VIDIOCSTUNER, &tuner);

    struct video_capability vidcap;
    memset(&vidcap, 0, sizeof(vidcap));
    ioctl(videofd, VIDIOCGCAP, &vidcap);

    capchannels = vidcap.channels;
    for (int i = 0; i < vidcap.channels; i++)
    {
        struct video_channel test;
        memset(&test, 0, sizeof(test));
        test.channel = i;
        ioctl(videofd, VIDIOCGCHAN, &test);

        cout << "Probed: " << device << " - " << test.name << endl;
        channelnames[i] = test.name;
        inputChannel[i] = "";
        inputTuneTo[i] = "";
        externalChanger[i] = "";
    }

    struct video_channel vc;
    memset(&vc, 0, sizeof(vc));
    ioctl(videofd, VIDIOCGCHAN, &vc);
    vc.norm = mode;
    ioctl(videofd, VIDIOCSCHAN, &vc);

    videomode = mode;

    pParent->RetrieveInputChannels(inputChannel, inputTuneTo, externalChanger);
}

void Channel::SetFreqTable(const QString &name)
{
    int i = 0;
    char *listname = (char *)chanlists[i].name;

    curList = NULL;
    while (listname != NULL)
    {
        if (name == listname)
        {
            curList = chanlists[i].list;
            totalChannels = chanlists[i].count;
            break;
        }
        i++;
        listname = (char *)chanlists[i].name;
    }

    if (!curList)
    {
        curList = chanlists[1].list;
        totalChannels = chanlists[1].count;
    }
}

int Channel::GetCurrentChannelNum(const QString &channame)
{
    for (int i = 0; i < totalChannels; i++)
    {
        if (channame == curList[i].name)
            return i;
    }

    return -1;
}

bool Channel::ChannelUp(void)
{
    if (ChannelBase::ChannelUp())
        return true;

    QString nextchan;
    bool finished = false;
    int chancount = 0;
    int curchannel = GetCurrentChannelNum(curchannelname);

    while (!finished)
    {
        curchannel++;
        chancount++;

        if (curchannel == totalChannels)
            curchannel = 0;

        nextchan = curList[curchannel].name;

        finished = SetChannelByString(nextchan);

        if (chancount > totalChannels)
        {
            cerr << "Error, couldn't find any available channels.\n";
            cerr << "Your database is most likely setup incorrectly.\n";
            break;
        }
    }

    return finished;
}

bool Channel::ChannelDown(void)
{
    if (ChannelBase::ChannelDown())
        return true;

    QString nextchan;
    bool finished = false;
    int chancount = 0;
    int curchannel = GetCurrentChannelNum(curchannelname);

    while (!finished)
    {
        curchannel--;
        chancount++;

        if (curchannel < 0)
            curchannel = totalChannels - 1;

        nextchan = curList[curchannel].name;

        finished = SetChannelByString(nextchan);

        if (chancount > totalChannels)
        {
            cerr << "Error, couldn't find any available channels.\n";
            cerr << "Your database is most likely setup incorrectly.\n";
            break;
        }
    }

    return finished;
}

bool Channel::SetChannelByString(const QString &chan)
{
    if (!Open())
    {
        cerr << "channel object wasn't open, can't change channels\n";
        return false;
    }

    QSqlDatabase *db_conn;
    pthread_mutex_t db_lock;

    if (!pParent->CheckChannel(this, chan, db_conn, db_lock))
        return false;

    pthread_mutex_lock(&db_lock);
    QString thequery = QString("SELECT finetune, freqid "
                               "FROM channel WHERE channum = \"%1\";")
                               .arg(chan);

    QSqlQuery query = db_conn->exec(thequery);
    if (!query.isActive())
        MythContext::DBError("fetchtuningparamschanid", query);
    if (query.numRowsAffected() <= 0)
    {
        pthread_mutex_unlock(&db_lock);
        return false;
    }
    query.next();

    int finetune = query.value(0).toInt();
    QString freqid = query.value(1).toString();

    pthread_mutex_unlock(&db_lock);

    // Tune
    if (externalChanger[currentcapchannel].isEmpty())
    {
        if (!TuneTo(freqid, finetune))
            return false;
    }
    else if (!ChangeExternalChannel(chan))
        return false;

    curchannelname = chan;

    pParent->SetVideoFiltersForChannel(this, chan);
    SetContrast();
    SetColour();
    SetBrightness();

    inputChannel[currentcapchannel] = curchannelname;

    return true;
}

bool Channel::TuneTo(const QString &channum, int finetune)
{
    int i = GetCurrentChannelNum(channum);
    if (i == -1)
        return false;

    int frequency = curList[i].freq * 16 / 1000 + finetune;

    if (usingv4l2)
    {
        struct v4l2_frequency vf;
        memset(&vf, 0, sizeof(vf));
        vf.frequency = frequency;
        vf.type = V4L2_TUNER_ANALOG_TV;

        if (ioctl(videofd, VIDIOC_S_FREQUENCY, &vf) < 0)
        {
            perror("VIDIOC_S_FREQUENCY");
            return false;
        }
        return true;
    }

    if (ioctl(videofd, VIDIOCSFREQ, &frequency) == -1)
    {
        perror("VIDIOCSFREQ");
        return false;
    }

    return true;
}

void Channel::SwitchToInput(int newcapchannel, bool setstarting)
{
    if (newcapchannel == currentcapchannel)
        return;

    if (usingv4l2)
    {
        if (ioctl(videofd, VIDIOC_S_INPUT, &newcapchannel) < 0)
            perror("VIDIOC_S_INPUT");
   
        if (ioctl(videofd, VIDIOC_S_STD, &videomode) < 0)
            perror("VIDIOC_S_STD");
    }
    else
    {
        struct video_channel set;
        memset(&set, 0, sizeof(set));
        ioctl(videofd, VIDIOCGCHAN, &set);
        set.channel = newcapchannel;
        set.norm = videomode;
        if (ioctl(videofd, VIDIOCSCHAN, &set) < 0)
           perror("VIDIOCSCHAN");
    }

    currentcapchannel = newcapchannel;
    curchannelname = "";

    if (inputTuneTo[currentcapchannel] != "Undefined")
        TuneTo(inputTuneTo[currentcapchannel], 0);

    if (setstarting && !inputChannel[currentcapchannel].isEmpty())
        SetChannelByString(inputChannel[currentcapchannel]);
}

void Channel::SetContrast()
{
   struct video_picture vid_pic;
   memset(&vid_pic, 0, sizeof(vid_pic));

    if (ioctl(videofd, VIDIOCGPICT, &vid_pic) < 0)
    {
        return;
    }

    QString field_name = "contrast";
    int contrast = pParent->GetChannelValue(field_name, this, curchannelname);
    if (contrast != -1)
    {
        vid_pic.contrast = contrast;

        if (ioctl(videofd, VIDIOCSPICT, &vid_pic) < 0)
        {
            return;
        }
    }

    return;
}

void Channel::SetBrightness()
{
   struct video_picture vid_pic;
   memset(&vid_pic, 0, sizeof(vid_pic));

    if (ioctl(videofd, VIDIOCGPICT, &vid_pic) < 0)
    {
        return;
    }

    QString field_name = "brightness";
    int brightness = pParent->GetChannelValue(field_name, this, curchannelname);
    if (brightness != -1)
    {
        vid_pic.brightness = brightness;

        if (ioctl(videofd, VIDIOCSPICT, &vid_pic) < 0)
        {
            return;
        }
    }

    return;
}

void Channel::SetColour()
{
   struct video_picture vid_pic;
   memset(&vid_pic, 0, sizeof(vid_pic));

    if (ioctl(videofd, VIDIOCGPICT, &vid_pic) < 0)
    {
        return;
    }

    QString field_name = "colour";
    int colour = pParent->GetChannelValue(field_name, this, curchannelname);
    if (colour != -1)
    {
        vid_pic.colour = colour;

        if (ioctl(videofd, VIDIOCSPICT, &vid_pic) < 0)
        {
            return;
        }
    }

    return;
}

int Channel::ChangeContrast(bool up)
{
    struct video_picture vid_pic;
    memset(&vid_pic, 0, sizeof(vid_pic));

    int newcontrast;    // The int should have ample space to avoid overflow
                        // in the case that we're just over or under 65535

    QString channel_field = "contrast";
    int current_contrast = pParent->GetChannelValue(channel_field, this, 
                                                    curchannelname);

    if (ioctl(videofd, VIDIOCGPICT, &vid_pic) < 0)
    {
        return -1;
    }
    if (current_contrast < -1) // Couldn't get from database
    {
        if (up)
        {
            newcontrast = vid_pic.contrast + 655;
            newcontrast = (newcontrast > 65535)?(65535):(newcontrast);
        }
        else
        {
            newcontrast = vid_pic.contrast - 655;
            newcontrast = (newcontrast < 0)?(0):(newcontrast);
        }
    }
    else
    {
        if (up)
        {
            newcontrast = current_contrast + 655;
            newcontrast = (newcontrast > 65535)?(65535):(newcontrast);
        }
        else
        {
            newcontrast = current_contrast - 655;
            newcontrast = (newcontrast < 0)?(0):(newcontrast);
        }

        QString field_name = "contrast";
        pParent->SetChannelValue(field_name, newcontrast, this, curchannelname);
    }

    vid_pic.contrast = newcontrast;

    if (ioctl(videofd, VIDIOCSPICT, &vid_pic) < 0)
    {
        return -1;
    }

    return vid_pic.contrast / 655;
}

int Channel::ChangeBrightness(bool up)
{
    struct video_picture vid_pic;
    memset(&vid_pic, 0, sizeof(vid_pic));

    int newbrightness;  // The int should have ample space to avoid overflow
                        // in the case that we're just over or under 65535

    QString channel_field = "brightness";
    int current_brightness = pParent->GetChannelValue(channel_field, this, 
                                                      curchannelname);

    if (ioctl(videofd, VIDIOCGPICT, &vid_pic) < 0)
    {
        return -1;
    }
    if (current_brightness < -1) // Couldn't get from database
    {
        if (up)
        {
            newbrightness = vid_pic.brightness + 655;
            newbrightness = (newbrightness > 65535)?(65535):(newbrightness);
        }
        else
        {
            newbrightness = vid_pic.brightness - 655;
            newbrightness = (newbrightness < 0)?(0):(newbrightness);
        }
    }
    else
    {
        if (up)
        {
            newbrightness = current_brightness + 655;
            newbrightness = (newbrightness > 65535)?(65535):(newbrightness);
        }
        else
        {
            newbrightness = current_brightness - 655;
            newbrightness = (newbrightness < 0)?(0):(newbrightness);
        }

        QString field_name = "brightness";
        pParent->SetChannelValue(field_name, newbrightness, this, 
                                 curchannelname);
    }

    vid_pic.brightness = newbrightness;

    if (ioctl(videofd, VIDIOCSPICT, &vid_pic) < 0)
    {
        return -1;
    }

    return vid_pic.brightness / 655;
}

int Channel::ChangeColour(bool up)
{
    struct video_picture vid_pic;
    memset(&vid_pic, 0, sizeof(vid_pic));

    int newcolour;    // The int should have ample space to avoid overflow
                      // in the case that we're just over or under 65535

    QString channel_field = "colour";
    int current_colour = pParent->GetChannelValue(channel_field, this, 
                                                  curchannelname);

    if (ioctl(videofd, VIDIOCGPICT, &vid_pic) < 0)
    {
        return -1;
    }
    if (current_colour < -1) // Couldn't get from database
    {
        if (up)
        {
            newcolour = vid_pic.colour + 655;
            newcolour = (newcolour > 65535)?(65535):(newcolour);
        }
        else
        {
            newcolour = vid_pic.colour - 655;
            newcolour = (newcolour < 0)?(0):(newcolour);
        }
    }
    else
    {
        if (up)
        {
            newcolour = current_colour + 655;
            newcolour = (newcolour > 65535)?(65535):(newcolour);
        }
        else
        {
            newcolour = current_colour - 655;
            newcolour = (newcolour < 0)?(0):(newcolour);
        }

        QString field_name = "colour";
        pParent->SetChannelValue(field_name, newcolour, this, curchannelname);
    }

    vid_pic.colour = newcolour;

    if (ioctl(videofd, VIDIOCSPICT, &vid_pic) < 0)
    {
        return -1;
    }
    
    return vid_pic.colour / 655;
}
