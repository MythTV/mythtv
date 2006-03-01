/**
 *  FirewireChannelBase
 *  Copyright (c) 2005 by Jim Westfall Dave Abrahams
 *  Distributed as part of MythTV under GPL v2 and later.
 */


#include <iostream>
#include "mythcontext.h"
#include "firewirechannelbase.h"

bool FirewireChannelBase::UseExternalChanger()
{ 
    return !externalChanger[currentcapchannel].isEmpty(); 
}

FirewireChannelBase::FirewireChannelBase(TVRec *parent)
  : ChannelBase(parent), isopen(false)
{
	
    isopen = false;
    channelnames[0] = "MPEG2TS";
}

FirewireChannelBase::~FirewireChannelBase(void)
{
    this->Close();
}

bool FirewireChannelBase::SetChannelByString(const QString &chan)
{
     inputChannel[currentcapchannel] = chan;
     curchannelname = chan;

     if (this->UseExternalChanger())
         return ChangeExternalChannel(chan);
     else
         return isopen && SetChannelByNumber(chan.toInt());
}

bool FirewireChannelBase::Open()
{
    if (this->UseExternalChanger())
    {
        this->SetExternalChanger();
        return true;
    }
    else
    {
        if (!this->isopen)
            this->isopen = this->OpenFirewire();
        return this->isopen;
    }
}

void FirewireChannelBase::Close()
{
    if (this->isopen)
        CloseFirewire();
    this->isopen = false;
}
    
bool FirewireChannelBase::SwitchToInput(const QString &input, const QString &chan)
{
    currentcapchannel = 0;
    if (channelnames.empty())
       channelnames[currentcapchannel] = input;

    return SetChannelByString(chan);
}

void FirewireChannelBase::SetExternalChanger(void)
{	
    RetrieveInputChannels(inputChannel, inputTuneTo,
                          externalChanger, sourceid);
}
