/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Copyright (c) 2004, 2005 John Pullan <john@pullan.org>
 * Copyright (c) 2005 - 2007 Daniel Kristjansson
 *
 * Description:
 *     Collection of classes to provide channel scanning functionallity
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

// C includes
#include <pthread.h>
#include <unistd.h>

// Qt includes
#include <qmutex.h>

// MythTV includes - General
#include "channelscan_sm.h"
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
#include "v4lchannel.h"

QString ChannelScanSM::loc(const ChannelScanSM *siscan)
{
    if (siscan && siscan->channel)
        return QString("ChannelScanSM(%1)").arg(siscan->channel->GetDevice());
    return "ChannelScanSM(u)";
}

#define LOC     (ChannelScanSM::loc(this) + ": ")
#define LOC_ERR (ChannelScanSM::loc(this) + ", Error: ")

#define kDecryptionTimeout 4250

class ScannedChannelInfo
{
  public:
    ScannedChannelInfo() : mgt(NULL) {}

    // MPEG
    pat_map_t         pats;
    pmt_vec_t         pmts;
    QMap<uint,uint>   program_encryption_status; // pnum->enc_status

    // ATSC
    const MasterGuideTable *mgt;
    cvct_vec_t        cvcts;
    tvct_vec_t        tvcts;
    
    // DVB
    nit_vec_t         nits;
    sdt_map_t         sdts;
};

/** \class ChannelScanSM
 *  \brief Scanning class for cards that support a SignalMonitor class.
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
 *
 *   TODO
 *
 */

ChannelScanSM::ChannelScanSM(
    ScanMonitor *_scan_monitor,
    const QString &_cardtype, ChannelBase *_channel,
    int _sourceID, uint signal_timeout, uint channel_timeout,
    const QString &_inputname)
    : // Set in constructor
      scan_monitor(_scan_monitor),
      channel(_channel),
      signalMonitor(SignalMonitor::Init(_cardtype, -1, _channel)),
      sourceID(_sourceID),
      signalTimeout(signal_timeout),
      channelTimeout(channel_timeout),
      inputname(Q3DeepCopy<QString>(_inputname)),
      // State
      scanning(false),
      threadExit(false),
      waitingForTables(false),
      // Transports List
      transportsScanned(0),
      currentTestingDecryption(false),
      // Misc
      channelsFound(999),
      analogSignalHandler(new AnalogSignalHandler(this)),
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

        DVBChannel *dvbchannel = dynamic_cast<DVBChannel*>(channel);
        if (dvbchannel && dvbchannel->GetRotor())
            dtvSigMon->AddFlags(SignalMonitor::kDVBSigMon_WaitForPos);

        data->AddMPEGListener(this);
        data->AddATSCMainListener(this);
        data->AddDVBMainListener(this);
    }
}

ChannelScanSM::~ChannelScanSM(void)
{
    StopScanner();
    VERBOSE(VB_SIPARSER, LOC + "ChannelScanSMner Stopped");

    if (signalMonitor)
    {
        signalMonitor->RemoveListener(analogSignalHandler);
        delete signalMonitor;
        signalMonitor = NULL;
    }

    if (analogSignalHandler)
    {
        delete analogSignalHandler;
        analogSignalHandler = NULL;
    }
}

void ChannelScanSM::SetAnalog(bool is_analog)
{
    signalMonitor->RemoveListener(analogSignalHandler);

    if (is_analog)
        signalMonitor->AddListener(analogSignalHandler);
}

void ChannelScanSM::HandleAllGood(void)
{
    QString cur_chan = (*current).FriendlyName;
    QStringList list = QStringList::split(" ", cur_chan);
    QString freqid = (list.size() >= 2) ? list[1] : cur_chan;

    bool ok = false;

    QString msg = QObject::tr("Updated Channel %1").arg(cur_chan);

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
            QObject::tr("Added Channel %1").arg(cur_chan) :
            QObject::tr("Failed to add channel %1").arg(cur_chan);
    }
    else
    {
        // nothing to do here, XMLTV & DataDirect have better info
    }

    scan_monitor->ScanAppendTextToLog(msg);

    // tell UI we are done with these channels
    if (scanning)
    {
        UpdateScanPercentCompleted();
        waitingForTables = false;
        nextIt = current.nextTransport();
    }
}

