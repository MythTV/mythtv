
#include <qobject.h>
#include <qstring.h>
#include "dvbchannel.h"
#include "sitypes.h"
#include "dvbtypes.h"

typedef QValueList<Event> QList_Events;
typedef QValueList<QList_Events*> QListList_Events;

typedef enum { IDLE, TRANSPORT_LIST, EIT_CRAWL } SCANMODE;

/* Class used for doing a list of frequencies / transports 
   This is used for ATSC/NA Digital Cable and also scan all transports */
class TransportScanList
{
public:
    enum {DVBT_TUNINGTIMEOUT=1000, ATSC_TUNINGTIMEOUT=2000};

    TransportScanList()
    {
        mplexid = -1;
        complete = false;
        scanning = false;
        FriendlyName = "";
        offset1 = 0;
        offset2 = 0;
        timeoutTune = ATSC_TUNINGTIMEOUT; 
    }

    TransportScanList(int _mplexid)
    {
        mplexid = _mplexid;
        complete = false;
        scanning = false;
        FriendlyName = "";
        offset1 = 0;
        offset2 = 0;
        timeoutTune = ATSC_TUNINGTIMEOUT; 
    }

    int mplexid;                /* DB Mplexid */
    bool complete;              /* scan status */
    QString standard;           /* DVB/ATSC */
    DVBTuning tuning;        /* DVB Tuning struct if mplexid == -1 */

    QString FriendlyName;       /* Name to display in scanner dialog */
    int SourceID;               /* Associated SourceID */
    bool UseTimer;              /* Set if timer is used after lock for getting PAT */

    bool scanning;              /* Probbably Unnecessary */
    int offset1;                /* First frequency offset */
    int offset2;                /* Second frequency offset */
    unsigned timeoutTune;      /* Timeout to tune to a frequency*/
};

class SIScan : public QObject
{
Q_OBJECT

public:

    SIScan(DVBChannel* advbchannel, int _sourceID);
    ~SIScan();

    void StartScanner();
    void StopScanner();

    bool ATSCScanTransport(int SourceID, int FrequencyBand);
    bool DVBTScanTransport(int SourceID, unsigned country);
    bool ScanTransports();
    bool ScanServices(int TransportID = -1);
    bool ScanServicesSourceID(int SourceID);
    void SetSourceID(int _SourceID);

    void SetFTAOnly(bool _fFTAOnly) { FTAOnly = _fFTAOnly;}
    void SetForceUpdate(bool _force) { forceUpdate = _force;}
    void SetScanTimeout(int _timeout) { ScanTimeout = _timeout;}

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

    void verifyTransport(TransportScanList& t);

    int CreateMultiplex(const fe_type_t cardType,const TransportScanList& a,
                    const DVBTuning& tuning);

    void UpdateTransportsInDB(NITObject NIT);
    void CheckNIT(NITObject& NIT);

    void UpdateServicesInDB(QMap_SDTObject SDT);
    int  GenerateNewChanID(int SourceID);
    int  GetDVBTID(uint16_t NetworkID,uint16_t TransportID,int CurrentMplexID);
    void AddEvents();

    bool scannerRunning;                 
    bool serviceListReady;
    bool sourceIDTransportTuned;
    bool transportListReady;
    bool eventsReady;
    int  transportsToScan;
    int  transportsCount;

    QListList_Events Events;
    QMap_SDTObject SDT;

    NITObject NIT;
    DVBChannel *chan;
    int sourceID;
    QValueList<TransportScanList> scanTransports;

    QTime timer;

    bool threadExit;
    bool FTAOnly;
    bool forceUpdate;
    int ScanTimeout;
    SCANMODE scanMode;
    pthread_mutex_t events_lock;
};

#define SISCAN(args...) \
    VERBOSE(VB_SIPARSER, QString("SIScan#%1: ").arg(chan->GetCardNum()) << args);
