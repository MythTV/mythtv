// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson

#include <cmath>

#include <algorithm>

#include "atscstreamdata.h"
#include "atsctables.h"
#include "sctetables.h"
#include "eithelper.h"

#define LOC QString("ATSCStream[%1]: ").arg(m_cardId)

/** \class ATSCStreamData
 *  \brief Encapsulates data about ATSC stream and emits events for most tables.
 */

/** \fn ATSCStreamData::ATSCStreamData(int, int, bool)
 *  \brief Initializes ATSCStreamData.
 *
 *   This adds the PID of the PAT and ATSC PSIP tables to "_pids_listening"
 *
 *  \param desiredMajorChannel If you want rewritten PAT and PMTs for a desired
 *                             channel set this to a value greater than zero.
 *  \param desiredMinorChannel If you want rewritten PAT and PMTs for a desired
 *                             channel set this to a value greater than zero.
 *  \param cardnum             The card number that this stream is on.
 *                             Currently only used for differentiating streams
 *                             in log messages.
 *  \param cacheTables         If true important tables will be cached.
 */
ATSCStreamData::ATSCStreamData(int desiredMajorChannel,
                               int desiredMinorChannel,
                               int cardnum, bool cacheTables)
    : MPEGStreamData(-1, cardnum, cacheTables),
      m_desiredMajorChannel(desiredMajorChannel),
      m_desiredMinorChannel(desiredMinorChannel)
{
    AddListeningPID(PID::ATSC_PSIP_PID);
    AddListeningPID(PID::SCTE_PSIP_PID);
}

ATSCStreamData::~ATSCStreamData()
{
    Reset(-1,-1);

    QMutexLocker locker(&m_listenerLock);
    m_atscMainListeners.clear();
    m_atscAuxListeners.clear();
    m_atscEitListeners.clear();

    m_scteMainlisteners.clear();
    m_atsc81EitListeners.clear();
}

void ATSCStreamData::SetDesiredChannel(int major, int minor)
{
    bool reset = true;
    const MasterGuideTable *mgt = GetCachedMGT();
    tvct_vec_t tvcts = GetCachedTVCTs();
    cvct_vec_t cvcts = GetCachedCVCTs();

    if (mgt && (!tvcts.empty() || !cvcts.empty()))
    {
        const TerrestrialVirtualChannelTable *tvct = nullptr;
        const CableVirtualChannelTable       *cvct = nullptr;
        int chan_idx = -1;
        for (uint i = 0; (i < tvcts.size()) && (chan_idx < 0); i++)
        {
            tvct = tvcts[i];
            chan_idx = tvcts[i]->Find(major, minor);
        }
        for (uint i = (chan_idx < 0) ? 0 : cvcts.size();
             (i < cvcts.size()) && (chan_idx < 0); i++)
        {
            cvct = cvcts[i];
            chan_idx = cvcts[i]->Find(major, minor);
        }

        if (chan_idx >= 0)
        {
            m_desiredMajorChannel = major;
            m_desiredMinorChannel = minor;

            ProcessMGT(mgt);

            if (cvct)
            {
                ProcessCVCT(cvct->TransportStreamID(), cvct);
                SetDesiredProgram(cvct->ProgramNumber(chan_idx));
            }
            else if (tvct)
            {
                ProcessTVCT(tvct->TransportStreamID(), tvct);
                SetDesiredProgram(tvct->ProgramNumber(chan_idx));
            }
            reset = false;
        }
    }

    ReturnCachedTable(mgt);
    ReturnCachedTVCTTables(tvcts);
    ReturnCachedCVCTTables(cvcts);

    if (reset)
        Reset(major, minor);
}

void ATSCStreamData::Reset(int desiredProgram)
{
    MPEGStreamData::Reset(desiredProgram);
    AddListeningPID(PID::ATSC_PSIP_PID);
    AddListeningPID(PID::SCTE_PSIP_PID);
}

