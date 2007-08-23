//////////////////////////////////////////////////////////////////////////////
// Program Name: httpstatus.h
//                                                                            
// Purpose - Html & XML status HttpServerExtension 
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef HTTPSTATUS_H_
#define HTTPSTATUS_H_

#include <qdom.h>
#include <qdatetime.h> 

#include "httpserver.h"
#include "mainserver.h"
#include "autoexpire.h"
#include "mythcontext.h"
#include "jobqueue.h"
#include "programinfo.h"


typedef enum 
{
    HSM_Unknown         =  0,
    HSM_GetStatusHTML   =  1,
    HSM_GetStatusXML    =  2

} HttpStatusMethod;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// 
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class HttpStatus : public HttpServerExtension
{
    private:

        Scheduler                   *m_pSched;
        QMap<int, EncoderLink *>    *m_pEncoders;
        AutoExpire                  *m_pExpirer;
        bool                         m_bIsMaster;
        int                          m_nPreRollSeconds;
        QMutex                       m_settingLock;

    private:

        HttpStatusMethod GetMethod( const QString &sURI );

        void    GetStatusXML      ( HTTPRequest *pRequest );
        void    GetStatusHTML     ( HTTPRequest *pRequest );

        void    FillStatusXML     ( QDomDocument *pDoc);
    
        void    PrintStatus       ( QTextStream &os, QDomDocument *pDoc );
        int     PrintEncoderStatus( QTextStream &os, QDomElement encoders );
        int     PrintScheduled    ( QTextStream &os, QDomElement scheduled );
        int     PrintJobQueue     ( QTextStream &os, QDomElement jobs );
        int     PrintMachineInfo  ( QTextStream &os, QDomElement info );

    public:
                 HttpStatus( QMap<int, EncoderLink *> *tvList, Scheduler *sched, AutoExpire *expirer, bool bIsMaster );
        virtual ~HttpStatus();

        bool     ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );
};

#endif
