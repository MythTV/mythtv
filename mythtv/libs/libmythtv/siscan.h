// -*- Mode: c++ -*-

#ifndef SISCAN_H
#define SISCAN_H

// Qt includes
#include <qobject.h>
#include <qstring.h>
#include <qmap.h>
#include <qmutex.h>
#include <qvaluelist.h>
#include <qdatetime.h>

// MythTV includes
#include "frequencytables.h"

#ifdef USING_DVB
#include "sitypes.h"
#include "dvbtypes.h"
typedef QValueList<Event> QList_Events;
typedef QValueList<QList_Events*> QListList_Events;
#define USE_OWN_SIPARSER
#else
typedef unsigned short uint16_t;
typedef QMap<uint16_t, void*> QMap_Events;
typedef QMap<uint16_t, void*> QMap_SDTObject;
typedef QValueList<void*> QList_Events;
typedef QValueList<QList_Events> QListList_Events;
class DVBTuning;
typedef int fe_type_t;
typedef void* NITObject;
typedef void* NIT;
#endif // USING_DVB

#define SISCAN(args...) \
    VERBOSE(VB_SIPARSER, QString("SIScan(%1): ").arg(channel->GetDevice()) << args);

class MSqlQuery;
class ChannelBase;
class Channel;
class DVBChannel;
class SignalMonitor;
class DTVSignalMonitor;
class DVBSignalMonitor;
class ServiceDescriptionTable;
class NetworkInformationTable;
class VirtualChannelTable;
class MasterGuideTable;
class ScanStreamData;
class EITHelper;
class DVBSIParser;

typedef enum
{
    IDLE,           ///< Don't do anything
    TRANSPORT_LIST, ///< Actively scan for channels
    EIT_CRAWL       ///< Scan for event information
} SCANMODE;

class SIScan : public QObject
{
    Q_OBJECT
  public:
    SIScan(QString _cardtype, ChannelBase* _channel, int _sourceID);
    ~SIScan();

    void StartScanner(void);
    void StopScanner(void);

    bool ScanTransports(int SourceID,
                        const QString std,
                        const QString modulation,
                        const QString country);
    bool ScanTransports(void);
    bool ScanTransport(int mplexid);

    bool ScanServices(void);
    bool ScanServicesSourceID(int SourceID);

    void SetSourceID(int _SourceID)   { sourceID                = _SourceID; }
    void SetFTAOnly(bool _fFTAOnly)   { ignoreEncryptedServices = _fFTAOnly; }
    void SetTVOnly(bool _tvOnly)      { ignoreAudioOnlyServices = _tvOnly;   }
    void SetForceUpdate(bool _force)  { forceUpdate             = _force;    }
    void SetScanTimeout(int _timeout) { scanTimeout             = _timeout;  }
    void SetChannelFormat(const QString _fmt) { channelFormat   = _fmt; }

    SignalMonitor    *GetSignalMonitor(void) { return signalMonitor; }
    DTVSignalMonitor *GetDTVSignalMonitor(void);
    DVBSignalMonitor *GetDVBSignalMonitor(void);

  public slots:
    // Handler for complete tables
    void TransportTableComplete();
    void ServiceTableComplete();

  signals:
    // Values from 1-100 of scan completion
    void PctServiceScanComplete(int pct);
    void PctTransportScanComplete(int pct);
    void ServiceScanUpdateStatusText(const QString& status);
    void ServiceScanUpdateText(const QString& status);
    void TransportScanUpdateText(const QString& status);
    void ServiceScanComplete(void);
    void TransportScanComplete(void);

  private slots:
    void HandleVCT(uint tsid, const VirtualChannelTable*);
    void HandleMGT(const MasterGuideTable*);
    void HandleSDT(uint tsid, const ServiceDescriptionTable*);
    void HandleNIT(const NetworkInformationTable*);

  private:
    // some useful gets
    Channel          *GetChannel(void);
    DVBChannel       *GetDVBChannel(void);

    /// \brief Called by SpawnScanner to run scanning thread
    void RunScanner(void);
    /// \brief Thunk to call RunScanner from pthread
    static void *SpawnScanner(void *param);

    void HandleActiveScan(void);
    void ScanTransport(TransportScanItem &item, uint scanOffsetIt);

    /// \brief Updates Transport Scan progress bar
    inline void UpdateScanPercentCompleted(void);

    void HandleATSCDBInsertion(const ScanStreamData *sd, bool wait);
    void UpdateVCTinDB(int mplexid,
                       const VirtualChannelTable*,
                       bool force_update);

    void HandleDVBDBInsertion(const ScanStreamData *sd, bool wait);
    void UpdateSDTinDB(int mplexid,
                       const ServiceDescriptionTable*,
                       bool force_update);

    void OptimizeNITFrequencies(NetworkInformationTable *nit);

    // old stuff
    void verifyTransport(TransportScanItem& t);
    void CheckNIT(NITObject& NIT);
    void UpdateTransportsInDB(NITObject NIT);
    void UpdateServicesInDB(int tid_db, QMap_SDTObject SDT);
    static void *SpawnSectionReader(void*);

  private:
    // Set in constructor
    ChannelBase      *channel;
    SignalMonitor    *signalMonitor;
    int               sourceID;
    SCANMODE          scanMode;

    // Settable
    bool              ignoreAudioOnlyServices;
    bool              ignoreEncryptedServices;
    bool              forceUpdate;
    int               scanTimeout;
    QString           channelFormat;

    // State
    bool              threadExit;
    bool              waitingForTables;
    QTime             timer;

    // Transports List
    int                         transportsScanned;
    transport_scan_items_t      scanTransports;
    transport_scan_items_it_t   scanIt;
    uint                        scanOffsetIt;
    QMap<uint, uint>            dvbChanNums;

    bool serviceListReady;
    bool transportListReady;

    QMap_SDTObject SDT;
    NITObject NIT;
    pthread_t siparser_thread;
  public:
    DVBSIParser* siparser;
  private:

    /// Scanner thread, runs SIScan::StartScanner()
    pthread_t        scanner_thread;
    bool             scanner_thread_running;

    /// EIT Helper, if USING_DVB is true
    EITHelper       *eitHelper;
};

inline void SIScan::UpdateScanPercentCompleted(void)
{
    int tmp = (transportsScanned * 100) / scanTransports.size();
    emit PctServiceScanComplete(tmp);
}

#endif // SISCAN_H
