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

using namespace std;

class FirewireChannel : public ChannelBase
{
 public:
    FirewireChannel(TVRec *parent);
    ~FirewireChannel(void);

    bool SetChannelByString(const QString &chan);
    bool Open();
    void Close();
    void SwitchToInput(const QString &inputname, const QString &chan);
    void SwitchToInput(int newcapchannel, bool setstarting)
                      { (void)newcapchannel; (void)setstarting; }
    void SetExternalChanger(void);

};

#endif
