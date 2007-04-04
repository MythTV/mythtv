//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpdevice.h
//                                                                            
// Purpose - UPnp Device Description parser/generator
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __UPNPDEVICE_H__
#define __UPNPDEVICE_H__

#include "upnputil.h"
#include "refcounted.h"
#include "mythcontext.h"

#include <qdom.h>
#include <qurl.h>
#include <sys/time.h>


extern const char *myth_source_version;

class UPnpDeviceDesc;
class UPnpDevice;
class UPnpService;
class UPnpIcon;

/////////////////////////////////////////////////////////////////////////////
// Typedefs
/////////////////////////////////////////////////////////////////////////////

typedef QPtrList< UPnpDevice  >  UPnpDeviceList;
typedef QPtrList< UPnpService >  UPnpServiceList;
typedef QPtrList< UPnpIcon    >  UPnpIconList;


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

class UPnpIcon
{
    public:

        QString     m_sMimeType;
        int         m_nWidth;
        int         m_nHeight;
        int         m_nDepth;
        QString     m_sURL;

        UPnpIcon() : m_nWidth ( 0 ), m_nHeight( 0 ), m_nDepth( 0 )  {}
};

/////////////////////////////////////////////////////////////////////////////

class UPnpService
{
    public:
        QString     m_sServiceType;
        QString     m_sServiceId;
        QString     m_sSCPDURL;
        QString     m_sControlURL;
        QString     m_sEventSubURL;

        UPnpService() {}        
};

/////////////////////////////////////////////////////////////////////////////

class UPnpDevice
{
    public:

        QString         m_sDeviceType;
        QString         m_sFriendlyName;
        QString         m_sManufacturer;
        QString         m_sManufacturerURL;
        QString         m_sModelDescription;
        QString         m_sModelName;
        QString         m_sModelNumber;
        QString         m_sModelURL;
        QString         m_sSerialNumber;
        QString         m_sUPC;
        QString         m_sPresentationURL;
        QString         m_sUDN;

        NameValueList   m_lstExtra;

        UPnpIconList    m_listIcons;
        UPnpServiceList m_listServices;
        UPnpDeviceList  m_listDevices;

    public:

        UPnpDevice()
        {
            m_sModelNumber  = MYTH_BINARY_VERSION;
            m_sSerialNumber = myth_source_version;

            m_listIcons   .setAutoDelete( true );
            m_listServices.setAutoDelete( true );
            m_listDevices .setAutoDelete( true );
        }

        QString GetUDN()
        {
            if (m_sUDN.isEmpty())
                m_sUDN = "uuid:" + LookupUDN( m_sDeviceType );

            return m_sUDN;
        }
};


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// 
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class UPnpDeviceDesc
{
    public:

        UPnpDevice      m_rootDevice;
        QString         m_sHostName;
        QUrl            m_HostUrl;

    protected: 

        void    _InternalLoad( QDomNode  oNode, UPnpDevice *pCurDevice );

        void     ProcessIconList   ( QDomNode oListNode, UPnpDevice *pDevice );
        void     ProcessServiceList( QDomNode oListNode, UPnpDevice *pDevice );
        void     ProcessDeviceList ( QDomNode oListNode, UPnpDevice *pDevice );

        void     OutputDevice( QTextStream &os, 
                               UPnpDevice *pDevice, 
                               const QString &sUserAgent = "" );

        void     SetStrValue ( const QDomNode &n, QString &sValue );
        void     SetNumValue ( const QDomNode &n, int     &nValue );

        QString  FormatValue ( const QString &sName, const QString &sValue );
        QString  FormatValue ( const QString &sName, int nValue );

        QString  GetHostName ();

    public:

                 UPnpDeviceDesc();
        virtual ~UPnpDeviceDesc();

        bool     Load       ( const QString &sFileName );
        bool     Load       ( const QDomDocument &xmlDevDesc );

        void     GetValidXML( const QString &sBaseAddress, int nPort, QTextStream &os, const QString &sUserAgent = "" );
        QString  GetValidXML( const QString &sBaseAddress, int nPort );

        QString  FindDeviceUDN( UPnpDevice *pDevice, QString sST );

        UPnpDevice *FindDevice( const QString &sURI );

        static UPnpDevice     *FindDevice( UPnpDevice *pDevice, const QString &sURI );
        static UPnpDeviceDesc *Retrieve  ( QString &sURL, bool bInQtThread = TRUE   );

};

/////////////////////////////////////////////////////////////////////////////
// DeviceLocation Class Definition/Implementation
/////////////////////////////////////////////////////////////////////////////

class DeviceLocation : public RefCounted
{
    public:

        static int      g_nAllocated;       // Debugging only

    protected:

        // ==================================================================
        // Destructor protected to force use of Release Method
        // ==================================================================

        virtual        ~DeviceLocation()
        {
            // Should be atomic decrement
            g_nAllocated--;

            if (m_pDeviceDesc != NULL)
                delete m_pDeviceDesc;
        }

        UPnpDeviceDesc *m_pDeviceDesc;  // We take ownership of this pointer.

    public:

        QString     m_sURI;           // Service Type URI
        QString     m_sUSN;           // Unique Service Name
        QString     m_sLocation;      // URL to Device Description
        TaskTime    m_ttExpires;        
        QString     m_sSecurityPin;   // Use for MythXML methods needed pin

    public:

        // ==================================================================

        DeviceLocation( const QString &sURI,
                        const QString &sUSN,
                        const QString &sLocation,
                        TaskTime       ttExpires ) : m_pDeviceDesc( NULL      ),
                                                     m_sURI       ( sURI      ),
                                                     m_sUSN       ( sUSN      ),
                                                     m_sLocation  ( sLocation ),
                                                     m_ttExpires  ( ttExpires )
        {
            // Should be atomic increment
            g_nAllocated++;
        }

        // ==================================================================

        int ExpiresInSecs()
        {
            TaskTime ttNow;
            gettimeofday( &ttNow, NULL );

            return m_ttExpires.tv_sec - ttNow.tv_sec;
        }

        // ==================================================================

        UPnpDeviceDesc *GetDeviceDesc( bool bInQtThread = TRUE )
        {
            if (m_pDeviceDesc == NULL)
                m_pDeviceDesc = UPnpDeviceDesc::Retrieve( m_sLocation, bInQtThread );

            return m_pDeviceDesc;
        }

        // ==================================================================

        QString GetFriendlyName( bool bInQtThread = TRUE )
        {
            UPnpDeviceDesc *pDevice = GetDeviceDesc( bInQtThread );

            if ( pDevice != NULL)
                return pDevice->m_rootDevice.m_sFriendlyName;

            return "<Unknown>";
        }


};

#endif
