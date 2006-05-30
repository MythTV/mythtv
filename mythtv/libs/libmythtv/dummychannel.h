/**
 *  DummyChannel
 */


#ifndef DUMMYCHANNEL_H
#define DUMMYCHANNEL_H

#include <qstring.h>
#include "tv_rec.h"
#include "channelbase.h"

using namespace std;

class DummyChannel : public ChannelBase
{
  public:
    DummyChannel(TVRec *parent): ChannelBase(parent)
        { (void)parent; curchannelname = ""; curinputname = ""; return; }
    ~DummyChannel(void) { return; }

    bool Open(void) { return InitializeInputs(); }
    void Close(void) { return; }

    // Sets
    bool SetChannelByString(const QString &chan)
           { curchannelname = chan; return true; }
    void SetExternalChanger(void) { return; }
    QString GetCurrentInput(void) const { return curinputname; }

    // Gets
    bool IsOpen(void) const { return true; }
    QString GetDevice(void) const { return "/dev/dummy"; }

    // Commands
    bool SwitchToInput(const QString &inputname, const QString &chan)
         { curinputname = inputname; curchannelname = chan; return true; }
    bool SwitchToInput(int newcapchannel, bool setstarting)
         { currentInputID = newcapchannel; (void)setstarting; return true; }

  private:
    QString curinputname;
};

#endif
