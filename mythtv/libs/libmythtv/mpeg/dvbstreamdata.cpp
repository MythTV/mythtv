// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include <algorithm>
#include <iostream>
using namespace std;

#include "qsharedpointer.h"
#include "qcryptographichash.h"
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

/// \remarks The time in seconds after which an EIT table is
// considered stale and the event hash needs to be checked.
qint64 DVBStreamData::EIT_STALE_TIME = 600;

/// \remarks The time in seconds after which an SDT table is
// considered stale.
qint64 DVBStreamData::SDT_STALE_TIME = 3 * 60 * 60;

eit_tssn_cache_t  DVBStreamData::_cached_eits;
QMutex            DVBStreamData::_cached_eits_lock;
sdt_tsn_cache_t   DVBStreamData::_cached_sdts;
QMutex            DVBStreamData::_cached_sdts_lock;

EitCache::~EitCache()
{
	unsigned long bytes = 0;
	uint sections = 0;

	// Tidy up for heap debugging
    for (eit_tssn_cache_t::iterator network = begin();
            network != end(); ++network)
    {
        cerr << QString(
                "ONID 0x%1")
                .arg(network.key(),4,16).toStdString() << endl;
        for (eit_tss_cache_t::iterator stream = (*network).begin();
                stream != (*network).end(); ++stream)
        {
        	cerr << QString(
                    "TSID 0x%1")
                    .arg(stream.key(),4,16).toStdString() << endl;
            for (eit_ts_cache_t::iterator service = (*stream).begin();
                    service != (*stream).end(); ++service)
            {
            	cerr << QString(
                        "SID 0x%1")
                        .arg(service.key(),4,16).toStdString() << endl;
                for (eit_t_cache_t::iterator table = (*service).begin(); table != (*service).end(); ++table)
                {
                	cerr << QString(
                            "TID 0x%1 Sections %2 Status %3")
                            .arg(table.key(),2,16)
							.arg((*table).sections.size())
							.arg((*table).status.HasAllSections() ? "complete" : "incomplete")
							.toStdString()
							<< endl;
                    for (eit_sections_cache_t::iterator section = (*table).sections.begin();
                            section != (*table).sections.end(); ++section)
                    {
                    	if (NULL != *section)
                    	{
                    		bytes += (*section)->SectionLength();
                    		delete *section;
                    	}
                    	sections++;
                    }
                    (*table).sections.clear();
                }
                (*service).clear();
            }
            (*stream).clear();
        }
        (*network).clear();
    }
    clear();
    cerr << "EIT cache deleted approximately "
    		<< bytes
			<<" bytes in "
			<< sections
			<< " sections"
			<< endl;
}

SdtCache::~SdtCache()
{
	unsigned long bytes  = 0;
	uint sections = 0;

	// Tidy up for heap debugging
    for (sdt_tsn_cache_t::iterator network = begin();
            network != end(); ++network)
    {
        cerr << QString(
                "ONID 0x%1")
                .arg(network.key(),4,16).toStdString() << endl;
        for (sdt_ts_cache_t::iterator stream = (*network).begin(); stream != (*network).end(); ++stream)
        {
        	cerr << QString(
                    "TSID 0x%1")
                    .arg(stream.key(),4,16).toStdString() << endl;
            for (sdt_t_cache_t::iterator table = (*stream).begin(); table != (*stream).end(); ++table)
            {
            	cerr << QString(
                        "TID 0x%1 Sections %2 Status %3")
                        .arg(table.key(),2,16)
						.arg((*table).sections.size())
						.arg((*table).status.HasAllSections() ? "complete" : "incomplete")
						.toStdString() << endl;
				for (sdt_sections_cache_t::iterator section = (*table).sections.begin();
						section != (*table).sections.end(); ++section)
				{
                	if (NULL != *section)
                	{
                		bytes += (*section)->SectionLength();
                		delete *section;
                	}
                   	sections++;
				}
                (*table).sections.clear();
            }
            (*stream).clear();
        }
        (*network).clear();
    }
    clear();
    cerr << "SDT cache deleted approximately "
    		<< bytes
			<<" bytes in "
			<< sections
			<< " entries"
			<< endl;
}

