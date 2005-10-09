/**
 *  FirewireChannel
 *  Copyright (c) 2005 by Jim Westfall
 *  SA3250HD support Copyright (c) 2005 by Matt Porter
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

// SA3250HD defines
#define AVC1394_SA3250_COMMAND_CHANNEL		0x000007c00
#define AVC1394_SA3250_OPERAND_KEY_PRESS	0xe7
#define AVC1394_SA3250_OPERAND_KEY_RELEASE	0x67

#define SA3250_CMD0 (AVC1394_CTYPE_CONTROL | AVC1394_SUBUNIT_TYPE_PANEL | \
                     AVC1394_SUBUNIT_ID_0 | AVC1394_SA3250_COMMAND_CHANNEL)
#define SA3250_CMD1 (0x04 << 24)
#define SA3250_CMD2 0xff000000

class FirewireChannel : public ChannelBase
{
  public:
    FirewireChannel(FireWireDBOptions firewire_opts, TVRec *parent);
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
    bool SwitchToInput(const QString &inputname, const QString &chan);
    bool SwitchToInput(int newcapchannel, bool setstarting)
        { (void)newcapchannel; (void)setstarting; return false; }

  private:
    FireWireDBOptions  fw_opts;
    nodeid_t           fwnode;
    raw1394handle_t    fwhandle;
    bool               isopen;
};

#endif
