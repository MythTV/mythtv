/**
 *  Dbox2Channel
 *  Copyright (c) 2006 by Silicondust Engineering Ltd.
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef HDHOMERUNCHANNEL_H
#define HDHOMERUNCHANNEL_H

#include <qstring.h>

#include "mythcontext.h"
#include "mythdbcon.h"
#include "channelbase.h"
#include "hdhrsignalmonitor.h"

#include "hdhomerun/hdhomerun_pkt.h"
#include "hdhomerun/hdhomerun_discover.h"
#include "hdhomerun/hdhomerun_control.h"
#include "hdhomerun/hdhomerun_video.h"

typedef struct hdhomerun_control_sock_t hdhr_socket_t;

class HDHRChannel : public QObject, public ChannelBase
{
    friend class HDHRSignalMonitor;
    Q_OBJECT

  public:
    HDHRChannel(TVRec *parent, const QString &device, uint tuner);
    ~HDHRChannel(void);

    bool Open(void);
    void Close(void);
    bool IsOpen(void) const { return (_control_socket != NULL); }

    bool SetChannelByString(const QString &chan);
    bool SwitchToInput(const QString &inputname, const QString &chan);
    bool SwitchToInput(int newcapchannel, bool setstarting);

    bool DeviceSetTarget(unsigned short localPort);
    bool DeviceClearTarget(void);
    bool DeviceSetChannel(int chanNum);

  private:
    bool FindDevice(void);
    bool Connect(void);

    QString DeviceGet(const QString &name);
    QString DeviceSet(const QString &name, const QString &value);

    QString TunerGet(const QString &name);
    QString TunerSet(const QString &name, const QString &value);

  private:
    hdhr_socket_t *_control_socket;
    uint           _device_id;
    uint           _device_ip;
    uint           _tuner;
    mutable QMutex _lock;
};

#endif
