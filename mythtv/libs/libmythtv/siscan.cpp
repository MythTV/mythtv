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
#include "channelutil.h"
#include "cardutil.h"

// MythTV includes - DTV
#include "dtvsignalmonitor.h"
#include "scanstreamdata.h"

// MythTV includes - ATSC
#include "atsctables.h"

// MythTV includes - DVB
#include "dvbsignalmonitor.h"
#include "dvbtables.h"

#include "dvbchannel.h"
#include "hdhrchannel.h"
#include "channel.h"

QString SIScan::loc(const SIScan *siscan)
{
    if (siscan && siscan->channel)
        return QString("SIScan(%1)").arg(siscan->channel->GetDevice());
    return "SIScan(u)";
}

#define LOC     (SIScan::loc(this) + ": ")
#define LOC_ERR (SIScan::loc(this) + ", Error: ")

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

/** \fn SIScan::SIScan(QString,ChannelBase*,int,uint,uint,const QString&)
 */
SIScan::SIScan(const QString &_cardtype, ChannelBase *_channel, int _sourceID,
               uint signal_timeout, uint channel_timeout,
               const QString &_inputname)
    : // Set in constructor
      channel(_channel),
      signalMonitor(SignalMonitor::Init(_cardtype, -1, _channel)),
      sourceID(_sourceID),
      scanMode(IDLE),
      signalTimeout(signal_timeout),
      channelTimeout(channel_timeout),
      inputname(QDeepCopy<QString>(_inputname)),
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

    // Create a stream data for digital signal monitors
    DTVSignalMonitor* dtvSigMon = GetDTVSignalMonitor();
    if (dtvSigMon)
    {
        VERBOSE(VB_SIPARSER, LOC + "Connecting up DTVSignalMonitor");
        ScanStreamData *data = new ScanStreamData();

        dtvSigMon->SetStreamData(data);
        dtvSigMon->AddFlags(SignalMonitor::kDTVSigMon_WaitForMGT |
                            SignalMonitor::kDTVSigMon_WaitForVCT |
                            SignalMonitor::kDTVSigMon_WaitForNIT |
                            SignalMonitor::kDTVSigMon_WaitForSDT);

        data->AddMPEGListener(this);
        data->AddATSCMainListener(this);
        data->AddDVBMainListener(this);
    }
}

SIScan::~SIScan(void)
{
    StopScanner();
    VERBOSE(VB_SIPARSER, LOC + "SIScanner Stopped");
    if (signalMonitor)
        delete signalMonitor;
}

void SIScan::SetAnalog(bool is_analog)
{
    if (is_analog)
    {
        connect(signalMonitor, SIGNAL(AllGood(      void)),
                this,          SLOT(  HandleAllGood(void)));
    }
    else
    {
        disconnect(signalMonitor, SIGNAL(AllGood(      void)),
                   this,          SLOT(  HandleAllGood(void)));
    }
}

void SIScan::HandleAllGood(void)
{
    QString cur_chan = (*current).FriendlyName;
    QStringList list = QStringList::split(" ", cur_chan);
    QString freqid = (list.size() >= 2) ? list[1] : cur_chan;

    bool ok = false;

    QString msg = tr("Updated Channel %1").arg(cur_chan);

    if (!ChannelUtil::FindChannel(sourceID, freqid))
    {
        int chanid = ChannelUtil::CreateChanID(sourceID, freqid);

        QString callsign = QString("%1%2")
            .arg(ChannelUtil::GetUnknownCallsign()).arg(freqid);

        ok = ChannelUtil::CreateChannel(
            0      /* mplexid */,
            sourceID,
            chanid,
            callsign,
            ""     /* service name       */,
            freqid /* channum            */,
            0      /* service id         */,
            0      /* ATSC major channel */,
            0      /* ATSC minor channel */,
            false  /* use on air guide   */,
            false  /* hidden             */,
            false  /* hidden in guide    */,
            freqid);

        msg = (ok) ?
            tr("Added Channel %1").arg(cur_chan) :
            tr("Failed to add channel %1").arg(cur_chan);
    }
    else
    {
        // nothing to do here, XMLTV & DataDirect have better info
    }

    emit ServiceScanUpdateText(msg);

    // tell UI we are done with these channels
    if (scanMode == TRANSPORT_LIST)
    {
        UpdateScanPercentCompleted();
        waitingForTables = false;
        nextIt = current.nextTransport();
    }
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
        VERBOSE(VB_SIPARSER, LOC + "Unable to find any transports for " +
                QString("sourceid %1").arg(sourceID));

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

        VERBOSE(VB_SIPARSER, LOC + "Adding " + fn);

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
    VERBOSE(VB_SIPARSER, LOC +
            QString("Got a Program Association Table for %1")
            .arg((*current).FriendlyName));

    // Add pmts to list, so we can do MPEG scan properly.
    ScanStreamData *sd = GetDTVSignalMonitor()->GetScanStreamData();
    for (uint i = 0; i < pat->ProgramCount(); i++)
    {
        if (pat->ProgramPID(i)) // don't add NIT "program", MPEG/ATSC safe.
            sd->AddListeningPID(pat->ProgramPID(i));
    }
}

