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
        int     PrintEncoderStatus( QTextStream &os, const QDomElement& encoders );
        int     PrintScheduled    ( QTextStream &os, const QDomElement& scheduled );
        int     PrintFrontends    ( QTextStream &os, const QDomElement& frontends );
        int     PrintBackends     ( QTextStream &os, const QDomElement& backends );
        int     PrintJobQueue     ( QTextStream &os, const QDomElement& jobs );
        int     PrintMachineInfo  ( QTextStream &os, const QDomElement& info );
        int     PrintMiscellaneousInfo ( QTextStream &os, const QDomElement& info );

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
        virtual ~HttpStatus() = default;

        void     SetMainServer(MainServer *mainServer)
                    { m_pMainServer = mainServer; }

        QStringList GetBasePaths() override; // HttpServerExtension
        
        bool     ProcessRequest( HTTPRequest *pRequest ) override; // HttpServerExtension
};

#endif