void ATSCStreamData::Reset(int desiredMajorChannel, int desiredMinorChannel)
{
    m_desiredMajorChannel = desiredMajorChannel;
    m_desiredMinorChannel = desiredMinorChannel;

    MPEGStreamData::Reset(-1);
    m_mgtVersion = -1;
    m_tvctVersion.clear();
    m_cvctVersion.clear();
    m_eitStatus.clear();

    m_sourceIdToAtscMajMin.clear();
    m_atscEitPids.clear();
    m_atscEttPids.clear();

    {
        QMutexLocker locker(&m_cacheLock);

        DeleteCachedTable(m_cachedMgt);
        m_cachedMgt = nullptr;

        for (const auto & cached : std::as_const(m_cachedTvcts))
            DeleteCachedTable(cached);
        m_cachedTvcts.clear();

        for (const auto & cached : std::as_const(m_cachedCvcts))
            DeleteCachedTable(cached);
        m_cachedCvcts.clear();
    }

    AddListeningPID(PID::ATSC_PSIP_PID);
    AddListeningPID(PID::SCTE_PSIP_PID);
}

/** \fn ATSCStreamData::IsRedundant(uint pid, const PSIPTable&) const
 *  \brief Returns true if table already seen.
 *  \todo All RRT tables are ignored
 *  \todo We don't check the start time of EIT and ETT tables
 *        in the version check, so many tables are improperly
 *        ignored.
 */
bool ATSCStreamData::IsRedundant(uint pid, const PSIPTable &psip) const
{
    if (MPEGStreamData::IsRedundant(pid, psip))
        return true;

    const int table_id = psip.TableID();
    const int version  = psip.Version();

    if (TableID::EIT == table_id)
    {
        uint key = (pid<<16) | psip.TableIDExtension();
        return m_eitStatus.IsSectionSeen(key, version, psip.Section());
    }

    if (TableID::ETT == table_id)
        return false; // retransmit ETT's we've seen

    if (TableID::STT == table_id)
        return false; // each SystemTimeTable matters

    if (TableID::STTscte == table_id)
        return false; // each SCTESystemTimeTable matters

    if (TableID::MGT == table_id)
        return VersionMGT() == version;

    if (TableID::TVCT == table_id)
    {
        return VersionTVCT(psip.TableIDExtension()) == version;
    }

    if (TableID::CVCT == table_id)
    {
        return VersionCVCT(psip.TableIDExtension()) == version;
    }

    if (TableID::RRT == table_id)
        return VersionRRT(psip.TableIDExtension()) == version;

    if (TableID::PIM == table_id)
        return true; // ignore these messages..

    if (TableID::PNM == table_id)
        return true; // ignore these messages..

     return false;
}