/** \fn ChannelScanSM::ScanServicesSourceID(int)
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
bool ChannelScanSM::ScanServicesSourceID(int SourceID)
{
    if (scanning)
        return false;

    scanTransports.clear();
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
        MythDB::DBError("Get Transports for SourceID", query);
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
        scanTransports.push_back(item);
    }

    waitingForTables  = false;
    transportsScanned = 0;
    if (scanTransports.size())
    {
        nextIt   = scanTransports.begin();
        scanning = true;
    }

    return scanning;
}

void ChannelScanSM::HandlePAT(const ProgramAssociationTable *pat)
{
    VERBOSE(VB_SIPARSER, LOC +
            QString("Got a Program Association Table for %1")
            .arg((*current).FriendlyName) + "\n" + pat->toString());

    // Add pmts to list, so we can do MPEG scan properly.
    ScanStreamData *sd = GetDTVSignalMonitor()->GetScanStreamData();
    for (uint i = 0; i < pat->ProgramCount(); i++)
    {
        if (pat->ProgramPID(i)) // don't add NIT "program", MPEG/ATSC safe.
            sd->AddListeningPID(pat->ProgramPID(i));
    }
}

void ChannelScanSM::HandlePMT(uint, const ProgramMapTable *pmt)
{
    VERBOSE(VB_SIPARSER, LOC +
            QString("Got a Program Map Table for %1")
            .arg((*current).FriendlyName) + "\n" + pmt->toString());

    if (!currentTestingDecryption && pmt->IsEncrypted())
        currentEncryptionStatus[pmt->ProgramNumber()] = kEncUnknown;
}

void ChannelScanSM::HandleVCT(uint, const VirtualChannelTable *vct)
{
    VERBOSE(VB_SIPARSER, LOC + QString("Got a Virtual Channel Table for %1")
            .arg((*current).FriendlyName) + "\n" + vct->toString());

    for (uint i = 0; !currentTestingDecryption && i < vct->ChannelCount(); i++)
    {
        if (vct->IsAccessControlled(i))
        {
            currentEncryptionStatus[vct->ProgramNumber(i)] = kEncUnknown;
        }
    }

    UpdateChannelInfo(true);
}

void ChannelScanSM::HandleMGT(const MasterGuideTable *mgt)
{
    VERBOSE(VB_SIPARSER, LOC + QString("Got the Master Guide for %1")
            .arg((*current).FriendlyName) + "\n" + mgt->toString());

    UpdateChannelInfo(true);
}

void ChannelScanSM::HandleSDT(uint, const ServiceDescriptionTable *sdt)
{
    VERBOSE(VB_SIPARSER, LOC +
            QString("Got a Service Description Table for %1")
            .arg((*current).FriendlyName) + "\n" + sdt->toString());

    for (uint i = 0; !currentTestingDecryption && i < sdt->ServiceCount(); i++)
    {
        if (sdt->IsEncrypted(i))
        {
            currentEncryptionStatus[sdt->ServiceID(i)] = kEncUnknown;
        }
    }

    UpdateChannelInfo(true);
}

void ChannelScanSM::HandleNIT(const NetworkInformationTable *nit)
{
    VERBOSE(VB_SIPARSER, LOC +
            QString("Got a Network Information Table for %1")
            .arg((*current).FriendlyName) + "\n" + nit->toString());

    UpdateChannelInfo(true);
}

void ChannelScanSM::HandleEncryptionStatus(uint pnum, bool encrypted)
{
    currentEncryptionStatus[pnum] = encrypted ? kEncEncrypted : kEncDecrypted;

    if (kEncDecrypted == currentEncryptionStatus[pnum])
        currentTestingDecryption = false;

    UpdateChannelInfo(true);
}

bool ChannelScanSM::TestNextProgramEncryption(void)
{
    ScannedChannelInfo *info = channelMap[current];
    if (!info || info->pmts.empty())
    {
        VERBOSE(VB_IMPORTANT, LOC + "Can't monitor decryption -- no pmts");
        currentTestingDecryption = false;
        return false;
    }

    uint pnum = 0;
    do
    {
        QMap<uint, uint>::const_iterator it = currentEncryptionStatus.begin();
        for (; !pnum && (it != currentEncryptionStatus.end()); ++it)
            pnum = (currentEncryptionStatusChecked[it.key()]) ? 0 : it.key();

        if (!pnum)
            break;

        currentEncryptionStatusChecked[pnum] = true;

        const ProgramMapTable *pmt = NULL;
        for (uint i = 0; !pmt && (i < info->pmts.size()); i++)
        {
            pmt = (info->pmts[i]->ProgramNumber() == pnum) ?
                info->pmts[i] : NULL;
        }

        if (pmt)
        {
            QString cur_chan, cur_chan_tr;
            GetCurrentTransportInfo(cur_chan, cur_chan_tr);

            QString msg_tr =
                QObject::tr("%1 -- Testing decryption of program %2")
                .arg(cur_chan_tr).arg(pnum);
            QString msg =
                QString("%1 -- Testing decryption of program %2")
                .arg(cur_chan).arg(pnum);

            scan_monitor->ScanAppendTextToLog(msg_tr);
            VERBOSE(VB_SIPARSER, msg);

            if (GetDVBChannel())
                GetDVBChannel()->SetPMT(pmt);

            GetDTVSignalMonitor()->GetStreamData()->TestDecryption(pmt);

            currentTestingDecryption = true;
            timer.start();
            return true;
        }

        VERBOSE(VB_IMPORTANT, LOC +
                "Can't monitor decryption of program %1 -- no pmt");

    } while (pnum);

    currentTestingDecryption = false;
    return false;
}

bool ChannelScanSM::UpdateChannelInfo(bool wait_until_complete)
{
    if (current == scanTransports.end())
        return true;

    if (wait_until_complete && currentTestingDecryption)
        return false;

    const ScanStreamData *sd = GetDTVSignalMonitor()->GetScanStreamData();
    if (!channelMap[current])
        channelMap[current] = new ScannedChannelInfo();
    ScannedChannelInfo *info = channelMap[current];

    bool transport_tune_complete = true;

    // MPEG

    // Grab PAT tables
    pat_vec_t pattmp = sd->GetCachedPATs();
    QMap<uint,bool> tsid_checked;
    for (uint i = 0; i < pattmp.size(); i++)
    {
        uint tsid = pattmp[i]->TransportStreamID();
        if (tsid_checked[tsid])
            continue;
        tsid_checked[tsid] = true;
        if (!info->pats[tsid].empty())
            continue;

        if (!wait_until_complete || sd->HasCachedAllPAT(tsid))
        {
            info->pats[tsid] = sd->GetCachedPATs(tsid);
            if (info->pmts.size())
            {
                sd->ReturnCachedPMTTables(info->pmts);
                info->pmts.clear();
            }
        }
        else
            transport_tune_complete = false;
    }
    transport_tune_complete &= !pattmp.empty();
    sd->ReturnCachedPATTables(pattmp);

    // Grab PMT tables
    if ((!wait_until_complete || sd->HasCachedAllPMTs()) && info->pmts.empty())
        info->pmts = sd->GetCachedPMTs();

    // ATSC
    if (!info->mgt && sd->HasCachedMGT())
        info->mgt = sd->GetCachedMGT();

    if ((!wait_until_complete || sd->HasCachedAllCVCTs()) &&
        info->cvcts.empty())
    {
        info->cvcts = sd->GetCachedCVCTs();
    }

    if ((!wait_until_complete || sd->HasCachedAllTVCTs()) &&
        info->cvcts.empty())
    {
        info->tvcts = sd->GetCachedTVCTs();
    }

    // DVB
    if ((!wait_until_complete || sd->HasCachedAllNIT()) && info->nits.empty())
        info->nits = sd->GetCachedNIT();

    sdt_vec_t sdttmp = sd->GetCachedSDTs();
    tsid_checked.clear();
    for (uint i = 0; i < sdttmp.size(); i++)
    {
        uint tsid = sdttmp[i]->TSID();
        if (tsid_checked[tsid])
            continue;
        tsid_checked[tsid] = true;
        if (!info->sdts[tsid].empty())
            continue;

        if (!wait_until_complete || sd->HasCachedAllSDT(tsid))
            info->sdts[tsid] = sd->GetCachedSDTs(tsid);
    }
    sd->ReturnCachedSDTTables(sdttmp);

    // Check if transport tuning is complete
    if (transport_tune_complete)
    {
        transport_tune_complete &= !info->pmts.empty();
        if (sd->HasCachedMGT() || sd->HasCachedAnyVCTs())
        {
            transport_tune_complete &=
                (!info->tvcts.empty() || !info->cvcts.empty());
        }
        if (sd->HasCachedAnyNIT() || sd->HasCachedAnySDTs())
        {
            transport_tune_complete &= !info->nits.empty();
            transport_tune_complete &= !info->sdts.empty();
        }
    }
    transport_tune_complete |= !wait_until_complete;

    if (transport_tune_complete &&
        /*!ignoreEncryptedServices &&*/ currentEncryptionStatus.size())
    {
        //GetDTVSignalMonitor()->GetStreamData()->StopTestingDecryption();

        if (TestNextProgramEncryption())
            return false;

        QMap<uint, uint>::const_iterator it = currentEncryptionStatus.begin();
        for (; it != currentEncryptionStatus.end(); ++it)
        {
            info->program_encryption_status[it.key()] = *it;

            QString msg_tr1 = QObject::tr("Program %1").arg(it.key());
            QString msg_tr2 = QObject::tr("Unknown decryption status");
            if (kEncEncrypted == *it)
                msg_tr2 = QObject::tr("Encrypted");
            else if (kEncDecrypted == *it)
                msg_tr2 = QObject::tr("Decrypted");
            QString msg_tr =QString("%1, %2").arg(msg_tr1).arg(msg_tr2);

            QString msg = LOC + QString("Program %1").arg(it.key());
            if (kEncEncrypted == *it)
                msg = msg + " -- Encrypted";
            else if (kEncDecrypted == *it)
                msg = msg + " -- Decrypted";
            else if (kEncUnknown == *it)
                msg = msg + " -- Unknown decryption status";

            scan_monitor->ScanAppendTextToLog(msg_tr);
            VERBOSE(VB_SIPARSER, msg);
        }
    }

    // Start scanning next transport if we are done with this one..
    if (transport_tune_complete)
    {
        QString cchan, cchan_tr;
        uint cchan_cnt = GetCurrentTransportInfo(cchan, cchan_tr);
        channelsFound += cchan_cnt;
        QString chan_tr = QObject::tr("%1 -- Timed out").arg(cchan_tr);
        QString chan    = QString(    "%1 -- Timed out").arg(cchan);
        QString msg_tr  = "";
        QString msg     = "";

        SignalMonitor *sm = GetSignalMonitor();
        if ((timer.elapsed() > (int)channelTimeout))
        {
            msg_tr = (cchan_cnt) ?
                QObject::tr("%1 possible channels").arg(cchan_cnt) :
                QObject::tr("no channels");
            msg_tr = QString("%1, %2").arg(chan_tr).arg(msg_tr);
            msg = (cchan_cnt) ?
                QString("%1 possible channels").arg(cchan_cnt) :
                QString("no channels");
            msg = QString("%1, %2").arg(chan_tr).arg(msg);
        }
        else if ((current != scanTransports.end()) &&
                 (timer.elapsed() > (int)(*current).timeoutTune) &&
                 sm && !sm->HasSignalLock())
        {
            msg_tr = QObject::tr("%1, no signal").arg(chan_tr);
            msg = QString("%1, no signal").arg(chan);
        }
        else
        {
            msg_tr = QObject::tr("%1 -- Found %2 probable channels")
                .arg(cchan_tr).arg(cchan_cnt);

            msg = QString("%1 -- Found %2 probable channels")
                .arg(cchan).arg(cchan_cnt);
        }

        scan_monitor->ScanAppendTextToLog(msg_tr);
        VERBOSE(VB_SIPARSER, LOC + msg);

        currentEncryptionStatus.clear();
        currentEncryptionStatusChecked.clear();

        if (scanning)
        {
            transportsScanned++;
            UpdateScanPercentCompleted();
            waitingForTables = false;
            nextIt = current.nextTransport();
        }
        else
        {
            scan_monitor->ScanPercentComplete(100);
            scan_monitor->ScanComplete();
        }

        return true;
    }

    return false;
}

