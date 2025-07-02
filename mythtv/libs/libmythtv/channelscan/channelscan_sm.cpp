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
#include <utility>

// Qt includes
#include <QMutexLocker>
#include <QObject>

// MythTV includes - Other Libs
#include "libmythbase/mthread.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythlogging.h"

// MythTV includes - General
#include "cardutil.h"
#include "channelscan_sm.h"
#include "channelutil.h"
#include "frequencies.h"
#include "scanwizardconfig.h"
#include "sourceutil.h"

// MythTV includes - DTV
#include "mpeg/scanstreamdata.h"
#include "recorders/dtvsignalmonitor.h"

// MythTV includes - ATSC
#include "mpeg/atsctables.h"

// MythTV includes - DVB
#include "mpeg/dvbtables.h"
#include "recorders/dvbsignalmonitor.h"

#include "recorders/dvbchannel.h"
#include "recorders/hdhrchannel.h"
#include "recorders/v4lchannel.h"

/// SDT's should be sent every 2 seconds and NIT's every
/// 10 seconds, so lets wait at least 30 seconds, in
/// case of bad transmitter or lost packets.
const std::chrono::milliseconds ChannelScanSM::kDVBTableTimeout  = 30s;
/// No logic here, lets just wait at least 10 seconds.
const std::chrono::milliseconds ChannelScanSM::kATSCTableTimeout = 10s;
/// No logic here, lets just wait at least 15 seconds.
const std::chrono::milliseconds ChannelScanSM::kMPEGTableTimeout = 15s;

// Freesat and Sky
static const uint kRegionUndefined = 0xFFFF;        // Not regional

QString ChannelScanSM::loc(const ChannelScanSM *siscan)
{
    if (siscan && siscan->m_channel)
        return QString("ChannelScanSM[%1]").arg(siscan->m_channel->GetInputID());
    return "ChannelScanSM(u)";
}

#define LOC     (ChannelScanSM::loc(this) + ": ")

static constexpr qint64 kDecryptionTimeout { 4250 };

static const QString kATSCChannelFormat = "%1.%2";

class ScannedChannelInfo
{
  public:
    ScannedChannelInfo() = default;

    bool IsEmpty() const
    {
        return m_pats.empty()  && m_pmts.empty()     &&
               m_programEncryptionStatus.isEmpty()   &&
               !m_mgt          && m_cvcts.empty()    &&
               m_tvcts.empty() && m_nits.empty()     &&
               m_sdts.empty()  && m_bats.empty();
    }

    // MPEG
    pat_map_t               m_pats;
    pmt_vec_t               m_pmts;
    QMap<uint,uint>         m_programEncryptionStatus; // pnum->enc_status

    // ATSC
    const MasterGuideTable *m_mgt {nullptr};
    cvct_vec_t              m_cvcts;
    tvct_vec_t              m_tvcts;

    // DVB
    nit_vec_t               m_nits;
    sdt_map_t               m_sdts;
    bat_vec_t               m_bats;
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

ChannelScanSM::ChannelScanSM(ScanMonitor *scan_monitor,
                             const QString &cardtype, ChannelBase *channel,
                             int sourceID, std::chrono::milliseconds signal_timeout,
                             std::chrono::milliseconds channel_timeout, QString inputname,
                             bool test_decryption)
    : // Set in constructor
      m_scanMonitor(scan_monitor),
      m_channel(channel),
      m_signalMonitor(SignalMonitor::Init(cardtype, m_channel->GetInputID(),
                                          channel, true)),
      m_sourceID(sourceID),
      m_signalTimeout(signal_timeout),
      m_channelTimeout(channel_timeout),
      m_inputName(std::move(inputname)),
      m_testDecryption(test_decryption),
      // Misc
      m_analogSignalHandler(new AnalogSignalHandler(this))
{
    m_current = m_scanTransports.end();
    m_scanDTVTunerType = GuessDTVTunerType(DTVTunerType(DTVTunerType::kTunerTypeUnknown));

    // Create a stream data for digital signal monitors
    DTVSignalMonitor* dtvSigMon = GetDTVSignalMonitor();
    if (dtvSigMon)
    {
        LOG(VB_CHANSCAN, LOG_INFO, LOC + "Connecting up DTVSignalMonitor");
        auto *data = new ScanStreamData();

        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
                "SELECT dvb_nit_id, bouquet_id, region_id, lcnoffset "
                "FROM videosource "
                "WHERE videosource.sourceid = :SOURCEID");
        query.bindValue(":SOURCEID", m_sourceID);
        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("ChannelScanSM", query);
        }
        else if (query.next())
        {
            int nitid = query.value(0).toInt();
            data->SetRealNetworkID(nitid);
            LOG(VB_CHANSCAN, LOG_INFO, LOC +
                QString("Setting NIT-ID to %1").arg(nitid));

            m_bouquetId = query.value(1).toUInt();
            m_regionId  = query.value(2).toUInt();
            m_lcnOffset = query.value(3).toUInt();
            m_nitId     = nitid > 0 ? nitid : 0;
        }

        LOG(VB_CHANSCAN, LOG_INFO, LOC +
            QString("Freesat/Sky bouquet_id:%1 region_id:%2")
                .arg(m_bouquetId).arg(m_regionId));

        dtvSigMon->SetStreamData(data);
        dtvSigMon->AddFlags(SignalMonitor::kDTVSigMon_WaitForMGT |
                            SignalMonitor::kDTVSigMon_WaitForVCT |
                            SignalMonitor::kDTVSigMon_WaitForNIT |
                            SignalMonitor::kDTVSigMon_WaitForSDT);

#if CONFIG_DVB
        auto *dvbchannel = dynamic_cast<DVBChannel*>(m_channel);
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

    ScanStreamData *sd = nullptr;
    if (GetDTVSignalMonitor())
    {
        sd = GetDTVSignalMonitor()->GetScanStreamData();
    }

    if (m_signalMonitor)
    {
        m_signalMonitor->RemoveListener(m_analogSignalHandler);
        delete m_signalMonitor;
        m_signalMonitor = nullptr;
    }

    delete sd;

    if (m_analogSignalHandler)
    {
        delete m_analogSignalHandler;
        m_analogSignalHandler = nullptr;
    }

    teardown_frequency_tables();
}

void ChannelScanSM::SetAnalog(bool is_analog)
{
    m_signalMonitor->RemoveListener(m_analogSignalHandler);

    if (is_analog)
        m_signalMonitor->AddListener(m_analogSignalHandler);
}

void ChannelScanSM::HandleAllGood(void)
{
    QMutexLocker locker(&m_lock);

    QString cur_chan = (*m_current).m_friendlyName;
    QStringList list = cur_chan.split(" ", Qt::SkipEmptyParts);
    QString freqid = (list.size() >= 2) ? list[1] : cur_chan;

    QString msg = QObject::tr("Updated Channel %1").arg(cur_chan);

    if (!ChannelUtil::FindChannel(m_sourceID, freqid))
    {
        int chanid = ChannelUtil::CreateChanID(m_sourceID, freqid);

        QString callsign = QString("%1-%2")
            .arg(ChannelUtil::GetUnknownCallsign()).arg(chanid);

        bool ok = ChannelUtil::CreateChannel(
            0      /* mplexid */,
            m_sourceID,
            chanid,
            callsign,
            ""     /* service name       */,
            freqid /* channum            */,
            0      /* service id         */,
            0      /* ATSC major channel */,
            0      /* ATSC minor channel */,
            false  /* use on air guide   */,
            kChannelVisible /* visible   */,
            freqid);

        msg = (ok) ?
            QObject::tr("Added Channel %1").arg(cur_chan) :
            QObject::tr("Failed to add channel %1").arg(cur_chan);
    }
    else
    {
        // nothing to do here, XMLTV has better info
    }

    m_scanMonitor->ScanAppendTextToLog(msg);

    // tell UI we are done with these channels
    if (m_scanning)
    {
        UpdateScanPercentCompleted();
        m_waitingForTables = false;
        m_nextIt = m_current.nextTransport();
        m_dvbt2Tried = true;
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
    if (m_scanning)
        return false;

    m_scanTransports.clear();
    m_nextIt = m_scanTransports.end();

    std::vector<uint> multiplexes = SourceUtil::GetMplexIDs(sourceid);

    if (multiplexes.empty())
    {
        LOG(VB_CHANSCAN, LOG_ERR, LOC + "Unable to find any transports for " +
            QString("sourceid %1").arg(sourceid));

        return false;
    }

    LOG(VB_CHANSCAN, LOG_INFO, LOC +
        QString("Found %1 transports for ").arg(multiplexes.size()) +
        QString("sourceid %1").arg(sourceid));

    for (uint multiplex : multiplexes)
        AddToList(multiplex);

    m_extendScanList = follow_nit;
    m_waitingForTables  = false;
    m_transportsScanned = 0;
    if (!m_scanTransports.empty())
    {
        m_nextIt   = m_scanTransports.begin();
        m_scanning = true;
    }
    else
    {
        LOG(VB_CHANSCAN, LOG_ERR, LOC +
            "Unable to find add any transports for " +
            QString("sourceid %1").arg(sourceid));

        return false;
    }

    return m_scanning;
}

void ChannelScanSM::LogLines(const QString& string)
{
    if (VERBOSE_LEVEL_CHECK(VB_CHANSCAN, LOG_DEBUG))
    {
        QStringList lines = string.split('\n');
        for (const QString& line : std::as_const(lines))
            LOG(VB_CHANSCAN, LOG_DEBUG, line);
    }
}

void ChannelScanSM::HandlePAT(const ProgramAssociationTable *pat)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_CHANSCAN, LOG_INFO, LOC +
        QString("Got a Program Association Table for %1")
            .arg((*m_current).m_friendlyName));
    LogLines(pat->toString());

    // Add pmts to list, so we can do MPEG scan properly.
    DTVSignalMonitor *monitor = GetDTVSignalMonitor();
    if (nullptr == monitor)
        return;
    ScanStreamData *sd = monitor->GetScanStreamData();
    for (uint i = 0; i < pat->ProgramCount(); ++i)
    {
        sd->AddListeningPID(pat->ProgramPID(i));
    }
}

void ChannelScanSM::HandleCAT(const ConditionalAccessTable *cat)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_CHANSCAN, LOG_INFO, LOC +
        QString("Got a Conditional Access Table for %1")
            .arg((*m_current).m_friendlyName));
    LogLines(cat->toString());
}

