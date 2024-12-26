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

#include "libmythbase/programinfo.h"
#include "libmythupnp/httpserver.h"

enum HttpStatusMethod : std::uint8_t
{
    HSM_Unknown         =  0,
    HSM_GetStatusHTML   =  1,
    HSM_GetStatusXML    =  2

};

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
        MainServer                  *m_pMainServer{nullptr};
        bool                         m_bIsMaster;
        int                          m_nPreRollSeconds;
        QMutex                       m_settingLock;

    private:

        static HttpStatusMethod GetMethod( const QString &sURI );

        void    GetStatusXML      ( HTTPRequest *pRequest );
        void    GetStatusHTML     ( HTTPRequest *pRequest );

        void    FillStatusXML     ( QDomDocument *pDoc);
    
        static void    PrintStatus       ( QTextStream &os, QDomDocument *pDoc );
        static int     PrintEncoderStatus( QTextStream &os, const QDomElement& encoders );
        static int     PrintScheduled    ( QTextStream &os, const QDomElement& scheduled );
        static int     PrintFrontends    ( QTextStream &os, const QDomElement& frontends );
        static int     PrintBackends     ( QTextStream &os, const QDomElement& backends );
        static int     PrintJobQueue     ( QTextStream &os, const QDomElement& jobs );
        static int     PrintMachineInfo  ( QTextStream &os, const QDomElement& info );
        static int     PrintMiscellaneousInfo ( QTextStream &os, const QDomElement& info );

        static void    FillProgramInfo   ( QDomDocument *pDoc,
                                           QDomNode     &node,
                                           ProgramInfo  *pInfo,
                                           bool          bIncChannel = true,
                                           bool          bDetails    = true );

        static void    FillChannelInfo   ( QDomElement  &channel,
                                           ProgramInfo  *pInfo,
                                           bool          bDetails = true );


    public:
                 HttpStatus( QMap<int, EncoderLink *> *tvList, Scheduler *sched,
                             bool bIsMaster );
        ~HttpStatus() override = default;

        void     SetMainServer(MainServer *mainServer)
                    { m_pMainServer = mainServer; }

        QStringList GetBasePaths() override; // HttpServerExtension
        
        bool     ProcessRequest( HTTPRequest *pRequest ) override; // HttpServerExtension
};

#endif