// service_id is synonymous with the MPEG program number in the PMT.
DVBStreamData::DVBStreamData(uint desired_netid,  uint desired_tsid,
                             int desired_program, int cardnum, bool cacheTableSections)
    : MPEGStreamData(desired_program, cardnum, cacheTableSections),
      _desired_netid(desired_netid), _desired_tsid(desired_tsid),
      _dvb_real_network_id(-1), _dvb_eit_dishnet_long(false)
{
    LOG(VB_DVBSICACHE, LOG_DEBUG, LOC + QString("Constructing DvbStreamData %1")
        .arg(reinterpret_cast<std::uintptr_t>(this),0,16));
    _nito_status.SetVersion(-1,0);
    AddListeningPID(DVB_NIT_PID);
    AddListeningPID(DVB_SDT_PID);
    AddListeningPID(DVB_RST_PID);
    AddListeningPID(DVB_TDT_PID);
}

DVBStreamData::~DVBStreamData()
{
    LOG(VB_DVBSICACHE, LOG_DEBUG, LOC + QString("Destructing DvbStreamData %1")
        .arg(reinterpret_cast<std::uintptr_t>(this),0,16));
    Reset(_desired_netid, _desired_tsid, _desired_program);

    QMutexLocker locker(&_listener_lock);
    _dvb_main_listeners.clear();
    _dvb_other_listeners.clear();
    _dvb_eit_listeners.clear();
    _dvb_has_eit.clear();
}

void DVBStreamData::SetDesiredService(uint netid, uint tsid, int serviceid)
{
    if ((_desired_netid == netid) && (_desired_tsid == tsid))
        SetDesiredProgram(serviceid);
    else
        Reset(netid, tsid, serviceid);
}

void DVBStreamData::CheckStaleEIT(const DVBEventInformationTableSection& eit, uint onid, uint tsid, uint sid, uint tid) const
{
    uint section = eit.Section();

    onid = InternalOriginalNetworkID(onid, tsid);

    if (!(_cached_eits.contains(onid) &&
    		_cached_eits[onid].contains(tsid) &&
			_cached_eits[onid][tsid].contains(sid) &&
			_cached_eits[onid][tsid][sid].contains(tid)))
    	return; // Table is not cached

    // Check if table is stale
    eit_sections_cache_wrapper_t& wrapper = _cached_eits[onid][tsid][sid][tid];

    if (wrapper.timestamp.secsTo(QDateTime::currentDateTimeUtc())
                > EIT_STALE_TIME)
    {
        LOG(VB_DVBSICACHE, LOG_DEBUG, LOC + QString("Table 0x%1/0x%2/0x%3/0x%4 section %5 check EIT hash")
            .arg(onid,0,16).arg(tsid,0,16).arg(sid,0,16).arg(tid,0,16).arg(section));
        // Check the hash
        if (QCryptographicHash::hash(QByteArray((const char*)(eit.psipdata()),
                                                eit.Length() - 4),
                                     QCryptographicHash::Md5)
                != wrapper.hashes[section])
        {
            // Table is stale. Remove it from the cache.
            LOG(VB_DVBSICACHE, LOG_DEBUG, LOC + QString("Table 0x%1/0x%2/0x%3/0x%4 is stale - removing")
                .arg(onid,0,16).arg(tsid,0,16).arg(sid,0,16).arg(tid,0,16));
            for (eit_sections_cache_t::iterator section = wrapper.sections.begin();
                    section != wrapper.sections.end(); ++section)
                DeleteCachedTableSection(*section);
            wrapper.sections.clear();
            wrapper.hashes.clear();
        }
        else
        {
        	//  Table is not stale probable duplicate section
        	// reset the time stamp
        	wrapper.timestamp = QDateTime::currentDateTimeUtc();
        }
    }
}