void ChannelScanSM::HandlePMT(uint /*program_num*/, const ProgramMapTable *pmt)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("Got a Program Map Table for %1 program %2 (0x%3)")
            .arg((*m_current).m_friendlyName).arg(pmt->ProgramNumber())
            .arg(pmt->ProgramNumber(),4,16,QChar('0')));
    LogLines(pmt->toString());

    if (!m_currentTestingDecryption &&
        pmt->IsEncrypted(GetDTVChannel()->GetSIStandard()))
        m_currentEncryptionStatus[pmt->ProgramNumber()] = kEncUnknown;

    UpdateChannelInfo(true);
}

void ChannelScanSM::HandleVCT(uint /*pid*/, const VirtualChannelTable *vct)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_CHANSCAN, LOG_INFO, LOC +
        QString("Got a Virtual Channel Table for %1")
            .arg((*m_current).m_friendlyName));
    LogLines(vct->toString());

    for (uint i = 0; !m_currentTestingDecryption && i < vct->ChannelCount(); ++i)
    {
        if (vct->IsAccessControlled(i))
        {
            m_currentEncryptionStatus[vct->ProgramNumber(i)] = kEncUnknown;
        }
    }

    UpdateChannelInfo(true);
}

void ChannelScanSM::HandleMGT(const MasterGuideTable *mgt)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_CHANSCAN, LOG_INFO, LOC + QString("Got the Master Guide for %1")
            .arg((*m_current).m_friendlyName));
    LogLines(mgt->toString());

    UpdateChannelInfo(true);
}

/**
 * \fn ChannelScanSM::HandleSDT
 *
 * \param tsid Unused. Present so that the HandleSDT and HandleSDTo
 *             functions have the same type signature.
 * \param sdt  A pointer to the service description table.
 */
void ChannelScanSM::HandleSDT(uint /*tsid*/, const ServiceDescriptionTable *sdt)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_CHANSCAN, LOG_INFO, LOC +
        QString("Got a Service Description Table for %1 section %2/%3")
            .arg((*m_current).m_friendlyName)
            .arg(sdt->Section()).arg(sdt->LastSection()));
    LogLines(sdt->toString());

    // If this is Astra 28.2 add start listening for Freesat BAT and SDTo
    if (!m_setOtherTables && (
        sdt->OriginalNetworkID() == OriginalNetworkID::SES2 ||
        sdt->OriginalNetworkID() == OriginalNetworkID::BBC))
    {
        DTVSignalMonitor *monitor = GetDTVSignalMonitor();
        if (nullptr != monitor)
        {
            ScanStreamData *stream = monitor->GetScanStreamData();
            if (nullptr != stream)
            {
                stream->SetFreesatAdditionalSI(true);
                m_setOtherTables = true;
                // The whole BAT & SDTo group comes round in 10s
                m_otherTableTimeout = 10s;
                // Delay processing the SDT until we've seen BATs and SDTos
                m_otherTableTime = std::chrono::milliseconds(m_timer.elapsed()) + m_otherTableTimeout;

                LOG(VB_CHANSCAN, LOG_INFO, LOC +
                    QString("SDT has OriginalNetworkID %1, look for "
                            "additional Freesat SI").arg(sdt->OriginalNetworkID()));
            }
        }
    }

    if (!m_timer.hasExpired(m_otherTableTime.count()))
    {
        // Set the version for the SDT so we see it again.
        DTVSignalMonitor *monitor = GetDTVSignalMonitor();
        if (nullptr != monitor)
        {
            monitor->GetDVBStreamData()->
                SetVersionSDT(sdt->TSID(), -1, 0);
        }
    }

    uint id = sdt->OriginalNetworkID() << 16 | sdt->TSID();
    m_tsScanned.insert(id);

    for (uint i = 0; !m_currentTestingDecryption && i < sdt->ServiceCount(); ++i)
    {
        if (sdt->IsEncrypted(i))
        {
            m_currentEncryptionStatus[sdt->ServiceID(i)] = kEncUnknown;
        }
    }

    UpdateChannelInfo(true);
}

void ChannelScanSM::HandleNIT(const NetworkInformationTable *nit)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_CHANSCAN, LOG_INFO, LOC +
        QString("Got a Network Information Table id %1 for %2 section %3/%4")
            .arg(nit->NetworkID()).arg((*m_current).m_friendlyName)
            .arg(nit->Section()).arg(nit->LastSection()));
    LogLines(nit->toString());

    UpdateChannelInfo(true);
}

void ChannelScanSM::HandleBAT(const BouquetAssociationTable *bat)
{
    QMutexLocker locker(&m_lock);

    LOG(VB_CHANSCAN, LOG_INFO, LOC +
        QString("Got a Bouquet Association Table id %1 for %2 section %3/%4")
            .arg(bat->BouquetID()).arg((*m_current).m_friendlyName)
            .arg(bat->Section()).arg(bat->LastSection()));
    LogLines(bat->toString());

    m_otherTableTime = std::chrono::milliseconds(m_timer.elapsed()) + m_otherTableTimeout;

    for (uint i = 0; i < bat->TransportStreamCount(); ++i)
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
            if (!authority.IsValid() || !services.IsValid())
                continue;

            for (uint j = 0; j < services.ServiceCount(); ++j)
            {
                // If the default authority is given in the SDT this
                // overrides any definition in the BAT (or in the NIT)
                LOG(VB_CHANSCAN, LOG_DEBUG, LOC +
                    QString("Found default authority '%1' in BAT for service nid %2 tid %3 sid %4")
                        .arg(authority.DefaultAuthority())
                        .arg(netid).arg(tsid).arg(services.ServiceID(j)));
               uint64_t index = ((uint64_t)netid << 32) | (tsid << 16) |
                                 services.ServiceID(j);
               if (! m_defAuthorities.contains(index))
                   m_defAuthorities[index] = authority.DefaultAuthority();
            }
        }
    }
    UpdateChannelInfo(true);
}

void ChannelScanSM::HandleSDTo(uint tsid, const ServiceDescriptionTable *sdt)
{
    QMutexLocker locker(&m_lock);
#if 0
    LOG(VB_CHANSCAN, LOG_DEBUG, LOC +
        QString("Got a Service Description Table (other) for Transport ID %1 section %2/%3")
            .arg(tsid).arg(sdt->Section()).arg(sdt->LastSection()));
    LogLines(sdt->toString());
#endif
    m_otherTableTime = std::chrono::milliseconds(m_timer.elapsed()) + m_otherTableTimeout;

    uint netid = sdt->OriginalNetworkID();

    for (uint i = 0; i < sdt->ServiceCount(); ++i)
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
            if (!authority.IsValid())
                continue;
#if 0
            LOG(VB_CHANSCAN, LOG_DEBUG, LOC +
                QString("Found default authority '%1' in SDTo for service nid %2 tid %3 sid %4")
                    .arg(authority.DefaultAuthority())
                    .arg(netid).arg(tsid).arg(serviceId));
#endif
            m_defAuthorities[((uint64_t)netid << 32) | (tsid << 16) | serviceId] =
                authority.DefaultAuthority();
        }
    }
}

void ChannelScanSM::HandleEncryptionStatus(uint pnum, bool encrypted)
{
    QMutexLocker locker(&m_lock);

    m_currentEncryptionStatus[pnum] = encrypted ? kEncEncrypted : kEncDecrypted;

    if (kEncDecrypted == m_currentEncryptionStatus[pnum])
        m_currentTestingDecryption = false;

    UpdateChannelInfo(true);
}

bool ChannelScanSM::TestNextProgramEncryption(void)
{
    if (!m_currentInfo || m_currentInfo->m_pmts.empty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Can't monitor decryption -- no pmts");
        m_currentTestingDecryption = false;
        return false;
    }

    while (true)
    {
        uint pnum = 0;
        QMap<uint, uint>::const_iterator it = m_currentEncryptionStatus.cbegin();
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("%1/%2 checked")
            .arg(currentEncryptionStatusChecked.size())
            .arg(currentEncryptionStatus.size()));
#endif
        while (it != m_currentEncryptionStatus.cend())
        {
            if (!m_currentEncryptionStatusChecked[it.key()])
            {
                pnum = it.key();
                break;
            }
            ++it;
        }

        if (!pnum)
            break;

        m_currentEncryptionStatusChecked[pnum] = true;

        if (!m_testDecryption)
        {
            m_currentEncryptionStatus[pnum] = kEncEncrypted;
            continue;
        }

        const ProgramMapTable *pmt = nullptr;
        for (uint i = 0; !pmt && (i < m_currentInfo->m_pmts.size()); ++i)
        {
            pmt = (m_currentInfo->m_pmts[i]->ProgramNumber() == pnum) ?
                m_currentInfo->m_pmts[i] : nullptr;
        }

        if (pmt)
        {
            QString cur_chan;
            QString cur_chan_tr;
            GetCurrentTransportInfo(cur_chan, cur_chan_tr);

            QString msg_tr =
                QObject::tr("Program %1 Testing Decryption")
                .arg(pnum);
            QString msg =
                QString("%1 -- Testing decryption of program %2")
                .arg(cur_chan).arg(pnum);

            m_scanMonitor->ScanAppendTextToLog(msg_tr);
            LOG(VB_CHANSCAN, LOG_INFO, LOC + msg);

#if CONFIG_DVB
            if (GetDVBChannel())
                GetDVBChannel()->SetPMT(pmt);
#endif // CONFIG_DVB

            DTVSignalMonitor *monitor = GetDTVSignalMonitor();
            if (nullptr != monitor)
            {
                monitor->GetStreamData()->ResetDecryptionMonitoringState();
                monitor->GetStreamData()->TestDecryption(pmt);
            }

            m_currentTestingDecryption = true;
            m_timer.start();
            return true;
        }

        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Can't monitor decryption of program %1 -- no pmt")
                .arg(pnum));

    }

    m_currentTestingDecryption = false;
    return false;
}

DTVTunerType ChannelScanSM::GuessDTVTunerType(DTVTunerType type) const
{
    if (m_scanDTVTunerType != DTVTunerType::kTunerTypeUnknown)
        type = m_scanDTVTunerType;

    const DTVChannel *chan = GetDTVChannel();

    if (!chan)
        return type;

    std::vector<DTVTunerType> tts = chan->GetTunerTypes();

    for (auto & tt : tts)
    {
        if (tt == type)
            return type;
    }

    if (!tts.empty())
        return tts[0];

    return type;
}