bool ATSCStreamData::HandleTables(uint pid, const PSIPTable &psip)
{
    if (MPEGStreamData::HandleTables(pid, psip))
        return true;

    if (IsRedundant(pid, psip))
        return true;

    const int version = psip.Version();

    // Decode any table we know about
    switch (psip.TableID())
    {
        case TableID::MGT:
        {
            SetVersionMGT(version);
            if (m_cacheTables)
            {
                auto *mgt = new MasterGuideTable(psip);
                CacheMGT(mgt);
                ProcessMGT(mgt);
            }
            else
            {
                MasterGuideTable mgt(psip);
                ProcessMGT(&mgt);
            }
            return true;
        }
        case TableID::TVCT:
        {
            uint tsid = psip.TableIDExtension();
            SetVersionTVCT(tsid, version);
            if (m_cacheTables)
            {
                auto *vct = new TerrestrialVirtualChannelTable(psip);
                CacheTVCT(pid, vct);
                ProcessTVCT(tsid, vct);
            }
            else
            {
                TerrestrialVirtualChannelTable vct(psip);
                ProcessTVCT(tsid, &vct);
            }
            return true;
        }
        case TableID::CVCT:
        {
            uint tsid = psip.TableIDExtension();
            SetVersionCVCT(tsid, version);
            if (m_cacheTables)
            {
                auto *vct = new CableVirtualChannelTable(psip);
                CacheCVCT(pid, vct);
                ProcessCVCT(tsid, vct);
            }
            else
            {
                CableVirtualChannelTable vct(psip);
                ProcessCVCT(tsid, &vct);
            }
            return true;
        }
        case TableID::RRT:
        {
            uint region = psip.TableIDExtension();
            SetVersionRRT(region, version);
            RatingRegionTable rrt(psip);
            QMutexLocker locker(&m_listenerLock);
            for (auto & listener : m_atscAuxListeners)
                listener->HandleRRT(&rrt);
            return true;
        }
        case TableID::EIT:
        {
            QMutexLocker locker(&m_listenerLock);
            if (m_atscEitListeners.empty() && !m_eitHelper)
                return true;

            uint key = (pid<<16) | psip.TableIDExtension();
            m_eitStatus.SetSectionSeen(key, version, psip.Section(), psip.LastSection());

            EventInformationTable eit(psip);
            for (auto & listener : m_atscEitListeners)
                listener->HandleEIT(pid, &eit);

            const uint mm = GetATSCMajorMinor(eit.SourceID());
            if (mm && m_eitHelper)
                m_eitHelper->AddEIT(mm >> 16, mm & 0xffff, &eit);

            return true;
        }
        case TableID::ETT:
        {
            ExtendedTextTable ett(psip);

            QMutexLocker locker(&m_listenerLock);
            for (auto & listener : m_atscEitListeners)
                listener->HandleETT(pid, &ett);

            if (ett.IsEventETM() && m_eitHelper) // Guide ETTs
            {
                const uint mm = GetATSCMajorMinor(ett.SourceID());
                if (mm)
                    m_eitHelper->AddETT(mm >> 16, mm & 0xffff, &ett);
            }

            return true;
        }
        case TableID::STT:
        {
            SystemTimeTable stt(psip);

            UpdateTimeOffset(stt.UTCUnix());

            // only update internal offset if it changes
            if (stt.GPSOffset() != m_gpsUtcOffset)
                m_gpsUtcOffset = stt.GPSOffset();

            m_listenerLock.lock();
            for (auto & listener : m_atscMainListeners)
                listener->HandleSTT(&stt);
            m_listenerLock.unlock();

            if (m_eitHelper && GPSOffset() != m_eitHelper->GetGPSOffset())
                m_eitHelper->SetGPSOffset(GPSOffset());

            return true;
        }
        case TableID::DCCT:
        {
            DirectedChannelChangeTable dcct(psip);

            QMutexLocker locker(&m_listenerLock);
            for (auto & listener : m_atscAuxListeners)
                listener->HandleDCCT(&dcct);

            return true;
        }
        case TableID::DCCSCT:
        {
            DirectedChannelChangeSelectionCodeTable dccsct(psip);

            QMutexLocker locker(&m_listenerLock);
            for (auto & listener : m_atscAuxListeners)
                listener->HandleDCCSCT(&dccsct);

            return true;
        }

        // ATSC A/81 & SCTE 65 tables
        case TableID::AEIT:
        {
            AggregateEventInformationTable aeit(psip);

            QMutexLocker locker(&m_listenerLock);
            for (auto & listener : m_atsc81EitListeners)
                listener->HandleAEIT(pid, &aeit);

            return true;
        }
        case TableID::AETT:
        {
            AggregateExtendedTextTable aett(psip);

            QMutexLocker locker(&m_listenerLock);
            for (auto & listener : m_atsc81EitListeners)
                listener->HandleAETT(pid, &aett);

            return true;
        }

        // SCTE 65 tables
        case TableID::NITscte:
        {
            SCTENetworkInformationTable nit(psip);

            QMutexLocker locker(&m_listenerLock);
            for (auto & listener : m_scteMainlisteners)
                listener->HandleNIT(&nit);

            return true;
        }
        case TableID::NTT:
        {
            NetworkTextTable ntt(psip);

            QMutexLocker locker(&m_listenerLock);
            for (auto & listener : m_scteMainlisteners)
                listener->HandleNTT(&ntt);

            return true;
        }
        case TableID::SVCTscte:
        {
            ShortVirtualChannelTable svct(psip);

            QMutexLocker locker(&m_listenerLock);
            for (auto & listener : m_scteMainlisteners)
                listener->HandleSVCT(&svct);

            return true;
        }
        case TableID::STTscte:
        {
            SCTESystemTimeTable stt(psip);

            QMutexLocker locker(&m_listenerLock);
            for (auto & listener : m_scteMainlisteners)
                listener->HandleSTT(&stt);

            return true;
        }

        // SCTE 57 table -- SCTE 65 standard supercedes this
        case TableID::PIM:
        {
            ProgramInformationMessageTable pim(psip);

            QMutexLocker locker(&m_listenerLock);
            for (auto & listener : m_scteMainlisteners)
                listener->HandlePIM(&pim);

            return true;
        }
        // SCTE 57 table -- SCTE 65 standard supercedes this
        case TableID::PNM:
        {
            ProgramNameMessageTable pnm(psip);

            QMutexLocker locker(&m_listenerLock);
            for (auto & listener : m_scteMainlisteners)
                listener->HandlePNM(&pnm);

            return true;
        }

        // SCTE 80
        case TableID::ADET:
        {
            AggregateDataEventTable adet(psip);

            QMutexLocker locker(&m_listenerLock);
            for (auto & listener : m_scteMainlisteners)
                listener->HandleADET(&adet);

            return true;
        }

        case TableID::NIT:
        case TableID::NITo:
        case TableID::SDT:
        case TableID::SDTo:
        case TableID::BAT:
        case TableID::TDT:
        case TableID::TOT:
        case TableID::DVBCA_81:
        case TableID::DVBCA_82:
        case TableID::DVBCA_83:
        case TableID::DVBCA_84:
        case TableID::DVBCA_85:
        case TableID::DVBCA_86:
        case TableID::DVBCA_87:
        case TableID::DVBCA_88:
        case TableID::DVBCA_89:
        case TableID::DVBCA_8a:
        case TableID::DVBCA_8b:
        case TableID::DVBCA_8c:
        case TableID::DVBCA_8d:
        case TableID::DVBCA_8e:
        {
            // All DVB specific tables, not handled here
            return false;
        }

        default:
        {
            LOG(VB_RECORD, LOG_ERR, LOC +
                QString("ATSCStreamData::HandleTables(): Unknown table 0x%1 version:%2 pid:%3 0x%4")
                    .arg(psip.TableID(),0,16).arg(psip.Version()).arg(pid).arg(pid,0,16));
            break;
        }
    }
    return false;
}

