// -*- Mode: c++ -*-

// C includes
#include <cstdio>
#include <pthread.h>
#include <unistd.h>

// Qt includes
#include <qmutex.h>

// MythTV includes - General
#include "siscan.h"
#include "scheduledrecording.h"
#include "frequencies.h"
#include "mythdbcon.h"
#include "channelbase.h"
#include "channelutil.h"
#include "cardutil.h"

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

#define SIPARSER_EXTRA_TIME 2000 /**< milliseconds to add for SIParser scan */

#ifdef USING_DVB
#ifdef USE_SIPARSER
static bool ignore_encrypted_services(int db_mplexid, QString videodevice);
#endif
static int  create_dtv_multiplex_ofdm(
    const TransportScanItem& item, const DVBTuning& tuning);
void delete_services(int db_mplexid, const ServiceDescriptionTable*);
#endif // USING_DVB

/** \class SIScan
 *  \brief Scanning class for cards that support a SignalMonitor class.
 *
 *   Currently both SIParser and ScanStreamData are being used in
 *   this class. The SIParser is being phased out, so that is not
 *   described here.
 *
 *   With ScanStreamData, we call ScanTransport() on each transport 
 *   and frequency offset in the list of transports. This list is 
 *   created from a FrequencyTable object.
 *
 *   Each ScanTransport() call resets the ScanStreamData and the
 *   SignalMonitor, then tunes to a new frequency and notes the tuning
 *   time in the "timer" QTime object.
 *
 *   HandleActiveScan is called every time through the event loop
 *   and is what calls ScanTransport(), as well as checking when
 *   the current time is "timeoutTune" or "channelTimeout" milliseconds
 *   ahead of "timer". When the "timeoutTune" is exceeded we check
 *   to see if we have a signal lock on the channel, if we don't we
 *   check the next transport. When the larger "channelTimeout" is
 *   exceeded we do nothing unless "waitingForTables" is still true,
 *   if so we check if we at least got a PAT and if so we insert
 *    a channel based on that by calling HandleMPEGDBInsertion().
 *
 *   Meanwhile the ScanStreamData() emits several signals. For 
 *   the UI it emits signal quality signals. For SIScan it emits
 *   UpdateMGT, UpdateVCT, UpdateNIT, and UpdateSDT signals. We
 *   connect these to the HandleMGT, HandleVCT, etc. These in
 *   turn just call HandleATSCDBInsertion() or
 *   HandleDVBDBInsertion() depending on the type of table.
 *
 *   HandleATSCDBInsertion() first checks if we have all the VCTs 
 *   described in the MGT. If we do we call UpdateVCTinDB() for each
 *   TVCT and CVCT in the stream. UpdateVCTinDB() inserts the actual
 *   channels. Then we set "waitingForTables" to false, set the
 *   scanOffsetIt to 99 and updates the UI to reflect the added channel.
 *   HandleDVBDBInsertion() and HandleMPEGDBInsertion() are similar.
 */

/** \fn SIScan::SIScan(QString,ChannelBase*,int,uint,uint)
 */
SIScan::SIScan(QString _cardtype, ChannelBase* _channel, int _sourceID,
               uint signal_timeout, uint channel_timeout)
    : // Set in constructor
      channel(_channel),
      signalMonitor(SignalMonitor::Init(_cardtype, -1, _channel)),
      sourceID(_sourceID),
      scanMode(IDLE),
      signalTimeout(signal_timeout),
      channelTimeout(channel_timeout),
      // Settable
      ignoreAudioOnlyServices(false),
      ignoreDataServices(false),
      ignoreEncryptedServices(false),
      forceUpdate(false),
      renameChannels(false),
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
    current = scanTransports.end();

