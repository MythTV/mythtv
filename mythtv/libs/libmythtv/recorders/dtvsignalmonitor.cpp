#include <algorithm> // for lower_bound
#include <unistd.h>

#include "libmythbase/compat.h"

#include "dtvchannel.h"
#include "dtvsignalmonitor.h"
#include "mpeg/atsctables.h"
#include "mpeg/dvbtables.h"
#include "mpeg/mpegtables.h"
#include "mpeg/scanstreamdata.h"

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DBG_SM(FUNC, MSG) LOG(VB_CHANNEL, LOG_INFO, \
    QString("DTVSigMon[%1]: %2 %3").arg(m_inputid).arg(FUNC, MSG))

#define LOC QString("DTVSigMon[%1]: ").arg(m_inputid)

// inserts tid&crc value into an ordered list
// returns true if item is inserted
static bool insert_crc(QList<uint64_t> &seen_crc, const PSIPTable &psip)
{
    uint64_t key = (((uint64_t)psip.TableID()) << 32) | psip.CRC();

    QList<uint64_t>::iterator it =
        std::lower_bound(seen_crc.begin(), seen_crc.end(), key);

    if ((it == seen_crc.end()) || (*it != key))
    {
        seen_crc.insert(it, key);
        return true;
    }

    return false;
}

/** \class DTVSignalMonitor
 *  \brief This class is intended to detect the presence of needed tables.
 */

DTVSignalMonitor::DTVSignalMonitor(int db_cardnum,
                                   DTVChannel *_channel,
                                   bool _release_stream,
                                   uint64_t wait_for_mask)
    : SignalMonitor(db_cardnum, _channel, _release_stream, wait_for_mask),
      m_seenPAT(QObject::tr("Seen")+" PAT", "seen_pat", 1, true, 0, 1, 0ms),
      m_seenPMT(QObject::tr("Seen")+" PMT", "seen_pmt", 1, true, 0, 1, 0ms),
      m_seenMGT(QObject::tr("Seen")+" MGT", "seen_mgt", 1, true, 0, 1, 0ms),
      m_seenVCT(QObject::tr("Seen")+" VCT", "seen_vct", 1, true, 0, 1, 0ms),
      m_seenNIT(QObject::tr("Seen")+" NIT", "seen_nit", 1, true, 0, 1, 0ms),
      m_seenSDT(QObject::tr("Seen")+" SDT", "seen_sdt", 1, true, 0, 1, 0ms),
      m_seenCrypt(QObject::tr("Seen")+" Crypt", "seen_crypt", 1, true, 0, 1, 0ms),
      m_matchingPAT(QObject::tr("Matching")+" PAT", "matching_pat", 1, true, 0, 1, 0ms),
      m_matchingPMT(QObject::tr("Matching")+" PMT", "matching_pmt", 1, true, 0, 1, 0ms),
      m_matchingMGT(QObject::tr("Matching")+" MGT", "matching_mgt", 1, true, 0, 1, 0ms),
      m_matchingVCT(QObject::tr("Matching")+" VCT", "matching_vct", 1, true, 0, 1, 0ms),
      m_matchingNIT(QObject::tr("Matching")+" NIT", "matching_nit", 1, true, 0, 1, 0ms),
      m_matchingSDT(QObject::tr("Matching")+" SDT", "matching_sdt", 1, true, 0, 1, 0ms),
      m_matchingCrypt(QObject::tr("Matching")+" Crypt", "matching_crypt",
                    1, true, 0, 1, 0ms)
{
}

DTVSignalMonitor::~DTVSignalMonitor()
{
    DTVSignalMonitor::SetStreamData(nullptr);
}

DTVChannel *DTVSignalMonitor::GetDTVChannel(void)
{
    return dynamic_cast<DTVChannel*>(m_channel);
}

