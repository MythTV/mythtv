// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include <algorithm>
using namespace std;

#include "qsharedpointer.h"
#include "dvbstreamdata.h"
#include "dvbtables.h"
#include "premieretables.h"
#include "eithelper.h"

#define PREMIERE_ONID 133
#define FREESAT_EIT_PID 3842
#define MCA_ONID 6144
#define MCA_EIT_TSID 136
#define MCA_EIT_PID 1018

#define LOC QString("DVBStream[%1]: ").arg(_cardid)

// service_id is synonymous with the MPEG program number in the PMT.
DVBStreamData::DVBStreamData(uint desired_netid,  uint desired_tsid,
                             int desired_program, int cardnum, bool cacheTables)
    : MPEGStreamData(desired_program, cardnum, cacheTables),
      _desired_netid(desired_netid), _desired_tsid(desired_tsid),
      _dvb_real_network_id(-1), _dvb_eit_dishnet_long(false)
{
    _nit_status.SetVersion(-1,0);
    _nito_status.SetVersion(-1,0);
    AddListeningPID(DVB_NIT_PID);
    AddListeningPID(DVB_SDT_PID);
    AddListeningPID(DVB_RST_PID);
    AddListeningPID(DVB_TDT_PID);
}

DVBStreamData::~DVBStreamData()
{
    Reset(_desired_netid, _desired_tsid, _desired_program);

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
        return _nit_status.IsSectionSeen(version, psip.Section());
    }

    if (TableID::SDT == table_id)
    {
        return _sdt_status.IsSectionSeen(psip.TableIDExtension(), version, psip.Section());
    }

    if (TableID::TDT == table_id)
        return false;

    if (TableID::BAT == table_id)
    {
        return _bat_status.IsSectionSeen(psip.TableIDExtension(), version, psip.Section());
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
        DVBEventInformationTable eit(psip);
        return EITSectionSeen(eit.OriginalNetworkID(), eit.TSID(),
                              eit.ServiceID(), eit.TableID(),
                              version, eit.Section());
    }

    ////////////////////////////////////////////////////////////////////////
    // Other transport tables

    if (TableID::NITo == table_id)
    {
        return _nito_status.IsSectionSeen(version, psip.Section());
    }

    if (TableID::SDTo == table_id)
    {
        return _sdto_status.IsSectionSeen(psip.TableIDExtension(), version, psip.Section());
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
        DVBEventInformationTable eit(psip);
        return EITSectionSeen(eit.OriginalNetworkID(), eit.TSID(),
                              eit.ServiceID(), eit.TableID(),
                              version, eit.Section());
    }

    if (((PREMIERE_EIT_DIREKT_PID == pid) || (PREMIERE_EIT_SPORT_PID == pid)) &&
                                TableID::PREMIERE_CIT == table_id)
        return _cit_status.IsSectionSeen(PremiereContentInformationTable(psip).ContentID(),
                                         version, psip.Section());

    return false;
}

