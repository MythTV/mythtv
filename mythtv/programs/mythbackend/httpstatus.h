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

#include <QDomDocument>
#include <QMutex>
#include <QMap>

#include "httpserver.h"
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

class Scheduler;
class AutoExpire;
class EncoderLink;
class MainServer;

class HttpStatus : public HttpServerExtension
{
    private:

        Scheduler                   *m_pSched;
        QMap<int, EncoderLink *>    *m_pEncoders;
        AutoExpire                  *m_pExpirer;
        MainServer                  *m_pMainServer;
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
        int     PrintMiscellaneousInfo ( QTextStream &os, QDomElement info );

        void    FillProgramInfo   ( QDomDocument *pDoc,
                                    QDomNode     &node,
                                    ProgramInfo  *pInfo,
                                    bool          bIncChannel = true,
                                    bool          bDetails    = true );

        void    FillChannelInfo   ( QDomElement  &channel,
                                    ProgramInfo  *pInfo,
                                    bool          bDetails = true );


    public:
                 HttpStatus( QMap<int, EncoderLink *> *tvList, Scheduler *sched,
                             AutoExpire *expirer, bool bIsMaster );
        virtual ~HttpStatus();

        void     SetMainServer(MainServer *mainServer)
                    { m_pMainServer = mainServer; }

        virtual QStringList GetBasePaths();
        
        bool     ProcessRequest( HttpWorkerThread *pThread,
                                 HTTPRequest *pRequest );
};

#endif
