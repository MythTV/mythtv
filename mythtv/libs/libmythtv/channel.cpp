#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <linux/videodev.h>
#include "channel.h"
#include "frequencies.h"
#include "tv.h"

#include <iostream>
using namespace std;

#ifdef HAVE_V4L2
#ifdef V4L2_CAP_VIDEO_CAPTURE
#define USING_V4L2 1
#else
#warning old broken version of v4l2 detected.
#endif
#endif

Channel::Channel(TVRec *parent, const QString &videodevice)
{
    device = videodevice;
    isopen = false;
    videofd = -1;
    curchannelname = "";
    curList = 0;
    totalChannels = 0;
    pParent = parent;
    usingv4l2 = false;
    videomode = VIDEO_MODE_NTSC;
    capchannels = 0;
    currentcapchannel = -1;
    
    channelorder = "channum + 0";
}

Channel::~Channel(void)
{
    if (isopen)
        close(videofd);
}

bool Channel::Open(void)
{
    videofd = open(device.ascii(), O_RDWR);
    if (videofd > 0)
        isopen = true;
    else
    {
         cout << "Can't open: " << device << endl;
         perror(device.ascii());
         return false;
    }

#ifdef USING_V4L2
    struct v4l2_capability vcap;
    memset(&vcap, 0, sizeof(vcap));
    if (ioctl(videofd, VIDIOC_QUERYCAP, &vcap) < 0)
        usingv4l2 = false;
    else
    {
        if (vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
            usingv4l2 = true;
    }
#endif
    return isopen;
}

void Channel::Close(void)
{
    if (isopen)
        close(videofd);
    videofd = -1;
}

void Channel::SetFormat(const QString &format)
{
    if (!isopen)
        Open();

    if (!isopen)
        return;
   
#ifdef USING_V4L2
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
#endif
 
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
        curList = chanlists[0].list;
        totalChannels = chanlists[0].count;
    }
}
 
