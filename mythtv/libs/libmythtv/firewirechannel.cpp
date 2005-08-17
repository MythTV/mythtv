/**
 *  FirewireChannel
 *  Copyright (c) 2005 by Jim Westfall
 *  Distributed as part of MythTV under GPL v2 and later.
 */


#include <iostream>
#include "mythcontext.h"
#include "firewirechannel.h"

class TVRec;


FirewireChannel::FirewireChannel(firewire_options_t firewire_opts, TVRec *parent)
    : ChannelBase(parent),fw_opts(firewire_opts)
{
	
    isopen = false;
    capchannels = 1;
    fwhandle = NULL;
    channelnames[0] = "MPEG2TS";

    if (externalChanger[currentcapchannel].isEmpty())
    {
        if (fw_opts.model == "DCT-6200")
        {
            if ((fwhandle = raw1394_new_handle_on_port(fw_opts.port)) == NULL)
            {
                VERBOSE(VB_IMPORTANT,
                        QString("FireWireChannel: unable to get "
                                "handle for port: %1").arg(fw_opts.port));
                return;
            }

            VERBOSE(VB_GENERAL, QString("FireWireChannel: allocated raw1394 "
                                        "handle for port %1").arg(fw_opts.port));
            isopen = true;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "FireWireChannel: internal channel "
                    "changer only supported by DCT-6200 models");
        }
    }
}

FirewireChannel::~FirewireChannel(void)
{
    if (isopen)
    {
        VERBOSE(VB_GENERAL,QString("FireWireChannel: releasing raw1394 handle"));
        raw1394_destroy_handle(fwhandle);
    }
}

bool FirewireChannel::SetChannelByString(const QString &chan)
{
     int dig[3];
     int channel = chan.toInt();
     quadlet_t cmd[2];

     inputChannel[currentcapchannel] = chan;
     curchannelname = chan;

     if (externalChanger[currentcapchannel].isEmpty())
     {
         dig[2] = channel % 10;
         dig[1] = (channel % 100) / 10;
         dig[0] = (channel % 1000) / 100;

         if (isopen && fw_opts.model == "DCT-6200")
         {
            VERBOSE(VB_GENERAL, QString("FireWireChannel: channel:%1%2%3 "
                                        "cmds: 0x%4, 0x%5, 0x%6")
                    .arg(dig[0]).arg(dig[1])
                    .arg(dig[2]).arg(DCT6200_CMD0 | dig[0], 0, 16)
                    .arg(DCT6200_CMD0 | dig[1], 0, 16)
                    .arg(DCT6200_CMD0 | dig[2], 0, 16));
            for (int i = 0;i < 3 ;i++)
            {
                cmd[0] = DCT6200_CMD0 | dig[i];
                cmd[1] = 0x0;
     
                avc1394_transaction_block(fwhandle, fw_opts.node, cmd, 2, 1);
                usleep(500000);
            }
         }
         else
         {
             VERBOSE(VB_IMPORTANT, "FireWireChannel: internal channel "
                     "changer only supported by DCT-6200 models");
         }
     }
     else if (!ChangeExternalChannel(chan))
     {
         return false;
     }
     return true;
}

bool FirewireChannel::Open()
{
    SetExternalChanger();
    return true;
}

void FirewireChannel::Close()
{
}
    
bool FirewireChannel::SwitchToInput(const QString &input, const QString &chan)
{
    currentcapchannel = 0;
    if (channelnames.empty())
       channelnames[currentcapchannel] = input;

    return SetChannelByString(chan);
}

void FirewireChannel::SetExternalChanger(void)
{	
     pParent->RetrieveInputChannels(
         inputChannel, inputTuneTo, externalChanger, sourceid);
}
