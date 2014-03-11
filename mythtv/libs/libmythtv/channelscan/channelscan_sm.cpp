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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

// C includes
#include <unistd.h>

// C++ includes
#include <algorithm>
using namespace std;

// Qt includes
#include <QObject>

// MythTV includes - General
#include "channelscan_sm.h"
#include "frequencies.h"
#include "scanwizardconfig.h"
#include "mythdbcon.h"
#include "channelutil.h"
#include "cardutil.h"
#include "sourceutil.h"
#include "mthread.h"
#include "mythdb.h"
#include "mythlogging.h"

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

/// SDT's should be sent every 2 seconds and NIT's every
/// 10 seconds, so lets wait at least 30 seconds, in
/// case of bad transmitter or lost packets.
const uint ChannelScanSM::kDVBTableTimeout  = 30 * 1000;
/// No logic here, lets just wait at least 10 seconds.
const uint ChannelScanSM::kATSCTableTimeout = 10 * 1000;
/// No logic here, lets just wait at least 15 seconds.
const uint ChannelScanSM::kMPEGTableTimeout = 15 * 1000;

QString ChannelScanSM::loc(const ChannelScanSM *siscan)
{
    if (siscan && siscan->channel)
        return QString("ChannelScanSM(%1)").arg(siscan->channel->GetDevice());
    return "ChannelScanSM(u)";
}

#define LOC     (ChannelScanSM::loc(this) + ": ")

#define kDecryptionTimeout 4250

class ScannedChannelInfo
{
  public:
    ScannedChannelInfo() : mgt(NULL) {}

    bool IsEmpty() const
    {
        return pats.empty() && pmts.empty()        &&
               program_encryption_status.isEmpty() &&
               !mgt         && cvcts.empty()       && tvcts.empty() &&
               nits.empty() && sdts.empty();
    }

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

ChannelScanSM::ChannelScanSM(ScanMonitor *_scan_monitor,
                             const QString &_cardtype, ChannelBase *_channel,
                             int _sourceID, uint signal_timeout,
                             uint channel_timeout, const QString &_inputname,
                             bool test_decryption)
    : // Set in constructor
      scan_monitor(_scan_monitor),
      channel(_channel),
      signalMonitor(SignalMonitor::Init(_cardtype, -1, _channel)),
      sourceID(_sourceID),
      signalTimeout(signal_timeout),
      channelTimeout(channel_timeout),
      otherTableTimeout(0),
      otherTableTime(0),
      setOtherTables(false),
      inputname(_inputname),
      m_test_decryption(test_decryption),
      extend_scan_list(false),
      // Optional state
      scanDTVTunerType(DTVTunerType::kTunerTypeUnknown),
      // State
      scanning(false),
      threadExit(false),
      waitingForTables(false),
      // Transports List
      transportsScanned(0),
      currentTestingDecryption(false),
      // Misc
      channelsFound(999),
      currentInfo(NULL),
      analogSignalHandler(new AnalogSignalHandler(this)),
      scannerThread(NULL)
{
    inputname.detach();

    current = scanTransports.end();

    // Create a stream data for digital signal monitors
    DTVSignalMonitor* dtvSigMon = GetDTVSignalMonitor();
    if (dtvSigMon)
    {
        LOG(VB_CHANSCAN, LOG_INFO, LOC + "Connecting up DTVSignalMonitor");
        ScanStreamData *data = new ScanStreamData();

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
                "SELECT dvb_nit_id "
                "FROM videosource "
                "WHERE videosource.sourceid = :SOURCEID");
        query.bindValue(":SOURCEID", _sourceID);
        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("ChannelScanSM", query);
        }
        else if (query.next())
        {
            uint nitid = query.value(0).toInt();
            data->SetRealNetworkID(nitid);
            LOG(VB_CHANSCAN, LOG_INFO, LOC +
                QString("Setting NIT-ID to %1").arg(nitid));
        }

        dtvSigMon->SetStreamData(data);
        dtvSigMon->AddFlags(SignalMonitor::kDTVSigMon_WaitForMGT |
                            SignalMonitor::kDTVSigMon_WaitForVCT |
                            SignalMonitor::kDTVSigMon_WaitForNIT |
                            SignalMonitor::kDTVSigMon_WaitForSDT);

#ifdef USING_DVB
        DVBChannel *dvbchannel = dynamic_cast<DVBChannel*>(channel);
        if (dvbchannel && dvbchannel->GetRotor())
            dtvSigMon->AddFlags(SignalMonitor::kDVBSigMon_WaitForPos);
#endif

        data->AddMPEGListener(this);
        data->AddATSCMainListener(this);
        data->AddDVBMainListener(this);
        data->AddDVBOtherListener(this);
    }
}

ChannelScanSM::~ChannelScanSM(void)
{
    StopScanner();
    LOG(VB_CHANSCAN, LOG_INFO, LOC + "ChannelScanSM Stopped");

    ScanStreamData *sd = NULL;
    if (GetDTVSignalMonitor())
    {
        sd = GetDTVSignalMonitor()->GetScanStreamData();
    }

    if (signalMonitor)
    {
        signalMonitor->RemoveListener(analogSignalHandler);
        delete signalMonitor;
        signalMonitor = NULL;
    }

    delete sd;

    if (analogSignalHandler)
    {
        delete analogSignalHandler;
        analogSignalHandler = NULL;
    }

    teardown_frequency_tables();
}

void ChannelScanSM::SetAnalog(bool is_analog)
{
    signalMonitor->RemoveListener(analogSignalHandler);

    if (is_analog)
        signalMonitor->AddListener(analogSignalHandler);
}

