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
    HSM_GetStatusXML    =  2,
    HSM_GetProgramGuide =  3,
    
    HSM_GetHosts        =  4,
    HSM_GetKeys         =  5,
    HSM_GetSetting      =  6,
    HSM_PutSetting      =  7,
    
    HSM_GetChannelIcon  =  8,
    HSM_GetRecorded     =  9,
    HSM_GetPreviewImage = 10,

    HSM_GetRecording    = 11,
    HSM_GetMusic        = 12,
    
    HSM_GetDeviceDesc   = 13,
    HSM_GetCDSDesc      = 14,
    HSM_GetCMGRDesc     = 15,
    
    HSM_Asterisk        = 16,

    HSM_GetExpiring       = 17,
    HSM_GetProgramDetails = 18

} HttpStatusMethod;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// 
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class ThreadData : public HttpWorkerData
{
    public:

        typedef enum
        {
            DT_Unknown   = 0,
            DT_Recording = 1,
            DT_Music     = 2

        } ThreadDataType;
        

        ThreadDataType  m_eType;

        QString         m_sChanId;   
        QString         m_sStartTime;
        QString         m_sFileName;

        int             m_nTrackNumber;

    public:

        ThreadData( long nTrackNumber, const QString &sFileName )
        {
            m_eType        = DT_Music;
            m_nTrackNumber = nTrackNumber;
            m_sFileName    = sFileName;
        }

        ThreadData( const QString &sChanId,
                    const QString &sStartTime,
                    const QString &sFileName )
        {
            m_eType      = DT_Recording;
            m_sChanId    = sChanId;
            m_sStartTime = sStartTime;
            m_sFileName  = sFileName;
        }

        virtual ~ThreadData() 
        {
        }

        bool IsSameRecording( const QString &sChanId, 
                              const QString &sStartTime )
        {
            return( (sChanId == m_sChanId ) && (sStartTime == m_sStartTime ));
        }
};


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

    private:

        HttpStatusMethod GetMethod( const QString &sURI );

        void    GetStatusXML   ( HTTPRequest *pRequest );
        void    GetStatusHTML  ( HTTPRequest *pRequest );

        void    GetProgramGuide( HTTPRequest *pRequest );
        void    GetProgramDetails( HTTPRequest *pRequest );

        void    GetHosts       ( HTTPRequest *pRequest );
        void    GetKeys        ( HTTPRequest *pRequest );
        void    GetSetting     ( HTTPRequest *pRequest );
        void    PutSetting     ( HTTPRequest *pRequest );

        void    GetChannelIcon ( HTTPRequest *pRequest );
        void    GetRecorded    ( HTTPRequest *pRequest );
        void    GetPreviewImage( HTTPRequest *pRequest );

        void    GetExpiring    ( HTTPRequest *pRequest );

        void    GetRecording   ( HttpWorkerThread *pThread, 
                                 HTTPRequest      *pRequest );

        void    GetMusic       ( HttpWorkerThread *pThread, 
                                 HTTPRequest      *pRequest );

        void    GetDeviceDesc  ( HTTPRequest *pRequest ); 
        void    GetFile        ( HTTPRequest *pRequest, QString sFileName );

        void    FillProgramInfo ( QDomDocument *pDoc, 
                                  QDomElement  &e, 
                                  ProgramInfo  *pInfo,
                                  bool          bIncChannel = true,
                                  bool          bDetails    = true );

        void    FillStatusXML   ( QDomDocument *pDoc);
        void    FillChannelInfo ( QDomElement  &channel, 
                                  ProgramInfo  *pInfo,
                                  bool          bDetails = true );

    
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
