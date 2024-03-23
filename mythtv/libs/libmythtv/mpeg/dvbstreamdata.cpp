// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include <algorithm>

#include <QSharedPointer>
#include "dvbstreamdata.h"
#include "dvbtables.h"
#include "mpegdescriptors.h"
#include "mpegtables.h"
#include "premieretables.h"
#include "eithelper.h"

static constexpr uint8_t MCA_EIT_TSID { 136 };

#define LOC QString("DVBStream[%1]: ").arg(m_cardId)

// service_id is synonymous with the MPEG program number in the PMT.
DVBStreamData::DVBStreamData(uint desired_netid,  uint desired_tsid,
                             int desired_program, int cardnum, bool cacheTables)
    : MPEGStreamData(desired_program, cardnum, cacheTables),
      m_desiredNetId(desired_netid), m_desiredTsId(desired_tsid)
{
    m_nitStatus.SetVersion(-1,0);
    m_nitoStatus.SetVersion(-1,0);
    AddListeningPID(PID::DVB_NIT_PID);
    AddListeningPID(PID::DVB_SDT_PID);
    AddListeningPID(PID::DVB_TDT_PID);
}

DVBStreamData::~DVBStreamData()
{
    DVBStreamData::Reset(m_desiredNetId, m_desiredTsId, m_desiredProgram);

    QMutexLocker locker(&m_listenerLock);
    m_dvbMainListeners.clear();
    m_dvbOtherListeners.clear();
    m_dvbEitListeners.clear();
    m_dvbHasEit.clear();
}

void DVBStreamData::SetDesiredService(uint netid, uint tsid, int serviceid)
{
    bool reset = true;

    if (HasCachedAllSDT(tsid, true))
    {
        sdt_const_ptr_t first_sdt = GetCachedSDT(tsid, 0, true);
        uint networkID = first_sdt->OriginalNetworkID();
        if (networkID == netid)
        {
            reset = false;
            m_desiredNetId = netid;
            m_desiredTsId = tsid;
            uint last_section = first_sdt->LastSection();
            ProcessSDT(m_desiredTsId, first_sdt);
            ReturnCachedTable(first_sdt);
            for (uint i = 1; i <= last_section; ++i)
            {
                sdt_const_ptr_t sdt = GetCachedSDT(m_desiredTsId, i, true);
                ProcessSDT(m_desiredTsId, sdt);
                ReturnCachedTable(sdt);
            }
            SetDesiredProgram(serviceid);
        }
    }

    if (reset)
        Reset(netid, tsid, serviceid);
}


/** \fn DVBStreamData::IsRedundant(uint,const PSIPTable&) const
 *  \brief Returns true if table already seen.
 *  \todo This is just a stub.
 */