bool ATSCStreamData::HasEITPIDChanges(const uint_vec_t &in_use_pids) const
{
    QMutexLocker locker(&m_listenerLock);
    uint eit_count = (uint) std::round(m_atscEitPids.size() * m_eitRate);
    uint ett_count = (uint) std::round(m_atscEttPids.size() * m_eitRate);
    return (in_use_pids.size() != (eit_count + ett_count) || m_atscEitReset);
}

bool ATSCStreamData::GetEITPIDChanges(const uint_vec_t &cur_pids,
                                      uint_vec_t &add_pids,
                                      uint_vec_t &del_pids) const
{
    QMutexLocker locker(&m_listenerLock);

    m_atscEitReset = false;

    uint eit_count = (uint) std::round(m_atscEitPids.size() * m_eitRate);
    uint ett_count = (uint) std::round(m_atscEttPids.size() * m_eitRate);

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC + QString("eit size: %1, rate: %2, cnt: %3")
            .arg(m_atscEitPids.size()).arg(m_eitRate).arg(eit_count));
#endif

    uint_vec_t add_pids_tmp;
    atsc_eit_pid_map_t::const_iterator it = m_atscEitPids.begin();
    for (uint i = 0; it != m_atscEitPids.end() && (i < eit_count); (++it),(i++))
        add_pids_tmp.push_back(*it);

    atsc_ett_pid_map_t::const_iterator it2 = m_atscEttPids.begin();
    for (uint i = 0; it2 != m_atscEttPids.end() && (i < ett_count); (++it2),(i++))
        add_pids_tmp.push_back(*it2);

    uint_vec_t::const_iterator it3;
    for (uint pid : cur_pids)
    {
        it3 = find(add_pids_tmp.begin(), add_pids_tmp.end(), pid);
        if (it3 == add_pids_tmp.end())
            del_pids.push_back(pid);
    }

    for (uint pid : add_pids_tmp)
    {
        it3 = find(cur_pids.begin(), cur_pids.end(), pid);
        if (it3 == cur_pids.end())
            add_pids.push_back(pid);
    }

    return !add_pids.empty() || !del_pids.empty();
}

