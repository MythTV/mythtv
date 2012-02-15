/** -*- Mode: c++ -*-
 *  IPTVChannel
 *  Copyright (c) 2006-2009 Silicondust Engineering Ltd, and
 *                          Daniel Thor Kristjansson
 *  Copyright (c) 2012 Digital Nirvana, Inc.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _IPTV_CHANNEL_H_
#define _IPTV_CHANNEL_H_

// Qt headers
#include <QMutex>

// MythTV headers
#include "dtvchannel.h"

class IPTVChannel : public DTVChannel
{
  public:
    IPTVChannel(TVRec*);
    ~IPTVChannel();

    // Commands
    bool Open(void);
    void Close(void);
    bool Tune(const QString &freqid, int finetune);

    // Gets
    bool IsOpen(void) const;

  private:
    mutable QMutex m_lock;
    volatile bool m_open;
};

#endif // _IPTV_CHANNEL_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
