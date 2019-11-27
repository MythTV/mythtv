// -*- Mode: c++ -*-

#ifndef V4L2encSignalMonitor_H
#define V4L2encSignalMonitor_H

#include <QMap>

#include "dtvsignalmonitor.h"
#include "v4lchannel.h"
#include "v4l2util.h"

class V4L2encStreamHandler;

using FilterMap = QMap<uint,int>;

class V4L2encSignalMonitor: public DTVSignalMonitor
{
  public:
    V4L2encSignalMonitor(int db_cardnum, V4LChannel *_channel,
                         bool _release_stream, uint64_t _flags = 0);
    virtual ~V4L2encSignalMonitor();

    void Stop(void) override; // SignalMonitor

  protected:
    V4L2encSignalMonitor(void);
    V4L2encSignalMonitor(const V4L2encSignalMonitor &);

    void UpdateValues(void) override; // SignalMonitor
    V4LChannel *GetV4L2encChannel(void)
        { return dynamic_cast<V4LChannel*>(m_channel); }

    bool HasLock(void);
    int  StableResolution(void);

  protected:
    V4L2util              m_v4l2;
    V4L2encStreamHandler *m_stream_handler {nullptr};
    bool                  m_isTS           {false};

  private:
    int                   m_strength       {0};
    int                   m_stable_time    {1500};
    int                   m_width          {0};
    int                   m_height         {0};
    uint                  m_lock_cnt       {0};
    MythTimer             m_timer;
    QDateTime             m_status_time;
};

#endif // V4L2encSIGNALMONITOR_H
