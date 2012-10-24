/** -*- Mode: c++ -*-
 *  CetonChannel
 *  Copyright (c) 2011 Ronald Frazier
 *  Copyright (c) 2006-2009 by Silicondust Engineering Ltd.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef CETONCHANNEL_H
#define CETONCHANNEL_H

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
    CetonChannel(TVRec *parent, const QString &device);
    ~CetonChannel(void);

    bool Open(void);
    void Close(void);
    bool EnterPowerSavingMode(void);

    // Gets
    bool IsOpen(void) const;
    QString GetDevice(void) const { return _device_id; }
    virtual vector<DTVTunerType> GetTunerTypes(void) const
        { return _tuner_types; }

    // Sets
    virtual bool SetChannelByString(const QString &channum);

    using DTVChannel::Tune;
    // ATSC/DVB scanning/tuning stuff
    bool Tune(const DTVMultiplex &tuning, QString inputname);

    // Virtual tuning
    bool Tune(const QString &freqid, int /*finetune*/);

  private:
    QString               _device_id;
    CetonStreamHandler   *_stream_handler;
    vector<DTVTunerType>  _tuner_types;
};

#endif
