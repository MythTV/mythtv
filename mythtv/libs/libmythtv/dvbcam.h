
#ifndef DVBCAM_H
#define DVBCAM_H

#include <qobject.h>

#include <dvbci.h>
#include "dvbtypes.h"

class DVBCam: public QObject
{
    Q_OBJECT
public:
    DVBCam(int cardnum);
    ~DVBCam();

    bool Open();
    bool Close();

    static void *CiHandlerThreadHelper(void*self);
    void CiHandlerLoop();

public slots:
    void ChannelChanged(dvb_channel_t& chan, uint8_t* pmt, int len);

private:
    void SetPids(cCiCaPmt& capmt, dvb_pids_t& pid);

    int             cardnum;
    cCiHandler      *ciHandler;

    bool            exitCiThread;
    bool            ciThreadRunning;
    bool            noCardSupport;

    bool            setCamProgramMapTable;
    bool            first;
    uint8_t*        pmtbuf;
    int             pmtlen;
    pthread_t       ciHandlerThread;

    dvb_channel_t   chan_opts;
};

#endif // DVBCAM_H