void ChannelScanSM::ScanNITs(void)
{
    dvbChanNums.clear();

    ChannelMap::iterator it;
    for (it = channelMap.begin(); it != channelMap.end(); ++it)
    {
        for (uint i = 0; i < (*it)->nits.size(); i++)
        {
            const NetworkInformationTable *nit = (*it)->nits[i];
            if (!nit->TransportStreamCount())
                continue;

            scan_monitor->ScanAppendTextToLog(
                QObject::tr("Processing NIT %1").arg(nit->NetworkName()));

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
                    MPEGDescriptor::Find(list,
                                         DescriptorID::dvb_uk_channel_list);

                if (desc)
                {
                    UKChannelListDescriptor uklist(desc);
                    for (uint j = 0; j < uklist.ChannelCount(); j++)
                    {
                        dvbChanNums[uklist.ServiceID(j)] =
                            uklist.ChannelNumber(j);
                    }
                }
            }
        }
    }
}

#define PCM_INFO_INIT(SISTD) \
    ChannelInsertInfo &info = pnum_to_dbchan[pnum]; \
    info.db_mplexid   = mplexid;   info.source_id    = sourceID;  \
    info.service_id   = pnum;      info.freqid       = freqidStr; \
    info.si_standard  = SISTD;

static void update_info(ChannelInsertInfo &info,
                        const VirtualChannelTable *vct, uint i)
{
    if (vct->ModulationMode(i) == 0x01 /* NTSC Modulation */ ||
        vct->ServiceType(i)    == 0x01 /* Analog TV */)
    {
        info.si_standard = "ntsc";
        info.format      = "ntsc";
    }

    info.callsign = vct->ShortChannelName(i);

    info.service_name = vct->GetExtendedChannelName(i);
    if (info.service_name.isEmpty())
        info.service_name = vct->ShortChannelName(i);

    info.chan_num           = QString::null;

    info.service_id         = vct->ProgramNumber(i);
    info.atsc_major_channel = vct->MajorChannel(i);
    info.atsc_minor_channel = vct->MinorChannel(i);

    info.use_on_air_guide = !vct->IsHidden(i) ||
        (vct->IsHidden(i) && !vct->IsHiddenInGuide(i));

    info.hidden           = vct->IsHidden(i);
    info.hidden_in_guide  = vct->IsHiddenInGuide(i);

    info.vct_tsid         = vct->TransportStreamID();
    info.vct_chan_tsid    = vct->ChannelTransportStreamID(i);
    info.is_encrypted    |= vct->IsAccessControlled(i);
    info.is_data_service  = vct->ServiceType(i) == 0x04;
    info.is_audio_service = vct->ServiceType(i) == 0x03;

    info.in_vct       = true;
}