void DVBStreamData::CheckStaleSDT(const ServiceDescriptionTableSection& sdt, uint onid, uint tsid, uint tid) const
{
    uint section = sdt.Section();

    onid = InternalOriginalNetworkID(onid, tsid);

    if (!(_cached_sdts.contains(onid) &&
    		_cached_sdts[onid].contains(tsid) &&
			_cached_sdts[onid][tsid].contains(tid)))
    	return; // Table is not cached

    // Check if table is stale
    sdt_sections_cache_wrapper_t& wrapper = _cached_sdts[onid][tsid][tid];

    if ((wrapper.timestamp.secsTo(QDateTime::currentDateTimeUtc()))
                > SDT_STALE_TIME)
    {
        LOG(VB_DVBSICACHE, LOG_DEBUG, LOC + QString("Table 0x%1/0x%2/0x%3 section %4 check SDT hash")
            .arg(onid).arg(tsid).arg(tid).arg(section));
        // Check the hash
        if (QCryptographicHash::hash(QByteArray((const char*)(sdt.psipdata()),
                                                sdt.Length() - 4),
                                     QCryptographicHash::Md5)
                != wrapper.hashes[section])
        {
            // Table is stale. Remove it from the cache.
            LOG(VB_DVBSICACHE, LOG_DEBUG, LOC + QString("Table 0x%1/0x%2/0x%3 is stale - removing")
                .arg(onid).arg(tsid).arg(tid));
            for (sdt_sections_cache_t::iterator section = wrapper.sections.begin();
                    section != wrapper.sections.end(); ++section)
                DeleteCachedTableSection(*section);
            wrapper.sections.clear();
            wrapper.hashes.clear();
        }
        else
        {
        	//  Table is not stale probable duplicate section
        	// reset the time stamp
        	wrapper.timestamp = QDateTime::currentDateTimeUtc();
        }
    }
}

