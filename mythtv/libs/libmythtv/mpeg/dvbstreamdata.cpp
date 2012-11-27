// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include <algorithm>
using namespace std;

#include "dvbstreamdata.h"
#include "dvbtables.h"
#include "premieretables.h"
#include "eithelper.h"

#define PREMIERE_ONID 133
#define FREESAT_EIT_PID 3842
#define MCA_ONID 6144
#define MCA_EIT_TSID 136
#define MCA_EIT_PID 1018

// service_id is synonymous with the MPEG program number in the PMT.
DVBStreamData::DVBStreamData(uint desired_netid,  uint desired_tsid,
                             int desired_program, bool cacheTables)
    : MPEGStreamData(desired_program, cacheTables),
      _desired_netid(desired_netid), _desired_tsid(desired_tsid),
      _dvb_eit_dishnet_long(false),
      _nit_version(-2), _nito_version(-2)
{
    SetVersionNIT(-1,0);
    SetVersionNITo(-1,0);
    AddListeningPID(DVB_NIT_PID);
    AddListeningPID(DVB_SDT_PID);
    AddListeningPID(DVB_TDT_PID);
}

DVBStreamData::~DVBStreamData()
{
    Reset();

    QMutexLocker locker(&_listener_lock);
    _dvb_main_listeners.clear();
    _dvb_other_listeners.clear();
    _dvb_eit_listeners.clear();
    _dvb_has_eit.clear();
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
            _desired_netid = netid;
            _desired_tsid = tsid;
            uint last_section = first_sdt->LastSection();
            ProcessSDT(_desired_tsid, first_sdt);
            ReturnCachedTable(first_sdt);
            for (uint i = 1; i <= last_section; ++i)
            {
                sdt_const_ptr_t sdt = GetCachedSDT(_desired_tsid, i, true);
                ProcessSDT(_desired_tsid, sdt);
                ReturnCachedTable(sdt);
            }
            SetDesiredProgram(serviceid);
        }
    }

    if (reset)
        ResetDVB(netid, tsid, serviceid);
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

    if (TableID::TDT == table_id)
        return false;

    if (TableID::BAT == table_id)
    {
        if (VersionBAT(psip.TableIDExtension()) != version)
            return false;
        return BATSectionSeen(psip.TableIDExtension(), psip.Section());
    }

    bool is_eit = false;
    if (DVB_EIT_PID == pid || FREESAT_EIT_PID == pid)
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

    if (DVB_EIT_PID == pid || FREESAT_EIT_PID == pid || MCA_EIT_PID == pid)
    {
        // Standard Now/Next Event Information Tables for other transport
        is_eit |= TableID::PF_EITo == table_id;
        // Standard Future Event Information Tables for other transports
        is_eit |= (TableID::SC_EITbego <= table_id &&
                   TableID::SC_EITendo >= table_id);
    }
    if (DVB_DNLONG_EIT_PID == pid || DVB_BVLONG_EIT_PID == pid)
    {
        // Dish Network and Bev Long Term Future Event Information
        // for all transports
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

    if (((PREMIERE_EIT_DIREKT_PID == pid) || (PREMIERE_EIT_SPORT_PID == pid)) &&
        TableID::PREMIERE_CIT == table_id)
    {
        uint content_id = PremiereContentInformationTable(psip).ContentID();
        if (VersionCIT(content_id) != version)
            return false;
        return CITSectionSeen(content_id, psip.Section());
    }

    return false;
}

void DVBStreamData::ResetMPEG(int desiredProgram)
{
    ResetDVB(0, 0, desiredProgram);
}

