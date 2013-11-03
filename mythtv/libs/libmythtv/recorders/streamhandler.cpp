// -*- Mode: c++ -*-

// MythTV headers
#include "streamhandler.h"

#define LOC      QString("SH(%1): ").arg(_device)

StreamHandler::StreamHandler(const QString &device) :
    MThread("StreamHandler"),
    _device(device),
    _needs_buffering(false),
    _allow_section_reader(false),

    _running_desired(false),
    _error(false),
    _running(false),
    _using_buffering(false),
    _using_section_reader(false),

    _pid_lock(QMutex::Recursive),
    _open_pid_filters(0),

    _listener_lock(QMutex::Recursive)
{
}

StreamHandler::~StreamHandler()
{
    QMutexLocker locker(&_add_rm_lock);

    {
        QMutexLocker locker2(&_listener_lock);
        if (!_stream_data_list.empty())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "dtor & _stream_data_list not empty");
        }
    }

    // This should never be triggered.. just to be safe..
    if (_running)
        Stop();
}

void StreamHandler::AddListener(MPEGStreamData *data,
                                bool allow_section_reader,
                                bool needs_buffering,
                                QString output_file)
{
    QMutexLocker locker(&_add_rm_lock);

    LOG(VB_RECORD, LOG_INFO, LOC + QString("AddListener(0x%1) -- begin")
                .arg((uint64_t)data,0,16));
    if (!data)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("AddListener(0x%1) -- null data")
                .arg((uint64_t)data,0,16));
        return;
    }

    _listener_lock.lock();

    LOG(VB_RECORD, LOG_INFO, LOC + QString("AddListener(0x%1) -- locked")
                .arg((uint64_t)data,0,16));

    if (_stream_data_list.empty())
    {
        QMutexLocker locker(&_start_stop_lock);
        _allow_section_reader = allow_section_reader;
        _needs_buffering      = needs_buffering;
    }
    else
    {
        QMutexLocker locker(&_start_stop_lock);
        _allow_section_reader &= allow_section_reader;
        _needs_buffering      |= needs_buffering;
    }

    StreamDataList::iterator it = _stream_data_list.find(data);
    if (it != _stream_data_list.end())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Programmer Error, attempted "
                "to add a listener which is already being listened to.");
    }
    else
    {
        _stream_data_list[data] = output_file;
    }

    if (!output_file.isEmpty())
        AddNamedOutputFile(output_file);

    _listener_lock.unlock();

    Start();

    LOG(VB_RECORD, LOG_INFO, LOC + QString("AddListener(0x%1) -- end")
                .arg((uint64_t)data,0,16));
}

void StreamHandler::RemoveListener(MPEGStreamData *data)
{
    QMutexLocker locker(&_add_rm_lock);

    LOG(VB_RECORD, LOG_INFO, LOC + QString("RemoveListener(0x%1) -- begin")
                .arg((uint64_t)data,0,16));
    if (!data)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("RemoveListener(0x%1) -- null data")
                .arg((uint64_t)data,0,16));
        return;
    }

    _listener_lock.lock();

    LOG(VB_RECORD, LOG_INFO, LOC + QString("RemoveListener(0x%1) -- locked")
                .arg((uint64_t)data,0,16));

    StreamDataList::iterator it = _stream_data_list.find(data);

    if (it != _stream_data_list.end())
    {
        if (!(*it).isEmpty())
            RemoveNamedOutputFile(*it);
        _stream_data_list.erase(it);
    }

    if (_stream_data_list.empty())
    {
        _listener_lock.unlock();
        Stop();
    }
    else
    {
        _listener_lock.unlock();
    }

    LOG(VB_RECORD, LOG_INFO, LOC + QString("RemoveListener(0x%1) -- end")
                .arg((uint64_t)data,0,16));
}

void StreamHandler::Start(void)
{
    QMutexLocker locker(&_start_stop_lock);

    if (_running)
    {
        if ((_using_section_reader && !_allow_section_reader) ||
            (_needs_buffering      && !_using_buffering))
        {
            LOG(VB_RECORD, LOG_INFO, LOC + "Restarting StreamHandler");
            SetRunningDesired(false);
            locker.unlock();
            wait();
            locker.relock();
        }
    }

    if (_running)
        return;

    _eit_pids.clear();

    _error = false;
    SetRunningDesired(true);
    MThread::start();

    while (!_running && !_error && _running_desired)
        _running_state_changed.wait(&_start_stop_lock, 100);

    if (_error)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Start failed");
        SetRunningDesired(false);
    }
}

void StreamHandler::Stop(void)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "Stopping");
    SetRunningDesired(false);
    wait();
    LOG(VB_RECORD, LOG_DEBUG, LOC + "Stopped");
}

bool StreamHandler::IsRunning(void) const
{
    QMutexLocker locker(&_start_stop_lock);
    return _running;
}

void StreamHandler::SetRunning(bool is_running,
                               bool is_using_buffering,
                               bool is_using_section_reader)
{
    QMutexLocker locker(&_start_stop_lock);
    _running              = is_running;
    _using_buffering      = is_using_buffering;
    _using_section_reader = is_using_section_reader;
    _running_state_changed.wakeAll();
}

