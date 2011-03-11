#ifndef DVBCAM_H
#define DVBCAM_H

#include <deque>
using namespace std;

#include <QString>
#include <QMutex>
#include <QThread>

#include "mpegtables.h"

#include "dvbtypes.h"

class ChannelBase;
class cCiHandler;
typedef QMap<const ChannelBase*, ProgramMapTable*> pmt_list_t;

class DVBCam;

class CiHandlerThread : public QThread
{
    Q_OBJECT
  public:
    CiHandlerThread() : m_parent(NULL) {}
    void SetParent(DVBCam *parent) { m_parent = parent; }
    void run(void);
  private:
    DVBCam *m_parent;
};

class DVBCam
{
    friend class CiHandlerThread;
  public:
    DVBCam(const QString &device);
    ~DVBCam();

    bool Start();
    bool Stop();
    bool IsRunning() const { return ciHandlerThread.isRunning(); }
    void SetPMT(const ChannelBase *chan, const ProgramMapTable *pmt);
    void SetTimeOffset(double offset_in_seconds);

  private:
    void CiHandlerLoop(void);
    void HandleUserIO(void);
    void HandlePMT(void);

    void SendPMT(const ProgramMapTable &pmt, uint cplm);

    QString         device;
    int             numslots;
    cCiHandler     *ciHandler;

    bool            exitCiThread;

    CiHandlerThread ciHandlerThread;

    pmt_list_t      PMTList;
    pmt_list_t      PMTAddList;
    QMutex          pmt_lock;
    bool            have_pmt;
    bool            pmt_sent;
    bool            pmt_updated;
    bool            pmt_added;
};

#endif // DVBCAM_H