void DVBStreamData::ResetDVB(uint desired_netid, uint desired_tsid,
                             int desired_serviceid)
{
    MPEGStreamData::ResetMPEG(desired_serviceid);

    _desired_netid = desired_netid;
    _desired_tsid  = desired_tsid;

    SetVersionNIT(-1,0);
    _sdt_versions.clear();
    _sdt_section_seen.clear();
    _eit_version.clear();
    _eit_section_seen.clear();
    _cit_version.clear();
    _cit_section_seen.clear();

    SetVersionNITo(-1,0);
    _sdto_versions.clear();
    _sdto_section_seen.clear();
    _bat_versions.clear();
    _bat_section_seen.clear();

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
    AddListeningPID(DVB_TDT_PID);
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

            if (_cache_tables)
            {
                NetworkInformationTable *nit =
                    new NetworkInformationTable(psip);
                CacheNIT(nit);
                QMutexLocker locker(&_listener_lock);
                for (uint i = 0; i < _dvb_main_listeners.size(); i++)
                    _dvb_main_listeners[i]->HandleNIT(nit);
            }
            else
            {
                NetworkInformationTable nit(psip);
                QMutexLocker locker(&_listener_lock);
                for (uint i = 0; i < _dvb_main_listeners.size(); i++)
                    _dvb_main_listeners[i]->HandleNIT(&nit);
            }

            return true;
        }
        case TableID::SDT:
        {
            uint tsid = psip.TableIDExtension();
            SetVersionSDT(tsid, psip.Version(), psip.LastSection());
            SetSDTSectionSeen(tsid, psip.Section());

            if (_cache_tables)
            {
                ServiceDescriptionTable *sdt =
                    new ServiceDescriptionTable(psip);
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

            QMutexLocker locker(&_listener_lock);
            for (uint i = 0; i < _dvb_main_listeners.size(); i++)
                _dvb_main_listeners[i]->HandleTDT(&tdt);

            return true;
        }
        case TableID::NITo:
        {
            SetVersionNITo(psip.Version(), psip.LastSection());
            SetNIToSectionSeen(psip.Section());
            NetworkInformationTable nit(psip);

            QMutexLocker locker(&_listener_lock);
            for (uint i = 0; i < _dvb_other_listeners.size(); i++)
                _dvb_other_listeners[i]->HandleNITo(&nit);

            return true;
        }
        case TableID::SDTo:
        {
            uint tsid = psip.TableIDExtension();
            SetVersionSDTo(tsid, psip.Version(), psip.LastSection());
            SetSDToSectionSeen(tsid, psip.Section());
            ServiceDescriptionTable sdt(psip);

            // some providers send the SDT for the current multiplex as SDTo
            // this routine changes the TableID to SDT and recalculates the CRC
            if (_desired_netid == sdt.OriginalNetworkID() &&
                _desired_tsid  == tsid)
            {
                ServiceDescriptionTable *sdta =
                    new ServiceDescriptionTable(psip);
                if (!sdta->Mutate())
                {
                    delete sdta;
                    return true;
                }
                if (_cache_tables)
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

            QMutexLocker locker(&_listener_lock);
            for (uint i = 0; i < _dvb_other_listeners.size(); i++)
                _dvb_other_listeners[i]->HandleSDTo(tsid, &sdt);

            return true;
        }
        case TableID::BAT:
        {
            uint bid = psip.TableIDExtension();
            SetVersionBAT(bid, psip.Version(), psip.LastSection());
            SetBATSectionSeen(bid, psip.Section());
            BouquetAssociationTable bat(psip);

            QMutexLocker locker(&_listener_lock);
            for (uint i = 0; i < _dvb_other_listeners.size(); i++)
                _dvb_other_listeners[i]->HandleBAT(&bat);

            return true;
        }
    }

    if ((DVB_EIT_PID == pid || DVB_DNLONG_EIT_PID == pid || FREESAT_EIT_PID == pid ||
        ((MCA_ONID == _desired_netid) && (MCA_EIT_TSID == _desired_tsid) &&
        (MCA_EIT_PID == pid)) || DVB_BVLONG_EIT_PID == pid) &&

        DVBEventInformationTable::IsEIT(psip.TableID()))
    {
        QMutexLocker locker(&_listener_lock);
        if (!_dvb_eit_listeners.size() && !_eit_helper)
            return true;

        uint service_id = psip.TableIDExtension();
        SetVersionEIT(psip.TableID(), service_id, psip.Version(),  psip.LastSection());
        SetEITSectionSeen(psip.TableID(), service_id, psip.Section());

        DVBEventInformationTable eit(psip);
        for (uint i = 0; i < _dvb_eit_listeners.size(); i++)
            _dvb_eit_listeners[i]->HandleEIT(&eit);

        if (_eit_helper)
            _eit_helper->AddEIT(&eit);

        return true;
    }

    if (_desired_netid == PREMIERE_ONID &&
        (PREMIERE_EIT_DIREKT_PID == pid || PREMIERE_EIT_SPORT_PID == pid) &&
        PremiereContentInformationTable::IsEIT(psip.TableID()))
    {
        QMutexLocker locker(&_listener_lock);
        if (!_dvb_eit_listeners.size() && !_eit_helper)
            return true;

        PremiereContentInformationTable cit(psip);
        SetVersionCIT(cit.ContentID(), cit.Version());
        SetCITSectionSeen(cit.ContentID(), cit.Section());

        for (uint i = 0; i < _dvb_eit_listeners.size(); i++)
            _dvb_eit_listeners[i]->HandleEIT(&cit);

        if (_eit_helper)
            _eit_helper->AddEIT(&cit);

        return true;
    }

    return false;
}