bool DVBStreamData::IsRedundant(uint pid, const PSIPTable &psip) const
{
    if (MPEGStreamData::IsRedundant(pid, psip))
        return true;

    const int table_id = psip.TableID();
    const int version  = psip.Version();

    if (TableID::NIT == table_id)
    {
        return m_nitStatus.IsSectionSeen(version, psip.Section());
    }

    if (TableID::SDT == table_id)
    {
        return m_sdtStatus.IsSectionSeen(psip.TableIDExtension(), version, psip.Section());
    }

    if (TableID::TDT == table_id)
        return false;

    if (TableID::BAT == table_id)
    {
        return m_batStatus.IsSectionSeen(psip.TableIDExtension(), version, psip.Section());
    }

    bool is_eit = false;
    if (PID::DVB_EIT_PID == pid || PID::FREESAT_EIT_PID == pid)
    {
        // Standard Now/Next Event Information Tables for this transport
        is_eit |= TableID::PF_EIT  == table_id;
        // Standard Future Event Information Tables for this transport
        is_eit |= (TableID::SC_EITbeg  <= table_id &&
                   TableID::SC_EITend  >= table_id);
    }
    if (is_eit)
    {
        uint service_id = psip.TableIDExtension();
        uint key = (table_id<<16) | service_id;
        return m_eitStatus.IsSectionSeen(key, version, psip.Section());
    }

    ////////////////////////////////////////////////////////////////////////
    // Other transport tables

    if (TableID::NITo == table_id)
    {
        return m_nitoStatus.IsSectionSeen(version, psip.Section());
    }

    if (TableID::SDTo == table_id)
    {
        return m_sdtoStatus.IsSectionSeen(psip.TableIDExtension(), version, psip.Section());
    }

    if (PID::DVB_EIT_PID == pid || PID::FREESAT_EIT_PID == pid || PID::MCA_EIT_PID == pid)
    {
        // Standard Now/Next Event Information Tables for other transport
        is_eit |= TableID::PF_EITo == table_id;
        // Standard Future Event Information Tables for other transports
        is_eit |= (TableID::SC_EITbego <= table_id &&
                   TableID::SC_EITendo >= table_id);
    }
    if (PID::DVB_DNLONG_EIT_PID == pid || PID::DVB_BVLONG_EIT_PID == pid)
    {
        // Dish Network and Bev Long Term Future Event Information
        // for all transports
        is_eit |= (TableID::DN_EITbego <= table_id &&
                   TableID::DN_EITendo >= table_id);
    }
    if (is_eit)
    {
        uint service_id = psip.TableIDExtension();
        uint key = (table_id<<16) | service_id;
        return m_eitStatus.IsSectionSeen(key, version, psip.Section());
    }

    if (((PID::PREMIERE_EIT_DIREKT_PID == pid) || (PID::PREMIERE_EIT_SPORT_PID == pid)) &&
        TableID::PREMIERE_CIT == table_id)
    {
        uint content_id = PremiereContentInformationTable(psip).ContentID();
        return m_citStatus.IsSectionSeen(content_id, version, psip.Section());
    }

    return false;
}

void DVBStreamData::Reset(uint desired_netid, uint desired_tsid,
                          int desired_serviceid)
{
    MPEGStreamData::Reset(desired_serviceid);

    m_desiredNetId = desired_netid;
    m_desiredTsId  = desired_tsid;

    m_nitStatus.SetVersion(-1,0);
    m_sdtStatus.clear();
    m_eitStatus.clear();
    m_citStatus.clear();

    m_nitoStatus.SetVersion(-1,0);
    m_sdtoStatus.clear();
    m_batStatus.clear();

    {
        m_cacheLock.lock();

        for (const auto & nit : std::as_const(m_cachedNit))
            DeleteCachedTable(nit);
        m_cachedNit.clear();

        for (const auto & cached : std::as_const(m_cachedSdts))
            DeleteCachedTable(cached);
        m_cachedSdts.clear();

        for (const auto & cached : std::as_const(m_cachedBats))
            DeleteCachedTable(cached);
        m_cachedBats.clear();

        m_cacheLock.unlock();
    }
    AddListeningPID(PID::DVB_NIT_PID);
    AddListeningPID(PID::DVB_SDT_PID);
    AddListeningPID(PID::DVB_TDT_PID);
}

/** \fn DVBStreamData::HandleTables(uint pid, const PSIPTable&)
 *  \brief Process PSIP packets.
 */