/** \fn DVBStreamData::IsRedundant(uint,const PSIPTable&) const
 *  \brief Returns true if table section already seen.
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
        ServiceDescriptionTableSection sdt(psip);

        uint onid = sdt.OriginalNetworkID();
        uint tsid = sdt.TSID();

        CheckStaleSDT(sdt, onid, tsid, table_id);

        return SDTSectionSeen(sdt);
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
        DVBEventInformationTableSection eit(psip);

        uint onid = eit.OriginalNetworkID();
        uint tsid = eit.TSID();
        uint sid = eit.ServiceID();

        CheckStaleEIT(eit, onid, tsid, sid, table_id);

        return EITSectionSeen(eit);
    }

    ////////////////////////////////////////////////////////////////////////
    // Other transport tables

    if (TableID::NITo == table_id)
    {
        return _nito_status.IsSectionSeen(version, psip.Section());
    }

    if (TableID::SDTo == table_id)
    {
        ServiceDescriptionTableSection sdt(psip);

        uint onid = sdt.OriginalNetworkID();
        uint tsid = sdt.TSID();

        CheckStaleSDT(sdt, onid, tsid, table_id);

        return SDTSectionSeen(sdt);
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
        DVBEventInformationTableSection eit(psip);

        uint onid = eit.OriginalNetworkID();
        uint tsid = eit.TSID();
        uint sid = eit.ServiceID();

        CheckStaleEIT(eit, onid, tsid, sid, table_id);

        return EITSectionSeen(eit);
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
    _cit_status.clear();

    _nito_status.SetVersion(-1,0);
    _bat_status.clear();

    {
        QMutexLocker locker(&_cache_lock);

        nit_vec_t nit_sections;;
        for (nit_cache_t::iterator nit = _cached_nit.begin();
                nit != _cached_nit.end(); ++nit)
            nit_sections.push_back(*nit);
        for (nit_vec_t::iterator nit = nit_sections.begin();
                nit != nit_sections.end(); ++nit)
            DeleteCachedTableSection(*nit);
        _cached_nit.clear();
    }

    {
        QMutexLocker locker(&_cached_sdts_lock);
        for (sdt_tsn_cache_t::iterator network = _cached_sdts.begin();
                network != _cached_sdts.end();)
        {
            for (sdt_ts_cache_t::iterator stream = (*network).begin(); stream != (*network).end();)
            {
                for (sdt_t_cache_t::iterator table = (*stream).begin(); table != (*stream).end();)
                {
                    if (!(*table).status.HasAllSections())
                    {
                        // Reset any incomplete tables
                        for (sdt_sections_cache_t::iterator section = (*table).sections.begin();
                                section != (*table).sections.end(); ++section)
                            DeleteCachedTableSection(*section);
                        table = (*stream).erase(table);
                    }
                    else
                    	++table;
                }
                if ((*stream).isEmpty())
                	stream = (*network).erase(stream);
                else
                	++stream;
            }
            if ((*network).isEmpty())
            	network = _cached_sdts.erase(network);
            else
            	network++;
        }
    }

    {
        QMutexLocker locker(&_cached_eits_lock);
        for (eit_tssn_cache_t::iterator network = _cached_eits.begin();
                network != _cached_eits.end();)
        {
            for (eit_tss_cache_t::iterator stream = (*network).begin();
                    stream != (*network).end();)
            {
                for (eit_ts_cache_t::iterator service = (*stream).begin();
                        service != (*stream).end();)
                {
                    for (eit_t_cache_t::iterator table = (*service).begin(); table != (*service).end();)
                    {
                        // Reset any incomplete tables
                        if (!(*table).status.HasAllSections())
                        {
							for (eit_sections_cache_t::iterator section = (*table).sections.begin();
									section != (*table).sections.end(); ++section)
								DeleteCachedTableSection(*section);
	                        table = (*service).erase(table);
	                    }
	                    else
	                    	++table;
                    }
                    if ((*service).isEmpty())
                    	service = (*stream).erase(service);
                    else
                    	service++;
                }
                if ((*stream).isEmpty())
                	stream = (*network).erase(stream);
                else
                	stream++;
            }
            if ((*network).isEmpty())
            	network = _cached_eits.erase(network);
            else
            	network++;
        }
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
        case TableID::SDTo:
        {
            ServiceDescriptionTableSection *sdtsection = new ServiceDescriptionTableSection(psip);
            ProcessSDTSection(sdtsection);
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
/*
        case TableID::SDTo:
        {
            ServiceDescriptionTableSection *sdtsection= new ServiceDescriptionTableSection(psip);
          // some providers send the SDT for the current multiplex as SDTo
            // this routine changes the TableID to SDT and recalculates the CRC
            if (_desired_netid == sdt.OriginalNetworkID() &&
                _desired_tsid  == tsid)
            {
                ServiceDescriptionTableSection *sdta =
                    new ServiceDescriptionTableSection(psip);
                if (!sdta->Mutate())
                {
                    delete sdta;
                    return true;
                }
                SetSDTSectionSeen(*sdta);
                ProcessSDT(sdta);
                return true;
            }

            ProcessSDTSection(sdtsection);

            return true;
        }
*/
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

        DVBEventInformationTableSection::IsEIT(psip.TableID()))
    {
        QMutexLocker locker(&_listener_lock);
        if (!_dvb_eit_listeners.size() && !_eit_helper)
            return true;

        DVBEventInformationTableSection* eit_section = new DVBEventInformationTableSection(psip);

        uint onid = eit_section->OriginalNetworkID();
        uint tsid = eit_section->TSID();
        uint sid = eit_section->ServiceID();
        uint tid = eit_section->TableID();
        uint version = eit_section->Version();
        uint section_number = eit_section->Section();
        uint segment_last_section = eit_section->SegmentLastSectionNumber();
        uint last_section = eit_section->LastSection();

        LOG(VB_DVBSICACHE, LOG_DEBUG, LOC + QString("New EIT section "
                "0x%1(0x%2)/0x%3/0x%4/%5/%6 %7(%8/%9) %10(%11/%12) %13(%14/%15)")
            .arg(onid,0,16)
            .arg(InternalOriginalNetworkID(onid, tsid),0,16)
            .arg(tsid,0,16)
            .arg(sid,0,16)
            .arg(tid)
            .arg(version)
            .arg(section_number)
            .arg(section_number >> 3)
            .arg(section_number % 8)
            .arg(segment_last_section)
            .arg(segment_last_section >> 3)
            .arg(segment_last_section % 8)
            .arg(last_section)
            .arg(last_section >> 3)
            .arg(last_section % 8)
            );

        SetEITSectionSeen(*eit_section);

        for (uint i = 0; i < _dvb_eit_listeners.size(); i++)
            _dvb_eit_listeners[i]->HandleEIT(eit_section);

        if(HasAllEITSections(*eit_section))
        {
        	// Translate the onid
            onid = InternalOriginalNetworkID(onid, tsid);

            LOG(VB_DVBSICACHE, LOG_DEBUG, LOC + QString(
                    "Subtable 0x%1/0x%2/0x%3/%4 complete version %5")
                .arg(onid,0,16)
                .arg(tsid,0,16)
                .arg(sid,0,16)
                .arg(tid)
                .arg(version));

            // Complete table seen
            // Grab the eit cache lock
            QMutexLocker locker(&_cached_eits_lock);

            eit_sections_cache_t& sections = _cached_eits[onid][tsid][sid][tid].sections;

            if (_eit_helper)
            	_eit_helper->AddEIT(sections);

            // Delete the table sections from the cache
            // But leave the status information intact
            for (eit_sections_cache_t::iterator i = sections.begin(); i != sections.end(); ++i)
                DeleteCachedTableSection(*i);

            sections.clear();
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

void DVBStreamData::ProcessSDTSection(sdt_section_ptr_t sdtsection)
{
    QMutexLocker locker(&_listener_lock);

    for (uint i = 0; i < sdtsection->ServiceCount(); i++)
    {
        /*
         * FIXME always signal EIT presence. We filter later. To many
         * networks set these flags wrong.
         * This allows the user to simply set useonairguide on a
         * channel manually.
         */
#if 0
        if (sdtsection->HasEITSchedule(i) || sdtsection->HasEITPresentFollowing(i))
#endif
            _dvb_has_eit[sdtsection->ServiceID(i)] = true;
    }

    uint tsid = sdtsection->TSID();
    uint onid = InternalOriginalNetworkID(sdtsection->OriginalNetworkID(), tsid);
    uint tid = sdtsection->TableID();

    SetSDTSectionSeen(*sdtsection);

    if(HasAllSDTSections(*sdtsection))
    {
        // Complete table seen
        // Grab the eit cache lock
        QMutexLocker locker(&_cached_sdts_lock);

        // Build a vector of the cached table sections
        sdt_sections_cache_t& sections = _cached_sdts[onid]
													  [tsid]
													   [tid].sections;

        // and pass up to the sdt listeners
        if (TableID::SDT == tid)
            for (uint i = 0; i < _dvb_main_listeners.size(); i++)
                _dvb_main_listeners[i]->HandleSDT(sections);
        else
            for (uint i = 0; i < _dvb_other_listeners.size(); i++)
               _dvb_other_listeners[i]->HandleSDTo(sections);


        // Delete the table sections from the cache
        // But leave the status information intact
        for (sdt_sections_cache_t::iterator i = sections.begin(); i != sections.end(); ++i)
            DeleteCachedTableSection(*i);
        sections.clear();
    }
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

bool DVBStreamData::HasAllBATSections(uint bid) const
{
    return _bat_status.HasAllSections(bid);
}

void DVBStreamData::SetEITSectionSeen(DVBEventInformationTableSection& eit_section)
{
    uint transport_stream_id = eit_section.TSID();
    uint original_network_id = InternalOriginalNetworkID(eit_section.OriginalNetworkID(), transport_stream_id);
    uint serviceid = eit_section.ServiceID();
    uint tableid = eit_section.TableID();
    uint version = eit_section.Version();
    uint section_number = eit_section.Section();
    uint segment_last_section = eit_section.SegmentLastSectionNumber();
    uint last_section = eit_section.LastSection();

    QMutexLocker locker(&_cached_eits_lock);

    // This will create the index entries if needed
    eit_sections_cache_wrapper_t& wrapper = _cached_eits[original_network_id][transport_stream_id]
                                                            [serviceid][tableid];
    TableStatus& status = wrapper.status;
	eit_sections_cache_t& sections = wrapper.sections;

    QString bitmap;
    uint count = 1;
    for (TableStatus::sections_t::const_iterator iTr
                = status.m_sections.begin();
            iTr != status.m_sections.end();
            iTr++)
    {
        bitmap.append(QString("%1 ").arg(*iTr, 8, 2, QChar('0')));
        if (!(count % 8))
            bitmap.append(QString("\n"));
        count++;
    }

    LOG(VB_DVBSICACHE, LOG_DEBUG, LOC + QString(
            "Subtable 0x%1/0x%2/0x%3/%4 version %5 section %6(%7/%8) "
            "sls %9(%10/%11) ls %12(%13/%14) - old map\n"
            "%15")
        .arg(original_network_id, 0, 16)
        .arg(transport_stream_id, 0, 16)
        .arg(serviceid, 0, 16)
        .arg(tableid)
        .arg(version)
        .arg(section_number)
        .arg(section_number >> 3)
        .arg(section_number % 8)
        .arg(segment_last_section)
        .arg(segment_last_section >> 3)
        .arg(segment_last_section % 8)
        .arg(last_section)
        .arg(last_section >> 3)
        .arg(last_section % 8)
        .arg(bitmap));

    status.SetSectionSeen(version, section_number, last_section, segment_last_section);

    for (TableStatus::sections_t::const_iterator iTr
                = status.m_sections.begin();
            iTr != status.m_sections.end();
            iTr++)
    {
        bitmap.append(QString("%1 ").arg(*iTr, 8, 2, QChar('0')));
        if (!(count % 8))
            bitmap.append(QString("\n"));
        count++;
    }

    LOG(VB_DVBSICACHE, LOG_DEBUG, LOC + QString(
            "Subtable 0x%1/0x%2/0x%3/%4 version %5 section %6(%7/%8) "
            "sls %9(%10/%11) ls %12(%13/%14) - new map\n"
            "%15")
        .arg(original_network_id, 0, 16)
        .arg(transport_stream_id, 0, 16)
        .arg(serviceid, 0, 16)
        .arg(tableid)
        .arg(version)
        .arg(section_number)
        .arg(section_number >> 3)
        .arg(section_number % 8)
        .arg(segment_last_section)
        .arg(segment_last_section >> 3)
        .arg(segment_last_section % 8)
        .arg(last_section)
        .arg(last_section >> 3)
        .arg(last_section % 8)
        .arg(bitmap));

    eit_sections_cache_t::iterator it = sections.find(section_number);

    if (it != sections.end())
        DeleteCachedTableSection(*it);

    sections[section_number] = &eit_section;
    wrapper.timestamp = QDateTime::currentDateTimeUtc();
    wrapper.hashes[section_number] = QCryptographicHash::hash(
            QByteArray((const char*)(eit_section.psipdata()), eit_section.Length() - 4),
            QCryptographicHash::Md5); // Hash the events loop

    LOG(VB_DVBSICACHE, LOG_DEBUG, LOC + QString(
                        "Added eit cache entry 0x%1/0x%2/0x%3/0x%4/%5 count %6")
                        .arg(original_network_id,4,16)
                        .arg(transport_stream_id,4,16)
                        .arg(serviceid,4,16)
                        .arg(tableid,2,16)
                        .arg(section_number)
                        .arg(sections.size()));
}

bool DVBStreamData::EITSectionSeen(const DVBEventInformationTableSection& eit_section) const
{
    uint transport_stream_id = eit_section.TSID();
    uint original_network_id = InternalOriginalNetworkID(eit_section.OriginalNetworkID(), transport_stream_id);
    uint serviceid = eit_section.ServiceID();
    uint tableid = eit_section.TableID();
    uint version = eit_section.Version();
    uint section = eit_section.Section();

    QMutexLocker locker(&_cached_eits_lock);

    if (_cached_eits.contains(original_network_id) &&
    		_cached_eits[original_network_id].contains(transport_stream_id) &&
			_cached_eits[original_network_id][transport_stream_id].contains(serviceid) &&
			_cached_eits[original_network_id][transport_stream_id][serviceid].contains(tableid))
    	return _cached_eits[original_network_id]
						[transport_stream_id]
						 [serviceid]
						  [tableid].status.IsSectionSeen(version, section);
    else
    	return false;
}

bool DVBStreamData::HasAllEITSections(const DVBEventInformationTableSection& eit_section) const
{
    uint transport_stream_id = eit_section.TSID();
    uint original_network_id = InternalOriginalNetworkID(eit_section.OriginalNetworkID(), transport_stream_id);
    uint serviceid = eit_section.ServiceID();
    uint tableid = eit_section.TableID();

    QMutexLocker locker(&_cached_eits_lock);

    return (_cached_eits.contains(original_network_id) &&
        _cached_eits[original_network_id].contains(transport_stream_id) &&
        _cached_eits[original_network_id][transport_stream_id].contains(serviceid) &&
        _cached_eits[original_network_id][transport_stream_id][serviceid].contains(tableid) &&
        _cached_eits[original_network_id][transport_stream_id][serviceid][tableid].status.HasAllSections() &&
        !_cached_eits[original_network_id][transport_stream_id][serviceid][tableid].sections.isEmpty());
}

void DVBStreamData::SetSDTSectionSeen(ServiceDescriptionTableSection& sdt_section)
{
    uint transport_stream_id = sdt_section.TSID();
    uint original_network_id = InternalOriginalNetworkID(sdt_section.OriginalNetworkID(), transport_stream_id);
    uint tableid = sdt_section.TableID();
    uint version = sdt_section.Version();
    uint section_number = sdt_section.Section();
    uint last_section = sdt_section.LastSection();

    QMutexLocker locker(&_cached_sdts_lock);

    // This will create the indexes if needed
	sdt_sections_cache_wrapper_t& wrapper = _cached_sdts[original_network_id]
                                      [transport_stream_id]
                                      [tableid];

    wrapper.status.SetSectionSeen(version, section_number, last_section);

    sdt_sections_cache_t& sections = wrapper.sections;

    sdt_sections_cache_t::iterator it = sections.find(section_number);

    if (it != sections.end())
        DeleteCachedTableSection(*it);

    sections[section_number] = &sdt_section;
    wrapper.timestamp = QDateTime::currentDateTimeUtc();
    wrapper.hashes[section_number] = QCryptographicHash::hash(
            QByteArray((const char*)(sdt_section.psipdata()), sdt_section.Length() - 4),
            QCryptographicHash::Md5); // Hash the events loop
}

bool DVBStreamData::SDTSectionSeen(const ServiceDescriptionTableSection& sdt_section) const
{
    uint transport_stream_id = sdt_section.TSID();
    uint original_network_id = InternalOriginalNetworkID(sdt_section.OriginalNetworkID(), transport_stream_id);
    uint tableid = sdt_section.TableID();
    uint version = sdt_section.Version();
    uint section = sdt_section.Section();

    QMutexLocker locker(&_cached_sdts_lock);

    if (_cached_sdts.contains(original_network_id) &&
    		_cached_sdts[original_network_id].contains(transport_stream_id) &&
			_cached_sdts[original_network_id][transport_stream_id].contains(tableid))
		return _cached_sdts[original_network_id]
							[transport_stream_id]
							[tableid].status.IsSectionSeen(version, section);
    else
    	return false;
}

bool DVBStreamData::HasAllSDTSections(const ServiceDescriptionTableSection& sdt_section) const
{
    uint transport_stream_id = sdt_section.TSID();
    uint original_network_id = InternalOriginalNetworkID(sdt_section.OriginalNetworkID(), transport_stream_id);
    uint tableid = sdt_section.TableID();

    QMutexLocker locker(&_cached_sdts_lock);

    if (_cached_sdts.contains(original_network_id) &&
    		_cached_sdts[original_network_id].contains(transport_stream_id) &&
			_cached_sdts[original_network_id]
						 [transport_stream_id].contains(tableid) &&
			_cached_sdts[original_network_id]
						 [transport_stream_id]
						  [tableid].status.HasAllSections() &&
			!_cached_sdts[original_network_id]
						 [transport_stream_id]
						  [tableid].sections.isEmpty())
    	return true;
    else
    	return false;
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

bool DVBStreamData::DeleteCachedTableSection(PSIPTable *psip) const
{
    if (!psip)
        return false;

    uint tableID = psip->TableID();

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
    else if ((TableID::SDT == tableID) || (TableID::SDTo == tableID))
    {
        ServiceDescriptionTableSection* sdt = (ServiceDescriptionTableSection*)(psip);
        uint tsid = sdt->TSID();
        uint onid = InternalOriginalNetworkID(sdt->OriginalNetworkID(), tsid);
        uint section= sdt->Section();
        sdt_sections_cache_t& sections = _cached_sdts[onid]
                                                      [tsid]
                                                       [tableID].sections;
        if (sections[section])
        {
            sections[section] = NULL;
            delete psip;
        }
    }
    else if (DVBEventInformationTableSection::IsEIT(tableID))
    {
        DVBEventInformationTableSection* eit = (DVBEventInformationTableSection*)(psip);
        uint tsid = eit->TSID();
        uint onid = InternalOriginalNetworkID(eit->OriginalNetworkID(), tsid);
        uint sid = eit->ServiceID();
        uint section= eit->Section();

        // It should be in the cache - maybe if it is not I should treat that as an error
        eit_sections_cache_t& sections = _cached_eits[onid]
                                                      [tsid]
                                                       [sid]
                                                        [tableID].sections;
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

void DVBStreamData::LogSICache()
{
    uint approximate_cached_sections_size = 0;
    LOG(VB_GENERAL, LOG_INFO, QString(
            "Validate SDT cache top level entries %1")
                        .arg(_cached_sdts.size()));
    for (sdt_tsn_cache_t::iterator network = _cached_sdts.begin();
            network != _cached_sdts.end(); ++network)
    {
        LOG(VB_GENERAL, LOG_INFO, QString(
                "ONID 0x%1")
                .arg(network.key(),4,16));
        for (sdt_ts_cache_t::iterator stream = (*network).begin();
                stream != (*network).end(); ++stream)
        {
            LOG(VB_GENERAL, LOG_INFO, QString(
                    "TSID 0x%1")
                    .arg(stream.key(),4,16));
            for (sdt_t_cache_t::iterator table = (*stream).begin(); table != (*stream).end(); ++table)
            {
                LOG(VB_GENERAL, LOG_INFO, QString(
                        "TID 0x%1 Sections %2 Status %3")
                        .arg(table.key(),2,16)
						.arg((*table).sections.size())
						.arg((*table).status.HasAllSections() ? "complete" : "incomplete")
						);
                for (sdt_sections_cache_t::iterator section = (*table).sections.begin();
                        section != (*table).sections.end(); ++section)
                {
                    LOG(VB_GENERAL, LOG_INFO, QString(
                            "Section %1 value 0x%2")
                            .arg(section.key())
                            .arg(uint64_t(section.value()),0,16));
                    approximate_cached_sections_size += (*section)->Length();
                }
            }
        }
    }
    LOG(VB_GENERAL, LOG_INFO, QString(
            "============================================="));
    LOG(VB_GENERAL, LOG_INFO, QString(
            "Approximate size of sdt section cache %1Mb")
    		.arg(approximate_cached_sections_size / (0x100000)));

    approximate_cached_sections_size = 0;
    LOG(VB_GENERAL, LOG_INFO, QString(
            "EIT cache top level entries %1")
                        .arg(_cached_eits.size()));
    for (eit_tssn_cache_t::iterator network = _cached_eits.begin();
            network != _cached_eits.end(); ++network)
    {
        LOG(VB_GENERAL, LOG_INFO, QString(
                "ONID 0x%1")
                .arg(network.key(),4,16));
        for (eit_tss_cache_t::iterator stream = (*network).begin();
                stream != (*network).end(); ++stream)
        {
            LOG(VB_GENERAL, LOG_INFO, QString(
                    "TSID 0x%1")
                    .arg(stream.key(),4,16));
            for (eit_ts_cache_t::iterator service = (*stream).begin();
                    service != (*stream).end(); ++service)
            {
                LOG(VB_GENERAL, LOG_INFO, QString(
                        "SID 0x%1")
                        .arg(service.key(),4,16));
                for (eit_t_cache_t::iterator table = (*service).begin(); table != (*service).end(); ++table)
                {
                    LOG(VB_GENERAL, LOG_INFO, QString(
                            "TID 0x%1 Sections %2 Status %3")
                            .arg(table.key(),2,16)
							.arg((*table).sections.size())
							.arg((*table).status.HasAllSections() ? "complete" : "incomplete")
							);
                    for (eit_sections_cache_t::iterator section = (*table).sections.begin();
                            section != (*table).sections.end(); ++section)
                    {
                        LOG(VB_GENERAL, LOG_INFO, QString(
                                "Section %1 value 0x%2")
                                .arg(section.key())
                                .arg(uint64_t(section.value()),0,16));
                        approximate_cached_sections_size += (*section)->Length();
                    }
                }
            }
        }
    }
    LOG(VB_GENERAL, LOG_INFO, QString(
            "============================================="));
    LOG(VB_GENERAL, LOG_INFO, QString(
            "Approximate size of eit section cache %1Mb")
    		.arg(approximate_cached_sections_size / (0x100000)));
    LOG(VB_FLUSH, LOG_INFO, "");
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
