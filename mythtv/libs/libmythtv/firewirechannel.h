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
