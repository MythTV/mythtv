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

class IPTVStreamHandler;
class IPTVRecorder;

class IPTVChannel : public DTVChannel
{
  public:
    IPTVChannel(TVRec*, const QString&);
    ~IPTVChannel();

    // Commands
    virtual bool Open(void);
    virtual void Close(void); 
    virtual bool Tune(const QString &freqid, int finetune);
    virtual bool Tune(const DTVMultiplex&, QString) { return false; }

    // Sets
    void SetRecorder(IPTVRecorder*);

    // Gets
    bool IsOpen(void) const;

  private:
    mutable QMutex m_lock;
    volatile bool m_open;
    QString m_last_channel_id;
    IPTVStreamHandler *m_stream_handler;
    IPTVRecorder *m_recorder;
};

#endif // _IPTV_CHANNEL_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