bool Channel::SetChannelByString(const QString &chan)
{
    if (!isopen)
        Open();
    if (!isopen)
        return false;

    if (curchannelname == chan)
        return true;

    int finetune = 0;

    if (pParent->CheckChannel(this, chan, finetune))
    {
        if (externalChanger[currentcapchannel].isEmpty())
        {
            int i = GetCurrentChannelNum(chan);
            if (i == -1 || !TuneTo(chan, finetune))
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

    return false;
}

bool Channel::TuneTo(const QString &chan, int finetune)
{
    int i = GetCurrentChannelNum(chan);
    int frequency = curList[i].freq * 16 / 1000 + finetune;

#ifdef USING_V4L2
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
#endif

    if (ioctl(videofd, VIDIOCSFREQ, &frequency) == -1)
    {
        perror("VIDIOCSFREQ");
        return false;
    }

    return true;
}

int Channel::GetCurrentChannelNum(const QString &channame)
{
    bool foundit = false;
    int i;
    for (i = 0; i < totalChannels; i++)
    {
        if (channame == curList[i].name)
        {
            foundit = true;
            break;
        }
    }

    if (foundit)
        return i;
    
    return -1;
}

bool Channel::ChannelUp(void)
{
    QString nextchan = pParent->GetNextChannel(this, CHANNEL_DIRECTION_UP);
    if (SetChannelByString(nextchan))
        return true;

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
    QString nextchan = pParent->GetNextChannel(this, CHANNEL_DIRECTION_DOWN);
    if (SetChannelByString(nextchan))
        return true;

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

bool Channel::NextFavorite(void)
{
    QString nextchan = pParent->GetNextChannel(this, 
                                               CHANNEL_DIRECTION_FAVORITE);
    return SetChannelByString(nextchan);
}

QString Channel::GetCurrentName(void)
{
    return curchannelname;
}

QString Channel::GetCurrentInput(void)
{
    return channelnames[currentcapchannel];
}

void Channel::ToggleInputs(void)
{
    int newcapchannel = currentcapchannel;

    do 
    {
        newcapchannel = (newcapchannel + 1) % capchannels;
    } while (inputTuneTo[newcapchannel].isEmpty());

    SwitchToInput(newcapchannel, true);
}

QString Channel::GetInputByNum(int capchannel)
{
    if (capchannel > capchannels)
        return "";
    return channelnames[capchannel];
}

int Channel::GetInputByName(const QString &input)
{
    for (int i = capchannels-1; i >= 0; i--)
        if (channelnames[i] == input)
            return i;
    return -1;
}

void Channel::SwitchToInput(const QString &inputname)
{
    int input = GetInputByName(inputname);

    if (input >= 0)
        SwitchToInput(input, true);
}

void Channel::SwitchToInput(const QString &inputname, const QString &chan)
{
    int input = GetInputByName(inputname);

    if (input >= 0)
    {
        SwitchToInput(input, false);
        SetChannelByString(chan);
    }
}

void Channel::SwitchToInput(int newcapchannel, bool setstarting)
{
    if (newcapchannel == currentcapchannel)
        return;

#ifdef USING_V4L2
    if (usingv4l2)
    {
        if (ioctl(videofd, VIDIOC_S_INPUT, &newcapchannel) < 0)
            perror("VIDIOC_S_INPUT");
   
        if (ioctl(videofd, VIDIOC_S_STD, &videomode) < 0)
            perror("VIDIOC_S_STD");
    }
    else
#endif
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
        perror("VIDIOCGPICT");
        return;
    }

    QString field_name = "contrast";
    int contrast = pParent->GetChannelValue(field_name, this, curchannelname);
    if (contrast != -1)
    {
        vid_pic.contrast = contrast;

        if (ioctl(videofd, VIDIOCSPICT, &vid_pic) < 0)
        {
            perror("VIDIOCSPICT");
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
        perror("VIDIOCGPICT");
        return;
    }

    QString field_name = "brightness";
    int brightness = pParent->GetChannelValue(field_name, this, curchannelname);
    if (brightness != -1)
    {
        vid_pic.brightness = brightness;

        if (ioctl(videofd, VIDIOCSPICT, &vid_pic) < 0)
        {
            perror("VIDIOCSPICT");
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
        perror("VIDIOCGPICT");
        return;
    }

    QString field_name = "colour";
    int colour = pParent->GetChannelValue(field_name, this, curchannelname);
    if (colour != -1)
    {
        vid_pic.colour = colour;

        if (ioctl(videofd, VIDIOCSPICT, &vid_pic) < 0)
        {
            perror("VIDIOCSPICT");
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
        perror("VIDIOCGPICT");
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
        perror("VIDIOCSPICT");
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
        perror("VIDIOCGPICT");
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
        perror("VIDIOCSPICT");
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
        perror("VIDIOCGPICT");
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
        perror("VIDIOCSPICT");
        return -1;
    }
    
    return vid_pic.colour / 655;
}

bool Channel::ChangeExternalChannel(const QString &channum)
{
    if (externalChanger[currentcapchannel].isEmpty())
        return false;

    QString command = QString("%1 %2").arg(externalChanger[currentcapchannel])
                                      .arg(channum);

    cout << "External channel change: " << command << endl;
    pid_t child = fork();
    if (child < 0)
    {
        perror("fork");
    }
    else if (child == 0)
    {
        for(int i = 3; i < sysconf(_SC_OPEN_MAX) - 1; ++i)
            close(i);
        execl("/bin/sh", "sh", "-c", command.ascii(), NULL);
        perror("exec");
        exit(1);
    }
    else
    {
        int status;
        if (waitpid(child, &status, 0) < 0)
        {
            perror("waitpid");
        }
        else if (status != 0)
        {
            cerr << "External channel change command exited with status "
                 << status << endl;
            return false;
        }
    }

    return true;
}

void Channel::StoreInputChannels(void)
{
    pParent->StoreInputChannels(inputChannel);
}

