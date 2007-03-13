//////////////////////////////////////////////////////////////////////////////
// Program Name: ssdp.h
//                                                                            
// Purpose - SSDP Discovery Service Implmenetation
//                                                                            
// Created By  : David Blain                    Created On : Oct. 1, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __SSDP_H__
#define __SSDP_H__

#include <qthread.h>
#include <qsocketdevice.h>
#include <qfile.h>

#include "httpserver.h"
#include "taskqueue.h"

#include "ssdpcache.h"
#include "upnptasknotify.h"

#define SSDP_GROUP      "239.255.255.250"
#define SSDP_PORT       1900
#define SSDP_SEARCHPORT 6549

typedef enum 
{
    SSDPM_Unknown         = 0,
    SSDPM_GetDeviceDesc   = 1,
    SSDPM_GetDeviceList   = 2

} SSDPMethod;

typedef enum
{
    SSDP_Unknown        = 0,
    SSDP_MSearch        = 1,
    SSDP_MSearchResp    = 2,
    SSDP_Notify         = 3

} SSDPRequestType;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDPThread Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#define SocketIdx_Search     0
#define SocketIdx_Multicast  1
#define SocketIdx_Broadcast  2

#define NumberOfSockets     (sizeof( m_Sockets ) / sizeof( QSocketDevice * ))

class SSDP : public QThread
{
    private:

        QSocketDevice      *m_Sockets[3];

        int                 m_nPort;
        int                 m_nSearchPort;
        int                 m_nServicePort;

        UPnpNotifyTask     *m_pNotifyTask;

        bool                m_bTermRequested;
        QMutex              m_lock;

    protected:

        bool    ProcessSearchRequest ( const QStringMap &sHeaders,
                                       QHostAddress  peerAddress,
                                       Q_UINT16      peerPort );
        bool    ProcessSearchResponse( const QStringMap &sHeaders );
        bool    ProcessNotify        ( const QStringMap &sHeaders );

        bool    IsTermRequested      ();

        QString GetHeaderValue    ( const QStringMap &headers, 
                                    const QString    &sKey, 
                                    const QString    &sDefault );

        void    ProcessData       ( QSocketDevice *pSocket );

        SSDPRequestType ProcessRequestLine( const QString &sLine );

    public:

                     SSDP   ( int nServicePort );
                    ~SSDP   ();

        virtual void run    ();

                void RequestTerminate(void);

                void EnableNotifications ();
                void DisableNotifications();

                void PerformSearch( const QString &sST );

};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// SSDPExtension Class Definition
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class SSDPExtension : public HttpServerExtension
{
    private:

        QString     m_sUPnpDescPath;
        int         m_nServicePort;

    private:

        SSDPMethod GetMethod( const QString &sURI );

        void       GetDeviceDesc( HTTPRequest *pRequest );
        void       GetFile      ( HTTPRequest *pRequest, QString sFileName );
        void       GetDeviceList( HTTPRequest *pRequest );

    public:
                 SSDPExtension( int nServicePort );
        virtual ~SSDPExtension( );

        bool     ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );
};

#endif
