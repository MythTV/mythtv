// -*- Mode: c++ -*-

#ifndef V4L2encSignalMonitor_H
#define V4L2encSignalMonitor_H

#include <QMap>

#include "dtvsignalmonitor.h"
#include "v4lchannel.h"
#include "v4l2util.h"

class V4L2encStreamHandler;

typedef QMap<uint,int> FilterMap;

class V4L2encSignalMonitor: public DTVSignalMonitor
{
  public:
    V4L2encSignalMonitor(int db_cardnum, V4LChannel *_channel,
                      uint64_t _flags = 0);
    virtual ~V4L2encSignalMonitor();

    void Stop(void);

  protected:
    V4L2encSignalMonitor(void);
    V4L2encSignalMonitor(const V4L2encSignalMonitor &);

    virtual void UpdateValues(void);
    V4LChannel *GetV4L2encChannel(void)
        { return dynamic_cast<V4LChannel*>(channel); }

    bool HasLock(void);
    int  StableResolution(void);

  protected:
    V4L2util           m_v4l2;
    V4L2encStreamHandler *m_stream_handler;
    bool               m_isTS;

  private:
    int      m_strength;
    int      m_stable_time;
    int      m_width;
    int      m_height;
    uint     m_lock_cnt;
    MythTimer m_timer;
    QDateTime m_status_time;
};

#endif // V4L2encSIGNALMONITOR_H
