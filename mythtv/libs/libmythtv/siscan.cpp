// -*- Mode: c++ -*-

// C includes
#include <cstdio>
#include <pthread.h>

// Qt includes
#include <qmutex.h>

// MythTV includes - General
#include "siscan.h"
#include "scheduledrecording.h"
#include "frequencies.h"
#include "mythdbcon.h"
#include "channelbase.h"
#include "channelutil.h"
#include "videosource.h" // for CardUtil

#ifdef USING_V4L
#include "channel.h"
#endif // USING_V4L

// MythTV includes - DTV
#include "dtvsignalmonitor.h"
#include "scanstreamdata.h"

// MythTV includes - ATSC
#include "atsctables.h"

// MythTV includes - DVB
#include "dvbsignalmonitor.h"
#include "dvbtables.h"
#include "eithelper.h"

#ifdef USING_DVB
#include "dvbchannel.h"
#include "siparser.h"
#include "dvbtypes.h"
#else // if !USING_DVB
class TransportObject;
class DVBTuning;
class dvb_channel_t;
typedef int fe_type_t;
#endif // USING_DVB

/// \brief How long we wait for the hardware to stabalize on a signal
#define SIGNAL_LOCK_TIMEOUT   500
/// \brief How long we wait for ATSC tables, before giving up
#define ATSC_TABLES_TIMEOUT 1500
/// \brief How long we wait for DVB tables, before giving up
#define DVB_TABLES_TIMEOUT  40000

#ifdef USING_DVB
static uint scan_for_best_freq(const TransportObject& transport,
                               DVBChannel* channel,
                               dvb_channel_t& chan_opts);
static bool ignore_encrypted_services(int db_mplexid, QString videodevice);
static int  create_dtv_multiplex_ofdm(
    const TransportScanItem& item, const DVBTuning& tuning);
void delete_services(int db_mplexid, QMap_SDTObject SDT);
#endif // USING_DVB

int GetDVBTID(uint16_t NetworkID, uint16_t TransportID, int CurrentMplexId);

/** \fn SIScan(QString _cardtype, ChannelBase* _channel, int _sourceID)
 */
SIScan::SIScan(QString _cardtype, ChannelBase* _channel, int _sourceID)
    : // Set in constructor
      channel(_channel),
      signalMonitor(SignalMonitor::Init(_cardtype, -1, _channel)),
      sourceID(_sourceID),
      scanMode(IDLE),
      // Settable
      ignoreAudioOnlyServices(true),
      ignoreEncryptedServices(false),
      forceUpdate(false),
      scanTimeout(0), /* Set scan timeout off by default */
      channelFormat("%1_%2"),
      // State
      threadExit(false),
      waitingForTables(false),
      // Transports List
      transportsScanned(0),
      // Misc
      scanner_thread_running(false),
      eitHelper(NULL)
{
    // Initialize statics
    init_freq_tables();

    /* setup boolean values for thread */
    serviceListReady = false;
    transportListReady = false;
    forceUpdate = false;

    siparser = NULL;
#ifdef USING_DVB
    if (GetDVBChannel())
        siparser = GetDVBChannel()->siparser;
    if (siparser)
    {
        /* Signals to process tables and do database inserts */
        connect(siparser, SIGNAL(FindTransportsComplete()),
                this,     SLOT(TransportTableComplete()));
        connect(siparser, SIGNAL(FindServicesComplete()),
                this,     SLOT(ServiceTableComplete()));

#ifdef USING_DVB_EIT
        eitHelper = new EITHelper();
        connect(siparser,  SIGNAL(EventsReady(QMap_Events*)),
                eitHelper, SLOT(HandleEITs(QMap_Events*)));
#endif // USING_DVB_EIT
        return;
    }
#endif // USING_DVB
    // Create a stream data for digital signal monitors
    DTVSignalMonitor* dtvSigMon = GetDTVSignalMonitor();
    if (dtvSigMon)
    {
        MPEGStreamData *data = new ScanStreamData();

        // Get NIT before we enable SDT in case of UK channel descriptor.
        data->RemoveListeningPID(DVB_SDT_PID);

        dtvSigMon->SetStreamData(data);
        dtvSigMon->AddFlags(kDTVSigMon_WaitForMGT | kDTVSigMon_WaitForVCT |
                            kDTVSigMon_WaitForNIT | kDTVSigMon_WaitForSDT);
        connect(data, SIGNAL(UpdateMGT(const MasterGuideTable*)),
                this, SLOT(  HandleMGT(const MasterGuideTable*)));
        connect(data, SIGNAL(UpdateVCT(uint, const VirtualChannelTable*)),
                this, SLOT(  HandleVCT(uint, const VirtualChannelTable*)));
        connect(data, SIGNAL(UpdateNIT(const NetworkInformationTable*)),
                this, SLOT(  HandleNIT(const NetworkInformationTable*)));
        connect(data, SIGNAL(UpdateSDT(uint, const ServiceDescriptionTable*)),
                this, SLOT(  HandleSDT(uint, const ServiceDescriptionTable*)));
    }
}

SIScan::~SIScan(void)
{
    StopScanner();
    SISCAN("SIScanner Stopped");
    if (signalMonitor)
        delete signalMonitor;
}

bool SIScan::ScanTransports(void)
{
    VERBOSE(VB_SIPARSER, "ScanTransports()");
#ifdef USING_DVB
    if (siparser)
        return siparser->FindTransports();
#endif // USING_DVB
    // We are always scanning for transports when tuned..
    return true;
}

/** \fn SIScan::ScanServicesSourceID(int)
 *  \brief If we are not already scanning a frequency table, this creates
 *         a new frequency table from database and begins scanning it.
 *
 *   This is used by DVB to scan for channels we are told about from other
 *   channels.
 *
 *   Note: Something similar could be used with ATSC when EIT for other
 *   channels is available on another ATSC channel, as encouraged by the
 *   ATSC specification.
 */
bool SIScan::ScanServicesSourceID(int SourceID)
{
    if (scanMode == TRANSPORT_LIST)
        return false;

    MSqlQuery query(MSqlQuery::InitCon());
    // Run DB query to get transports on sourceid SourceID
    // connected to this card
    QString theQuery = QString("SELECT mplexid, sistandard, transportid "
                               "FROM dtv_multiplex "
                               "WHERE sourceid = %1").arg(SourceID);
    query.prepare(theQuery);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Get Transports for SourceID", query);
        return false;
    }

    if (query.size() <= 0)
    {
        SISCAN(QString("Unable to find any transports for sourceId %1.")
               .arg(sourceID));
        return false;
    }

    scanIt = scanTransports.end();

    scanTimeout = DVB_TABLES_TIMEOUT;
    while (query.next())
    {
        if (query.value(1).toString() == "atsc")
            scanTimeout = ATSC_TABLES_TIMEOUT;

        QString fn = QString("Transport ID %1").arg(query.value(2).toString());
        SISCAN("Adding "+fn);

        TransportScanItem item(SourceID, query.value(0).toInt(), fn);
        scanTransports += item;
    }

    timer = QTime();
    waitingForTables  = false;

    scanIt            = scanTransports.begin();
    transportsScanned = 0;
    scanOffsetIt      = 0;
    if (scanTransports.size())
        scanMode      = TRANSPORT_LIST;

    return true;
}