void DVBStreamData::ProcessSDT(uint tsid, const ServiceDescriptionTable *sdt)
{
    QMutexLocker locker(&_listener_lock);

    for (uint i = 0; i < sdt->ServiceCount(); i++)
    {
        if (sdt->HasEITSchedule(i) || sdt->HasEITPresentFollowing(i))
            _dvb_has_eit[sdt->ServiceID(i)] = true;
    }

    for (uint i = 0; i < _dvb_main_listeners.size(); i++)
        _dvb_main_listeners[i]->HandleSDT(tsid, sdt);
}

bool DVBStreamData::HasEITPIDChanges(const uint_vec_t &in_use_pids) const
{
    QMutexLocker locker(&_listener_lock);
    bool want_eit = (_eit_rate >= 0.5f) && HasAnyEIT();
    bool has_eit  = in_use_pids.size();
    return want_eit != has_eit;
}

bool DVBStreamData::GetEITPIDChanges(const uint_vec_t &cur_pids,
                                     uint_vec_t &add_pids,
                                     uint_vec_t &del_pids) const
{
    QMutexLocker locker(&_listener_lock);

    if ((_eit_rate >= 0.5f) && HasAnyEIT())
    {
        if (find(cur_pids.begin(), cur_pids.end(),
                 (uint) DVB_EIT_PID) == cur_pids.end())
        {
            add_pids.push_back(DVB_EIT_PID);
        }

        if (_dvb_eit_dishnet_long &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) DVB_DNLONG_EIT_PID) == cur_pids.end())
        {
            add_pids.push_back(DVB_DNLONG_EIT_PID);
        }

        if (_dvb_eit_dishnet_long &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) DVB_BVLONG_EIT_PID) == cur_pids.end())
        {
            add_pids.push_back(DVB_BVLONG_EIT_PID);
        }

        if (_desired_netid == PREMIERE_ONID &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) PREMIERE_EIT_DIREKT_PID) == cur_pids.end())
        {
            add_pids.push_back(PREMIERE_EIT_DIREKT_PID);
        }

        if (_desired_netid == PREMIERE_ONID &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) PREMIERE_EIT_SPORT_PID) == cur_pids.end())
        {
            add_pids.push_back(PREMIERE_EIT_SPORT_PID);
        }

        if (find(cur_pids.begin(), cur_pids.end(),
                 (uint) FREESAT_EIT_PID) == cur_pids.end())
        {
            add_pids.push_back(FREESAT_EIT_PID);
        }

        if (MCA_ONID == _desired_netid && MCA_EIT_TSID == _desired_tsid &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) MCA_EIT_PID) == cur_pids.end())
        {
            add_pids.push_back(MCA_EIT_PID);
        }

    }
    else
    {
        if (find(cur_pids.begin(), cur_pids.end(),
                 (uint) DVB_EIT_PID) != cur_pids.end())
        {
            del_pids.push_back(DVB_EIT_PID);
        }

        if (_dvb_eit_dishnet_long &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) DVB_DNLONG_EIT_PID) != cur_pids.end())
        {
            del_pids.push_back(DVB_DNLONG_EIT_PID);
        }

        if (_dvb_eit_dishnet_long &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) DVB_BVLONG_EIT_PID) != cur_pids.end())
        {
            del_pids.push_back(DVB_BVLONG_EIT_PID);
        }

        if (_desired_netid == PREMIERE_ONID &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) PREMIERE_EIT_DIREKT_PID) != cur_pids.end())
        {
            del_pids.push_back(PREMIERE_EIT_DIREKT_PID);
        }

        if (_desired_netid == PREMIERE_ONID &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) PREMIERE_EIT_SPORT_PID) != cur_pids.end())
        {
            del_pids.push_back(PREMIERE_EIT_SPORT_PID);
        }

        if (find(cur_pids.begin(), cur_pids.end(),
                 (uint) FREESAT_EIT_PID) == cur_pids.end())
        {
            del_pids.push_back(FREESAT_EIT_PID);
        }

        if (MCA_ONID == _desired_netid && MCA_EIT_TSID == _desired_tsid &&
            find(cur_pids.begin(), cur_pids.end(),
                 (uint) MCA_EIT_PID) != cur_pids.end())
        {
            del_pids.push_back(MCA_EIT_PID);
        }
    }

    return add_pids.size() || del_pids.size();
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

