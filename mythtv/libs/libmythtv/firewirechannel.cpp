/**
 *  FirewireChannel
 *  Copyright (c) 2005 by Jim Westfall
 *  SA3250HD support Copyright (c) 2005 by Matt Porter
 *  SA4200HD/Alternate 3250 support Copyright (c) 2006 by Chris Ingrassia
 *  Distributed as part of MythTV under GPL v2 and later.
 */


#include <iostream>
#include "mythcontext.h"
#include "firewirechannel.h"

class TVRec;

#define LOC QString("FireChan: ")
#define LOC_ERR QString("FireChan, Error: ")

#ifndef AVC1394_PANEL_COMMAND_PASS_THROUGH
#define AVC1394_PANEL_COMMAND_PASS_THROUGH     0x000007C00
#endif

#ifndef AVC1394_PANEL_OPERATION_0
#define AVC1394_PANEL_OPERATION_0              0x000000020
#endif

#define DCT6200_CMD0  (AVC1394_CTYPE_CONTROL | \
                       AVC1394_SUBUNIT_TYPE_PANEL | \
                       AVC1394_SUBUNIT_ID_0 | \
                       AVC1394_PANEL_COMMAND_PASS_THROUGH | \
                       AVC1394_PANEL_OPERATION_0)

// SA3250HD defines
#define AVC1394_SA3250_OPERAND_KEY_PRESS	0xE7
#define AVC1394_SA3250_OPERAND_KEY_RELEASE	0x67

#define SA3250_CMD0   (AVC1394_CTYPE_CONTROL | \
                       AVC1394_SUBUNIT_TYPE_PANEL | \
                       AVC1394_SUBUNIT_ID_0 | \
                       AVC1394_PANEL_COMMAND_PASS_THROUGH)
#define SA3250_CMD1   (0x04 << 24)
#define SA3250_CMD2    0xff000000

// power defines
#define AVC1394_CMD_OPERAND_POWER_STATE        0x7F
#define STB_POWER_STATE   (AVC1394_CTYPE_STATUS | \
                           AVC1394_SUBUNIT_TYPE_UNIT | \
                           AVC1394_SUBUNIT_ID_IGNORE | \
                           AVC1394_COMMAND_POWER | \
                           AVC1394_CMD_OPERAND_POWER_STATE)

#define STB_POWER_ON      (AVC1394_CTYPE_CONTROL | \
                           AVC1394_SUBUNIT_TYPE_UNIT | \
                           AVC1394_SUBUNIT_ID_IGNORE | \
                           AVC1394_COMMAND_POWER | \
                           AVC1394_CMD_OPERAND_POWER_ON)

static bool is_supported(const QString &model)
{
    return ((model == "DCT-6200") ||
            (model == "SA3250HD") ||
	    (model == "SA4200HD"));
}

FirewireChannel::FirewireChannel(FireWireDBOptions firewire_opts,
                                 TVRec *parent)
    : FirewireChannelBase(parent), fw_opts(firewire_opts), fwhandle(NULL)
{
}

FirewireChannel::~FirewireChannel(void)
{
    Close();
}

bool FirewireChannel::SetChannelByNumber(int channel)
{
    // Change channel using internal changer

    if (!is_supported(fw_opts.model))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Model: '%1' ").arg(fw_opts.model) +
                "is not supported by internal channel changer.");
        return false;
    }

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
            if (!avc1394_transaction_block(fwhandle, fw_opts.node, cmd, 2, 1))
            {
                VERBOSE(VB_IMPORTANT, "AVC transaction failed.");
                return false;
            }
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

        if(!avc1394_transaction_block(fwhandle, fw_opts.node, cmd, 3, 1))
        {
            VERBOSE(VB_IMPORTANT, "AVC transaction failed.");
            return false;
        }

        cmd[0] = SA3250_CMD0 | AVC1394_SA3250_OPERAND_KEY_RELEASE;
        cmd[1] = SA3250_CMD1 | (dig[0] << 16) | (dig[1] << 8) | dig[2];
        cmd[2] = SA3250_CMD2;

        VERBOSE(VB_CHANNEL, LOC +
                QString("Channel3: %1%2%3 cmds: 0x%4, 0x%5, 0x%6")
                .arg(dig[0] & 0xf).arg(dig[1] & 0xf)
                .arg(dig[2] & 0xf)
                .arg(cmd[0], 0, 16).arg(cmd[1], 0, 16)
                .arg(cmd[2], 0, 16));

        if (!avc1394_transaction_block(fwhandle, fw_opts.node, cmd, 3, 1))
        {
            VERBOSE(VB_IMPORTANT, "AVC transaction failed.");
            return false;
        }
    }
    else if (fw_opts.model == "SA4200HD")
    {
        quadlet_t cmd[3] =
        {
            SA3250_CMD0 | AVC1394_SA3250_OPERAND_KEY_PRESS,
            SA3250_CMD1 | (channel << 8),
            SA3250_CMD2,
        };

        VERBOSE(VB_CHANNEL, LOC +
                QString("SA4200Channel: %1 cmds: 0x%2 0x%3 0x%4")
                .arg(channel).arg(cmd[0], 0, 16)
                .arg(cmd[1], 0, 16)
                .arg(cmd[2], 0, 16));

        if (!avc1394_transaction_block(fwhandle, fw_opts.node, cmd, 3, 1))
        {
            VERBOSE(VB_IMPORTANT, "AVC transaction failed.");
            return false;
        }
    }

    return true;
}