void SIScan::HandleVCT(uint, const VirtualChannelTable*)
{
    SISCAN(QString("Got a Virtual Channel Table for %1")
           .arg((*scanIt).FriendlyName));
    HandleATSCDBInsertion(GetDTVSignalMonitor()->GetScanStreamData(), true);
}

void SIScan::HandleMGT(const MasterGuideTable*)
{
    SISCAN(QString("Got the Master Guide for %1").arg((*scanIt).FriendlyName));
    HandleATSCDBInsertion(GetDTVSignalMonitor()->GetScanStreamData(), true);
}

void SIScan::HandleSDT(uint, const ServiceDescriptionTable*)
{
    SISCAN(QString("Got a Service Description Table for %1")
           .arg((*scanIt).FriendlyName));
    HandleDVBDBInsertion(GetDTVSignalMonitor()->GetScanStreamData(), true);
}

void SIScan::HandleNIT(const NetworkInformationTable*)
{
    SISCAN(QString("Got a Network Information Table for %1")
           .arg((*scanIt).FriendlyName));
    const ScanStreamData *sd = GetDTVSignalMonitor()->GetScanStreamData();
    const NetworkInformationTable *nit = sd->GetCachedNIT();
    if (!nit)
        return;

    dvbChanNums.clear();

    //emit TransportScanUpdateText(tr("Optimizing transport frequency"));
    //OptimizeNITFrequencies(&nit);

    if (nit->TransportStreamCount())
    {
        emit TransportScanUpdateText(
            tr("Network %1 Processing").arg(nit->NetworkName()));

        ChannelUtil::CreateMultiplexes(sourceID, nit);

        // Get channel numbers from UK Frequency List Descriptors
        for (uint i = 0; i < nit->TransportStreamCount(); i++)
        {
            const desc_list_t& list =
                MPEGDescriptor::Parse(nit->TransportDescriptors(i),
                                      nit->TransportDescriptorsLength(i));

            const unsigned char* desc =
                MPEGDescriptor::Find(list, DescriptorID::dvb_uk_channel_list);

            if (desc)
            {
                UKChannelListDescriptor uklist(desc);
                for (uint j = 0; j < uklist.ChannelCount(); j++)
                    dvbChanNums[uklist.ServiceID(j)] = uklist.ChannelNumber(j);
            }
        }
    }
    sd->ReturnCachedTable(nit);

    emit TransportScanUpdateText(tr("Finished processing Transport List"));
    emit TransportScanComplete();
}

void SIScan::HandleATSCDBInsertion(const ScanStreamData *sd, bool wait_until_complete)
{
    bool hasAll = sd->HasCachedAllVCTs();
    if (wait_until_complete && !hasAll)
        return;

    // Insert TVCTs
    tvct_vec_t tvcts = sd->GetAllCachedTVCTs();
    for (uint i = 0; i < tvcts.size(); i++)
        UpdateVCTinDB((*scanIt).mplexid, tvcts[i], true);
    sd->ReturnCachedTVCTTables(tvcts);

    // Insert CVCTs
    cvct_vec_t cvcts = sd->GetAllCachedCVCTs();
    for (uint i = 0; i < cvcts.size(); i++)
        UpdateVCTinDB((*scanIt).mplexid, cvcts[i], true);
    sd->ReturnCachedCVCTTables(cvcts);

    // tell UI we are done with these channels
    if (scanMode == TRANSPORT_LIST)
    {
        UpdateScanPercentCompleted();
        waitingForTables = false;
    }
}

void SIScan::HandleDVBDBInsertion(const ScanStreamData *sd, bool wait_until_complete)
{
    bool hasAll = sd->HasCachedAllVCTs();
    if (wait_until_complete && !hasAll)
        return;

    emit ServiceScanUpdateText(tr("Updating Services"));

    vector<const ServiceDescriptionTable*> sdts = sd->GetAllCachedSDTs();
    for (uint i = 0; i < sdts.size(); i++)
        UpdateSDTinDB((*scanIt).mplexid, sdts[i], forceUpdate);
    sd->ReturnCachedSDTTables(sdts);

    emit ServiceScanUpdateText(tr("Finished processing Transport"));

    if (scanMode == TRANSPORT_LIST)
    {
        UpdateScanPercentCompleted();
        waitingForTables = false;
    }
    else
    {
        emit PctServiceScanComplete(100);
        emit ServiceScanComplete();
    }    
}

DTVSignalMonitor* SIScan::GetDTVSignalMonitor(void)
{
    return dynamic_cast<DTVSignalMonitor*>(signalMonitor);
}

DVBSignalMonitor* SIScan::GetDVBSignalMonitor(void)
{
#ifdef USING_DVB
    return dynamic_cast<DVBSignalMonitor*>(signalMonitor);
#else
    return NULL;
#endif
}

DVBChannel *SIScan::GetDVBChannel(void)
{
#ifdef USING_DVB
    return dynamic_cast<DVBChannel*>(channel);
#else
    return NULL;
#endif
}

Channel *SIScan::GetChannel(void)
{
#ifdef USING_V4L
    return dynamic_cast<Channel*>(channel);
#else
    return NULL;
#endif
}

bool SIScan::ScanServices(void)
{
    VERBOSE(VB_SIPARSER, "SIScan::ScanServices()");
#ifdef USING_DVB
    if (siparser)
    {
        // Requests the SDT table for the current transport from sections
        return siparser->FindServices();
    }
    if (GetDVBSignalMonitor())
    {
        // We are always scanning for VCTs, but we disable
        // SDT scans until we hace an NIT since it the NIT may
        // contain human friendly channel numbers to use in the
        // SDT insertion.
        GetDVBSignalMonitor()->GetScanStreamData()->
            AddListeningPID(DVB_SDT_PID);
        GetDVBSignalMonitor()->UpdateFiltersFromStreamData();
        return true;
    }
#endif // USING_DVB
    return false;
}

/** \fn SIScan::SpawnScanner(void*)
 *  \brief Thunk that allows scanner_thread pthread to
 *         call SIScan::RunScanner().
 */
void *SIScan::SpawnScanner(void *param)
{
    SIScan *scanner = (SIScan *)param;
    scanner->RunScanner();
    return NULL;
}

/** \fn SIScan::StartScanner(void)
 *  \brief Starts the SIScan event loop.
 */
void SIScan::StartScanner(void)
{
    pthread_create(&scanner_thread, NULL, SpawnScanner, this);
}

/** \fn SIScan::RunScanner(void)
 *  \brief This runs the event loop for SIScan until 'threadExit' is true.
 */
