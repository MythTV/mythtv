/**
 *  FirewireChannel
 *  Copyright (c) 2005 by Jim Westfall
 *  Distributed as part of MythTV under GPL v2 and later.
 */


#include <iostream>
#include "mythcontext.h"
#include "firewirechannel.h"

class TVRec;


FirewireChannel::FirewireChannel(TVRec *parent): ChannelBase(parent) {
    capchannels = 1;
    channelnames[0] = "MPEG2TS";
}

FirewireChannel::~FirewireChannel(void)
{   
}

bool FirewireChannel::SetChannelByString(const QString &chan) {


     inputChannel[currentcapchannel] = chan;
     curchannelname = chan;

     if (externalChanger[currentcapchannel].isEmpty()) {
         VERBOSE(VB_IMPORTANT,QString("FireWire: external channel changer only supported at this time.")); 
     } else if(!ChangeExternalChannel(chan)) {
         return false;
     }
     return true;
}

bool FirewireChannel::Open() {
    SetExternalChanger();
    return true;
}
void FirewireChannel::Close() {

}
    
void FirewireChannel::SwitchToInput(const QString &input, const QString &chan)
{
    currentcapchannel = 0;
    if (channelnames.empty())
       channelnames[currentcapchannel] = input;

    SetChannelByString(chan);
}

void FirewireChannel::SetExternalChanger(void) {
	
     pParent->RetrieveInputChannels(inputChannel, inputTuneTo,
                                       externalChanger, sourceid);
}
