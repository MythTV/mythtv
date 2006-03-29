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

class MSqlQuery;
class ChannelBase;
class Channel;
class DVBChannel;
class SignalMonitor;
class DTVSignalMonitor;
class DVBSignalMonitor;
class ProgramAssociationTable;
class ProgramMapTable;
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
} SCANMODE;

typedef vector<const ProgramMapTable*>  pmt_vec_t;
typedef QMap<uint, pmt_vec_t>           pmt_map_t;

class SIScan : public QObject
{
    Q_OBJECT
  public:
    SIScan(QString _cardtype, ChannelBase* _channel, int _sourceID,
           uint signal_timeout, uint channel_timeout);
    ~SIScan();

    void StartScanner(void);
    void StopScanner(void);

    bool ScanTransports(
        int src, const QString std, const QString mod, const QString country);
    bool ScanTransportsStartingOn(
        int sourceid, const QMap<QString,QString> &valueMap);
    bool ScanTransport(int mplexid);

    bool ScanServicesSourceID(int SourceID);

    void SetSourceID(int _SourceID)   { sourceID                = _SourceID; }
    void SetFTAOnly(bool _fFTAOnly)   { ignoreEncryptedServices = _fFTAOnly; }
    void SetTVOnly(bool _tvOnly)
        { ignoreAudioOnlyServices = ignoreDataServices = _tvOnly; }
    void SetForceUpdate(bool _force)  { forceUpdate             = _force;    }
    void SetRenameChannels(bool _r)   { renameChannels          = _r; }
    void SetChannelFormat(const QString _fmt) { channelFormat   = _fmt; }
    void SetSignalTimeout(uint val)    { signalTimeout = val; }
    void SetChannelTimeout(uint val)   { channelTimeout = val; }

    uint GetSignalTimeout(void)  const { return signalTimeout; }
    uint GetChannelTimeout(void) const { return channelTimeout; }

    SignalMonitor    *GetSignalMonitor(void) { return signalMonitor; }
    DTVSignalMonitor *GetDTVSignalMonitor(void);
    DVBSignalMonitor *GetDVBSignalMonitor(void);

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
    void HandlePAT(const ProgramAssociationTable*);
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

    bool HasTimedOut(void);
    void HandleActiveScan(void);
    bool Tune(const transport_scan_items_it_t transport);
    int InsertMultiplex(const transport_scan_items_it_t transport);
    void ScanTransport(const transport_scan_items_it_t transport);

    /// \brief Updates Transport Scan progress bar
    inline void UpdateScanPercentCompleted(void);

    void HandleMPEGDBInsertion(const ScanStreamData *sd, bool wait);
    void UpdatePATinDB(int mplexid,
                       const ProgramAssociationTable*,
                       const pmt_map_t&,
                       bool force_update);

    void UpdatePMTinDB(int mplexid,
                       int db_source_id,
                       int freqid, int pmt_indx,
                       const ProgramMapTable*,
                       bool force_update);

    void HandleATSCDBInsertion(const ScanStreamData *sd, bool wait);
    void UpdateVCTinDB(int mplexid,
                       const VirtualChannelTable*,
                       bool force_update);

    void HandleDVBDBInsertion(const ScanStreamData *sd, bool wait);
    void UpdateSDTinDB(int mplexid,
                       const ServiceDescriptionTable*,
                       bool force_update);

    bool HandlePostInsertion(void);

    void OptimizeNITFrequencies(NetworkInformationTable *nit);

    static QString loc(const SIScan*);

  private:
    // Set in constructor
    ChannelBase      *channel;
    SignalMonitor    *signalMonitor;
    int               sourceID;
    SCANMODE          scanMode;
    uint              signalTimeout;
    uint              channelTimeout;

    // Settable
    bool              ignoreAudioOnlyServices;
    bool              ignoreDataServices;
    bool              ignoreEncryptedServices;
    bool              forceUpdate;
    bool              renameChannels;
    QString           channelFormat;

    // State
    bool              threadExit;
    bool              waitingForTables;
    QTime             timer;

    // Transports List
    int                         transportsScanned;
    transport_scan_items_t      scanTransports;
    transport_scan_items_it_t   current;
    transport_scan_items_it_t   nextIt;
    QMap<uint, uint>            dvbChanNums;

    /// Scanner thread, runs SIScan::StartScanner()
    pthread_t        scanner_thread;
    bool             scanner_thread_running;
};

inline void SIScan::UpdateScanPercentCompleted(void)
{
    int tmp = (transportsScanned * 100) / scanTransports.size();
    emit PctServiceScanComplete(tmp);
}

#endif // SISCAN_H