void DVBStreamData::SetBATSectionSeen(uint bid, uint section)
{
    sections_map_t::iterator it = _bat_section_seen.find(bid);
    if (it == _bat_section_seen.end())
    {
        _bat_section_seen[bid].resize(32, 0);
        it = _bat_section_seen.find(bid);
    }
    (*it)[section>>3] |= bit_sel[section & 0x7];
}

bool DVBStreamData::BATSectionSeen(uint bid, uint section) const
{
    sections_map_t::const_iterator it = _bat_section_seen.find(bid);
    if (it == _bat_section_seen.end())
        return false;
    return (bool) ((*it)[section>>3] & bit_sel[section & 0x7]);
}

bool DVBStreamData::HasAllBATSections(uint bid) const
{
    sections_map_t::const_iterator it = _bat_section_seen.find(bid);
    if (it == _bat_section_seen.end())
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

void DVBStreamData::SetCITSectionSeen(uint contentid, uint section)
{
    sections_map_t::iterator it = _cit_section_seen.find(contentid);
    if (it == _cit_section_seen.end())
    {
        _cit_section_seen[contentid].resize(32, 0);
        it = _cit_section_seen.find(contentid);
    }
    (*it)[section>>3] |= bit_sel[section & 0x7];
}

bool DVBStreamData::CITSectionSeen(uint contentid, uint section) const
{
    sections_map_t::const_iterator it = _cit_section_seen.find(contentid);
    if (it == _cit_section_seen.end())
        return false;
    return (bool) ((*it)[section>>3] & bit_sel[section & 0x7]);
}

bool DVBStreamData::HasCachedAnyNIT(bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, "Currently we ignore \'current\' param");

    return (bool)(_cached_nit.size());
}

bool DVBStreamData::HasCachedAllNIT(bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, "Currently we ignore \'current\' param");

    if (_cached_nit.empty())
        return false;

    uint last_section = (*_cached_nit.begin())->LastSection();
    if (!last_section)
        return true;

    for (uint i = 0; i <= last_section; i++)
        if (_cached_nit.find(i) == _cached_nit.end())
            return false;

    return true;
}

bool DVBStreamData::HasCachedAllSDT(uint tsid, bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, "Currently we ignore \'current\' param");

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
        LOG(VB_GENERAL, LOG_WARNING, "Currently we ignore \'current\' param");

    for (uint i = 0; i <= 255; i++)
        if (_cached_sdts.find((tsid << 8) | i) != _cached_sdts.end())
            return true;

    return false;
}

bool DVBStreamData::HasCachedSDT(bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (_cached_nit.empty())
        return false;

    nit_cache_t::const_iterator it = _cached_nit.begin();
    for (; it != _cached_nit.end(); ++it)
    {
        for (uint i = 0; i < (*it)->TransportStreamCount(); i++)
        {
            if (HasCachedAllSDT((*it)->TSID(i), current))
                return true;
        }
    }

    return false;
}

bool DVBStreamData::HasCachedAnySDTs(bool current) const
{
    QMutexLocker locker(&_cache_lock);
    return !_cached_sdts.empty();
}

bool DVBStreamData::HasCachedAllSDTs(bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (_cached_nit.empty())
        return false;

    nit_cache_t::const_iterator it = _cached_nit.begin();
    for (; it != _cached_nit.end(); ++it)
    {
        if ((int)(*it)->TransportStreamCount() > _cached_sdts.size())
            return false;

        for (uint i = 0; i < (*it)->TransportStreamCount(); i++)
            if (!HasCachedAllSDT((*it)->TSID(i), current))
                return false;
    }

    return true;
}

nit_const_ptr_t DVBStreamData::GetCachedNIT(
    uint section_num, bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, "Currently we ignore \'current\' param");

    nit_ptr_t nit = NULL;

    nit_cache_t::const_iterator it = _cached_nit.find(section_num);
    if (it != _cached_nit.end())
        IncrementRefCnt(nit = *it);

    return nit;
}

nit_vec_t DVBStreamData::GetCachedNIT(bool current) const
{
    QMutexLocker locker(&_cache_lock);

    nit_vec_t nits;

    for (uint i = 0; i < 256; i++)
    {
        nit_const_ptr_t nit = GetCachedNIT(i, current);
        if (nit)
            nits.push_back(nit);
    }

    return nits;
}

sdt_const_ptr_t DVBStreamData::GetCachedSDT(
    uint tsid, uint section_num, bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, "Currently we ignore \'current\' param");

    sdt_ptr_t sdt = NULL;

    uint key = (tsid << 8) | section_num;
    sdt_cache_t::const_iterator it = _cached_sdts.find(key);
    if (it != _cached_sdts.end())
        IncrementRefCnt(sdt = *it);

    return sdt;
}