void ATSCStreamData::ProcessMGT(const MasterGuideTable *mgt)
{
    QMutexLocker locker(&m_listenerLock);

    m_atscEitReset = true;
    m_atscEitPids.clear();
    m_atscEttPids.clear();

    for (uint i = 0 ; i < mgt->TableCount(); i++)
    {
        const int table_class = mgt->TableClass(i);
        const uint pid        = mgt->TablePID(i);

        if (table_class == TableClass::EIT)
        {
            const uint num = mgt->TableType(i) - 0x100;
            m_atscEitPids[num] = pid;
        }
        else if (table_class == TableClass::ETTe)
        {
            const uint num = mgt->TableType(i) - 0x200;
            m_atscEttPids[num] = pid;
        }
    }

    for (auto & listener : m_atscMainListeners)
        listener->HandleMGT(mgt);
}

void ATSCStreamData::ProcessVCT(uint tsid, const VirtualChannelTable *vct)
{
    for (auto & listener : m_atscMainListeners)
        listener->HandleVCT(tsid, vct);

    m_sourceIdToAtscMajMin.clear();
    for (uint i = 0; i < vct->ChannelCount() ; i++)
    {
        if (vct->IsHidden(i) && vct->IsHiddenInGuide(i))
        {
            LOG(VB_EIT, LOG_INFO, LOC +
                QString("%1 chan %2-%3 is hidden in guide")
                    .arg(vct->ModulationMode(i) == 1 ? "NTSC" : "ATSC")
                    .arg(vct->MajorChannel(i))
                    .arg(vct->MinorChannel(i)));
            continue;
        }

        if (1 == vct->ModulationMode(i))
        {
            LOG(VB_EIT, LOG_INFO, LOC + QString("Ignoring NTSC chan %1-%2")
                    .arg(vct->MajorChannel(i))
                    .arg(vct->MinorChannel(i)));
            continue;
        }

        LOG(VB_EIT, LOG_INFO, LOC + QString("Adding Source #%1 ATSC chan %2-%3")
                .arg(vct->SourceID(i))
                .arg(vct->MajorChannel(i))
                .arg(vct->MinorChannel(i)));

        m_sourceIdToAtscMajMin[vct->SourceID(i)] =
            vct->MajorChannel(i) << 16 | vct->MinorChannel(i);
    }
}

void ATSCStreamData::ProcessTVCT(uint tsid,
                                 const TerrestrialVirtualChannelTable *vct)
{
    QMutexLocker locker(&m_listenerLock);
    ProcessVCT(tsid, vct);
    for (auto & listener : m_atscAuxListeners)
        listener->HandleTVCT(tsid, vct);
}