void ChannelScanSM::UpdateScanTransports(uint nit_frequency, const NetworkInformationTable *nit)
{
    LOG(VB_CHANSCAN, LOG_DEBUG, LOC + QString("%1 NIT nid:%2 fr:%3 section:%4/%5 ts count:%6 ")
        .arg(__func__).arg(nit->NetworkID()).arg(nit_frequency).arg(nit->Section()).arg(nit->LastSection())
        .arg(nit->TransportStreamCount()));

    for (uint i = 0; i < nit->TransportStreamCount(); ++i)
    {
        uint32_t tsid  = nit->TSID(i);
        uint32_t netid = nit->OriginalNetworkID(i);
        uint32_t id    = netid << 16 | tsid;

        if (m_tsScanned.contains(id) || m_extendTransports.contains(id))
            continue;

        const desc_list_t& list =
            MPEGDescriptor::Parse(nit->TransportDescriptors(i),
                                  nit->TransportDescriptorsLength(i));

        for (const auto * const item : list)
        {
            uint64_t frequency = 0;
            const MPEGDescriptor desc(item);
            uint tag = desc.DescriptorTag();
//          QString tagString = desc.DescriptorTagString();

            DTVTunerType tt(DTVTunerType::kTunerTypeUnknown);
            switch (tag)
            {
                case DescriptorID::terrestrial_delivery_system:
                {
                    const TerrestrialDeliverySystemDescriptor cd(desc);
                    if (cd.IsValid())
                        frequency = cd.FrequencyHz();
                    tt = DTVTunerType::kTunerTypeDVBT;
                    break;
                }
                case DescriptorID::extension:
                {
                    switch (desc.DescriptorTagExtension())
                    {
                        case DescriptorID::t2_delivery_system:
                        {
                            tt = DTVTunerType::kTunerTypeDVBT2;
                            continue;                           // T2 descriptor not yet used
                        }
                        default:
                            continue;                           // Next descriptor
                    }
                }
                case DescriptorID::satellite_delivery_system:
                {
                    const SatelliteDeliverySystemDescriptor cd(desc);
                    if (cd.IsValid())
                        frequency = cd.FrequencykHz();
                    tt = DTVTunerType::kTunerTypeDVBS1;
                    break;
                }
                case DescriptorID::s2_satellite_delivery_system:
                {
                    tt = DTVTunerType::kTunerTypeDVBS2;
                    continue;                           // S2 descriptor not yet used
                }
                case DescriptorID::cable_delivery_system:
                {
                    const CableDeliverySystemDescriptor cd(desc);
                    if (cd.IsValid())
                        frequency = cd.FrequencyHz();
                    tt = DTVTunerType::kTunerTypeDVBC;
                    break;
                }
                default:
                    continue;                           // Next descriptor
            }

            // Have now a delivery system descriptor
            tt = GuessDTVTunerType(tt);
            DTVMultiplex tuning;
            if (tuning.FillFromDeliverySystemDesc(tt, desc))
            {
                LOG(VB_CHANSCAN, LOG_DEBUG, QString("NIT onid:%1 add ts(%2):%3  %4")
                    .arg(netid).arg(i).arg(tsid).arg(tuning.toString()));
                m_extendTransports[id] = tuning;
            }
            else
            {
                LOG(VB_CHANSCAN, LOG_DEBUG, QString("NIT onid:%1 cannot add ts(%2):%3 fr:%4")
                    .arg(netid).arg(i).arg(tsid).arg(frequency));
            }

            // Next TS in loop
            break;
        }
    }
}

