// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "dvbstreamdata.h"
#include "dvbtables.h"

DVBStreamData::DVBStreamData(bool cacheTables)
    : MPEGStreamData(-1, cacheTables),
      _nit_version(-1), _nito_version(-1),
      _cached_nit(NULL)
{
    setName("DVBStreamData");
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

    if (TableID::NITo == table_id)
    {
        if (VersionNITo() != version)
            return false;
        return NIToSectionSeen(psip.Section());
    }

    if (TableID::SDT == table_id)
    {
        if (VersionSDT(psip.TableIDExtension()) != version)
            return false;
        return SDTSectionSeen(psip.TableIDExtension(), psip.Section());
    }

    if (TableID::SDTo == table_id)
    {
        if (VersionSDTo(psip.TableIDExtension()) != version)
            return false;
        return SDToSectionSeen(psip.TableIDExtension(), psip.Section());
    }

    return false;
}

void DVBStreamData::Reset()
{
    MPEGStreamData::Reset(-1);
    _nit_version = -1;
    _sdt_versions.clear();
    
    {
        _cache_lock.lock();

        DeleteCachedTable(_cached_nit);
        _cached_nit = NULL;

        sdt_cache_t::iterator it = _cached_sdts.begin();
        for (; it != _cached_sdts.end(); ++it)
            DeleteCachedTable(*it);
        _cached_sdts.clear();

        _cache_lock.unlock();
    }
    AddListeningPID(DVB_NIT_PID);
}

/** \fn DVBStreamData::HandleTables(uint pid, const PSIPTable&)
 *  \brief Assembles PSIP packets and processes them.
 *  \todo This is just a stub.
 */
bool DVBStreamData::HandleTables(uint pid, const PSIPTable &psip)
{
    if (MPEGStreamData::HandleTables(pid, psip))
        return true;

    switch (psip.TableID())
    {
        case TableID::NIT:
        {
            SetVersionNIT(psip.Version());
            SetNITSectionSeen(psip.Section());

            if (_cache_tables)
            {
                NetworkInformationTable *nit =
                    new NetworkInformationTable(psip);
                CacheNIT(nit);
                emit UpdateNIT(nit);
            }
            else
            {
                NetworkInformationTable nit(psip);
                emit UpdateNIT(&nit);
            }
            return true;
        }
        case TableID::SDT:
        {
            SetVersionSDT(psip.TableIDExtension(), psip.Version());
            SetSDTSectionSeen(psip.TableIDExtension(), psip.Section());

            if (_cache_tables)
            {
                ServiceDescriptionTable *sdt =
                    new ServiceDescriptionTable(psip);
                CacheSDT(psip.TableIDExtension(), sdt);
                emit UpdateSDT(psip.TableIDExtension(), sdt);
            }
            else
            {
                ServiceDescriptionTable sdt(psip);
                emit UpdateSDT(psip.TableIDExtension(), &sdt);
            }
            return true;
        }
        case TableID::NITo:
        {
            SetVersionNITo(psip.Version());
            SetNIToSectionSeen(psip.Section());
            NetworkInformationTable nit(psip);
            emit UpdateNITo(&nit);
        }
        case TableID::SDTo:
        {
            SetVersionSDTo(psip.TableIDExtension(), psip.Version());
            SetSDToSectionSeen(psip.TableIDExtension(), psip.Section());
            ServiceDescriptionTable sdt(psip);
            emit UpdateSDTo(psip.TableIDExtension(), &sdt);
        }
    }
    return false;
}

void DVBStreamData::SetNITSectionSeen(uint section)
{
    static const unsigned char bit_sel[] =
        { 0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, };
    _nit_section_seen[section>>3] |= bit_sel[section & 0x7];
}

bool DVBStreamData::NITSectionSeen(uint section) const
{
    static const unsigned char bit_sel[] =
        { 0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, };
    return (bool) (_nit_section_seen[section>>3] & bit_sel[section & 0x7]);
}

void DVBStreamData::SetNIToSectionSeen(uint section)
{
    static const unsigned char bit_sel[] =
        { 0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, };
    _nito_section_seen[section>>3] |= bit_sel[section & 0x7];
}

bool DVBStreamData::NIToSectionSeen(uint section) const
{
    static const unsigned char bit_sel[] =
        { 0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, };
    return (bool) (_nito_section_seen[section>>3] & bit_sel[section & 0x7]);
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

bool DVBStreamData::HasCachedNIT(bool current) const
{
    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    return (bool)(_cached_nit);
}

bool DVBStreamData::HasCachedSDT(uint tsid, bool current) const
{
    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    _cache_lock.lock();
    sdt_cache_t::const_iterator it = _cached_sdts.find(tsid);
    bool exists = (it != _cached_sdts.end());
    _cache_lock.unlock();
    return exists;
}

bool DVBStreamData::HasCachedAllSDTs(bool current) const
{
    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    if (!_cached_nit)
        return false;

    _cache_lock.lock();
    bool ret = (bool)(_cached_nit);
    for (uint i = 0; ret && (i < _cached_nit->TransportStreamCount()); ++i)
        ret &= HasCachedSDT(_cached_nit->TSID(i));
    _cache_lock.unlock();

    return ret;
}

const NetworkInformationTable *DVBStreamData::GetCachedNIT(bool current) const
{
    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    _cache_lock.lock();
    const NetworkInformationTable *nit = _cached_nit;
    IncrementRefCnt(nit);
    _cache_lock.unlock();

    return nit;
}

const sdt_ptr_t DVBStreamData::GetCachedSDT(uint tsid, bool current) const
{
    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    ServiceDescriptionTable *sdt = NULL;

    _cache_lock.lock();
    sdt_cache_t::const_iterator it = _cached_sdts.find(tsid);
    if (it != _cached_sdts.end())
        IncrementRefCnt(sdt = *it);
    _cache_lock.unlock();

    return sdt;
}

sdt_vec_t DVBStreamData::GetAllCachedSDTs(bool current) const
{
    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    vector<const ServiceDescriptionTable*> sdts;

    _cache_lock.lock();
    sdt_cache_t::const_iterator it = _cached_sdts.begin();
    for (; it != _cached_sdts.end(); ++it)
    {
        ServiceDescriptionTable* sdt = *it;
        IncrementRefCnt(sdt);
        sdts.push_back(sdt);
    }
    _cache_lock.unlock();

    return sdts;
}

void DVBStreamData::ReturnCachedSDTTables(sdt_vec_t &sdts) const
{
    for (sdt_vec_t::iterator it = sdts.begin(); it != sdts.end(); ++it)
        ReturnCachedTable(*it);
    sdts.clear();
}

void DVBStreamData::CacheNIT(NetworkInformationTable *nit)
{
    _cache_lock.lock();
    DeleteCachedTable(_cached_nit);
    _cached_nit = nit;
    _cache_lock.unlock();
}

void DVBStreamData::CacheSDT(uint tsid, ServiceDescriptionTable *sdt)
{
    _cache_lock.lock();
    sdt_cache_t::iterator it = _cached_sdts.find(tsid);
    if (it != _cached_sdts.end())
        DeleteCachedTable(*it);
    _cached_sdts[tsid] = sdt;
    _cache_lock.unlock();    
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