QStringList DTVSignalMonitor::GetStatusList(void) const
{
    QStringList list = SignalMonitor::GetStatusList();
    QMutexLocker locker(&m_statusLock);

    // mpeg tables
    if (m_flags & kDTVSigMon_WaitForPAT)
    {
        list<<m_seenPAT.GetName()<<m_seenPAT.GetStatus();
        list<<m_matchingPAT.GetName()<<m_matchingPAT.GetStatus();
    }
    if (m_flags & kDTVSigMon_WaitForPMT)
    {
#define DEBUG_PMT 0
#if DEBUG_PMT
        static int seenGood = -1;
        static int matchingGood = -1;
#endif
        list<<m_seenPMT.GetName()<<m_seenPMT.GetStatus();
        list<<m_matchingPMT.GetName()<<m_matchingPMT.GetStatus();
#if DEBUG_PMT
        if ((seenGood != (int)m_seenPMT.IsGood()) ||
            (matchingGood != (int)m_matchingPMT.IsGood()))
        {
            DBG_SM("GetStatusList", QString("WaitForPMT seen(%1) matching(%2)")
                                    .arg(m_seenPMT.IsGood())
                                    .arg(m_matchingPMT.IsGood()));
            seenGood = (int)m_seenPMT.IsGood();
            matchingGood = (int)m_matchingPMT.IsGood();
        }
#endif
    }
    // atsc tables
    if (m_flags & kDTVSigMon_WaitForMGT)
    {
        list<<m_seenMGT.GetName()<<m_seenMGT.GetStatus();
        list<<m_matchingMGT.GetName()<<m_matchingMGT.GetStatus();
    }
    if (m_flags & kDTVSigMon_WaitForVCT)
    {
        list<<m_seenVCT.GetName()<<m_seenVCT.GetStatus();
        list<<m_matchingVCT.GetName()<<m_matchingVCT.GetStatus();
    }
    // dvb tables
    if (m_flags & kDTVSigMon_WaitForNIT)
    {
        list<<m_seenNIT.GetName()<<m_seenNIT.GetStatus();
        list<<m_matchingNIT.GetName()<<m_matchingNIT.GetStatus();
    }
    if (m_flags & kDTVSigMon_WaitForSDT)
    {
        list<<m_seenSDT.GetName()<<m_seenSDT.GetStatus();
        list<<m_matchingSDT.GetName()<<m_matchingSDT.GetStatus();
    }
    if (m_flags & kDTVSigMon_WaitForCrypt)
    {
        list<<m_seenCrypt.GetName()<<m_seenCrypt.GetStatus();
        list<<m_matchingCrypt.GetName()<<m_matchingCrypt.GetStatus();
    }
    if (!m_error.isEmpty())
    {
        list<<"error"<<m_error;
    }
    return list;
}

void DTVSignalMonitor::AddFlags(uint64_t _flags)
{
    SignalMonitor::AddFlags(_flags);
    UpdateMonitorValues();
}

void DTVSignalMonitor::RemoveFlags(uint64_t _flags)
{
    SignalMonitor::RemoveFlags(_flags);
    UpdateMonitorValues();
}

void DTVSignalMonitor::UpdateMonitorValues(void)
{
    QMutexLocker locker(&m_statusLock);
    m_seenPAT.SetValue(    (m_flags & kDTVSigMon_PATSeen)  ? 1 : 0);
    m_seenPMT.SetValue(    (m_flags & kDTVSigMon_PMTSeen)  ? 1 : 0);
    m_seenMGT.SetValue(    (m_flags & kDTVSigMon_MGTSeen)  ? 1 : 0);
    m_seenVCT.SetValue(    (m_flags & kDTVSigMon_VCTSeen)  ? 1 : 0);
    m_seenNIT.SetValue(    (m_flags & kDTVSigMon_NITSeen)  ? 1 : 0);
    m_seenSDT.SetValue(    (m_flags & kDTVSigMon_SDTSeen)  ? 1 : 0);
    m_seenCrypt.SetValue(  (m_flags & kDTVSigMon_CryptSeen)? 1 : 0);
    m_matchingPAT.SetValue((m_flags & kDTVSigMon_PATMatch) ? 1 : 0);
    m_matchingPMT.SetValue((m_flags & kDTVSigMon_PMTMatch) ? 1 : 0);
    m_matchingMGT.SetValue((m_flags & kDTVSigMon_MGTMatch) ? 1 : 0);
    m_matchingVCT.SetValue((m_flags & kDTVSigMon_VCTMatch) ? 1 : 0);
    m_matchingNIT.SetValue((m_flags & kDTVSigMon_NITMatch) ? 1 : 0);
    m_matchingSDT.SetValue((m_flags & kDTVSigMon_SDTMatch) ? 1 : 0);
    m_matchingCrypt.SetValue((m_flags & kDTVSigMon_CryptMatch) ? 1 : 0);
}

