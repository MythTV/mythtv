#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/videodev.h>
#include "channel.h"
#include "frequencies.h"

Channel::Channel(const string &videodevice)
{
    device = videodevice;
    isopen = false;
    videofd = -1;
    curchannel = -1;
}

Channel::~Channel(void)
{
    if (isopen)
        close(videofd);
}

void Channel::Open(void)
{
    videofd = open(device.c_str(), O_RDWR);
    if (videofd > 0)
        isopen = true;
}

void Channel::Close(void)
{
    if (isopen)
        close(videofd);
    videofd = -1;
}

void Channel::SetFormat(const string &format)
{
    if (!isopen)
        Open();

    if (!isopen)
        return;
    
    int mode = VIDEO_MODE_AUTO;
    struct video_tuner tuner;

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
}

void Channel::SetFreqTable(const string &name)
{
    int i = 0;
    char *listname = chanlists[i].name;

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
        listname = chanlists[i].name;
    }
    if (!curList)
    {
        curList = chanlists[0].list;
	totalChannels = chanlists[0].count;
    }
}
  
bool Channel::SetChannelByString(const string &chan)
{
    if (!isopen)
        Open();
    if (!isopen)
        return false;
    
    bool foundit = false;
    int i;
    for (i = 0; i < totalChannels; i++)
    {
        if (chan == curList[i].name)
	{
	    foundit = true;
	    break;
	}
    }
    if (!foundit)
        return false;

    return SetChannel(i);
}
   
bool Channel::SetChannel(int i)
{
    int frequency = curList[i].freq * 16 / 1000;
    if (ioctl(videofd, VIDIOCSFREQ, &frequency) == -1)
        perror("channel set:");

    curchannel = i;
    
    return true;
}

bool Channel::ChannelUp(void)
{
    curchannel++;

    if (curchannel == totalChannels)
        curchannel = 0;

    return SetChannel(curchannel);
}

bool Channel::ChannelDown(void)
{
    curchannel--;

    if (curchannel < 0)
        curchannel = totalChannels - 1;

    return SetChannel(curchannel);
}

char *Channel::GetCurrentChannelName(void)
{
    return curList[curchannel].name;
}