void SIScan::HandleVCT(uint, const VirtualChannelTable*)
{
    VERBOSE(VB_SIPARSER, LOC + QString("Got a Virtual Channel Table for %1")
            .arg((*current).FriendlyName));

    HandleATSCDBInsertion(GetDTVSignalMonitor()->GetScanStreamData(), true);
}

void SIScan::HandleMGT(const MasterGuideTable*)
{
    VERBOSE(VB_SIPARSER, LOC + QString("Got the Master Guide for %1")
            .arg((*current).FriendlyName));

    HandleATSCDBInsertion(GetDTVSignalMonitor()->GetScanStreamData(), true);
}

void SIScan::HandleSDT(uint, const ServiceDescriptionTable* sdt)
{
    VERBOSE(VB_SIPARSER, LOC +
            QString("Got a Service Description Table for %1")
            .arg((*current).FriendlyName));
    VERBOSE(VB_SIPARSER, LOC + sdt->toString());

    HandleDVBDBInsertion(GetDTVSignalMonitor()->GetScanStreamData(), true);
}

void SIScan::HandleNIT(const NetworkInformationTable *nit)
{
    VERBOSE(VB_SIPARSER, LOC +
            QString("Got a Network Information Table for %1")
            .arg((*current).FriendlyName));
    VERBOSE(VB_SIPARSER, LOC + nit->toString());

    dvbChanNums.clear();

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

    HandleDVBDBInsertion(GetDTVSignalMonitor()->GetScanStreamData(), true);
}