void DTVSignalMonitor::UpdateListeningForEIT(void)
{
    std::vector<uint> add_eit;
    std::vector<uint> del_eit;

    if (GetStreamData()->HasEITPIDChanges(m_eitPids) &&
        GetStreamData()->GetEITPIDChanges(m_eitPids, add_eit, del_eit))
    {
        for (uint eit : del_eit)
        {
            uint_vec_t::iterator it;
            it = find(m_eitPids.begin(), m_eitPids.end(), eit);
            if (it != m_eitPids.end())
                m_eitPids.erase(it);
            GetStreamData()->RemoveListeningPID(eit);
        }

        for (uint eit : add_eit)
        {
            m_eitPids.push_back(eit);
            GetStreamData()->AddListeningPID(eit);
        }
    }
}

void DTVSignalMonitor::SetChannel(int major, int minor)
{
    DBG_SM(QString("SetChannel(%1, %2)").arg(major).arg(minor), "");
    m_seenTableCrc.clear();
    ATSCStreamData *atsc = GetATSCStreamData();
    if (atsc && (m_majorChannel != major || m_minorChannel != minor))
    {
        RemoveFlags(kDTVSigMon_PATSeen   | kDTVSigMon_PATMatch |
                    kDTVSigMon_PMTSeen   | kDTVSigMon_PMTMatch |
                    kDTVSigMon_VCTSeen   | kDTVSigMon_VCTMatch |
                    kDTVSigMon_CryptSeen | kDTVSigMon_CryptMatch);
        m_majorChannel = major;
        m_minorChannel = minor;
        atsc->SetDesiredChannel(major, minor);
        AddFlags(kDTVSigMon_WaitForVCT | kDTVSigMon_WaitForPAT);
    }
}

void DTVSignalMonitor::SetProgramNumber(int progNum)
{
    DBG_SM(QString("SetProgramNumber(%1)").arg(progNum), "");
    m_seenTableCrc.clear();
    if (m_programNumber != progNum)
    {
        RemoveFlags(kDTVSigMon_PMTSeen   | kDTVSigMon_PMTMatch |
                    kDTVSigMon_CryptSeen | kDTVSigMon_CryptMatch);
        m_programNumber = progNum;
        if (GetStreamData())
            GetStreamData()->SetDesiredProgram(m_programNumber);
        AddFlags(kDTVSigMon_WaitForPMT);
    }
}

void DTVSignalMonitor::SetDVBService(uint network_id, uint transport_id, int service_id)
{
    DBG_SM(QString("SetDVBService(transport_id: %1, network_id: %2, "
                   "service_id: %3)").arg(transport_id).arg(network_id).arg(service_id), "");
    m_seenTableCrc.clear();

    if (network_id == m_networkID && transport_id == m_transportID &&
        service_id == m_programNumber)
    {
        return;
    }

    RemoveFlags(kDTVSigMon_PMTSeen   | kDTVSigMon_PMTMatch |
                kDTVSigMon_SDTSeen   | kDTVSigMon_SDTMatch |
                kDTVSigMon_CryptSeen | kDTVSigMon_CryptMatch);

    m_transportID   = transport_id;
    m_networkID     = network_id;
    m_programNumber = service_id;

    DVBStreamData *dvb = GetDVBStreamData();
    if (dvb != nullptr)
    {
        dvb->SetDesiredService(network_id, transport_id, m_programNumber);
        AddFlags(kDTVSigMon_WaitForPMT | kDTVSigMon_WaitForSDT);
        dvb->AddListeningPID(PID::DVB_SDT_PID);
    }
}

