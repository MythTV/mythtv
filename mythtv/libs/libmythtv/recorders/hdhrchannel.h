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
    HDHRChannel(TVRec *parent, const QString &device);
    ~HDHRChannel(void);

    bool Open(void);
    void Close(void);
    bool EnterPowerSavingMode(void);

    // Gets
    bool IsOpen(void) const;
    QString GetDevice(void) const { return _device_id; }
    virtual vector<DTVTunerType> GetTunerTypes(void) const
        { return _tuner_types; }
    virtual bool IsMaster(void) const;

    // Sets
    virtual bool SetChannelByString(const QString &channum);

    using DTVChannel::Tune;
    // ATSC/DVB scanning/tuning stuff
    bool Tune(const DTVMultiplex &tuning, QString inputname);

    // Virtual tuning
    bool Tune(const QString &freqid, int /*finetune*/);

  private:
    QString               _device_id;
    HDHRStreamHandler    *_stream_handler;
    vector<DTVTunerType>  _tuner_types;
};

#endif