void SIScan::RunScanner(void)
{
    SISCAN("Starting SIScanner");
    scanner_thread_running = true;
    threadExit = false;

    while (!threadExit)
    {
        int mplex = -1;
        if (scanTransports.size() && scanIt != scanTransports.end() &&
            (*scanIt).mplexid >= 0)
            mplex = (*scanIt).mplexid;
#ifdef USING_DVB
        if (GetDVBChannel() && GetDVBChannel()->GetMultiplexID() > 0)
            mplex = GetDVBChannel()->GetMultiplexID();
#endif // USING_DVB

#ifdef USING_DVB_EIT
        if (eitHelper && eitHelper->GetListSize() && (mplex > 0))
            eitHelper->ProcessEvents(mplex);
#endif // USING_DVB_EIT

        if (serviceListReady && siparser && (mplex > 0))
        {
#ifdef USING_DVB
            if (siparser->GetServiceObject(SDT))
            {
                UpdateServicesInDB(mplex, SDT);
                scanOffsetIt = 99;
            }
#endif // USING_DVB
            serviceListReady = false;
            if (scanMode == TRANSPORT_LIST)
            {
                UpdateScanPercentCompleted();
                waitingForTables = false;
            }
            else
            {
                emit PctServiceScanComplete(100);
                emit ServiceScanComplete();
            }
        }
        if (transportListReady && siparser && (mplex > 0))
        {
#ifdef USING_DVB
            if (siparser->GetTransportObject(NIT))
            {
                emit TransportScanUpdateText("Processing Transport List");
                SISCAN("Updating Transports");
                CheckNIT(NIT);
                UpdateTransportsInDB(NIT);
                SISCAN("Transport Update Complete");
                emit TransportScanUpdateText("Finished processing Transport List");
            }
            transportListReady = false;
#endif // USING_DVB
            emit TransportScanComplete();
        }

        if (scanMode == TRANSPORT_LIST)
            HandleActiveScan();

        usleep(250);
    }
    scanner_thread_running = false;
}

/** \fn SIScan::HandleActiveScan(void)
 *  \brief Handles the TRANSPORT_LIST SIScan mode.
 */
void SIScan::HandleActiveScan(void)
{
    QString offset_str = (scanOffsetIt) ?
        QObject::tr(" offset %2").arg(scanOffsetIt) : "";
    QString cur_chan = QString("%1%2")
        .arg((*scanIt).FriendlyName).arg(offset_str);
    QString time_out_table_str =
        QObject::tr("Timeout Scanning %1 -- no tables").arg(cur_chan);
    QString time_out_sig_str =
        QObject::tr("Timeout Scanning %1 -- no signal").arg(cur_chan);

    // handle first transport
    if (timer.isNull())
    {
        timer.start();
        ScanTransport(*scanIt, scanOffsetIt);
        return;
    }

    // See if we have timed out
    bool timed_out = ((scanTimeout > 0) && (timer.elapsed() > scanTimeout));
    if (timed_out)
    {
        if (scanOffsetIt < (*scanIt).offset_cnt())
        {
            emit ServiceScanUpdateText(time_out_table_str);
            SISCAN(time_out_table_str);
        }
        waitingForTables = false;
    }
    else if ((*scanIt).standard.lower() == "atsc")
    {
        if ((scanTimeout > 0) && (timer.elapsed() > SIGNAL_LOCK_TIMEOUT))
        {
            // If we don't have a signal in SIGNAL_LOCK_TIMEOUT msec, continue..
            timed_out = false;
            SignalMonitor *sm = GetSignalMonitor();
            if (!sm)
                return; // still waiting

            SignalMonitorList list = SignalMonitorValue::Parse(sm->GetStatusList());
            for (uint i = 0; i < list.size(); i++)
            {
                if (list[i].GetShortName() == "slock")
                {
                    timed_out = !list[i].IsGood();
                    break;
                }
            }
            if (!timed_out)
                return; // still waiting

            emit ServiceScanUpdateText(time_out_sig_str);
            SISCAN(time_out_sig_str);
        }
        else
        {
            return; // still waiting
        }
    }
    else if (waitingForTables && scanOffsetIt < (*scanIt).offset_cnt())
    {
        return; // still waiting
    }

    // Check if we got some tables, but not all...
    if (!siparser && GetDTVSignalMonitor())
    {
        const ScanStreamData *sd = GetDTVSignalMonitor()->GetScanStreamData();
        if (timed_out && waitingForTables && sd->GetCachedMGT())
        {
            VERBOSE(VB_IMPORTANT, sd->GetCachedMGT()->toString());
        }

        // Get NIT before we enable SDT in case of UK channel descriptor.
        GetDTVSignalMonitor()->GetScanStreamData()->RemoveListeningPID(DVB_SDT_PID);
    }

    // Increment iterator
    scanOffsetIt++;
    if (scanIt != scanTransports.end())
    {
        if (scanOffsetIt >= (*scanIt).offset_cnt())
        {
            transportsScanned++;
            UpdateScanPercentCompleted();
            scanOffsetIt=0;
            scanIt++;
        }
    }

    // If we are done emit ServiceScanComplete(), otherwise scan the transport..
    if ((scanIt == scanTransports.end()) &&
        (scanOffsetIt >= (*scanIt).offset_cnt()))
    {
        emit ServiceScanComplete();
        scanMode = IDLE;
        scanTransports.clear();
    }
    else
    {
        ScanTransport(*scanIt, scanOffsetIt);
    }
}

void SIScan::ScanTransport(TransportScanItem &item, uint scanOffsetIt)
{
    QString offset_str = (scanOffsetIt) ?
        QObject::tr(" offset %2").arg(scanOffsetIt) : "";
    QString cur_chan = QString("%1%2")
        .arg((*scanIt).FriendlyName).arg(offset_str);
    QString tune_msg_str =
        QObject::tr("Tuning to %1 mplexid(%2)")
        .arg(cur_chan).arg((*scanIt).mplexid);

    if (scanOffsetIt && (scanOffsetIt >= (*scanIt).offset_cnt()) &&
        item.freq_offset(scanOffsetIt) == item.freq_offset(0))
    {
        timer.addSecs(-120);
        return; // nothing to do
    }

    // Stop signal monitor for previous channel
    signalMonitor->Stop();

    emit ServiceScanUpdateStatusText(cur_chan);
    SISCAN(tune_msg_str);

    bool ok = false;
#ifdef USING_DVB
    // Tune to multiplex
    if (GetDVBChannel())
    {
        // Start signal monitor for this channel
        signalMonitor->Start();
        if (item.mplexid >= 0)
        {
            ok = GetDVBChannel()->TuneMultiplex(item.mplexid);
        }
        else
        {
            dvb_channel_t dvbchan;
            dvbchan.sistandard = item.standard;
            dvbchan.tuning     = item.tuning;
            dvbchan.tuning.params.frequency = item.freq_offset(scanOffsetIt);
            ok = GetDVBChannel()->TuneTransport(dvbchan, true, item.timeoutTune);

            if (ok)
            {
                // Try to read the actual values back from the card
                DVBTuning tuning;
                bool ok = GetDVBChannel()->GetTuningParams(tuning);
                if (!ok)
                    tuning = item.tuning;

                // Write the best info we have to the DB
                if (FE_OFDM == GetDVBChannel()->GetCardType())
                    item.mplexid = create_dtv_multiplex_ofdm(item, tuning);
                else
                    item.mplexid = ChannelUtil::CreateMultiplex(
                        item.SourceID, item.standard,
                        tuning.Frequency(), tuning.ModulationDB());
            }
        }
    }
#endif // USING_DVB
#ifdef USING_V4L
    if (GetChannel())
    {
        uint freq = item.freq_offset(scanOffsetIt);
        if (item.mplexid >= 0)
            ok = GetChannel()->TuneMultiplex(item.mplexid);
        else 
        {
            uint f = freq - 1750000; // convert to visual carrier
            QString inputname = ChannelUtil::GetInputName(item.SourceID);
            if (ok = GetChannel()->Tune(f, inputname, "8vsb"))
            {
                item.mplexid = ChannelUtil::CreateMultiplex(
                    item.SourceID, item.standard, f, "8vsb");
            }
        }
    }
#endif
    ok = (item.mplexid < 0) ? false : ok;

    // If we didn't tune successfully, bail with message
    if (!ok)
    {
        UpdateScanPercentCompleted();
        SISCAN(QString("Failed to tune %1 mplexid(%2) at offset %3")
               .arg(item.FriendlyName).arg(item.mplexid).arg(scanOffsetIt));
        return;
    }

    // If we have a DTV Signal Monitor, perform table scanner reset
    if (GetDTVSignalMonitor() && GetDTVSignalMonitor()->GetScanStreamData())
    {
        GetDTVSignalMonitor()->GetScanStreamData()->Reset();
        GetDTVSignalMonitor()->SetChannel(-1,-1);
    }

    // Start signal monitor for this channel
    if (!siparser)
        signalMonitor->Start();
#ifdef USING_DVB
    if (siparser)
        siparser->FindServices();
#endif // USING_DVB

    timer.start();
    waitingForTables = true;
}

