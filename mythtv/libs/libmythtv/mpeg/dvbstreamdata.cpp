// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "dvbstreamdata.h"
#include "dvbtables.h"

DVBStreamData::DVBStreamData(bool cacheTables)
    : MPEGStreamData(-1, cacheTables),
      _nit_version(-2), _nito_version(-2)
{
    setName("DVBStreamData");
    SetVersionNIT(-1,0);
    SetVersionNITo(-1,0);
    AddListeningPID(DVB_NIT_PID);
    AddListeningPID(DVB_SDT_PID);
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
        if (VersionNIT() != version)
            return false;
        return NITSectionSeen(psip.Section());
    }

    if (TableID::SDT == table_id)
    {
        if (VersionSDT(psip.TableIDExtension()) != version)
            return false;
        return SDTSectionSeen(psip.TableIDExtension(), psip.Section());
    }

    bool is_eit = false;
    if (DVB_EIT_PID == pid)
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
        if (VersionEIT(table_id, service_id) != version)
            return false;
        return EITSectionSeen(table_id, service_id, psip.Section());
    }

    ////////////////////////////////////////////////////////////////////////
    // Other transport tables

    if (TableID::NITo == table_id)
    {
        if (VersionNITo() != version)
            return false;
        return NIToSectionSeen(psip.Section());
    }

    if (TableID::SDTo == table_id)
    {
        if (VersionSDTo(psip.TableIDExtension()) != version)
            return false;
        return SDToSectionSeen(psip.TableIDExtension(), psip.Section());
    }

    if (DVB_EIT_PID == pid)
    {
        // Standard Now/Next Event Information Tables for other transport
        is_eit |= TableID::PF_EITo == table_id;
        // Standard Future Event Information Tables for other transports
        is_eit |= (TableID::SC_EITbego <= table_id &&
                   TableID::SC_EITendo >= table_id);
    }
    if (DVB_DNLONG_EIT_PID == pid)
    {
        // Dish Network Long Term Future Event Information for all transports
        is_eit |= (TableID::DN_EITbego <= table_id &&
                   TableID::DN_EITendo >= table_id);
    }
    if (is_eit)
    {
        uint service_id = psip.TableIDExtension();
        if (VersionEIT(table_id, service_id) != version)
            return false;
        return EITSectionSeen(table_id, service_id, psip.Section());
    }

    return false;
}

void DVBStreamData::Reset(void)
{
    MPEGStreamData::Reset(-1);

    SetVersionNIT(-1,0);
    _sdt_versions.clear();
    _sdt_section_seen.clear();
    _eit_version.clear();
    _eit_section_seen.clear();

    SetVersionNITo(-1,0);
    _sdto_versions.clear();
    _sdto_section_seen.clear();

    {
        _cache_lock.lock();

        nit_cache_t::iterator nit = _cached_nit.begin();
        for (; nit != _cached_nit.end(); ++nit)
            DeleteCachedTable(*nit);
        _cached_nit.clear();

        sdt_cache_t::iterator sit = _cached_sdts.begin();
        for (; sit != _cached_sdts.end(); ++sit)
            DeleteCachedTable(*sit);
        _cached_sdts.clear();

        _cache_lock.unlock();
    }
    AddListeningPID(DVB_NIT_PID);
    AddListeningPID(DVB_SDT_PID);
}

/** \fn DVBStreamData::HandleTables(uint pid, const PSIPTable&)
 *  \brief Assembles PSIP packets and processes them.
 *  \todo This is just a stub.
 */
bool DVBStreamData::HandleTables(uint pid, const PSIPTable &psip)
{
    if (MPEGStreamData::HandleTables(pid, psip))
        return true;

    if (IsRedundant(pid, psip))
        return true;

    switch (psip.TableID())
    {
        case TableID::NIT:
        {
            SetVersionNIT(psip.Version(), psip.LastSection());
            SetNITSectionSeen(psip.Section());
            NetworkInformationTable nit(psip);

            if (_cache_tables)
                CacheNIT(&nit);

            emit UpdateNIT(&nit);

            return true;
        }
        case TableID::SDT:
        {
            uint tsid = psip.TableIDExtension();
            SetVersionSDT(tsid, psip.Version(), psip.LastSection());
            SetSDTSectionSeen(tsid, psip.Section());
            ServiceDescriptionTable sdt(psip);

            if (_cache_tables)
                CacheSDT(&sdt);

            emit UpdateSDT(tsid, &sdt);

            return true;
        }
        case TableID::NITo:
        {
            SetVersionNITo(psip.Version(), psip.LastSection());
            SetNIToSectionSeen(psip.Section());
            NetworkInformationTable nit(psip);
            emit UpdateNITo(&nit);
            return true;
        }
        case TableID::SDTo:
        {
            uint tsid = psip.TableIDExtension();
            SetVersionSDTo(tsid, psip.Version(), psip.LastSection());
            SetSDToSectionSeen(tsid, psip.Section());
            ServiceDescriptionTable sdt(psip);
            emit UpdateSDTo(tsid, &sdt);
            return true;
        }
    }

    if ((DVB_EIT_PID == pid || DVB_DNLONG_EIT_PID == pid) &&
        DVBEventInformationTable::IsEIT(psip.TableID()))
    {
        uint service_id = psip.TableIDExtension();
        SetVersionEIT(psip.TableID(), service_id, psip.Version());
        SetEITSectionSeen(psip.TableID(), service_id, psip.Section());
        DVBEventInformationTable eit(psip);
        emit UpdateEIT(&eit);
        return true;
    }

    return false;
}