static void update_info(ChannelInsertInfo &info,
                        const ServiceDescriptionTable *sdt, uint i)
{
    // HACK beg -- special exception for this network
    //             (dbver == "1067")
    bool force_guide_present = (sdt->OriginalNetworkID() == 70);
    // HACK end -- special exception for this network

    // Figure out best service name...
    ServiceDescriptor *desc = sdt->GetServiceDescriptor(i);
    QString service_name = QString::null;
    if (desc)
        service_name = desc->ServiceName();
    if (service_name.stripWhiteSpace().isEmpty())
        service_name = QString::null;
    if (info.service_name.isEmpty())
        info.service_name = service_name;

    info.use_on_air_guide =
        sdt->HasEITPresentFollowing(i) ||
        sdt->HasEITSchedule(i) ||
        force_guide_present;

    info.hidden           = false;
    info.hidden_in_guide  = false;

    info.is_data_service =
        (desc && !desc->IsDTV() && !desc->IsDigitalAudio());
    info.is_audio_service = (desc && desc->IsDigitalAudio());

    info.sdt_tsid   = sdt->TSID();
    info.orig_netid = sdt->OriginalNetworkID();
    info.in_sdt     = true;
}

uint ChannelScanSM::GetCurrentTransportInfo(
    QString &cur_chan, QString &cur_chan_tr) const
{
    if (current.iter() == scanTransports.end())
    {
        cur_chan = cur_chan_tr = QString::null;
        return 0;
    }

    uint max_chan_cnt = 0;

    ChannelMap::const_iterator it = channelMap.find(current);
    if (it != channelMap.end())
    {
        QMap<uint,ChannelInsertInfo> list = GetChannelList(it);
        for (int i = 0; i < list.size(); i++)
        {
            max_chan_cnt +=
                (list[i].in_pat || list[i].in_pmt ||
                 list[i].in_sdt || list[i].in_vct) ? 1 : 0;
        }
    }

    QString offset_str_tr = current.offset() ?
        QObject::tr(" offset %2").arg(current.offset()) : "";
     cur_chan_tr = QString("%1%2")
        .arg((*current).FriendlyName).arg(offset_str_tr);

    QString offset_str = current.offset() ?
        QString(" offset %2").arg(current.offset()) : "";
    cur_chan = QString("%1%2")
        .arg((*current).FriendlyName).arg(offset_str);

    return max_chan_cnt;
}

