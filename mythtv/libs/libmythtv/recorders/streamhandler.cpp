// -*- Mode: c++ -*-

// C++ headers
#include <utility>

// MythTV headers
#include "streamhandler.h"

#include "libmythbase/threadedfilewriter.h"

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#define LOC      QString("SH[%1]: ").arg(m_inputId)

StreamHandler::~StreamHandler()
{
    QMutexLocker locker(&m_addRmLock);

    {
        QMutexLocker locker2(&m_listenerLock);
        if (!m_streamDataList.empty())
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
                                const QString& output_file)
{
    QMutexLocker locker(&m_addRmLock);

    LOG(VB_RECORD, LOG_INFO, LOC + QString("AddListener(0x%1) -- begin")
                .arg((uint64_t)data,0,16));
    if (!data)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("AddListener(0x%1) -- null data")
                .arg((uint64_t)data,0,16));
        return;
    }

    m_listenerLock.lock();

    LOG(VB_RECORD, LOG_INFO, LOC + QString("AddListener(0x%1) -- locked")
                .arg((uint64_t)data,0,16));

    if (m_streamDataList.empty())
    {
        QMutexLocker locker2(&m_startStopLock);
        m_allowSectionReader = allow_section_reader;
        m_needsBuffering     = needs_buffering;
    }
    else
    {
        QMutexLocker locker2(&m_startStopLock);
        m_allowSectionReader &= allow_section_reader;
        m_needsBuffering     |= needs_buffering;
    }

    m_streamDataList[data] = output_file;

    m_listenerLock.unlock();

    Start();

    LOG(VB_RECORD, LOG_INFO, LOC + QString("AddListener(0x%1) -- end")
                .arg((uint64_t)data,0,16));
}

void StreamHandler::RemoveListener(MPEGStreamData *data)
{
    QMutexLocker locker(&m_addRmLock);

    LOG(VB_RECORD, LOG_INFO, LOC + QString("RemoveListener(0x%1) -- begin")
                .arg((uint64_t)data,0,16));
    if (!data)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("RemoveListener(0x%1) -- null data")
                .arg((uint64_t)data,0,16));
        return;
    }

    m_listenerLock.lock();

    LOG(VB_RECORD, LOG_INFO, LOC + QString("RemoveListener(0x%1) -- locked")
                .arg((uint64_t)data,0,16));

    StreamDataList::iterator it = m_streamDataList.find(data);

    if (it != m_streamDataList.end())
    {
        if (!(*it).isEmpty())
            RemoveNamedOutputFile(*it);
        m_streamDataList.erase(it);
    }

    m_listenerLock.unlock();

    if (m_streamDataList.empty())
        Stop();

    LOG(VB_RECORD, LOG_INFO, LOC + QString("RemoveListener(0x%1) -- end")
                .arg((uint64_t)data,0,16));
}

void StreamHandler::Start(void)
{
    QMutexLocker locker(&m_startStopLock);

    if (m_running)
    {
        if ((m_usingSectionReader && !m_allowSectionReader) ||
            (m_needsBuffering     && !m_usingBuffering))
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

    m_eitPids.clear();

    m_bError = false;
    SetRunningDesired(true);
    MThread::start();

    while (!m_running && !m_bError && m_runningDesired)
        m_runningStateChanged.wait(&m_startStopLock, 100);

    if (m_bError)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Start failed");
        SetRunningDesired(false);
    }
}

void StreamHandler::Stop(void)
{
    LOG(VB_RECORD, LOG_DEBUG, LOC + "Stopping");
    StreamHandler::SetRunningDesired(false);
    wait();
    LOG(VB_RECORD, LOG_DEBUG, LOC + "Stopped");
}

bool StreamHandler::IsRunning(void) const
{
    // This used to use QMutexLocker, but that sometimes left the
    // mutex locked on exit, so...
    m_startStopLock.lock();
    bool r = m_running || m_restarting;
    m_startStopLock.unlock();
    return r;
}