/** \fn SIScan::StopScanner(void)
 *  \brief Stops the SIScan event loop and the signal monitor,
 *         blocking until both exit.
 */
void SIScan::StopScanner(void)
{
    threadExit = true;
    SISCAN("Stopping SIScanner");
#ifdef USING_DVB
    /* Force dvbchannel to exit */
    if (siparser && GetDVBChannel())
        GetDVBChannel()->StopTuning();
#endif // USING_DVB
    if (signalMonitor)
        signalMonitor->Stop();
    if (scanner_thread_running)
        pthread_join(scanner_thread, NULL);
}

void SIScan::verifyTransport(TransportScanItem& t)
{
    (void) t;
#ifdef USING_DVB
    MSqlQuery query(MSqlQuery::InitCon());
    /* See if mplexid is already in the database */
    QString theQuery = QString("select mplexid from dtv_multiplex where "
                       "sourceid = %1 and frequency = %2")
                       .arg(t.SourceID)
                       .arg(t.tuning.params.frequency);
    query.prepare(theQuery);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Check for existing transport", query);

    if (query.size() > 0)
    {
        query.next();
        t.mplexid = query.value(0).toInt();            
        return;
    }

    if (t.offset1)
    {
        theQuery = QString("select mplexid from dtv_multiplex where "
                       "sourceid = %1 and frequency = %2")
                       .arg(t.SourceID)
                       .arg(t.tuning.params.frequency+t.offset1);
        query.prepare(theQuery);

        if (!query.exec() || !query.isActive())
            MythContext::DBError("Check for existing transport", query);

        if (query.size() > 0)
        {
            query.next();
            t.mplexid = query.value(0).toInt();            
            return;
        }
    }

    if (t.offset2)
    {
        theQuery = QString("select mplexid from dtv_multiplex where "
                       "sourceid = %1 and frequency = %2")
                       .arg(t.SourceID)
                       .arg(t.tuning.params.frequency+t.offset2);
        query.prepare(theQuery);

        if (!query.exec() || !query.isActive())
            MythContext::DBError("Check for existing transport", query);

        if (query.size() > 0)
        {
            query.next();
            t.mplexid = query.value(0).toInt();            
            return;
        }
    }

    t.mplexid = -1;
#endif // USING_DVB
}

/** \fn SIScan::ScanTransports(int,const QString,const QString,const QString)
 *  \brief Generates a list of frequencies to scan and adds it to the
 *   scanTransport list, and then sets the scanMode to TRANSPORT_LIST.
 */
bool SIScan::ScanTransports(int SourceID,
                            const QString std,
                            const QString modulation,
                            const QString country)
{
    QString si_std = (std.lower() != "atsc") ? "dvb" : "atsc";
    QString name("");
    if (scanMode == TRANSPORT_LIST)
        return false;

    scanTransports.clear();
    scanIt = scanTransports.end();

    freq_table_list_t tables = get_matching_freq_tables(std, modulation, country);
    VERBOSE(VB_SIPARSER, QString("Looked up freq table (%1, %2, %3)")
            .arg(std).arg(modulation).arg(country));

    freq_table_list_t::iterator it = tables.begin();
    for (; it != tables.end(); ++it)
    {
        const FrequencyTable &ft = **it;
        int     name_num         = ft.name_offset;
        QString strNameFormat    = ft.name_format;
        uint    freq             = ft.frequencyStart;
        while (freq <= ft.frequencyEnd)
        {
            if (strNameFormat.length() >= 2)
                name = strNameFormat.arg(name_num);

            TransportScanItem item(SourceID, si_std, name, name_num, freq, ft);
            verifyTransport(item);
            scanTransports += item;

            VERBOSE(VB_SIPARSER, item.toString());

            name_num++;
            freq += ft.frequencyStep;
        }
    }

    timer = QTime();
    scanTimeout      = (si_std == "dvb") ? DVB_TABLES_TIMEOUT : ATSC_TABLES_TIMEOUT;
    waitingForTables = false;

    scanIt            = scanTransports.begin();
    transportsScanned = 0;
    scanOffsetIt      = 0;
    scanMode          = TRANSPORT_LIST;

    return true;
}

bool SIScan::ScanTransport(int mplexid)
{
    scanTransports.clear();
    scanIt = scanTransports.end();

    MSqlQuery query(MSqlQuery::InitCon());
    // Run DB query to get transports on sourceid SourceID
    // connected to this card
    QString theQuery = QString(
        "SELECT sourceid, mplexid, sistandard, transportid "
        "FROM dtv_multiplex WHERE mplexid = %2").arg(mplexid);
    query.prepare(theQuery);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("Get Transports for SourceID", query);
        return false;
    }

    if (query.size() <= 0)
    {
        SISCAN(QString("Unable to find transport to scan."));
        return false;
    }

    scanTimeout = DVB_TABLES_TIMEOUT;
    while (query.next())
    {
        if (query.value(2).toString() == "atsc")
            scanTimeout = ATSC_TABLES_TIMEOUT;

        QString fn = QString("Transport ID %1").arg(query.value(3).toString());
        SISCAN("Adding "+fn);

        TransportScanItem item(query.value(0).toInt(), query.value(1).toInt(), fn);
        scanTransports += item;
    }

    timer = QTime();
    waitingForTables  = false;

    transportsScanned = 0;
    scanIt            = scanTransports.end();
    scanOffsetIt      = 0;
    if (scanTransports.size())
    {
        scanIt        = scanTransports.begin();
        scanMode      = TRANSPORT_LIST;
        return true;
    }

    return false;
}

