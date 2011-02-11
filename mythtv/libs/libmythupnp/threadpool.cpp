/////////////////////////////////////////////////////////////////////////////
// Program Name: threadpool.cpp
// Created     : Oct. 21, 2005
//
// Purpose     : Thread Pool Class
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

#include <QCoreApplication>

#include <algorithm>
using namespace std;

#include "threadpool.h"
#include "upnp.h"       // only needed for Config... remove once config is moved.

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// CEvent Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

CEvent::CEvent( bool bInitiallyOwn /*= false */ ) 
{
    m_bSignaled = bInitiallyOwn;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

CEvent::~CEvent() 
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool CEvent::SetEvent   ()
{
    m_mutex.lock();
    m_bSignaled = true;

    m_wait.wakeAll();

    m_mutex.unlock();

    return( true );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool CEvent::ResetEvent ()
{
    m_mutex.lock();
    m_bSignaled = false;
    m_mutex.unlock();

    return( true );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool CEvent::IsSignaled()
{
    m_mutex.lock();
    bool bSignaled = m_bSignaled;
    m_mutex.unlock();

    return( bSignaled );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool CEvent::WaitForEvent( unsigned long time /*= ULONG_MAX */ ) 
{
    m_mutex.lock(); 

    if (m_bSignaled) 
    { 
        m_mutex.unlock(); 
        return true; 
    }    
    
    bool ret = m_wait.wait(&m_mutex, time); 
    
    m_mutex.unlock(); 
    
    return ret;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// WorkerEvent Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void WorkerEvent::customEvent(QEvent *e)
{
    if (m_parent)
        m_parent->WakeForWork();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void WorkerEvent::TimeOut()
{
    if (m_parent && m_parent->m_nIdleTimeoutMS > 0 && !m_parent->m_bTermRequested)
        m_parent->quit();
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// WorkerThread Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

WorkerThread::WorkerThread( ThreadPool *pThreadPool, const QString &sName )
{
    m_bInitialized   = false;
    m_bTermRequested = false;
    m_pThreadPool    = pThreadPool;
    m_sName          = sName;
    m_nIdleTimeoutMS = 60000;
    m_timer          = 0;
    m_wakeup         = 0;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

WorkerThread::~WorkerThread()
{
    m_bTermRequested = true;

    quit();
    wait();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool WorkerThread::WaitForInitialized( unsigned long msecs )
{
    m_mutex.lock();
    bool bInitialized = m_bInitialized;
    m_mutex.unlock();

    if (bInitialized)
        return true;

    return( m_Initialized.WaitForEvent( msecs ));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void WorkerThread::SignalWork()
{
    if (!m_bInitialized)
        return;

    QCoreApplication::postEvent(m_wakeup, new QEvent(QEvent::User));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void WorkerThread::WakeForWork()
{
    try
    {
        m_timer->stop();
        ProcessWork();

        if (m_nIdleTimeoutMS > 0)
            m_timer->start(m_nIdleTimeoutMS);
    }
    catch(...)
    {
        VERBOSE( VB_IMPORTANT, QString( "WorkerThread::Run( %1 ) - Unexpected Exception." )
                 .arg( m_sName ));
    }

    if (!m_bTermRequested)
    {
        m_pThreadPool->ThreadAvailable( this );
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void WorkerThread::SetTimeout( long nIdleTimeout )
{
    m_nIdleTimeoutMS = nIdleTimeout;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void WorkerThread::run( void )
{
    m_timer = new QTimer();
    m_timer->setSingleShot(true);

    m_wakeup = new WorkerEvent(this);

    m_mutex.lock();
    m_bInitialized = true;
    m_mutex.unlock();

    m_Initialized.SetEvent();

    connect(m_timer, SIGNAL(timeout()), m_wakeup, SLOT(TimeOut()));
    if (m_nIdleTimeoutMS > 0)
        m_timer->start(m_nIdleTimeoutMS);

    exec();

    if (m_pThreadPool != NULL )
    {
        m_pThreadPool->ThreadTerminating( this );
        m_pThreadPool = NULL;
    }

    delete m_wakeup;
    delete m_timer;

    VERBOSE( VB_UPNP, QString( "WorkerThread:Run - Exiting: %1" ).arg( m_sName ));
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// CThreadPool Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

ThreadPool::ThreadPool( const QString &sName )
{
    m_sName = sName;

    m_nInitialThreadCount = UPnp::g_pConfig->GetValue( "ThreadPool/" + m_sName + "/Initial", 1 );
    m_nMaxThreadCount     = UPnp::g_pConfig->GetValue( "ThreadPool/" + m_sName + "/Max"    , 25 );
    m_nIdleTimeout        = UPnp::g_pConfig->GetValue( "ThreadPool/" + m_sName + "/Timeout", 60000 );

    m_nInitialThreadCount = min( m_nInitialThreadCount, m_nMaxThreadCount );

    VERBOSE(VB_IMPORTANT, QString("ThreadPool:%1: Initial %2, Max %3, Timeout %4")
        .arg(sName) .arg(m_nInitialThreadCount) .arg(m_nMaxThreadCount)
        .arg(m_nIdleTimeout) );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

ThreadPool::~ThreadPool( )
{
    // --------------------------------------------------------------
    // Request Termination of all worker threads.
    // --------------------------------------------------------------

    // --------------------------------------------------------------
    // If we lock the m_mList mutex, a deadlock will occur due to the Worker 
    // thread removing themselves from the Avail Deque... should be relatively
    // safe to use this list's iterator at this time without the lock.
    // --------------------------------------------------------------

    WorkerThreadList::iterator it = m_lstThreads.begin(); 
    
    while (it != m_lstThreads.end() )
    {
        WorkerThread *pThread = *it;
        it = m_lstThreads.erase( it );

        if (pThread != NULL) 
            pThread->deleteLater();
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void ThreadPool::InitializeThreads()
{
    // --------------------------------------------------------------
    // Create the m_nInitialThreadCount threads...
    // --------------------------------------------------------------

    for (long nIdx = 0; nIdx < m_nInitialThreadCount; nIdx ++ )
        AddWorkerThread( true, -1 );

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

WorkerThread *ThreadPool::GetWorkerThread()
{
    WorkerThread *pThread     = NULL;
    long          nThreadCount= 0;

    while (pThread == NULL)
    {
        // --------------------------------------------------------------
        // See if we have a worker thread available.
        // --------------------------------------------------------------
        
        m_mList.lock();
        if ( m_lstAvailableThreads.size() > 0)
        {
            pThread = m_lstAvailableThreads.front();                
            m_lstAvailableThreads.pop_front();
        }
        
        nThreadCount = m_lstThreads.size();
        m_mList.unlock();

        if (pThread == NULL)
        {
            // ----------------------------------------------------------
            // Check to see if we need to create a new thread or 
            // wait for one to become available.
            // ----------------------------------------------------------
        
            if ( nThreadCount < m_nMaxThreadCount)
                pThread = AddWorkerThread( false, m_nIdleTimeout );
            else
            {
                QMutex mutex;
                mutex.lock();

                if (m_threadAvail.wait( &mutex, 5000 ) == false )
                {
                    VERBOSE(VB_IMPORTANT, QString("ThreadPool:%1: thread pool exhausted (max %2 threads)") .arg(m_sName) .arg(m_nMaxThreadCount));
                    return( NULL );     // timeout exceeded.
                }
            }
        }
    }

    return( pThread );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

WorkerThread *ThreadPool::AddWorkerThread( bool bMakeAvailable, long nTimeout )
{
    QString sName = QString(m_sName + "_WorkerThread"); 
    long nThreadCount;

    VERBOSE( VB_UPNP, QString( "ThreadPool:AddWorkerThread - %1" ).arg( sName ));

    WorkerThread *pThread = CreateWorkerThread( this, sName );

    if (pThread != NULL)
    {
        pThread->SetTimeout( nTimeout );
        pThread->start();

        if (pThread->WaitForInitialized( 5000 ))
        {
            // ------------------------------------------------------
            // Add new worker thread to list.
            // ------------------------------------------------------

            m_mList.lock();
            m_lstThreads.push_back( pThread );
            nThreadCount = m_lstThreads.size();

            VERBOSE(VB_IMPORTANT|VB_EXTRA, QString("ThreadPool:%1: thread pool size %2") .arg(m_sName) .arg(nThreadCount));
                
            if (bMakeAvailable)
            {
                m_lstAvailableThreads.push_back( pThread );
                m_threadAvail.wakeAll();
            }
            m_mList.unlock();

        }
        else
        {
            // ------------------------------------------------------
            // It's taking longer than 5 seconds to initialize this thread.... 
            // give up on it.
            // (This should never happen)
            // ------------------------------------------------------

            delete pThread;
            pThread = NULL;
        }
    }

    return( pThread );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void ThreadPool::ThreadAvailable ( WorkerThread *pThread )
{
    m_mList.lock();
    m_lstAvailableThreads.push_front(pThread);
    m_mList.unlock();

    m_threadAvail.wakeAll();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void ThreadPool::ThreadTerminating ( WorkerThread *pThread )
{
    long nThreadCount;

    m_mList.lock();
    WorkerThreadList::iterator it = find(m_lstAvailableThreads.begin(),
                                         m_lstAvailableThreads.end(), pThread);
    m_lstAvailableThreads.erase(it);

    it = find(m_lstThreads.begin(), m_lstThreads.end(), pThread);
    m_lstThreads.erase(it);

    nThreadCount = m_lstThreads.size();
    VERBOSE(VB_IMPORTANT|VB_EXTRA, QString("ThreadPool:%1: thread pool size %2") .arg(m_sName) .arg(nThreadCount));

    pThread->deleteLater();

    m_mList.unlock();
}