void ATSCStreamData::ProcessCVCT(uint tsid,
                                 const CableVirtualChannelTable *vct)
{
    QMutexLocker locker(&m_listenerLock);
    ProcessVCT(tsid, vct);
    for (auto & listener : m_atscAuxListeners)
        listener->HandleCVCT(tsid, vct);
}

bool ATSCStreamData::HasCachedMGT(bool current) const
{
    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    return (bool)(m_cachedMgt);
}

bool ATSCStreamData::HasChannel(uint major, uint minor) const
{
    bool hasit = false;

    {
        tvct_vec_t tvct = GetCachedTVCTs();
        for (size_t i = 0; i < tvct.size() && !hasit; i++)
        {
            if (tvct[i]->Find(major, minor) >= 0)
                hasit |= HasProgram(tvct[i]->ProgramNumber(i));
        }
        ReturnCachedTVCTTables(tvct);
    }

    if (!hasit)
    {
        cvct_vec_t cvct = GetCachedCVCTs();
        for (size_t i = 0; i < cvct.size() && !hasit; i++)
        {
            if (cvct[i]->Find(major, minor) >= 0)
                hasit |= HasProgram(cvct[i]->ProgramNumber(i));
        }
        ReturnCachedCVCTTables(cvct);
    }

    return hasit;
}

bool ATSCStreamData::HasCachedTVCT(uint pid, bool current) const
{
    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    m_cacheLock.lock();
    tvct_cache_t::const_iterator it = m_cachedTvcts.constFind(pid);
    bool exists = (it != m_cachedTvcts.constEnd());
    m_cacheLock.unlock();

    return exists;
}

bool ATSCStreamData::HasCachedCVCT(uint pid, bool current) const
{
    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    m_cacheLock.lock();
    cvct_cache_t::const_iterator it = m_cachedCvcts.constFind(pid);
    bool exists = (it != m_cachedCvcts.constEnd());
    m_cacheLock.unlock();

    return exists;
}

bool ATSCStreamData::HasCachedAllTVCTs(bool current) const
{
    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    if (!m_cachedMgt)
        return false;

    m_cacheLock.lock();
    bool ret = false;
    for (uint i = 0; (i < m_cachedMgt->TableCount()); ++i)
    {
        if (TableClass::TVCTc == m_cachedMgt->TableClass(i))
        {
            if (HasCachedTVCT(m_cachedMgt->TablePID(i)))
            {
                ret = true;
            }
            else
            {
                ret = false;
                break;
            }
        }
    }
    m_cacheLock.unlock();

    return ret;
}

bool ATSCStreamData::HasCachedAllCVCTs(bool current) const
{
    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    if (!m_cachedMgt)
        return false;

    m_cacheLock.lock();
    bool ret = false;
    for (uint i = 0; (i < m_cachedMgt->TableCount()); ++i)
    {
        if (TableClass::CVCTc == m_cachedMgt->TableClass(i))
        {
            if (HasCachedCVCT(m_cachedMgt->TablePID(i)))
            {
                ret = true;
            }
            else
            {
                ret = false;
                break;
            }
        }
    }
    m_cacheLock.unlock();

    return ret;
}

bool ATSCStreamData::HasCachedAnyTVCTs(bool current) const
{
    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    QMutexLocker locker(&m_cacheLock);
    return !m_cachedTvcts.empty();
}

bool ATSCStreamData::HasCachedAnyCVCTs(bool current) const
{
    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    QMutexLocker locker(&m_cacheLock);
    return !m_cachedCvcts.empty();
}

const MasterGuideTable *ATSCStreamData::GetCachedMGT(bool current) const
{
    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    m_cacheLock.lock();
    const MasterGuideTable *mgt = m_cachedMgt;
    IncrementRefCnt(mgt);
    m_cacheLock.unlock();

    return mgt;
}

tvct_const_ptr_t ATSCStreamData::GetCachedTVCT(uint pid, bool current) const
{
    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    tvct_ptr_t tvct = nullptr;

    m_cacheLock.lock();
    tvct_cache_t::const_iterator it = m_cachedTvcts.constFind(pid);
    if (it != m_cachedTvcts.constEnd())
        IncrementRefCnt(tvct = *it);
    m_cacheLock.unlock();

    return tvct;
}

