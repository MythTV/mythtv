// -*- Mode: c++ -*-

// MythTV headers
#include "streamhandler.h"

#include "threadedfilewriter.h"
#include <utility>

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#define LOC      QString("SH[%1](%2): ").arg(m_inputid).arg(m_device)

StreamHandler::~StreamHandler()
{
    QMutexLocker locker(&m_add_rm_lock);

    {
        QMutexLocker locker2(&m_listener_lock);
        if (!m_stream_data_list.empty())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "dtor & _stream_data_list not empty");
        }
    }

    // This should never be triggered.. just to be safe..
    if (m_running)
        Stop();
}

void StreamHandler::AddListener(MPEGStreamData *data,
                                bool allow_section_reader,
                                bool needs_buffering,
                                QString output_file)
{
    QMutexLocker locker(&m_add_rm_lock);

    LOG(VB_RECORD, LOG_INFO, LOC + QString("AddListener(0x%1) -- begin")
                .arg((uint64_t)data,0,16));
    if (!data)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("AddListener(0x%1) -- null data")
                .arg((uint64_t)data,0,16));
        return;
    }

    m_listener_lock.lock();

    LOG(VB_RECORD, LOG_INFO, LOC + QString("AddListener(0x%1) -- locked")
                .arg((uint64_t)data,0,16));

    if (m_stream_data_list.empty())
    {
        QMutexLocker locker2(&m_start_stop_lock);
        m_allow_section_reader = allow_section_reader;
        m_needs_buffering      = needs_buffering;
    }
    else
    {
        QMutexLocker locker2(&m_start_stop_lock);
        m_allow_section_reader &= allow_section_reader;
        m_needs_buffering      |= needs_buffering;
    }

    m_stream_data_list[data] = std::move(output_file);

    m_listener_lock.unlock();

    Start();

    LOG(VB_RECORD, LOG_INFO, LOC + QString("AddListener(0x%1) -- end")
                .arg((uint64_t)data,0,16));
}

void StreamHandler::RemoveListener(MPEGStreamData *data)
{
    QMutexLocker locker(&m_add_rm_lock);

    LOG(VB_RECORD, LOG_INFO, LOC + QString("RemoveListener(0x%1) -- begin")
                .arg((uint64_t)data,0,16));
    if (!data)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("RemoveListener(0x%1) -- null data")
                .arg((uint64_t)data,0,16));
        return;
    }

    m_listener_lock.lock();

    LOG(VB_RECORD, LOG_INFO, LOC + QString("RemoveListener(0x%1) -- locked")
                .arg((uint64_t)data,0,16));

    StreamDataList::iterator it = m_stream_data_list.find(data);

    if (it != m_stream_data_list.end())
    {
        if (!(*it).isEmpty())
            RemoveNamedOutputFile(*it);
        m_stream_data_list.erase(it);
    }

    m_listener_lock.unlock();

    if (m_stream_data_list.empty())
        Stop();

    LOG(VB_RECORD, LOG_INFO, LOC + QString("RemoveListener(0x%1) -- end")
                .arg((uint64_t)data,0,16));
}

void StreamHandler::Start(void)
{
    QMutexLocker locker(&m_start_stop_lock);

    if (m_running)
    {
        if ((m_using_section_reader && !m_allow_section_reader) ||
            (m_needs_buffering      && !m_using_buffering))
        {
            LOG(VB_RECORD, LOG_INFO, LOC + "Restarting StreamHandler");
            SetRunningDesired(false);
            m_restarting = true;
            locker.unlock();
            wait();
            locker.relock();
            m_restarting = false;
        }
    }

    if (m_running)
        return;

    m_eit_pids.clear();

    m_bError = false;
    SetRunningDesired(true);
    MThread::start();

    while (!m_running && !m_bError && m_running_desired)
        m_running_state_changed.wait(&m_start_stop_lock, 100);

    if (m_bError)
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
    // This used to use QMutexLocker, but that sometimes left the
    // mutex locked on exit, so...
    m_start_stop_lock.lock();
    bool r = m_running || m_restarting;
    m_start_stop_lock.unlock();
    return r;
}

