#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "videodev_myth.h"
#include "channel.h"
#include "frequencies.h"
#include "tv.h"

#include <iostream>
using namespace std;

ChannelBase::ChannelBase(TVRec *parent)
{
    curchannelname = "";
    pParent = parent;
    capchannels = 0;
    currentcapchannel = -1;
    channelorder = "channum + 0";

    //XXX from setformat pParent->RetrieveInputChannels(inputChannel,inputTuneTo,externalChanger);
}

ChannelBase::~ChannelBase(void)
{
}

bool ChannelBase::ChannelUp(void)
{
    QString nextchan = pParent->GetNextChannel(this, CHANNEL_DIRECTION_UP);
    if (SetChannelByString(nextchan))
        return true;
    else
        return false;
}

bool ChannelBase::ChannelDown(void)
{
    QString nextchan = pParent->GetNextChannel(this, CHANNEL_DIRECTION_DOWN);
    if (SetChannelByString(nextchan))
        return true;
    else
        return false;
}

bool ChannelBase::NextFavorite(void)
{
    QString nextchan = pParent->GetNextChannel(this, 
                                               CHANNEL_DIRECTION_FAVORITE);
    return SetChannelByString(nextchan);
}

QString ChannelBase::GetCurrentName(void)
{
    return curchannelname;
}

QString ChannelBase::GetCurrentInput(void)
{
    return channelnames[currentcapchannel];
}

int ChannelBase::GetCurrentInputNum(void)
{
    return currentcapchannel;
}

void ChannelBase::ToggleInputs(void)
{
    int newcapchannel = currentcapchannel;

    do 
    {
        newcapchannel = (newcapchannel + 1) % capchannels;
    } while (inputTuneTo[newcapchannel].isEmpty());

    SwitchToInput(newcapchannel, true);
}

QString ChannelBase::GetInputByNum(int capchannel)
{
    if (capchannel > capchannels)
        return "";
    return channelnames[capchannel];
}

int ChannelBase::GetInputByName(const QString &input)
{
    for (int i = capchannels-1; i >= 0; i--)
        if (channelnames[i] == input)
            return i;
    return -1;
}

void ChannelBase::SwitchToInput(const QString &inputname)
{
    int input = GetInputByName(inputname);

    if (input >= 0)
        SwitchToInput(input, true);
    else
        cerr << "Couldn't find input: " << inputname << " on card\n";
}

void ChannelBase::SwitchToInput(const QString &inputname, const QString &chan)
{
    int input = GetInputByName(inputname);

    if (input >= 0)
    {
        SwitchToInput(input, false);
        SetChannelByString(chan);
    }
    else
        cerr << "Couldn't find input: " << inputname << " on card\n";
}

bool ChannelBase::ChangeExternalChannel(const QString &channum)
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
        _exit(1);
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

void ChannelBase::StoreInputChannels(void)
{
    pParent->StoreInputChannels(inputChannel);
}
