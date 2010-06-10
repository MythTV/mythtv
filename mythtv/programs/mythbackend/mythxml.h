//////////////////////////////////////////////////////////////////////////////
// Program Name: mythxml.h
//
// Purpose - Myth XML protocol HttpServerExtension
//
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:
//
//////////////////////////////////////////////////////////////////////////////

#ifndef MYTHXML_H_
#define MYTHXML_H_

#include <QDomDocument>
#include <QMap>
#include <QDateTime>

#include "upnp.h"
#include "eventing.h"

#include "autoexpire.h"
#include "mythcontext.h"
#include "jobqueue.h"
#include "recordinginfo.h"
#include "scheduler.h"

extern AutoExpire              *expirer;
extern Scheduler               *sched;

typedef enum
{
    MXML_Unknown                =  0,
    MXML_GetServiceDescription  =  1,

    MXML_GetProgramGuide        =  2,
    MXML_GetHosts               =  3,
    MXML_GetKeys                =  4,
    MXML_GetSetting             =  5,
    MXML_PutSetting             =  6,

    MXML_GetChannelIcon         =  7,
    MXML_GetRecorded            =  8,
    MXML_GetPreviewImage        =  9,

    MXML_GetRecording           = 10,
    MXML_GetMusic               = 11,

    MXML_GetExpiring            = 12,
    MXML_GetProgramDetails      = 13,
    MXML_GetVideo               = 14,

    MXML_GetConnectionInfo      = 15,
    MXML_GetAlbumArt            = 16,
    MXML_GetVideoArt            = 17,
    MXML_GetInternetSearch      = 18,
    MXML_GetInternetSources     = 19

} MythXMLMethod;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
//
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class MythXML : public Eventing
{
    private:

        QString                      m_sControlUrl;
        QString                      m_sServiceDescFileName;

        bool                         m_bIsMaster;
        int                          m_nPreRollSeconds;

    protected:

        // Implement UPnpServiceImpl methods that we can

        virtual QString GetServiceType      () { return "urn:schemas-mythtv-org:service:MythTv:1"; }
        virtual QString GetServiceId        () { return "urn:mythtv-org:serviceId:MYTHTV_1-0"; }
        virtual QString GetServiceControlURL() { return m_sControlUrl.mid( 1 ); }
        virtual QString GetServiceDescURL   () { return m_sControlUrl.mid( 1 ) + "/GetServDesc"; }

    private:

        MythXMLMethod GetMethod( const QString &sURI );

        void    GetProgramGuide( HTTPRequest *pRequest );
        void    GetProgramDetails( HTTPRequest *pRequest );

        void    GetHosts       ( HTTPRequest *pRequest );
        void    GetKeys        ( HTTPRequest *pRequest );
        void    GetSetting     ( HTTPRequest *pRequest );
        void    PutSetting     ( HTTPRequest *pRequest );

        void    GetChannelIcon ( HTTPRequest *pRequest );
        void    GetRecorded    ( HTTPRequest *pRequest );
        void    GetPreviewImage( HTTPRequest *pRequest );

        void    GetConnectionInfo( HTTPRequest *pRequest );
        void    GetAlbumArt    ( HTTPRequest *pRequest );
        void    GetVideoArt    ( HTTPRequest *pRequest );

        void    GetExpiring    ( HTTPRequest *pRequest );

        void    GetRecording   ( HttpWorkerThread *pThread,
                                 HTTPRequest      *pRequest );

        void    GetMusic       ( HttpWorkerThread *pThread,
                                 HTTPRequest      *pRequest );

        void    GetVideo       ( HttpWorkerThread *pThread,
                                 HTTPRequest      *pRequest );

        void    GetInternetSearch( HTTPRequest *pRequest );
        void    GetInternetSources( HTTPRequest *pRequest );

        void    GetDeviceDesc  ( HTTPRequest *pRequest );
        void    GetFile        ( HTTPRequest *pRequest, QString sFileName );

    public:
                 MythXML( UPnpDevice *pDevice , const QString &sSharePath);
        virtual ~MythXML();

        bool     ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );

        // Static methods shared with HttpStatus

        static void FillProgramInfo ( QDomDocument *pDoc,
                                      QDomNode     &node,
                                      ProgramInfo  *pInfo,
                                      bool          bIncChannel = true,
                                      bool          bDetails    = true );

        static void FillChannelInfo ( QDomElement  &channel,
                                      ProgramInfo  *pInfo,
                                      bool          bDetails = true );

};

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
            DT_Music     = 2,
            DT_Video     = 3


        } ThreadDataType;


        ThreadDataType  m_eType;

        QString         m_sChanId;
        QString         m_sStartTime;
        QString         m_sFileName;
        QString         m_sVideoID;

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
            m_eType        = DT_Recording;
            m_sChanId      = sChanId;
            m_sStartTime   = sStartTime;
            m_sFileName    = sFileName;
            m_nTrackNumber = 0;
        }

        ThreadData( const QString &sVideoID,
                    const QString &sFileName )
        {
            m_eType        = DT_Video;
            m_sVideoID     = sVideoID;
            m_sFileName    = sFileName;
            m_nTrackNumber = 0;
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


#endif
