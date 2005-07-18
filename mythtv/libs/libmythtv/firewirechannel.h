/**
 *  FirewireChannel
 *  Copyright (c) 2005 by Jim Westfall
 *  Distributed as part of MythTV under GPL v2 and later.
 */


#ifndef FIREWIRECHANNEL_H
#define FIREWIRECHANNEL_H

#include <qstring.h>
#include "tv_rec.h"
#include "channelbase.h"
#include <libavc1394/avc1394.h>

using namespace std;

// 6200 defines for channel changes, taken from contrib/6200ch.c
#define AVC1394_SUBUNIT_TYPE_6200 	(9 << 19)  /* uses a reserved subunit type */ 
#define AVC1394_6200_COMMAND_CHANNEL 	0x00007C00 /* 6200 subunit command */
#define AVC1394_6200_OPERAND_SET 	0x00000020 /* 6200 subunit command operand */

#define DCT6200_CMD0 AVC1394_CTYPE_CONTROL | AVC1394_SUBUNIT_TYPE_6200 | \
        AVC1394_SUBUNIT_ID_0 | AVC1394_6200_COMMAND_CHANNEL | \
        AVC1394_6200_OPERAND_SET


class FirewireChannel : public ChannelBase
{
 public:
    FirewireChannel(firewire_options_t firewire_opts, TVRec *parent);
    ~FirewireChannel(void);

    bool Open(void);
    void Close(void);

    // Sets
    bool SetChannelByString(const QString &chan);
    void SetExternalChanger(void);    

    // Gets
    bool IsOpen(void) const { return isopen; }
    QString GetDevice(void) const
        { return QString("%1:%2").arg(fw_opts.port).arg(fw_opts.node); }

    // Commands
    void SwitchToInput(const QString &inputname, const QString &chan);
    void SwitchToInput(int newcapchannel, bool setstarting)
        { (void)newcapchannel; (void)setstarting; }

private:
    firewire_options_t fw_opts;
    nodeid_t           fwnode;
    raw1394handle_t    fwhandle;
    bool               isopen;
};

#endif