void StreamHandler::SetRunning(bool is_running,
                               bool is_using_buffering,
                               bool is_using_section_reader)
{
    QMutexLocker locker(&m_start_stop_lock);
    m_running              = is_running;
    m_using_buffering      = is_using_buffering;
    m_using_section_reader = is_using_section_reader;
    m_running_state_changed.wakeAll();
}

void StreamHandler::SetRunningDesired(bool desired)
{
    m_running_desired = desired;
    if (!desired)
        MThread::exit(0);
}

bool StreamHandler::AddPIDFilter(PIDInfo *info)
{
#ifdef DEBUG_PID_FILTERS
    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("AddPIDFilter(0x%1)")
            .arg(info->_pid, 0, 16));
#endif // DEBUG_PID_FILTERS

    QMutexLocker writing_locker(&m_pid_lock);
    m_pid_info[info->_pid] = info;

    CycleFiltersByPriority();

    return true;
}

bool StreamHandler::RemovePIDFilter(uint pid)
{
#ifdef DEBUG_PID_FILTERS
    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("RemovePIDFilter(0x%1)").arg(pid, 0, 16));
#endif // DEBUG_PID_FILTERS

    QMutexLocker write_locker(&m_pid_lock);

    PIDInfoMap::iterator it = m_pid_info.find(pid);
    if (it == m_pid_info.end())
        return false;

    PIDInfo *tmp = *it;
    m_pid_info.erase(it);

    bool ok = true;
    if (tmp->IsOpen())
    {
        ok = tmp->Close(m_device);
        m_open_pid_filters--;

        CycleFiltersByPriority();
    }

    delete tmp;

    return ok;
}

bool StreamHandler::RemoveAllPIDFilters(void)
{
    QMutexLocker write_locker(&m_pid_lock);

#ifdef DEBUG_PID_FILTERS
    LOG(VB_RECORD, LOG_DEBUG, LOC + "RemoveAllPIDFilters()");
#endif // DEBUG_PID_FILTERS

    vector<int> del_pids;
    PIDInfoMap::iterator it = m_pid_info.begin();
    for (; it != m_pid_info.end(); ++it)
        del_pids.push_back(it.key());

    bool ok = true;
    auto dit = del_pids.begin();
    for (; dit != del_pids.end(); ++dit)
        ok &= RemovePIDFilter(*dit);

    return UpdateFilters() && ok;
}