void DVBStreamData::SetNITSectionSeen(uint section)
{
    _nit_section_seen[section>>3] |= bit_sel[section & 0x7];
}

bool DVBStreamData::NITSectionSeen(uint section) const
{
    return (bool) (_nit_section_seen[section>>3] & bit_sel[section & 0x7]);
}

bool DVBStreamData::HasAllNITSections(void) const
{
    for (uint i = 0; i < 32; i++)
        if (_nit_section_seen[i] != 0xff)
            return false;
    return true;
}

void DVBStreamData::SetNIToSectionSeen(uint section)
{
    _nito_section_seen[section>>3] |= bit_sel[section & 0x7];
}

bool DVBStreamData::NIToSectionSeen(uint section) const
{
    return (bool) (_nito_section_seen[section>>3] & bit_sel[section & 0x7]);
}

bool DVBStreamData::HasAllNIToSections(void) const
{
    for (uint i = 0; i < 32; i++)
        if (_nito_section_seen[i] != 0xff)
            return false;
    return true;
}

void DVBStreamData::SetSDTSectionSeen(uint tsid, uint section)
{
    sections_map_t::iterator it = _sdt_section_seen.find(tsid);
    if (it == _sdt_section_seen.end())
    {
        _sdt_section_seen[tsid].resize(32, 0);
        it = _sdt_section_seen.find(tsid);
    }
    (*it)[section>>3] |= bit_sel[section & 0x7];
}

bool DVBStreamData::SDTSectionSeen(uint tsid, uint section) const
{
    sections_map_t::const_iterator it = _sdt_section_seen.find(tsid);
    if (it == _sdt_section_seen.end())
        return false;
    return (bool) ((*it)[section>>3] & bit_sel[section & 0x7]);
}

bool DVBStreamData::HasAllSDTSections(uint tsid) const
{
    sections_map_t::const_iterator it = _sdt_section_seen.find(tsid);
    if (it == _sdt_section_seen.end())
        return false;
    for (uint i = 0; i < 32; i++)
        if ((*it)[i] != 0xff)
            return false;
    return true;
}

void DVBStreamData::SetSDToSectionSeen(uint tsid, uint section)
{
    sections_map_t::iterator it = _sdto_section_seen.find(tsid);
    if (it == _sdto_section_seen.end())
    {
        _sdto_section_seen[tsid].resize(32, 0);
        it = _sdto_section_seen.find(tsid);
    }
    (*it)[section>>3] |= bit_sel[section & 0x7];
}

bool DVBStreamData::SDToSectionSeen(uint tsid, uint section) const
{
    sections_map_t::const_iterator it = _sdto_section_seen.find(tsid);
    if (it == _sdto_section_seen.end())
        return false;
    return (bool) ((*it)[section>>3] & bit_sel[section & 0x7]);
}

bool DVBStreamData::HasAllSDToSections(uint tsid) const
{
    sections_map_t::const_iterator it = _sdto_section_seen.find(tsid);
    if (it == _sdto_section_seen.end())
        return false;
    for (uint i = 0; i < 32; i++)
        if ((*it)[i] != 0xff)
            return false;
    return true;
}
void DVBStreamData::SetEITSectionSeen(uint tableid, uint serviceid,
                                      uint section)
{
    uint key = (tableid<<16) | serviceid;
    sections_map_t::iterator it = _eit_section_seen.find(key);
    if (it == _eit_section_seen.end())
    {
        _eit_section_seen[key].resize(32, 0);
        it = _eit_section_seen.find(key);
    }
    (*it)[section>>3] |= bit_sel[section & 0x7];
}

