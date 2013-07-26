//////////////////////////////////////////////////////////////////////////////
// Program Name: taskqueue.h
// Created     : Oct. 24, 2005
//
// Purpose     : Used to process delayed tasks
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __TASKQUEUE_H__
#define __TASKQUEUE_H__

// POSIX headers
#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif // _WIN32

// C++ headers
#include <map>

// Qt headers
#include <QMutex>

// MythTV headers
#include "referencecounter.h"
#include "upnputil.h"
#include "mthread.h"
#include "upnpexp.h"

class Task;
class TaskQueue;

/////////////////////////////////////////////////////////////////////////////
// Typedefs
/////////////////////////////////////////////////////////////////////////////

typedef std::multimap< TaskTime, Task *> TaskMap;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Task Class Definition
// 
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class Task : public ReferenceCounter
{
    protected:
        static long m_nTaskCount;

        long        m_nTaskId;

    protected:

        // Destructor protected to force use of Release Method

        virtual        ~Task();

    public:

                        Task(const QString &debugName); 

                long    Id() const { return( m_nTaskId ); }

        virtual void    Execute( TaskQueue *pQueue ) = 0;
        virtual QString Name   () = 0;

};


/////////////////////////////////////////////////////////////////////////////
// TaskQueue Singleton
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC TaskQueue : public MThread
{
    private: 

        // Singleton instance used by all.
        static TaskQueue*        g_pTaskQueue;  

    protected:

        TaskMap     m_mapTasks;
        QMutex      m_mutex;
        bool        m_bTermRequested;

    protected:

        bool  IsTermRequested();

        virtual void run    ();

    private:

        // ------------------------------------------------------------------
        // Private so the singleton pattern can be inforced.
        // ------------------------------------------------------------------

        TaskQueue();

    public:

        static TaskQueue* Instance();
        static void Shutdown();

        virtual ~TaskQueue();

        void  RequestTerminate   ( );

        void  Clear              ( );
        void  AddTask            ( long msec  , Task *pTask );
        void  AddTask            ( TaskTime tt, Task *pTask );
        void  AddTask            ( Task *pTask );
                                                          
        Task *GetNextExpiredTask ( TaskTime tt, long nWithinMilliSecs = 50 );
                                                                
};

#endif