bool DVBStreamData::HandleTables(uint pid, const PSIPTable &psip)
{
    if (MPEGStreamData::HandleTables(pid, psip))
        return true;

    // If the user specified a network ID and that network ID is a NITo then change that NITo into
    // the NIT and change the NIT into a NITo. See ticket #7486.
    if (m_dvbRealNetworkId > 0)
    {
        if ((psip.TableID() == TableID::NIT  && psip.TableIDExtension() != (uint)m_dvbRealNetworkId) ||
            (psip.TableID() == TableID::NITo && psip.TableIDExtension() == (uint)m_dvbRealNetworkId)  )
        {
            auto *nit = new NetworkInformationTable(psip);
            if (!nit->Mutate())
            {
                delete nit;
                return true;
            }
            bool retval = HandleTables(pid, *nit);
            delete nit;
            return retval;
        }
    }

    if (IsRedundant(pid, psip))
        return true;

    switch (psip.TableID())
    {
        case TableID::NIT:
        {
            m_nitStatus.SetSectionSeen(psip.Version(), psip.Section(),
                                       psip.LastSection());

            if (m_cacheTables)
            {
                auto *nit = new NetworkInformationTable(psip);
                CacheNIT(nit);
                QMutexLocker locker(&m_listenerLock);
                for (auto & listener : m_dvbMainListeners)
                    listener->HandleNIT(nit);
            }
            else
            {
                NetworkInformationTable nit(psip);
                QMutexLocker locker(&m_listenerLock);
                for (auto & listener : m_dvbMainListeners)
                    listener->HandleNIT(&nit);
            }

            return true;
        }
        case TableID::SDT:
        {
            uint tsid = psip.TableIDExtension();
            m_sdtStatus.SetSectionSeen(tsid, psip.Version(), psip.Section(),
                                        psip.LastSection());

            if (m_cacheTables)
            {
                auto *sdt = new ServiceDescriptionTable(psip);
                CacheSDT(sdt);
                ProcessSDT(tsid, sdt);
            }
            else
            {
                ServiceDescriptionTable sdt(psip);
                ProcessSDT(tsid, &sdt);
            }

            return true;
        }
        case TableID::TDT:
        {
            TimeDateTable tdt(psip);

            UpdateTimeOffset(tdt.UTCUnix());

            QMutexLocker locker(&m_listenerLock);
            for (auto & listener : m_dvbMainListeners)
                listener->HandleTDT(&tdt);

            return true;
        }
        case TableID::NITo:
        {
            m_nitoStatus.SetSectionSeen(psip.Version(), psip.Section(),
                                        psip.LastSection());
            NetworkInformationTable nit(psip);

            QMutexLocker locker(&m_listenerLock);
            for (auto & listener : m_dvbOtherListeners)
                listener->HandleNITo(&nit);

            return true;
        }
        case TableID::SDTo:
        {
            uint tsid = psip.TableIDExtension();
            m_sdtoStatus.SetSectionSeen(tsid, psip.Version(), psip.Section(),
                                        psip.LastSection());
            ServiceDescriptionTable sdt(psip);

            // some providers send the SDT for the current multiplex as SDTo
            // this routine changes the TableID to SDT and recalculates the CRC
            if (m_desiredNetId == sdt.OriginalNetworkID() &&
                m_desiredTsId  == tsid)
            {
                auto *sdta = new ServiceDescriptionTable(psip);
                if (!sdta->Mutate())
                {
                    delete sdta;
                    return true;
                }
                if (m_cacheTables)
                {
                    CacheSDT(sdta);
                    ProcessSDT(tsid, sdta);
                }
                else
                {
                    ProcessSDT(tsid, sdta);
                    delete sdta;
                }
                return true;
            }

            QMutexLocker locker(&m_listenerLock);
            for (auto & listener : m_dvbOtherListeners)
                listener->HandleSDTo(tsid, &sdt);

            return true;
        }
        case TableID::BAT:
        {
            uint bouquet_id = psip.TableIDExtension();
            m_batStatus.SetSectionSeen(bouquet_id, psip.Version(), psip.Section(),
                                       psip.LastSection());

            if (m_cacheTables)
            {
                auto *bat = new BouquetAssociationTable(psip);
                CacheBAT(bat);
                QMutexLocker locker(&m_listenerLock);
                for (auto & listener : m_dvbOtherListeners)
                    listener->HandleBAT(bat);
            }
            else
            {
                BouquetAssociationTable bat(psip);
                QMutexLocker locker(&m_listenerLock);
                for (auto & listener : m_dvbOtherListeners)
                    listener->HandleBAT(&bat);
            }

            return true;
        }
    }

    if ((PID::DVB_EIT_PID == pid || PID::DVB_DNLONG_EIT_PID == pid || PID::FREESAT_EIT_PID == pid ||
        ((OriginalNetworkID::MCA == m_desiredNetId) && (MCA_EIT_TSID == m_desiredTsId) &&
        (PID::MCA_EIT_PID == pid)) || PID::DVB_BVLONG_EIT_PID == pid) &&

        DVBEventInformationTable::IsEIT(psip.TableID()))
    {
        QMutexLocker locker(&m_listenerLock);
        if (m_dvbEitListeners.empty() && !m_eitHelper)
            return true;

        uint service_id = psip.TableIDExtension();
        uint key = (psip.TableID()<<16) | service_id;
        m_eitStatus.SetSectionSeen(key, psip.Version(), psip.Section(),
                                    psip.LastSection());

        DVBEventInformationTable eit(psip);
        for (auto & listener : m_dvbEitListeners)
            listener->HandleEIT(&eit);

        if (m_eitHelper)
            m_eitHelper->AddEIT(&eit);

        return true;
    }

    if (m_desiredNetId == OriginalNetworkID::PREMIERE &&
        (PID::PREMIERE_EIT_DIREKT_PID == pid || PID::PREMIERE_EIT_SPORT_PID == pid) &&
        PremiereContentInformationTable::IsEIT(psip.TableID()))
    {
        QMutexLocker locker(&m_listenerLock);
        if (m_dvbEitListeners.empty() && !m_eitHelper)
            return true;

        PremiereContentInformationTable cit(psip);
        m_citStatus.SetSectionSeen(cit.ContentID(), psip.Version(), psip.Section(), psip.LastSection());

        for (auto & listener : m_dvbEitListeners)
            listener->HandleEIT(&cit);

        if (m_eitHelper)
            m_eitHelper->AddEIT(&cit);

        return true;
    }

    return false;
}