bool ChannelScanSM::UpdateChannelInfo(bool wait_until_complete)
{
    QMutexLocker locker(&m_mutex);

    if (m_current == m_scanTransports.end())
        return true;

    if (wait_until_complete && !m_waitingForTables)
        return true;

    if (wait_until_complete && m_currentTestingDecryption)
        return false;

    DTVSignalMonitor *dtv_sm = GetDTVSignalMonitor();
    if (!dtv_sm)
        return false;

    const ScanStreamData *sd = dtv_sm->GetScanStreamData();

    if (!m_currentInfo)
        m_currentInfo = new ScannedChannelInfo();

    bool transport_tune_complete = true;

    // MPEG

    // Grab PAT tables
    pat_vec_t pattmp = sd->GetCachedPATs();
    QMap<uint,bool> tsid_checked;
    for (auto & pat : pattmp)
    {
        uint tsid = pat->TransportStreamID();
        if (tsid_checked[tsid])
            continue;
        tsid_checked[tsid] = true;
        if (m_currentInfo->m_pats.contains(tsid))
            continue;

        if (!wait_until_complete || sd->HasCachedAllPAT(tsid))
        {
            m_currentInfo->m_pats[tsid] = sd->GetCachedPATs(tsid);
            if (!m_currentInfo->m_pmts.empty())
            {
                sd->ReturnCachedPMTTables(m_currentInfo->m_pmts);
                m_currentInfo->m_pmts.clear();
            }
        }
        else
        {
            transport_tune_complete = false;
        }
    }
    transport_tune_complete &= !pattmp.empty();
    sd->ReturnCachedPATTables(pattmp);

    // Grab PMT tables
    if ((!wait_until_complete || sd->HasCachedAllPMTs()) &&
        m_currentInfo->m_pmts.empty())
        m_currentInfo->m_pmts = sd->GetCachedPMTs();

    // ATSC
    if (!m_currentInfo->m_mgt && sd->HasCachedMGT())
        m_currentInfo->m_mgt = sd->GetCachedMGT();

    if ((!wait_until_complete || sd->HasCachedAllCVCTs()) &&
        m_currentInfo->m_cvcts.empty())
    {
        m_currentInfo->m_cvcts = sd->GetCachedCVCTs();
    }

    if ((!wait_until_complete || sd->HasCachedAllTVCTs()) &&
        m_currentInfo->m_tvcts.empty())
    {
        m_currentInfo->m_tvcts = sd->GetCachedTVCTs();
    }

    // DVB
    if ((!wait_until_complete || sd->HasCachedAllNIT()) &&
        (m_currentInfo->m_nits.empty() ||
         m_timer.hasExpired(m_otherTableTime.count())))
    {
        m_currentInfo->m_nits = sd->GetCachedNIT();
    }

    sdt_vec_t sdttmp = sd->GetCachedSDTs();
    tsid_checked.clear();
    for (auto & sdt : sdttmp)
    {
        uint tsid = sdt->TSID();
        if (tsid_checked[tsid])
            continue;
        tsid_checked[tsid] = true;
        if (m_currentInfo->m_sdts.contains(tsid))
            continue;

        if (!wait_until_complete || sd->HasCachedAllSDT(tsid))
            m_currentInfo->m_sdts[tsid] = sd->GetCachedSDTSections(tsid);
    }
    sd->ReturnCachedSDTTables(sdttmp);

    if ((!wait_until_complete || sd->HasCachedAllBATs()) &&
        (m_currentInfo->m_bats.empty() ||
         m_timer.hasExpired(m_otherTableTime.count())))
    {
        m_currentInfo->m_bats = sd->GetCachedBATs();
    }

    // Check if transport tuning is complete
    if (transport_tune_complete)
    {
        transport_tune_complete &= !m_currentInfo->m_pmts.empty();

        if (!(sd->HasCachedMGT() || sd->HasCachedAnyNIT()))
        {
            transport_tune_complete = false;
        }

        if (sd->HasCachedMGT() || sd->HasCachedAnyVCTs())
        {
            transport_tune_complete &= sd->HasCachedMGT();
            transport_tune_complete &=
                (!m_currentInfo->m_tvcts.empty() || !m_currentInfo->m_cvcts.empty());
        }
        else if (sd->HasCachedAnyNIT() || sd->HasCachedAnySDTs())
        {
            transport_tune_complete &= !m_currentInfo->m_nits.empty();
            transport_tune_complete &= !m_currentInfo->m_sdts.empty();
        }
        if (sd->HasCachedAnyBATs())
        {
            transport_tune_complete &= !m_currentInfo->m_bats.empty();
        }
        if (transport_tune_complete)
        {
            uint tsid = dtv_sm->GetTransportID();
            LOG(VB_CHANSCAN, LOG_INFO, LOC +
                QString("\nTable status after transport tune complete:") +
                QString("\nsd->HasCachedAnyNIT():         %1").arg(sd->HasCachedAnyNIT()) +
                QString("\nsd->HasCachedAnySDTs():        %1").arg(sd->HasCachedAnySDTs()) +
                QString("\nsd->HasCachedAnyBATs():        %1").arg(sd->HasCachedAnyBATs()) +
                QString("\nsd->HasCachedAllPMTs():        %1").arg(sd->HasCachedAllPMTs()) +
                QString("\nsd->HasCachedAllNIT():         %1").arg(sd->HasCachedAllNIT()) +
                QString("\nsd->HasCachedAllSDT(%1):    %2").arg(tsid,5).arg(sd->HasCachedAllSDT(tsid)) +
                QString("\nsd->HasCachedAllBATs():        %1").arg(sd->HasCachedAllBATs()) +
                QString("\nsd->HasCachedMGT():            %1").arg(sd->HasCachedMGT()) +
                QString("\nsd->HasCachedAnyVCTs():        %1").arg(sd->HasCachedAnyVCTs()) +
                QString("\nsd->HasCachedAllCVCTs():       %1").arg(sd->HasCachedAllCVCTs()) +
                QString("\nsd->HasCachedAllTVCTs():       %1").arg(sd->HasCachedAllTVCTs()) +
                QString("\ncurrentInfo->m_pmts.empty():   %1").arg(m_currentInfo->m_pmts.empty()) +
                QString("\ncurrentInfo->m_nits.empty():   %1").arg(m_currentInfo->m_nits.empty()) +
                QString("\ncurrentInfo->m_sdts.empty():   %1").arg(m_currentInfo->m_sdts.empty()) +
                QString("\ncurrentInfo->m_bats.empty():   %1").arg(m_currentInfo->m_bats.empty()) +
                QString("\ncurrentInfo->m_cvtcs.empty():  %1").arg(m_currentInfo->m_cvcts.empty()) +
                QString("\ncurrentInfo->m_tvtcs.empty():  %1").arg(m_currentInfo->m_tvcts.empty()));
        }
    }
    if (!wait_until_complete)
        transport_tune_complete = true;
    if (transport_tune_complete)
    {
        LOG(VB_CHANSCAN, LOG_INFO, LOC +
            QString("transport_tune_complete: wait_until_complete %1").arg(wait_until_complete));
    }

    if (transport_tune_complete &&
        /*!ignoreEncryptedServices &&*/ !m_currentEncryptionStatus.empty())
    {
        //GetDTVSignalMonitor()->GetStreamData()->StopTestingDecryption();

        if (TestNextProgramEncryption())
            return false;

        QMap<uint, uint>::const_iterator it = m_currentEncryptionStatus.cbegin();
        for (; it != m_currentEncryptionStatus.cend(); ++it)
        {
            m_currentInfo->m_programEncryptionStatus[it.key()] = *it;

            if (m_testDecryption)
            {
                QString msg_tr1 = QObject::tr("Program %1").arg(it.key());
                QString msg_tr2 = QObject::tr("Unknown decryption status");
                if (kEncEncrypted == *it)
                    msg_tr2 = QObject::tr("Encrypted");
                else if (kEncDecrypted == *it)
                    msg_tr2 = QObject::tr("Decrypted");
                QString msg_tr =QString("%1 %2").arg(msg_tr1, msg_tr2);
                m_scanMonitor->ScanAppendTextToLog(msg_tr);
            }

            QString msg = QString("Program %1").arg(it.key());
            if (kEncEncrypted == *it)
                msg = msg + " -- Encrypted";
            else if (kEncDecrypted == *it)
                msg = msg + " -- Decrypted";
            else if (kEncUnknown == *it)
                msg = msg + " -- Unknown decryption status";

            LOG(VB_CHANSCAN, LOG_INFO, LOC + msg);
        }
    }

    // Append transports from the NIT to the scan list
    if (transport_tune_complete && m_extendScanList &&
        !m_currentInfo->m_nits.empty())
    {
        // Update transport with delivery system descriptors from the NIT
        auto it = m_currentInfo->m_nits.begin();
        while (it != m_currentInfo->m_nits.end())
        {
            UpdateScanTransports((*m_current).m_tuning.m_frequency, *it);
            ++it;
        }
    }

    // Start scanning next transport if we are done with this one..
    if (transport_tune_complete)
    {
        QString cchan;
        QString cchan_tr;
        uint cchan_cnt = GetCurrentTransportInfo(cchan, cchan_tr);
        m_channelsFound += cchan_cnt;
        QString chan_tr = QObject::tr("%1 -- Timed out").arg(cchan_tr);
        QString chan    = QString(    "%1 -- Timed out").arg(cchan);
        QString msg_tr  = "";
        QString msg     = "";

        if (!m_currentInfo->IsEmpty())
        {
            TransportScanItem &item = *m_current;
            item.m_tuning.m_frequency = item.freq_offset(m_current.offset());
            item.m_signalStrength     = m_signalMonitor->GetSignalStrength();
            item.m_networkID          = dtv_sm->GetNetworkID();
            item.m_transportID        = dtv_sm->GetTransportID();

            if (m_scanDTVTunerType == DTVTunerType::kTunerTypeDVBC)
            {
                if (item.m_tuning.m_modSys == DTVModulationSystem::kModulationSystem_UNDEFINED)
                {
                    item.m_tuning.m_modSys = DTVModulationSystem::kModulationSystem_DVBC_ANNEX_A;
                }
            }
            if (m_scanDTVTunerType == DTVTunerType::kTunerTypeDVBT)
            {
                item.m_tuning.m_modSys = DTVModulationSystem::kModulationSystem_DVBT;
            }
            if (m_scanDTVTunerType == DTVTunerType::kTunerTypeDVBT2)
            {
                if (m_dvbt2Tried)
                    item.m_tuning.m_modSys = DTVModulationSystem::kModulationSystem_DVBT2;
                else
                    item.m_tuning.m_modSys = DTVModulationSystem::kModulationSystem_DVBT;
            }

            LOG(VB_CHANSCAN, LOG_INFO, LOC +
                QString("Adding %1 offset:%2 ss:%3")
                    .arg(item.m_tuning.toString()).arg(m_current.offset())
                    .arg(item.m_signalStrength));

            LOG(VB_CHANSCAN, LOG_DEBUG, LOC +
                QString("%1(%2) m_inputName: %3 ").arg(__FUNCTION__).arg(__LINE__).arg(m_inputName) +
                QString("tunerType:%1 %2 ").arg(m_scanDTVTunerType).arg(m_scanDTVTunerType.toString()) +
                QString("m_modSys:%1 %2 ").arg(item.m_tuning.m_modSys).arg(item.m_tuning.m_modSys.toString()) +
                QString("m_dvbt2Tried:%1").arg(m_dvbt2Tried));

            m_channelList << ChannelListItem(m_current, m_currentInfo);
            m_currentInfo = nullptr;
        }
        else
        {
            delete m_currentInfo;
            m_currentInfo = nullptr;
        }

        SignalMonitor *sm = GetSignalMonitor();
        if (HasTimedOut())
        {
            msg_tr = (cchan_cnt) ?
                QObject::tr("%1 possible channels").arg(cchan_cnt) :
                QObject::tr("no channels");
            msg_tr = QString("%1, %2").arg(chan_tr, msg_tr);
            msg = (cchan_cnt) ?
                QString("%1 possible channels").arg(cchan_cnt) :
                QString("no channels");
            msg = QString("%1, %2").arg(chan_tr, msg);
        }
        else if ((m_current != m_scanTransports.end()) &&
                 m_timer.hasExpired((*m_current).m_timeoutTune.count()) &&
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

        m_scanMonitor->ScanAppendTextToLog(msg_tr);
        LOG(VB_CHANSCAN, LOG_INFO, LOC + msg);

        m_currentEncryptionStatus.clear();
        m_currentEncryptionStatusChecked.clear();

        m_setOtherTables = false;
        m_otherTableTime = 0ms;

        if (m_scanning)
        {
            ++m_transportsScanned;
            UpdateScanPercentCompleted();
            m_waitingForTables = false;
            m_nextIt = m_current.nextTransport();
            m_dvbt2Tried = true;
        }
        else
        {
            m_scanMonitor->ScanPercentComplete(100);
            m_scanMonitor->ScanComplete();
        }

        return true;
    }

    return false;
}

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PCM_INFO_INIT(SISTD) \
    ChannelInsertInfo &info = pnum_to_dbchan[pnum]; \
    info.m_dbMplexId    = mplexid; info.m_sourceId   = m_sourceID;  \
    info.m_serviceId    = pnum;    info.m_freqId     = freqidStr; \
    info.m_siStandard   = SISTD;

static void update_info(ChannelInsertInfo &info,
                        const VirtualChannelTable *vct, uint i)
{
    if (vct->ModulationMode(i) == 0x01 /* NTSC Modulation */ ||
        vct->ServiceType(i)    == 0x01 /* Analog TV */)
    {
        info.m_siStandard = "ntsc";
        info.m_format     = "ntsc";
    }

    info.m_callSign = vct->ShortChannelName(i);

    info.m_serviceName = vct->GetExtendedChannelName(i);
    if (info.m_serviceName.isEmpty())
        info.m_serviceName = vct->ShortChannelName(i);

    info.m_chanNum.clear();

    info.m_serviceId        = vct->ProgramNumber(i);
    info.m_atscMajorChannel = vct->MajorChannel(i);
    info.m_atscMinorChannel = vct->MinorChannel(i);

    info.m_useOnAirGuide    = !vct->IsHidden(i) || !vct->IsHiddenInGuide(i);

    info.m_hidden           = vct->IsHidden(i);
    info.m_hiddenInGuide    = vct->IsHiddenInGuide(i);

    info.m_vctTsId          = vct->TransportStreamID();
    info.m_vctChanTsId      = vct->ChannelTransportStreamID(i);
    info.m_isEncrypted     |= vct->IsAccessControlled(i);
    info.m_isDataService    = vct->ServiceType(i) == 0x04;
    info.m_isAudioService   = vct->ServiceType(i) == 0x03;

    info.m_inVct        = true;
}

static void update_info(ChannelInsertInfo &info,
                        const ServiceDescriptionTable *sdt, uint i,
                        const QMap<uint64_t, QString> &defAuthorities)
{
    // HACK begin -- special exception for these networks
    // this enables useonairguide by default for all matching channels
    //             (dbver == "1067")
    bool force_guide_present = (
        // Telenor (NO)
        (sdt->OriginalNetworkID() == OriginalNetworkID::TELENOR) ||
#if 0 // #9592#comment:23 - meanwhile my provider changed his signaling
        // Kabelplus (AT) formerly Kabelsignal, registered to NDS, see #9592
        (sdt->OriginalNetworkID() ==   222) ||
#endif
        // ERT (GR) from the private temporary allocation, see #9592:comment:17
        (sdt->OriginalNetworkID() == 65330) ||
        // Digitenne (NL) see #13427
        (sdt->OriginalNetworkID() == OriginalNetworkID::NOZEMA)
    );
    // HACK end -- special exception for these networks

    // Figure out best service name and callsign...
    ServiceDescriptor *desc = sdt->GetServiceDescriptor(i);
    QString callsign;
    QString service_name;
    if (desc)
    {
        callsign = desc->ServiceShortName();
        if (callsign.trimmed().isEmpty())
        {
            callsign = QString("%1-%2-%3")
                .arg(ChannelUtil::GetUnknownCallsign()).arg(sdt->TSID())
                .arg(sdt->ServiceID(i));
        }

        service_name = desc->ServiceName();
        if (service_name.trimmed().isEmpty())
            service_name.clear();

        info.m_serviceType = desc->ServiceType();
        info.m_isDataService =
            (desc && !desc->IsDTV() && !desc->IsDigitalAudio());
        info.m_isAudioService = (desc && desc->IsDigitalAudio());
        delete desc;
    }
    else
    {
        LOG(VB_CHANSCAN, LOG_INFO, "ChannelScanSM: " +
            QString("No ServiceDescriptor for onid %1 tid %2 sid %3")
                .arg(sdt->OriginalNetworkID()).arg(sdt->TSID()).arg(sdt->ServiceID(i)));
    }

    if (info.m_callSign.isEmpty())
        info.m_callSign = callsign;
    if (info.m_serviceName.isEmpty())
        info.m_serviceName = service_name;

    info.m_useOnAirGuide =
        sdt->HasEITPresentFollowing(i) ||
        sdt->HasEITSchedule(i) ||
        force_guide_present;

    info.m_hidden           = false;
    info.m_hiddenInGuide    = false;
    info.m_serviceId        = sdt->ServiceID(i);
    info.m_sdtTsId          = sdt->TSID();
    info.m_origNetId        = sdt->OriginalNetworkID();
    info.m_inSdt            = true;

    desc_list_t parsed =
        MPEGDescriptor::Parse(sdt->ServiceDescriptors(i),
                              sdt->ServiceDescriptorsLength(i));
    // Look for default authority
    const unsigned char *def_auth =
        MPEGDescriptor::Find(parsed, DescriptorID::default_authority);
    if (def_auth)
    {
        DefaultAuthorityDescriptor authority(def_auth);
        if (authority.IsValid())
        {
#if 0
            LOG(VB_CHANSCAN, LOG_DEBUG,
                QString("ChannelScanSM: Found default authority '%1' in SDT for service onid %2 tid %3 sid %4")
                    .arg(authority.DefaultAuthority())
                    .arg(info.m_origNetId).arg(info.m_sdtTsId).arg(info.m_serviceId));
#endif
            info.m_defaultAuthority = authority.DefaultAuthority();
            return;
        }
    }

    // If no authority in the SDT then use the one found in the BAT or the SDTo
    uint64_t index = (uint64_t)info.m_origNetId << 32 |
        info.m_sdtTsId << 16 | info.m_serviceId;
    if (defAuthorities.contains(index))
        info.m_defaultAuthority = defAuthorities[index];

    // Is this service relocated from somewhere else?
    ServiceRelocatedDescriptor *srdesc = sdt->GetServiceRelocatedDescriptor(i);
    if (srdesc)
    {
        info.m_oldOrigNetId = srdesc->OldOriginalNetworkID();
        info.m_oldTsId      = srdesc->OldTransportID();
        info.m_oldServiceId = srdesc->OldServiceID();

        LOG(VB_CHANSCAN, LOG_DEBUG, "ChannelScanSM: " +
            QString("Service '%1' onid:%2 tid:%3 sid:%4 ")
                .arg(info.m_serviceName)
                .arg(info.m_origNetId)
                .arg(info.m_sdtTsId)
                .arg(info.m_serviceId) +
            QString(" relocated from onid:%1 tid:%2 sid:%3")
                .arg(info.m_oldOrigNetId)
                .arg(info.m_oldTsId)
                .arg(info.m_oldServiceId));

        delete srdesc;
    }

}

uint ChannelScanSM::GetCurrentTransportInfo(
    QString &cur_chan, QString &cur_chan_tr) const
{
    if (m_current.iter() == m_scanTransports.end())
    {
        cur_chan.clear();
        cur_chan_tr.clear();
        return 0;
    }

    uint max_chan_cnt = 0;

    QMap<uint,ChannelInsertInfo> list = GetChannelList(m_current, m_currentInfo);
    {
        for (const auto & info : std::as_const(list))
        {
            max_chan_cnt +=
                (info.m_inPat || info.m_inPmt ||
                 info.m_inSdt || info.m_inVct) ? 1 : 0;
        }
    }

    QString offset_str_tr = m_current.offset() ?
        QObject::tr(" offset %2").arg(m_current.offset()) : "";
    cur_chan_tr = QString("%1%2")
        .arg((*m_current).m_friendlyName, offset_str_tr);

    QString offset_str = m_current.offset() ?
        QString(" offset %2").arg(m_current.offset()) : "";
    cur_chan = QString("%1%2")
        .arg((*m_current).m_friendlyName, offset_str);

    return max_chan_cnt;
}

QMap<uint,ChannelInsertInfo>
ChannelScanSM::GetChannelList(transport_scan_items_it_t trans_info,
                              ScannedChannelInfo *scan_info) const
{
    QMap<uint,ChannelInsertInfo> pnum_to_dbchan;

    uint    mplexid   = (*trans_info).m_mplexid;
    int     freqid    = (*trans_info).m_friendlyNum;
    QString freqidStr = (freqid) ? QString::number(freqid) : QString("");
    QString iptv_channel = (*trans_info).m_iptvChannel;

    // channels.conf
    const DTVChannelInfoList &echan = (*trans_info).m_expectedChannels;
    for (const auto & chan : echan)
    {
        uint pnum = chan.m_serviceid;
        PCM_INFO_INIT("mpeg");
        info.m_serviceName = chan.m_name;
        info.m_inChannelsConf = true;
    }

    // PATs
    for (const auto& pat_list : std::as_const(scan_info->m_pats))
    {
        for (const auto *pat : pat_list)
        {
            bool could_be_opencable = false;
            for (uint i = 0; i < pat->ProgramCount(); ++i)
            {
                if ((pat->ProgramNumber(i) == 0) &&
                    (pat->ProgramPID(i) == PID::SCTE_PSIP_PID))
                {
                    could_be_opencable = true;
                }
            }

            for (uint i = 0; i < pat->ProgramCount(); ++i)
            {
                uint pnum = pat->ProgramNumber(i);
                if (pnum)
                {
                    PCM_INFO_INIT("mpeg");
                    info.m_patTsId = pat->TransportStreamID();
                    info.m_couldBeOpencable = could_be_opencable;
                    info.m_inPat = true;
                }
            }
        }
    }

    // PMTs
    for (const auto *pmt : scan_info->m_pmts)
    {
        uint pnum = pmt->ProgramNumber();
        PCM_INFO_INIT("mpeg");
        for (uint i = 0; i < pmt->StreamCount(); ++i)
        {
            info.m_couldBeOpencable |=
                (StreamID::OpenCableVideo == pmt->StreamType(i));
        }

        desc_list_t descs = MPEGDescriptor::ParseOnlyInclude(
            pmt->ProgramInfo(), pmt->ProgramInfoLength(),
            DescriptorID::registration);

        for (auto & desc : descs)
        {
            RegistrationDescriptor reg(desc);
            if (!reg.IsValid())
                continue;
            if (reg.FormatIdentifierString() == "CUEI" ||
                reg.FormatIdentifierString() == "SCTE")
                info.m_couldBeOpencable = true;
        }

        info.m_isEncrypted |= pmt->IsEncrypted(GetDTVChannel()->GetSIStandard());
        info.m_inPmt = true;
    }

    // Cable VCTs
    for (const auto *cvct : scan_info->m_cvcts)
    {
        for (uint i = 0; i < cvct->ChannelCount(); ++i)
        {
            uint pnum = cvct->ProgramNumber(i);
            PCM_INFO_INIT("atsc");
            update_info(info, cvct, i);

            // One-part channel number, as defined in the ATSC Standard:
            // Program and System Information Protocol for Terrestrial Broadcast and Cable
            // Doc. A65/2013  7 August 2013  page 35
            if ((info.m_atscMajorChannel & 0x3F0) == 0x3F0)
            {
                info.m_chanNum = QString::number(((info.m_atscMajorChannel & 0x00F) << 10) + info.m_atscMinorChannel);
            }
            else
            {
                info.m_chanNum = kATSCChannelFormat.arg(info.m_atscMajorChannel).arg(info.m_atscMinorChannel);
            }
        }
    }

    // Terrestrial VCTs
    for (const auto *tvct : scan_info->m_tvcts)
    {
        for (uint i = 0; i < tvct->ChannelCount(); ++i)
        {
            uint pnum = tvct->ProgramNumber(i);
            PCM_INFO_INIT("atsc");
            update_info(info, tvct, i);
            info.m_chanNum = kATSCChannelFormat.arg(info.m_atscMajorChannel).arg(info.m_atscMinorChannel);
        }
    }

    // SDTs
    QString siStandard = (scan_info->m_mgt == nullptr) ? "dvb" : "atsc";
    for (const auto& sdt_list : std::as_const(scan_info->m_sdts))
    {
        for (const auto *sdt_it : sdt_list)
        {
            for (uint i = 0; i < sdt_it->ServiceCount(); ++i)
            {
                uint pnum = sdt_it->ServiceID(i);
                PCM_INFO_INIT(siStandard);
                update_info(info, sdt_it, i, m_defAuthorities);
            }
        }
    }

    // NIT
    QMap<qlonglong, uint> ukChanNums;
    QMap<qlonglong, uint> scnChanNums;
    QMap<uint,ChannelInsertInfo>::iterator dbchan_it;
    for (dbchan_it = pnum_to_dbchan.begin();
         dbchan_it != pnum_to_dbchan.end(); ++dbchan_it)
    {
        ChannelInsertInfo &info = *dbchan_it;

        // NIT
        for (const auto *item : scan_info->m_nits)
        {
            for (uint i = 0; i < item->TransportStreamCount(); ++i)
            {
                const NetworkInformationTable *nit = item;
                if ((nit->TSID(i)              == info.m_sdtTsId) &&
                    (nit->OriginalNetworkID(i) == info.m_origNetId))
                {
                    info.m_netId = nit->NetworkID();
                    info.m_inNit = true;
                }
                else
                {
                    continue;
                }

                // Descriptors in the transport stream loop of this transport in the NIT
                const desc_list_t &list =
                    MPEGDescriptor::Parse(nit->TransportDescriptors(i),
                                          nit->TransportDescriptorsLength(i));

                // Presence of T2 delivery system descriptor indicates DVB-T2 delivery system
                // DVB BlueBook A038 (Feb 2019) page 104, paragraph 6.4.6.3
                {
                    const unsigned char *desc =
                        MPEGDescriptor::FindExtension(
                            list, DescriptorID::t2_delivery_system);

                    if (desc)
                    {
                        T2DeliverySystemDescriptor t2tdsd(desc);
                        if (t2tdsd.IsValid())
                        {
                            (*trans_info).m_tuning.m_modSys = DTVModulationSystem::kModulationSystem_DVBT2;
                        }
                    }
                }

                // Logical channel numbers
                {
                    const unsigned char *desc =
                        MPEGDescriptor::Find(
                            list, PrivateDescriptorID::dvb_logical_channel_descriptor);

                    if (desc)
                    {
                        DVBLogicalChannelDescriptor uklist(desc);
                        if (!uklist.IsValid())
                            continue;
                        for (uint j = 0; j < uklist.ChannelCount(); ++j)
                        {
                            ukChanNums[((qlonglong)info.m_origNetId<<32) |
                                    uklist.ServiceID(j)] =
                                uklist.ChannelNumber(j);
                        }
                    }
                }

                // HD Simulcast logical channel numbers
                {
                    const unsigned char *desc =
                        MPEGDescriptor::Find(
                            list, PrivateDescriptorID::dvb_simulcast_channel_descriptor);

                    if (desc)
                    {
                        DVBSimulcastChannelDescriptor scnlist(desc);
                        if (!scnlist.IsValid())
                            continue;
                        for (uint j = 0; j < scnlist.ChannelCount(); ++j)
                        {
                            scnChanNums[((qlonglong)info.m_origNetId<<32) |
                                        scnlist.ServiceID(j)] =
                                 scnlist.ChannelNumber(j);
                        }
                    }
                }
            }
        }
    }

    // BAT

    // Channel numbers for Freesat and Sky on Astra 28.2E
    //
    // Get the Logical Channel Number (LCN) information from the BAT.
    // The first filter is on the bouquet ID.
    // Then collect all LCN for the selected region and
    // for the common (undefined) region with id 0xFFFF.
    // The LCN of the selected region has priority; if
    // a service is not defined there then the value of the LCN
    // table of the common region is used.
    // This works because the BAT of each transport contains
    // the LCN of all transports and services for all bouquets.
    //
    // For reference, this website has the Freesat and Sky channel numbers
    // for each bouquet and region:
    // https://www.satellite-calculations.com/DVB/28.2E/28E_FreeSat_ChannelNumber.php
    // https://www.satellite-calculations.com/DVB/28.2E/28E_Sky_ChannelNumber.php
    //

    // Lookup table from LCN to service ID
    QMap<uint,qlonglong> lcn_sid;

    for (const auto *bat : scan_info->m_bats)
    {
        // Only the bouquet selected by user
        if (bat->BouquetID() != m_bouquetId)
            continue;

        for (uint t = 0; t < bat->TransportStreamCount(); ++t)
        {
            uint netid = bat->OriginalNetworkID(t);

            // No network check to allow scanning on all Sky satellites
#if 0
            if (!(netid == OriginalNetworkID::SES2  ||
                  netid == OriginalNetworkID::BBC   ||
                  netid == OriginalNetworkID::SKYNZ ))
                continue;
#endif
            desc_list_t parsed =
                MPEGDescriptor::Parse(bat->TransportDescriptors(t),
                                      bat->TransportDescriptorsLength(t));

            uint priv_dsid = 0;
            for (const auto *item : parsed)
            {
                if (item[0] == DescriptorID::private_data_specifier)
                {
                    PrivateDataSpecifierDescriptor pd(item);
                    if (pd.IsValid())
                        priv_dsid = pd.PrivateDataSpecifier();
                }

                // Freesat logical channels
                if (priv_dsid == PrivateDataSpecifierID::FSAT &&
                    item[0] == PrivateDescriptorID::freesat_lcn_table)
                {
                    FreesatLCNDescriptor ld(item);
                    if (ld.IsValid())
                    {
                        for (uint i = 0; i<ld.ServiceCount(); i++)
                        {
                            uint service_id = ld.ServiceID(i);
                            for (uint j=0; j<ld.LCNCount(i); j++)
                            {
                                uint region_id = ld.RegionID(i,j);
                                uint lcn = ld.LogicalChannelNumber(i,j);
                                if (region_id == m_regionId)
                                {
                                    lcn_sid[lcn] = ((qlonglong)netid<<32) | service_id;
                                }
                                else if (region_id == kRegionUndefined)
                                {
                                    if (lcn_sid.value(lcn,0) == 0)
                                        lcn_sid[lcn] = ((qlonglong)netid<<32) | service_id;
                                }
                            }
                        }
                    }
                }

                // Sky logical channels
                if (priv_dsid == PrivateDataSpecifierID::BSB1 &&
                    item[0] == PrivateDescriptorID::sky_lcn_table)
                {
                    SkyLCNDescriptor ld(item);
                    if (ld.IsValid())
                    {
                        uint region_id = ld.RegionID();
                        for (uint i = 0; i<ld.ServiceCount(); i++)
                        {
                            uint service_id = ld.ServiceID(i);
                            uint lcn = ld.LogicalChannelNumber(i);
                            if (region_id == m_regionId)
                            {
                                lcn_sid[lcn] = ((qlonglong)netid<<32) | service_id;
                            }
                            else if (region_id == kRegionUndefined)
                            {
                                if (lcn_sid.value(lcn,0) == 0)
                                    lcn_sid[lcn] = ((qlonglong)netid<<32) | service_id;
                            }
#if 0
                            LOG(VB_CHANSCAN, LOG_INFO, LOC +
                                QString("LCN bid:%1 tid:%2 rid:%3 sid:%4 lcn:%5")
                                    .arg(bat->BouquetID()).arg(bat->TSID(t)).arg(region_id).arg(service_id).arg(lcn));
#endif
                        }
                    }
                }
            }
        }
    }

    // Create the reverse table from service id to LCN.
    // If the service has more than one logical
    // channel number the lowest number is used.
    QMap<qlonglong, uint> sid_lcn;
    QMap<uint, qlonglong>::const_iterator r = lcn_sid.constEnd();
    while (r != lcn_sid.constBegin())
    {
        --r;
        qlonglong sid = r.value();
        uint lcn = r.key();
        sid_lcn[sid] = lcn;
    }

    // ------------------------------------------------------------------------

    // Get DVB Logical Channel Numbers
    for (dbchan_it = pnum_to_dbchan.begin();
         dbchan_it != pnum_to_dbchan.end(); ++dbchan_it)
    {
        ChannelInsertInfo &info = *dbchan_it;
        qlonglong key = ((qlonglong)info.m_origNetId<<32) | info.m_serviceId;
        QMap<qlonglong, uint>::const_iterator it;

        // DVB HD Simulcast channel numbers
        //
        // The HD simulcast channel number table gives the correct channel number
        // when HD and SD versions of the same channel are simultaneously broadcast
        // and the receiver is capable of receiving the HD signal.
        // The latter is assumed correct for a MythTV system.
        //
        it = scnChanNums.constFind(key);
        if (it != scnChanNums.constEnd())
        {
            info.m_simulcastChannel = *it;
        }

        // DVB Logical Channel Numbers (a.k.a. UK channel numbers)
        it = ukChanNums.constFind(key);
        if (it != ukChanNums.constEnd())
        {
            info.m_logicalChannel = *it;
        }

        // Freesat and Sky channel numbers
        it = sid_lcn.constFind(key);
        if (it != sid_lcn.constEnd())
        {
            info.m_logicalChannel = *it;
        }

        LOG(VB_CHANSCAN, LOG_INFO, LOC +
            QString("service %1 (0x%2) lcn:%3 scn:%4 callsign '%5'")
                .arg(info.m_serviceId).arg(info.m_serviceId,4,16,QChar('0'))
                .arg(info.m_logicalChannel).arg(info.m_simulcastChannel).arg(info.m_callSign));
    }

    // Get QAM/SCTE/MPEG channel numbers
    for (dbchan_it = pnum_to_dbchan.begin();
         dbchan_it != pnum_to_dbchan.end(); ++dbchan_it)
    {
        ChannelInsertInfo &info = *dbchan_it;

        if (!info.m_chanNum.isEmpty())
            continue;

        if ((info.m_siStandard == "mpeg") ||
            (info.m_siStandard == "scte") ||
            (info.m_siStandard == "opencable"))
        {
            if (info.m_freqId.isEmpty())
            {
                info.m_chanNum = QString("%1-%2")
                    .arg(info.m_sourceId)
                    .arg(info.m_serviceId);
            }
            else
            {
                info.m_chanNum = QString("%1-%2")
                    .arg(info.m_freqId)
                    .arg(info.m_serviceId);
            }
        }
    }

    // Get IPTV channel numbers
    for (dbchan_it = pnum_to_dbchan.begin();
         dbchan_it != pnum_to_dbchan.end(); ++dbchan_it)
    {
        ChannelInsertInfo &info = *dbchan_it;

        if (!info.m_chanNum.isEmpty())
            continue;

        if (!iptv_channel.isEmpty())
        {
            info.m_chanNum = iptv_channel;
            if (info.m_serviceId)
                info.m_chanNum += "-" + QString::number(info.m_serviceId);
        }
    }

    // Check for decryption success
    for (dbchan_it = pnum_to_dbchan.begin();
         dbchan_it != pnum_to_dbchan.end(); ++dbchan_it)
    {
        uint pnum = dbchan_it.key();
        ChannelInsertInfo &info = *dbchan_it;
        info.m_decryptionStatus = scan_info->m_programEncryptionStatus[pnum];
    }

    return pnum_to_dbchan;
}

ScanDTVTransportList ChannelScanSM::GetChannelList(bool addFullTS) const
{
    ScanDTVTransportList list;

    uint cardid = m_channel->GetInputID();

    DTVTunerType tuner_type;
    tuner_type = GuessDTVTunerType(tuner_type);

    for (const auto & it : std::as_const(m_channelList))
    {
        QMap<uint,ChannelInsertInfo> pnum_to_dbchan =
            GetChannelList(it.first, it.second);

        ScanDTVTransport item((*it.first).m_tuning, tuner_type, cardid);
        item.m_iptvTuning = (*(it.first)).m_iptvTuning;
        item.m_signalStrength = (*(it.first)).m_signalStrength;
        item.m_networkID = (*(it.first)).m_networkID;
        item.m_transportID = (*(it.first)).m_transportID;

        QMap<uint,ChannelInsertInfo>::iterator dbchan_it;
        for (dbchan_it = pnum_to_dbchan.begin();
             dbchan_it != pnum_to_dbchan.end(); ++dbchan_it)
        {
            item.m_channels.push_back(*dbchan_it);
        }

        if (!item.m_channels.empty())
        {
            // Add a MPTS channel which can be used to record the entire transport
            // stream multiplex. These recordings can be used in stream analyzer software.
            if (addFullTS)
            {
                // Find the first channel that is present in PAT/PMT and use that as
                // template for the new MPTS channel.
                ChannelInsertInfo info {};
                for (auto & channel : pnum_to_dbchan)
                {
                    if (channel.m_inPat && channel.m_inPmt)
                    {
                        info = channel;
                        break;
                    }
                }

                // If we cannot find a valid channel then use the first channel in
                // the list as template for the new MPTS channel.
                if (info.m_serviceId == 0)
                {
                    dbchan_it = pnum_to_dbchan.begin();
                    info = *dbchan_it;
                }

                // Use transport stream ID as (fake) service ID to use in callsign
                // and as channel number.
                info.m_serviceId = info.m_sdtTsId ? info.m_sdtTsId : info.m_patTsId;
                info.m_chanNum = QString("%1").arg(info.m_serviceId);
                info.m_logicalChannel = info.m_serviceId;

                if (tuner_type == DTVTunerType::kTunerTypeASI)
                {
                    info.m_callSign = QString("MPTS_%1")
                                    .arg(CardUtil::GetDisplayName(cardid));
                }
                else if (info.m_siStandard == "mpeg" ||
                         info.m_siStandard == "scte" ||
                         info.m_siStandard == "opencable")
                {
                    info.m_callSign = QString("MPTS_%1").arg(info.m_freqId);
                }
                else if (info.m_atscMajorChannel > 0)
                {
                    if (info.m_atscMajorChannel < 0x3F0)
                    {
                        info.m_callSign = QString("MPTS_%1").arg(info.m_atscMajorChannel);
                    }
                    else
                    {
                        info.m_callSign = QString("MPTS_%1").arg(info.m_freqId);
                    }
                }
                else if (info.m_serviceId > 0)
                {
                    info.m_callSign = QString("MPTS_%1").arg(info.m_serviceId);
                }
                else if (!info.m_chanNum.isEmpty())
                {
                    info.m_callSign = QString("MPTS_%1").arg(info.m_chanNum);
                }
                else
                {
                    info.m_callSign = "MPTS_UNKNOWN";
                }

                info.m_serviceName = info.m_callSign;
                info.m_atscMinorChannel = 0;
                info.m_format = "MPTS";
                info.m_useOnAirGuide = false;
                info.m_isEncrypted = false;
                item.m_channels.push_back(info);
            }

            list.push_back(item);
        }
    }

    return list;
}

DTVSignalMonitor* ChannelScanSM::GetDTVSignalMonitor(void)
{
    return dynamic_cast<DTVSignalMonitor*>(m_signalMonitor);
}

DVBSignalMonitor* ChannelScanSM::GetDVBSignalMonitor(void)
{
#if CONFIG_DVB
    return dynamic_cast<DVBSignalMonitor*>(m_signalMonitor);
#else
    return nullptr;
#endif
}

DTVChannel *ChannelScanSM::GetDTVChannel(void)
{
    return dynamic_cast<DTVChannel*>(m_channel);
}

const DTVChannel *ChannelScanSM::GetDTVChannel(void) const
{
    return dynamic_cast<const DTVChannel*>(m_channel);
}

HDHRChannel *ChannelScanSM::GetHDHRChannel(void)
{
#if CONFIG_HDHOMERUN
    return dynamic_cast<HDHRChannel*>(m_channel);
#else
    return nullptr;
#endif
}

DVBChannel *ChannelScanSM::GetDVBChannel(void)
{
#if CONFIG_DVB
    return dynamic_cast<DVBChannel*>(m_channel);
#else
    return nullptr;
#endif
}

const DVBChannel *ChannelScanSM::GetDVBChannel(void) const
{
#if CONFIG_DVB
    return dynamic_cast<const DVBChannel*>(m_channel);
#else
    return nullptr;
#endif
}

V4LChannel *ChannelScanSM::GetV4LChannel(void)
{
#if CONFIG_V4L2
    return dynamic_cast<V4LChannel*>(m_channel);
#else
    return nullptr;
#endif
}

/** \fn ChannelScanSM::StartScanner(void)
 *  \brief Starts the ChannelScanSM event loop.
 */
void ChannelScanSM::StartScanner(void)
{
    while (m_scannerThread)
    {
        m_threadExit = true;
        if (m_scannerThread->wait(1s))
        {
            delete m_scannerThread;
            m_scannerThread = nullptr;
        }
    }
    m_threadExit = false;
    m_scannerThread = new MThread("Scanner", this);
    m_scannerThread->start();
}

/** \fn ChannelScanSM::run(void)
 *  \brief This runs the event loop for ChannelScanSM until 'm_threadExit' is true.
 */
void ChannelScanSM::run(void)
{
    LOG(VB_CHANSCAN, LOG_INFO, LOC + "run -- begin");

    while (!m_threadExit)
    {
        if (m_scanning)
            HandleActiveScan();

        usleep(10 * 1000);
    }

    LOG(VB_CHANSCAN, LOG_INFO, LOC + "run -- end");
}

// See if we have timed out
bool ChannelScanSM::HasTimedOut(void)
{
    if (m_currentTestingDecryption &&
        m_timer.hasExpired(kDecryptionTimeout))
    {
        m_currentTestingDecryption = false;
        return true;
    }

    if (!m_waitingForTables)
        return true;

#if CONFIG_DVB
    // If the rotor is still moving, reset the timer and keep waiting
    DVBSignalMonitor *sigmon = GetDVBSignalMonitor();
    if (sigmon)
    {
        const DiSEqCDevRotor *rotor =
            GetDVBChannel() ? GetDVBChannel()->GetRotor() : nullptr;
        if (rotor)
        {
            bool was_moving = false;
            bool is_moving = false;
            sigmon->GetRotorStatus(was_moving, is_moving);
            if (was_moving && !is_moving)
            {
                m_timer.restart();
                return false;
            }
        }
    }
#endif // CONFIG_DVB

    // have the tables have timed out?
    if (m_timer.hasExpired(m_channelTimeout.count()))
    {
        // the channelTimeout alone is only valid if we have seen no tables..
        const ScanStreamData *sd = nullptr;
        if (GetDTVSignalMonitor())
            sd = GetDTVSignalMonitor()->GetScanStreamData();

        if (!sd)
            return true;

        if (sd->HasCachedAnyNIT() || sd->HasCachedAnySDTs())
            return m_timer.hasExpired(kDVBTableTimeout.count());
        if (sd->HasCachedMGT() || sd->HasCachedAnyVCTs())
            return m_timer.hasExpired(kATSCTableTimeout.count());
        if (sd->HasCachedAnyPAT() || sd->HasCachedAnyPMTs())
            return m_timer.hasExpired(kMPEGTableTimeout.count());

        return true;
    }

    // ok the tables haven't timed out, but have we hit the signal timeout?
    SignalMonitor *sm = GetSignalMonitor();
    if (m_timer.hasExpired((*m_current).m_timeoutTune.count()) &&
        sm && !sm->HasSignalLock())
    {
        const ScanStreamData *sd = nullptr;
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
    QMutexLocker locker(&m_lock);

    bool do_post_insertion = m_waitingForTables;

    if (!HasTimedOut())
        return;

    if (0 == m_nextIt.offset() && m_nextIt == m_scanTransports.begin())
    {
        m_channelList.clear();
        m_channelsFound = 0;
        m_dvbt2Tried = true;
    }

    if ((m_scanDTVTunerType == DTVTunerType::kTunerTypeDVBT2) && ! m_dvbt2Tried)
    {
        // If we failed to get a lock with DVB-T try DVB-T2.
        m_dvbt2Tried = true;
        ScanTransport(m_current);
        return;
    }

    if (0 == m_nextIt.offset() && m_nextIt != m_scanTransports.begin())
    {
        // Add channel to scanned list and potentially check decryption
        if (do_post_insertion && !UpdateChannelInfo(false))
            return;

        // Stop signal monitor for previous transport
        locker.unlock();
        m_signalMonitor->Stop();
        locker.relock();
    }

    m_current = m_nextIt; // Increment current
    m_dvbt2Tried = false;

    if (m_current != m_scanTransports.end())
    {
        ScanTransport(m_current);

        // Increment nextIt
        m_nextIt = m_current;
        ++m_nextIt;
    }
    else if (!m_extendTransports.isEmpty())
    {
        --m_current;
        QMap<uint32_t,DTVMultiplex>::iterator it = m_extendTransports.begin();
        while (it != m_extendTransports.end())
        {
            if (!m_tsScanned.contains(it.key()))
            {
                QString name = QString("TransportID %1").arg(it.key() & 0xffff);
                TransportScanItem item(m_sourceID, name, *it, m_signalTimeout);
                LOG(VB_CHANSCAN, LOG_INFO, LOC + "Adding " + name + ' ' + item.m_tuning.toString());
                m_scanTransports.push_back(item);
                m_tsScanned.insert(it.key());
            }
            ++it;
        }
        m_extendTransports.clear();
        m_nextIt = m_current;
        ++m_nextIt;
    }
    else
    {
        m_scanMonitor->ScanComplete();
        m_scanning = false;
        m_current = m_nextIt = m_scanTransports.end();
    }
}

bool ChannelScanSM::Tune(const transport_scan_items_it_t transport)
{
    const TransportScanItem &item = *transport;

#if CONFIG_DVB
    DVBSignalMonitor *monitor = GetDVBSignalMonitor();
    if (monitor)
    {
        // always wait for rotor to finish
        monitor->AddFlags(SignalMonitor::kDVBSigMon_WaitForPos);
        monitor->SetRotorTarget(1.0F);
    }
#endif // CONFIG_DVB

    DTVChannel *channel = GetDTVChannel();
    if (!channel)
        return false;

    if (item.m_mplexid > 0 && transport.offset() == 0)
        return channel->TuneMultiplex(item.m_mplexid, m_inputName);

    if (item.m_tuning.m_sistandard == "MPEG")
    {
        IPTVTuningData tuning = item.m_iptvTuning;
        if (tuning.GetProtocol() == IPTVTuningData::inValid)
            tuning.GuessProtocol();
        return channel->Tune(tuning, true);
    }

    const uint64_t freq = item.freq_offset(transport.offset());
    DTVMultiplex tuning = item.m_tuning;
    tuning.m_frequency = freq;

    if (m_scanDTVTunerType == DTVTunerType::kTunerTypeDVBT)
    {
        tuning.m_modSys = DTVModulationSystem::kModulationSystem_DVBT;
    }
    if (m_scanDTVTunerType == DTVTunerType::kTunerTypeDVBT2)
    {
        if (m_dvbt2Tried)
            tuning.m_modSys = DTVModulationSystem::kModulationSystem_DVBT2;
        else
            tuning.m_modSys = DTVModulationSystem::kModulationSystem_DVBT;
    }

    return channel->Tune(tuning);
}

void ChannelScanSM::ScanTransport(const transport_scan_items_it_t transport)
{
    QString offset_str = (transport.offset()) ?
        QObject::tr(" offset %2").arg(transport.offset()) : "";
    QString cur_chan = QString("%1%2")
        .arg((*m_current).m_friendlyName, offset_str);
    QString tune_msg_str =
        QObject::tr("ScanTransport Tuning to %1 mplexid(%2)")
        .arg(cur_chan).arg((*m_current).m_mplexid);

    const TransportScanItem &item = *transport;

    if (transport.offset() &&
        (item.freq_offset(transport.offset()) == item.freq_offset(0)))
    {
        m_waitingForTables = false;
        return; // nothing to do
    }

    if (m_channelsFound)
    {
        QString progress = QObject::tr("Found %n", "", m_channelsFound);
        m_scanMonitor->ScanUpdateStatusTitleText(progress);
    }

    m_scanMonitor->ScanUpdateStatusText(cur_chan);
    LOG(VB_CHANSCAN, LOG_INFO, LOC + tune_msg_str);

    if (!Tune(transport))
    {   // If we did not tune successfully, bail with message
        UpdateScanPercentCompleted();
        LOG(VB_CHANSCAN, LOG_ERR, LOC +
            QString("Failed to tune %1 mplexid(%2) at offset %3")
                .arg(item.m_friendlyName).arg(item.m_mplexid)
                .arg(transport.offset()));
        return;
    }

    // If we have a DTV Signal Monitor, perform table scanner reset
    if (GetDTVSignalMonitor() && GetDTVSignalMonitor()->GetScanStreamData())
    {
        GetDTVSignalMonitor()->GetScanStreamData()->Reset();
        GetDTVSignalMonitor()->SetChannel(-1,-1);
        GetDTVSignalMonitor()->SetDVBService(0, 0, -1);
    }

    // Start signal monitor for this channel
    if (m_signalMonitor)
        m_signalMonitor->Start();

    m_timer.start();
    m_waitingForTables = (item.m_tuning.m_sistandard != "analog");
}

/** \fn ChannelScanSM::StopScanner(void)
 *  \brief Stops the ChannelScanSM event loop and the signal monitor,
 *         blocking until both exit.
 */
void ChannelScanSM::StopScanner(void)
{
    LOG(VB_CHANSCAN, LOG_INFO, LOC + "StopScanner");

    while (m_scannerThread)
    {
        m_threadExit = true;
        if (m_scannerThread->wait(1s))
        {
            delete m_scannerThread;
            m_scannerThread = nullptr;
        }
    }

    if (m_signalMonitor)
        m_signalMonitor->Stop();
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
    LOG(VB_CHANSCAN, LOG_DEBUG, LOC +
        QString("%1:%2 ").arg(__FUNCTION__).arg(__LINE__) +
        QString("SourceID:%1 ").arg(SourceID) +
        QString("std:%1 ").arg(std) +
        QString("modulation:%1 ").arg(modulation) +
        QString("country:%1 ").arg(country) +
        QString("table_start:%1 ").arg(table_start) +
        QString("table_end:%1 ").arg(table_end));

    QString name("");
    if (m_scanning)
        return false;

    m_scanTransports.clear();
    m_nextIt = m_scanTransports.end();

    freq_table_list_t tables =
        get_matching_freq_tables(std, modulation, country);

    if (tables.empty())
    {
        QString msg = QString("No freq table for (%1, %2, %3) found")
                      .arg(std, modulation, country);
        m_scanMonitor->ScanAppendTextToLog(msg);
    }
    LOG(VB_CHANSCAN, LOG_INFO, LOC +
        QString("Looked up freq table (%1, %2, %3) w/%4 entries")
            .arg(std, modulation, country, QString::number(tables.size())));

    QString start = table_start;
    const QString& end   = table_end;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (auto it = tables.begin(); it != tables.end(); ++it)
    {
        const FrequencyTable &ft = **it;
        int     name_num         = ft.m_nameOffset;
        QString strNameFormat    = ft.m_nameFormat;
        uint    freq             = ft.m_frequencyStart;
        while (freq <= ft.m_frequencyEnd)
        {
            name = strNameFormat;
            if (strNameFormat.indexOf("%") >= 0)
                name = strNameFormat.arg(name_num);

            if (start.isEmpty() || name == start)
            {
                start.clear();

                TransportScanItem item(SourceID, std, name, name_num,
                                       freq, ft, m_signalTimeout);
                m_scanTransports.push_back(item);

                LOG(VB_CHANSCAN, LOG_INFO, LOC + "ScanTransports " +
                    item.toString());
            }

            ++name_num;
            freq += ft.m_frequencyStep;

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

    m_extendScanList = true;
    m_timer.start();
    m_waitingForTables = false;

    m_nextIt            = m_scanTransports.begin();
    m_transportsScanned = 0;
    m_scanning          = true;

    return true;
}

bool ChannelScanSM::ScanForChannels(uint sourceid,
                             const QString &std,
                             const QString &cardtype,
                             const DTVChannelList &channels)
{
    m_scanTransports.clear();
    m_nextIt = m_scanTransports.end();

    DTVTunerType tunertype;
    tunertype.Parse(cardtype);

    auto it = channels.cbegin();
    for (uint i = 0; it != channels.cend(); ++it, ++i)
    {
        DTVTransport tmp = *it;
        tmp.m_sistandard = std;
        TransportScanItem item(sourceid, QString::number(i),
                               tunertype, tmp, m_signalTimeout);

        m_scanTransports.push_back(item);

        LOG(VB_CHANSCAN, LOG_INFO, LOC + "ScanForChannels " + item.toString());
    }

    if (m_scanTransports.empty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "ScanForChannels() no transports");
        return false;
    }

    m_timer.start();
    m_waitingForTables = false;

    m_nextIt            = m_scanTransports.begin();
    m_transportsScanned = 0;
    m_scanning          = true;

    return true;
}

bool ChannelScanSM::ScanIPTVChannels(uint sourceid,
                                     const fbox_chan_map_t &iptv_channels)
{
    m_scanTransports.clear();
    m_nextIt = m_scanTransports.end();

    fbox_chan_map_t::const_iterator Ichan = iptv_channels.begin();
    for (uint idx = 0; Ichan != iptv_channels.end(); ++Ichan, ++idx)
    {
        TransportScanItem item(sourceid, QString::number(idx),
                               Ichan.value().m_tuning, Ichan.key(),
                               m_signalTimeout);

        m_scanTransports.push_back(item);

        LOG(VB_CHANSCAN, LOG_INFO, LOC + "ScanIPTVChannels " + item.toString());
    }

    if (m_scanTransports.empty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "ScanIPTVChannels() no transports");
        return false;
    }

    m_timer.start();
    m_waitingForTables = false;

    m_nextIt            = m_scanTransports.begin();
    m_transportsScanned = 0;
    m_scanning          = true;

    return true;
}


/** \fn ChannelScanSM::ScanTransportsStartingOn(int,const QMap<QString,QString>&)
 *  \brief Generates a list of frequencies to scan and adds it to the
 *   scanTransport list, and then sets the scanning to TRANSPORT_LIST.
 */
bool ChannelScanSM::ScanTransportsStartingOn(
    int sourceid, const QMap<QString,QString> &startChan)
{
    auto iter = startChan.find("type");
    if (iter == startChan.end())
        return false;
    iter = startChan.find("std");
    if (iter == startChan.end())
        return false;

    QString si_std = ((*iter).toLower() != "atsc") ? "dvb" : "atsc";
    bool    ok     = false;

    if (m_scanning)
        return false;

    m_scanTransports.clear();
    m_nextIt = m_scanTransports.end();

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
        tuning.m_sistandard = si_std;
        TransportScanItem item(
            sourceid, QObject::tr("Frequency %1").arg(startChan["frequency"]),
            tuning, m_signalTimeout);
        m_scanTransports.push_back(item);
    }

    if (!ok)
        return false;

    m_extendScanList = true;

    m_timer.start();
    m_waitingForTables = false;

    m_nextIt            = m_scanTransports.begin();
    m_transportsScanned = 0;
    m_scanning          = true;

    return true;
}

bool ChannelScanSM::AddToList(uint mplexid)
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT sourceid, sistandard, transportid, frequency, modulation, mod_sys "
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
    uint    frequency  = query.value(3).toUInt();
    QString modulation = query.value(4).toString();
    QString mod_sys    = query.value(5).toString();
    DTVModulationSystem delsys;
    delsys.Parse(mod_sys);
    DTVTunerType tt = CardUtil::ConvertToTunerType(delsys);
    QString fn = (tsid) ? QString("Transport ID %1").arg(tsid) :
        QString("Multiplex #%1").arg(mplexid);

    if (modulation == "8vsb")
    {
        QString chan = QString("%1 Hz").arg(frequency);
        int findFrequency = (query.value(3).toInt() / 1000) - 1750;
        for (const auto & list : gChanLists[0].list)
        {
            if ((list.freq <= findFrequency + 200) &&
                (list.freq >= findFrequency - 200))
            {
                chan = QString("%1").arg(list.name);
            }
        }
        fn = QObject::tr("ATSC Channel %1").arg(chan);
        tt = DTVTunerType::kTunerTypeATSC;
    }

    tt = GuessDTVTunerType(tt);

    TransportScanItem item(sourceid, sistandard, fn, mplexid, m_signalTimeout);

    LOG(VB_CHANSCAN, LOG_DEBUG, LOC +
        QString("tunertype:%1 %2 sourceid:%3 sistandard:%4 fn:'%5' mplexid:%6")
            .arg(tt).arg(tt.toString()).arg(sourceid).arg(sistandard, fn).arg(mplexid));

    if (item.m_tuning.FillFromDB(tt, mplexid))
    {
        LOG(VB_CHANSCAN, LOG_INFO, LOC + "Adding " + fn);
        m_scanTransports.push_back(item);
        return true;
    }

    LOG(VB_CHANSCAN, LOG_INFO, LOC + "Not adding incomplete transport " + fn);
    return false;
}

