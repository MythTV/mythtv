/**
 *  DummyChannel
 */


#ifndef DUMMYCHANNEL_H
#define DUMMYCHANNEL_H

#include <QString>

#include "tv_rec.h"
#include "channelbase.h"

using namespace std;

class DummyChannel : public ChannelBase
{
  public:
    DummyChannel(TVRec *parent): ChannelBase(parent)
        { (void)parent; m_curchannelname.clear(); curinputname.clear(); return; }
    ~DummyChannel(void) { return; }

    virtual bool IsTunable(const QString &channum) const
        { return true; }

    virtual bool Open(void)     { return InitializeInput(); }
    virtual void Close(void)    { return; }

    // Sets
    virtual bool SetChannelByString(const QString &chan)
        { m_curchannelname = chan; return true; }

    // Gets
    virtual bool    IsOpen(void)             const { return true; }
    virtual QString GetDevice(void)          const { return "/dev/dummy"; }
    virtual QString GetCurrentInput(void)    const { return curinputname; }
    virtual uint    GetCurrentSourceID(void) const { return 0; }

  private:
    QString curinputname;
};

#endif