QMap<uint,ChannelInsertInfo>
ChannelScanSM::GetChannelList(ChannelMap::const_iterator it) const
{
    QMap<uint,ChannelInsertInfo> pnum_to_dbchan;

    uint    mplexid   = (*it.key()).mplexid;
    int     freqid    = (*it.key()).friendlyNum;
    QString freqidStr = (freqid) ? QString::number(freqid) : QString::null;

    // channels.conf
    const DTVChannelInfoList &echan = (*it.key()).expectedChannels;
    for (uint i = 0; i < echan.size(); i++)
    {
        uint pnum = echan[i].serviceid;
        PCM_INFO_INIT("mpeg");
        info.service_name = echan[i].name;
        info.in_channels_conf = true;
    }

    // PATs
    pat_map_t::const_iterator pat_list_it = (*it)->pats.begin();
    for (; pat_list_it != (*it)->pats.end(); ++pat_list_it)
    {
        pat_vec_t::const_iterator pat_it = (*pat_list_it).begin();
        for (; pat_it != (*pat_list_it).end(); ++pat_it)
        {
            bool could_be_opencable = false;
            for (uint i = 0; i < (*pat_it)->ProgramCount(); i++)
            {
                if (((*pat_it)->ProgramNumber(i) == 0) &&
                    ((*pat_it)->ProgramPID(i) == 0x1ffc))
                {
                    could_be_opencable = true;
                }
            }

            for (uint i = 0; i < (*pat_it)->ProgramCount(); i++)
            {
                uint pnum = (*pat_it)->ProgramNumber(i);
                if (pnum)
                {
                    PCM_INFO_INIT("mpeg");
                    info.pat_tsid = (*pat_it)->TransportStreamID();
                    info.could_be_opencable = true;
                    info.in_pat = true;
                }
            }
        }
    }

    // PMTs
    pmt_vec_t::const_iterator pmt_it = (*it)->pmts.begin();
    for (; pmt_it != (*it)->pmts.end(); ++pmt_it)
    {
        const ProgramMapTable *pmt = *pmt_it;
        uint pnum = pmt->ProgramNumber();
        PCM_INFO_INIT("mpeg");
        for (uint i = 0; i < pmt->StreamCount(); i++)
        {
            info.could_be_opencable |=
                (StreamID::OpenCableVideo == pmt->StreamType(i));
        }

        desc_list_t descs = MPEGDescriptor::ParseOnlyInclude(
            pmt->ProgramInfo(), pmt->ProgramInfoLength(),
            DescriptorID::registration);

        for (uint i = 0; i < descs.size(); i++)
        {
            RegistrationDescriptor reg(descs[i]);
            if (reg.FormatIdentifierString() == "CUEI")
                info.is_opencable = true;
        }

        info.is_encrypted |= pmt->IsEncrypted();
        info.in_pmt = true;
    }

    // Cable VCTs
    cvct_vec_t::const_iterator cvct_it = (*it)->cvcts.begin();
    for (; cvct_it != (*it)->cvcts.end(); ++cvct_it)
    {
        for (uint i = 0; i < (*cvct_it)->ChannelCount(); i++)
        {
            uint pnum = (*cvct_it)->ProgramNumber(i);
            PCM_INFO_INIT("atsc");
            update_info(info, *cvct_it, i);
        }
    }

    // Terrestrial VCTs
    tvct_vec_t::const_iterator tvct_it = (*it)->tvcts.begin();
    for (; tvct_it != (*it)->tvcts.end(); ++tvct_it)
    {
        for (uint i = 0; i < (*tvct_it)->ChannelCount(); i++)
        {
            uint pnum = (*tvct_it)->ProgramNumber(i);
            PCM_INFO_INIT("atsc");
            update_info(info, *tvct_it, i);
        }
    }

    // SDTs
    sdt_map_t::const_iterator sdt_list_it = (*it)->sdts.begin();
    for (; sdt_list_it != (*it)->sdts.end(); ++sdt_list_it)
    {
        sdt_vec_t::const_iterator sdt_it = (*sdt_list_it).begin();
        for (; sdt_it != (*sdt_list_it).end(); ++sdt_it)
        {
            for (uint i = 0; i < (*sdt_it)->ServiceCount(); i++)
            {
                uint pnum = (*sdt_it)->ServiceID(i);
                PCM_INFO_INIT("dvb");
                update_info(info, *sdt_it, i);
            }
        }
    }

    // NIT
    QMap<uint,ChannelInsertInfo>::iterator dbchan_it;
    for (dbchan_it = pnum_to_dbchan.begin();
         dbchan_it != pnum_to_dbchan.end(); ++dbchan_it)
    {
        ChannelInsertInfo &info = *dbchan_it;

        // NIT
        nit_vec_t::const_iterator nits_it = (*it)->nits.begin();
        for (; nits_it != (*it)->nits.end(); ++nits_it)
        {
            for (uint i = 0; i < (*nits_it)->TransportStreamCount(); i++)
            {
                const NetworkInformationTable *nit = (*nits_it);
                if ((nit->TSID(i)              == info.sdt_tsid) &&
                    (nit->OriginalNetworkID(i) == info.orig_netid))
                {
                    info.netid = nit->NetworkID();
                    info.in_nit = true;
                }
            }
        }
    }

    // Get UK channel numbers
    for (dbchan_it = pnum_to_dbchan.begin();
         dbchan_it != pnum_to_dbchan.end(); ++dbchan_it)
    {
        uint pnum = dbchan_it.key();
        ChannelInsertInfo &info = *dbchan_it;
            
        if ((dvbChanNums.find(pnum) != dvbChanNums.end()) &&
            info.chan_num.isEmpty())
        {
            info.chan_num = QString::number(dvbChanNums[pnum]);
        }
    }

    // Check for decryption success
    for (dbchan_it = pnum_to_dbchan.begin();
         dbchan_it != pnum_to_dbchan.end(); ++dbchan_it)
    {
        uint pnum = dbchan_it.key();
        ChannelInsertInfo &info = *dbchan_it;
        info.decryption_status = (*it)->program_encryption_status[pnum];
    }

    return pnum_to_dbchan;
}