bool ChannelScanSM::ScanTransport(uint mplexid, bool follow_nit)
{
    m_scanTransports.clear();
    m_nextIt = m_scanTransports.end();

    AddToList(mplexid);

    m_timer.start();
    m_waitingForTables  = false;

    m_extendScanList = follow_nit;
    m_transportsScanned = 0;
    if (!m_scanTransports.empty())
    {
        m_nextIt   = m_scanTransports.begin();
        m_scanning = true;
        return true;
    }

    return false;
}

bool ChannelScanSM::ScanCurrentTransport(const QString &sistandard)
{
    m_scanTransports.clear();
    m_nextIt = m_scanTransports.end();

    m_signalTimeout = 30s;
    QString name;
    TransportScanItem item(m_sourceID, sistandard, name, 0, m_signalTimeout);
    m_scanTransports.push_back(item);

    m_timer.start();
    m_waitingForTables = false;
    m_extendScanList = false;
    m_transportsScanned = 0;
    m_nextIt   = m_scanTransports.begin();
    m_scanning = true;
    return true;
}

/** \fn ChannelScanSM::CheckImportedList(const DTVChannelInfoList&,uint,QString&,QString&,QString&)
 *  \brief If we are scanning a dvb-utils import verify channel is in list.
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
    for (const auto & channel : channels)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("comparing %1 %2 against %3 %4")
                .arg(channel.m_serviceid).arg(channel.m_name)
                .arg(mpeg_program_num).arg(common_status_info));

        if (channel.m_serviceid == mpeg_program_num)
        {
            found = true;
            if (!channel.m_name.isEmpty())
            {
                service_name = channel.m_name;
                callsign     = channel.m_name;
            }
        }
    }

    if (found)
    {
        common_status_info += QString(" %1 %2")
            .arg(QObject::tr("as"), service_name);
    }
    else
    {
        m_scanMonitor->ScanAppendTextToLog(
            QObject::tr("Skipping %1, not in imported channel map")
            .arg(common_status_info));
    }

    return found;
}
