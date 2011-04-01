//////////////////////////////////////////////////////////////////////////////
// Program Name: taskqueue.h
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

#ifndef __TASKQUEUE_H__
#define __TASKQUEUE_H__

// POSIX headers
#include <sys/types.h>
#ifndef USING_MINGW
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif // USING_MINGW

// C++ headers
#include <map>

// Qt headers
#include <QThread>

// MythTV headers
#include "upnputil.h"
#include "refcounted.h"

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

class Task : public RefCounted 
{
    protected:
        static long m_nTaskCount;

        long        m_nTaskId;

    protected:

        // Destructor protected to force use of Release Method

        virtual        ~Task();

    public:

                        Task(); 

                long    Id() { return( m_nTaskId ); }

        virtual void    Execute( TaskQueue *pQueue ) = 0;
        virtual QString Name   () = 0;

};


/////////////////////////////////////////////////////////////////////////////
// TaskQueue Singleton
/////////////////////////////////////////////////////////////////////////////

class TaskQueue : public QThread
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

        virtual ~TaskQueue();

        void  RequestTerminate   ( );

        void  Clear              ( );
        void  AddTask            ( long msec  , Task *pTask );
        void  AddTask            ( TaskTime tt, Task *pTask );
        void  AddTask            ( Task *pTask );
                                                          
        Task *GetNextExpiredTask ( TaskTime tt, long nWithinMilliSecs = 50 );
                                                                
};

#endif
