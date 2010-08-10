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

    bool IsTunable(const QString &input, const QString &channum) const
        { return true; }

    bool Open(void)     { return InitializeInputs(); }
    void Close(void)    { return; }

    // Sets
    bool SetChannelByString(const QString &chan)
           { m_curchannelname = chan; return true; }
    void SetExternalChanger(void) { return; }

    // Gets
    bool    IsOpen(void)             const { return true; }
    QString GetDevice(void)          const { return "/dev/dummy"; }
    QString GetCurrentInput(void)    const { return curinputname; }
    uint    GetCurrentSourceID(void) const { return 0; }


    // Commands
    bool SelectInput(const QString &inputname, const QString &chan, bool use_sm)
         { curinputname = inputname; m_curchannelname = chan; return true; }

  private:
    QString curinputname;
};

#endif
