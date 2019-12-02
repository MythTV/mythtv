/** -*- Mode: c++ -*-
 *  HDHRChannel
 *  Copyright (c) 2006-2009 by Silicondust Engineering Ltd.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef HDHOMERUNCHANNEL_H
#define HDHOMERUNCHANNEL_H

// Qt headers
#include <QString>

// MythTV headers
#include "dtvchannel.h"

class HDHRChannel;
class HDHRStreamHandler;
class ProgramMapTable;

class HDHRChannel : public DTVChannel
{
    friend class HDHRSignalMonitor;
    friend class HDHRRecorder;

  public:
    HDHRChannel(TVRec *parent, QString device);
    ~HDHRChannel(void);

    bool Open(void) override; // ChannelBase
    void Close(void) override; // ChannelBase
    bool EnterPowerSavingMode(void) override; // DTVChannel

    // Gets
    bool IsOpen(void) const override; // ChannelBase
    QString GetDevice(void) const override // ChannelBase
        { return m_device_id; }
    vector<DTVTunerType> GetTunerTypes(void) const override // DTVChannel
        { return m_tuner_types; }
    bool IsMaster(void) const override; // DTVChannel

    // Sets
    bool SetChannelByString(const QString &channum) override; // DTVChannel

    using DTVChannel::Tune;
    // ATSC/DVB scanning/tuning stuff
    bool Tune(const DTVMultiplex &tuning) override; // DTVChannel

    // Virtual tuning
    bool Tune(const QString &freqid, int /*finetune*/) override; // DTVChannel

  private:
    QString               m_device_id;
    HDHRStreamHandler    *m_stream_handler {nullptr};
    vector<DTVTunerType>  m_tuner_types;
};

#endif