ScanDTVTransportList ChannelScanSM::GetChannelList(void) const
{
    ScanDTVTransportList list;

    uint cardid = channel->GetCardID();

    DTVTunerType tuner_type = DTVTunerType::kTunerTypeATSC;
    if (GetDVBChannel())
        tuner_type = GetDVBChannel()->GetCardType();

    ChannelMap::const_iterator it = channelMap.begin();
    for (; it != channelMap.end(); ++it)
    {
        QMap<uint,ChannelInsertInfo> pnum_to_dbchan = GetChannelList(it);

        // Insert channels into DB
        ScanDTVTransport item((*it.key()).tuning, tuner_type, cardid);

        QMap<uint,ChannelInsertInfo>::iterator dbchan_it;
        for (dbchan_it = pnum_to_dbchan.begin();
             dbchan_it != pnum_to_dbchan.end(); ++dbchan_it)
        {
            item.channels.push_back(*dbchan_it);
        }
        if (item.channels.size())
            list.push_back(item);
    }

    return list;
}


DTVSignalMonitor* ChannelScanSM::GetDTVSignalMonitor(void)
{
    return dynamic_cast<DTVSignalMonitor*>(signalMonitor);
}

DVBSignalMonitor* ChannelScanSM::GetDVBSignalMonitor(void)
{
#ifdef USING_DVB
    return dynamic_cast<DVBSignalMonitor*>(signalMonitor);
#else
    return NULL;
#endif
}

