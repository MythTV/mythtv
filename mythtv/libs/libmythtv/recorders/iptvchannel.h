/** -*- Mode: c++ -*-
 *  IPTVChannel
 *  Copyright (c) 2006-2009 Silicondust Engineering Ltd, and
 *                          Daniel Thor Kristjansson
 *  Copyright (c) 2012 Digital Nirvana, Inc.
 *  Copyright (c) 2013 Bubblestuff Pty Ltd
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef IPTV_CHANNEL_H
#define IPTV_CHANNEL_H

// Qt headers
#include <QMutex>

// MythTV headers
#include "dtvchannel.h"
#include "iptvstreamhandler.h"

class IPTVTuningData;
class IPTVRecorder;
class MPEGStreamData;

class IPTVChannel : QObject, public DTVChannel
{
    Q_OBJECT
    friend class IPTVRecorder;

  public:
    IPTVChannel(TVRec *rec, QString videodev);
    ~IPTVChannel() override;

    // Commands
    bool Open(void) override; // ChannelBase

    using DTVChannel::Tune;
    bool Tune(const IPTVTuningData &tuning, bool scanning) override ; // DTVChannel
    bool Tune(const DTVMultiplex &/*tuning*/) override // DTVChannel
        { return false; }

    // Sets
    void SetStreamData(MPEGStreamData *sd);

    // Gets
    bool IsOpen(void) const override; // ChannelBase
    QString GetDevice(void) const override // ChannelBase
        { return m_lastTuning.GetDeviceKey(); }
    IPTVStreamHandler *GetStreamHandler(void) const { return m_streamHandler; }
    bool IsIPTV(void) const override { return true; } // DTVChannel
    bool IsPIDTuningSupported(void) const  override // DTVChannel
        { return true; }

  protected:
    void Close(void) override; // ChannelBase
    bool EnterPowerSavingMode(void) override; // DTVChannel
    bool IsExternalChannelChangeSupported(void) override // ChannelBase
        { return true; }

  private:
    void OpenStreamHandler(void);
    void CloseStreamHandler(void);

  private:
    mutable QMutex     m_tuneLock;
    volatile bool      m_firstTune      {true};
    IPTVTuningData     m_lastTuning;
    mutable QMutex     m_streamLock;
    IPTVStreamHandler *m_streamHandler  {nullptr};
    MPEGStreamData    *m_streamData     {nullptr};
    QString            m_videoDev;
};

#endif // IPTV_CHANNEL_H

/* vim: set expandtab tabstop=4 shiftwidth=4: */