cvct_const_ptr_t ATSCStreamData::GetCachedCVCT(uint pid, bool current) const
{
    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    cvct_ptr_t cvct = nullptr;

    m_cacheLock.lock();
    cvct_cache_t::const_iterator it = m_cachedCvcts.constFind(pid);
    if (it != m_cachedCvcts.constEnd())
        IncrementRefCnt(cvct = *it);
    m_cacheLock.unlock();

    return cvct;
}

tvct_vec_t ATSCStreamData::GetCachedTVCTs(bool current) const
{
    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    std::vector<const TerrestrialVirtualChannelTable*> tvcts;

    m_cacheLock.lock();
    for (auto *tvct : std::as_const(m_cachedTvcts))
    {
        IncrementRefCnt(tvct);
        tvcts.push_back(tvct);
    }
    m_cacheLock.unlock();

    return tvcts;
}

cvct_vec_t ATSCStreamData::GetCachedCVCTs(bool current) const
{
    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    std::vector<const CableVirtualChannelTable*> cvcts;

    m_cacheLock.lock();
    for (auto *cvct : std::as_const(m_cachedCvcts))
    {
        IncrementRefCnt(cvct);
        cvcts.push_back(cvct);
    }
    m_cacheLock.unlock();

    return cvcts;
}

void ATSCStreamData::CacheMGT(MasterGuideTable *mgt)
{
    QMutexLocker locker(&m_cacheLock);

    DeleteCachedTable(m_cachedMgt);
    m_cachedMgt = mgt;
}

void ATSCStreamData::CacheTVCT(uint pid, TerrestrialVirtualChannelTable* tvct)
{
    QMutexLocker locker(&m_cacheLock);

    DeleteCachedTable(m_cachedTvcts[pid]);
    m_cachedTvcts[pid] = tvct;
}

void ATSCStreamData::CacheCVCT(uint pid, CableVirtualChannelTable* cvct)
{
    QMutexLocker locker(&m_cacheLock);

    DeleteCachedTable(m_cachedCvcts[pid]);
    m_cachedCvcts[pid] = cvct;
}

bool ATSCStreamData::DeleteCachedTable(const PSIPTable *psip) const
{
    if (!psip)
        return false;

    QMutexLocker locker(&m_cacheLock);
    if (m_cachedRefCnt[psip] > 0)
    {
        m_cachedSlatedForDeletion[psip] = 1;
        return false;
    }
    if (TableID::MGT == psip->TableID())
    {
        if (psip == m_cachedMgt)
            m_cachedMgt = nullptr;
        delete psip;
    }
    else if ((TableID::TVCT == psip->TableID()) &&
             m_cachedTvcts[psip->tsheader()->PID()])
    {
        m_cachedTvcts[psip->tsheader()->PID()] = nullptr;
        delete psip;
    }
    else if ((TableID::CVCT == psip->TableID()) &&
             m_cachedCvcts[psip->tsheader()->PID()])
    {
        m_cachedCvcts[psip->tsheader()->PID()] = nullptr;
        delete psip;
    }
    else
    {
        return MPEGStreamData::DeleteCachedTable(psip);
    }
    psip_refcnt_map_t::iterator it;
    it = m_cachedSlatedForDeletion.find(psip);
    if (it != m_cachedSlatedForDeletion.end())
        m_cachedSlatedForDeletion.erase(it);

    return true;
}

void ATSCStreamData::ReturnCachedTVCTTables(tvct_vec_t &tvcts) const
{
    for (auto & tvct : tvcts)
        ReturnCachedTable(tvct);
    tvcts.clear();
}

void ATSCStreamData::ReturnCachedCVCTTables(cvct_vec_t &cvcts) const
{
    for (auto & cvct : cvcts)
        ReturnCachedTable(cvct);
    cvcts.clear();
}

