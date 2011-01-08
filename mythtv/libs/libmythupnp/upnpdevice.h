//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpdevice.h
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Device Description parser/generator
//
// Copyright (c) 2005 David Blain <mythtv@theblains.net>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef __UPNPDEVICE_H__
#define __UPNPDEVICE_H__

#include <QDomDocument>
#include <QUrl>
#include <QHash>

#include "compat.h"
#include "upnpexp.h"
#include "upnputil.h"
#include "refcounted.h"
#include "mythversion.h"  // for MYTH_BINARY_VERSION

extern const char *myth_source_version;

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

        QString     m_sMimeType;
        int         m_nWidth;
        int         m_nHeight;
        int         m_nDepth;
        QString     m_sURL;

        UPnpIcon() : m_nWidth ( 0 ), m_nHeight( 0 ), m_nDepth( 0 )  {}
};

/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC UPnpService
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
        QString         m_sUDN;

        NameValues      m_lstExtra;

        /// MythTV specific information
        bool            m_securityPin;
        QString         m_protocolVersion;

        UPnpIconList    m_listIcons;
        UPnpServiceList m_listServices;
        UPnpDeviceList  m_listDevices;

    public:

        UPnpDevice()
        {
            m_sModelNumber  = MYTH_BINARY_VERSION;
            m_sSerialNumber = myth_source_version;
            m_securityPin   = false;
            m_protocolVersion = MYTH_PROTO_VERSION;
        }
        ~UPnpDevice()
        {
            while (!m_listIcons.empty())
            {
                delete m_listIcons.back();
                m_listIcons.pop_back();
            }
            while (!m_listServices.empty())
            {
                delete m_listServices.back();
                m_listServices.pop_back();
            }
            while (!m_listDevices.empty())
            {
                delete m_listDevices.back();
                m_listDevices.pop_back();
            }
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
        static UPnpDeviceDesc *Retrieve  ( QString &sURL, bool bInQtThread = true   );

};

/////////////////////////////////////////////////////////////////////////////
// DeviceLocation Class Definition/Implementation
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC DeviceLocation : public RefCounted
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
            gettimeofday( (&ttNow), NULL );

            return m_ttExpires.tv_sec - ttNow.tv_sec;
        }

        // ==================================================================

        UPnpDeviceDesc *GetDeviceDesc( bool bInQtThread = true )
        {
            if (m_pDeviceDesc == NULL)
                m_pDeviceDesc = UPnpDeviceDesc::Retrieve( m_sLocation, bInQtThread );

            return m_pDeviceDesc;
        }

        // ==================================================================

        QString GetFriendlyName( bool bInQtThread = true )
        {
            UPnpDeviceDesc *pDevice = GetDeviceDesc( bInQtThread );

            if ( pDevice == NULL)
               return "<Unknown>";

            QString sName = pDevice->m_rootDevice.m_sFriendlyName;

            if (sName == "mythtv: MythTV AV Media Server")
                return sName + " (" + pDevice->m_sHostName + ")";

            return sName;
        }

        QString GetNameAndDetails( bool bInQtThread = true )
        {
            UPnpDeviceDesc *pDevice = GetDeviceDesc( bInQtThread );

            if ( pDevice == NULL)
               return "<Unknown> (" + m_sLocation + ")";

            return pDevice->m_rootDevice.m_sFriendlyName
                   + " (" + pDevice->m_sHostName + "), "
                   + pDevice->m_rootDevice.m_sUDN;
        }

        void GetDeviceDetail(QHash<QString, QString> &map,
                             bool bInQtThread = true)
        {
            map["location"] = m_sLocation;

            UPnpDeviceDesc *pDevice = GetDeviceDesc(bInQtThread);
            if (!pDevice)
                return;

            map["hostname"] = pDevice->m_sHostName;
            map["name"] = pDevice->m_rootDevice.m_sFriendlyName;
            map["modelname"] = pDevice->m_rootDevice.m_sModelName;
            map["modelnumber"] = pDevice->m_rootDevice.m_sModelNumber;
            map["modelurl"] = pDevice->m_rootDevice.m_sModelURL;
            map["modeldescription"] = pDevice->m_rootDevice.m_sModelDescription;
            map["manufacturer"] = pDevice->m_rootDevice.m_sManufacturer;
            map["manufacturerurl"] = pDevice->m_rootDevice.m_sManufacturerURL;
            map["devicetype"] = pDevice->m_rootDevice.m_sDeviceType;
            map["serialnumber"] = pDevice->m_rootDevice.m_sSerialNumber;
            map["UDN"] = pDevice->m_rootDevice.m_sUDN;
            map["UPC"] = pDevice->m_rootDevice.m_sUPC;
        }

        bool NeedSecurityPin( bool bInQtThread = true )
        {
            UPnpDeviceDesc *pDevice = GetDeviceDesc(bInQtThread);
            if (!pDevice)
                return false;

            return pDevice->m_rootDevice.m_securityPin;
        }
};

#endif
