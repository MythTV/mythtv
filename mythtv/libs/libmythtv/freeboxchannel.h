/** -*- Mode: c++ -*-
 *  FreeboxChannel
 *  Copyright (c) 2006 by Laurent Arnal, Benjamin Lerman & Mickaël Remars
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef FREEBOXCHANNEL_H
#define FREEBOXCHANNEL_H

#include "channelbase.h"
#include "freeboxchannelinfo.h"

#include <qmutex.h>

class FreeboxRecorder;

class FreeboxChannel : public ChannelBase
{
    friend class FreeboxRecorderImpl;
  public:
    FreeboxChannel(TVRec *parent, const QString &videodev);
    ~FreeboxChannel(void) { }

    bool Open(void);
    void Close(void);

    bool IsOpen(void) const;

    bool SwitchToInput(const QString &inputname, const QString &channum);
    bool SwitchToInput(int inputNum, bool setstarting);
    bool SetChannelByString(const QString &channum);

    void SetRecorder(FreeboxRecorder *rec);

  private:
    FreeboxChannelInfo GetChanInfo(const QString& channum,
                                   uint           sourceid = 0) const;
    FreeboxChannelInfo GetCurrentChanInfo(void) const
        { return GetChanInfo(curchannelname); }

    QString               m_videodev;
    FreeboxRecorder      *m_recorder;
    fbox_chan_map_t       m_freeboxchannels;
    mutable QMutex        m_lock;

  private:
    FreeboxChannel& operator=(const FreeboxChannel&); //< avoid default impl
    FreeboxChannel(const FreeboxChannel&);            //< avoid default impl
    FreeboxChannel();                                 //< avoid default impl
};

#endif // FREEBOXCHANNEL_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
