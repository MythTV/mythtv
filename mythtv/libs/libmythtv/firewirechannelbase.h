/**
 *  FirewireChannelBase
 *  Copyright (c) 2005 by Jim Westfall and Dave Abrahams
 *  Distributed as part of MythTV under GPL v2 and later.
 */


#ifndef LIBMYTHTV_FIREWIRECHANNELBASE_H
#define LIBMYTHTV_FIREWIRECHANNELBASE_H

#include <qstring.h>
#include "tv_rec.h"
#include "channelbase.h"

#include "mythconfig.h"

namespace AVS
{
  class AVCDeviceController;
  class AVCDevice;
}

class FirewireChannelBase : public ChannelBase
{
  public:
    FirewireChannelBase(TVRec *parent)   
        : ChannelBase(parent), isopen(false) { } 
    ~FirewireChannelBase() { Close(); }

    bool Open(void);
    void Close(void);

    // Sets
    bool SetChannelByString(const QString &chan);
    virtual bool SetChannelByNumber(int channel) = 0;

    // Gets
    bool IsOpen(void) const { return isopen; }

    // Commands
    bool SwitchToInput(const QString &inputname, const QString &chan);
    bool SwitchToInput(int newcapchannel, bool setstarting)
        { (void)newcapchannel; (void)setstarting; return false; }

  private:
    virtual bool OpenFirewire() = 0;
    virtual void CloseFirewire() = 0;

  protected:
    bool isopen;
};

#endif