void DTVSignalMonitor::SetStreamData(MPEGStreamData *data)
{
    if (m_streamData)
        m_streamData->RemoveMPEGListener(this);

    ATSCStreamData *atsc = GetATSCStreamData();
    DVBStreamData  *dvb  = GetDVBStreamData();
    if (atsc)
    {
        atsc->RemoveATSCMainListener(this);
        atsc->RemoveATSCAuxListener(this);
    }
    if (dvb)
        dvb->RemoveDVBMainListener(this);

    m_streamData = data;
    if (!data)
        return;

    data->AddMPEGListener(this);

    atsc = GetATSCStreamData();
    dvb  = GetDVBStreamData();
    if (atsc)
    {
        atsc->AddATSCMainListener(this);
        atsc->AddATSCAuxListener(this);
    }
    if (dvb)
        dvb->AddDVBMainListener(this);
}


void DTVSignalMonitor::HandlePAT(const ProgramAssociationTable *pat)
{
    AddFlags(kDTVSigMon_PATSeen);
    int pmt_pid = pat->FindPID(m_programNumber);
    if (GetStreamData() && pmt_pid)
    {
        AddFlags(kDTVSigMon_PATMatch);
        GetStreamData()->AddListeningPID(pmt_pid);
        insert_crc(m_seenTableCrc, *pat);
        return;
    }

    if (m_programNumber >= 0)
    {
        // BEGIN HACK HACK HACK
        // Reset version in case we're physically on the wrong transport
        // due to tuning hardware being in a transitional state or we
        // are in the middle of something like a DiSEqC rotor turn.
        uint tsid = pat->TransportStreamID();
        GetStreamData()->SetVersionPAT(tsid, -1,0);
        // END HACK HACK HACK

        LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("HandlePAT() ") +
            QString("Received PAT for tsid %1 waiting for tsid %2")
                .arg(tsid).arg(m_transportID));

        // Only one entry in the PAT, just use this program
        int spts_program = 0;
        if (pat->ProgramCount() == 1)
        {
            spts_program = pat->ProgramNumber(0);
            pmt_pid = pat->ProgramPID(0);
        }

        // Two entries in the PAT, but if one is a pointer to the NIT PID
        // instead of a real program then the other is the single program.
        if (pat->ProgramCount() == 2)
        {
            if (pat->ProgramNumber(0) == 0)
            {
                spts_program = pat->ProgramNumber(1);
                pmt_pid = pat->ProgramPID(1);
            }
            else if (pat->ProgramNumber(1) == 0)
            {
                spts_program = pat->ProgramNumber(0);
                pmt_pid = pat->ProgramPID(0);
            }
        }

        if (spts_program > 0)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Program %1 found in PAT of SPTS").arg(spts_program));
            SetProgramNumber(spts_program);
            AddFlags(kDTVSigMon_PATMatch);
            GetStreamData()->AddListeningPID(pmt_pid);
        }

        if (insert_crc(m_seenTableCrc, *pat))
        {
            QString errStr = QString("Program #%1 not found in PAT!")
                .arg(m_programNumber);
            LOG(VB_GENERAL, LOG_ERR, LOC + errStr + "\n" + pat->toString());
        }
    }
}

void DTVSignalMonitor::HandlePMT(uint /*program_num*/, const ProgramMapTable *pmt)
{
    AddFlags(kDTVSigMon_PMTSeen);

    if (m_programNumber < 0)
        return; // don't print error messages during channel scan.

    if (pmt->ProgramNumber() != (uint)m_programNumber)
    {
        if (insert_crc(m_seenTableCrc, *pmt))
        {
            LOG(VB_CHANNEL, LOG_DEBUG, LOC +
                QString("Wrong PMT; pmt->ProgramNumber(%1) desired(%2)")
                .arg(pmt->ProgramNumber()).arg(m_programNumber));
        }
        return; // Not the PMT we are looking for...
    }

    LOG(VB_CHANNEL, LOG_DEBUG, LOC +
        QString("Found PMT; pmt->ProgramNumber(%1)")
        .arg(pmt->ProgramNumber()));

    if (pmt->IsEncrypted(GetDTVChannel()->GetSIStandard())) {
        LOG(VB_GENERAL, LOG_NOTICE, LOC +
            QString("PMT says program %1 is encrypted").arg(m_programNumber));
        GetStreamData()->TestDecryption(pmt);
    }

    // if PMT contains audio and/or video stream set as matching.
    uint hasAudio = 0;
    uint hasVideo = 0;

    for (uint i = 0; i < pmt->StreamCount(); i++)
    {
        if (pmt->IsVideo(i, GetDTVChannel()->GetSIStandard()))
            hasVideo++;
        if (pmt->IsAudio(i, GetDTVChannel()->GetSIStandard()))
            hasAudio++;
    }

    if ((hasVideo >= GetStreamData()->GetVideoStreamsRequired()) &&
        (hasAudio >= GetStreamData()->GetAudioStreamsRequired()))
    {
        if (pmt->IsEncrypted(GetDTVChannel()->GetSIStandard()) &&
            !m_ignoreEncrypted)
            AddFlags(kDTVSigMon_WaitForCrypt);

        AddFlags(kDTVSigMon_PMTMatch);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("We want %1 audio and %2 video streams")
                .arg(GetStreamData()->GetAudioStreamsRequired())
                .arg(GetStreamData()->GetVideoStreamsRequired()) +
            QString("\n\t\t\tBut have %1 audio and %2 video streams")
                .arg(hasAudio).arg(hasVideo));
    }
}