void ChannelScanSM::HandleAllGood(void)
{
    QMutexLocker locker(&lock);

    QString cur_chan = (*current).FriendlyName;
    QStringList list = cur_chan.split(" ", QString::SkipEmptyParts);
    QString freqid = (list.size() >= 2) ? list[1] : cur_chan;

    bool ok = false;

    QString msg = QObject::tr("Updated Channel %1").arg(cur_chan);

    if (!ChannelUtil::FindChannel(sourceID, freqid))
    {
        int chanid = ChannelUtil::CreateChanID(sourceID, freqid);

        QString callsign = QString("%1-%2")
            .arg(ChannelUtil::GetUnknownCallsign()).arg(chanid);

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

/**
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
bool ChannelScanSM::ScanExistingTransports(uint sourceid, bool follow_nit)
{
    if (scanning)
        return false;

    scanTransports.clear();
    nextIt = scanTransports.end();

    vector<uint> multiplexes = SourceUtil::GetMplexIDs(sourceid);

    if (multiplexes.empty())
    {
        LOG(VB_CHANSCAN, LOG_ERR, LOC + "Unable to find any transports for " +
            QString("sourceid %1").arg(sourceid));

        return false;
    }

    for (uint i = 0; i < multiplexes.size(); i++)
        AddToList(multiplexes[i]);

    extend_scan_list = follow_nit;
    waitingForTables  = false;
    transportsScanned = 0;
    if (!scanTransports.empty())
    {
        nextIt   = scanTransports.begin();
        scanning = true;
    }
    else
    {
        LOG(VB_CHANSCAN, LOG_ERR, LOC +
            "Unable to find add any transports for " +
            QString("sourceid %1").arg(sourceid));

        return false;
    }


    return scanning;
}

void ChannelScanSM::HandlePAT(const ProgramAssociationTable *pat)
{
    QMutexLocker locker(&lock);

    LOG(VB_CHANSCAN, LOG_INFO, LOC +
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
    QMutexLocker locker(&lock);

    LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("Got a Program Map Table for %1")
            .arg((*current).FriendlyName) + "\n" + pmt->toString());

    if (!currentTestingDecryption &&
        pmt->IsEncrypted(GetDTVChannel()->GetSIStandard()))
        currentEncryptionStatus[pmt->ProgramNumber()] = kEncUnknown;
}

void ChannelScanSM::HandleVCT(uint, const VirtualChannelTable *vct)
{
    QMutexLocker locker(&lock);

    LOG(VB_CHANSCAN, LOG_INFO, LOC +
        QString("Got a Virtual Channel Table for %1")
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
    QMutexLocker locker(&lock);

    LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("Got the Master Guide for %1")
            .arg((*current).FriendlyName) + "\n" + mgt->toString());

    UpdateChannelInfo(true);
}

void ChannelScanSM::HandleSDT(uint tsid, const ServiceDescriptionTable *sdt)
{
    QMutexLocker locker(&lock);

    LOG(VB_CHANSCAN, LOG_INFO, LOC +
        QString("Got a Service Description Table for %1")
            .arg((*current).FriendlyName) + "\n" + sdt->toString());

    // If this is Astra 28.2 add start listening for Freesat BAT and SDTo
    if (!setOtherTables && (sdt->OriginalNetworkID() == 2 ||
        sdt->OriginalNetworkID() == 59))
    {
        GetDTVSignalMonitor()->GetScanStreamData()->
                               SetFreesatAdditionalSI(true);
        setOtherTables = true;
        // The whole BAT & SDTo group comes round in 10s
        otherTableTimeout = 10000;
        // Delay processing the SDT until we've seen BATs and SDTos
        otherTableTime = timer.elapsed() + otherTableTimeout;

        LOG(VB_CHANSCAN, LOG_INFO, LOC +
            QString("SDT has OriginalNetworkID %1, look for "
                    "additional Freesat SI").arg(sdt->OriginalNetworkID()));
    }

    if ((uint)timer.elapsed() < otherTableTime)
    {
        // Set the version for the SDT so we see it again.
        GetDTVSignalMonitor()->GetDVBStreamData()->
            SetVersionSDT(sdt->TSID(), -1, 0);
    }

    uint id = sdt->OriginalNetworkID() << 16 | sdt->TSID();
    ts_scanned.insert(id);

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
    QMutexLocker locker(&lock);

    LOG(VB_CHANSCAN, LOG_INFO, LOC +
        QString("Got a Network Information Table for %1")
            .arg((*current).FriendlyName) + "\n" + nit->toString());

    UpdateChannelInfo(true);
}

void ChannelScanSM::HandleBAT(const BouquetAssociationTable *bat)
{
    QMutexLocker locker(&lock);

    LOG(VB_CHANSCAN, LOG_INFO, LOC + "Got a Bouquet Association Table\n" +
        bat->toString());

    otherTableTime = timer.elapsed() + otherTableTimeout;

    for (uint i = 0; i < bat->TransportStreamCount(); i++)
    {
        uint tsid = bat->TSID(i);
        uint netid = bat->OriginalNetworkID(i);
        desc_list_t parsed =
            MPEGDescriptor::Parse(bat->TransportDescriptors(i),
                                  bat->TransportDescriptorsLength(i));
        // Look for default authority
        const unsigned char *def_auth =
            MPEGDescriptor::Find(parsed, DescriptorID::default_authority);
        const unsigned char *serv_list =
            MPEGDescriptor::Find(parsed, DescriptorID::service_list);

        if (def_auth && serv_list)
        {
            DefaultAuthorityDescriptor authority(def_auth);
            ServiceListDescriptor services(serv_list);

            for (uint j = 0; j < services.ServiceCount(); j++)
            {
                // If the default authority is given in the SDT this
                // overrides any definition in the BAT (or in the NIT)
                LOG(VB_CHANSCAN, LOG_INFO, LOC +
                    QString("found default authority(BAT) for service %1 %2 %3")
                        .arg(netid).arg(tsid).arg(services.ServiceID(j)));
               uint64_t index = ((uint64_t)netid << 32) | (tsid << 16) |
                                 services.ServiceID(j);
               if (! defAuthorities.contains(index))
                   defAuthorities[index] = authority.DefaultAuthority();
            }
        }
    }
}

void ChannelScanSM::HandleSDTo(uint tsid, const ServiceDescriptionTable *sdt)
{
    QMutexLocker locker(&lock);

    LOG(VB_CHANSCAN, LOG_INFO, LOC +
        "Got a Service Description Table (other)\n" + sdt->toString());

    otherTableTime = timer.elapsed() + otherTableTimeout;

    uint netid = sdt->OriginalNetworkID();

    for (uint i = 0; i < sdt->ServiceCount(); i++)
    {
        uint serviceId = sdt->ServiceID(i);
        desc_list_t parsed =
            MPEGDescriptor::Parse(sdt->ServiceDescriptors(i),
                                  sdt->ServiceDescriptorsLength(i));
        // Look for default authority
        const unsigned char *def_auth =
            MPEGDescriptor::Find(parsed, DescriptorID::default_authority);
        if (def_auth)
        {
            DefaultAuthorityDescriptor authority(def_auth);
            LOG(VB_CHANSCAN, LOG_INFO, LOC +
                QString("found default authority(SDTo) for service %1 %2 %3")
                    .arg(netid).arg(tsid).arg(serviceId));
            defAuthorities[((uint64_t)netid << 32) | (tsid << 16) | serviceId] =
                authority.DefaultAuthority();
        }
    }
}

void ChannelScanSM::HandleEncryptionStatus(uint pnum, bool encrypted)
{
    QMutexLocker locker(&lock);

    currentEncryptionStatus[pnum] = encrypted ? kEncEncrypted : kEncDecrypted;

    if (kEncDecrypted == currentEncryptionStatus[pnum])
        currentTestingDecryption = false;

    UpdateChannelInfo(true);
}

bool ChannelScanSM::TestNextProgramEncryption(void)
{
    if (!currentInfo || currentInfo->pmts.empty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Can't monitor decryption -- no pmts");
        currentTestingDecryption = false;
        return false;
    }

    do
    {
        uint pnum = 0;
        QMap<uint, uint>::const_iterator it = currentEncryptionStatus.begin();
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("%1/%2 checked")
            .arg(currentEncryptionStatusChecked.size())
            .arg(currentEncryptionStatus.size()));
#endif
        while (it != currentEncryptionStatus.end())
        {
            if (!currentEncryptionStatusChecked[it.key()])
            {
                pnum = it.key();
                break;
            }
            ++it;
        }

        if (!pnum)
            break;

        currentEncryptionStatusChecked[pnum] = true;

        if (!m_test_decryption)
        {
            currentEncryptionStatus[pnum] = kEncEncrypted;
            continue;
        }

        const ProgramMapTable *pmt = NULL;
        for (uint i = 0; !pmt && (i < currentInfo->pmts.size()); i++)
        {
            pmt = (currentInfo->pmts[i]->ProgramNumber() == pnum) ?
                currentInfo->pmts[i] : NULL;
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
            LOG(VB_CHANSCAN, LOG_INFO, LOC + msg);

#ifdef USING_DVB
            if (GetDVBChannel())
                GetDVBChannel()->SetPMT(pmt);
#endif // USING_DVB

            GetDTVSignalMonitor()->GetStreamData()->TestDecryption(pmt);

            currentTestingDecryption = true;
            timer.start();
            return true;
        }

        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Can't monitor decryption of program %1 -- no pmt")
                .arg(pnum));

    } while (true);

    currentTestingDecryption = false;
    return false;
}

DTVTunerType ChannelScanSM::GuessDTVTunerType(DTVTunerType type) const
{
    if (scanDTVTunerType != (int)DTVTunerType::kTunerTypeUnknown)
        type = scanDTVTunerType;

    const DTVChannel *chan = GetDTVChannel();

    if (!chan)
        return type;

    vector<DTVTunerType> tts = chan->GetTunerTypes();

    for (uint i = 0; i < tts.size(); ++i)
    {
        if (tts[i] == type)
            return type;
    }

    if (!tts.empty())
        return tts[0];

    return type;
}

void ChannelScanSM::UpdateScanTransports(const NetworkInformationTable *nit)
{
    for (uint i = 0; i < nit->TransportStreamCount(); ++i)
    {
        uint32_t tsid  = nit->TSID(i);
        uint32_t netid = nit->OriginalNetworkID(i);
        uint32_t id    = netid << 16 | tsid;

        if (ts_scanned.contains(id) || extend_transports.contains(id))
            continue;

        const desc_list_t& list =
            MPEGDescriptor::Parse(nit->TransportDescriptors(i),
                                  nit->TransportDescriptorsLength(i));

        for (uint j = 0; j < list.size(); ++j)
        {
            int mplexid = -1;
            uint64_t frequency = 0;
            const MPEGDescriptor desc(list[j]);
            uint tag = desc.DescriptorTag();
            DTVTunerType tt = DTVTunerType::kTunerTypeUnknown;

            switch (tag)
            {
                case DescriptorID::terrestrial_delivery_system:
                {
                    const TerrestrialDeliverySystemDescriptor cd(desc);
                    frequency = cd.FrequencyHz();
                    tt = DTVTunerType::kTunerTypeDVBT;
                    break;
                }
                case DescriptorID::satellite_delivery_system:
                {
                    const SatelliteDeliverySystemDescriptor cd(desc);
                    frequency = cd.FrequencyHz()/1000;
                    tt = DTVTunerType::kTunerTypeDVBS1;
                    break;
                }
                case DescriptorID::cable_delivery_system:
                {
                    const CableDeliverySystemDescriptor cd(desc);
                    frequency = cd.FrequencyHz();
                    tt = DTVTunerType::kTunerTypeDVBC;
                    break;
                }
                default:
                    LOG(VB_CHANSCAN, LOG_ERR, LOC +
                        "unknown delivery system descriptor");
                    continue;
            }

            mplexid = ChannelUtil::GetMplexID(sourceID, frequency, tsid, netid);
            mplexid = max(0, mplexid);

            tt = GuessDTVTunerType(tt);

            DTVMultiplex tuning;
            if (mplexid)
            {
                if (!tuning.FillFromDB(tt, mplexid))
                    continue;
            }
            else if (!tuning.FillFromDeliverySystemDesc(tt, desc))
            {
                continue;
            }

            extend_transports[id] = tuning;
            break;
        }
    }
}

bool ChannelScanSM::UpdateChannelInfo(bool wait_until_complete)
{
    if (current == scanTransports.end())
        return true;

    if (wait_until_complete && currentTestingDecryption)
        return false;

    DTVSignalMonitor *dtv_sm = GetDTVSignalMonitor();
    if (!dtv_sm)
        return false;

    const ScanStreamData *sd = dtv_sm->GetScanStreamData();

    if (!currentInfo)
        currentInfo = new ScannedChannelInfo();

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
        if (currentInfo->pats.contains(tsid))
            continue;

        if (!wait_until_complete || sd->HasCachedAllPAT(tsid))
        {
            currentInfo->pats[tsid] = sd->GetCachedPATs(tsid);
            if (!currentInfo->pmts.empty())
            {
                sd->ReturnCachedPMTTables(currentInfo->pmts);
                currentInfo->pmts.clear();
            }
        }
        else
            transport_tune_complete = false;
    }
    transport_tune_complete &= !pattmp.empty();
    sd->ReturnCachedPATTables(pattmp);

    // Grab PMT tables
    if ((!wait_until_complete || sd->HasCachedAllPMTs()) &&
        currentInfo->pmts.empty())
        currentInfo->pmts = sd->GetCachedPMTs();

    // ATSC
    if (!currentInfo->mgt && sd->HasCachedMGT())
        currentInfo->mgt = sd->GetCachedMGT();

    if ((!wait_until_complete || sd->HasCachedAllCVCTs()) &&
        currentInfo->cvcts.empty())
    {
        currentInfo->cvcts = sd->GetCachedCVCTs();
    }

    if ((!wait_until_complete || sd->HasCachedAllTVCTs()) &&
        currentInfo->tvcts.empty())
    {
        currentInfo->tvcts = sd->GetCachedTVCTs();
    }

    // DVB
    if ((!wait_until_complete || sd->HasCachedAllNIT()) &&
        (currentInfo->nits.empty() ||
        timer.elapsed() > (int)otherTableTime))
    {
        currentInfo->nits = sd->GetCachedNIT();
    }

    sdt_vec_t sdttmp = sd->GetCachedSDTs();
    tsid_checked.clear();
    for (uint i = 0; i < sdttmp.size(); i++)
    {
        uint tsid = sdttmp[i]->TSID();
        if (tsid_checked[tsid])
            continue;
        tsid_checked[tsid] = true;
        if (currentInfo->sdts.contains(tsid))
            continue;

        if (!wait_until_complete || sd->HasCachedAllSDT(tsid))
            currentInfo->sdts[tsid] = sd->GetCachedSDTs(tsid);
    }
    sd->ReturnCachedSDTTables(sdttmp);

    // Check if transport tuning is complete
    if (transport_tune_complete)
    {
        transport_tune_complete &= !currentInfo->pmts.empty();
        if (sd->HasCachedMGT() || sd->HasCachedAnyVCTs())
        {
            transport_tune_complete &= sd->HasCachedMGT();
            transport_tune_complete &=
                (!currentInfo->tvcts.empty() || !currentInfo->cvcts.empty());
        }
        if (sd->HasCachedAnyNIT() || sd->HasCachedAnySDTs())
        {
            transport_tune_complete &= !currentInfo->nits.empty();
            transport_tune_complete &= !currentInfo->sdts.empty();
        }
        if (transport_tune_complete)
        {
            LOG(VB_CHANSCAN, LOG_INFO, LOC +
                QString("transport_tune_complete: "
                        "\n\t\t\tcurrentInfo->pmts.empty():     %1"
                        "\n\t\t\tsd->HasCachedAnyNIT():         %2"
                        "\n\t\t\tsd->HasCachedAnySDTs():        %3"
                        "\n\t\t\tcurrentInfo->nits.empty():     %4"
                        "\n\t\t\tcurrentInfo->sdts.empty():     %5")
                    .arg(currentInfo->pmts.empty())
                    .arg(sd->HasCachedAnyNIT())
                    .arg(sd->HasCachedAnySDTs())
                    .arg(currentInfo->nits.empty())
                    .arg(currentInfo->sdts.empty()));
        }
    }
    transport_tune_complete |= !wait_until_complete;
    if (transport_tune_complete)
    {
        LOG(VB_CHANSCAN, LOG_INFO, LOC +
            QString("transport_tune_complete: wait_until_complete %1")
                .arg(wait_until_complete));
    }

    if (transport_tune_complete &&
        /*!ignoreEncryptedServices &&*/ currentEncryptionStatus.size())
    {
        //GetDTVSignalMonitor()->GetStreamData()->StopTestingDecryption();

        if (TestNextProgramEncryption())
            return false;

        QMap<uint, uint>::const_iterator it = currentEncryptionStatus.begin();
        for (; it != currentEncryptionStatus.end(); ++it)
        {
            currentInfo->program_encryption_status[it.key()] = *it;

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
            LOG(VB_CHANSCAN, LOG_INFO, LOC + msg);
        }
    }

    // append transports from the NIT to the scan list
    if (transport_tune_complete && extend_scan_list &&
        !currentInfo->nits.empty())
    {
        // append delivery system descriptos to scan list
        nit_vec_t::const_iterator it = currentInfo->nits.begin();
        while (it != currentInfo->nits.end())
        {
            UpdateScanTransports(*it);
            ++it;
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

        if (!currentInfo->IsEmpty())
        {
            LOG(VB_CHANSCAN, LOG_INFO, LOC +
                QString("Adding %1, offset %2 to channelList.")
                    .arg((*current).tuning.toString()).arg(current.offset()));
            channelList << ChannelListItem(current, currentInfo);
            currentInfo = NULL;
        }
        else
        {
            delete currentInfo;
            currentInfo = NULL;
        }

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
        LOG(VB_CHANSCAN, LOG_INFO, LOC + msg);

        currentEncryptionStatus.clear();
        currentEncryptionStatusChecked.clear();

        setOtherTables = false;
        otherTableTime = 0;

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
                        const ServiceDescriptionTable *sdt, uint i,
                        const QMap<uint64_t, QString> &defAuthorities)
{
    // HACK beg -- special exception for this network
    //             (dbver == "1067")
    bool force_guide_present = (sdt->OriginalNetworkID() == 70);
    // HACK end -- special exception for this network

    // Figure out best service name and callsign...
    ServiceDescriptor *desc = sdt->GetServiceDescriptor(i);
    QString callsign = QString::null;
    QString service_name = QString::null;
    if (desc)
    {
        callsign = desc->ServiceShortName();
        if (callsign.trimmed().isEmpty())
            callsign = QString("%1-%2-%3")
                .arg(ChannelUtil::GetUnknownCallsign()).arg(sdt->TSID())
                .arg(sdt->ServiceID(i));

        service_name = desc->ServiceName();
        if (service_name.trimmed().isEmpty())
            service_name = QString::null;
    }

    if (info.callsign.isEmpty())
        info.callsign = callsign;
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
    delete desc;

    info.service_id = sdt->ServiceID(i);
    info.sdt_tsid   = sdt->TSID();
    info.orig_netid = sdt->OriginalNetworkID();
    info.in_sdt     = true;

    desc_list_t parsed =
        MPEGDescriptor::Parse(sdt->ServiceDescriptors(i),
                                sdt->ServiceDescriptorsLength(i));
    // Look for default authority
    const unsigned char *def_auth =
        MPEGDescriptor::Find(parsed, DescriptorID::default_authority);
    if (def_auth)
    {
        DefaultAuthorityDescriptor authority(def_auth);
        LOG(VB_CHANSCAN, LOG_INFO, QString("ChannelScanSM: found default "
                                          "authority(SDT) for service %1 %2 %3")
                .arg(info.orig_netid).arg(info.sdt_tsid).arg(info.service_id));
        info.default_authority = authority.DefaultAuthority();
    }
    else
    {
        uint64_t index = (uint64_t)info.orig_netid << 32 |
                        info.sdt_tsid << 16 | info.service_id;
        if (defAuthorities.contains(index))
            info.default_authority = defAuthorities[index];
    }
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

    QMap<uint,ChannelInsertInfo> list = GetChannelList(current, currentInfo);
    {
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
ChannelScanSM::GetChannelList(transport_scan_items_it_t trans_info,
                              ScannedChannelInfo *scan_info) const
{
    QMap<uint,ChannelInsertInfo> pnum_to_dbchan;

    uint    mplexid   = (*trans_info).mplexid;
    int     freqid    = (*trans_info).friendlyNum;
    QString freqidStr = (freqid) ? QString::number(freqid) : QString::null;
    QString iptv_channel = (*trans_info).iptv_channel;

    // channels.conf
    const DTVChannelInfoList &echan = (*trans_info).expectedChannels;
    for (uint i = 0; i < echan.size(); i++)
    {
        uint pnum = echan[i].serviceid;
        PCM_INFO_INIT("mpeg");
        info.service_name = echan[i].name;
        info.in_channels_conf = true;
    }

    // PATs
    pat_map_t::const_iterator pat_list_it = scan_info->pats.begin();
    for (; pat_list_it != scan_info->pats.end(); ++pat_list_it)
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
                    info.could_be_opencable = could_be_opencable;
                    info.in_pat = true;
                }
            }
        }
    }

    // PMTs
    pmt_vec_t::const_iterator pmt_it = scan_info->pmts.begin();
    for (; pmt_it != scan_info->pmts.end(); ++pmt_it)
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
            if (reg.FormatIdentifierString() == "CUEI" ||
                reg.FormatIdentifierString() == "SCTE")
                info.is_opencable = true;
        }

        info.is_encrypted |= pmt->IsEncrypted(GetDTVChannel()->GetSIStandard());
        info.in_pmt = true;
    }

    // Cable VCTs
    cvct_vec_t::const_iterator cvct_it = scan_info->cvcts.begin();
    for (; cvct_it != scan_info->cvcts.end(); ++cvct_it)
    {
        for (uint i = 0; i < (*cvct_it)->ChannelCount(); i++)
        {
            uint pnum = (*cvct_it)->ProgramNumber(i);
            PCM_INFO_INIT("atsc");
            update_info(info, *cvct_it, i);
        }
    }

    // Terrestrial VCTs
    tvct_vec_t::const_iterator tvct_it = scan_info->tvcts.begin();
    for (; tvct_it != scan_info->tvcts.end(); ++tvct_it)
    {
        for (uint i = 0; i < (*tvct_it)->ChannelCount(); i++)
        {
            uint pnum = (*tvct_it)->ProgramNumber(i);
            PCM_INFO_INIT("atsc");
            update_info(info, *tvct_it, i);
        }
    }

    // SDTs
    sdt_map_t::const_iterator sdt_list_it = scan_info->sdts.begin();
    for (; sdt_list_it != scan_info->sdts.end(); ++sdt_list_it)
    {
        sdt_vec_t::const_iterator sdt_it = (*sdt_list_it).begin();
        for (; sdt_it != (*sdt_list_it).end(); ++sdt_it)
        {
            for (uint i = 0; i < (*sdt_it)->ServiceCount(); i++)
            {
                uint pnum = (*sdt_it)->ServiceID(i);
                PCM_INFO_INIT("dvb");
                update_info(info, *sdt_it, i, defAuthorities);
            }
        }
    }

    // NIT
    QMap<qlonglong, uint> ukChanNums;
    QMap<uint,ChannelInsertInfo>::iterator dbchan_it;
    for (dbchan_it = pnum_to_dbchan.begin();
         dbchan_it != pnum_to_dbchan.end(); ++dbchan_it)
    {
        ChannelInsertInfo &info = *dbchan_it;

        // NIT
        nit_vec_t::const_iterator nits_it = scan_info->nits.begin();
        for (; nits_it != scan_info->nits.end(); ++nits_it)
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
                else
                {
                    continue;
                }

                // Get channel numbers from UK Frequency List Descriptors
                const desc_list_t &list =
                    MPEGDescriptor::Parse(nit->TransportDescriptors(i),
                                          nit->TransportDescriptorsLength(i));

                const unsigned char *desc =
                    MPEGDescriptor::Find(
                        list, PrivateDescriptorID::dvb_uk_channel_list);

                if (desc)
                {
                    UKChannelListDescriptor uklist(desc);
                    for (uint j = 0; j < uklist.ChannelCount(); j++)
                    {
                        ukChanNums[((qlonglong)info.orig_netid<<32) |
                                   uklist.ServiceID(j)] =
                            uklist.ChannelNumber(j);
                    }
                }
            }
        }
    }

    // Get IPTV or UK channel numbers
    for (dbchan_it = pnum_to_dbchan.begin();
         dbchan_it != pnum_to_dbchan.end(); ++dbchan_it)
    {
        ChannelInsertInfo &info = *dbchan_it;

        if (!info.chan_num.isEmpty())
            continue;

        if (iptv_channel.isEmpty()) // UK channel numbers
        {
            QMap<qlonglong, uint>::const_iterator it = ukChanNums.find
                       (((qlonglong)info.orig_netid<<32) | info.service_id);

            if (it != ukChanNums.end())
                info.chan_num = QString::number(*it);
        }
        else // IPTV programs
        {
            info.chan_num = iptv_channel;
            if (info.service_id)
                info.chan_num += "-" + QString::number(info.service_id);
        }

        LOG(VB_CHANSCAN, LOG_INFO, LOC +
            QString("GetChannelList: set chan_num '%1'").arg(info.chan_num));
    }

    // Get QAM/SCTE/MPEG channel numbers
    for (dbchan_it = pnum_to_dbchan.begin();
         dbchan_it != pnum_to_dbchan.end(); ++dbchan_it)
    {
        ChannelInsertInfo &info = *dbchan_it;

        if (!info.chan_num.isEmpty())
            continue;

        if ((info.si_standard == "mpeg") ||
            (info.si_standard == "scte") ||
            (info.si_standard == "opencable"))
            info.chan_num = QString("%1-%2")
                              .arg(info.freqid)
                              .arg(info.service_id);
    }

    // Check for decryption success
    for (dbchan_it = pnum_to_dbchan.begin();
         dbchan_it != pnum_to_dbchan.end(); ++dbchan_it)
    {
        uint pnum = dbchan_it.key();
        ChannelInsertInfo &info = *dbchan_it;
        info.decryption_status = scan_info->program_encryption_status[pnum];
    }

    return pnum_to_dbchan;
}