void DVBStreamData::Reset(uint desired_netid, uint desired_tsid,
                          int desired_serviceid)
{
    MPEGStreamData::Reset(desired_serviceid);

    _desired_netid = desired_netid;
    _desired_tsid  = desired_tsid;

    _nit_status.SetVersion(-1,0);
    _sdt_status.clear();
    _eit_status.clear();
    _cit_status.clear();

    _nito_status.SetVersion(-1,0);
    _sdto_status.clear();
    _bat_status.clear();

    {
        _cache_lock.lock();

        nit_vec_t nit_sections;;
        for (nit_cache_t::iterator nit = _cached_nit.begin();
                nit != _cached_nit.end(); ++nit)
            nit_sections.push_back(*nit);
        for (nit_vec_t::iterator nit = nit_sections.begin();
                nit != nit_sections.end(); ++nit)
            DeleteCachedTableSection(*nit);
        _cached_nit.clear();


        sdt_vec_t sdt_sections;;
        for (sdt_cache_t::iterator sdt = _cached_sdts.begin();
                sdt != _cached_sdts.end(); ++sdt)
            sdt_sections.push_back(*sdt);

        for (sdt_vec_t::iterator sdt = sdt_sections.begin();
                sdt != sdt_sections.end(); ++sdt)
            DeleteCachedTableSection(*sdt);
        _cached_sdts.clear();

        //ValidateEITCache(); // Uncomment this to debug the eit cache

        for (eit_stssn_cache_t::iterator network = _cached_eits.begin();
                network != _cached_eits.end(); ++network)
        {
            for (eit_stss_cache_t::iterator stream = (*network).begin();
                    stream != (*network).end(); ++stream)
            {
                for (eit_sts_cache_t::iterator service = (*stream).begin();
                        service != (*stream).end(); ++service)
                {
                    for (eit_st_cache_t::iterator table = (*service).begin(); table != (*service).end(); ++table)
                    {
                        for (eit_sections_cache_t::iterator section = (*table).begin();
                                section != (*table).end(); ++section)
                        	DeleteCachedTableSection(*section);
                        (*table).clear();
                    }
                    (*service).clear();
                }
                (*stream).clear();
            }
            (*network).clear();
        }

        _cached_eits.clear();

        _cache_lock.unlock();
    }
    AddListeningPID(DVB_NIT_PID);
    AddListeningPID(DVB_SDT_PID);
    AddListeningPID(DVB_RST_PID);
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
            if (_dvb_real_network_id >= 0 && psip.TableIDExtension() != (uint)_dvb_real_network_id)
            {
                NetworkInformationTable *nit = new NetworkInformationTable(psip);
                if (!nit->Mutate())
                {
                    delete nit;
                    return true;
                }
                bool retval = HandleTables(pid, *nit);
                delete nit;
                return retval;
            }
            _nit_status.SetSectionSeen(psip.Version(), psip.Section(),
                                        psip.LastSection());

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
            _sdt_status.SetSectionSeen(tsid, psip.Version(), psip.Section(), psip.LastSection());

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
            if (_dvb_real_network_id >= 0 && psip.TableIDExtension() == (uint)_dvb_real_network_id)
            {
                NetworkInformationTable *nit = new NetworkInformationTable(psip);
                if (!nit->Mutate())
                {
                    delete nit;
                    return true;
                }
                bool retval = HandleTables(pid, *nit);
                delete nit;
                return retval;
            }

            _nito_status.SetSectionSeen(psip.Version(), psip.Section(),
                                        psip.LastSection());

            NetworkInformationTable nit(psip);

            QMutexLocker locker(&_listener_lock);
            for (uint i = 0; i < _dvb_other_listeners.size(); i++)
                _dvb_other_listeners[i]->HandleNITo(&nit);

            return true;
        }
        case TableID::SDTo:
        {
            uint tsid = psip.TableIDExtension();
            _sdto_status.SetSectionSeen(tsid, psip.Version(), psip.Section(), psip.LastSection());
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
            _bat_status.SetSectionSeen(bid, psip.Version(), psip.Section(),
                                       psip.LastSection());
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

        DVBEventInformationTable* eit = new DVBEventInformationTable(psip);

        SetEITSectionSeen(eit->OriginalNetworkID(), eit->TSID(),
                      eit->ServiceID(), eit->TableID(),
                      eit->Version(), eit->Section(),
					  eit->SegmentLastSectionNumber(), eit->LastSection());
        CacheEIT(eit);

        for (uint i = 0; i < _dvb_eit_listeners.size(); i++)
            _dvb_eit_listeners[i]->HandleEIT(eit);

        if(HasAllEITSections(eit->OriginalNetworkID(), eit->TSID(),
                      eit->ServiceID(), eit->TableID()))
        {
            uint onid = eit->OriginalNetworkID();
            uint tsid = eit->TSID();
            if (onid== 0x233a)
                onid = GenerateUniqueUKOriginalNetworkID(tsid);
            uint sid = eit->ServiceID();
            uint tid = eit->TableID();
            LOG(VB_EITDVBCACHE, LOG_DEBUG, LOC + QString(
                    "Subtable 0x%1/0x%2/0x%3/%4 complete version %5")
                .arg(onid,0,16)
                .arg(tsid,0,16)
                .arg(sid,0,16)
                .arg(tid)
                .arg(eit->Version()));

            // Complete table seen
            // Grab the cache lock
            QMutexLocker locker(&_cache_lock);

            // Build a vector of the cached table sections
            eit_vec_t table;
            eit_sections_cache_t& sections = _cached_eits[onid][tsid][sid][tid];
            for (eit_sections_cache_t::iterator i = sections.begin(); i != sections.end(); ++i)
            	table.push_back(*i);

            // and pass up to the EIT helper
            // Temporary testing using old code
            if (_eit_helper)
            {
                for (eit_vec_t::iterator i = table.begin(); i != table.end(); ++i)
                    _eit_helper->AddEIT(*i);
            }

            // Delete the table sections from the cache
            for (eit_vec_t::iterator i = table.begin(); i != table.end(); ++i)
                DeleteCachedTableSection(*i);
            sections.clear();
            _cached_eits[onid][tsid][sid].remove(tid);
            if (_cached_eits[onid][tsid][sid].empty())
                _cached_eits[onid][tsid].remove(sid);
            if (_cached_eits[onid][tsid].empty())
                _cached_eits[onid].remove(tsid);
            if (_cached_eits[onid].empty())
                _cached_eits.remove(onid);
        }

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
        _cit_status.SetSectionSeen(cit.ContentID(), psip.Version(), psip.Section(), psip.LastSection());

        for (uint i = 0; i < _dvb_eit_listeners.size(); i++)
            _dvb_eit_listeners[i]->HandleEIT(&cit);

        if (_eit_helper)
            _eit_helper->AddEIT(&cit);

        return true;
    }

    return false;
}