void ATSCStreamData::AddATSCMainListener(ATSCMainStreamListener *val)
{
    QMutexLocker locker(&m_listenerLock);

    if (std::any_of(m_atscMainListeners.cbegin(), m_atscMainListeners.cend(),
                    [val](auto & listener){ return val == listener; } ))
        return;
    m_atscMainListeners.push_back(val);
}

void ATSCStreamData::RemoveATSCMainListener(ATSCMainStreamListener *val)
{
    QMutexLocker locker(&m_listenerLock);

    for (auto it = m_atscMainListeners.begin(); it != m_atscMainListeners.end(); ++it)
    {
        if (((void*)val) == ((void*)*it))
        {
            m_atscMainListeners.erase(it);
            return;
        }
    }
}

void ATSCStreamData::AddSCTEMainListener(SCTEMainStreamListener *val)
{
    QMutexLocker locker(&m_listenerLock);

    if (std::any_of(m_scteMainlisteners.cbegin(), m_scteMainlisteners.cend(),
                    [val](auto & listener){ return val == listener; } ))
        return;

    m_scteMainlisteners.push_back(val);
}

void ATSCStreamData::RemoveSCTEMainListener(SCTEMainStreamListener *val)
{
    QMutexLocker locker(&m_listenerLock);

    for (auto it = m_scteMainlisteners.begin(); it != m_scteMainlisteners.end(); ++it)
    {
        if (((void*)val) == ((void*)*it))
        {
            m_scteMainlisteners.erase(it);
            return;
        }
    }
}

void ATSCStreamData::AddATSCAuxListener(ATSCAuxStreamListener *val)
{
    QMutexLocker locker(&m_listenerLock);

    if (std::any_of(m_atscAuxListeners.cbegin(), m_atscAuxListeners.cend(),
                    [val](auto & listener){ return val == listener; } ))
        return;

    m_atscAuxListeners.push_back(val);
}

void ATSCStreamData::RemoveATSCAuxListener(ATSCAuxStreamListener *val)
{
    QMutexLocker locker(&m_listenerLock);

    for (auto it = m_atscAuxListeners.begin(); it != m_atscAuxListeners.end(); ++it)
    {
        if (((void*)val) == ((void*)*it))
        {
            m_atscAuxListeners.erase(it);
            return;
        }
    }
}

void ATSCStreamData::AddATSCEITListener(ATSCEITStreamListener *val)
{
    QMutexLocker locker(&m_listenerLock);

    if (std::any_of(m_atscEitListeners.cbegin(), m_atscEitListeners.cend(),
                    [val](auto & listener){ return val == listener; } ))
        return;

    m_atscEitListeners.push_back(val);
}

void ATSCStreamData::RemoveATSCEITListener(ATSCEITStreamListener *val)
{
    QMutexLocker locker(&m_listenerLock);

    for (auto it = m_atscEitListeners.begin(); it != m_atscEitListeners.end(); ++it)
    {
        if (((void*)val) == ((void*)*it))
        {
            m_atscEitListeners.erase(it);
            return;
        }
    }
}

void ATSCStreamData::AddATSC81EITListener(ATSC81EITStreamListener *val)
{
    QMutexLocker locker(&m_listenerLock);

    if (std::any_of(m_atsc81EitListeners.cbegin(), m_atsc81EitListeners.cend(),
                    [val](auto & listener){ return val == listener; } ))
        return;

    m_atsc81EitListeners.push_back(val);
}

void ATSCStreamData::RemoveATSC81EITListener(ATSC81EITStreamListener *val)
{
    QMutexLocker locker(&m_listenerLock);

    for (auto it = m_atsc81EitListeners.begin(); it != m_atsc81EitListeners.end(); ++it)
    {
        if (((void*)val) == ((void*)*it))
        {
            m_atsc81EitListeners.erase(it);
            return;
        }
    }
}
