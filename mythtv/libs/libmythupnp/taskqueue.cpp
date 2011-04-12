//////////////////////////////////////////////////////////////////////////////
// Program Name: taskqueue.cpp
// Created     : Oct. 24, 2005
//
// Purpose     : Used to process delayed tasks
//                                                                            
// Copyright (c) 2005 David Blain <mythtv@theblains.net>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include "taskqueue.h"
#include "mythverbose.h"

#include <QDateTime>

#include <iostream>

using std::cerr;

/////////////////////////////////////////////////////////////////////////////
// Define Global instance 
/////////////////////////////////////////////////////////////////////////////

TaskQueue* TaskQueue::g_pTaskQueue = NULL;

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

Task::Task()
{
    m_nTaskId = m_nTaskCount++;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Task::~Task()
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
    return g_pTaskQueue ? g_pTaskQueue : (g_pTaskQueue = new TaskQueue());
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

TaskQueue::TaskQueue() : m_bTermRequested( false )
{
    VERBOSE(VB_UPNP, "TaskQueue::ctor - Starting TaskQueue Thread...");

    start();

    VERBOSE(VB_UPNP, "TaskQueue::ctor - TaskQueue Thread Started.");
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

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void TaskQueue::run( )
{
    Task *pTask;

    VERBOSE(VB_UPNP, "TaskQueue::run - TaskQueue Thread Running.");

    while ( !m_bTermRequested )
    {
        // ------------------------------------------------------------------
        // Process Any Tasks that may need to be executed.
        // ------------------------------------------------------------------

        TaskTime ttNow;
        gettimeofday( (&ttNow), NULL );

        if ((pTask = GetNextExpiredTask( ttNow )) != NULL)
        {
            try
            {
                pTask->Execute( this );
                pTask->Release();
            }
            catch( ... )
            {
                cerr << "TaskQueue::run - Call to Execute threw an exception.";
            }

        }
        // Make sure to throttle our processing.

        msleep( 100 );
    }

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void TaskQueue::Clear( )
{
    m_mutex.lock(); 

    for ( TaskMap::iterator it  = m_mapTasks.begin();
                            it != m_mapTasks.end();
                          ++it )
    {
        if ((*it).second != NULL)
            (*it).second->Release();
    }

    m_mapTasks.clear();

    m_mutex.unlock(); 
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void TaskQueue::AddTask( long msec, Task *pTask )
{
    TaskTime tt;
    gettimeofday( (&tt), NULL );

    AddMicroSecToTaskTime( tt, (msec * 1000) );

    AddTask( tt, pTask );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void TaskQueue::AddTask( TaskTime ttKey, Task *pTask )
{

    if (pTask != NULL)
    {
        m_mutex.lock(); 
        pTask->AddRef();
        m_mapTasks.insert( TaskMap::value_type( ttKey, pTask ));
        m_mutex.unlock();
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void TaskQueue::AddTask( Task *pTask )
{

    if (pTask != NULL)
    {
        TaskTime tt;
        gettimeofday( (&tt), NULL );

        AddTask( tt, pTask );
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Task *TaskQueue::GetNextExpiredTask( TaskTime tt, long nWithinMilliSecs /*=50*/ )
{
    Task *pTask = NULL;

    AddMicroSecToTaskTime( tt, nWithinMilliSecs * 1000 );

    m_mutex.lock(); 

    TaskMap::iterator it = m_mapTasks.begin();

    if (it != m_mapTasks.end())
    {
        TaskTime ttTask = (*it).first;

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