void DVBStreamData::ProcessSDT(uint tsid, sdt_const_ptr_t sdt)
{
    QMutexLocker locker(&_listener_lock);

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
                 (uint) FREESAT_EIT_PID) != cur_pids.end())
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

bool DVBStreamData::HasAllNITSections(void) const
{
    return _nit_status.HasAllSections();
}

bool DVBStreamData::HasAllNIToSections(void) const
{
    return _nit_status.HasAllSections();
}

bool DVBStreamData::HasAllSDTSections(uint tsid) const
{
    return _sdt_status.HasAllSections(tsid);
}

bool DVBStreamData::HasAllSDToSections(uint tsid) const
{
    return _sdto_status.HasAllSections(tsid);
}

bool DVBStreamData::HasAllBATSections(uint bid) const
{
    return _bat_status.HasAllSections(bid);
}

void DVBStreamData::SetEITSectionSeen(uint original_network_id, uint transport_stream_id,
                                      uint serviceid, uint tableid,
                                      uint version, uint section,
                                      uint segment_last_section, uint last_section)
{
    if (original_network_id == 0x233a)
        original_network_id = GenerateUniqueUKOriginalNetworkID(transport_stream_id);

    uint64_t key =
            uint64_t(original_network_id) << 48 |
            uint64_t(transport_stream_id)  << 32 |
            serviceid << 16 | tableid;

    TableStatusMap::const_iterator table_status = _eit_status.find(key);
    if (table_status != _eit_status.end())
    {
		QString bitmap;
		uint count = 1;
		for (TableStatus::sections_t::const_iterator iTr
					= table_status->m_sections.begin();
				iTr != table_status->m_sections.end();
				iTr++)
		{
			bitmap.append(QString("%1 ").arg(*iTr, 8, 2, QChar('0')));
			if (!(count % 8))
				bitmap.append(QString("\n"));
			count++;
		}

		LOG(VB_EIT, LOG_DEBUG, LOC + QString(
				"Subtable 0x%1/0x%2/0x%3/%4 version %5 section %6(%7/%8) "
				"sls %9(%10/%11) ls %12(%13/%14) - old map\n"
				"%15")
			.arg(key >> 48, 0, 16)
			.arg((key >> 32) & 0xffff, 0, 16)
			.arg((key >> 16) & 0xffff, 0, 16)
			.arg(key & 0xffff)
			.arg(version)
			.arg(section)
			.arg(section >> 3)
			.arg(section % 8)
			.arg(segment_last_section)
			.arg(segment_last_section >> 3)
			.arg(segment_last_section % 8)
			.arg(last_section)
			.arg(last_section >> 3)
			.arg(last_section % 8)
			.arg(bitmap));
    }

    _eit_status.SetSectionSeen(key, version, section, last_section, segment_last_section);

    table_status = _eit_status.find(key);
    if (table_status != _eit_status.end())
    {
		QString bitmap;
		uint count = 1;
		for (TableStatus::sections_t::const_iterator iTr
					= table_status->m_sections.begin();
				iTr != table_status->m_sections.end();
				iTr++)
		{
			bitmap.append(QString("%1 ").arg(*iTr, 8, 2, QChar('0')));
			if (!(count % 8))
				bitmap.append(QString("\n"));
			count++;
		}

		LOG(VB_EIT, LOG_DEBUG, LOC + QString(
				"Subtable 0x%1/0x%2/0x%3/%4 version %5 section %6(%7/%8) "
				"sls %9(%10/%11) ls %12(%13/%14) - new map\n"
				"%15")
			.arg(key >> 48, 0, 16)
			.arg((key >> 32) & 0xffff, 0, 16)
			.arg((key >> 16) & 0xffff, 0, 16)
			.arg(key & 0xffff)
			.arg(version)
			.arg(section)
			.arg(section >> 3)
			.arg(section % 8)
			.arg(segment_last_section)
			.arg(segment_last_section >> 3)
			.arg(segment_last_section % 8)
			.arg(last_section)
			.arg(last_section >> 3)
			.arg(last_section % 8)
			.arg(bitmap));
    }
}