bool FirewireChannel::OpenFirewire(void)
{
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

    // verify node looks like a stb
    if (!avc1394_check_subunit_type(fwhandle, fw_opts.node,
                                    AVC1394_SUBUNIT_TYPE_TUNER))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("node %1 is not subunit "
                "type tuner.").arg(fw_opts.node));
        CloseFirewire();
        return false;
    }

    if (!avc1394_check_subunit_type(fwhandle, fw_opts.node, 
                                    AVC1394_SUBUNIT_TYPE_PANEL))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + QString("node %1 is not subunit "
                "type panel.").arg(fw_opts.node));
        CloseFirewire();
        return false;
    }

    // check power, power on if off
    if (GetPowerState() == Off)
    {
        quadlet_t *response, cmd = STB_POWER_ON;
        VERBOSE(VB_IMPORTANT, LOC + QString("Powering on (cmd: 0x%1)")
                                            .arg(cmd, 0, 16));
        response = avc1394_transaction_block(fwhandle, fw_opts.node, 
                                             &cmd, 1, 1);
        if (response)
        {
            if (AVC1394_MASK_RESPONSE(response[0]) == AVC1394_RESPONSE_ACCEPTED)
            {
                VERBOSE(VB_IMPORTANT, LOC + QString("Power on cmd successful "
                                                    "(0x%1)")
                                                    .arg(response[0], 0, 16));
                sleep(1);
                if (GetPowerState() == Off)
                {
                    VERBOSE(VB_IMPORTANT, LOC + "STB is still off!?");
                    return false;
                }
                return true;
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC + QString("Power on cmd failed "
                                                    "(0x%1)")
                                                    .arg(response[0], 0, 16));
                return false;
            }
        }
        else
        {
            VERBOSE(VB_IMPORTANT, LOC + "Power on cmd failed (no response)");
            return false;
        }
    }
    return true;
}

void FirewireChannel::CloseFirewire(void)
{
    VERBOSE(VB_CHANNEL, LOC + "Releasing raw1394 handle");
    raw1394_destroy_handle(fwhandle);
}

FirewireChannel::PowerState FirewireChannel::GetPowerState(void)
{
    quadlet_t *response, cmd = STB_POWER_STATE;

    VERBOSE(VB_CHANNEL, LOC + QString("Requesting STB Power State (cmd: 0x%1)")
                                      .arg(STB_POWER_STATE, 0, 16));
    response = avc1394_transaction_block(fwhandle, fw_opts.node, &cmd, 1, 1);
    if (response)
    {
        if (AVC1394_MASK_RESPONSE(response[0]) == AVC1394_RESPONSE_IMPLEMENTED)
        {
            if ((response[0] & 0xFF) == AVC1394_CMD_OPERAND_POWER_ON)
            {
                VERBOSE(VB_CHANNEL, LOC + QString("STB Power State: ON (0x%1)")
                                                  .arg(response[0], 0, 16));
                return On;
            }
            else if ((response[0] & 0xFF) == AVC1394_CMD_OPERAND_POWER_OFF)
            {
                VERBOSE(VB_IMPORTANT, LOC + QString("STB Power State: OFF " 
                                                    "(0x%1)")
                                                    .arg(response[0], 0, 16));
                return Off;
            }
            else
            {
                VERBOSE(VB_CHANNEL, LOC + QString("STB Power State: "
                                                  "Unknown Response (0x%1)")
                                                  .arg(response[0], 0, 16));
                return Failed;
            }
        }
        else
        {
            VERBOSE(VB_CHANNEL, LOC + QString("STB Power State: Failed (0x%1)")
                                              .arg(response[0], 0, 16));
            return Failed;
        }
    }
        
    VERBOSE(VB_CHANNEL, LOC + "Failed to get STB Power State");
    return Failed;
}