sdt_vec_t DVBStreamData::GetCachedSDTs(bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, "Currently we ignore \'current\' param");

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

bool DVBStreamData::DeleteCachedTable(PSIPTable *psip) const
{
    if (!psip)
        return false;

    uint tid = psip->TableIDExtension();

    QMutexLocker locker(&_cache_lock);
    if (_cached_ref_cnt[psip] > 0)
    {
        _cached_slated_for_deletion[psip] = 1;
        return false;
    }
    else if ((TableID::NIT == psip->TableID()) &&
             _cached_nit[psip->Section()])
    {
        _cached_nit[psip->Section()] = NULL;
        delete psip;
    }
    else if ((TableID::SDT == psip->TableID()) &&
             _cached_sdts[tid << 8 | psip->Section()])
    {
        _cached_sdts[tid << 8 | psip->Section()] = NULL;
        delete psip;
    }
    else
    {
        return MPEGStreamData::DeleteCachedTable(psip);
    }
    psip_refcnt_map_t::iterator it;
    it = _cached_slated_for_deletion.find(psip);
    if (it != _cached_slated_for_deletion.end())
        _cached_slated_for_deletion.erase(it);

    return true;
}

void DVBStreamData::CacheNIT(NetworkInformationTable *nit)
{
    QMutexLocker locker(&_cache_lock);

    nit_cache_t::iterator it = _cached_nit.find(nit->Section());
    if (it != _cached_nit.end())
        DeleteCachedTable(*it);

    _cached_nit[nit->Section()] = nit;
}

void DVBStreamData::CacheSDT(ServiceDescriptionTable *sdt)
{
    uint key = (sdt->TSID() << 8) | sdt->Section();

    QMutexLocker locker(&_cache_lock);

    sdt_cache_t::iterator it = _cached_sdts.find(key);
    if (it != _cached_sdts.end())
        DeleteCachedTable(*it);

    _cached_sdts[key] = sdt;
}

void DVBStreamData::AddDVBMainListener(DVBMainStreamListener *val)
{
    QMutexLocker locker(&_listener_lock);

    dvb_main_listener_vec_t::iterator it = _dvb_main_listeners.begin();
    for (; it != _dvb_main_listeners.end(); ++it)
        if (((void*)val) == ((void*)*it))
            return;

    _dvb_main_listeners.push_back(val);
}

void DVBStreamData::RemoveDVBMainListener(DVBMainStreamListener *val)
{
    QMutexLocker locker(&_listener_lock);

    dvb_main_listener_vec_t::iterator it = _dvb_main_listeners.begin();
    for (; it != _dvb_main_listeners.end(); ++it)
    {
        if (((void*)val) == ((void*)*it))
        {
            _dvb_main_listeners.erase(it);
            return;
        }
    }
}

void DVBStreamData::AddDVBOtherListener(DVBOtherStreamListener *val)
{
    QMutexLocker locker(&_listener_lock);

    dvb_other_listener_vec_t::iterator it = _dvb_other_listeners.begin();
    for (; it != _dvb_other_listeners.end(); ++it)
        if (((void*)val) == ((void*)*it))
            return;

    _dvb_other_listeners.push_back(val);
}

void DVBStreamData::RemoveDVBOtherListener(DVBOtherStreamListener *val)
{
    QMutexLocker locker(&_listener_lock);

    dvb_other_listener_vec_t::iterator it = _dvb_other_listeners.begin();
    for (; it != _dvb_other_listeners.end(); ++it)
    {
        if (((void*)val) == ((void*)*it))
        {
            _dvb_other_listeners.erase(it);
            return;
        }
    }
}

void DVBStreamData::AddDVBEITListener(DVBEITStreamListener *val)
{
    QMutexLocker locker(&_listener_lock);

    dvb_eit_listener_vec_t::iterator it = _dvb_eit_listeners.begin();
    for (; it != _dvb_eit_listeners.end(); ++it)
        if (((void*)val) == ((void*)*it))
            return;

    _dvb_eit_listeners.push_back(val);
}

void DVBStreamData::RemoveDVBEITListener(DVBEITStreamListener *val)
{
    QMutexLocker locker(&_listener_lock);

    dvb_eit_listener_vec_t::iterator it = _dvb_eit_listeners.begin();
    for (; it != _dvb_eit_listeners.end(); ++it)
    {
        if (((void*)val) == ((void*)*it))
        {
            _dvb_eit_listeners.erase(it);
            return;
        }
    }
}