bool DVBStreamData::EITSectionSeen(uint original_network_id, uint transport_stream_id,
                                   uint serviceid, uint tableid,
                                   uint version, uint section) const
{
    if (original_network_id == 0x233a)
        original_network_id = GenerateUniqueUKOriginalNetworkID(transport_stream_id);

    uint64_t key =
            uint64_t(original_network_id) << 48 |
            uint64_t(transport_stream_id)  << 32 |
            serviceid << 16 | tableid;
    return _eit_status.IsSectionSeen(key, version, section);
}

bool DVBStreamData::HasAllEITSections(uint original_network_id, uint transport_stream_id,
                                      uint serviceid, uint tableid) const
{
    if (original_network_id == 0x233a)
        original_network_id = GenerateUniqueUKOriginalNetworkID(transport_stream_id);

    uint64_t key =
            uint64_t(original_network_id) << 48 |
            uint64_t(transport_stream_id)  << 32 |
            serviceid << 16 | tableid;
    return _eit_status.HasAllSections(key);
}

bool DVBStreamData::HasCachedAnyNIT(bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    return (bool)(_cached_nit.size());
}

bool DVBStreamData::HasCachedAllNIT(bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

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
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

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
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

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
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    nit_ptr_t nit = NULL;

    nit_cache_t::const_iterator it = _cached_nit.find(section_num);
    if (it != _cached_nit.end())
        IncrementRefCnt(nit = *it);

    return nit;
}

nit_const_vec_t DVBStreamData::GetCachedNIT(bool current) const
{
    QMutexLocker locker(&_cache_lock);

    nit_const_vec_t nits;

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
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    sdt_ptr_t sdt = NULL;

    uint key = (tsid << 8) | section_num;
    sdt_cache_t::const_iterator it = _cached_sdts.find(key);
    if (it != _cached_sdts.end())
        IncrementRefCnt(sdt = *it);

    return sdt;
}

sdt_const_vec_t DVBStreamData::GetCachedSDTs(bool current) const
{
    QMutexLocker locker(&_cache_lock);

    if (!current)
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Currently we ignore \'current\' param");

    sdt_const_vec_t sdts;

    sdt_cache_t::const_iterator it = _cached_sdts.begin();
    for (; it != _cached_sdts.end(); ++it)
    {
        IncrementRefCnt(*it);
        sdts.push_back(*it);
    }

    return sdts;
}

void DVBStreamData::ReturnCachedSDTTables(sdt_const_vec_t &sdts) const
{
    for (sdt_const_vec_t::iterator it = sdts.begin(); it != sdts.end(); ++it)
        ReturnCachedTable(*it);
    sdts.clear();
}

bool DVBStreamData::DeleteCachedTableSection(PSIPTable *psip) const
{
    if (!psip)
        return false;

    uint tableID = psip->TableID();
    uint tableIDExtentension = psip->TableIDExtension();

    QMutexLocker locker(&_cache_lock);
    if (_cached_ref_cnt[psip] > 0)
    {
        _cached_slated_for_deletion[psip] = 1;
        return false;
    }
    else if ((TableID::NIT == tableID) &&
             _cached_nit[psip->Section()])
    {
        _cached_nit[psip->Section()] = NULL;
        delete psip;
    }
    else if ((TableID::SDT == tableID) &&
             _cached_sdts[tableIDExtentension << 8 | psip->Section()])
    {
        _cached_sdts[tableIDExtentension << 8 | psip->Section()] = NULL;
        delete psip;
    }
    else if (DVBEventInformationTable::IsEIT(tableID))
    {
        DVBEventInformationTable* eit = (DVBEventInformationTable*)(psip);
        uint onid = eit->OriginalNetworkID();
        uint tsid = eit->TSID();
        uint sid = eit->ServiceID();
        uint tid = eit->TableID();
        uint section= eit->Section();
        if (onid== 0x233a)
            onid = GenerateUniqueUKOriginalNetworkID(tsid);
        eit_sections_cache_t& sections = _cached_eits[onid]
                                                      [tsid]
                                                       [sid]
                                                        [tid];
        if (sections[section])
        {
            sections[section] = NULL;
            delete psip;
        }
    }
    else
    {
        return MPEGStreamData::DeleteCachedTableSection(psip);
    }
    psip_refcnt_map_t::iterator it;
    it = _cached_slated_for_deletion.find(psip);
    if (it != _cached_slated_for_deletion.end())
        _cached_slated_for_deletion.erase(it);

    return true;
}

