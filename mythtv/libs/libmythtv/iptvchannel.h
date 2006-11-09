/** -*- Mode: c++ -*-
 *  IPTVChannel
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _IPTV_CHANNEL_H_
#define _IPTV_CHANNEL_H_

#include "dtvchannel.h"
#include "iptvchannelinfo.h"

#include <qmutex.h>

class IPTVFeederWrapper;

class IPTVChannel : public DTVChannel
{
  public:
    IPTVChannel(TVRec *parent, const QString &videodev);
    ~IPTVChannel();

    bool Open(void);
    void Close(void);

    bool IsOpen(void) const;

    bool SwitchToInput(const QString &inputname, const QString &channum);
    bool SwitchToInput(int inputNum, bool setstarting);
    bool SetChannelByString(const QString &channum);
    bool TuneMultiplex(uint /*mplexid*/, QString /*sourceid*/)
        { return false; } // TODO
    bool Tune(const DTVMultiplex &/*tuning*/, QString /*inputname*/)
        { return false; } // TODO

    IPTVChannelInfo GetCurrentChanInfo(void) const
        { return GetChanInfo(curchannelname); }

    IPTVFeederWrapper       *GetFeeder(void)       { return m_feeder; }
    const IPTVFeederWrapper *GetFeeder(void) const { return m_feeder; }
    
  private:
    IPTVChannelInfo GetChanInfo(
        const QString &channum, uint sourceid = 0) const;

    QString               m_videodev;
    fbox_chan_map_t       m_freeboxchannels;
    IPTVFeederWrapper    *m_feeder;
    mutable QMutex        m_lock;

  private:
    IPTVChannel &operator=(const IPTVChannel&); //< avoid default impl
    IPTVChannel(const IPTVChannel&);            //< avoid default impl
    IPTVChannel();                              //< avoid default impl
};

#endif // _IPTV_CHANNEL_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