void DTVSignalMonitor::HandleSTT(const SystemTimeTable* /*stt*/)
{
    LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("Time Offset: %1")
            .arg(GetStreamData()->TimeOffset()));
}

void DTVSignalMonitor::HandleMGT(const MasterGuideTable* mgt)
{
    AddFlags(kDTVSigMon_MGTSeen);

    ATSCStreamData *atsc = GetATSCStreamData();
    if (!atsc)
        return;

    for (uint i=0; i<mgt->TableCount(); i++)
    {
        if ((TableClass::TVCTc == mgt->TableClass(i)) ||
            (TableClass::CVCTc == mgt->TableClass(i)))
        {
            atsc->AddListeningPID(mgt->TablePID(i));
            AddFlags(kDTVSigMon_MGTMatch);
        }
    }
}

void DTVSignalMonitor::HandleTVCT(
    uint /*pid*/, const TerrestrialVirtualChannelTable* tvct)
{
    AddFlags(kDTVSigMon_VCTSeen | kDTVSigMon_TVCTSeen);
    int idx = tvct->Find(m_majorChannel, m_minorChannel);

    if (m_minorChannel < 0)
        return; // don't print error message during channel scan.

    if (idx < 0)
    {
        if (insert_crc(m_seenTableCrc, *tvct))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Could not find channel %1_%2 in TVCT")
                .arg(m_majorChannel).arg(m_minorChannel));
            LOG(VB_GENERAL, LOG_DEBUG, LOC + tvct->toString());
        }
        ATSCStreamData *atsc = GetATSCStreamData();
        if (atsc)
            atsc->SetVersionTVCT(tvct->TransportStreamID(),-1);
        return;
    }

    DBG_SM("HandleTVCT()", QString("tvct->ProgramNumber(idx %1): prog num %2")
           .arg(idx).arg(tvct->ProgramNumber(idx)));

    SetProgramNumber(tvct->ProgramNumber(idx));
    AddFlags(kDTVSigMon_VCTMatch | kDTVSigMon_TVCTMatch);
}

void DTVSignalMonitor::HandleCVCT(uint /*pid*/, const CableVirtualChannelTable* cvct)
{
    AddFlags(kDTVSigMon_VCTSeen | kDTVSigMon_CVCTSeen);
    int idx = cvct->Find(m_majorChannel, m_minorChannel);

    if (idx < 0)
    {
        if (insert_crc(m_seenTableCrc, *cvct))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Could not find channel %1_%2 in CVCT")
                .arg(m_majorChannel).arg(m_minorChannel));
            LOG(VB_GENERAL, LOG_DEBUG, LOC + cvct->toString());
        }
        ATSCStreamData *atsc = GetATSCStreamData();
        if (atsc)
            atsc->SetVersionCVCT(cvct->TransportStreamID(),-1);
        return;
    }

    DBG_SM("HandleCVCT()", QString("cvct->ProgramNumber(idx %1): prog num %2")
           .arg(idx).arg(cvct->ProgramNumber(idx)));

    SetProgramNumber(cvct->ProgramNumber(idx));
    AddFlags(kDTVSigMon_VCTMatch | kDTVSigMon_CVCTMatch);
}