void StreamHandler::SetRunning(bool is_running,
                               bool is_using_buffering,
                               bool is_using_section_reader)
{
    QMutexLocker locker(&m_startStopLock);
    m_running              = is_running;
    m_usingBuffering       = is_using_buffering;
    m_usingSectionReader   = is_using_section_reader;
    m_runningStateChanged.wakeAll();
}

void StreamHandler::SetRunningDesired(bool desired)
{
    m_runningDesired = desired;
    if (!desired)
        MThread::exit(0);
}

bool StreamHandler::AddPIDFilter(PIDInfo *info)
{
#ifdef DEBUG_PID_FILTERS
    LOG(VB_RECORD, LOG_DEBUG, LOC + QString("AddPIDFilter(0x%1)")
            .arg(info->m_pid, 0, 16));
#endif // DEBUG_PID_FILTERS

    QMutexLocker writing_locker(&m_pidLock);
    m_pidInfo[info->m_pid] = info;

    m_filtersChanged = true;

    CycleFiltersByPriority();

    return true;
}

bool StreamHandler::RemovePIDFilter(uint pid)
{
#ifdef DEBUG_PID_FILTERS
    LOG(VB_RECORD, LOG_DEBUG, LOC +
        QString("RemovePIDFilter(0x%1)").arg(pid, 0, 16));
#endif // DEBUG_PID_FILTERS

    QMutexLocker write_locker(&m_pidLock);

    PIDInfoMap::iterator it = m_pidInfo.find(pid);
    if (it == m_pidInfo.end())
        return false;

    PIDInfo *tmp = *it;
    m_pidInfo.erase(it);

    bool ok = true;
    if (tmp->IsOpen())
    {
        ok = tmp->Close(m_device);
        m_openPidFilters--;

        CycleFiltersByPriority();
    }

    delete tmp;

    m_filtersChanged = true;

    return ok;
}

bool StreamHandler::RemoveAllPIDFilters(void)
{
    QMutexLocker write_locker(&m_pidLock);

#ifdef DEBUG_PID_FILTERS
    LOG(VB_RECORD, LOG_DEBUG, LOC + "RemoveAllPIDFilters()");
#endif // DEBUG_PID_FILTERS

    std::vector<int> del_pids;
    for (auto it = m_pidInfo.begin(); it != m_pidInfo.end(); ++it)
        del_pids.push_back(it.key());

    bool ok = true;
    for (int & pid : del_pids)
        ok &= RemovePIDFilter(pid);

    return UpdateFilters() && ok;
}

void StreamHandler::UpdateListeningForEIT(void)
{
    QMutexLocker read_locker(&m_listenerLock);

    for (auto it1 = m_streamDataList.cbegin(); it1 != m_streamDataList.cend(); ++it1)
    {
        std::vector<uint> add_eit;
        std::vector<uint> del_eit;

        MPEGStreamData *sd = it1.key();
        if (sd->HasEITPIDChanges(m_eitPids) &&
            sd->GetEITPIDChanges(m_eitPids, add_eit, del_eit))
        {
            for (uint eit : del_eit)
            {
                uint_vec_t::iterator it2;
                it2 = find(m_eitPids.begin(), m_eitPids.end(), eit);
                if (it2 != m_eitPids.end())
                    m_eitPids.erase(it2);
                sd->RemoveListeningPID(eit);
            }

            for (uint eit : add_eit)
            {
                m_eitPids.push_back(eit);
                sd->AddListeningPID(eit);
            }
        }
    }
}