void SIScan::ServiceTableComplete()
{
    serviceListReady = true;
}

void SIScan::TransportTableComplete()
{
    transportListReady = true;
}

/** \fn SIScan::OptimizeNITFrequencies(NetworkInformationTable *nit)
 *  \brief Checks that we can tune to a transports described in NIT.
 *
 *   For each transport freqency list entry, we try to tune to it
 *   if it works use it as the frequency.
 *
 *  \todo We should probably try and work out the strongest signal.
 */
void SIScan::OptimizeNITFrequencies(NetworkInformationTable *nit)
{
    (void) nit;
#if 0
    dvb_channel_t chan_opts;
    DVBTuning    &tuning = chan_opts.tuning;

    QValueList<TransportObject>::iterator it;
    for (it = NIT.Transport.begin() ; it != NIT.Transport.end() ; ++it )
    {
        const TransportObject& transport = *it;
        // Parse the default transport object
        bool ok = false;
        if (transport.Type == "DVB-T")
            ok = tuning.parseOFDM(transport);
        else if (transport.Type == "DVB-C")
            ok = tuning.parseQAM(transport);

        // Find the best frequency from those listed in transport object.
        if (ok)
        {
            (*it).Frequency = scan_for_best_freq(
                transport, GetDVBChannel(), chan_opts);
        }
    }
#endif // USING_DVB
}

// ///////////////////// Static helper methods /////////////////////
// ///////////////////// Static helper methods /////////////////////
// ///////////////////// Static helper methods /////////////////////
// ///////////////////// Static helper methods /////////////////////
// ///////////////////// Static helper methods /////////////////////

#ifdef USING_DVB
static uint scan_for_best_freq(const TransportObject& transport,
                               DVBChannel *dvbc,
                               dvb_channel_t& chan_opts)
{
    uint init_freq = chan_opts.tuning.params.frequency;
    // Start off with the main frequency,
    // then try the other ones in the list
    QValueList<unsigned>::const_iterator fit = transport.frequencies.begin();
    bool first_it = true;
    do
    {
        if (!first_it)
        {
            chan_opts.tuning.params.frequency = *fit;
            fit++;
        }
        first_it = false;

        if (dvbc->Tune(chan_opts, true))
        {
            return chan_opts.tuning.params.frequency;
            break;
        }
    }
    while (fit != transport.frequencies.end());

    return init_freq;
}
#endif // USING_DVB

// ///////////////////// DB STUFF /////////////////////
// ///////////////////// DB STUFF /////////////////////
// ///////////////////// DB STUFF /////////////////////
// ///////////////////// DB STUFF /////////////////////
// ///////////////////// DB STUFF /////////////////////

/** \fn UpdateVCTinDB(int, const const VirtualChannelTable*, bool)
 */
void SIScan::UpdateVCTinDB(int tid_db,
                           const VirtualChannelTable *vct,
                           bool force_update)
{
    (void) force_update;

    int db_mplexid = ChannelUtil::GetBetterMplexID(
        tid_db, vct->TransportStreamID(), 0);

    if (db_mplexid == -1)
    {
        VERBOSE(VB_IMPORTANT, "Warning: Could not determine "
                " what transport this channel is associated with, \n"
                " using GetBetterMplex(), trying GetDVBTID().");
        db_mplexid = GetDVBTID(0, vct->TransportStreamID(), tid_db);
        if (db_mplexid == -1)
        {
            VERBOSE(VB_IMPORTANT, "VCT: Error determing what transport this "
                    "service table is associated with so failing");
            return;
        }
    }

/*    bool fta_only     = ignore_encrypted_services(db_mplexid, videodevice);*/
    int db_source_id   = ChannelUtil::GetSourceID(db_mplexid);
    int freqid         = (*scanIt).friendlyNum;

    for (uint i = 0; i < vct->ChannelCount(); i++)
    {
        if (vct->ModulationMode(i) == 0x01 /* NTSC Modulation */ ||
            vct->ServiceType(i)    == 0x01 /* Analog TV */)
        {
            continue;
        }

        if (vct->ServiceType(i) == 0x04)
        {
            VERBOSE(VB_IMPORTANT, QString("Ignoring Data Service: %1 %2-%3")
                    .arg(vct->ShortChannelName(i))
                    .arg(vct->MajorChannel(i)).arg(vct->MinorChannel(i)));
            continue;            
        }

        if (vct->ServiceType(i) == 0x03 && ignoreAudioOnlyServices)
        {
            VERBOSE(VB_IMPORTANT, QString("Ignoring Radio Service: %1 %2-%3")
                    .arg(vct->ShortChannelName(i))
                    .arg(vct->MajorChannel(i)).arg(vct->MinorChannel(i)));
            continue;            
        }

        if (vct->IsAccessControlled(i) && ignoreEncryptedServices)
        {
            VERBOSE(VB_IMPORTANT, QString("Ignoring Encrypted Service: %1 %2-%3")
                    .arg(vct->ShortChannelName(i))
                    .arg(vct->MajorChannel(i)).arg(vct->MinorChannel(i)));
            continue;
        }

        // See if service already in database
        int chanid = ChannelUtil::GetChanID(
            db_mplexid, vct->ChannelTransportStreamID(i),
            vct->MajorChannel(i), vct->MinorChannel(i),
            vct->ProgramNumber(i));

        QString chan_num = ChannelUtil::GetChanNum(chanid);
        if (chan_num == QString::null || chan_num == "")
        {
            chan_num = channelFormat
                .arg(vct->MajorChannel(i))
                .arg(vct->MinorChannel(i));
        }

        QString common_status_info = tr("%1 %2-%3 as %4 on %5 (%6)")
            .arg(vct->ShortChannelName(i))
            .arg(vct->MajorChannel(i)).arg(vct->MinorChannel(i))
            .arg(chan_num)
            .arg((*scanIt).FriendlyName).arg(freqid);

        if (chanid < 0)
        {   // The service is not in database, add it
            emit ServiceScanUpdateText(
                tr("Adding %1").arg(common_status_info));
            chanid = ChannelUtil::CreateChanID(db_source_id, chan_num);
            if (chanid > 0)
            {
                ChannelUtil::CreateChannel(
                    db_mplexid,
                    db_source_id,
                    chanid,
                    vct->ShortChannelName(i),
                    chan_num,
                    vct->ProgramNumber(i),
                    vct->MajorChannel(i), vct->MinorChannel(i),
                    true,
                    vct->IsHidden(i), vct->IsHiddenInGuide(i),
                    freqid);
            }
        }
        else
        {   // The service is in database, update it
            emit ServiceScanUpdateText(
                tr("Updating %1").arg(common_status_info));
            ChannelUtil::UpdateChannel(
                db_mplexid,
                db_source_id,
                chanid,
                vct->ShortChannelName(i),
                chan_num,
                vct->ProgramNumber(i),
                vct->MajorChannel(i), vct->MinorChannel(i),
                freqid);
        }
    }
}

