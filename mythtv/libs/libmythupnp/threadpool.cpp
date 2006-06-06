/////////////////////////////////////////////////////////////////////////////
// Program Name: threadpool.cpp
//                                                                            
// Purpose - Thread Pool Class                                                              
//                                                                            
// Created By  : David Blain                    Created On : Oct 21, 2005
// Modified By :                                Modified On:                  
//                                                                            
/////////////////////////////////////////////////////////////////////////////

#include "threadpool.h"

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

CEvent::CEvent( bool bInitiallyOwn /*= FALSE */ ) 
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
    m_mutex.unlock();

    m_wait.wakeAll();

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
    if (IsSignaled())
        return( true );

    return( m_wait.wait( time ) );
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
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

WorkerThread::~WorkerThread()
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool WorkerThread::IsTermRequested()
{
    m_mutex.lock();
    bool bTermRequested = m_bTermRequested;
    m_mutex.unlock();

    return( bTermRequested );
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

void WorkerThread::RequestTerminate()
{
    m_mutex.lock();  
    m_bTermRequested = true; 
    m_mutex.unlock();

    m_WorkAvailable.SetEvent();

    // Give time for thread to terminate.

    wait( 500 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void WorkerThread::SignalWork()
{
    m_WorkAvailable.SetEvent();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void WorkerThread::run( void )
{
    m_mutex.lock();
    m_bInitialized = true;
    m_mutex.unlock();

    m_Initialized.SetEvent();

    while( !IsTermRequested() )
    {
        if (m_WorkAvailable.WaitForEvent(500))
        {
            m_WorkAvailable.ResetEvent();

            if ( !IsTermRequested() )
            {
                try
                {
                    ProcessWork();
                }
                catch(...)
                {
                    VERBOSE( VB_IMPORTANT, QString( "WorkerThread::Run( %1 ) - Unexpected Exception." )
                                            .arg( m_sName ));
                }

                m_pThreadPool->ThreadAvailable( this );
            }
        }
    }

    if (m_pThreadPool != NULL )
    {
        m_pThreadPool->ThreadTerminating( this );
        m_pThreadPool = NULL;
    }
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

    m_lstThreads         .setAutoDelete( false );
    m_lstAvailableThreads.setAutoDelete( false );

    m_nInitialThreadCount = gContext->GetNumSetting( "ThreadCountInitial_" + m_sName, 1 );
    m_nMaxThreadCount     = gContext->GetNumSetting( "ThreadCountMax_"     + m_sName, 5 );

    m_nInitialThreadCount = min( m_nInitialThreadCount, m_nMaxThreadCount );

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

        if (pThread != NULL)
        {
            pThread->RequestTerminate();

            delete pThread;
        }

        it = m_lstThreads.erase( it );
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
        AddWorkerThread( true );

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
        // See if we have a worker thread availible.
        // --------------------------------------------------------------
        
        m_mList.lock();
        {
            if ( m_lstAvailableThreads.count() > 0)
            {
                pThread = m_lstAvailableThreads.getFirst();                
            
                m_lstAvailableThreads.removeFirst();                
            }
        
            nThreadCount = m_lstThreads.count();
        }
        m_mList.unlock();

        if (pThread == NULL)
        {
            // ----------------------------------------------------------
            // Check to see if we need to create a new thread or 
            // wait for one to become available.
            // ----------------------------------------------------------
        
            if ( nThreadCount < m_nMaxThreadCount)
                pThread = AddWorkerThread( false );
            else
            {
                if (m_threadAvail.wait( 5000 ) == false )
                    return( NULL );     // timeout exceeded.
            }
        }
    }

    return( pThread );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

WorkerThread *ThreadPool::AddWorkerThread( bool bMakeAvailable )
{
    QString sName = m_sName + "_WorkerThread"; 

    WorkerThread *pThread = CreateWorkerThread( this, sName );

    if (pThread != NULL)
    {
        pThread->start();

        if (pThread->WaitForInitialized( 5000 ))
        {
            // ------------------------------------------------------
            // Add new worker thread to list.
            // ------------------------------------------------------

            m_mList.lock();
            {

                m_lstThreads.append( pThread );
                
                if (bMakeAvailable)
                {
                    m_lstAvailableThreads.append( pThread );
                
                    m_threadAvail.wakeAll();
                }
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
    m_lstAvailableThreads.prepend( pThread );
    m_mList.unlock();

    m_threadAvail.wakeAll();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void ThreadPool::ThreadTerminating ( WorkerThread *pThread )
{
    m_mList.lock();
    {
        m_lstAvailableThreads.remove( pThread );

        // Need to leave in m_lstThreads so that we can delete the ptr in destructor
    }
    m_mList.unlock();
}