bool StreamHandler::UpdateFiltersFromStreamData(void)
{
    UpdateListeningForEIT();

    pid_map_t pids;

    {
        QMutexLocker read_locker(&m_listenerLock);
        for (auto it = m_streamDataList.cbegin(); it != m_streamDataList.cend(); ++it)
            it.key()->GetPIDs(pids);
    }

    QMap<uint, PIDInfo*> add_pids;
    std::vector<uint>    del_pids;

    {
        QMutexLocker read_locker(&m_pidLock);

        // PIDs that need to be added..
        for (auto lit = pids.constBegin(); lit != pids.constEnd(); ++lit)
        {
            if ((*lit != 0U) && (m_pidInfo.find(lit.key()) == m_pidInfo.end()))
            {
                add_pids[lit.key()] = CreatePIDInfo(
                    lit.key(), StreamID::PrivSec, 0);
            }
        }

        // PIDs that need to be removed..
        for (auto fit = m_pidInfo.cbegin(); fit != m_pidInfo.cend(); ++fit)
        {
            bool in_pids = pids.find(fit.key()) != pids.end();
            if (!in_pids)
                del_pids.push_back(fit.key());
        }
    }

    // Remove PIDs
    bool ok = true;
    for (uint & pid : del_pids)
        ok &= RemovePIDFilter(pid);

    // Add PIDs
    for (auto & pid : add_pids)
        ok &= AddPIDFilter(pid);

    // Cycle filters if it's been a while
    if (m_cycleTimer.isRunning() && (m_cycleTimer.elapsed() > 1s))
        CycleFiltersByPriority();

    return ok;
}

PIDPriority StreamHandler::GetPIDPriority(uint pid) const
{
    QMutexLocker reading_locker(&m_listenerLock);

    PIDPriority tmp = kPIDPriorityNone;

    for (auto it = m_streamDataList.cbegin(); it != m_streamDataList.cend(); ++it)
        tmp = std::max(tmp, it.key()->GetPIDPriority(pid));

    return tmp;
}

void StreamHandler::WriteMPTS(const unsigned char * buffer, uint len)
{
    if (m_mptsTfw == nullptr)
        return;
    m_mptsTfw->Write(buffer, len);
}

bool StreamHandler::AddNamedOutputFile([[maybe_unused]] const QString &file)
{
#if !defined( USING_MINGW ) && !defined( _MSC_VER )
    QMutexLocker lk(&m_mptsLock);

    m_mptsFiles.insert(file);
    QString fn = QString("%1.raw").arg(file);

    if (m_mptsFiles.size() == 1)
    {
        m_mptsBaseFile = fn;
        m_mptsTfw = new ThreadedFileWriter(fn,
                                           O_WRONLY|O_TRUNC|O_CREAT|O_LARGEFILE,
                                           0644);
        if (!m_mptsTfw->Open())
        {
            delete m_mptsTfw;
            m_mptsTfw = nullptr;
            return false;
        }
        LOG(VB_RECORD, LOG_INFO, LOC +
            QString("Opened '%1'").arg(m_mptsBaseFile));
    }
    else
    {
        if (link(m_mptsBaseFile.toLocal8Bit(), fn.toLocal8Bit()) < 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Failed to link '%1' to '%2'")
                .arg(m_mptsBaseFile, fn) + ENO);
        }
        else
        {
            LOG(VB_RECORD, LOG_INFO, LOC +
                QString("linked '%1' to '%2'")
                .arg(m_mptsBaseFile, fn));
        }
    }
#endif //  !defined( USING_MINGW ) && !defined( _MSC_VER )
    return true;
}

void StreamHandler::RemoveNamedOutputFile([[maybe_unused]] const QString &file)
{
#if !defined( USING_MINGW ) && !defined( _MSC_VER )
    QMutexLocker lk(&m_mptsLock);

    QSet<QString>::iterator it = m_mptsFiles.find(file);
    if (it != m_mptsFiles.end())
    {
        m_mptsFiles.erase(it);
        if (m_mptsFiles.isEmpty())
        {
            delete m_mptsTfw;
            m_mptsTfw = nullptr;
        }
    }
#endif //  !defined( USING_MINGW ) && !defined( _MSC_VER )
}
