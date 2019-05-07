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
    explicit DummyChannel(TVRec *parent): ChannelBase(parent)
        { (void)parent; m_curchannelname.clear(); curinputname.clear(); return; }
    ~DummyChannel(void) { return; }

    bool IsTunable(const QString &/*channum*/) const override // ChannelBase
        { return true; }

    bool Open(void) override // ChannelBase
        { return InitializeInput(); }
    void Close(void) override // ChannelBase
        { return; }

    // Sets
    bool SetChannelByString(const QString &chan) override // ChannelBase
        { m_curchannelname = chan; return true; }

    // Gets
    bool    IsOpen(void)             const override // ChannelBase
        { return true; }
    QString GetDevice(void) const override // ChannelBase
        { return "/dev/dummy"; }
    QString GetCurrentInput(void)    const { return curinputname; }
    uint    GetCurrentSourceID(void) const { return 0; }

  private:
    QString curinputname;
};

#endif