void DVBStreamData::CacheNIT(nit_ptr_t nit)
{
    QMutexLocker locker(&_cache_lock);

    nit_cache_t::iterator it = _cached_nit.find(nit->Section());
    if (it != _cached_nit.end())
        DeleteCachedTableSection(*it);

    _cached_nit[nit->Section()] = nit;
}

void DVBStreamData::CacheSDT(sdt_ptr_t sdt)
{
    uint key = (sdt->TSID() << 8) | sdt->Section();

    QMutexLocker locker(&_cache_lock);

    sdt_cache_t::iterator it = _cached_sdts.find(key);
    if (it != _cached_sdts.end())
        DeleteCachedTableSection(*it);

    _cached_sdts[key] = sdt;
}

void DVBStreamData::CacheEIT(eit_ptr_t eit)
{
    uint onid = eit->OriginalNetworkID();
    uint tsid = eit->TSID();
    uint sid = eit->ServiceID();
    uint tid = eit->TableID();
    uint sec = eit->Section();
 
    if (onid == 0x233a)
    	onid = GenerateUniqueUKOriginalNetworkID(tsid);

    QMutexLocker locker(&_cache_lock);

    eit_sections_cache_t& sections = _cached_eits[onid]
												  [tsid]
												   [sid]
												    [tid];

    eit_sections_cache_t::iterator it = sections.find(sec);

    if (it != sections.end())
        DeleteCachedTableSection(*it);

    sections[sec] = eit;
    LOG(VB_EITDVBCACHE, LOG_DEBUG, LOC + QString(
                        "Added eit cache entry 0x%1/0x%2/0x%3/0x%4/%5 count %6")
                        .arg(onid,4,16)
                        .arg(tsid,4,16)
                        .arg(sid,4,16)
                        .arg(tid,2,16)
                        .arg(sec)
                        .arg(sections.size()));
}

void DVBStreamData::ValidateEITCache()
{
    LOG(VB_EITDVBCACHE, LOG_DEBUG, LOC + QString(
            "Validate EIT cache top level entries %1")
                        .arg(_cached_eits.size()));
    for (eit_stssn_cache_t::iterator network = _cached_eits.begin();
            network != _cached_eits.end(); ++network)
    {
        LOG(VB_EITDVBCACHE, LOG_DEBUG, LOC + QString(
                "ONID 0x%1")
                .arg(network.key(),4,16));
        for (eit_stss_cache_t::iterator stream = (*network).begin();
                stream != (*network).end(); ++stream)
        {
            LOG(VB_EITDVBCACHE, LOG_DEBUG, LOC + QString(
                    "TSID 0x%1")
                    .arg(stream.key(),4,16));
            for (eit_sts_cache_t::iterator service = (*stream).begin();
                    service != (*stream).end(); ++service)
            {
                LOG(VB_EITDVBCACHE, LOG_DEBUG, LOC + QString(
                        "SID 0x%1")
                        .arg(service.key(),4,16));
                for (eit_st_cache_t::iterator table = (*service).begin(); table != (*service).end(); ++table)
                {
                    LOG(VB_EITDVBCACHE, LOG_DEBUG, LOC + QString(
                            "TID 0x%1")
                            .arg(table.key(),2,16));
                    for (eit_sections_cache_t::iterator section = (*table).begin();
                            section != (*table).end(); ++section)
                    {
                        LOG(VB_EITDVBCACHE, LOG_DEBUG, LOC + QString(
                                "Section %1 value 0x%2")
                                .arg(section.key())
                                .arg(uint64_t(section.value()),0,16));
                    }
                }
            }
        }
    }
    LOG(VB_EITDVBCACHE|VB_FLUSH, LOG_DEBUG, LOC + QString(
            "============================================="));
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
