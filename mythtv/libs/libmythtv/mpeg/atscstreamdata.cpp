// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson

#include <cmath>

#include <algorithm>
using namespace std;

#include "atscstreamdata.h"
#include "atsctables.h"
#include "RingBuffer.h"
#include "eithelper.h"

#define LOC QString("ATSCStream: ")

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
      _atsc_eit_reset(false),
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
    bool reset = true;
    const MasterGuideTable *mgt = GetCachedMGT();
    tvct_vec_t tvcts = GetAllCachedTVCTs();
    cvct_vec_t cvcts = GetAllCachedCVCTs();

    if (mgt && (tvcts.size() || cvcts.size()))
    {
        const TerrestrialVirtualChannelTable *tvct = NULL;
        const CableVirtualChannelTable       *cvct = NULL;
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
            _desired_major_channel = major;
            _desired_minor_channel = minor;

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

void ATSCStreamData::Reset(int major, int minor)
{
    _desired_major_channel = major;
    _desired_minor_channel = minor;
    
    MPEGStreamData::Reset(-1);
    _mgt_version = -1;
    _tvct_version.clear();
    _cvct_version.clear();
    _eit_version.clear();
    _eit_section_seen.clear();

    _sourceid_to_atscsrcid.clear();
    _atsc_eit_pids.clear();
    _atsc_ett_pids.clear();

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
            if (_cache_tables)
            {
                TerrestrialVirtualChannelTable *vct =
                    new TerrestrialVirtualChannelTable(psip); 
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
            if (_cache_tables)
            {
                CableVirtualChannelTable *vct =
                    new CableVirtualChannelTable(psip);
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
            RatingRegionTable rrt(psip);
            QMutexLocker locker(&_listener_lock);
            for (uint i = 0; i < _atsc_aux_listeners.size(); i++)
                _atsc_aux_listeners[i]->HandleRRT(&rrt);
            return true;
        }
        case TableID::EIT:
        {
            QMutexLocker locker(&_listener_lock);
            if (!_atsc_eit_listeners.size() && !_eit_helper)
                return true;

            if (VersionEIT(pid, psip.TableIDExtension()) != version)
                SetVersionEIT(pid, psip.TableIDExtension(), version);
            SetEITSectionSeen(pid, psip.TableIDExtension(), psip.Section());

            EventInformationTable eit(psip);
            for (uint i = 0; i < _atsc_eit_listeners.size(); i++)
                _atsc_eit_listeners[i]->HandleEIT(pid, &eit);

            const uint atscsrcid = GetATSCSRCID(eit.SourceID());
            if (atscsrcid && _eit_helper)
                _eit_helper->AddEIT(atscsrcid, &eit);
                
            return true;
        }
        case TableID::ETT:
        {
            ExtendedTextTable ett(psip);

            QMutexLocker locker(&_listener_lock);
            for (uint i = 0; i < _atsc_eit_listeners.size(); i++)
                _atsc_eit_listeners[i]->HandleETT(pid, &ett);

            if (ett.IsEventETM() && _eit_helper) // Guide ETTs
            {
                const uint atscsrcid = GetATSCSRCID(ett.SourceID());
                if (atscsrcid)
                    _eit_helper->AddETT(atscsrcid, &ett);
            }

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

            if (_eit_helper && GPSOffset() != _eit_helper->GetGPSOffset())
                _eit_helper->SetGPSOffset(GPSOffset());

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

bool ATSCStreamData::HasEITPIDChanges(const uint_vec_t &in_use_pids) const
{
    QMutexLocker locker(&_listener_lock);
    uint eit_count = (uint) round(_atsc_eit_pids.size() * _eit_rate);
    uint ett_count = (uint) round(_atsc_ett_pids.size() * _eit_rate);
    return (in_use_pids.size() != (eit_count + ett_count) || _atsc_eit_reset);
}

bool ATSCStreamData::GetEITPIDChanges(const uint_vec_t &cur_pids,
                                      uint_vec_t &add_pids,
                                      uint_vec_t &del_pids) const
{
    QMutexLocker locker(&_listener_lock);

    _atsc_eit_reset = false;

    uint eit_count = (uint) round(_atsc_eit_pids.size() * _eit_rate);
    uint ett_count = (uint) round(_atsc_ett_pids.size() * _eit_rate);
    uint i;

    atsc_eit_pid_map_t::const_iterator it = _atsc_eit_pids.begin();
    for (i = 0; it != _atsc_eit_pids.end() && (i < eit_count); (++it),(i++))
    {
        if (find(cur_pids.begin(), cur_pids.end(), *it) == cur_pids.end())
            add_pids.push_back(*it);
    }

    atsc_ett_pid_map_t::const_iterator it2 = _atsc_ett_pids.begin();
    for (i = 0; it2 != _atsc_ett_pids.end() && (i < ett_count); (++it2),(i++))
    {
        if (find(cur_pids.begin(), cur_pids.end(), *it2) == cur_pids.end())
            add_pids.push_back(*it2);
    }

    for (i = 0; i < cur_pids.size(); i++)
    {
        uint pid = cur_pids[i];
        if (find(add_pids.begin(), add_pids.end(), pid) == add_pids.end())
            del_pids.push_back(pid);
    }

    return add_pids.size() || del_pids.size();
}

void ATSCStreamData::ProcessMGT(const MasterGuideTable *mgt)
{
    QMutexLocker locker(&_listener_lock);

    _atsc_eit_reset = true;
    _atsc_eit_pids.clear();
    _atsc_ett_pids.clear();

    for (uint i = 0 ; i < mgt->TableCount(); i++)
    {
        const int table_class = mgt->TableClass(i);
        const uint pid        = mgt->TablePID(i);

        if (table_class == TableClass::EIT)
        {
            const uint num = mgt->TableType(i) - 0x100;
            _atsc_eit_pids[num] = pid;
        }
        else if (table_class == TableClass::ETTe)
        {
            const uint num = mgt->TableType(i) - 0x200;
            _atsc_ett_pids[num] = pid;
        }
    }

    for (uint i = 0; i < _atsc_main_listeners.size(); i++)
        _atsc_main_listeners[i]->HandleMGT(mgt);
}

void ATSCStreamData::ProcessVCT(uint tsid, const VirtualChannelTable *vct)
{
    for (uint i = 0; i < _atsc_main_listeners.size(); i++)
        _atsc_main_listeners[i]->HandleVCT(tsid, vct);

    _sourceid_to_atscsrcid.clear();
    for (uint i = 0; i < vct->ChannelCount() ; i++)
    {
        if (vct->IsHiddenInGuide(i))
        {
            VERBOSE(VB_EIT, QString("%1 chan %2-%3 is hidden in guide")
                    .arg(vct->ModulationMode(i) == 1 ? "NTSC" : "ATSC")
                    .arg(vct->MajorChannel(i))
                    .arg(vct->MinorChannel(i)));
            continue;
        }

        if (1 == vct->ModulationMode(i))
        {
            VERBOSE(VB_EIT, QString("Ignoring NTSC chan %1-%2")
                    .arg(vct->MajorChannel(i))
                    .arg(vct->MinorChannel(i)));
            continue;
        }

        VERBOSE(VB_EIT, QString("Adding Source #%1 ATSC chan %2-%3")
                .arg(vct->SourceID(i))
                .arg(vct->MajorChannel(i))
                .arg(vct->MinorChannel(i)));

        _sourceid_to_atscsrcid[vct->SourceID(i)] =
            vct->MajorChannel(i) << 8 | vct->MinorChannel(i);
    }
}

void ATSCStreamData::ProcessTVCT(uint tsid,
                                 const TerrestrialVirtualChannelTable *vct)
{
    QMutexLocker locker(&_listener_lock);
    ProcessVCT(tsid, vct);
    for (uint i = 0; i < _atsc_aux_listeners.size(); i++)
        _atsc_aux_listeners[i]->HandleTVCT(tsid, vct);
}

void ATSCStreamData::ProcessCVCT(uint tsid,
                                 const CableVirtualChannelTable *vct)
{
    QMutexLocker locker(&_listener_lock);
    ProcessVCT(tsid, vct);
    for (uint i = 0; i < _atsc_aux_listeners.size(); i++)
        _atsc_aux_listeners[i]->HandleCVCT(tsid, vct);
}

bool ATSCStreamData::HasCachedMGT(bool current) const
{
    if (!current)
        VERBOSE(VB_IMPORTANT, "Currently we ignore \'current\' param");

    return (bool)(_cached_mgt);
}

bool ATSCStreamData::HasChannel(uint major, uint minor) const
{
    bool hasit = false;

    {
        tvct_vec_t tvct = GetAllCachedTVCTs();
        for (uint i = 0; i < tvct.size() && !hasit; i++)
        {
            if (tvct[i]->Find(major, minor) >= 0)
                hasit |= HasProgram(tvct[i]->ProgramNumber(i));
        }
        ReturnCachedTVCTTables(tvct);
    }

    if (!hasit)
    {
        cvct_vec_t cvct = GetAllCachedCVCTs();
        for (uint i = 0; i < cvct.size() && !hasit; i++)
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
