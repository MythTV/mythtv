//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpdevice.h
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Device Description parser/generator
//
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef UPNPDEVICE_H
#define UPNPDEVICE_H

#include <utility>

// Qt headers
#include <QDomDocument>
#include <QList>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

// MythTV headers
#include "libmythbase/compat.h"
#include "libmythbase/mythchrono.h"
#include "libmythbase/mythtypes.h"
#include "libmythbase/referencecounter.h"

#include "upnpexp.h"
#include "upnputil.h"

class UPnpDeviceDesc;
class UPnpDevice;
class UPnpService;
class UPnpIcon;
class QTextStream;

/////////////////////////////////////////////////////////////////////////////
// Typedefs
/////////////////////////////////////////////////////////////////////////////

using UPnpDeviceList  = QList< UPnpDevice*  >;
using UPnpServiceList = QList< UPnpService* >;
using UPnpIconList    = QList< UPnpIcon*    >;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC UPnpIcon
{
  public:
    QString     m_sURL;
    QString     m_sMimeType;
    int         m_nWidth    { 0 };
    int         m_nHeight   { 0 };
    int         m_nDepth    { 0 };

    UPnpIcon() = default;

    QString toString(uint padding) const
    {
        QString pad;
        for (uint i = 0; i < padding; i++)
            pad += " ";
        return QString("%0Icon %1 %2x%3^%4 %5")
            .arg(pad, m_sURL).arg(m_nWidth).arg(m_nHeight)
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

    UPnpService() = default;

    QString toString(uint padding) const
    {
        QString pad;
        for (uint i = 0; i < padding; i++)
            pad += " ";
        return
            QString("%0Service %1\n"
                    "%0  id:            %2\n"
                    "%0  SCPD URL:      %3\n"
                    "%0  Control URL:   %4\n"
                    "%0  Event Sub URL: %5")
            .arg(pad, m_sEventSubURL, m_sServiceType, m_sServiceId,
                 m_sSCPDURL, m_sControlURL, m_sEventSubURL);
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
        bool            m_securityPin     { false };
        QString         m_protocolVersion;

        UPnpIconList    m_listIcons;
        UPnpServiceList m_listServices;
        UPnpDeviceList  m_listDevices;

    public:
        UPnpDevice();
        ~UPnpDevice();

        QString GetUDN(void) const;

        void toMap(InfoMap &map) const;

        UPnpService GetService(const QString &urn, bool *found = nullptr) const;

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
        QUrl            m_hostUrl;

    protected:

        void    InternalLoad( QDomNode  oNode, UPnpDevice *pCurDevice );

        static void     ProcessIconList   ( const QDomNode& oListNode, UPnpDevice *pDevice );
        static void     ProcessServiceList( const QDomNode& oListNode, UPnpDevice *pDevice );
        void     ProcessDeviceList ( const QDomNode& oListNode, UPnpDevice *pDevice );

        void     OutputDevice( QTextStream &os,
                               UPnpDevice *pDevice,
                               const QString &sUserAgent = "" );

        static void     SetStrValue ( const QDomNode &n, QString &sValue );
        static void     SetNumValue ( const QDomNode &n, int     &nValue );
        static void     SetBoolValue( const QDomNode &n, bool    &nValue );

        static QString  FormatValue ( const NameValue &node );
        static QString  FormatValue ( const QString &sName, const QString &sValue );
        static QString  FormatValue ( const QString &sName, int nValue );

        QString  GetHostName () const;

    public:

                 UPnpDeviceDesc() = default;
        virtual ~UPnpDeviceDesc() = default;

        bool     Load       ( const QString &sFileName );
        bool     Load       ( const QDomDocument &xmlDevDesc );

        void     GetValidXML( const QString &sBaseAddress, int nPort, QTextStream &os, const QString &sUserAgent = "" );
        QString  GetValidXML( const QString &sBaseAddress, int nPort );

        QString  FindDeviceUDN( UPnpDevice *pDevice, QString sST );

        UPnpDevice *FindDevice( const QString &sURI );

        static UPnpDevice     *FindDevice( UPnpDevice *pDevice, const QString &sURI );
        static UPnpDeviceDesc *Retrieve  ( QString &sURL );

        void toMap(InfoMap &map) const
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

        ~DeviceLocation() override
        {
            // Should be atomic decrement
            g_nAllocated--;

            delete m_pDeviceDesc;
        }

        UPnpDeviceDesc *m_pDeviceDesc { nullptr };  // We take ownership of this pointer.

    public:

        QString     m_sURI;           // Service Type URI
        QString     m_sUSN;           // Unique Service Name
        QString     m_sLocation;      // URL to Device Description
        std::chrono::microseconds    m_ttExpires;
        QString     m_sSecurityPin;   // Use for MythXML methods needed pin

    public:

        // ==================================================================

        DeviceLocation( QString sURI,
                        QString sUSN,
                        QString sLocation,
                        std::chrono::microseconds ttExpires ) : ReferenceCounter(
                                                         "DeviceLocation"     ),
                                                     m_sURI       (std::move( sURI      )),
                                                     m_sUSN       (std::move( sUSN      )),
                                                     m_sLocation  (std::move( sLocation )),
                                                     m_ttExpires  ( ttExpires )
        {
            // Should be atomic increment
            g_nAllocated++;
        }

        // ==================================================================

        std::chrono::seconds ExpiresInSecs(void) const
        {
            auto ttNow = nowAsDuration<std::chrono::microseconds>();
            return duration_cast<std::chrono::seconds>(m_ttExpires - ttNow);
        }

        // ==================================================================

        UPnpDeviceDesc *GetDeviceDesc(void)
        {
            if (m_pDeviceDesc == nullptr)
                m_pDeviceDesc = UPnpDeviceDesc::Retrieve( m_sLocation );

            return m_pDeviceDesc;
        }

        // ==================================================================

        QString GetFriendlyName(void)
        {
            UPnpDeviceDesc *pDevice = GetDeviceDesc();

            if ( pDevice == nullptr)
               return "<Unknown>";

            QString sName = pDevice->m_rootDevice.m_sFriendlyName;

            if (sName == "mythtv: MythTV AV Media Server")
                return sName + " (" + pDevice->m_sHostName + ")";

            return sName;
        }

        QString GetNameAndDetails(void)
        {
            UPnpDeviceDesc *pDevice = GetDeviceDesc();

            if ( pDevice == nullptr)
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
                .arg(m_sURI, m_sUSN, m_sLocation,
                     QString::number(ExpiresInSecs().count()),
                     m_sSecurityPin);
        }
};

#endif // UPNPDEVICE_H
