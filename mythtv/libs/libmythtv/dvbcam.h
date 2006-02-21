#ifndef DVBCAM_H
#define DVBCAM_H

#include <deque>
using namespace std;

#include <qobject.h>
#include <qmutex.h>

#include <dvbci.h>
#include "dvbtypes.h"

typedef deque<PMTObject> pmt_list_t;

class DVBCam: public QObject
{
    Q_OBJECT
public:
    DVBCam(int cardnum);
    ~DVBCam();

    bool Start();
    bool Stop();
    bool IsRunning() const { return ciThreadRunning; }
    void SetPMT(const PMTObject *pmt);
    void AddPMT(const PMTObject *pmt);

private:
    static void *CiHandlerThreadHelper(void*);
    void CiHandlerLoop(void);
    void HandleUserIO(void);
    void HandlePMT(void);

    void SendPMT(const PMTObject &pmt, uint cplm);

    int             cardnum;
    int             numslots;
    cCiHandler     *ciHandler;

    bool            exitCiThread;
    bool            ciThreadRunning;

    pthread_t       ciHandlerThread;

    pmt_list_t      PMTList;
    pmt_list_t      PMTAddList;
    QMutex          pmt_lock;
    bool            have_pmt;
    bool            pmt_sent;
    bool            pmt_updated;
    bool            pmt_added;
};

#endif // DVBCAM_H