void StreamHandler::SetRunningDesired(bool desired)
{
    _running_desired = desired;
    if (!desired)
        MThread::exit(0);
}

bool StreamHandler::AddPIDFilter(PIDInfo *info)
{
#ifdef DEBUG_PID_FILTERS
    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("AddPIDFilter(0x%1)")
            .arg(info->_pid, 0, 16));
#endif // DEBUG_PID_FILTERS

    QMutexLocker writing_locker(&_pid_lock);
    _pid_info[info->_pid] = info;

    CycleFiltersByPriority();

    return true;
}

bool StreamHandler::RemovePIDFilter(uint pid)
{
#ifdef DEBUG_PID_FILTERS
    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("RemovePIDFilter(0x%1)").arg(pid, 0, 16));
#endif // DEBUG_PID_FILTERS

    QMutexLocker write_locker(&_pid_lock);

    PIDInfoMap::iterator it = _pid_info.find(pid);
    if (it == _pid_info.end())
        return false;

    PIDInfo *tmp = *it;
    _pid_info.erase(it);

    bool ok = true;
    if (tmp->IsOpen())
    {
        ok = tmp->Close(_device);
        _open_pid_filters--;

        CycleFiltersByPriority();
    }

    delete tmp;

    return ok;
}

bool StreamHandler::RemoveAllPIDFilters(void)
{
    QMutexLocker write_locker(&_pid_lock);

#ifdef DEBUG_PID_FILTERS
    LOG(VB_RECORD, LOG_DEBUG, LOC + "RemoveAllPIDFilters()");
#endif // DEBUG_PID_FILTERS

    vector<int> del_pids;
    PIDInfoMap::iterator it = _pid_info.begin();
    for (; it != _pid_info.end(); ++it)
        del_pids.push_back(it.key());

    bool ok = true;
    vector<int>::iterator dit = del_pids.begin();
    for (; dit != del_pids.end(); ++dit)
        ok &= RemovePIDFilter(*dit);

    return UpdateFilters() && ok;
}

void StreamHandler::UpdateListeningForEIT(void)
{
    vector<uint> add_eit, del_eit;

    QMutexLocker read_locker(&_listener_lock);

    StreamDataList::const_iterator it = _stream_data_list.begin();
    for (; it != _stream_data_list.end(); ++it)
    {
        MPEGStreamData *sd = it.key();
        if (sd->HasEITPIDChanges(_eit_pids) &&
            sd->GetEITPIDChanges(_eit_pids, add_eit, del_eit))
        {
            for (uint i = 0; i < del_eit.size(); i++)
            {
                uint_vec_t::iterator it;
                it = find(_eit_pids.begin(), _eit_pids.end(), del_eit[i]);
                if (it != _eit_pids.end())
                    _eit_pids.erase(it);
                sd->RemoveListeningPID(del_eit[i]);
            }

            for (uint i = 0; i < add_eit.size(); i++)
            {
                _eit_pids.push_back(add_eit[i]);
                sd->AddListeningPID(add_eit[i]);
            }
        }
    }
}

bool StreamHandler::UpdateFiltersFromStreamData(void)
{
    UpdateListeningForEIT();

    pid_map_t pids;

    {
        QMutexLocker read_locker(&_listener_lock);
        StreamDataList::const_iterator it = _stream_data_list.begin();
        for (; it != _stream_data_list.end(); ++it)
            it.key()->GetPIDs(pids);
    }

    QMap<uint, PIDInfo*> add_pids;
    vector<uint>         del_pids;

    {
        QMutexLocker read_locker(&_pid_lock);

        // PIDs that need to be added..
        pid_map_t::const_iterator lit = pids.constBegin();
        for (; lit != pids.constEnd(); ++lit)
        {
            if (*lit && (_pid_info.find(lit.key()) == _pid_info.end()))
            {
                add_pids[lit.key()] = CreatePIDInfo(
                    lit.key(), StreamID::PrivSec, 0);
            }
        }

        // PIDs that need to be removed..
        PIDInfoMap::const_iterator fit = _pid_info.begin();
        for (; fit != _pid_info.end(); ++fit)
        {
            bool in_pids = pids.find(fit.key()) != pids.end();
            if (!in_pids)
                del_pids.push_back(fit.key());
        }
    }

    // Remove PIDs
    bool ok = true;
    vector<uint>::iterator dit = del_pids.begin();
    for (; dit != del_pids.end(); ++dit)
        ok &= RemovePIDFilter(*dit);

    // Add PIDs
    QMap<uint, PIDInfo*>::iterator ait = add_pids.begin();
    for (; ait != add_pids.end(); ++ait)
        ok &= AddPIDFilter(*ait);

    // Cycle filters if it's been a while
    if (_cycle_timer.isRunning() && (_cycle_timer.elapsed() > 1000))
        CycleFiltersByPriority();

    return ok;
}

PIDPriority StreamHandler::GetPIDPriority(uint pid) const
{
    QMutexLocker reading_locker(&_listener_lock);

    PIDPriority tmp = kPIDPriorityNone;

    StreamDataList::const_iterator it = _stream_data_list.begin();
    for (; it != _stream_data_list.end(); ++it)
        tmp = max(tmp, it.key()->GetPIDPriority(pid));

    return tmp;
}
