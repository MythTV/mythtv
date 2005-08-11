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

#ifdef USING_DVB
#include "dvbchannel.h"
#include "dvbtypes.h"
#endif // USING_DVB

#ifdef USE_SIPARSER
#include "dvbsiparser.h"
#endif // USE_SIPARSER

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
void delete_services(int db_mplexid, const ServiceDescriptionTable*);
#endif // USING_DVB

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
      scanner_thread_running(false)
{
    // Initialize statics
    init_freq_tables();

#ifdef USE_SIPARSER
    /* setup boolean values for thread */
    serviceListReady = false;
    transportListReady = false;

    siparser = NULL;
    if (GetDVBChannel())
    {
        VERBOSE(VB_SIPARSER, "Creating SIParser");
        siparser = new DVBSIParser(GetDVBChannel()->GetCardNum());
        pthread_create(&siparser_thread, NULL, SpawnSectionReader, siparser);
        connect(siparser,        SIGNAL(UpdatePMT(const PMTObject*)),
                GetDVBChannel(), SLOT(SetPMT(const PMTObject*)));
    }    
    if (siparser)
    {
        VERBOSE(VB_IMPORTANT, "Connecting up SIParser");
        /* Signals to process tables and do database inserts */
        connect(siparser, SIGNAL(FindTransportsComplete()),
                this,     SLOT(TransportTableComplete()));
        connect(siparser, SIGNAL(FindServicesComplete()),
                this,     SLOT(ServiceTableComplete()));
        return;
    }
#endif // USE_SIPARSER

    // Create a stream data for digital signal monitors
    DTVSignalMonitor* dtvSigMon = GetDTVSignalMonitor();
    if (dtvSigMon)
    {
        VERBOSE(VB_SIPARSER, "Connecting up DTVSignalMonitor");
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

#ifdef USE_SIPARSER
void *SIScan::SpawnSectionReader(void *param)
{
    (void) param;
    DVBSIParser *siparser = (DVBSIParser*) param;
    siparser->StartSectionReader();
    return NULL;
}
#endif // USE_SIPARSER

bool SIScan::ScanTransports(const QString _sistandard)
{
    (void) _sistandard;
    VERBOSE(VB_SIPARSER, "ScanTransports()");
#ifdef USE_SIPARSER
    if (siparser)
    {
        siparser->FillPMap(_sistandard);
        return siparser->FindTransports();
    }
#endif // USE_SIPARSER
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

    scanIt = scanTransports.end();

    MSqlQuery query(MSqlQuery::InitCon());
    // Run DB query to get transports on sourceid SourceID
    // connected to this card
    QString theQuery = QString(
        "SELECT sourceid, mplexid, sistandard, transportid "
        "FROM dtv_multiplex WHERE sourceid = %1").arg(SourceID);
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

    scanTimeout = DVB_TABLES_TIMEOUT;
    while (query.next())
    {
        int sourceid = query.value(0).toInt();
        int mplexid  = query.value(1).toInt();
        QString std  = query.value(2).toString();
        int tsid     = query.value(3).toInt();
        scanTimeout = (std == "atsc") ?
            ATSC_TABLES_TIMEOUT : DVB_TABLES_TIMEOUT;

        QString fn = (tsid) ? QString("Transport ID %1").arg(tsid) :
            QString("Multiplex #%1").arg(mplexid);

        SISCAN("Adding "+fn);

        TransportScanItem item(sourceid, std, fn, mplexid);
        scanTransports += item;
    }

    timer = QTime();
    waitingForTables  = false;

    transportsScanned = 0;
    scanOffsetIt      = 0;
    if (scanTransports.size())
    {
        scanIt        = scanTransports.begin();
        scanMode      = TRANSPORT_LIST;
        return true;
    }

    return false;
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
#ifdef USE_SIPARSER
    // Requests the SDT table for the current transport from sections
    if (siparser)
        return siparser->FindServices();
#endif // USE_SIPARSER
#ifdef USING_DVB
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
#ifdef USE_SIPARSER
        HandleSIParserEvents();
#endif // USE_SIPARSER

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
    else
    {
        if (timer.elapsed() > (int)(*scanIt).timeoutTune)
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

    // Check if we got some tables, but not all...
#ifdef USE_SIPARSER
    if (!siparser && GetDTVSignalMonitor())
#else // if !USE_SIPARSER
    if (GetDTVSignalMonitor())
#endif // !USE_SIPARSER
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
            // Stop signal monitor for previous transport
            signalMonitor->Stop();

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

    emit ServiceScanUpdateStatusText(cur_chan);
    SISCAN(tune_msg_str);

    bool ok = false;
#ifdef USING_DVB
    // Tune to multiplex
    if (GetDVBChannel())
    {
        if (item.mplexid >= 0)
        {
#ifdef USE_SIPARSER
            if (siparser)
                siparser->Reset();
#endif // USE_SIPARSER

            ok = GetDVBChannel()->TuneMultiplex(item.mplexid);

#ifdef USE_SIPARSER
            if (ok && siparser)
                siparser->FillPMap(item.standard);
#endif // USE_SIPARSER
        }
        else
        {
            dvb_channel_t dvbchan;
            dvbchan.sistandard = item.standard;
            dvbchan.tuning     = item.tuning;
            dvbchan.tuning.params.frequency = item.freq_offset(scanOffsetIt);

#ifdef USE_SIPARSER
            if (siparser)
                siparser->Reset();
#endif // USE_SIPARSER

            ok = GetDVBChannel()->Tune(dvbchan, true);

#ifdef USE_SIPARSER
            if (ok && siparser)
                siparser->FillPMap(item.standard);
#endif // USE_SIPARSER

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
    signalMonitor->Start();

#ifdef USE_SIPARSER
    if (siparser)
        siparser->FindServices();
#endif // USE_SIPARSER

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

#ifdef USE_SIPARSER
    /* Force dvbchannel to exit */
    if (siparser)
    {
        siparser->StopSectionReader();
        pthread_join(siparser_thread, NULL);
        delete siparser;
    }
#endif // USE_SIPARSER

    if (scanner_thread_running)
        pthread_join(scanner_thread, NULL);
    if (signalMonitor)
        signalMonitor->Stop();
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
        int sourceid = query.value(0).toInt();
        int mplexid  = query.value(1).toInt();
        QString std  = query.value(2).toString();
        int tsid     = query.value(3).toInt();
        scanTimeout = (std == "atsc") ?
            ATSC_TABLES_TIMEOUT : DVB_TABLES_TIMEOUT;

        QString fn = (tsid) ? QString("Transport ID %1").arg(tsid) :
            QString("Multiplex #%1").arg(mplexid);
        
        SISCAN("Adding "+fn);

        TransportScanItem item(sourceid, std, fn, mplexid);
        scanTransports += item;
    }

    timer = QTime();
    waitingForTables  = false;

    transportsScanned = 0;
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
#ifdef USE_SIPARSER
    serviceListReady = true;
#endif // USE_SIPARSER
}

void SIScan::TransportTableComplete()
{
#ifdef USE_SIPARSER
    transportListReady = true;
#endif // USE_SIPARSER
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

    VERBOSE(VB_SIPARSER, QString("UpdateVCTinDB(): mplex: %1:%2")
            .arg(tid_db).arg((*scanIt).mplexid));
    int db_mplexid = ChannelUtil::GetBetterMplexID(
        tid_db, vct->TransportStreamID(), 1);

    if (db_mplexid == -1)
    {
        VERBOSE(VB_IMPORTANT, "VCT: Error determing what transport this "
                "service table is associated with so failing");
        return;
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
void SIScan::UpdateSDTinDB(int tid_db, const ServiceDescriptionTable *sdt,
                           bool force_update)
{
    if (!sdt->ServiceCount())
        return;

    int db_mplexid = ChannelUtil::GetBetterMplexID(
        tid_db, sdt->TSID(), sdt->OriginalNetworkID());

    if (db_mplexid == -1)
    {
        VERBOSE(VB_IMPORTANT, "SDT: Error determing what transport this "
                "service table is associated with so failing");
        return;
    }

    //bool fta_only = ignore_encrypted_services(db_mplexid, videodevice);
    int db_source_id   = ChannelUtil::GetSourceID(db_mplexid);

    /* This will be fixed post .17 to be more elegant */
    bool upToDate = ChannelUtil::GetServiceVersion(db_mplexid) == (int)sdt->Version();
    if (upToDate && !force_update)
    {
        emit ServiceScanUpdateText("Channels up to date");
        return;
    }
    if (!upToDate)
        ChannelUtil::SetServiceVersion(db_mplexid, sdt->Version());

    for (uint i = 0; i < sdt->ServiceCount(); i++)
    {
        // Figure out best service name...
        ServiceDescriptor *desc = sdt->GetServiceDescriptor(i);
        QString service_name = "";
        if (desc)
            service_name = desc->ServiceName();

        if (service_name.stripWhiteSpace().isEmpty())
            service_name = QString("%1 %2")
                .arg(sdt->ServiceID(i)).arg(db_mplexid);

        // Figure out best channel number
        QString chan_num = QString::number(sdt->ServiceID(i));
        bool have_uk_chan_num =
            dvbChanNums.find(sdt->ServiceID(i)) != dvbChanNums.end();
        if (have_uk_chan_num)
            chan_num = QString::number(dvbChanNums[sdt->ServiceID(i)]);

        // Skip to next if this is a service we don't care for
        if (desc && desc->IsDigitalAudio() && ignoreAudioOnlyServices)
        {
            emit ServiceScanUpdateText(
                tr("Skipping %1 - Radio Service").arg(service_name));
            continue;
        }
        else if (desc && !desc->IsDTV() && !desc->IsDigitalAudio())
        {
            emit ServiceScanUpdateText(tr("Skipping %1 - not a Television or "
                                          "Radio Service").arg(service_name));
            continue;
        }
        else if (ignoreEncryptedServices && !sdt->HasFreeCA(i))
        {
            emit ServiceScanUpdateText(tr("Skipping %1 - Encrypted Service")
                .arg(service_name));
            continue;
        }

        // See if service already in database based on service ID
        int chanid = ChannelUtil::GetChanID(db_mplexid, -1, 0, 0,
                                            sdt->ServiceID(i));

        if (chanid < 0)
        {   // The service is not in database, add it
            emit ServiceScanUpdateText(tr("Adding %1").arg(service_name));
            chanid = ChannelUtil::CreateChanID(db_source_id, chan_num);
            if (chanid > 0)
            {
                ChannelUtil::CreateChannel(
                    db_mplexid, db_source_id, chanid,
                    service_name,
                    chan_num,
                    sdt->ServiceID(i),
                    0, 0,
                    sdt->HasEITSchedule(i) || sdt->HasEITPresentFollowing(i),
                    false, false, -1);
            }
        }
        else if (force_update || (desc && have_uk_chan_num))
        {   // The service is in database & we have good info, update it
            emit ServiceScanUpdateText(tr("Updating %1").arg(service_name));

            ChannelUtil::UpdateChannel(
                db_mplexid,
                db_source_id,
                chanid,
                service_name,
                chan_num,
                sdt->ServiceID(i),
                0, 0, 
                -1);
        }
        else
        {
            emit ServiceScanUpdateText(
                tr("Skipping %1 - already in DB, and we don't have better data.")
                .arg(service_name));
        }

        if (desc)
            delete desc;
    }
}

#ifdef USE_SIPARSER
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
    (void) tid_db;
    (void) SDT;

    if (SDT.empty())
    {
        SISCAN("SDT List Empty assuming this is by error leaving services intact");
        return;
    }

    QMap_SDTObject::Iterator s = SDT.begin();

    int db_mplexid = ChannelUtil::GetBetterMplexID(
        tid_db, (*s).TransportID, ((*s).ATSCSourceID) ? 1 : (*s).NetworkID);

    if (db_mplexid == -1)
    {
        VERBOSE(VB_IMPORTANT, "SRV: Error determing what transport this "
                "service table is associated with so failing");
        return;
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
}
#endif // USE_SIPARSER

#ifdef USE_SIPARSER
void SIScan::CheckNIT(NITObject& NIT)
{
    (void) NIT;
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

#if 0 // disable for now
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
#endif
    }
}
#endif // USE_SIPARSER

#ifdef USE_SIPARSER
void SIScan::UpdateTransportsInDB(NITObject NIT)
{
    (void) NIT;
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
}
#endif // USE_SIPARSER

#ifdef USE_SIPARSER
void SIScan::HandleSIParserEvents(void)
{
    if (!siparser)
        return;

    int mplex = -1;
    if (scanTransports.size() && scanIt != scanTransports.end() &&
        (*scanIt).mplexid > 0)
        mplex = (*scanIt).mplexid;
    else if (GetDVBChannel() && GetDVBChannel()->GetMultiplexID() > 0)
        mplex = GetDVBChannel()->GetMultiplexID();

    if (serviceListReady && (mplex > 0))
    {
        if (siparser->GetServiceObject(SDT))
        {
            UpdateServicesInDB(mplex, SDT);
            scanOffsetIt = 99;
        }
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
    if (transportListReady)
    {
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
        emit TransportScanComplete();
    }
}
#endif // USE_SIPARSER

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