void SIScan::HandleMPEGDBInsertion(const ScanStreamData *sd, bool)
{
    // Try to determine if this might be "OpenCable" transport.
    QString sistandard = sd->GetSIStandard((*current).tuning.sistandard);

    if (!(*current).mplexid)
        (*current).mplexid = InsertMultiplex(current);

    if (!(*current).mplexid)
        return;

    int     mplexid = (*current).mplexid;
    int     freqid  = (*current).friendlyNum;
    QString fn      = (*current).FriendlyName;

    pat_vec_t pats = sd->GetCachedPATs();
    pmt_map_t pmt_map = sd->GetCachedPMTMap();
    for (uint i = 0; i < pats.size(); i++)
    {
        UpdatePATinDB(mplexid, fn, freqid, pats[i], pmt_map,
                      (*current).expectedChannels, sistandard, true);
    }
    sd->ReturnCachedPMTTables(pmt_map);
    sd->ReturnCachedPATTables(pats);

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

    if (!(*current).mplexid)
        (*current).mplexid = InsertMultiplex(current);

    if (!(*current).mplexid)
        return;

    int     mplexid = (*current).mplexid;
    int     freqid  = (*current).friendlyNum;
    QString fn      = (*current).FriendlyName;

    // Insert Terrestrial VCTs
    tvct_vec_t tvcts = sd->GetAllCachedTVCTs();
    for (uint i = 0; i < tvcts.size(); i++)
    {
        UpdateVCTinDB(mplexid, fn, freqid, tvcts[i],
                      (*current).expectedChannels, true);
    }
    sd->ReturnCachedTVCTTables(tvcts);

    // Insert Cable VCTs
    cvct_vec_t cvcts = sd->GetAllCachedCVCTs();
    for (uint i = 0; i < cvcts.size(); i++)
    {
        UpdateVCTinDB(mplexid, fn, freqid, cvcts[i],
                      (*current).expectedChannels, true);
    }
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
    if (wait_until_complete && !(dsd.HasCachedSDT() && dsd.HasCachedAllNIT()))
        return;

    emit ServiceScanUpdateText(tr("Updating Services"));

    if (!(*current).mplexid)
        (*current).mplexid = InsertMultiplex(current);

    vector<const ServiceDescriptionTable*> sdts = sd->GetAllCachedSDTs();
    for (uint i = 0; i < sdts.size(); i++)
    {
        UpdateSDTinDB((*current).mplexid, sdts[i],
                      (*current).expectedChannels, forceUpdate);
    }
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

/** \fn SIScan::HandlePostInsertion(void)
 *  \brief Insert channels based on any partial tables we do have.
 *  \return true if we saw any tables
 */
bool SIScan::HandlePostInsertion(void)
{
    DTVSignalMonitor* dtvSigMon = GetDTVSignalMonitor();
    if (!dtvSigMon)
        return false;

    const ScanStreamData *sd = dtvSigMon->GetScanStreamData();

    VERBOSE(VB_SIPARSER, LOC + "HandlePostInsertion() " +
            QString("pat(%1)").arg(sd->HasCachedAnyPAT()));

    const MasterGuideTable *mgt = sd->GetCachedMGT();
    if (mgt)
    {
        VERBOSE(VB_IMPORTANT, mgt->toString());
        HandleATSCDBInsertion(sd, false);
        sd->ReturnCachedTable(mgt);
        return true;
    }

    const NetworkInformationTable *nit = sd->GetCachedNIT(0);
    sdt_vec_t sdts = sd->GetAllCachedSDTs();
    if (nit || sdts.size())
    {
        if (nit)
            VERBOSE(VB_IMPORTANT, nit->toString());
        HandleDVBDBInsertion(sd, false);
        sd->ReturnCachedSDTTables(sdts);
        sd->ReturnCachedTable(nit);
        return true;
    }

    if (sd->HasCachedAnyPAT())
    {
        VERBOSE(VB_IMPORTANT, LOC + "Post insertion found PAT..");
        HandleMPEGDBInsertion(sd, false);
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

DTVChannel *SIScan::GetDTVChannel(void)
{
    return dynamic_cast<DTVChannel*>(channel);
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
    VERBOSE(VB_SIPARSER, LOC + "Starting SIScanner");

    scanner_thread_running = true;
    threadExit = false;

    while (!threadExit)
    {
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
        VERBOSE(VB_SIPARSER, LOC + time_out_table_str);
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
        VERBOSE(VB_SIPARSER, LOC + time_out_sig_str);

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

bool SIScan::Tune(const transport_scan_items_it_t transport)
{
    const TransportScanItem &item = *transport;
    const uint64_t freq = item.freq_offset(transport.offset());

#ifdef USING_DVB
    if (GetDVBSignalMonitor())
    {
        // always wait for rotor to finish
        GetDVBSignalMonitor()->AddFlags(SignalMonitor::kDVBSigMon_WaitForPos);
        GetDVBSignalMonitor()->SetRotorTarget(1.0f);
    }
#endif // USING_DVB

    if (!GetDTVChannel())
        return false;

    if (item.mplexid > 0)
        return GetDTVChannel()->TuneMultiplex(item.mplexid, inputname);

    DTVMultiplex tuning = item.tuning;
    tuning.frequency = freq;
    return GetDTVChannel()->Tune(tuning, inputname);
}

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
    VERBOSE(VB_SIPARSER, LOC + tune_msg_str);

    if (!Tune(transport))
    {   // If we did not tune successfully, bail with message
        UpdateScanPercentCompleted();
        VERBOSE(VB_SIPARSER, LOC +
                QString("Failed to tune %1 mplexid(%2) at offset %3")
                .arg(item.FriendlyName).arg(item.mplexid)
                .arg(transport.offset()));
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

    timer.start();
    waitingForTables = true;
}

/** \fn SIScan::StopScanner(void)
 *  \brief Stops the SIScan event loop and the signal monitor,
 *         blocking until both exit.
 */
void SIScan::StopScanner(void)
{
    VERBOSE(VB_SIPARSER, LOC + "Stopping SIScanner");

    threadExit = true;

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
    nextIt = scanTransports.end();

    freq_table_list_t tables =
        get_matching_freq_tables(std, modulation, country);

    VERBOSE(VB_SIPARSER, LOC + 
            QString("Looked up freq table (%1, %2, %3) w/%4 entries")
            .arg(std).arg(modulation).arg(country).arg(tables.size()));

    freq_table_list_t::iterator it = tables.begin();
    for (; it != tables.end(); ++it)
    {
        const FrequencyTable &ft = **it;
        int     name_num         = ft.name_offset;
        QString strNameFormat    = ft.name_format;
        uint    freq             = ft.frequencyStart;
        while (freq <= ft.frequencyEnd)
        {
            name = strNameFormat;
            if (strNameFormat.find("%") >= 0)
                name = strNameFormat.arg(name_num);

            TransportScanItem item(SourceID, si_std, name, name_num,
                                   freq, ft, signalTimeout);
            scanTransports += item;

            VERBOSE(VB_SIPARSER, LOC + item.toString());

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

bool SIScan::ScanForChannels(uint sourceid,
                             const QString &std,
                             const QString &cardtype,
                             const DTVChannelList &channels)
{
    scanTransports.clear();
    nextIt = scanTransports.end();

    DTVTunerType tunertype;
    tunertype.Parse(cardtype);

    DTVChannelList::const_iterator it = channels.begin();
    for (uint i = 0; it != channels.end(); ++it, i++)
    {
        DTVTransport tmp = *it;
        tmp.sistandard = std;
        TransportScanItem item(sourceid, QString::number(i),
                               tunertype, tmp, signalTimeout);

        scanTransports += item;

        VERBOSE(VB_SIPARSER, LOC + item.toString());
    }

    if (scanTransports.empty())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "ScanForChannels() no transports");
        return false;
    }

    timer.start();
    waitingForTables = false;

    nextIt            = scanTransports.begin();
    transportsScanned = 0;
    scanMode          = TRANSPORT_LIST;

    return true;
}

/** \fn SIScan::ScanTransportsStartingOn(int,const QMap<QString,QString>&)
 *  \brief Generates a list of frequencies to scan and adds it to the
 *   scanTransport list, and then sets the scanMode to TRANSPORT_LIST.
 */
bool SIScan::ScanTransportsStartingOn(int sourceid,
                                      const QMap<QString,QString> &startChan)
{
    QMap<QString,QString>::const_iterator it;

    if (startChan.find("std")        == startChan.end() ||
        startChan.find("modulation") == startChan.end())
    {
        return false;
    }

    QString std    = *startChan.find("std");
    QString mod    = (*(startChan.find("modulation"))).upper();
    QString si_std = (std.lower() != "atsc") ? "dvb" : "atsc";
    QString name   = "";
    bool    ok     = false;

    if (scanMode == TRANSPORT_LIST)
        return false;

    scanTransports.clear();
    nextIt = scanTransports.end();

    DTVMultiplex tuning;

    DTVTunerType type;

    if (std == "dvb") 
    {
        ok = type.Parse(mod);
    }
    else if (std == "atsc")
    {
        type = DTVTunerType::kTunerTypeATSC;
        ok = true;
    }

    if (ok)
    {
        ok = tuning.ParseTuningParams(
            type,
            startChan["frequency"],      startChan["inversion"],
            startChan["symbolrate"],     startChan["fec"],
            startChan["polarity"],
            startChan["coderate_hp"],    startChan["coderate_lp"],
            startChan["constellation"],  startChan["trans_mode"],
            startChan["guard_interval"], startChan["hierarchy"],
            startChan["modulation"],     startChan["bandwidth"]);
    }

    if (ok)
    {
        tuning.sistandard = si_std;
        scanTransports += TransportScanItem(
            sourceid, tr("Frequency %1").arg(startChan["frequency"]),
            tuning, signalTimeout);
    }

    if (!ok)
        return false;

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
        VERBOSE(VB_SIPARSER, LOC + "Unable to find transport to scan.");
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
        
        VERBOSE(VB_SIPARSER, LOC + "Adding " + fn);

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

/** \fn SIScan::CheckImportedList(const DTVChannelInfoList&,uint,QString&,QString&,QString&)
 *  \brief If we as scanning a dvb-utils import verify channel is in list..
 */
bool SIScan::CheckImportedList(const DTVChannelInfoList &channels,
                               uint mpeg_program_num,
                               QString &service_name,
                               QString &callsign,
                               QString &common_status_info)
{
    if (channels.empty())
        return true;

    bool found = false;
    for (uint i = 0; i < channels.size(); i++)
    {
        VERBOSE(VB_IMPORTANT,
                QString("comparing %1 %2 against %3 %4")
                .arg(channels[i].serviceid).arg(channels[i].name)
                .arg(mpeg_program_num).arg(common_status_info));

        if (channels[i].serviceid == mpeg_program_num)
        {
            found = true;
            if (!channels[i].name.isEmpty())
            {
                service_name = QDeepCopy<QString>(channels[i].name);
                callsign     = QDeepCopy<QString>(channels[i].name);
            }
        }
    }

    if (found)
    {
        common_status_info += QString(" %1 %2")
            .arg(tr("as")).arg(service_name);
    }
    else
    {
        emit ServiceScanUpdateText(
            tr("Skipping %1, not in imported channel map")
            .arg(common_status_info));
    }

    return found;
}

// ///////////////////// DB STUFF /////////////////////
// ///////////////////// DB STUFF /////////////////////
// ///////////////////// DB STUFF /////////////////////
// ///////////////////// DB STUFF /////////////////////
// ///////////////////// DB STUFF /////////////////////

void SIScan::UpdatePMTinDB(
    int db_source_id,
    int db_mplexid, const QString &friendlyName, int freqid,
    int pmt_indx, const ProgramMapTable *pmt,
    const DTVChannelInfoList &channels, bool /*force_update*/)
{
    VERBOSE(VB_IMPORTANT, LOC + pmt->toString());

    // See if service already in database based on program number
    int chanid = ChannelUtil::GetChanID(
        db_mplexid, -1, -1, -1, pmt->ProgramNumber());

    QString chan_num = ChannelUtil::GetChanNum(chanid);
    if (chan_num.isEmpty() || renameChannels)
    {
        chan_num = QString("%1#%2")
            .arg((freqid) ? freqid : db_mplexid).arg(pmt_indx);
    }
        
    QString callsign = ChannelUtil::GetCallsign(chanid);
    QString service_name = ChannelUtil::GetServiceName(chanid);

    if (callsign.isEmpty())
    {
        callsign = QString("%1%2")
            .arg(ChannelUtil::GetUnknownCallsign())
            .arg(chan_num);
    }
    else if (service_name.isEmpty())
        service_name = callsign; // only do this for real callsigns

    QString common_status_info = tr("%1%2%3 on %4 (%5)")
        .arg(service_name)
        .arg(service_name.isEmpty() ? "" : QString(" %1 ").arg(tr("as")))
        .arg(chan_num)
        .arg(friendlyName).arg(freqid);

    if (!CheckImportedList(channels, pmt->ProgramNumber(),
                           service_name, chan_num, common_status_info))
    {
        return;
    }

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
            false, false, false, QString::number(freqid));
    }
    else
    {   // The service is in database, update it
        emit ServiceScanUpdateText(
            tr("Updating %1").arg(common_status_info));
        ChannelUtil::UpdateChannel(
            db_mplexid, db_source_id, chanid,
            callsign,
            service_name,
            chan_num,
            pmt->ProgramNumber(),
            0, 0,
            false, false, false, QString::number(freqid));
    }
}

void SIScan::IgnoreDataOnlyMsg(const QString &name, int aux_num)
{
    QString vmsg = QString("Ignoring Data Channel: %1").arg(name);
    if (aux_num > 0)
        vmsg += QString(" on %1").arg(aux_num);
    VERBOSE(VB_SIPARSER, vmsg);

    //

    QString tmsg = tr("Skipping %1").arg(name);
    if (aux_num > 0)
        tmsg += " " + tr("on %1").arg(aux_num);
    tmsg += " - " + tr("Data Only Channel (off-air?)");
    emit ServiceScanUpdateText(tmsg);
}

void SIScan::IgnoreEmptyChanMsg(const QString &name, int aux_num)
{
    QString vmsg = QString("Ignoring Empty Channel: %1").arg(name);
    if (aux_num > 0)
        vmsg += QString(" on %1").arg(aux_num);
    VERBOSE(VB_SIPARSER, vmsg);

    //

    QString tmsg = tr("Skipping %1").arg(name);
    if (aux_num > 0)
        tmsg += " " + tr("on %1").arg(aux_num);
    tmsg += " - " + tr("Empty Channel (off-air?)");
    emit ServiceScanUpdateText(tmsg);
}

void SIScan::IgnoreAudioOnlyMsg(const QString &name, int aux_num)
{
    QString vmsg = QString("Ignoring Audio Only Channel: %1").arg(name);
    if (aux_num > 0)
        vmsg += QString(" on %1").arg(aux_num);
    VERBOSE(VB_SIPARSER, vmsg);

    //

    QString tmsg = tr("Skipping %1").arg(name);
    if (aux_num > 0)
        tmsg += " " + tr("on %1").arg(aux_num);
    tmsg += " - " + tr("Audio Only Channel");
    emit ServiceScanUpdateText(tmsg);
}

void SIScan::IgnoreEncryptedMsg(const QString &name, int aux_num)
{
    QString vmsg = QString("Ignoring Encrypted Channel: %1").arg(name);
    if (aux_num > 0)
        vmsg += QString(" on %1").arg(aux_num);
    VERBOSE(VB_SIPARSER, vmsg);

    //

    QString tmsg = tr("Skipping %1").arg(name);
    if (aux_num > 0)
        tmsg += " " + tr("on %1").arg(aux_num);
    tmsg += " - " + tr("Encrypted Channel");
    emit ServiceScanUpdateText(tmsg);
}

void SIScan::UpdatePATinDB(
    int db_mplexid, const QString &friendlyName, int freqid,
    const ProgramAssociationTable *pat, const pmt_map_t &pmt_map,
    const DTVChannelInfoList &channels, const QString &si_standard,
    bool force_update)
{
    VERBOSE(VB_SIPARSER, LOC +
            QString("UpdatePATinDB(): tsid: 0x%1  mplex: %2")
            .arg(pat->TransportStreamID(),0,16).arg(db_mplexid));

    VERBOSE(VB_IMPORTANT, LOC + pat->toString());

    int db_source_id   = ChannelUtil::GetSourceID(db_mplexid);

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
            {
                IgnoreEmptyChanMsg(friendlyName, pat->ProgramNumber(i));
                continue;
            }
            else if (ignoreAudioOnlyServices &&
                     (*vit)->IsStillPicture(si_standard))
            {
                IgnoreAudioOnlyMsg(friendlyName, pat->ProgramNumber(i));
                continue;
            }
            else if (ignoreEncryptedServices && (*vit)->IsEncrypted())
            {
                IgnoreEncryptedMsg(friendlyName, pat->ProgramNumber(i));
                continue;
            }

            UpdatePMTinDB(db_source_id, db_mplexid, friendlyName, freqid,
                          i, *vit, channels, force_update);
        }
    }    
}

/** \fn SIScan::UpdateVCTinDB(int,const QString&,int,const VirtualChannelTable*,bool)
 */
void SIScan::UpdateVCTinDB(int db_mplexid,
                           const QString &friendlyName, int freqid,
                           const VirtualChannelTable *vct,
                           const DTVChannelInfoList &channels,
                           bool force_update)
{
    (void) force_update;

    VERBOSE(VB_SIPARSER, LOC +
            QString("UpdateVCTinDB(): tsid: 0x%1  mplex: %1")
            .arg(vct->TransportStreamID(),0,16).arg(db_mplexid));

    int db_source_id   = ChannelUtil::GetSourceID(db_mplexid);

    for (uint i = 0; i < vct->ChannelCount(); i++)
    {
        if (vct->ModulationMode(i) == 0x01 /* NTSC Modulation */ ||
            vct->ServiceType(i)    == 0x01 /* Analog TV */)
        {
            continue;
        }

        QString basic_status_info = QString("%1 %2-%3")
            .arg(vct->ShortChannelName(i))
            .arg(vct->MajorChannel(i)).arg(vct->MinorChannel(i));

        if (vct->ServiceType(i) == 0x04 && ignoreDataServices)
        {
            IgnoreEmptyChanMsg(basic_status_info, vct->ProgramNumber(i));
            continue;            
        }

        if (vct->ServiceType(i) == 0x03 && ignoreAudioOnlyServices)
        {
            IgnoreAudioOnlyMsg(basic_status_info, vct->ProgramNumber(i));
            continue;            
        }

        if (vct->IsAccessControlled(i) && ignoreEncryptedServices)
        {
            IgnoreEncryptedMsg(basic_status_info, vct->ProgramNumber(i));
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
            .arg(chan_num).arg(friendlyName).arg(freqid);

        bool use_eit = !vct->IsHidden(i) ||
            (vct->IsHidden(i) && !vct->IsHiddenInGuide(i));

        QString callsign = vct->ShortChannelName(i);

        if (!CheckImportedList(channels, vct->ProgramNumber(i),
                               longName, callsign, common_status_info))
        {
            continue;
        }

        QString msg = "";
        if (chanid < 0)
        {   // The service is not in database, add it
            msg = tr("Adding %1").arg(common_status_info);
            chanid = ChannelUtil::CreateChanID(db_source_id, chan_num);
            if (chanid > 0)
            {
                ChannelUtil::CreateChannel(
                    db_mplexid,
                    db_source_id,
                    chanid,
                    callsign,
                    longName,
                    chan_num,
                    vct->ProgramNumber(i),
                    vct->MajorChannel(i), vct->MinorChannel(i),
                    use_eit,
                    vct->IsHidden(i), vct->IsHiddenInGuide(i),
                    QString::number(freqid));
            }
        }
        else
        {   // The service is in database, update it
            msg = tr("Updating %1").arg(common_status_info);
            ChannelUtil::UpdateChannel(
                db_mplexid,
                db_source_id,
                chanid,
                callsign,
                longName,
                chan_num,
                vct->ProgramNumber(i),
                vct->MajorChannel(i), vct->MinorChannel(i),
                use_eit,
                vct->IsHidden(i), vct->IsHiddenInGuide(i),
                QString::number(freqid));
        }
        emit ServiceScanUpdateText(msg);
        VERBOSE(VB_SIPARSER, msg);
    }
}

/** \fn SIScan::UpdateSDTinDB(int,const ServiceDescriptionTable*,bool)
 *  \brief Inserts channels from service description table.
 */
void SIScan::UpdateSDTinDB(int /*mplexid*/, const ServiceDescriptionTable *sdt,
                           const DTVChannelInfoList &channels,
                           bool force_update)
{
    if (!sdt->ServiceCount())
        return;

    int db_mplexid = ChannelUtil::GetMplexID(
        sourceID, sdt->TSID(), sdt->OriginalNetworkID());

    // HACK beg -- special exception for this network (dbver == "1067")
    bool force_guide_present = (sdt->OriginalNetworkID() == 70);
    // HACK end -- special exception for this network

    if (db_mplexid == -1)
    {
        VERBOSE(VB_IMPORTANT, "SDT: Error determing what transport this "
                "service table is associated with so failing");
        emit ServiceScanUpdateText(
            tr("Found channel, but it doesn't match existing tsid. You may "
               "wish to delete existing channels and do a full scan."));
        return;
    }

    int db_source_id = ChannelUtil::GetSourceID(db_mplexid);

    /* This will be fixed post .17 to be more elegant */
    bool upToDate = (ChannelUtil::GetServiceVersion(db_mplexid) ==
                     (int)sdt->Version());
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
            IgnoreAudioOnlyMsg(service_name, sdt->ServiceID(i));
            continue;
        }
        else if (desc && !desc->IsDTV() && !desc->IsDigitalAudio() &&
                 ignoreDataServices)
        {
            IgnoreDataOnlyMsg(service_name, sdt->ServiceID(i));
            continue;
        }
        else if (ignoreEncryptedServices && sdt->IsEncrypted(i))
        {
            IgnoreEncryptedMsg(service_name, sdt->ServiceID(i));
            continue;
        }

        // Default authority
        QString default_authority = "";
        desc_list_t parsed =
            MPEGDescriptor::Parse(sdt->ServiceDescriptors(i),
                                  sdt->ServiceDescriptorsLength(i));
        const unsigned char *def_auth =
            MPEGDescriptor::Find(parsed, DescriptorID::default_authority);
        if (def_auth)
            default_authority =
                QString::fromAscii((const char*)def_auth+2, def_auth[1]);

        QString common_status_info = service_name;

        if (!CheckImportedList(channels, sdt->ServiceID(i),
                               service_name, service_name, common_status_info))
        {
            continue;
        }

        // See if service already in database based on service ID
        int chanid = ChannelUtil::GetChanID(db_mplexid, -1, -1, -1,
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
                    sdt->HasEITSchedule(i) ||
                    sdt->HasEITPresentFollowing(i) ||
                    force_guide_present,
                    false, false, QString::null,
                    QString::null, "Default", QString::null,
                    default_authority);
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
                sdt->HasEITSchedule(i) ||
                sdt->HasEITPresentFollowing(i) ||
                force_guide_present,
                false, false, QString::null,
                QString::null, QString::null, QString::null,
                default_authority);
        }
        else
        {
            emit ServiceScanUpdateText(
                tr("Skipping %1 - already in DB, and "
                   "we don't have better data.")
                .arg(service_name));
        }

        if (desc)
            delete desc;
    }
}

