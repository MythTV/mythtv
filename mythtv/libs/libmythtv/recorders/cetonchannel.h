/** -*- Mode: c++ -*-
 *  CetonChannel
 *  Copyright (c) 2011 Ronald Frazier
 *  Copyright (c) 2006-2009 by Silicondust Engineering Ltd.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef CETONCHANNEL_H
#define CETONCHANNEL_H

#include <utility>

// Qt headers
#include <QString>

// MythTV headers
#include "dtvchannel.h"

class CetonChannel;
class CetonStreamHandler;

class CetonChannel : public DTVChannel
{
    friend class CetonSignalMonitor;
    friend class CetonRecorder;

  public:
    CetonChannel(TVRec *parent, QString device)
        : DTVChannel(parent), m_deviceId(std::move(device)) {}
    ~CetonChannel(void) override;

    bool Open(void) override; // ChannelBase
    void Close(void) override; // ChannelBase
    bool EnterPowerSavingMode(void) override; // DTVChannel

    // Gets
    bool IsOpen(void) const override; // ChannelBase
    QString GetDevice(void) const override // ChannelBase
        { return m_deviceId; }
    std::vector<DTVTunerType> GetTunerTypes(void) const override // DTVChannel
        { return m_tunerTypes; }

    // Sets
    bool SetChannelByString(const QString &channum) override; // ChannelBase

    using DTVChannel::Tune;
    // ATSC/DVB scanning/tuning stuff
    bool Tune(const DTVMultiplex &tuning) override; // DTVChannel

    // Virtual tuning
    bool Tune(const QString &freqid, int /*finetune*/) override; // ChannelBase

  private:
    QString               m_deviceId;
    CetonStreamHandler   *m_streamHandler {nullptr};
    std::vector<DTVTunerType>  m_tunerTypes;
};

#endif
