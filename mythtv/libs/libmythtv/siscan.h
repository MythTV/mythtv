
#include <qobject.h>
#include <qstring.h>
#include "dvbchannel.h"
#include "sitypes.h"

typedef QValueList<Event> QList_Events;
typedef QValueList<QList_Events*> QListList_Events;

class TransportScanList
{
public:
    TransportScanList()
    {
        mplexid = -1;
        complete = false;
        scanning = false;
    }
    TransportScanList(int _mplexid)
    {
         mplexid = _mplexid;
         complete = false;
         scanning = false;
    }
    int mplexid;
    bool complete;
    bool scanning;

};

class SIScan : public QObject
{
Q_OBJECT

public:

    SIScan(DVBChannel* advbchannel,QSqlDatabase *thedb, pthread_mutex_t* _db_lock, int _sourceID);
    ~SIScan();

    void StartScanner();
    void StopScanner();

    bool ScanTransports();
    bool ScanServices(int TransportID = -1);
    bool ScanServicesSourceID(int SourceID);
    bool FillEvents(int SourceID);
    void SetSourceID(int _SourceID);

    void SetFTAOnly(bool _fFTAOnly) { FTAOnly = _fFTAOnly;}
    void SetForceUpdate(bool _force) { forceUpdate = _force;}

public slots:

    // Handler for complete tables
    void TransportTableComplete();
    void ServiceTableComplete();
    void EventsReady(QMap_Events* EventList);

signals:
    // Values from 1-100 of scan completion
    void PctServiceScanComplete(int pct);
    void PctTransportScanComplete(int pct);
    void ServiceScanUpdateText(const QString& status);
    void TransportScanUpdateText(const QString& status);
    void ServiceScanComplete();
    void TransportScanComplete();

private:

    static void *ServiceScanThreadHelper(void*);
    static void *TransportScanThreadHelper(void*);

    void UpdateTransportsInDB(NITObject NIT);
    void CheckNIT(NITObject& NIT);

    void UpdateServicesInDB(QMap_SDTObject SDT);
    int  GenerateNewChanID();
    int  GetDVBTID(uint16_t NetworkID,uint16_t TransportID,int CurrentMplexID);
    void AddEvents();

    bool scannerRunning;
    bool serviceListReady;
    bool sourceIDTransportScan;
    bool sourceIDTransportTuned;
    bool transportListReady;
    bool eventsReady;
    int  transportsToScan;
    int  transportsCount;

    QListList_Events Events;
    QMap_SDTObject SDT;

    NITObject NIT;
    QSqlDatabase *db;
    pthread_mutex_t* db_lock;
    DVBChannel *chan;
    int sourceID;
    QValueList<TransportScanList> scanTransports;

    bool fillingEvents;

    bool threadExit;
    bool FTAOnly;
    bool forceUpdate;
    pthread_mutex_t events_lock;
};

#define SISCAN(args...) \
    VERBOSE(VB_GENERAL, QString("SIScan#%1: ").arg(chan->GetCardNum()) << args);
