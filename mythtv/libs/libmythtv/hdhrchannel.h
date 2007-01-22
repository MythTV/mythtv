/** -*- Mode: c++ -*-
 *  Dbox2Channel
 *  Copyright (c) 2006 by Silicondust Engineering Ltd.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef HDHOMERUNCHANNEL_H
#define HDHOMERUNCHANNEL_H

// Qt headers
#include <qstring.h>

// MythTV headers
#include "dtvchannel.h"

// HDHomeRun headers
#include "hdhomerun/hdhomerun.h"

typedef struct hdhomerun_control_sock_t hdhr_socket_t;

class HDHRChannel : public DTVChannel
{
    friend class HDHRSignalMonitor;
    friend class HDHRRecorder;

  public:
    HDHRChannel(TVRec *parent, const QString &device, uint tuner);
    ~HDHRChannel(void);

    bool Open(void);
    void Close(void);
    bool EnterPowerSavingMode(void);

    // Sets
    bool SetChannelByString(const QString &chan);

    // Gets
    bool IsOpen(void) const { return (_control_socket != NULL); }
    QString GetDevice(void) const
        { return QString("%1/%2").arg(_device_id, 8, 16).arg(_tuner); }
    vector<uint> GetPIDs(void) const
        { QMutexLocker locker(&_lock); return _pids; }
    QString GetSIStandard(void) const { return "atsc"; }

    // Commands
    bool SwitchToInput(const QString &inputname, const QString &chan);
    bool SwitchToInput(int newcapchannel, bool setstarting);
    bool AddPID(uint pid, bool do_update = true);
    bool DelPID(uint pid, bool do_update = true);
    bool DelAllPIDs(void);
    bool UpdateFilters(void);

    // ATSC scanning stuff
    bool TuneMultiplex(uint mplexid, QString inputname);
    bool Tune(const DTVMultiplex &tuning, QString inputname);

  private:
    bool FindDevice(void);
    bool Connect(void);
    bool Tune(uint frequency, QString inputname,
              QString modulation, QString si_std);

    bool DeviceSetTarget(unsigned short localPort);
    bool DeviceClearTarget(void);

    QString DeviceGet(const QString &name, bool report_error_return = true);
    QString DeviceSet(const QString &name, const QString &value,
                      bool report_error_return = true);

    QString TunerGet(const QString &name, bool report_error_return = true);
    QString TunerSet(const QString &name, const QString &value,
                     bool report_error_return = true);

  private:
    hdhr_socket_t  *_control_socket;
    uint            _device_id;
    uint            _device_ip;
    uint            _tuner;
    bool            _ignore_filters;
    vector<uint>    _pids;
    mutable QMutex  _lock;
};

#endif