ScanDTVTransportList ChannelScanSM::GetChannelList(void) const
{
    ScanDTVTransportList list;

    uint cardid = channel->GetCardID();

    DTVTunerType tuner_type = GuessDTVTunerType(DTVTunerType::kTunerTypeATSC);

    ChannelList::const_iterator it = channelList.begin();
    for (; it != channelList.end(); ++it)
    {
        QMap<uint,ChannelInsertInfo> pnum_to_dbchan =
            GetChannelList(it->first, it->second);

        ScanDTVTransport item((*it->first).tuning, tuner_type, cardid);
        item.iptv_tuning = (*(it->first)).iptv_tuning;

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

const DTVChannel *ChannelScanSM::GetDTVChannel(void) const
{
    return dynamic_cast<const DTVChannel*>(channel);
}

HDHRChannel *ChannelScanSM::GetHDHRChannel(void)
{
#ifdef USING_HDHOMERUN
    return dynamic_cast<HDHRChannel*>(channel);
#else
    return NULL;
#endif
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
#ifdef USING_V4L2
    return dynamic_cast<V4LChannel*>(channel);
#else
    return NULL;
#endif
}

/** \fn ChannelScanSM::StartScanner(void)
 *  \brief Starts the ChannelScanSM event loop.
 */
void ChannelScanSM::StartScanner(void)
{
    while (scannerThread)
    {
        threadExit = true;
        if (scannerThread->wait(1000))
        {
            delete scannerThread;
            scannerThread = NULL;
        }
    }
    threadExit = false;
    scannerThread = new MThread("Scanner", this);
    scannerThread->start();
}

/** \fn ChannelScanSM::run(void)
 *  \brief This runs the event loop for ChannelScanSM until 'threadExit' is true.
 */
void ChannelScanSM::run(void)
{
    LOG(VB_CHANSCAN, LOG_INFO, LOC + "run -- begin");

    while (!threadExit)
    {
        if (scanning)
            HandleActiveScan();

        usleep(10 * 1000);
    }

    LOG(VB_CHANSCAN, LOG_INFO, LOC + "run -- end");
}

// See if we have timed out
bool ChannelScanSM::HasTimedOut(void)
{
    if (currentTestingDecryption &&
        (timer.elapsed() > (int)kDecryptionTimeout))
    {
        currentTestingDecryption = false;
        return true;
    }

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
    {
        // the channelTimeout alone is only valid if we have seen no tables..
        const ScanStreamData *sd = NULL;
        if (GetDTVSignalMonitor())
            sd = GetDTVSignalMonitor()->GetScanStreamData();

        if (!sd)
            return true;

        if (sd->HasCachedAnyNIT() || sd->HasCachedAnySDTs())
            return timer.elapsed() > (int) kDVBTableTimeout;
        if (sd->HasCachedMGT() || sd->HasCachedAnyVCTs())
            return timer.elapsed() > (int) kATSCTableTimeout;
        if (sd->HasCachedAnyPAT() || sd->HasCachedAnyPMTs())
            return timer.elapsed() > (int) kMPEGTableTimeout;

        return true;
    }

    // ok the tables haven't timed out, but have we hit the signal timeout?
    SignalMonitor *sm = GetSignalMonitor();
    if ((timer.elapsed() > (int)(*current).timeoutTune) &&
        sm && !sm->HasSignalLock())
    {
        const ScanStreamData *sd = NULL;
        if (GetDTVSignalMonitor())
            sd = GetDTVSignalMonitor()->GetScanStreamData();

        if (!sd)
            return true;

        // Just is case we temporarily lose the signal after we've seen
        // tables...
        if (!sd->HasCachedAnyPAT() && !sd->HasCachedAnyPMTs() &&
            !sd->HasCachedMGT()    && !sd->HasCachedAnyVCTs() &&
            !sd->HasCachedAnyNIT() && !sd->HasCachedAnySDTs())
        {
            return true;
        }
    }

    return false;
}

/** \fn ChannelScanSM::HandleActiveScan(void)
 *  \brief Handles the TRANSPORT_LIST ChannelScanSM mode.
 */
void ChannelScanSM::HandleActiveScan(void)
{
    QMutexLocker locker(&lock);

    bool do_post_insertion = waitingForTables;

    if (!HasTimedOut())
        return;

    if (0 == nextIt.offset() && nextIt != scanTransports.begin())
    {
        // Add channel to scanned list and potentially check decryption
        if (do_post_insertion && !UpdateChannelInfo(false))
            return;

        // Stop signal monitor for previous transport
        locker.unlock();
        signalMonitor->Stop();
        locker.relock();
    }

    if (0 == nextIt.offset() && nextIt == scanTransports.begin())
    {
        channelList.clear();
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
    else if (!extend_transports.isEmpty())
    {
        --current;
        QMap<uint32_t,DTVMultiplex>::iterator it = extend_transports.begin();
        while (it != extend_transports.end())
        {
            if (!ts_scanned.contains(it.key()))
            {
                QString name = QString("TransportID %1").arg(it.key() & 0xffff);
                TransportScanItem item(sourceID, name, *it, signalTimeout);
                LOG(VB_CHANSCAN, LOG_INFO, LOC + "Adding " + name + " - " +
                    item.tuning.toString());
                scanTransports.push_back(item);
                ts_scanned.insert(it.key());
            }
            ++it;
        }
        extend_transports.clear();
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

bool ChannelScanSM::Tune(const transport_scan_items_it_t &transport)
{
    const TransportScanItem &item = *transport;

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

    if (item.mplexid > 0 && transport.offset() == 0)
        return GetDTVChannel()->TuneMultiplex(item.mplexid, inputname);

    if (item.tuning.sistandard == "MPEG")
        return GetDTVChannel()->Tune(item.iptv_tuning, true);

    const uint64_t freq = item.freq_offset(transport.offset());
    DTVMultiplex tuning = item.tuning;
    tuning.frequency = freq;
    return GetDTVChannel()->Tune(tuning, inputname);
}

void ChannelScanSM::ScanTransport(const transport_scan_items_it_t &transport)
{
    QString offset_str = (transport.offset()) ?
        QObject::tr(" offset %2").arg(transport.offset()) : "";
    QString cur_chan = QString("%1%2")
        .arg((*current).FriendlyName).arg(offset_str);
    QString tune_msg_str =
        QObject::tr("ScanTransport Tuning to %1 mplexid(%2)")
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
        QString progress = QObject::tr(": Found %n", "", channelsFound);
        scan_monitor->ScanUpdateStatusTitleText(progress);
    }

    scan_monitor->ScanUpdateStatusText(cur_chan);
    LOG(VB_CHANSCAN, LOG_INFO, LOC + tune_msg_str);

    if (!Tune(transport))
    {   // If we did not tune successfully, bail with message
        UpdateScanPercentCompleted();
        LOG(VB_CHANSCAN, LOG_ERR, LOC +
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
    waitingForTables = (item.tuning.sistandard != "analog");
}

/** \fn ChannelScanSM::StopScanner(void)
 *  \brief Stops the ChannelScanSM event loop and the signal monitor,
 *         blocking until both exit.
 */
void ChannelScanSM::StopScanner(void)
{
    LOG(VB_CHANSCAN, LOG_INFO, LOC + "StopScanner");

    while (scannerThread)
    {
        threadExit = true;
        if (scannerThread->wait(1000))
        {
            delete scannerThread;
            scannerThread = NULL;
        }
    }

    if (signalMonitor)
        signalMonitor->Stop();
}

/**
 *  \brief Generates a list of frequencies to scan and adds it to the
 *   scanTransport list, and then sets the scanning to TRANSPORT_LIST.
 */
bool ChannelScanSM::ScanTransports(
    int SourceID,
    const QString &std,
    const QString &modulation,
    const QString &country,
    const QString &table_start,
    const QString &table_end)
{
    QString name("");
    if (scanning)
        return false;

    scanTransports.clear();
    nextIt = scanTransports.end();

    freq_table_list_t tables =
        get_matching_freq_tables(std, modulation, country);

    if (tables.size() == 0)
    {
        QString msg = QString("No freq table for (%1, %2, %3) found")
                      .arg(std).arg(modulation).arg(country);
        scan_monitor->ScanAppendTextToLog(msg);
    }
    LOG(VB_CHANSCAN, LOG_INFO, LOC +
        QString("Looked up freq table (%1, %2, %3) w/%4 entries")
            .arg(std).arg(modulation).arg(country).arg(tables.size()));

    QString start = table_start;
    QString end   = table_end;
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
            if (strNameFormat.indexOf("%") >= 0)
                name = strNameFormat.arg(name_num);

            if (start.isEmpty() || name == start)
            {
                start = QString::null;

                TransportScanItem item(SourceID, std, name, name_num,
                                       freq, ft, signalTimeout);
                scanTransports.push_back(item);

                LOG(VB_CHANSCAN, LOG_INFO, LOC + "ScanTransports " +
                    item.toString());
            }

            name_num++;
            freq += ft.frequencyStep;

            if (!end.isEmpty() && name == end)
                break;
        }
        if (!end.isEmpty() && name == end)
            break;
    }

    while (!tables.empty())
    {
        delete tables.back();
        tables.pop_back();
    }

    extend_scan_list = true;
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

        LOG(VB_CHANSCAN, LOG_INFO, LOC + "ScanForChannels " + item.toString());
    }

    if (scanTransports.empty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "ScanForChannels() no transports");
        return false;
    }

    timer.start();
    waitingForTables = false;

    nextIt            = scanTransports.begin();
    transportsScanned = 0;
    scanning          = true;

    return true;
}

bool ChannelScanSM::ScanIPTVChannels(uint sourceid,
                                     const fbox_chan_map_t &iptv_channels)
{
    scanTransports.clear();
    nextIt = scanTransports.end();

    fbox_chan_map_t::const_iterator Ichan = iptv_channels.begin();
    for (uint idx = 0; Ichan != iptv_channels.end(); ++Ichan, ++idx)
    {
        TransportScanItem item(sourceid, QString::number(idx),
                               Ichan.value().m_tuning, Ichan.key(),
                               signalTimeout);

        scanTransports.push_back(item);

        LOG(VB_CHANSCAN, LOG_INFO, LOC + "ScanIPTVChannels " + item.toString());
    }

    if (scanTransports.empty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "ScanIPTVChannels() no transports");
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
bool ChannelScanSM::ScanTransportsStartingOn(
    int sourceid, const QMap<QString,QString> &startChan)
{
    QMap<QString,QString>::const_iterator it;

    if (startChan.find("std")        == startChan.end() ||
        startChan.find("type")       == startChan.end())
    {
        return false;
    }

    QString std    = *startChan.find("std");
    QString si_std = (std.toLower() != "atsc") ? "dvb" : "atsc";
    bool    ok     = false;

    if (scanning)
        return false;

    scanTransports.clear();
    nextIt = scanTransports.end();

    DTVMultiplex tuning;

    DTVTunerType type;
    ok = type.Parse(startChan["type"]);

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
            startChan["modulation"],     startChan["bandwidth"],
            startChan["mod_sys"],        startChan["rolloff"]);
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

    extend_scan_list = true;

    timer.start();
    waitingForTables = false;

    nextIt            = scanTransports.begin();
    transportsScanned = 0;
    scanning          = true;

    return true;
}

bool ChannelScanSM::AddToList(uint mplexid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT sourceid, sistandard, transportid, frequency, modulation "
        "FROM dtv_multiplex "
        "WHERE mplexid = :MPLEXID");
    query.bindValue(":MPLEXID", mplexid);
    if (!query.exec())
    {
        MythDB::DBError("ChannelScanSM::AddToList()", query);
        return false;
    }

    if (!query.next())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "AddToList() " +
                QString("Failed to locate mplexid(%1) in DB").arg(mplexid));
        return false;
    }

    uint    sourceid   = query.value(0).toUInt();
    QString sistandard = query.value(1).toString();
    uint    tsid       = query.value(2).toUInt();
    DTVTunerType tt = DTVTunerType::kTunerTypeUnknown;

    QString fn = (tsid) ? QString("Transport ID %1").arg(tsid) :
        QString("Multiplex #%1").arg(mplexid);

    if (query.value(4).toString() == "8vsb")
    {
        QString chan = QString("%1 Hz").arg(query.value(3).toInt());
        struct CHANLIST *curList = chanlists[0].list;
        int totalChannels = chanlists[0].count;
        int findFrequency = (query.value(3).toInt() / 1000) - 1750;
        for (int x = 0 ; x < totalChannels ; x++)
        {
            if ((curList[x].freq <= findFrequency + 200) &&
                (curList[x].freq >= findFrequency - 200))
            {
                chan = QString("%1").arg(curList[x].name);
            }
        }
        fn = QObject::tr("ATSC Channel %1").arg(chan);
        tt = DTVTunerType::kTunerTypeATSC;
    }

    tt = GuessDTVTunerType(tt);

    TransportScanItem item(sourceid, sistandard, fn, mplexid, signalTimeout);

    if (item.tuning.FillFromDB(tt, mplexid))
    {
        LOG(VB_CHANSCAN, LOG_INFO, LOC + "Adding " + fn);
        scanTransports.push_back(item);
        return true;
    }

    LOG(VB_CHANSCAN, LOG_INFO, LOC + "Not adding incomplete transport " + fn);
    return false;
}