void DVBStreamData::ProcessSDT(uint tsid, const ServiceDescriptionTable *sdt)
{
    QMutexLocker locker(&m_listenerLock);

    for (uint i = 0; i < sdt->ServiceCount(); i++)
    {
        /*
         * FIXME always signal EIT presence. We filter later. To many
         * networks set these flags wrong.
         * This allows the user to simply set useonairguide on a
         * channel manually.
         */
#if 0
        if (sdt->HasEITSchedule(i) || sdt->HasEITPresentFollowing(i))
#endif
            m_dvbHasEit[sdt->ServiceID(i)] = true;
    }

    for (auto & listener : m_dvbMainListeners)
        listener->HandleSDT(tsid, sdt);
}

bool DVBStreamData::HasEITPIDChanges(const uint_vec_t &in_use_pids) const
{
    QMutexLocker locker(&m_listenerLock);
    bool want_eit = (m_eitRate >= 0.5F) && HasAnyEIT();
    bool has_eit  = !in_use_pids.empty();
    return want_eit != has_eit;
}

bool DVBStreamData::GetEITPIDChanges(const uint_vec_t &cur_pids,
                                     uint_vec_t &add_pids,
                                     uint_vec_t &del_pids) const
{
    QMutexLocker locker(&m_listenerLock);

    if ((m_eitRate >= 0.5F) && HasAnyEIT())
    {
        if (find(cur_pids.begin(), cur_pids.end(),
                 (uint) PID::DVB_EIT_PID) == cur_pids.end())
        {
            add_pids.push_back(PID::DVB_EIT_PID);
        }

        if (m_dvbEitDishnetLong &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) PID::DVB_DNLONG_EIT_PID) == cur_pids.end())
        {
            add_pids.push_back(PID::DVB_DNLONG_EIT_PID);
        }

        if (m_dvbEitDishnetLong &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) PID::DVB_BVLONG_EIT_PID) == cur_pids.end())
        {
            add_pids.push_back(PID::DVB_BVLONG_EIT_PID);
        }

#if 0
        // OpenTV EIT pids
        if (m_dvbEitDishnetLong)
        {
            for (uint pid = PID::OTV_EIT_TIT_PID_START; pid <= PID::OTV_EIT_TIT_PID_END; pid++)
            {
                if (find(cur_pids.begin(), cur_pids.end(),
                         pid) == cur_pids.end())
                    add_pids.push_back(pid);
            }
            for (uint pid = PID::OTV_EIT_SUP_PID_START; pid <= PID::OTV_EIT_SUP_PID_END; pid++)
            {
                if (find(cur_pids.begin(), cur_pids.end(),
                         pid) == cur_pids.end())
                    add_pids.push_back(pid);
            }
        }
