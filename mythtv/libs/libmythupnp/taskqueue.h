//////////////////////////////////////////////////////////////////////////////
// Program Name: taskqueue.h
//                                                                            
// Purpose - Used to process delayed tasks
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __TASKQUEUE_H__
#define __TASKQUEUE_H__

#include <qthread.h>
#include <qsocketdevice.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>

#include "upnpglobal.h"
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
//
/////////////////////////////////////////////////////////////////////////////

class TaskQueue : public QThread
{
    protected:

        TaskMap     m_mapTasks;
        QMutex      m_mutex;
        bool        m_bTermRequested;

    protected:

        bool  IsTermRequested();

        virtual void run    ();

    public:

                 TaskQueue();
        virtual ~TaskQueue();

        void  RequestTerminate   ( );

        void  Clear              ( );
        void  AddTask            ( long msec  , Task *pTask );
        void  AddTask            ( TaskTime tt, Task *pTask );
        void  AddTask            ( Task *pTask );
                                                          
        Task *GetNextExpiredTask ( TaskTime tt, long nWithinMilliSecs = 50 );
                                                                
};

#endif
