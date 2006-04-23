// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "atscstreamdata.h"
#include "atsctables.h"
#include "RingBuffer.h"

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
 *  \param cacheTables         If true important tables will be cached.
 */
ATSCStreamData::ATSCStreamData(int desiredMajorChannel,
                               int desiredMinorChannel,
                               bool cacheTables)
    : MPEGStreamData(-1, cacheTables),
      _GPS_UTC_offset(GPS_LEAP_SECONDS),
      _mgt_version(-1),
      _cached_mgt(NULL),
      _desired_major_channel(desiredMajorChannel),
      _desired_minor_channel(desiredMinorChannel)
{
    AddListeningPID(ATSC_PSIP_PID);
}

ATSCStreamData::~ATSCStreamData()
{
    Reset(-1,-1);

    QMutexLocker locker(&_listener_lock);
    _atsc_main_listeners.clear();
    _atsc_aux_listeners.clear();
    _atsc_eit_listeners.clear();
}

void ATSCStreamData::SetDesiredChannel(int major, int minor)
{
    // TODO this should reset only if it can't regen from cached tables.
    _desired_major_channel = major;
    _desired_minor_channel = minor;
    _desired_program = -1;
}

void ATSCStreamData::Reset(int desiredMajorChannel, int desiredMinorChannel)
{
    _desired_major_channel = desiredMajorChannel;
    _desired_minor_channel = desiredMinorChannel;
    
    MPEGStreamData::Reset(-1);
    _mgt_version = -1;
    _tvct_version.clear();
    _cvct_version.clear();
    _eit_version.clear();
    _eit_section_seen.clear();

    { 
        QMutexLocker locker(&_cache_lock);

        DeleteCachedTable(_cached_mgt);
        _cached_mgt = NULL;

        tvct_cache_t::iterator tit = _cached_tvcts.begin();
        for (; tit != _cached_tvcts.end(); ++tit)
            DeleteCachedTable(*tit);
        _cached_tvcts.clear();

        cvct_cache_t::iterator cit = _cached_cvcts.begin();
        for (; cit != _cached_cvcts.end(); ++cit)
            DeleteCachedTable(*cit);
        _cached_cvcts.clear();
    }

    AddListeningPID(ATSC_PSIP_PID);
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
        if (VersionEIT(pid, psip.TableIDExtension()) != version)
            return false;
        return EITSectionSeen(pid, psip.TableIDExtension(), psip.Section());
    }

    if (TableID::ETT == table_id)
        return false; // retransmit ETT's we've seen

    if (TableID::STT == table_id)
        return false; // each SystemTimeTable matters

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
        return true; // we ignore RatingRegionTables

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
            if (_cache_tables)
            {
                MasterGuideTable *mgt = new MasterGuideTable(psip);
                CacheMGT(mgt);
                QMutexLocker locker(&_listener_lock);
                for (uint i = 0; i < _atsc_main_listeners.size(); i++)
                    _atsc_main_listeners[i]->HandleMGT(mgt);
            }
            else
            {
                MasterGuideTable mgt(psip);
                QMutexLocker locker(&_listener_lock);
                for (uint i = 0; i < _atsc_main_listeners.size(); i++)
                    _atsc_main_listeners[i]->HandleMGT(&mgt);
            }
            return true;
        }
        case TableID::TVCT:
        {
            uint tsid = psip.TableIDExtension();
            SetVersionTVCT(tsid, version);
            if (_cache_tables)
            {
                TerrestrialVirtualChannelTable *vct =
                    new TerrestrialVirtualChannelTable(psip); 
                CacheTVCT(pid, vct);
                QMutexLocker locker(&_listener_lock);
                for (uint i = 0; i < _atsc_main_listeners.size(); i++)
                    _atsc_main_listeners[i]->HandleVCT(tsid, vct);
                for (uint i = 0; i < _atsc_aux_listeners.size(); i++)
                    _atsc_aux_listeners[i]->HandleTVCT(tsid, vct);
            }
            else
            {
                TerrestrialVirtualChannelTable vct(psip);
                QMutexLocker locker(&_listener_lock);
                for (uint i = 0; i < _atsc_main_listeners.size(); i++)
                    _atsc_main_listeners[i]->HandleVCT(tsid, &vct);
                for (uint i = 0; i < _atsc_aux_listeners.size(); i++)
                    _atsc_aux_listeners[i]->HandleTVCT(tsid, &vct);
            }
            return true;
        }
        case TableID::CVCT:
        {
            uint tsid = psip.TableIDExtension();
            SetVersionCVCT(tsid, version);
            if (_cache_tables)
            {
                CableVirtualChannelTable *vct =
                    new CableVirtualChannelTable(psip);
                CacheCVCT(pid, vct);
                QMutexLocker locker(&_listener_lock);
                for (uint i = 0; i < _atsc_main_listeners.size(); i++)
                    _atsc_main_listeners[i]->HandleVCT(tsid, vct);
                for (uint i = 0; i < _atsc_aux_listeners.size(); i++)
                    _atsc_aux_listeners[i]->HandleCVCT(tsid, vct);
            }
            else
            {
                CableVirtualChannelTable vct(psip);
                QMutexLocker locker(&_listener_lock);
                for (uint i = 0; i < _atsc_main_listeners.size(); i++)
                    _atsc_main_listeners[i]->HandleVCT(tsid, &vct);
                for (uint i = 0; i < _atsc_aux_listeners.size(); i++)
                    _atsc_aux_listeners[i]->HandleCVCT(tsid, &vct);
            }
            return true;
        }
        case TableID::RRT:
        {
            RatingRegionTable rrt(psip);
            QMutexLocker locker(&_listener_lock);
            for (uint i = 0; i < _atsc_aux_listeners.size(); i++)
                _atsc_aux_listeners[i]->HandleRRT(&rrt);
            return true;
        }
        case TableID::EIT:
        {
            QMutexLocker locker(&_listener_lock);
            if (!_atsc_eit_listeners.size())
                return true;

            if (VersionEIT(pid, psip.TableIDExtension()) != version)
                SetVersionEIT(pid, psip.TableIDExtension(), version);
            SetEITSectionSeen(pid, psip.TableIDExtension(), psip.Section());

            EventInformationTable eit(psip);
            for (uint i = 0; i < _atsc_eit_listeners.size(); i++)
                _atsc_eit_listeners[i]->HandleEIT(pid, &eit);

            return true;
        }
        case TableID::ETT:
        {
            ExtendedTextTable ett(psip);

            QMutexLocker locker(&_listener_lock);
            for (uint i = 0; i < _atsc_eit_listeners.size(); i++)
                _atsc_eit_listeners[i]->HandleETT(pid, &ett);

            return true;
        }
        case TableID::STT:
        {
            SystemTimeTable stt(psip);

            // only update internal offset if it changes
            if (stt.GPSOffset() != _GPS_UTC_offset)
                _GPS_UTC_offset = stt.GPSOffset();

            for (uint i = 0; i < _atsc_main_listeners.size(); i++)
                _atsc_main_listeners[i]->HandleSTT(&stt);

            return true;
        }
        case TableID::DCCT:
        {
            DirectedChannelChangeTable dcct(psip);

            QMutexLocker locker(&_listener_lock);
            for (uint i = 0; i < _atsc_aux_listeners.size(); i++)
                _atsc_aux_listeners[i]->HandleDCCT(&dcct);

            return true;
        }
        case TableID::DCCSCT:
        {
            DirectedChannelChangeSelectionCodeTable dccsct(psip);

            QMutexLocker locker(&_listener_lock);
            for (uint i = 0; i < _atsc_aux_listeners.size(); i++)
                _atsc_aux_listeners[i]->HandleDCCSCT(&dccsct);

            return true;
        }
        default:
        {
            VERBOSE(VB_RECORD,
                    QString("ATSCStreamData::HandleTables(): Unknown "
                            "table 0x%1").arg(psip.TableID(),0,16));
            break;
        }
    }
    return false;
}

