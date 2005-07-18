// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "dvbstreamdata.h"
#include "dvbtables.h"

DVBStreamData::DVBStreamData(bool cacheTables)
    : QObject(NULL, "DVBStreamData"),
      MPEGStreamData(-1, cacheTables), _nit_version(-1), _cached_nit(NULL)
{
    AddListeningPID(DVB_NIT_PID);
    AddListeningPID(DVB_SDT_PID);
}

/** \fn DVBStreamData::IsRedundant(const PSIPTable&) const
 *  \brief Returns true if table already seen.
 *  \todo This is just a stub.
 */
bool DVBStreamData::IsRedundant(const PSIPTable &psip) const
{
    if (MPEGStreamData::IsRedundant(psip))
        return true;

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
            VERBOSE(VB_RECORD, "DVBStreamData: Got NIT");
            SetVersionNIT(psip.Version());
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
            VERBOSE(VB_RECORD, "DVBStreamData: Got SDT");
            uint tsid = psip.TableIDExtension();
            SetVersionSDT(tsid, psip.Version());
            if (_cache_tables)
            {
                ServiceDescriptionTable *sdt =
                    new ServiceDescriptionTable(psip);
                CacheSDT(tsid, sdt);
                emit UpdateSDT(tsid, sdt);
            }
            else
            {
                ServiceDescriptionTable sdt(psip);
                emit UpdateSDT(tsid, &sdt);
            }
            return true;
        }
    }
    return false;
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
