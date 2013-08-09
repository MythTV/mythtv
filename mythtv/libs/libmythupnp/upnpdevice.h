//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpdevice.h
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Device Description parser/generator
//
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __UPNPDEVICE_H__
#define __UPNPDEVICE_H__

#include <QDomDocument>
#include <QUrl>

#include "compat.h"
#include "upnpexp.h"
#include "upnputil.h"
#include "mythtypes.h"
#include "referencecounter.h"

class UPnpDeviceDesc;
class UPnpDevice;
class UPnpService;
class UPnpIcon;
class QTextStream;

/////////////////////////////////////////////////////////////////////////////
// Typedefs
/////////////////////////////////////////////////////////////////////////////

typedef QList< UPnpDevice*  >  UPnpDeviceList;
typedef QList< UPnpService* >  UPnpServiceList;
typedef QList< UPnpIcon*    >  UPnpIconList;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC UPnpIcon
{
  public:
    QString     m_sURL;
    QString     m_sMimeType;
    int         m_nWidth;
    int         m_nHeight;
    int         m_nDepth;

    UPnpIcon() : m_nWidth(0), m_nHeight(0), m_nDepth(0) {}

    QString toString(uint padding) const
    {
        QString pad;
        for (uint i = 0; i < padding; i++)
            pad += " ";
        return QString("%0Icon %1 %2x%3^%4 %5")
            .arg(pad).arg(m_sURL).arg(m_nWidth).arg(m_nHeight)
            .arg(m_nDepth).arg(m_sMimeType);
    }
};

/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC UPnpService
{
  public:
    QString m_sServiceType;
    QString m_sServiceId;
    QString m_sSCPDURL;
    QString m_sControlURL;
    QString m_sEventSubURL;

    UPnpService() {}        

    QString toString(uint padding) const
    {
        QString pad;
        for (uint i = 0; i < padding; i++)
            pad += " ";
        return
            QString("%0Service %1\n").arg(pad).arg(m_sServiceType) +
            QString("%0  id:            %1\n").arg(pad).arg(m_sServiceId) +
            QString("%0  SCPD URL:      %1\n").arg(pad).arg(m_sSCPDURL) +
            QString("%0  Control URL:   %1\n").arg(pad).arg(m_sControlURL) +
            QString("%0  Event Sub URL: %1").arg(pad).arg(m_sEventSubURL);
    }
};

/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC UPnpDevice
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
        mutable QString m_sUDN;

        NameValues      m_lstExtra;

        /// MythTV specific information
        bool            m_securityPin;
        QString         m_protocolVersion;

        UPnpIconList    m_listIcons;
        UPnpServiceList m_listServices;
        UPnpDeviceList  m_listDevices;

    public:
        UPnpDevice();
        ~UPnpDevice();

        QString GetUDN(void) const;

        void toMap(InfoMap &map);

        UPnpService GetService(const QString &urn, bool *found = NULL) const;

        QString toString(uint padding = 0) const;
};


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
//
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC UPnpDeviceDesc
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
        void     SetBoolValue( const QDomNode &n, bool    &nValue );

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
        static UPnpDeviceDesc *Retrieve  ( QString &sURL );

        void toMap(InfoMap &map)
        {
            map["hostname"] = m_sHostName;
            m_rootDevice.toMap(map);
        }
};

/////////////////////////////////////////////////////////////////////////////
// DeviceLocation Class Definition/Implementation
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC DeviceLocation : public ReferenceCounter
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
                        TaskTime       ttExpires ) : ReferenceCounter(
                                                         "DeviceLocation"     ),
                                                     m_pDeviceDesc( NULL      ),
                                                     m_sURI       ( sURI      ),
                                                     m_sUSN       ( sUSN      ),
                                                     m_sLocation  ( sLocation ),
                                                     m_ttExpires  ( ttExpires )
        {
            // Should be atomic increment
            g_nAllocated++;
        }

        // ==================================================================

        int ExpiresInSecs(void) const
        {
            TaskTime ttNow;
            gettimeofday( (&ttNow), NULL );

            return m_ttExpires.tv_sec - ttNow.tv_sec;
        }

        // ==================================================================

        UPnpDeviceDesc *GetDeviceDesc(void)
        {
            if (m_pDeviceDesc == NULL)
                m_pDeviceDesc = UPnpDeviceDesc::Retrieve( m_sLocation );

            return m_pDeviceDesc;
        }

        // ==================================================================

        QString GetFriendlyName(void)
        {
            UPnpDeviceDesc *pDevice = GetDeviceDesc();

            if ( pDevice == NULL)
               return "<Unknown>";

            QString sName = pDevice->m_rootDevice.m_sFriendlyName;

            if (sName == "mythtv: MythTV AV Media Server")
                return sName + " (" + pDevice->m_sHostName + ")";

            return sName;
        }

        QString GetNameAndDetails(void)
        {
            UPnpDeviceDesc *pDevice = GetDeviceDesc();

            if ( pDevice == NULL)
               return "<Unknown> (" + m_sLocation + ")";

            return pDevice->m_rootDevice.m_sFriendlyName
                   + " (" + pDevice->m_sHostName + "), "
                   + pDevice->m_rootDevice.m_sUDN;
        }

        void GetDeviceDetail(InfoMap &map)
        {
            map["location"] = m_sLocation;

            UPnpDeviceDesc *pDevice = GetDeviceDesc();
            if (!pDevice)
                return;

            pDevice->toMap(map);
        }

        bool NeedSecurityPin(void)
        {
            UPnpDeviceDesc *pDevice = GetDeviceDesc();
            if (!pDevice)
                return false;

            return pDevice->m_rootDevice.m_securityPin;
        }

        QString toString() const
        {
            return QString("\nURI:%1\nUSN:%2\nDeviceXML:%3\n"
                           "Expires:%4\nMythTV PIN:%5")
                .arg(m_sURI).arg(m_sUSN).arg(m_sLocation)
                .arg(ExpiresInSecs()).arg(m_sSecurityPin);
        }
};

#endif
