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
    : QObject(NULL, "ATSCStreamData"),
      MPEGStreamData(-1, cacheTables),
      _GPS_UTC_offset(13 /* leap seconds as of 2004 */),
      _mgt_version(-1),
      _cached_mgt(NULL),
      _desired_major_channel(desiredMajorChannel),
      _desired_minor_channel(desiredMinorChannel)
{
    AddListeningPID(ATSC_PSIP_PID);
}

void ATSCStreamData::SetDesiredChannel(int major, int minor)
{
    // TODO this should reset only if it can't regen from cached tables.
    _desired_major_channel = major;
    _desired_minor_channel = minor;
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
    _ett_version.clear();

    { 
        _cache_lock.lock();

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

        _cache_lock.unlock();
    }
}

/** \fn ATSCStreamData::IsRedundant(const PSIPTable&) const
 *  \brief Returns true if table already seen.
 *  \todo All RRT tables are ignored
 *  \todo We don't check the start time of EIT and ETT tables
 *        in the version check, so many tables are improperly
 *        ignored.
 */
bool ATSCStreamData::IsRedundant(const PSIPTable &psip) const
{
    const int table_id = psip.TableID();
    const int version = psip.Version();

    if (MPEGStreamData::IsRedundant(psip))
        return true;

    if (TableID::EIT == table_id)
    {
        // HACK This isn't right, we should also check start_time..
        // But this gives us a little sample.
        return  VersionEIT(psip.tsheader()->PID()) == version;
    }

    if (TableID::ETT == table_id)
    {
        // HACK This isn't right, we should also check start_time..
        // But this gives us a little sample.
        return VersionETT(psip.tsheader()->PID()) == version;
    }

    if (TableID::STT == table_id)
        return false; // each SystemTimeTable matters

    if (TableID::MGT == table_id)
        return VersionMGT() == version;

    if (TableID::TVCT == table_id)
    {
        TerrestrialVirtualChannelTable tvct(psip);
        return VersionTVCT(tvct.TransportStreamID()) == version;
    }

    if (TableID::CVCT == table_id)
    {
        CableVirtualChannelTable cvct(psip);
        return VersionCVCT(cvct.TransportStreamID()) == version;
    }

    if (TableID::RRT == table_id)
        return true; // we ignore RatingRegionTables

    return false;
}

bool ATSCStreamData::HandleTables(uint pid, const PSIPTable &psip)
{
    if (MPEGStreamData::HandleTables(pid, psip))
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
                emit UpdateMGT(mgt);
            }
            else
            {
                MasterGuideTable mgt(psip);
                emit UpdateMGT(&mgt);
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
                emit UpdateTVCT(tsid, vct);
                emit UpdateVCT(tsid,  vct);
            }
            else
            {
                TerrestrialVirtualChannelTable vct(psip);
                emit UpdateTVCT(tsid, &vct);
                emit UpdateVCT(tsid,  &vct);
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
                emit UpdateCVCT(tsid, vct);
                emit UpdateVCT(tsid,  vct);
            }
            else
            {
                CableVirtualChannelTable vct(psip);
                emit UpdateCVCT(tsid, &vct);
                emit UpdateVCT(tsid,  &vct);
            }
            return true;
        }
        case TableID::RRT:
        {
            RatingRegionTable rrt(psip);
            emit UpdateRRT(&rrt);
            return true;
        }
        case TableID::EIT:
        {
            EventInformationTable eit(psip);
            SetVersionEIT(pid, version);
            emit UpdateEIT(pid, &eit);
            return true;
        }
        case TableID::ETT:
        {
            ExtendedTextTable ett(psip);
            SetVersionETT(pid, version);
            emit UpdateETT(pid, &ett);
            return true;
        }
        case TableID::STT:
        {
            SystemTimeTable stt(psip);
            if (stt.GPSOffset() != _GPS_UTC_offset) // only update if it changes
                _GPS_UTC_offset = stt.GPSOffset();
            emit UpdateSTT(&stt);
            return true;
        }
        case TableID::DCCT:
        {
            DirectedChannelChangeTable dcct(psip);
            emit UpdateDCCT(&dcct);
            return true;
        }
        case TableID::DCCSCT:
        {
            DirectedChannelChangeSelectionCodeTable dccsct(psip);
            emit UpdateDCCSCT(&dccsct);
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
    _cache_lock.lock();
    DeleteCachedTable(_cached_mgt);
    _cached_mgt = mgt;
    _cache_lock.unlock();
}

void ATSCStreamData::CacheTVCT(uint pid, TerrestrialVirtualChannelTable* tvct)
{
    _cache_lock.lock();
    tvct_cache_t::iterator it = _cached_tvcts.find(pid);
    if (it != _cached_tvcts.end())
        DeleteCachedTable(*it);
    _cached_tvcts[pid] = tvct;

    _cache_lock.unlock();
}

void ATSCStreamData::CacheCVCT(uint pid, CableVirtualChannelTable* cvct)
{
    _cache_lock.lock();
    cvct_cache_t::iterator it = _cached_cvcts.find(pid);
    if (it != _cached_cvcts.end())
        DeleteCachedTable(*it);
    _cached_cvcts[pid] = cvct;

    _cache_lock.unlock();
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

void ATSCStreamData::PrintMGT(const MasterGuideTable *mgt) const
{
    VERBOSE(VB_RECORD, mgt->toString());
}

void ATSCStreamData::PrintSTT(const SystemTimeTable *stt) const
{
    VERBOSE(VB_RECORD, stt->toString());
}

void ATSCStreamData::PrintRRT(const RatingRegionTable*) const
{
    VERBOSE(VB_RECORD, QString("RRT"));
}

void ATSCStreamData::PrintDCCT(const DirectedChannelChangeTable*) const
{
    VERBOSE(VB_RECORD, QString("DCCT"));
}

void ATSCStreamData::PrintDCCSCT(
    const DirectedChannelChangeSelectionCodeTable*) const
{
    VERBOSE(VB_RECORD, QString("DCCSCT"));
}

void ATSCStreamData::PrintTVCT(
    uint /*pid*/, const TerrestrialVirtualChannelTable* tvct) const
{
    VERBOSE(VB_RECORD, tvct->toString());
}

void ATSCStreamData::PrintCVCT(
    uint /*pid*/, const CableVirtualChannelTable* cvct) const
{
    VERBOSE(VB_RECORD, cvct->toString());
}

void ATSCStreamData::PrintEIT(uint pid, const EventInformationTable *eit) const
{
    VERBOSE(VB_RECORD, QString("EIT pid(0x%1) %2")
            .arg(pid, 0, 16).arg(eit->toString()));
}

void ATSCStreamData::PrintETT(uint pid, const ExtendedTextTable *ett) const
{
    VERBOSE(VB_RECORD, QString("ETT pid(0x%1) %2")
            .arg(pid, 0, 16).arg(ett->toString()));
}