// FindBestMplexFreq()
//  - Examines the freq in the DB against the curent tuning frequency and
//    it's offset frequencies. If the frequency in the DB is any of the
//    tuning frequencies or offsets then use the DB frequency.
uint64_t SIScan::FindBestMplexFreq(
    const uint64_t tuning_freq,
    const transport_scan_items_it_t transport, const uint sourceid,
    const uint transportid, const uint networkid)
{
    uint64_t    db_freq;
    QString     tmp_modulation;
    QString     tmp_si_std;
    uint        tmp_transportid, tmp_networkid;
    int         mplexid;

    if (!transportid || !networkid)
        return tuning_freq;

    mplexid = ChannelUtil::GetMplexID(sourceid, transportid, networkid);
    if (mplexid <= 0)
        return tuning_freq;

    if (!ChannelUtil::GetTuningParams(
            (uint)mplexid, tmp_modulation,
            db_freq, tmp_transportid, tmp_networkid, tmp_si_std))
    {
        return tuning_freq;
    }

    for (uint i = 0; i < (*transport).offset_cnt(); i++)
    {
        if (db_freq == (*transport).freq_offset(i))
            return db_freq;
    }

    return tuning_freq;
}

uint SIScan::InsertMultiplex(const transport_scan_items_it_t transport)
{
    DTVMultiplex tuning = (*transport).tuning;
    uint tsid  = 0;
    uint netid = 0;

    tuning.frequency = (*transport).freq_offset(transport.offset());

#ifdef USING_DVB
    if (GetDVBSignalMonitor())
    {
        DVBSignalMonitor *sm = GetDVBSignalMonitor();

        tsid  = sm->GetDetectedTransportID();
        netid = sm->GetDetectedNetworkID();

        // Try to read the actual values back from the card
        if (GetDVBChannel()->IsTuningParamsProbeSupported())
            GetDVBChannel()->ProbeTuningParams(tuning);

        tuning.frequency = FindBestMplexFreq(
            tuning.frequency, transport, (*transport).SourceID, tsid, netid);
    }
#endif // USING_DVB

#ifdef USING_V4L
    if (GetChannel())
    {
        // convert to visual carrier
        tuning.frequency = tuning.frequency - 1750000;
    }
#endif // USING_V4L

    return ChannelUtil::CreateMultiplex(
        (*transport).SourceID, tuning, tsid, netid);
}