DTVChannel *ChannelScanSM::GetDTVChannel(void)
{
    return dynamic_cast<DTVChannel*>(channel);
}

DVBChannel *ChannelScanSM::GetDVBChannel(void)
{
#ifdef USING_DVB
    return dynamic_cast<DVBChannel*>(channel);
#else
    return NULL;
#endif
}

const DVBChannel *ChannelScanSM::GetDVBChannel(void) const
{
#ifdef USING_DVB
    return dynamic_cast<const DVBChannel*>(channel);
#else
    return NULL;
#endif
}

V4LChannel *ChannelScanSM::GetV4LChannel(void)
{
#ifdef USING_V4L
    return dynamic_cast<V4LChannel*>(channel);
#else
    return NULL;
#endif
}

/** \fn ChannelScanSM::SpawnScanner(void*)
 *  \brief Thunk that allows scanner_thread pthread to
 *         call ChannelScanSM::RunScanner().
 */
void *ChannelScanSM::SpawnScanner(void *param)
{
    ChannelScanSM *scanner = (ChannelScanSM *)param;
    scanner->RunScanner();
    return NULL;
}

/** \fn ChannelScanSM::StartScanner(void)
 *  \brief Starts the ChannelScanSM event loop.
 */
void ChannelScanSM::StartScanner(void)
{
    pthread_create(&scanner_thread, NULL, SpawnScanner, this);
}

/** \fn ChannelScanSM::RunScanner(void)
 *  \brief This runs the event loop for ChannelScanSM until 'threadExit' is true.
 */
void ChannelScanSM::RunScanner(void)
{
    VERBOSE(VB_SIPARSER, LOC + "ChannelScanSM::RunScanner -- begin");

    scanner_thread_running = true;
    threadExit = false;

    while (!threadExit)
    {
        if (scanning)
            HandleActiveScan();

        usleep(250);
    }
    scanner_thread_running = false;

    VERBOSE(VB_SIPARSER, LOC + "ChannelScanSM::RunScanner -- end");
}

// See if we have timed out
bool ChannelScanSM::HasTimedOut(void)
{
    if (currentTestingDecryption)
        return (timer.elapsed() > (int)kDecryptionTimeout);

    if (!waitingForTables)
        return true;

#ifdef USING_DVB
    // If the rotor is still moving, reset the timer and keep waiting
    DVBSignalMonitor *sigmon = GetDVBSignalMonitor();
    if (sigmon)
    {
        const DiSEqCDevRotor *rotor = GetDVBChannel()->GetRotor();
        if (rotor)
        {
            bool was_moving, is_moving;
            sigmon->GetRotorStatus(was_moving, is_moving);
            if (was_moving && !is_moving)
            {
                timer.restart();
                return false;
            }
        }
    }
#endif // USING_DVB


    // have the tables have timed out?
    if (timer.elapsed() > (int)channelTimeout)
        return true;

    // ok the tables haven't timed out, but have we hit the signal timeout?
    SignalMonitor *sm = GetSignalMonitor();
    if ((timer.elapsed() > (int)(*current).timeoutTune) &&
        sm && !sm->HasSignalLock())
    {
        return true;
    }

    return false;
}