/** \fn SIScan::UpdateSDTinDB(int,const ServiceDescriptionTable*,bool)
 *  \brief dummy method for now.
 */
void SIScan::UpdateSDTinDB(int tid_db, const ServiceDescriptionTable *sdt,
                           bool force_update)
{
    (void) tid_db;
    (void) sdt;
    (void) force_update;
}

/** \fn SIScan::UpdateServicesInDB(int tid_db, QMap_SDTObject SDT)
 *
 *   If the mplexid matches the first networkid/transport ID assume you
 *   are on the right mplexid..
 *
 *   If NOT then search that SourceID for a matching
 *   NetworkId/TransportID and use that as the mplexid..
 *
 *     3. Check the existance of each ServiceID on the correct mplexid.
 *     If there are any extras mark then as hidden (as opposed to
 *     removing them).  There might also be a better way of marking
 *     them as old.. I am not sure yet.. If the ChanID scheme is
 *     better than whats in place now this might be easier..
 *
 *     4. Add any missing ServiceIDs.. Set the hidden flag if they are
 *     not a TV service, and or if they are not FTA (Depends on if FTA
 *     is allowed or not)..
 *
 *     First get the mplexid from channel.. This will help you determine
 *     what mplexid these channels are really associated with..
 *     They COULD be from another transport..
 *
 *     Check the ServiceVersion and if its the same as you have, or a
 *     forced update is required because its been too long since an update
 *     forge ahead.
 *
 *  \todo If you get a Version mismatch and you are not in setupscan
 *        mode you will want to issue a scan of the other transports
 *        that have the same networkID.
 */
void SIScan::UpdateServicesInDB(int tid_db, QMap_SDTObject SDT)
{
    (void) SDT;
#ifdef USING_DVB
    if (SDT.empty())
    {
        SISCAN("SDT List Empty assuming this is by error leaving services intact");
        return;
    }

    QMap_SDTObject::Iterator s = SDT.begin();

    int db_mplexid = ChannelUtil::GetBetterMplexID(
        tid_db, (*s).TransportID, (*s).NetworkID);

    if (db_mplexid == -1)
    {
        VERBOSE(VB_IMPORTANT, "Warning: Could not determine "
                " what transport this channel is associated with, \n"
                " using GetBetterMplex(), trying GetDVBTID().");
        db_mplexid = GetDVBTID((*s).NetworkID, (*s).TransportID, tid_db);
        if (db_mplexid == -1)
        {
            VERBOSE(VB_IMPORTANT, "SRV: Error determing what transport this "
                    "service table is associated with so failing");
            return;
        }
    }

/*  Check the ServiceVersion and if its the same as you have, or a 
    forced update is required because its been too long since an update 
    forge ahead.

    TODO: If you get a Version mismatch and you are not in setupscan
          mode you will want to issue a scan of the other transports
          that have the same networkID
*/

    MSqlQuery query(MSqlQuery::InitCon());
    QString versionQuery = QString("SELECT serviceversion,sourceid FROM dtv_multiplex "
                                   "WHERE mplexid = %1")
                                   .arg(db_mplexid);
    query.prepare(versionQuery);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Selecting channel/dtv_multiplex", query);

    if (query.size() > 0)
    {
        query.next();

        if (!forceUpdate && (query.value(0).toInt() == (*s).Version))
        {
            SISCAN("Service table up to date for this network.");
            emit ServiceScanUpdateText("Channels up to date");
            return;
        }
        else
        {
            versionQuery = QString("UPDATE dtv_multiplex SET serviceversion = %1 "
                                   "WHERE mplexid = %2")
                                   .arg((*s).Version)
                                   .arg(db_mplexid);
            query.prepare(versionQuery);

            if(!query.exec())
                MythContext::DBError("Selecting channel/dtv_multiplex", query);

            if (!query.isActive())
                MythContext::DBError("Check Channel full in channel/dtv_multiplex.", query);
        }
    }

    ignoreEncryptedServices = 
        ignore_encrypted_services(db_mplexid, GetDVBChannel()->GetDevice());

    // This code has caused too much trouble to leave in for now - Taylor
    //delete_services(db_mplexid, SDT);

    int db_source_id   = ChannelUtil::GetSourceID(db_mplexid);
    int freqid         = (*scanIt).friendlyNum;

    for (s = SDT.begin() ; s != SDT.end() ; ++s )
    {
        QString service_name = (*s).ServiceName;

        if (service_name.stripWhiteSpace().isEmpty())
            service_name = QString("%1 %2")
                .arg((*s).ServiceID).arg(db_mplexid);

        if (((*s).ServiceType == SDTObject::RADIO) && ignoreAudioOnlyServices)
        {
            emit ServiceScanUpdateText(
                tr("Skipping %1 - Radio Service").arg(service_name));
            continue;
        }
        else if (((*s).ServiceType != SDTObject::TV) &&
                 ((*s).ServiceType != SDTObject::RADIO))
        {
            emit ServiceScanUpdateText(tr("Skipping %1 - not a Television or "
                                          "Radio Service").arg(service_name));
            continue;
        }
        else if (ignoreEncryptedServices && ((*s).CAStatus != 0))
        {
            emit ServiceScanUpdateText(tr("Skipping %1 - Encrypted Service")
                .arg(service_name));
            continue;
        }

        // See if service already in database
        int chanid = ((*s).ATSCSourceID)
            ? ChannelUtil::GetChanID(
                db_mplexid, -1,
                (*s).ChanNum / 10, (*s).ChanNum % 10,
                (*s).ServiceID)
            : ChannelUtil::GetChanID(db_mplexid, -1, 0, 0,
                                     (*s).ServiceID);

        QString chan_num = QString::number((*s).ServiceID);
        if ((*s).ChanNum != -1)
            chan_num = QString::number((*s).ChanNum);

        if ((*s).ATSCSourceID && (*s).ChanNum > 0)
            chan_num = channelFormat
                .arg((*s).ChanNum / 10)
                .arg((*s).ChanNum % 10);

        QString common_status_info = tr("%1 %2-%3 as %4 on %5 (%6)")
            .arg(service_name)
            .arg((*s).ChanNum / 10).arg((*s).ChanNum % 10)
            .arg(chan_num)
            .arg((*scanIt).FriendlyName).arg(freqid);

        if (!(*s).ATSCSourceID)
            common_status_info = service_name;

        if (chanid < 0)
        {   // The service is not in database, add it
            emit ServiceScanUpdateText(tr("Adding %1").arg(service_name));
            chanid = ChannelUtil::CreateChanID(db_source_id, chan_num);
            int atsc_major = ((*s).ATSCSourceID) ? ((*s).ChanNum / 10) : 0;
            int atsc_minor = ((*s).ATSCSourceID) ? ((*s).ChanNum % 10) : 0;

            ChannelUtil::CreateChannel(
                db_mplexid,
                db_source_id,
                chanid,
                (*s).ServiceName,
                chan_num,
                (*s).ServiceID,
                atsc_major, atsc_minor,
                true,
                !(*s).EITPresent, !(*s).EITPresent,
                freqid);
        }
        else if ((*s).ATSCSourceID)
        {   // The service is in database & we have good info, update it
            emit ServiceScanUpdateText(tr("Updating %1").arg(service_name));

            ChannelUtil::UpdateChannel(
                db_mplexid,
                db_source_id,
                chanid,
                (*s).ServiceName,
                chan_num,
                (*s).ServiceID,
                (*s).ChanNum / 10, (*s).ChanNum % 10,
                freqid);
        }
        else
        {
            emit ServiceScanUpdateText(
                tr("Skipping %1 - already in DB, and we don't have better data.")
                .arg(service_name));
        }
    }
#endif // USING_DVB
}