#ifdef USE_SIPARSER
    siparser = NULL;
    if (_cardtype != "HDTV" && _cardtype != "ATSC")
    {
        /* setup boolean values for thread */
        serviceListReady = false;
        transportListReady = false;

        if (GetDVBChannel())
        {
            SISCAN("Creating SIParser");
            siparser = new DVBSIParser(GetDVBChannel()->GetCardNum());
            pthread_create(&siparser_thread, NULL, SpawnSectionReader, siparser);
            connect(siparser,
                    SIGNAL(UpdatePMT(uint, const ProgramMapTable*)),
                    GetDVBChannel(),
                    SLOT(  SetPMT(   uint, const ProgramMapTable*)));
        }    
        if (siparser)
        {
            SISCAN("Connecting up SIParser");
            /* Signals to process tables and do database inserts */
            connect(siparser, SIGNAL(FindTransportsComplete()),
                    this,     SLOT(TransportTableComplete()));
            connect(siparser, SIGNAL(FindServicesComplete()),
                    this,     SLOT(ServiceTableComplete()));
            channelTimeout += SIPARSER_EXTRA_TIME;
            return;
        }
    }
#endif // USE_SIPARSER

    // Create a stream data for digital signal monitors
    DTVSignalMonitor* dtvSigMon = GetDTVSignalMonitor();
    if (dtvSigMon)
    {
        SISCAN("Connecting up DTVSignalMonitor");
        MPEGStreamData *data = new ScanStreamData();

        dtvSigMon->SetStreamData(data);
        dtvSigMon->AddFlags(kDTVSigMon_WaitForMGT | kDTVSigMon_WaitForVCT |
                            kDTVSigMon_WaitForNIT | kDTVSigMon_WaitForSDT);
        connect(data, SIGNAL(UpdatePAT(const ProgramAssociationTable*)),
                this, SLOT(  HandlePAT(const ProgramAssociationTable*)));
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
    SISCAN("ScanTransports()");
#ifdef USE_SIPARSER
    if (siparser)
    {
        siparser->SetTableStandard(_sistandard);
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

    nextIt = scanTransports.end();

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

    while (query.next())
    {
        int sourceid = query.value(0).toInt();
        int mplexid  = query.value(1).toInt();
        QString std  = query.value(2).toString();
        int tsid     = query.value(3).toInt();

        QString fn = (tsid) ? QString("Transport ID %1").arg(tsid) :
            QString("Multiplex #%1").arg(mplexid);

        SISCAN("Adding "+fn);

        TransportScanItem item(sourceid, std, fn, mplexid, signalTimeout);
        scanTransports += item;
    }

    waitingForTables  = false;
    transportsScanned = 0;
    if (scanTransports.size())
    {
        nextIt        = scanTransports.begin();
        scanMode      = TRANSPORT_LIST;
        return true;
    }

    return false;
}

void SIScan::HandlePAT(const ProgramAssociationTable *pat)
{
    SISCAN(QString("Got a Program Association Table for %1")
           .arg((*current).FriendlyName));
    // Add pmts to list, so we can do MPEG scan properly.
    ScanStreamData *sd = GetDTVSignalMonitor()->GetScanStreamData();
    for (uint i = 0; i < pat->ProgramCount(); i++)
        sd->AddListeningPID(pat->ProgramPID(i));
}

void SIScan::HandleVCT(uint, const VirtualChannelTable*)
{
    SISCAN(QString("Got a Virtual Channel Table for %1")
           .arg((*current).FriendlyName));
    HandleATSCDBInsertion(GetDTVSignalMonitor()->GetScanStreamData(), true);
}

void SIScan::HandleMGT(const MasterGuideTable*)
{
    SISCAN(QString("Got the Master Guide for %1").arg((*current).FriendlyName));
    HandleATSCDBInsertion(GetDTVSignalMonitor()->GetScanStreamData(), true);
}

void SIScan::HandleSDT(uint, const ServiceDescriptionTable*)
{
    SISCAN(QString("Got a Service Description Table for %1")
           .arg((*current).FriendlyName));
    HandleDVBDBInsertion(GetDTVSignalMonitor()->GetScanStreamData(), true);
}

void SIScan::HandleNIT(const NetworkInformationTable *nit)
{
    SISCAN(QString("Got a Network Information Table for %1")
           .arg((*current).FriendlyName));

    dvbChanNums.clear();

    //emit TransportScanUpdateText(tr("Optimizing transport frequency"));
    //OptimizeNITFrequencies(&nit);

    if (nit->TransportStreamCount())
    {
        emit TransportScanUpdateText(
            tr("Network %1 Processing").arg(nit->NetworkName()));

        vector<uint> mp;
        mp = ChannelUtil::CreateMultiplexes(sourceID, nit);
        VERBOSE(VB_SIPARSER, QString("Created %1 multiplexes from NIT")
                .arg(mp.size()));

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

    const ScanStreamData *sd = GetDTVSignalMonitor()->GetScanStreamData();
    const DVBStreamData &dsd = *sd;
    if (dsd.HasAllNITSections())
    {
        emit TransportScanUpdateText(tr("Finished processing Transport List"));
        emit TransportScanComplete();
    }
}

void SIScan::HandleMPEGDBInsertion(const ScanStreamData *sd, bool)
{
    if ((*current).mplexid <= 0)
        (*current).mplexid = InsertMultiplex(current);

    const ProgramAssociationTable *pat = sd->GetCachedPAT();
    if (pat)
    {
        pmt_map_t pmt_map = sd->GetCachedPMTMap();
        UpdatePATinDB((*current).mplexid, pat, pmt_map, true);
        sd->ReturnCachedTables(pmt_map);
        sd->ReturnCachedTable(pat);
    }

    // tell UI we are done with these channels
    if (scanMode == TRANSPORT_LIST)
    {
        UpdateScanPercentCompleted();
        waitingForTables = false;
        nextIt = current.nextTransport();
    }
}

void SIScan::HandleATSCDBInsertion(const ScanStreamData *sd,
                                   bool wait_until_complete)
{
    if (wait_until_complete && !sd->HasCachedAllVCTs())
        return;

    if ((*current).mplexid <= 0)
        (*current).mplexid = InsertMultiplex(current);

    // Insert Terrestrial VCTs
    tvct_vec_t tvcts = sd->GetAllCachedTVCTs();
    for (uint i = 0; i < tvcts.size(); i++)
        UpdateVCTinDB((*current).mplexid, tvcts[i], true);
    sd->ReturnCachedTVCTTables(tvcts);

    // Insert Cable VCTs
    cvct_vec_t cvcts = sd->GetAllCachedCVCTs();
    for (uint i = 0; i < cvcts.size(); i++)
        UpdateVCTinDB((*current).mplexid, cvcts[i], true);
    sd->ReturnCachedCVCTTables(cvcts);

    // tell UI we are done with these channels
    if (scanMode == TRANSPORT_LIST)
    {
        UpdateScanPercentCompleted();
        waitingForTables = false;
        nextIt = current.nextTransport();
    }
}

void SIScan::HandleDVBDBInsertion(const ScanStreamData *sd,
                                  bool wait_until_complete)
{
    const DVBStreamData &dsd = (const DVBStreamData &)(*sd);
    if (wait_until_complete && !dsd.HasCachedSDT())
        return;

    emit ServiceScanUpdateText(tr("Updating Services"));

    if ((*current).mplexid <= 0)
        (*current).mplexid = InsertMultiplex(current);

    vector<const ServiceDescriptionTable*> sdts = sd->GetAllCachedSDTs();
    for (uint i = 0; i < sdts.size(); i++)
        UpdateSDTinDB((*current).mplexid, sdts[i], forceUpdate);
    sd->ReturnCachedSDTTables(sdts);

    emit ServiceScanUpdateText(tr("Finished processing Services"));

    if (scanMode == TRANSPORT_LIST)
    {
        UpdateScanPercentCompleted();
        waitingForTables = false;
        nextIt = current.nextTransport();
    }
    else
    {
        emit PctServiceScanComplete(100);
        emit ServiceScanComplete();
    }    
}

/** \fn SIScan::HandlePostInsertion()
 *  \brief Insert channels based on any partial tables we do have.
 *  \return true if we insterted any channels.
 */
bool SIScan::HandlePostInsertion(void)
{
    DTVSignalMonitor* dtvSigMon = GetDTVSignalMonitor();
    if (!dtvSigMon)
        return false;

    const ScanStreamData *sd = dtvSigMon->GetScanStreamData();

    SISCAN(QString("HandlePostInsertion() pat(%1)")
            .arg(sd->HasCachedPAT()));

    // TODO insert ATSC channels based on partial info
    const MasterGuideTable *mgt = sd->GetCachedMGT();
    if (mgt)
    {
        VERBOSE(VB_IMPORTANT, mgt->toString());
        sd->ReturnCachedTable(mgt);
    }

    const NetworkInformationTable *nit = sd->GetCachedNIT();
    if (nit)
    {
        VERBOSE(VB_IMPORTANT, nit->toString());
        HandleDVBDBInsertion(sd, false);
        sd->ReturnCachedTable(nit);
        return true;
    }

    const ProgramAssociationTable *pat = sd->GetCachedPAT();
    if (pat)
    {
        VERBOSE(VB_IMPORTANT, pat->toString());
        HandleMPEGDBInsertion(sd, false);
        sd->ReturnCachedTable(pat);
        return true;
    }
    return false;
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
    SISCAN("SIScan::ScanServices()");
#ifdef USE_SIPARSER
    // Requests the SDT table for the current transport from sections
    if (siparser)
        return siparser->FindServices();
#endif // USE_SIPARSER
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

// See if we have timed out
bool SIScan::HasTimedOut(void)
{
    if (!waitingForTables)
        return true;

    QString offset_str = current.offset() ?
        QObject::tr(" offset %2").arg(current.offset()) : "";
    QString cur_chan = QString("%1%2")
        .arg((*current).FriendlyName).arg(offset_str);
    QString time_out_table_str =
        QObject::tr("Timeout Scanning %1 -- no tables").arg(cur_chan);
    QString time_out_sig_str =
        QObject::tr("Timeout Scanning %1 -- no signal").arg(cur_chan);

    // have the tables have timed out?
    if (timer.elapsed() > (int)channelTimeout)
    { 
        emit ServiceScanUpdateText(time_out_table_str);
        SISCAN(time_out_table_str);
        return true;
    }

    // ok the tables haven't timed out, but have we hit the signal timeout?
    if (timer.elapsed() > (int)(*current).timeoutTune)
    {
        // If we don't have a signal in timeoutTune msec, continue..
        SignalMonitor *sm = GetSignalMonitor();
        if (NULL == sm || sm->HasSignalLock())
            return false;

        emit ServiceScanUpdateText(time_out_sig_str);
        SISCAN(time_out_sig_str);

        return true;
    }
    return false;
}

/** \fn SIScan::HandleActiveScan(void)
 *  \brief Handles the TRANSPORT_LIST SIScan mode.
 */
void SIScan::HandleActiveScan(void)
{
    bool do_post_insertion = waitingForTables;
#ifdef USE_SIPARSER
    do_post_insertion &= (NULL == siparser);
#endif // USE_SIPARSER

    if (!HasTimedOut())
        return;
    
    if (0 == nextIt.offset() && nextIt != scanTransports.begin())
    {
        // Stop signal monitor for previous transport
        signalMonitor->Stop();

        if (do_post_insertion)
            HandlePostInsertion();

        transportsScanned++;
        UpdateScanPercentCompleted();
    }

    current = nextIt; // Increment current

    if (current != scanTransports.end())
    {
        ScanTransport(current);

        // Increment nextIt
        nextIt = current;
        ++nextIt;
    }
    else
    {
        emit ServiceScanComplete();
        scanMode = IDLE;
        scanTransports.clear();
        current = nextIt = scanTransports.end();
    }
}

#ifdef USE_SIPARSER
#define SIP_RESET() do { if (siparser) siparser->Reset(); } while (false)
#define SIP_PMAP() do { if (ok && siparser) \
                   siparser->SetTableStandard(item.standard); } while (false)
#else // if !USE_SIPARSER
#define SIP_RESET()
#define SIP_PMAP()
#endif // !USE_SIPARSER

bool SIScan::Tune(const transport_scan_items_it_t transport)
{
    const TransportScanItem &item = *transport;
    const uint freq = item.freq_offset(transport.offset());
    bool ok = false;

#ifdef USING_DVB
    // Tune to multiplex
    if (GetDVBChannel())
    {
        if (item.mplexid > 0)
        {
            SIP_RESET();
            ok = GetDVBChannel()->TuneMultiplex(item.mplexid);
            SIP_PMAP();
        }
        else
        {
            dvb_channel_t dvbchan;
            dvbchan.sistandard = item.standard;
            dvbchan.tuning     = item.tuning;
            dvbchan.tuning.params.frequency = freq;

            SIP_RESET();
            ok = GetDVBChannel()->Tune(dvbchan, true);
            SIP_PMAP();
        }
    }
#endif // USING_DVB

#ifdef USING_V4L
    if (GetChannel())
    {
        if (item.mplexid > 0)
            ok = GetChannel()->TuneMultiplex(item.mplexid);
        else 
        {
            const uint freq_vis = freq - 1750000; // to visual carrier
            QString inputname = ChannelUtil::GetInputName(item.SourceID);
            ok = GetChannel()->Tune(freq_vis, inputname, "8vsb");
        }
    }
#endif // USING_V4L

    return ok;
}

#undef SIP_RESET
#undef SIP_PMAP

void SIScan::ScanTransport(const transport_scan_items_it_t transport)
{
    QString offset_str = (transport.offset()) ?
        QObject::tr(" offset %2").arg(transport.offset()) : "";
    QString cur_chan = QString("%1%2")
        .arg((*current).FriendlyName).arg(offset_str);
    QString tune_msg_str =
        QObject::tr("Tuning to %1 mplexid(%2)")
        .arg(cur_chan).arg((*current).mplexid);

    const TransportScanItem &item = *transport;

    if (transport.offset() && 
        (item.freq_offset(transport.offset()) == item.freq_offset(0)))
    {
        waitingForTables = false;
        return; // nothing to do
    }

    emit ServiceScanUpdateStatusText(cur_chan);
    SISCAN(tune_msg_str);

    if (!Tune(transport))
    {   // If we did not tune successfully, bail with message
        UpdateScanPercentCompleted();
        SISCAN(QString("Failed to tune %1 mplexid(%2) at offset %3")
               .arg(item.FriendlyName).arg(item.mplexid).arg(transport.offset()));
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
    if (scanner_thread_running)
        pthread_join(scanner_thread, NULL);

#ifdef USE_SIPARSER
    /* Force dvbchannel to exit */
    if (siparser)
    {
        siparser->StopSectionReader();
        pthread_join(siparser_thread, NULL);
        delete siparser;
    }
#endif // USE_SIPARSER

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
    nextIt = scanTransports.end();

    freq_table_list_t tables = get_matching_freq_tables(std, modulation, country);
    SISCAN(QString("Looked up freq table (%1, %2, %3)")
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

            TransportScanItem item(SourceID, si_std, name, name_num,
                                   freq, ft, signalTimeout);
            scanTransports += item;

            SISCAN(item.toString());

            name_num++;
            freq += ft.frequencyStep;
        }
    }

    timer.start();
    waitingForTables = false;

    nextIt            = scanTransports.begin();
    transportsScanned = 0;
    scanMode          = TRANSPORT_LIST;

    return true;
}

bool SIScan::ScanTransport(int mplexid)
{
    scanTransports.clear();
    nextIt = scanTransports.end();

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

    while (query.next())
    {
        int sourceid = query.value(0).toInt();
        int mplexid  = query.value(1).toInt();
        QString std  = query.value(2).toString();
        int tsid     = query.value(3).toInt();

        QString fn = (tsid) ? QString("Transport ID %1").arg(tsid) :
            QString("Multiplex #%1").arg(mplexid);
        
        SISCAN("Adding "+fn);

        TransportScanItem item(sourceid, std, fn, mplexid, signalTimeout);
        scanTransports += item;
    }

    timer.start();
    waitingForTables  = false;

    transportsScanned = 0;
    if (scanTransports.size())
    {
        nextIt        = scanTransports.begin();
        scanMode      = TRANSPORT_LIST;
        return true;
    }

    return false;
}

void SIScan::ServiceTableComplete()
{
#ifdef USE_SIPARSER
    SISCAN("ServiceTableComplete()");
    serviceListReady = true;
#endif // USE_SIPARSER
}

void SIScan::TransportTableComplete()
{
#ifdef USE_SIPARSER
    SISCAN("TransportTableComplete()");
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

// ///////////////////// DB STUFF /////////////////////
// ///////////////////// DB STUFF /////////////////////
// ///////////////////// DB STUFF /////////////////////
// ///////////////////// DB STUFF /////////////////////
// ///////////////////// DB STUFF /////////////////////

void SIScan::UpdatePMTinDB(int db_mplexid,
                           int db_source_id,
                           int freqid, int pmt_indx,
                           const ProgramMapTable *pmt,
                           bool /*force_update*/)
{
    // See if service already in database based on program number
    int chanid = ChannelUtil::GetChanID(
        db_mplexid, -1, 0, 0, pmt->ProgramNumber());

    QString chan_num = ChannelUtil::GetChanNum(chanid);
    if (chan_num.isEmpty() || renameChannels)
    {
        chan_num = QString("%1#%2")
            .arg((freqid) ? freqid : db_mplexid).arg(pmt_indx);
    }
        
    QString callsign = ChannelUtil::GetCallsign(chanid);
    if (callsign == QString::null || callsign == "")
        callsign = tr("C%1", "Synthesized callsign").arg(chan_num);

    QString service_name = ChannelUtil::GetServiceName(chanid);
    if (service_name == QString::null || service_name == "")
        service_name = callsign;

    QString common_status_info = tr("%1 %2-%3 as %4 on %5 (%6)")
        .arg(service_name)
        .arg(0).arg(0)
        .arg(chan_num)
        .arg((*current).FriendlyName).arg(freqid);

    if (chanid < 0)
    {   // The service is not in database, add it
        emit ServiceScanUpdateText(
            tr("Adding %1").arg(common_status_info));
        chanid = ChannelUtil::CreateChanID(db_source_id, chan_num);
        ChannelUtil::CreateChannel(
            db_mplexid, db_source_id, chanid,
            callsign,
            service_name,
            chan_num,
            pmt->ProgramNumber(),
            0, 0,
            false, false, false, freqid);
    }
    else
    {   // The service is in database, update it
        emit ServiceScanUpdateText(
            tr("Updating %1").arg(common_status_info));
        ChannelUtil::UpdateChannel(
            db_mplexid,
            db_source_id,
            chanid,
            callsign,
            service_name,
            chan_num,
            pmt->ProgramNumber(),
            0, 0,
            freqid);
    }
}

/** \fn SIScan::UpdatePATinDB(int,const ProgramAssociationTable*,const pmt_map_t&,bool)
 */
void SIScan::UpdatePATinDB(int tid_db,
                           const ProgramAssociationTable *pat,
                           const pmt_map_t &pmt_map,
                           bool /*force_update*/)
{
    SISCAN(QString("UpdatePATinDB(): mplex: %1:%2")
            .arg(tid_db).arg((*current).mplexid));

    int db_mplexid = ChannelUtil::GetBetterMplexID(
        tid_db, pat->TransportStreamID(), 1);

    if (db_mplexid == -1)
    {
        VERBOSE(VB_IMPORTANT, "PAT: Warning couldn't find better mplex.");
        emit ServiceScanUpdateText(
            tr("Found channel, but it doesn't match existing tsid. You may "
               "wish to delete existing channels and do a full scan."));
        db_mplexid = tid_db;
    }

    int db_source_id   = ChannelUtil::GetSourceID(db_mplexid);
    int freqid         = (*current).friendlyNum;

    for (uint i = 0; i < pat->ProgramCount(); i++)
    {
        pmt_map_t::const_iterator it = pmt_map.find(pat->ProgramNumber(i));
        if (it == pmt_map.end())
        {
            VERBOSE(VB_SIPARSER,
                   QString("UpdatePATinDB(): PMT for Program #%1 is missing")
                   .arg(pat->ProgramNumber(i)));
            continue;
        }
        pmt_vec_t::const_iterator vit = (*it).begin();
        for (; vit != (*it).end(); ++vit)
        {
            VERBOSE(VB_SIPARSER,
                    QString("UpdatePATinDB(): Prog %1 PID %2: PMT @")
                    .arg(pat->ProgramNumber(i))
                    .arg(pat->ProgramPID(i)) << *vit);

            // ignore all services without PMT, and
            // ignore services we have decided to ignore
            if (!(*vit))
                continue;
            else if ((*vit)->StreamCount() <= 0)
                continue;
            else if (ignoreAudioOnlyServices && (*vit)->IsStillPicture())
                continue;
            else if (ignoreEncryptedServices && (*vit)->IsEncrypted())
                continue;

            UpdatePMTinDB(db_mplexid, db_source_id, freqid, i, *vit, false);
        }
    }    
}

/** \fn SIScan::UpdateVCTinDB(int,const VirtualChannelTable*,bool)
 */
void SIScan::UpdateVCTinDB(int tid_db,
                           const VirtualChannelTable *vct,
                           bool force_update)
{
    (void) force_update;

    SISCAN(QString("UpdateVCTinDB(): mplex: %1:%2")
           .arg(tid_db).arg((*current).mplexid));
    int db_mplexid = ChannelUtil::GetBetterMplexID(
        tid_db, vct->TransportStreamID(), 1);

    if (db_mplexid == -1)
    {
        VERBOSE(VB_IMPORTANT, "VCT: Error determing what transport this "
                "service table is associated with so failing");
        emit ServiceScanUpdateText(
            tr("Found channel, but it doesn't match existing tsid. You may "
               "wish to delete existing channels and do a full scan."));
        return;
    }

/*    bool fta_only     = ignore_encrypted_services(db_mplexid, videodevice);*/
    int db_source_id   = ChannelUtil::GetSourceID(db_mplexid);
    int freqid         = (*current).friendlyNum;

    for (uint i = 0; i < vct->ChannelCount(); i++)
    {
        if (vct->ModulationMode(i) == 0x01 /* NTSC Modulation */ ||
            vct->ServiceType(i)    == 0x01 /* Analog TV */)
        {
            continue;
        }

        if (vct->ServiceType(i) == 0x04 && ignoreDataServices)
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
        if (chan_num.isEmpty() || renameChannels)
        {
            chan_num = channelFormat
                .arg(vct->MajorChannel(i))
                .arg(vct->MinorChannel(i));
        }

        // try to find an extended channel name, fallback to short name.
        QString longName = vct->GetExtendedChannelName(i);
        if (longName.isEmpty())
            longName = vct->ShortChannelName(i);

        QString common_status_info = tr("%1 %2-%3 as %4 on %5 (%6)")
            .arg(vct->ShortChannelName(i))
            .arg(vct->MajorChannel(i)).arg(vct->MinorChannel(i))
            .arg(chan_num)
            .arg((*current).FriendlyName).arg(freqid);

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
                    longName,
                    chan_num,
                    vct->ProgramNumber(i),
                    vct->MajorChannel(i), vct->MinorChannel(i),
                    !vct->IsHiddenInGuide(i) /* use on air guide */,
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
                longName,
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
        emit ServiceScanUpdateText(
            tr("Found channel, but it doesn't match existing tsid. You may "
               "wish to delete existing channels and do a full scan."));
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
        else if (desc && !desc->IsDTV() && !desc->IsDigitalAudio() &&
                 ignoreDataServices)
        {
            emit ServiceScanUpdateText(tr("Skipping %1 - not a Television or "
                                          "Radio Service").arg(service_name));
            continue;
        }
        else if (ignoreEncryptedServices && sdt->IsEncrypted(i))
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

            if (!renameChannels)
                chan_num = ChannelUtil::GetChanNum(chanid);

            ChannelUtil::UpdateChannel(
                db_mplexid,
                db_source_id,
                chanid,
                service_name,
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
        emit ServiceScanUpdateText(
            tr("Found channel, but it doesn't match existing tsid. You may "
               "wish to delete existing channels and do a full scan."));
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

            if (!query.exec())
                MythContext::DBError("Selecting channel/dtv_multiplex", query);

            if (!query.isActive())
                MythContext::DBError("Check Channel full in channel/dtv_multiplex.", query);
        }
    }

    ignoreEncryptedServices = 
        ignore_encrypted_services(db_mplexid, GetDVBChannel()->GetDevice());

    int db_source_id   = ChannelUtil::GetSourceID(db_mplexid);
    int freqid         = (*current).friendlyNum;

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
                 ((*s).ServiceType != SDTObject::RADIO) && ignoreDataServices)
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
            .arg((*current).FriendlyName).arg(freqid);

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

            if (!renameChannels)
                chan_num = ChannelUtil::GetChanNum(chanid);

            ChannelUtil::UpdateChannel(
                db_mplexid,
                db_source_id,
                chanid,
                (*s).ServiceName,
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
        if (GetDVBChannel()->TuneTransport(chan_opts, true, channelTimeout))
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
            if (GetDVBChannel()->TuneTransport(chan_opts, true, channelTimeout))
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

    if (!NIT.Network.empty())
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

        if (!query.exec())
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
#if QT_VERSION >= 0x030200
            query.bindValue(":FREQUENCY", ((Q_ULLONG) (*t).Frequency));
#else
            query.bindValue(":FREQUENCY", ((uint) (*t).Frequency));
#endif
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

            if (!query.exec() || !query.isActive())
                MythContext::DBError("Inserting new transport", query);
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

int SIScan::InsertMultiplex(const transport_scan_items_it_t transport)
{
    int mplexid = -1;

#ifdef USING_DVB
    if (GetDVBChannel())
    {
        // Try to read the actual values back from the card
        DVBTuning tuning;
        if (!GetDVBChannel()->GetTuningParams(tuning))
            tuning = (*transport).tuning;

        // Write the best info we have to the DB
        if (FE_OFDM == GetDVBChannel()->GetCardType())
            mplexid = create_dtv_multiplex_ofdm(*transport, tuning);
        else
            mplexid = ChannelUtil::CreateMultiplex(
                (*transport).SourceID, (*transport).standard,
                tuning.Frequency(), tuning.ModulationDB());
    }
#endif // USING_DVB

#ifdef USING_V4L
    if (GetChannel())
    {
        const uint freq = (*transport).freq_offset(transport.offset());
        const uint freq_vis = freq - 1750000; // convert to visual carrier
        mplexid = ChannelUtil::CreateMultiplex(
            (*transport).SourceID, (*transport).standard, freq_vis, "8vsb");
    }
#endif // USING_V4L

    return mplexid;
}

#ifdef USE_SIPARSER
void SIScan::HandleSIParserEvents(void)
{
    // if there is no SIParser then we have nothing to do
    if (!siparser)
        return;
    // ignore events before current is set
    if ((scanMode == TRANSPORT_LIST) && (current == scanTransports.end()))
        return;

    if (serviceListReady)
    {
        if ((*current).mplexid <= 0)
            (*current).mplexid = InsertMultiplex(current);

        if (siparser->GetServiceObject(SDT))
        {
            UpdateServicesInDB((*current).mplexid, SDT);
            nextIt = current.nextTransport();
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

#ifdef USE_SIPARSER
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
#endif // USE_SIPARSER

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
