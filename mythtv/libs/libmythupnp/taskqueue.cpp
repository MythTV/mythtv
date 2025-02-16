//////////////////////////////////////////////////////////////////////////////
// Program Name: taskqueue.cpp
// Created     : Oct. 24, 2005
//
// Purpose     : Used to process delayed tasks
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "taskqueue.h"

#include "libmythbase/mythlogging.h"

/////////////////////////////////////////////////////////////////////////////
// Define Global instance 
/////////////////////////////////////////////////////////////////////////////

static QMutex g_pTaskQueueCreationLock;
TaskQueue* TaskQueue::g_pTaskQueue = nullptr;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// 
// Task Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

long Task::m_nTaskCount = 0;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Task::Task(const QString &debugName)
  : ReferenceCounter(debugName),
    m_nTaskId(m_nTaskCount++)
{
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Task Queue Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

TaskQueue* TaskQueue::Instance()
{
    QMutexLocker locker(&g_pTaskQueueCreationLock);
    return g_pTaskQueue ? g_pTaskQueue : (g_pTaskQueue = new TaskQueue());
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void TaskQueue::Shutdown()
{
    QMutexLocker locker(&g_pTaskQueueCreationLock);
    delete g_pTaskQueue;
    g_pTaskQueue = nullptr;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

TaskQueue::TaskQueue() : MThread("TaskQueue")
{
    LOG(VB_UPNP, LOG_INFO, "Starting TaskQueue Thread...");

    start();

    LOG(VB_UPNP, LOG_INFO, "TaskQueue Thread Started.");
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

TaskQueue::~TaskQueue()
{
    m_bTermRequested = true; 

    wait();

    Clear();
}

void TaskQueue::RequestTerminate()
{
    m_bTermRequested = true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void TaskQueue::run( )
{
    RunProlog();

    LOG(VB_UPNP, LOG_INFO, "TaskQueue Thread Running.");

    while ( !m_bTermRequested )
    {
        // ------------------------------------------------------------------
        // Process Any Tasks that may need to be executed.
        // ------------------------------------------------------------------

        auto ttNow = nowAsDuration<std::chrono::microseconds>();

        Task *pTask = GetNextExpiredTask( ttNow );
        if (pTask != nullptr)
        {
            try
            {
                pTask->Execute( this );
                pTask->DecrRef();
            }
            catch( ... )
            {
                LOG(VB_GENERAL, LOG_ERR, "Call to Execute threw an exception.");
            }

        }
        // Make sure to throttle our processing.

        usleep( 100ms );
    }

    RunEpilog();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void TaskQueue::Clear( )
{
    m_mutex.lock(); 

    for (auto & task : m_mapTasks)
    {
        if (task.second != nullptr)
            task.second->DecrRef();
    }

    m_mapTasks.clear();

    m_mutex.unlock(); 
}

/////////////////////////////////////////////////////////////////////////////
/// \brief Add a task to run in the future.
/// \param msec The number of milliseconds in the future to run this task
/// \param pTask A pointer to the task.
/////////////////////////////////////////////////////////////////////////////

void TaskQueue::AddTask( std::chrono::milliseconds msec, Task *pTask )
{
    auto tt = nowAsDuration<std::chrono::microseconds>() + msec;

    AddTaskAbsolute( tt, pTask );
}

/////////////////////////////////////////////////////////////////////////////
/// \brief Add a task to run at a specific time.
/// \param msec The time when this task should run
/// \param pTask A pointer to the task.
/////////////////////////////////////////////////////////////////////////////

void TaskQueue::AddTaskAbsolute( std::chrono::microseconds ttKey, Task *pTask )
{
    if (pTask != nullptr)
    {
        m_mutex.lock();
        pTask->IncrRef();
        m_mapTasks.insert( TaskMap::value_type( ttKey, pTask ));
        m_mutex.unlock();
    }
}

/////////////////////////////////////////////////////////////////////////////
/// \brief Add a task to run now.
/// \param pTask A pointer to the task.
/////////////////////////////////////////////////////////////////////////////

void TaskQueue::AddTask( Task *pTask )
{

    if (pTask != nullptr)
    {
        auto tt = nowAsDuration<std::chrono::microseconds>();

        AddTaskAbsolute( tt, pTask );
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Task *TaskQueue::GetNextExpiredTask( std::chrono::microseconds tt, std::chrono::milliseconds nWithinMilliSecs /*=50*/ )
{
    Task *pTask = nullptr;

    tt += nWithinMilliSecs ;

    m_mutex.lock(); 

    auto it = m_mapTasks.begin();
    if (it != m_mapTasks.end())
    {
        std::chrono::microseconds ttTask = (*it).first;

        if (ttTask < tt)
        {
            // Do not release here... caller must call release.

            pTask = (*it).second;
        
            m_mapTasks.erase( it );    
        }
    }
    m_mutex.unlock(); 

    return pTask;
}