bool ChannelScanSM::ScanTransport(uint mplexid, bool follow_nit)
{
    scanTransports.clear();
    nextIt = scanTransports.end();

    AddToList(mplexid);

    timer.start();
    waitingForTables  = false;

    extend_scan_list = follow_nit;
    transportsScanned = 0;
    if (!scanTransports.empty())
    {
        nextIt   = scanTransports.begin();
        scanning = true;
        return true;
    }

    return false;
}

bool ChannelScanSM::ScanCurrentTransport(const QString &sistandard)
{
    scanTransports.clear();
    nextIt = scanTransports.end();

    signalTimeout = 30000;
    QString name;
    TransportScanItem item(sourceID, sistandard, name, 0, signalTimeout);
    scanTransports.push_back(item);

    timer.start();
    waitingForTables = false;
    extend_scan_list = false;
    transportsScanned = 0;
    nextIt   = scanTransports.begin();
    scanning = true;
    return true;
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
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("comparing %1 %2 against %3 %4")
                .arg(channels[i].serviceid).arg(channels[i].name)
                .arg(mpeg_program_num).arg(common_status_info));

        if (channels[i].serviceid == mpeg_program_num)
        {
            found = true;
            if (!channels[i].name.isEmpty())
            {
                service_name = channels[i].name; service_name.detach();
                callsign     = channels[i].name; callsign.detach();
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