void SIScan::CheckNIT(NITObject& NIT)
{
    (void) NIT;
#ifdef USING_DVB
    dvb_channel_t chan_opts;
    QValueList<TransportObject>::Iterator t;
    for (t = NIT.Transport.begin() ; t != NIT.Transport.end() ; ++t )
    {
        TransportObject& transport = *t;
        if (transport.frequencies.empty())
            continue;
        //for each transport freqency list entry, try to tune to it
        //if it works use it as the frequency, we should probably try
        //and work out the strongest signal.

        //Parse the default transport object
        if (transport.Type == "DVB-T")
        {
            if (!chan_opts.tuning.parseOFDM(
                             QString::number(transport.Frequency),
                             transport.Inversion,
                             transport.Bandwidth,
                             transport.CodeRateHP,
                             transport.CodeRateLP,
                             transport.Constellation,
                             transport.TransmissionMode,
                             transport.GuardInterval,
                             transport.Hiearchy))
            {
                //cerr << "failed to parse transport\n";
                break;
            }
        }
        else if (transport.Type == "DVB-C")
        {
            if (!chan_opts.tuning.parseQAM(
                             QString::number(transport.Frequency),
                             transport.Inversion,
                             QString::number(transport.SymbolRate),
                             transport.FEC_Inner,
                             transport.Modulation))
            {
                //cerr << "failed to parse transport\n";
                break;
            }
        }
        else  //Lets hope we never get here
            break;

        //Start off with the main frequency,
        if (GetDVBChannel()->TuneTransport(chan_opts, true, scanTimeout))
        {
//             cerr << "Tuned to main frequency "<< transport.Frequency << "\n";
            continue;
        }

        //Now the other ones in the list
        QValueList<unsigned>::Iterator f;
        for (f=transport.frequencies.begin();f!=transport.frequencies.end();f++)
        {
            unsigned nfrequency = *f;
            chan_opts.tuning.params.frequency = nfrequency;
//            cerr << "CheckNIT " << nfrequency << "\n";
            if (GetDVBChannel()->TuneTransport(chan_opts, true, scanTimeout))
            {
//                cerr << "Tuned frequency "<< nfrequency << "\n";
                transport.Frequency = nfrequency;
                break;
            }
        }
    }
#endif // USING_DVB
}

void SIScan::UpdateTransportsInDB(NITObject NIT)
{
    (void) NIT;
#ifdef USING_DVB
    QValueList<NetworkObject>::Iterator n;
    QValueList<TransportObject>::Iterator t;

    // TODO: Add in ServiceList stuff, and automaticly add those serviceIDs (how you get the
    //       the channel numbers for the UK DVB-T channels)

    n = NIT.Network.begin();

    if(!NIT.Network.empty())
    {
        SISCAN(QString("Network %1 Scanned").arg((*n).NetworkName));
        emit TransportScanUpdateText(QString("Network %1 Processing").arg((*n).NetworkName));
    }

    for (t = NIT.Transport.begin() ; t != NIT.Transport.end() ; ++t )
    {
        MSqlQuery query(MSqlQuery::InitCon());
        // See if transport already in database
        QString theQuery = QString("select * from dtv_multiplex where NetworkID = %1 and "
                   " TransportID = %2 and Frequency = %3 and sourceID = %4")
                   .arg((*t).NetworkID)
                   .arg((*t).TransportID)
                   .arg((*t).Frequency)
                   .arg(sourceID);
        query.prepare(theQuery);

        if(!query.exec())
            MythContext::DBError("Selecting transports", query);

        if (!query.isActive())
            MythContext::DBError("Check Transport in dtv_multiplex.", query);

        // If transport not present add it, and move on to the next
        if (query.size() <= 0)
        {
            emit TransportScanUpdateText(QString("Transport %1 - %2 Added")
                   .arg((*t).TransportID)
                   .arg((*t).NetworkID));

            SISCAN(QString("Transport TID = %1 NID = %2 added to Database")
                   .arg((*t).TransportID)
                   .arg((*t).NetworkID));

            query.prepare("INSERT into dtv_multiplex (transportid, "
                               "networkid, frequency, symbolrate, fec, "
                               "polarity, modulation, constellation, "
                               "bandwidth, hierarchy, hp_code_rate, "
                               "lp_code_rate, guard_interval, "
                               "transmission_mode, inversion, sourceid) "
                               "VALUES (:TRANSPORTID,:NETWORKID,:FREQUENCY,"
                               ":SYMBOLRATE,:FEC,:POLARITY,:MODULATION,"
                               ":CONSTELLATION,:BANDWIDTH,:HIERARCHY,"
                               ":HP_CODE_RATE,:LP_CODE_RATE,:GUARD_INTERVAL,"
                               ":TRANSMISSION_MODE,:INVERSION,:SOURCEID);");
                               query.bindValue(":TRANSPORTID",(*t).TransportID);
                               query.bindValue(":NETWORKID",(*t).NetworkID);
                               query.bindValue(":FREQUENCY",(*t).Frequency);
                               query.bindValue(":SYMBOLRATE",(*t).SymbolRate);
                               query.bindValue(":FEC",(*t).FEC_Inner);
                               query.bindValue(":POLARITY",(*t).Polarity);
                               query.bindValue(":MODULATION",(*t).Modulation);
                               query.bindValue(":CONSTELLATION",(*t).Constellation);
                               query.bindValue(":BANDWIDTH",(*t).Bandwidth);
                               query.bindValue(":HIERARCHY",(*t).Hiearchy);
                               query.bindValue(":HP_CODE_RATE",(*t).CodeRateHP);
                               query.bindValue(":LP_CODE_RATE",(*t).CodeRateLP);
                               query.bindValue(":GUARD_INTERVAL",(*t).GuardInterval);
                               query.bindValue(":TRANSMISSION_MODE",(*t).TransmissionMode);
                               query.bindValue(":INVERSION",(*t).Inversion);
                               query.bindValue(":SOURCEID",sourceID);
            if(!query.exec())
                MythContext::DBError("Inserting new transport", query);
            if (!query.isActive())
                MythContext::DBError("Adding transport to Database.", query);
        }
        else
        {
            emit TransportScanUpdateText(QString("Transport %1 - %2 Already in Database")
                   .arg((*t).TransportID)
                   .arg((*t).NetworkID));

            SISCAN(QString("Transport TID = %1 NID = %2 already in Database")
                   .arg((*t).TransportID)
                   .arg((*t).NetworkID));
        }
    }
#endif // USING_DVB
}