bool DVBStreamData::EITSectionSeen(uint tableid, uint serviceid,
                                   uint section) const
{
    uint key = (tableid<<16) | serviceid;
    sections_map_t::const_iterator it = _eit_section_seen.find(key);
    if (it == _eit_section_seen.end())
        return false;
    return (bool) ((*it)[section>>3] & bit_sel[section & 0x7]);
}

bool DVBStreamData::HasCachedAnyNIT(bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    return (bool)(_cached_nit.size());
}

bool DVBStreamData::HasCachedAllNIT(bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    if (_cached_nit.empty())
        return false;

    uint last_section = (*_cached_nit.begin())->LastSection();
    if (!last_section)
        return true;

    for (uint i = 1; i <= last_section; i++)
        if (_cached_nit.find(i) == _cached_nit.end())
            return false;

    return true;
}

bool DVBStreamData::HasCachedAllSDT(uint tsid, bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    sdt_cache_t::const_iterator it = _cached_sdts.find(tsid << 8);
    if (it == _cached_sdts.end())
        return false;

    uint last_section = (*it)->LastSection();
    if (!last_section)
        return true;

    for (uint i = 1; i <= last_section; i++)
        if (_cached_sdts.find((tsid << 8) | i) == _cached_sdts.end())
            return false;

    return true;
}

bool DVBStreamData::HasCachedAnySDT(uint tsid, bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    for (uint i = 0; i <= 255; i++)
        if (_cached_sdts.find((tsid << 8) | i) != _cached_sdts.end())
            return true;

    return false;
}

bool DVBStreamData::HasCachedAllSDTs(bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    if (_cached_nit.empty())
        return false;

    nit_cache_t::const_iterator it = _cached_nit.begin();
    for (; it != _cached_nit.end(); ++it)
    {
        if ((*it)->TransportStreamCount() > _cached_sdts.size())
            return false;

        for (uint i = 0; i < (*it)->TransportStreamCount(); i++)
            if (!HasCachedAllSDT((*it)->TSID(i), current))
                return false;
    }

    return true;
}

const nit_ptr_t DVBStreamData::GetCachedNIT(
    uint section_num, bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    nit_ptr_t nit = NULL;

    nit_cache_t::const_iterator it = _cached_nit.find(section_num);
    if (it != _cached_nit.end())
        IncrementRefCnt(nit = *it);

    return nit;
}

const sdt_ptr_t DVBStreamData::GetCachedSDT(
    uint tsid, uint section_num, bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    sdt_ptr_t sdt = NULL;

    uint key = (tsid << 8) | section_num;
    sdt_cache_t::const_iterator it = _cached_sdts.find(key);
    if (it != _cached_sdts.end())
        IncrementRefCnt(sdt = *it);

    return sdt;
}

sdt_vec_t DVBStreamData::GetAllCachedSDTs(bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    sdt_vec_t sdts;

    sdt_cache_t::const_iterator it = _cached_sdts.begin();
    for (; it != _cached_sdts.end(); ++it)
    {
        IncrementRefCnt(*it);
        sdts.push_back(*it);
    }

    return sdts;
}

void DVBStreamData::ReturnCachedSDTTables(sdt_vec_t &sdts) const
{
    for (sdt_vec_t::iterator it = sdts.begin(); it != sdts.end(); ++it)
        ReturnCachedTable(*it);
    sdts.clear();
}

void DVBStreamData::CacheNIT(const NetworkInformationTable *_nit)
{
    NetworkInformationTable *nit = new NetworkInformationTable(*_nit);

    QMutexLocker locker(&_cache_lock);

    nit_cache_t::iterator it = _cached_nit.find(_nit->Section());
    if (it != _cached_nit.end())
        DeleteCachedTable(*it);

    _cached_nit[_nit->Section()] = nit;
}

void DVBStreamData::CacheSDT(const ServiceDescriptionTable *_sdt)
{
    ServiceDescriptionTable *sdt = new ServiceDescriptionTable(*_sdt);
    uint key = (_sdt->TSID() << 8) | _sdt->Section();

    QMutexLocker locker(&_cache_lock);

    sdt_cache_t::iterator it = _cached_sdts.find(key);
    if (it != _cached_sdts.end())
        DeleteCachedTable(*it);

    _cached_sdts[key] = sdt;
}

void DVBStreamData::PrintNIT(const NetworkInformationTable* nit) const
{
    VERBOSE(VB_RECORD, nit->toString());
}

void DVBStreamData::PrintSDT(uint tsid,
                             const ServiceDescriptionTable* sdt) const
{
    (void) tsid;
    VERBOSE(VB_RECORD, sdt->toString());
}
