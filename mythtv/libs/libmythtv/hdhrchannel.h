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

    bool Init(QString &inputname, QString &startchannel, bool setchan);

    // Sets
    bool SetChannelByString(const QString &chan);

    // Gets
    bool IsOpen(void) const;
    QString GetDevice(void) const { return _device_id; }
    virtual vector<DTVTunerType> GetTunerTypes(void) const { return _tuner_types; }

    // ATSC/DVB scanning/tuning stuff
    bool TuneMultiplex(uint mplexid, QString inputname);
    bool Tune(const DTVMultiplex &tuning, QString inputname);

  private:
    bool Tune(uint frequency, QString inputname,
              QString modulation, QString si_std);

  private:
    QString               _device_id;
    HDHRStreamHandler    *_stream_handler;
    vector<DTVTunerType>  _tuner_types;

    mutable QMutex  _lock;
    mutable QMutex  tune_lock;
    mutable QMutex  hw_lock;
};

#endif