void ATSCStreamData::SetEITSectionSeen(uint pid, uint atsc_source_id,
                                       uint section)
{
    uint key = (pid<<16) | atsc_source_id;
    sections_map_t::iterator it = _eit_section_seen.find(key);
    if (it == _eit_section_seen.end())
    {
        _eit_section_seen[key].resize(32, 0);
        it = _eit_section_seen.find(key);
    }
    (*it)[section>>3] |= bit_sel[section & 0x7];
}

bool ATSCStreamData::EITSectionSeen(uint pid, uint atsc_source_id,
                                    uint section) const
{
    uint key = (pid<<16) | atsc_source_id;
    sections_map_t::const_iterator it = _eit_section_seen.find(key);
    if (it == _eit_section_seen.end())
        return false;
    return (bool) ((*it)[section>>3] & bit_sel[section & 0x7]);
}

bool ATSCStreamData::HasCachedMGT(bool current) const
{
    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    return (bool)(_cached_mgt);
}

bool ATSCStreamData::HasCachedTVCT(uint pid, bool current) const
{
    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    _cache_lock.lock();
    tvct_cache_t::const_iterator it = _cached_tvcts.find(pid);
    bool exists = (it != _cached_tvcts.end());
    _cache_lock.unlock();

    return exists;
}

bool ATSCStreamData::HasCachedCVCT(uint pid, bool current) const
{
    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    _cache_lock.lock();
    cvct_cache_t::const_iterator it = _cached_cvcts.find(pid);
    bool exists = (it != _cached_cvcts.end());
    _cache_lock.unlock();

    return exists;
}