#endif

        if (m_desiredNetId == OriginalNetworkID::PREMIERE &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) PID::PREMIERE_EIT_DIREKT_PID) == cur_pids.end())
        {
            add_pids.push_back(PID::PREMIERE_EIT_DIREKT_PID);
        }

        if (m_desiredNetId == OriginalNetworkID::PREMIERE &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) PID::PREMIERE_EIT_SPORT_PID) == cur_pids.end())
        {
            add_pids.push_back(PID::PREMIERE_EIT_SPORT_PID);
        }

        if (m_desiredNetId == OriginalNetworkID::SES2 &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) PID::FREESAT_EIT_PID) == cur_pids.end())
        {
            add_pids.push_back(PID::FREESAT_EIT_PID);
        }

        if (OriginalNetworkID::MCA == m_desiredNetId && MCA_EIT_TSID == m_desiredTsId &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) PID::MCA_EIT_PID) == cur_pids.end())
        {
            add_pids.push_back(PID::MCA_EIT_PID);
        }

    }
    else
    {
        if (find(cur_pids.begin(), cur_pids.end(),
                 (uint) PID::DVB_EIT_PID) != cur_pids.end())
        {
            del_pids.push_back(PID::DVB_EIT_PID);
        }

        if (m_dvbEitDishnetLong &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) PID::DVB_DNLONG_EIT_PID) != cur_pids.end())
        {
            del_pids.push_back(PID::DVB_DNLONG_EIT_PID);
        }

        if (m_dvbEitDishnetLong &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) PID::DVB_BVLONG_EIT_PID) != cur_pids.end())
        {
            del_pids.push_back(PID::DVB_BVLONG_EIT_PID);
        }

#if 0
        // OpenTV EIT pids
        if (m_dvbEitDishnetLong)
        {
            for (uint pid = PID::OTV_EIT_TIT_PID_START; pid <= PID::OTV_EIT_TIT_PID_END; pid++)
            {
                if (find(cur_pids.begin(), cur_pids.end(),
                         pid) != cur_pids.end())
                    del_pids.push_back(pid);
            }
            for(uint pid=PID::OTV_EIT_SUP_PID_START; pid <= PID::OTV_EIT_SUP_PID_END; pid++)
            {
                if (find(cur_pids.begin(), cur_pids.end(),
                         pid) != cur_pids.end())
                    del_pids.push_back(pid);
            }
        }
#endif

        if (m_desiredNetId == OriginalNetworkID::PREMIERE &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) PID::PREMIERE_EIT_DIREKT_PID) != cur_pids.end())
        {
            del_pids.push_back(PID::PREMIERE_EIT_DIREKT_PID);
        }

        if (m_desiredNetId == OriginalNetworkID::PREMIERE &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) PID::PREMIERE_EIT_SPORT_PID) != cur_pids.end())
        {
            del_pids.push_back(PID::PREMIERE_EIT_SPORT_PID);
        }

        if (m_desiredNetId == OriginalNetworkID::SES2 &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) PID::FREESAT_EIT_PID) != cur_pids.end())
        {
            del_pids.push_back(PID::FREESAT_EIT_PID);
        }

        if (OriginalNetworkID::MCA == m_desiredNetId && MCA_EIT_TSID == m_desiredTsId &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) PID::MCA_EIT_PID) != cur_pids.end())
        {
            del_pids.push_back(PID::MCA_EIT_PID);
        }
    }

    return !add_pids.empty() || !del_pids.empty();
}

bool DVBStreamData::HasAllNITSections(void) const
{
    return m_nitStatus.HasAllSections();
}

bool DVBStreamData::HasAllNIToSections(void) const
{
    return m_nitoStatus.HasAllSections();
}

