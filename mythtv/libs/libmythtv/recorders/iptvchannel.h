/** -*- Mode: c++ -*-
 *  IPTVChannel
 *  Copyright (c) 2006-2009 Silicondust Engineering Ltd, and
 *                          Daniel Thor Kristjansson
 *  Copyright (c) 2012 Digital Nirvana, Inc.
 *  Copyright (c) 2013 Bubblestuff Pty Ltd
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _IPTV_CHANNEL_H_
#define _IPTV_CHANNEL_H_

// Qt headers
#include <QMutex>

// MythTV headers
#include "dtvchannel.h"

class IPTVStreamHandler;
class IPTVTuningData;
class IPTVRecorder;
class MPEGStreamData;

class IPTVChannel : QObject, public DTVChannel
{
    Q_OBJECT

  public:
    IPTVChannel(TVRec*, const QString&);
    ~IPTVChannel();

    // Commands
    virtual bool Open(void);
    virtual void Close(void);

    using DTVChannel::Tune;
    virtual bool Tune(const IPTVTuningData&);
    virtual bool Tune(const DTVMultiplex&, QString) { return false; }

    // Sets
    void SetStreamData(MPEGStreamData*);

    // Gets
    bool IsOpen(void) const;
    virtual bool IsIPTV(void) const { return true; } // DTVChannel

  private:
    void SetStreamDataInternal(MPEGStreamData*, bool closeimmediately);
    void timerEvent(QTimerEvent*);
    void KillTimer(void);
    void OpenStreamHandler(void);
    void CloseStreamHandler(void);

  private:
    mutable QMutex m_lock;
    volatile bool m_open, m_firsttune;
    IPTVTuningData m_last_tuning;
    IPTVStreamHandler *m_stream_handler;
    MPEGStreamData *m_stream_data;
    int m_timer;
};

#endif // _IPTV_CHANNEL_H_

/* vim: set expandtab tabstop=4 shiftwidth=4: */