bool ATSCStreamData::HasCachedAllTVCTs(bool current) const
{
    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    if (!_cached_mgt)
        return false;

    _cache_lock.lock();
    bool ret = true;
    for (uint i = 0; ret && (i < _cached_mgt->TableCount()); ++i)
    {
        if (TableClass::TVCTc == _cached_mgt->TableClass(i))
            ret &= HasCachedTVCT(_cached_mgt->TablePID(i));
    }
    _cache_lock.unlock();

    return ret;
}

bool ATSCStreamData::HasCachedAllCVCTs(bool current) const
{
    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    if (!_cached_mgt)
        return false;

    _cache_lock.lock();
    bool ret = true;
    for (uint i = 0; ret && (i < _cached_mgt->TableCount()); ++i)
    {
        if (TableClass::CVCTc == _cached_mgt->TableClass(i))
            ret &= HasCachedCVCT(_cached_mgt->TablePID(i));
    }
    _cache_lock.unlock();

    return ret;
}

const MasterGuideTable *ATSCStreamData::GetCachedMGT(bool current) const
{
    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    _cache_lock.lock();
    const MasterGuideTable *mgt = _cached_mgt;
    IncrementRefCnt(mgt);
    _cache_lock.unlock();

    return mgt;
}

const tvct_ptr_t ATSCStreamData::GetCachedTVCT(uint pid, bool current) const
{
    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    TerrestrialVirtualChannelTable *tvct = NULL;

    _cache_lock.lock();
    tvct_cache_t::const_iterator it = _cached_tvcts.find(pid);
    if (it != _cached_tvcts.end())
        IncrementRefCnt(tvct = *it);
    _cache_lock.unlock();

    return tvct;
}

const cvct_ptr_t ATSCStreamData::GetCachedCVCT(uint pid, bool current) const
{
    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    CableVirtualChannelTable *cvct = NULL;

    _cache_lock.lock();
    cvct_cache_t::const_iterator it = _cached_cvcts.find(pid);
    if (it != _cached_cvcts.end())
        IncrementRefCnt(cvct = *it);
    _cache_lock.unlock();

    return cvct;
}

tvct_vec_t ATSCStreamData::GetAllCachedTVCTs(bool current) const
{
    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    vector<const TerrestrialVirtualChannelTable*> tvcts;

    _cache_lock.lock();
    tvct_cache_t::const_iterator it = _cached_tvcts.begin();
    for (; it != _cached_tvcts.end(); ++it)
    {
        TerrestrialVirtualChannelTable* tvct = *it;
        IncrementRefCnt(tvct);
        tvcts.push_back(tvct);
    }
    _cache_lock.unlock();

    return tvcts;
}

cvct_vec_t ATSCStreamData::GetAllCachedCVCTs(bool current) const
{
    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    vector<const CableVirtualChannelTable*> cvcts;

    _cache_lock.lock();
    cvct_cache_t::const_iterator it = _cached_cvcts.begin();
    for (; it != _cached_cvcts.end(); ++it)
    {
        CableVirtualChannelTable* cvct = *it;
        IncrementRefCnt(cvct);
        cvcts.push_back(cvct);
    }
    _cache_lock.unlock();

    return cvcts;
}

void ATSCStreamData::CacheMGT(MasterGuideTable *mgt)
{
    QMutexLocker locker(&_cache_lock);

    DeleteCachedTable(_cached_mgt);
    _cached_mgt = mgt;
}

void ATSCStreamData::CacheTVCT(uint pid, TerrestrialVirtualChannelTable* tvct)
{
    QMutexLocker locker(&_cache_lock);

    DeleteCachedTable(_cached_tvcts[pid]);
    _cached_tvcts[pid] = tvct;
}