bool DVBStreamData::HasAllSDTSections(uint tsid) const
{
    return m_sdtStatus.HasAllSections(tsid);
}

bool DVBStreamData::HasAllSDToSections(uint tsid) const
{
    return m_sdtoStatus.HasAllSections(tsid);
}

bool DVBStreamData::HasAllBATSections(uint bid) const
{
    return m_batStatus.HasAllSections(bid);
}

bool DVBStreamData::HasCachedAnyNIT(bool current) const
{
    QMutexLocker locker(&m_cacheLock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    return !m_cachedNit.empty();
}

bool DVBStreamData::HasCachedAllNIT(bool current) const
{
    QMutexLocker locker(&m_cacheLock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    if (m_cachedNit.empty())
        return false;

    uint last_section = (*m_cachedNit.begin())->LastSection();
    if (!last_section)
        return true;

    for (uint i = 0; i <= last_section; i++)
        if (m_cachedNit.find(i) == m_cachedNit.end())
            return false;

    return true;
}

bool DVBStreamData::HasCachedAnyBAT(uint batid, bool current) const
{
    QMutexLocker locker(&m_cacheLock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    for (uint i = 0; i <= 255; i++)
        if (m_cachedBats.find((batid << 8) | i) != m_cachedBats.end())
            return true;

    return false;
}

bool DVBStreamData::HasCachedAllBAT(uint batid, bool current) const
{
    QMutexLocker locker(&m_cacheLock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    bat_cache_t::const_iterator it = m_cachedBats.constFind(batid << 8);
    if (it == m_cachedBats.constEnd())
        return false;

    uint last_section = (*it)->LastSection();
    if (!last_section)
        return true;

    for (uint i = 1; i <= last_section; i++)
        if (m_cachedBats.constFind((batid << 8) | i) == m_cachedBats.constEnd())
            return false;

    return true;
}

bool DVBStreamData::HasCachedAnyBATs(bool /*current*/) const
{
    QMutexLocker locker(&m_cacheLock);
    return !m_cachedBats.empty();
}

bool DVBStreamData::HasCachedAllBATs(bool current) const
{
    QMutexLocker locker(&m_cacheLock);

    if (m_cachedBats.empty())
        return false;

    for (auto it = m_cachedBats.cbegin(); it != m_cachedBats.cend(); ++it)
    {
        if (!HasCachedAllBAT(it.key() >> 8, current))
            return false;
    }

    return true;
}

bool DVBStreamData::HasCachedAllSDT(uint tsid, bool current) const
{
    QMutexLocker locker(&m_cacheLock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    sdt_cache_t::const_iterator it = m_cachedSdts.constFind(tsid << 8);
    if (it == m_cachedSdts.constEnd())
        return false;

    uint last_section = (*it)->LastSection();
    if (!last_section)
        return true;

    for (uint i = 1; i <= last_section; i++)
        if (m_cachedSdts.constFind((tsid << 8) | i) == m_cachedSdts.constEnd())
            return false;

    return true;
}

bool DVBStreamData::HasCachedAnySDT(uint tsid, bool current) const
{
    QMutexLocker locker(&m_cacheLock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    for (uint i = 0; i <= 255; i++)
        if (m_cachedSdts.find((tsid << 8) | i) != m_cachedSdts.end())
            return true;

    return false;
}

bool DVBStreamData::HasCachedSDT(bool current) const
{
    QMutexLocker locker(&m_cacheLock);

    if (m_cachedNit.empty())
        return false;

    for (auto *nit : std::as_const(m_cachedNit))
    {
        for (uint i = 0; i < nit->TransportStreamCount(); i++)
        {
            if (HasCachedAllSDT(nit->TSID(i), current))
                return true;
        }
    }

    return false;
}

bool DVBStreamData::HasCachedAnySDTs(bool /*current*/) const
{
    QMutexLocker locker(&m_cacheLock);
    return !m_cachedSdts.empty();
}

bool DVBStreamData::HasCachedAllSDTs(bool current) const
{
    QMutexLocker locker(&m_cacheLock);

    if (m_cachedNit.empty())
        return false;

    for (auto *nit : std::as_const(m_cachedNit))
    {
        if ((int)nit->TransportStreamCount() > m_cachedSdts.size())
            return false;

        for (uint i = 0; i < nit->TransportStreamCount(); i++)
            if (!HasCachedAllSDT(nit->TSID(i), current))
                return false;
    }

    return true;
}

nit_const_ptr_t DVBStreamData::GetCachedNIT(
    uint section_num, bool current) const
{
    QMutexLocker locker(&m_cacheLock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    nit_ptr_t nit = nullptr;

    nit_cache_t::const_iterator it = m_cachedNit.constFind(section_num);
    if (it != m_cachedNit.constEnd())
        IncrementRefCnt(nit = *it);

    return nit;
}

nit_vec_t DVBStreamData::GetCachedNIT(bool current) const
{
    QMutexLocker locker(&m_cacheLock);

    nit_vec_t nits;

    for (uint i = 0; i < 256; i++)
    {
        nit_const_ptr_t nit = GetCachedNIT(i, current);
        if (nit)
            nits.push_back(nit);
    }

    return nits;
}

bat_const_ptr_t DVBStreamData::GetCachedBAT(
    uint batid, uint section_num, bool current) const
{
    QMutexLocker locker(&m_cacheLock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    bat_ptr_t bat = nullptr;

    uint key = (batid << 8) | section_num;
    bat_cache_t::const_iterator it = m_cachedBats.constFind(key);
    if (it != m_cachedBats.constEnd())
        IncrementRefCnt(bat = *it);

    return bat;
}

bat_vec_t DVBStreamData::GetCachedBATs(bool current) const
{
    QMutexLocker locker(&m_cacheLock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    bat_vec_t bats;

    for (auto *bat : std::as_const(m_cachedBats))
    {
        IncrementRefCnt(bat);
        bats.push_back(bat);
    }

    return bats;
}

sdt_const_ptr_t DVBStreamData::GetCachedSDT(
    uint tsid, uint section_num, bool current) const
{
    QMutexLocker locker(&m_cacheLock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    sdt_ptr_t sdt = nullptr;

    uint key = (tsid << 8) | section_num;
    sdt_cache_t::const_iterator it = m_cachedSdts.constFind(key);
    if (it != m_cachedSdts.constEnd())
        IncrementRefCnt(sdt = *it);

    return sdt;
}

sdt_vec_t DVBStreamData::GetCachedSDTSections(uint tsid, bool current) const
{
    QMutexLocker locker(&m_cacheLock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    sdt_vec_t sdts;
    sdt_const_ptr_t sdt = GetCachedSDT(tsid, 0);

    if (sdt)
    {
        uint lastSection = sdt->LastSection();

        sdts.push_back(sdt);

        for (uint section = 1; section <= lastSection; section++)
        {
            sdt = GetCachedSDT(tsid, section);

            if (sdt)
                sdts.push_back(sdt);
        }
    }

    return sdts;
}

sdt_vec_t DVBStreamData::GetCachedSDTs(bool current) const
{
    QMutexLocker locker(&m_cacheLock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    sdt_vec_t sdts;

    for (auto *sdt : std::as_const(m_cachedSdts))
    {
        IncrementRefCnt(sdt);
        sdts.push_back(sdt);
    }

    return sdts;
}

void DVBStreamData::ReturnCachedSDTTables(sdt_vec_t &sdts) const
{
    for (auto & sdt : sdts)
        ReturnCachedTable(sdt);
    sdts.clear();
}

bool DVBStreamData::DeleteCachedTable(const PSIPTable *psip) const
{
    if (!psip)
        return false;

    uint tid = psip->TableIDExtension();    // For SDTs
    uint bid = psip->TableIDExtension();    // For BATs

    QMutexLocker locker(&m_cacheLock);
    if (m_cachedRefCnt[psip] > 0)
    {
        m_cachedSlatedForDeletion[psip] = 1;
        return false;
    }
    if ((TableID::NIT == psip->TableID()) &&
             m_cachedNit[psip->Section()])
    {
        m_cachedNit[psip->Section()] = nullptr;
        delete psip;
    }
    else if ((TableID::SDT == psip->TableID()) &&
             m_cachedSdts[tid << 8 | psip->Section()])
    {
        m_cachedSdts[tid << 8 | psip->Section()] = nullptr;
        delete psip;
    }
    else if ((TableID::BAT == psip->TableID()) &&
             m_cachedBats[bid << 8 | psip->Section()])
    {
        m_cachedBats[bid << 8 | psip->Section()] = nullptr;
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

void DVBStreamData::CacheNIT(NetworkInformationTable *nit)
{
    QMutexLocker locker(&m_cacheLock);

    nit_cache_t::iterator it = m_cachedNit.find(nit->Section());
    if (it != m_cachedNit.end())
        DeleteCachedTable(*it);

    m_cachedNit[nit->Section()] = nit;
}

void DVBStreamData::CacheBAT(BouquetAssociationTable *bat)
{
    uint key = (bat->BouquetID() << 8) | bat->Section();

    QMutexLocker locker(&m_cacheLock);

    bat_cache_t::iterator it = m_cachedBats.find(key);
    if (it != m_cachedBats.end())
        DeleteCachedTable(*it);

    m_cachedBats[key] = bat;
}

void DVBStreamData::CacheSDT(ServiceDescriptionTable *sdt)
{
    uint key = (sdt->TSID() << 8) | sdt->Section();

    QMutexLocker locker(&m_cacheLock);

    sdt_cache_t::iterator it = m_cachedSdts.find(key);
    if (it != m_cachedSdts.end())
        DeleteCachedTable(*it);

    m_cachedSdts[key] = sdt;
}

void DVBStreamData::AddDVBMainListener(DVBMainStreamListener *val)
{
    QMutexLocker locker(&m_listenerLock);

    if (std::any_of(m_dvbMainListeners.cbegin(), m_dvbMainListeners.cend(),
                    [val](auto & listener){ return val == listener; } ))
        return;

    m_dvbMainListeners.push_back(val);
}

void DVBStreamData::RemoveDVBMainListener(DVBMainStreamListener *val)
{
    QMutexLocker locker(&m_listenerLock);

    for (auto it = m_dvbMainListeners.begin(); it != m_dvbMainListeners.end(); ++it)
    {
        if (((void*)val) == ((void*)*it))
        {
            m_dvbMainListeners.erase(it);
            return;
        }
    }
}

void DVBStreamData::AddDVBOtherListener(DVBOtherStreamListener *val)
{
    QMutexLocker locker(&m_listenerLock);

    if (std::any_of(m_dvbOtherListeners.cbegin(), m_dvbOtherListeners.cend(),
                    [val](auto & listener){ return val == listener; } ))
        return;

    m_dvbOtherListeners.push_back(val);
}

void DVBStreamData::RemoveDVBOtherListener(DVBOtherStreamListener *val)
{
    QMutexLocker locker(&m_listenerLock);

    for (auto it = m_dvbOtherListeners.begin(); it != m_dvbOtherListeners.end(); ++it)
    {
        if (((void*)val) == ((void*)*it))
        {
            m_dvbOtherListeners.erase(it);
            return;
        }
    }
}

void DVBStreamData::AddDVBEITListener(DVBEITStreamListener *val)
{
    QMutexLocker locker(&m_listenerLock);

    if (std::any_of(m_dvbEitListeners.cbegin(), m_dvbEitListeners.cend(),
                    [val](auto & listener){ return val == listener; } ))
        return;

    m_dvbEitListeners.push_back(val);
}

void DVBStreamData::RemoveDVBEITListener(DVBEITStreamListener *val)
{
    QMutexLocker locker(&m_listenerLock);

    for (auto it = m_dvbEitListeners.begin(); it != m_dvbEitListeners.end(); ++it)
    {
        if (((void*)val) == ((void*)*it))
        {
            m_dvbEitListeners.erase(it);
            return;
        }
    }
}
