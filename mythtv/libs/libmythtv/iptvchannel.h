/** -*- Mode: c++ -*-
 *  IPTVChannel
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _IPTV_CHANNEL_H_
#define _IPTV_CHANNEL_H_

#include <QString>
#include <QMutex>

#include "dtvchannel.h"
#include "iptvchannelinfo.h"

class IPTVFeederWrapper;

class IPTVChannel : public DTVChannel
{
    friend class IPTVSignalMonitor;
    friend class IPTVRecorder;

  public:
    IPTVChannel(TVRec *parent, const QString &videodev);
    ~IPTVChannel();

    // Commands
    bool Open(void);
    void Close(void);
    bool SetChannelByString(const QString &channum);

    // Gets
    bool IsOpen(void) const;

    // Channel scanning stuff
    using DTVChannel::Tune;
    bool Tune(const DTVMultiplex&, QString) { return true; }

  private:
    IPTVChannelInfo GetCurrentChanInfo(void) const
        { return GetChanInfo(m_curchannelname); }

    IPTVFeederWrapper       *GetFeeder(void)       { return m_feeder; }
    const IPTVFeederWrapper *GetFeeder(void) const { return m_feeder; }

    IPTVChannelInfo GetChanInfo(
        const QString &channum, uint sourceid = 0) const;

  private:
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