/** \fn ChannelScanSM::HandleActiveScan(void)
 *  \brief Handles the TRANSPORT_LIST ChannelScanSM mode.
 */
void ChannelScanSM::HandleActiveScan(void)
{
    bool do_post_insertion = waitingForTables;

    if (!HasTimedOut())
        return;

    if (0 == nextIt.offset() && nextIt != scanTransports.begin())
    {
        // Add channel to scanned list and potentially check decryption
        if (do_post_insertion && !UpdateChannelInfo(false))
            return;

        // Stop signal monitor for previous transport
        signalMonitor->Stop();
    }

    if (0 == nextIt.offset() && nextIt == scanTransports.begin())
    {
        channelMap.clear();
        channelsFound = 0;
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
        scan_monitor->ScanComplete();
        scanning = false;
        current = nextIt = scanTransports.end();
    }
}

bool ChannelScanSM::Tune(const transport_scan_items_it_t transport)
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

void ChannelScanSM::ScanTransport(const transport_scan_items_it_t transport)
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

    if (channelsFound)
    {
        QString progress = QObject::tr(": Found %1").arg(channelsFound);
        scan_monitor->ScanUpdateStatusTitleText(progress);
    }

    scan_monitor->ScanUpdateStatusText(cur_chan);
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

/** \fn ChannelScanSM::StopScanner(void)
 *  \brief Stops the ChannelScanSM event loop and the signal monitor,
 *         blocking until both exit.
 */
void ChannelScanSM::StopScanner(void)
{
    VERBOSE(VB_SIPARSER, LOC + "ChannelScanSM::StopScanner");

    threadExit = true;

    if (scanner_thread_running)
        pthread_join(scanner_thread, NULL);

    if (signalMonitor)
        signalMonitor->Stop();
}

/** \fn ChannelScanSM::ScanTransports(int,const QString,const QString,const QString)
 *  \brief Generates a list of frequencies to scan and adds it to the
 *   scanTransport list, and then sets the scanning to TRANSPORT_LIST.
 */
bool ChannelScanSM::ScanTransports(int SourceID,
                            const QString std,
                            const QString modulation,
                            const QString country)
{
    QString si_std = (std.lower() != "atsc") ? "dvb" : "atsc";
    QString name("");
    if (scanning)
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
            scanTransports.push_back(item);

            VERBOSE(VB_SIPARSER, LOC + item.toString());

            name_num++;
            freq += ft.frequencyStep;
        }
    }

    timer.start();
    waitingForTables = false;

    nextIt            = scanTransports.begin();
    transportsScanned = 0;
    scanning          = true;

    return true;
}

bool ChannelScanSM::ScanForChannels(uint sourceid,
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

        scanTransports.push_back(item);

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
    scanning          = true;

    return true;
}

/** \fn ChannelScanSM::ScanTransportsStartingOn(int,const QMap<QString,QString>&)
 *  \brief Generates a list of frequencies to scan and adds it to the
 *   scanTransport list, and then sets the scanning to TRANSPORT_LIST.
 */
bool ChannelScanSM::ScanTransportsStartingOn(int sourceid,
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

    if (scanning)
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
        TransportScanItem item(
            sourceid, QObject::tr("Frequency %1").arg(startChan["frequency"]),
            tuning, signalTimeout);
        scanTransports.push_back(item);
    }

    if (!ok)
        return false;

    timer.start();
    waitingForTables = false;

    nextIt            = scanTransports.begin();
    transportsScanned = 0;
    scanning          = true;

    return true;
}

bool ChannelScanSM::ScanTransport(int mplexid)
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
        MythDB::DBError("Get Transports for SourceID", query);
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
        scanTransports.push_back(item);
    }

    timer.start();
    waitingForTables  = false;

    transportsScanned = 0;
    if (scanTransports.size())
    {
        nextIt   = scanTransports.begin();
        scanning = true;
        return true;
    }

    return false;
}

/** \fn ChannelScanSM::CheckImportedList(const DTVChannelInfoList&,uint,QString&,QString&,QString&)
 *  \brief If we as scanning a dvb-utils import verify channel is in list..
 */
bool ChannelScanSM::CheckImportedList(
    const DTVChannelInfoList &channels,
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
                service_name = Q3DeepCopy<QString>(channels[i].name);
                callsign     = Q3DeepCopy<QString>(channels[i].name);
            }
        }
    }

    if (found)
    {
        common_status_info += QString(" %1 %2")
            .arg(QObject::tr("as")).arg(service_name);
    }
    else
    {
        scan_monitor->ScanAppendTextToLog(
            QObject::tr("Skipping %1, not in imported channel map")
            .arg(common_status_info));
    }

    return found;
}