void StreamHandler::UpdateListeningForEIT(void)
{
    vector<uint> add_eit;
    vector<uint> del_eit;

    QMutexLocker read_locker(&m_listener_lock);

    StreamDataList::const_iterator it1 = m_stream_data_list.begin();
    for (; it1 != m_stream_data_list.end(); ++it1)
    {
        MPEGStreamData *sd = it1.key();
        if (sd->HasEITPIDChanges(m_eit_pids) &&
            sd->GetEITPIDChanges(m_eit_pids, add_eit, del_eit))
        {
            for (size_t i = 0; i < del_eit.size(); i++)
            {
                uint_vec_t::iterator it2;
                it2 = find(m_eit_pids.begin(), m_eit_pids.end(), del_eit[i]);
                if (it2 != m_eit_pids.end())
                    m_eit_pids.erase(it2);
                sd->RemoveListeningPID(del_eit[i]);
            }

            for (size_t i = 0; i < add_eit.size(); i++)
            {
                m_eit_pids.push_back(add_eit[i]);
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
        QMutexLocker read_locker(&m_listener_lock);
        StreamDataList::const_iterator it = m_stream_data_list.begin();
        for (; it != m_stream_data_list.end(); ++it)
            it.key()->GetPIDs(pids);
    }

    QMap<uint, PIDInfo*> add_pids;
    vector<uint>         del_pids;

    {
        QMutexLocker read_locker(&m_pid_lock);

        // PIDs that need to be added..
        pid_map_t::const_iterator lit = pids.constBegin();
        for (; lit != pids.constEnd(); ++lit)
        {
            if (*lit && (m_pid_info.find(lit.key()) == m_pid_info.end()))
            {
                add_pids[lit.key()] = CreatePIDInfo(
                    lit.key(), StreamID::PrivSec, 0);
            }
        }

        // PIDs that need to be removed..
        PIDInfoMap::const_iterator fit = m_pid_info.begin();
        for (; fit != m_pid_info.end(); ++fit)
        {
            bool in_pids = pids.find(fit.key()) != pids.end();
            if (!in_pids)
                del_pids.push_back(fit.key());
        }
    }

    // Remove PIDs
    bool ok = true;
    auto dit = del_pids.begin();
    for (; dit != del_pids.end(); ++dit)
        ok &= RemovePIDFilter(*dit);

    // Add PIDs
    QMap<uint, PIDInfo*>::iterator ait = add_pids.begin();
    for (; ait != add_pids.end(); ++ait)
        ok &= AddPIDFilter(*ait);

    // Cycle filters if it's been a while
    if (m_cycle_timer.isRunning() && (m_cycle_timer.elapsed() > 1000))
        CycleFiltersByPriority();

    return ok;
}

PIDPriority StreamHandler::GetPIDPriority(uint pid) const
{
    QMutexLocker reading_locker(&m_listener_lock);

    PIDPriority tmp = kPIDPriorityNone;

    StreamDataList::const_iterator it = m_stream_data_list.begin();
    for (; it != m_stream_data_list.end(); ++it)
        tmp = max(tmp, it.key()->GetPIDPriority(pid));

    return tmp;
}

void StreamHandler::WriteMPTS(unsigned char * buffer, uint len)
{
    if (m_mpts_tfw == nullptr)
        return;
    m_mpts_tfw->Write(buffer, len);
}

bool StreamHandler::AddNamedOutputFile(const QString &file)
{
#if !defined( USING_MINGW ) && !defined( _MSC_VER )
    QMutexLocker lk(&m_mpts_lock);

    m_mpts_files.insert(file);
    QString fn = QString("%1.raw").arg(file);

    if (m_mpts_files.size() == 1)
    {
        m_mpts_base_file = fn;
        m_mpts_tfw = new ThreadedFileWriter(fn,
                                           O_WRONLY|O_TRUNC|O_CREAT|O_LARGEFILE,
                                           0644);
        if (!m_mpts_tfw->Open())
        {
            delete m_mpts_tfw;
            m_mpts_tfw = nullptr;
            return false;
        }
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("Opened '%1'").arg(m_mpts_base_file));
    }
    else
    {
        if (link(m_mpts_base_file.toLocal8Bit(), fn.toLocal8Bit()) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to link '%1' to '%2'")
                .arg(m_mpts_base_file)
                .arg(fn) +
                ENO);
        }
        else
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("linked '%1' to '%2'")
                .arg(m_mpts_base_file)
                .arg(fn));
        }
    }

#endif //  !defined( USING_MINGW ) && !defined( _MSC_VER )
    return true;
}

void StreamHandler::RemoveNamedOutputFile(const QString &file)
{
#if !defined( USING_MINGW ) && !defined( _MSC_VER )
    QMutexLocker lk(&m_mpts_lock);

    QSet<QString>::iterator it = m_mpts_files.find(file);
    if (it != m_mpts_files.end())
    {
        m_mpts_files.erase(it);
        if (m_mpts_files.isEmpty())
        {
            delete m_mpts_tfw;
            m_mpts_tfw = nullptr;
        }
    }
#endif //  !defined( USING_MINGW ) && !defined( _MSC_VER )
}
