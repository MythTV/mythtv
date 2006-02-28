/**
 *  FirewireChannel
 *  Copyright (c) 2005 by Jim Westfall
 *  SA3250HD support Copyright (c) 2005 by Matt Porter
 *  Distributed as part of MythTV under GPL v2 and later.
 */


#include <iostream>
#include "mythcontext.h"
#include "firewirechannel.h"

class TVRec;

#define LOC QString("FireChan: ")
#define LOC_ERR QString("FireChan, Error: ")

// 6200 defines for channel changes, taken from contrib/6200ch.c
/// 6200 uses a reserved subunit type
#define AVC1394_SUBUNIT_TYPE_6200 	(9 << 19)
/// 6200 subunit command
#define AVC1394_6200_COMMAND_CHANNEL 	0x00007C00
/// 6200 subunit command operand
#define AVC1394_6200_OPERAND_SET 	0x00000020
#define DCT6200_CMD0 (AVC1394_CTYPE_CONTROL | AVC1394_SUBUNIT_TYPE_6200    | \
                      AVC1394_SUBUNIT_ID_0  | AVC1394_6200_COMMAND_CHANNEL | \
                      AVC1394_6200_OPERAND_SET)

// SA3250HD defines
#define AVC1394_SA3250_COMMAND_CHANNEL		0x000007c00
#define AVC1394_SA3250_OPERAND_KEY_PRESS	0xe7
#define AVC1394_SA3250_OPERAND_KEY_RELEASE	0x67
#define SA3250_CMD0 (AVC1394_CTYPE_CONTROL | AVC1394_SUBUNIT_TYPE_PANEL | \
                     AVC1394_SUBUNIT_ID_0  | AVC1394_SA3250_COMMAND_CHANNEL)
#define SA3250_CMD1 (0x04 << 24)
#define SA3250_CMD2 0xff000000


static bool is_supported(const QString &model)
{
    return ((model == "DCT-6200") ||
            (model == "SA3250HD"));
}

FirewireChannel::FirewireChannel(FireWireDBOptions firewire_opts,
                                 TVRec *parent)
    : ChannelBase(parent), fw_opts(firewire_opts),
      fwhandle(NULL),      isopen(false)
{
}

FirewireChannel::~FirewireChannel(void)
{
    Close();
}

bool FirewireChannel::SetChannelByString(const QString &chan)
{
    inputs[currentInputID]->startChanNum = chan;
    curchannelname = chan;

    InputMap::const_iterator it = inputs.find(currentInputID);

    if (!(*it)->externalChanger.isEmpty())
        return ChangeExternalChannel(chan);

    // Change channel using internal changer

    if (!is_supported(fw_opts.model))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Model: '%1' ").arg(fw_opts.model) +
                "is not supported by internal channel changer.");
        return false;
    }

    if (!isopen)
        return false;

    int channel = chan.toInt();
    int dig[3];
    dig[0] = (channel % 1000) / 100;
    dig[1] = (channel % 100)  / 10;
    dig[2] = (channel % 10);

    if (fw_opts.model == "DCT-6200")
    {
        VERBOSE(VB_CHANNEL, LOC +
                QString("Channel1: %1%2%3 cmds: 0x%4, 0x%5, 0x%6")
                .arg(dig[0]).arg(dig[1])
                .arg(dig[2]).arg(DCT6200_CMD0 | dig[0], 0, 16)
                .arg(DCT6200_CMD0 | dig[1], 0, 16)
                .arg(DCT6200_CMD0 | dig[2], 0, 16));

        for (uint i = 0; i < 3 ;i++)
        {
            quadlet_t cmd[2] =  { DCT6200_CMD0 | dig[i], 0x0, };
            avc1394_transaction_block(fwhandle, fw_opts.node, cmd, 2, 1);
            usleep(500000);
        }
    }
    else if (fw_opts.model == "SA3250HD")
    {
        dig[0] |= 0x30;
        dig[1] |= 0x30;
        dig[2] |= 0x30;

        quadlet_t cmd[3] =
        {
            SA3250_CMD0 | AVC1394_SA3250_OPERAND_KEY_PRESS, 
            SA3250_CMD1 | (dig[2] << 16) | (dig[1] << 8) | dig[0],
            SA3250_CMD2,
        };

        VERBOSE(VB_CHANNEL, LOC +
                QString("Channel2: %1%2%3 cmds: 0x%4, 0x%5, 0x%6")
                .arg(dig[0] & 0xf).arg(dig[1] & 0xf)
                .arg(dig[2] & 0xf)
                .arg(cmd[0], 0, 16).arg(cmd[1], 0, 16)
                .arg(cmd[2], 0, 16));

        avc1394_transaction_block(fwhandle, fw_opts.node, cmd, 3, 1);

        cmd[0] = SA3250_CMD0 | AVC1394_SA3250_OPERAND_KEY_RELEASE;
        cmd[1] = SA3250_CMD1 | (dig[0] << 16) | (dig[1] << 8) | dig[2];
        cmd[2] = SA3250_CMD2;

        VERBOSE(VB_CHANNEL, LOC +
                QString("Channel3: %1%2%3 cmds: 0x%4, 0x%5, 0x%6")
                .arg(dig[0] & 0xf).arg(dig[1] & 0xf)
                .arg(dig[2] & 0xf)
                .arg(cmd[0], 0, 16).arg(cmd[1], 0, 16)
                .arg(cmd[2], 0, 16));

        avc1394_transaction_block(fwhandle, fw_opts.node, cmd, 3, 1);
    }

    return true;
}

bool FirewireChannel::Open(void)
{
    if (!InitializeInputs())
        return false;

    InputMap::const_iterator it = inputs.find(currentInputID);
    if (!(*it)->externalChanger.isEmpty())
        return true;

    if (!is_supported(fw_opts.model))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Model: '%1' ").arg(fw_opts.model) +
                "is not supported by internal channel changer.");
        return false;
    }

    // Open channel
    fwhandle = raw1394_new_handle_on_port(fw_opts.port);
    if (!fwhandle)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Unable to get handle " +
                QString("for port: %1").arg(fw_opts.port));
        return false;
    }

    VERBOSE(VB_CHANNEL, LOC + "Allocated raw1394 handle " +
            QString("for port %1").arg(fw_opts.port));

    isopen = true;
    return true;
}

void FirewireChannel::Close(void)
{
    // Close channel
    if (isopen)
    {
        VERBOSE(VB_CHANNEL, LOC + "Releasing raw1394 handle");
        raw1394_destroy_handle(fwhandle);
        isopen = false;
    }
}

bool FirewireChannel::SwitchToInput(const QString &input, const QString &chan)
{
    int inputNum = GetInputByName(input);
    if (inputNum < 0)
        return false;

    // input switching code would go here

    return SetChannelByString(chan);
}
