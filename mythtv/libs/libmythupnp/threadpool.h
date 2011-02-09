/////////////////////////////////////////////////////////////////////////////
// Program Name: threadpool.h
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

#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <deque>
using namespace std;

#include <QString> 
#include <QMutex> 
#include <QWaitCondition>
#include <QThread>
#include <QTimer>
#include <QObject>
#include <QPointer>

class ThreadPool;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
//
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class CEvent 
{
    private:

        QMutex          m_mutex;
        QWaitCondition  m_wait;
        bool            m_bSignaled;

    public:

                  CEvent( bool bInitiallyOwn = false );
         virtual ~CEvent();

        bool      SetEvent    ();
        bool      ResetEvent  ();
        bool      IsSignaled  ();
        bool      WaitForEvent( unsigned long time = ULONG_MAX  );
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// WorkerEvent Class Declaration
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class WorkerThread;
class WorkerEvent : public QObject
{
    Q_OBJECT

    public:
        WorkerEvent(WorkerThread *parent) : m_parent(parent) { }

        virtual void customEvent(QEvent *e);

    public slots:
        void     TimeOut();
 
    private:
        WorkerThread *m_parent;
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// WorkerThread Class Declaration
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class WorkerThread : public QThread
{
    Q_OBJECT

    protected:

        QMutex              m_mutex;

        CEvent              m_Initialized;
        bool                m_bInitialized;
                         
        QPointer<ThreadPool> m_pThreadPool;

        volatile bool       m_bTermRequested;
        QString             m_sName;

        long                m_nIdleTimeoutMS;
        QTimer             *m_timer; // audited ref #5318
        WorkerEvent        *m_wakeup;

        void                WakeForWork();

    protected:

        virtual void  run();
        virtual void  ProcessWork() = 0;


    public:

                 WorkerThread( ThreadPool *pThreadPool, const QString &sName );
        virtual ~WorkerThread();

        bool     WaitForInitialized( unsigned long msecs );
        void     SetTimeout        ( long nIdleTimeout );

    public slots:
        void     SignalWork();

    friend class WorkerEvent;
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// CThreadPool Class Declaration
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

typedef deque<WorkerThread*> WorkerThreadList;

class ThreadPool : public QObject
{
    Q_OBJECT

    friend class WorkerThread;

    protected:

        QString                     m_sName;

        QMutex                      m_mList;

        QWaitCondition              m_threadAvail;

        WorkerThreadList            m_lstThreads;
        WorkerThreadList            m_lstAvailableThreads;

        int                         m_nInitialThreadCount;
        int                         m_nMaxThreadCount;
        long                        m_nIdleTimeout;

    protected:

        void            InitializeThreads();

        WorkerThread   *AddWorkerThread  ( bool bMakeAvailable, long nTimeout );

        void            ThreadAvailable  ( WorkerThread *pThread );
        void            ThreadTerminating( WorkerThread *pThread );

        virtual WorkerThread *CreateWorkerThread( ThreadPool *, const QString &sName ) = 0;

    public:

                        ThreadPool( const QString &sName );
        virtual        ~ThreadPool( );

        WorkerThread   *GetWorkerThread();

};


#endif
