#ifndef DVBCAM_H
#define DVBCAM_H

// Qt
#include <QMap>
#include <QMutex>
#include <QRunnable>
#include <QString>
#include <QWaitCondition>

// MythTV
#include "dvbtypes.h"
#include "mpeg/mpegtables.h"

class ChannelBase;
class cCiHandler;
class MThread;
class DVBCam;

using pmt_list_t = QMap<const ChannelBase*, ProgramMapTable*>;

class DVBCam : public QRunnable
{
  public:
    explicit DVBCam(QString device);
    ~DVBCam() override;

    bool Start(void);
    bool Stop(void);
    bool IsRunning(void) const
    {
        QMutexLocker locker(&m_ciHandlerLock);
        return m_ciHandlerRunning;
    }

    void SetPMT(const ChannelBase *chan, const ProgramMapTable *pmt);
    void SetTimeOffset(double offset_in_seconds);

  private:
    void run(void) override; // QRunnable
    void HandleUserIO(void);
    void HandlePMT(void);
    void RemoveDuplicateServices(void);
    void SendPMT(const ProgramMapTable &pmt, uint cplm);

    QString         m_device;
    int             m_numslots         {0};

    mutable QMutex  m_ciHandlerLock;
    QWaitCondition  m_ciHandlerWait;
    bool            m_ciHandlerDoRun   {false};
    bool            m_ciHandlerRunning {false};
    cCiHandler     *m_ciHandler        {nullptr};
    MThread        *m_ciHandlerThread  {nullptr};

    QMutex          m_pmtLock;
    pmt_list_t      m_pmtList;
    pmt_list_t      m_pmtAddList;
    bool            m_havePmt          {false};
    bool            m_pmtSent          {false};
    bool            m_pmtUpdated       {false};
    bool            m_pmtAdded         {false};
};

#endif // DVBCAM_H