void ATSCStreamData::CacheCVCT(uint pid, CableVirtualChannelTable* cvct)
{
    QMutexLocker locker(&_cache_lock);

    DeleteCachedTable(_cached_cvcts[pid]);
    _cached_cvcts[pid] = cvct;
}

void ATSCStreamData::DeleteCachedTable(PSIPTable *psip) const
{
    if (!psip)
        return;

    QMutexLocker locker(&_cache_lock);
    if (_cached_ref_cnt[psip] > 0)
    {
        _cached_slated_for_deletion[psip] = 1;
        return;
    }
    else if (TableID::MGT == psip->TableID())
    {
        if (psip == _cached_mgt)
            _cached_mgt = NULL;
        delete psip;
    }
    else if ((TableID::TVCT == psip->TableID()) &&
             _cached_tvcts[psip->tsheader()->PID()])
    {
        _cached_tvcts[psip->tsheader()->PID()] = NULL;
        delete psip;
    }
    else if ((TableID::CVCT == psip->TableID()) &&
             _cached_cvcts[psip->tsheader()->PID()])
    {
        _cached_cvcts[psip->tsheader()->PID()] = NULL;
        delete psip;
    }
    else
    {
        MPEGStreamData::DeleteCachedTable(psip);
        return;
    }
    psip_refcnt_map_t::iterator it;
    it = _cached_slated_for_deletion.find(psip);
    if (it != _cached_slated_for_deletion.end())
        _cached_slated_for_deletion.erase(it);
}

void ATSCStreamData::ReturnCachedTVCTTables(tvct_vec_t &tvcts) const
{
    for (tvct_vec_t::iterator it = tvcts.begin(); it != tvcts.end(); ++it)
        ReturnCachedTable(*it);
    tvcts.clear();
}

void ATSCStreamData::ReturnCachedCVCTTables(cvct_vec_t &cvcts) const
{
    for (cvct_vec_t::iterator it = cvcts.begin(); it != cvcts.end(); ++it)
        ReturnCachedTable(*it);
    cvcts.clear();
}

void ATSCStreamData::AddATSCMainListener(ATSCMainStreamListener *val)
{
    QMutexLocker locker(&_listener_lock);

    atsc_main_listener_vec_t::iterator it = _atsc_main_listeners.begin();
    for (; it != _atsc_main_listeners.end(); ++it)
        if (((void*)val) == ((void*)*it))
            return;

    _atsc_main_listeners.push_back(val);
}

void ATSCStreamData::RemoveATSCMainListener(ATSCMainStreamListener *val)
{
    QMutexLocker locker(&_listener_lock);

    atsc_main_listener_vec_t::iterator it = _atsc_main_listeners.begin();
    for (; it != _atsc_main_listeners.end(); ++it)
    {
        if (((void*)val) == ((void*)*it))
        {
            _atsc_main_listeners.erase(it);
            return;
        }
    }
}

void ATSCStreamData::AddATSCAuxListener(ATSCAuxStreamListener *val)
{
    QMutexLocker locker(&_listener_lock);

    atsc_aux_listener_vec_t::iterator it = _atsc_aux_listeners.begin();
    for (; it != _atsc_aux_listeners.end(); ++it)
        if (((void*)val) == ((void*)*it))
            return;

    _atsc_aux_listeners.push_back(val);
}

void ATSCStreamData::RemoveATSCAuxListener(ATSCAuxStreamListener *val)
{
    QMutexLocker locker(&_listener_lock);

    atsc_aux_listener_vec_t::iterator it = _atsc_aux_listeners.begin();
    for (; it != _atsc_aux_listeners.end(); ++it)
    {
        if (((void*)val) == ((void*)*it))
        {
            _atsc_aux_listeners.erase(it);
            return;
        }
    }
}

void ATSCStreamData::AddATSCEITListener(ATSCEITStreamListener *val)
{
    QMutexLocker locker(&_listener_lock);

    atsc_eit_listener_vec_t::iterator it = _atsc_eit_listeners.begin();
    for (; it != _atsc_eit_listeners.end(); ++it)
        if (((void*)val) == ((void*)*it))
            return;

    _atsc_eit_listeners.push_back(val);
}

void ATSCStreamData::RemoveATSCEITListener(ATSCEITStreamListener *val)
{
    QMutexLocker locker(&_listener_lock);

    atsc_eit_listener_vec_t::iterator it = _atsc_eit_listeners.begin();
    for (; it != _atsc_eit_listeners.end(); ++it)
    {
        if (((void*)val) == ((void*)*it))
        {
            _atsc_eit_listeners.erase(it);
            return;
        }
    }
}