void DTVSignalMonitor::HandleTDT(const TimeDateTable* /*tdt*/)
{
    LOG(VB_CHANNEL, LOG_DEBUG, LOC + QString("Time Offset: %1")
            .arg(GetStreamData()->TimeOffset()));
}

void DTVSignalMonitor::HandleNIT(const NetworkInformationTable *nit)
{
    DBG_SM("HandleNIT()", QString("net_id = %1").arg(nit->NetworkID()));
    AddFlags(kDTVSigMon_NITSeen);
    if (!GetDVBStreamData())
        return;
}

void DTVSignalMonitor::HandleSDT(uint /*tsid*/, const ServiceDescriptionTable *sdt)
{
    AddFlags(kDTVSigMon_SDTSeen);

    m_detectedNetworkID = sdt->OriginalNetworkID();
    m_detectedTransportID = sdt->TSID();

    // if the multiplex is not properly configured with ONID and TSID then take
    // whatever SDT we see first
    if ((m_networkID == 0) && (m_transportID == 0))
    {
        m_networkID = m_detectedNetworkID;
        m_transportID = m_detectedTransportID;

        // FIXME assert if TableID == SDTo
    }

    if (sdt->OriginalNetworkID() != m_networkID || sdt->TSID() != m_transportID)
    {
        DVBStreamData *dvb = GetDVBStreamData();
        if (dvb)
            dvb->SetVersionSDT(sdt->TSID(), -1, 0);
    }
    else
    {
        DBG_SM("HandleSDT()", QString("tsid = %1 orig_net_id = %2")
               .arg(sdt->TSID()).arg(sdt->OriginalNetworkID()));
        AddFlags(kDTVSigMon_SDTMatch);
        RemoveFlags(kDVBSigMon_WaitForPos);
    }
}

void DTVSignalMonitor::HandleEncryptionStatus(uint /*program_number*/, bool enc_status)
{
    AddFlags(kDTVSigMon_CryptSeen);
    if (!enc_status)
        AddFlags(kDTVSigMon_CryptMatch);
}

ATSCStreamData *DTVSignalMonitor::GetATSCStreamData()
{
    return dynamic_cast<ATSCStreamData*>(m_streamData);
}

DVBStreamData *DTVSignalMonitor::GetDVBStreamData()
{
    return dynamic_cast<DVBStreamData*>(m_streamData);
}

ScanStreamData *DTVSignalMonitor::GetScanStreamData()
{
    return dynamic_cast<ScanStreamData*>(m_streamData);
}

const ATSCStreamData *DTVSignalMonitor::GetATSCStreamData() const
{
    return dynamic_cast<const ATSCStreamData*>(m_streamData);
}

const DVBStreamData *DTVSignalMonitor::GetDVBStreamData() const
{
    return dynamic_cast<const DVBStreamData*>(m_streamData);
}

const ScanStreamData *DTVSignalMonitor::GetScanStreamData() const
{
    return dynamic_cast<const ScanStreamData*>(m_streamData);
}

bool DTVSignalMonitor::IsAllGood(void) const
{
    QMutexLocker locker(&m_statusLock);
    if (!SignalMonitor::IsAllGood())
        return false;
    if ((m_flags & kDTVSigMon_WaitForPAT) && !m_matchingPAT.IsGood())
            return false;
    if ((m_flags & kDTVSigMon_WaitForPMT) && !m_matchingPMT.IsGood())
            return false;
    if ((m_flags & kDTVSigMon_WaitForMGT) && !m_matchingMGT.IsGood())
            return false;
    if ((m_flags & kDTVSigMon_WaitForVCT) && !m_matchingVCT.IsGood())
            return false;
    if ((m_flags & kDTVSigMon_WaitForNIT) && !m_matchingNIT.IsGood())
            return false;
    if ((m_flags & kDTVSigMon_WaitForSDT) && !m_matchingSDT.IsGood())
            return false;
    if ((m_flags & kDTVSigMon_WaitForCrypt) && !m_matchingCrypt.IsGood())
            return false;

    return true;
}