#define SISCAN_DEBUG
int GetDVBTID(uint16_t NetworkID, uint16_t TransportID, int CurrentMplexId)
{
#ifdef SISCAN_DEBUG
    printf("Request for Networkid/Transportid %d %d\n",NetworkID,TransportID);
#endif

    MSqlQuery query(MSqlQuery::InitCon());
    QString theQuery;
    query.prepare(
        QString("SELECT networkid, transportid FROM dtv_multiplex "
                "WHERE mplexid = %1").arg(CurrentMplexId));

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Getting mplexid global search", query);


    if (query.size() >= 1)
    {
        query.next();

#ifdef SISCAN_DEBUG
        printf("Query Results: %d %d\n",
               query.value(0).toInt(), query.value(1).toInt());
#endif

        // See if you can get an exact match based on the current 
        // mplexid's sourceID and the NetworkID/TransportID

        if ((query.value(0).toInt() == NetworkID) &&
            (query.value(1).toInt() == TransportID))
        {
            return CurrentMplexId;
        }

        // Then see if current one is NULL.
        // If so, update those values and return current mplexid

        if ((query.value(0).toInt() == 0) && (query.value(1).toInt() == 0))
        {
            query.prepare(
                QString("UPDATE dtv_multiplex SET networkid=%1, transportid=%2 "
                        "WHERE mplexid = %3")
                .arg(NetworkID).arg(TransportID).arg(CurrentMplexId));

            if (!query.exec() || !query.isActive())
                MythContext::DBError("Getting mplexid global search", query);

            return CurrentMplexId;
        }
    }

    // Values were set, so see where you can find this NetworkID/TransportID
    query.prepare(
        QString("SELECT a.mplexid "
                "FROM dtv_multiplex a, dtv_multiplex b "
                "WHERE a.sourceid=b.sourceID AND a.networkid='%1' AND "
                "      a.transportid='%2'    AND a.mplexid='%3'")
        .arg(NetworkID).arg(TransportID).arg(CurrentMplexId));

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Finding matching mplexid", query);

    // If you got an exact match just return it, since there is no question what
    // mplexid this NetworkId/TransportId is for.
    if (query.size() == 1)
    {
#ifdef SISCAN_DEBUG
        printf("Exact Match!\n");
#endif
        query.next();
        int tmp = query.value(0).toInt();
        return tmp;
    }

    // If you got more than 1 hit then all you can do is hope that 
    // this NetworkID/TransportID pair belongs to the CurrentMplexId;
    if (query.size() > 1)
    {
#ifdef SISCAN_DEBUG
        printf("more than 1 hit\n");
#endif
        return CurrentMplexId;
    }

    // If you got here you didn't find any match at all..
    // Try to see if this N/T pair is on a transport that we know about..
    // Search without the SourceID restriction..
    query.prepare(
        QString("SELECT mplexid FROM dtv_multiplex "
                "WHERE networkid = %1 AND transportid = %2")
        .arg(NetworkID).arg(TransportID));
        
    if (!query.exec() || !query.isActive())
        MythContext::DBError("Getting mplexid global search", query);

    // If you still didn't find this combo return -1 (failure)
    if (query.size() <= 0)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Error: Found no match for "
                        "NetworkID %1 TransportID %2")
                .arg(NetworkID).arg(TransportID));
        return -1;
    }

    query.next();

    // Now you are in trouble.. You found more than 1 match, 
    // so just guess that its the first entry in the database..
    if (query.size() > 1)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Warning: Found more than one match for "
                        "NetworkID %1 TransportID %2")
                .arg(NetworkID).arg(TransportID));
        int tmp = query.value(0).toInt();
        return tmp;
    }

    // You found the entries.
    // But it was on a different sourceID, so return that value.
    int retval = query.value(0).toInt();

    return retval;
}

// ///////////////////// Static DB helper methods /////////////////////
// ///////////////////// Static DB helper methods /////////////////////
// ///////////////////// Static DB helper methods /////////////////////
// ///////////////////// Static DB helper methods /////////////////////
// ///////////////////// Static DB helper methods /////////////////////

static bool ignore_encrypted_services(int db_mplexid, QString videodevice)
{
    MSqlQuery query(MSqlQuery::InitCon());
    // Get the freetoaironly field from cardinput
    QString FTAQuery = QString(
        "SELECT freetoaironly from cardinput, dtv_multiplex, capturecard "
        "WHERE cardinput.sourceid = dtv_multiplex.sourceid AND "
        "      cardinput.cardid = capturecard.cardid AND "
        "      mplexid=%1 AND videodevice = '%2'")
        .arg(db_mplexid).arg(videodevice);
    query.prepare(FTAQuery);

    if (!query.exec())
        MythContext::DBError("Getting FreeToAir for cardinput", query);

    if (query.size() > 0)
    {
        query.next();
        return query.value(0).toBool();
    }
    return true;
}

void delete_services(int db_mplexid, QMap_SDTObject SDT)
{
    (void) db_mplexid;
    (void) SDT;
#ifdef USING_DVB
    MSqlQuery query(MSqlQuery::InitCon());
    // Clear out entries in the channel table that don't exist anymore
    QString deleteQuery = "DELETE FROM channel WHERE NOT (";
    for (QMap_SDTObject::Iterator s = SDT.begin(); s != SDT.end(); ++s)
    {
        deleteQuery += (s == SDT.begin()) ? "(" : " OR (";

        if ( (*s).ChanNum != -1)
            deleteQuery += QString("channum = \"%1\" AND ").arg((*s).ChanNum);

        deleteQuery += QString("callsign = '%1' AND serviceid=%2 AND atscsrcid=%3) ")
                       .arg((*s).ServiceName.utf8())
                       .arg((*s).ServiceID)
                       .arg((*s).ATSCSourceID);
    }
    deleteQuery += QString(") AND mplexid = %1").arg(db_mplexid);

    query.prepare(deleteQuery);

    if (!query.exec() || !query.isActive())
        MythContext::DBError("Deleting non-existant channels", query);
#endif // USING_DVB
}

#ifdef USING_DVB
static int create_dtv_multiplex_ofdm(const TransportScanItem& item,
                                     const DVBTuning& tuning)
{
    return ChannelUtil::CreateMultiplex(
        item.SourceID,              item.standard,
        tuning.Frequency(),         tuning.ModulationDB(),
        -1,                         -1,
        true,
        -1,                         tuning.BandwidthChar(),
        -1,                         tuning.InversionChar(),
        tuning.TransmissionModeChar(),
        QString::null,              tuning.ConstellationDB(),
        tuning.HierarchyChar(),     tuning.HPCodeRateString(),
        tuning.LPCodeRateString(),  tuning.GuardIntervalString());
}
#endif // USING_DVB
