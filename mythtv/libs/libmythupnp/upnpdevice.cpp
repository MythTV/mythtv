//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpdevice.cpp
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

#include "upnp.h"
#include "upnpdevice.h"
#include "httpcomms.h"
#include "mythverbose.h"

// MythDB
#include "mythdb.h"

#include <cerrno>

#include <QFile>
#include <QTextStream>

int DeviceLocation::g_nAllocated   = 0;       // Debugging only

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// UPnpDeviceDesc Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpDeviceDesc::UPnpDeviceDesc()
{
    VERBOSE( VB_UPNP, "UPnpDeviceDesc - Constructor" );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpDeviceDesc::~UPnpDeviceDesc()
{
    // FIXME: Using this causes crashes
    //VERBOSE( VB_UPNP, "UPnpDeviceDesc - Destructor" );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpDeviceDesc::Load( const QString &sFileName )
{
    // ----------------------------------------------------------------------
    // Open Supplied XML uPnp Description file.
    // ----------------------------------------------------------------------

    QDomDocument doc ( "upnp" );
    QFile        file( sFileName );

    if ( !file.open( QIODevice::ReadOnly ) )
        return false;

    QString sErrMsg;
    int     nErrLine = 0;
    int     nErrCol  = 0;
    bool    bSuccess = doc.setContent( &file, false,
                                       &sErrMsg, &nErrLine, &nErrCol );

    file.close();

    if (!bSuccess)
    {
        VERBOSE(VB_IMPORTANT, QString("UPnpDeviceDesc::Load - "
                                      "Error parsing: %1 "
                                      "at line: %2  column: %3")
                                .arg( sFileName )
                                .arg( nErrLine )
                                .arg( nErrCol  ));

        VERBOSE(VB_IMPORTANT, QString("UPnpDeviceDesc::Load - Error Msg: %1" )
                                .arg( sErrMsg ));
        return false;
    }

    // --------------------------------------------------------------
    // XML Document Loaded... now parse it into the UPnpDevice Hierarchy
    // --------------------------------------------------------------

    return Load( doc );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool UPnpDeviceDesc::Load( const QDomDocument &xmlDevDesc )
{
    // --------------------------------------------------------------
    // Parse XML into the UPnpDevice Hierarchy
    // --------------------------------------------------------------

    QDomNode  oNode = xmlDevDesc.documentElement();

    _InternalLoad( oNode.namedItem( "device" ), &m_rootDevice );

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpDeviceDesc::_InternalLoad( QDomNode oNode, UPnpDevice *pCurDevice )
{
    QString pin = GetMythDB()->GetSetting( "SecurityPin", "");
    pCurDevice->m_securityPin = (pin.isEmpty() || pin == "0000") ?
                                false : true;

    for ( oNode = oNode.firstChild();
                 !oNode.isNull();
                  oNode = oNode.nextSibling() )
    {
        QDomElement e = oNode.toElement();

        if (!e.isNull())
        {
            if ( e.tagName() == "deviceType"       )
            { SetStrValue( e, pCurDevice->m_sDeviceType      ); continue; }
            if ( e.tagName() == "friendlyName"     )
            { SetStrValue( e, pCurDevice->m_sFriendlyName    ); continue; }
            if ( e.tagName() == "manufacturer"     )
            { SetStrValue( e, pCurDevice->m_sManufacturer    ); continue; }
            if ( e.tagName() == "manufacturerURL"  )
            { SetStrValue( e, pCurDevice->m_sManufacturerURL ); continue; }
            if ( e.tagName() == "modelDescription" )
            { SetStrValue( e, pCurDevice->m_sModelDescription); continue; }
            if ( e.tagName() == "modelName"        )
            { SetStrValue( e, pCurDevice->m_sModelName       ); continue; }
            if ( e.tagName() == "modelNumber"      )
            { SetStrValue( e, pCurDevice->m_sModelNumber     ); continue; }
            if ( e.tagName() == "modelURL"         )
            { SetStrValue( e, pCurDevice->m_sModelURL        ); continue; }
            if ( e.tagName() == "serialNumber"     )
            { SetStrValue( e, pCurDevice->m_sSerialNumber    ); continue; }
            if ( e.tagName() == "UPC"              )
            { SetStrValue( e, pCurDevice->m_sUPC             ); continue; }
            if ( e.tagName() == "presentationURL"  )
            { SetStrValue( e, pCurDevice->m_sPresentationURL ); continue; }
            if ( e.tagName() == "UDN"              )
            { SetStrValue( e, pCurDevice->m_sUDN             ); continue; }

            if ( e.tagName() == "iconList"         )
            { ProcessIconList   ( oNode, pCurDevice ); continue; }
            if ( e.tagName() == "serviceList"      )
            { ProcessServiceList( oNode, pCurDevice ); continue; }
            if ( e.tagName() == "deviceList"       )
            { ProcessDeviceList ( oNode, pCurDevice ); continue; }

            if ( e.tagName() == "mythtv:X_secure"  )
            { SetBoolValue( e, pCurDevice->m_securityPin ); continue; }
            if ( e.tagName() == "mythtv:X_protocol"  )
            { SetStrValue( e, pCurDevice->m_protocolVersion ); continue; }

            // Not one of the expected element names... add to extra list.

            QString sValue = "";

            SetStrValue( e, sValue );

            pCurDevice->m_lstExtra.push_back(NameValue(e.tagName(), sValue));
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpDeviceDesc::ProcessIconList( QDomNode oListNode, UPnpDevice *pDevice )
{
    for ( QDomNode oNode = oListNode.firstChild();
                  !oNode.isNull();
                   oNode = oNode.nextSibling() )
    {
        QDomElement e = oNode.toElement();

        if (!e.isNull())
        {
            if ( e.tagName() == "icon" )
            {
                UPnpIcon *pIcon = new UPnpIcon();
                pDevice->m_listIcons.append( pIcon );

                SetStrValue( e.namedItem( "mimetype" ), pIcon->m_sMimeType );
                SetNumValue( e.namedItem( "width"    ), pIcon->m_nWidth    );
                SetNumValue( e.namedItem( "height"   ), pIcon->m_nHeight   );
                SetNumValue( e.namedItem( "depth"    ), pIcon->m_nDepth    );
                SetStrValue( e.namedItem( "url"      ), pIcon->m_sURL      );
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpDeviceDesc::ProcessServiceList( QDomNode oListNode, UPnpDevice *pDevice )
{
    for ( QDomNode oNode = oListNode.firstChild();
                  !oNode.isNull();
                   oNode = oNode.nextSibling() )
    {
        QDomElement e = oNode.toElement();

        if (!e.isNull())
        {
            if ( e.tagName() == "service" )
            {
                UPnpService *pService = new UPnpService();
                pDevice->m_listServices.append( pService );

                SetStrValue( e.namedItem( "serviceType" ),
                             pService->m_sServiceType     );
                SetStrValue( e.namedItem( "serviceId"   ),
                             pService->m_sServiceId       );
                SetStrValue( e.namedItem( "SCPDURL"     ),
                             pService->m_sSCPDURL         );
                SetStrValue( e.namedItem( "controlURL"  ),
                             pService->m_sControlURL      );
                SetStrValue( e.namedItem( "eventSubURL" ),
                             pService->m_sEventSubURL     );

                VERBOSE(VB_UPNP,
                        QString("ProcessServiceList adding service : %1 : %2 :")
                        .arg(pService->m_sServiceType)
                        .arg(pService->m_sServiceId ));
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpDeviceDesc::ProcessDeviceList( QDomNode    oListNode,
                                        UPnpDevice *pDevice )
{
    for ( QDomNode oNode = oListNode.firstChild();
                  !oNode.isNull();
                   oNode = oNode.nextSibling() )
    {
        QDomElement e = oNode.toElement();

        if (!e.isNull())
        {
            if ( e.tagName() == "device")
            {
                UPnpDevice *pNewDevice = new UPnpDevice();
                pDevice->m_listDevices.append( pNewDevice );

                _InternalLoad( e, pNewDevice );
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpDeviceDesc::SetStrValue( const QDomNode &n, QString &sValue )
{
    if (!n.isNull())
    {
        QDomText  oText = n.firstChild().toText();

        if (!oText.isNull())
            sValue = oText.nodeValue();
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpDeviceDesc::SetNumValue( const QDomNode &n, int &nValue )
{
    if (!n.isNull())
    {
        QDomText  oText = n.firstChild().toText();

        if (!oText.isNull())
            nValue = oText.nodeValue().toInt();
    }
}

void UPnpDeviceDesc::SetBoolValue( const QDomNode &n, bool &nValue )
{
    if (!n.isNull())
    {
        QDomText  oText = n.firstChild().toText();

        if (!oText.isNull())
        {
            QString s = oText.nodeValue();
            nValue = (s == "yes" || s == "true" || s.toInt());
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString  UPnpDeviceDesc::GetValidXML( const QString &sBaseAddress, int nPort )
{
    QString     sXML;
    QTextStream os( &sXML, QIODevice::WriteOnly );

    GetValidXML( sBaseAddress, nPort, os );
    os << flush;
    return( sXML );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpDeviceDesc::GetValidXML(
    const QString &sBaseAddress, int nPort,
    QTextStream &os, const QString &sUserAgent )
{
//    os.setEncoding( QTextStream::UnicodeUTF8 );

    os << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
          "<root xmlns=\"urn:schemas-upnp-org:device-1-0\"  xmlns:mythtv=\"mythtv.org\">\n"
            "<specVersion>\n"
              "<major>1</major>\n"
              "<minor>0</minor>\n"
            "</specVersion>\n"
            "<URLBase>http://"
       << sBaseAddress << ":" << nPort << "/</URLBase>\n";

    OutputDevice( os, &m_rootDevice, sUserAgent );

    os << "</root>\n";
    os << flush;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpDeviceDesc::OutputDevice( QTextStream &os,
                                   UPnpDevice *pDevice,
                                   const QString &sUserAgent )
{
    if (pDevice == NULL)
        return;

    QString sFriendlyName = QString( "%1: %2" )
                               .arg( GetHostName() )
                               .arg( pDevice->m_sFriendlyName );

    // ----------------------------------------------------------------------
    // Only override the root device
    // ----------------------------------------------------------------------

    if (pDevice == &m_rootDevice)
        sFriendlyName = UPnp::GetConfiguration()->GetValue( "UPnP/FriendlyName",
                                                            sFriendlyName  );

    os << "<device>\n";
    os << FormatValue( "deviceType"   , pDevice->m_sDeviceType );
    os << FormatValue( "friendlyName" , sFriendlyName          );

    // ----------------------------------------------------------------------
    // XBox 360 needs specific values in the Device Description.
    //
    // -=>TODO: This should be externalized in a more generic/extension
    //          kind of way.
    // ----------------------------------------------------------------------

    bool bIsXbox360 =
        sUserAgent.startsWith(QString("Xbox/2.0"), Qt::CaseInsensitive) ||
        sUserAgent.startsWith( QString("Mozilla/4.0"), Qt::CaseInsensitive);

    os << FormatValue( "manufacturer" , pDevice->m_sManufacturer    );
    os << FormatValue( "modelURL"     , pDevice->m_sModelURL        );

    if ( bIsXbox360 )
    {
        os << FormatValue( "modelName"    ,
                           "Windows Media Connect Compatible (MythTV)");
    }
    else
    {
        os << FormatValue( "modelName"    , pDevice->m_sModelName       );
    }

    os << FormatValue( "manufacturerURL"  , pDevice->m_sManufacturerURL );
    os << FormatValue( "modelDescription" , pDevice->m_sModelDescription);
    os << FormatValue( "modelNumber"      , pDevice->m_sModelNumber     );
    os << FormatValue( "serialNumber"     , pDevice->m_sSerialNumber    );
    os << FormatValue( "UPC"              , pDevice->m_sUPC             );
    os << FormatValue( "presentationURL"  , pDevice->m_sPresentationURL );

    // MythTV Custom information
    os << FormatValue( "mythtv:X_secure" , pDevice->m_securityPin ? "true" : "false");
    os << FormatValue( "mythtv:X_protocol", pDevice->m_protocolVersion );

    NameValues::const_iterator nit = pDevice->m_lstExtra.begin();
    for (; nit != pDevice->m_lstExtra.end(); ++nit)
    {
        // -=>TODO: Hack to handle one element with attributes... need to
        //          handle attributes in a more generic way.

        if ((*nit).sName == "dlna:X_DLNADOC")
        {
            os << QString("<dlna:X_DLNADOC xmlns:dlna=\"urn:"
                          "schemas-dlna-org:device-1-0\">%1"
                          "</dlna:X_DLNADOC>\n" ).arg((*nit).sValue);
        }
        else
            os << FormatValue( (*nit).sName, (*nit).sValue );
    }

    // ----------------------------------------------------------------------
    // Output Any Icons.
    // ----------------------------------------------------------------------

    if (pDevice->m_listIcons.count() > 0)
    {
        os << "<iconList>\n";

        UPnpIconList::const_iterator it = pDevice->m_listIcons.begin();
        for (; it != pDevice->m_listIcons.end(); ++it)
        {

            os << "<icon>\n";
            os << FormatValue( "mimetype", (*it)->m_sMimeType );
            os << FormatValue( "width"   , (*it)->m_nWidth    );
            os << FormatValue( "height"  , (*it)->m_nHeight   );
            os << FormatValue( "depth"   , (*it)->m_nDepth    );
            os << FormatValue( "url"     , (*it)->m_sURL      );
            os << "</icon>\n";
        }
        os << "</iconList>\n";
    }

    os << FormatValue( "UDN"          , pDevice->GetUDN()      );

    // ----------------------------------------------------------------------
    // Output any Services
    // ----------------------------------------------------------------------

    if (pDevice->m_listServices.count() > 0)
    {
        // ------------------------------------------------------------------
        // -=>TODO: As a temporary fix don't expose the MSRR service unless we
        //          as an XBox360 or Windows MediaPlayer.
        //
        //          There is a problem with a DSM-520 with firmware 1.04 and
        //          the Denon AVR-4306 receiver.
        //
        //          If the MSRR Service is exposed, it won't let us browse
        //          any media content.
        //
        //          Need to find out real fix and remove this code.
        // ------------------------------------------------------------------

        //bool bDSM = sUserAgent.startsWith( "INTEL_NMPR/2.1 DLNADOC/1.00", false );

        os << "<serviceList>\n";

        UPnpServiceList::const_iterator it = pDevice->m_listServices.begin();
        for (; it != pDevice->m_listServices.end(); ++it)
        {
            if (!bIsXbox360 && (*it)->m_sServiceType.startsWith(
                    "urn:microsoft.com:service:X_MS_MediaReceiverRegistrar",
                    Qt::CaseInsensitive))
            {
                continue;
            }

            os << "<service>\n";
            os << FormatValue( "serviceType", (*it)->m_sServiceType );
            os << FormatValue( "serviceId"  , (*it)->m_sServiceId   );
            os << FormatValue( "SCPDURL"    , (*it)->m_sSCPDURL     );
            os << FormatValue( "controlURL" , (*it)->m_sControlURL  );
            os << FormatValue( "eventSubURL", (*it)->m_sEventSubURL );
            os << "</service>\n";
        }
        os << "</serviceList>\n";
    }

    // ----------------------------------------------------------------------
    // Output any Embedded Devices
    // ----------------------------------------------------------------------

    // -=>Note:  XBMC can't handle sub-devices, it's UserAgent is blank.
#if 0
    if (sUserAgent.length() > 0)
    {
        if (pDevice->m_listDevices.count() > 0)
        {
            os << "<deviceList>";

            for ( UPnpDevice *pEmbeddedDevice  = pDevice->m_listDevices.first();
                              pEmbeddedDevice != NULL;
                              pEmbeddedDevice  = pDevice->m_listDevices.next() )
            {
                OutputDevice( os, pEmbeddedDevice );
            }
            os << "</deviceList>";
        }
    }
#endif
    os << "</device>\n";
    os << flush;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpDeviceDesc::FormatValue( const QString &sName, const QString &sValue )
{
    QString sStr;

    if (sValue.length() > 0)
        sStr = QString( "<%1>%2</%3>\n" ).arg( sName ).arg(sValue).arg( sName );

    return( sStr );
}

/////////////////////////////////////////////////////////////////////////////

QString UPnpDeviceDesc::FormatValue( const QString &sName, int nValue )
{
    return( QString( "<%1>%2</%1>\n" ).arg( sName ).arg(nValue) );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpDeviceDesc::FindDeviceUDN( UPnpDevice *pDevice, QString sST )
{
    if (sST == pDevice->m_sDeviceType)
        return pDevice->GetUDN();

    if (sST == pDevice->GetUDN())
        return sST;

    // ----------------------------------------------------------------------
    // Check for matching Service
    // ----------------------------------------------------------------------

    UPnpServiceList::const_iterator sit = pDevice->m_listServices.begin();
    for (; sit != pDevice->m_listServices.end(); ++sit)
    {
        if (sST == (*sit)->m_sServiceType)
            return pDevice->GetUDN();
    }

    // ----------------------------------------------------------------------
    // Check Embedded Devices for a Match
    // ----------------------------------------------------------------------

    UPnpDeviceList::const_iterator dit = pDevice->m_listDevices.begin();
    for (; dit != pDevice->m_listDevices.end(); ++dit)
    {
        QString sUDN = FindDeviceUDN( *dit, sST );
        if (sUDN.length() > 0)
            return sUDN;
    }

    return "";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpDevice *UPnpDeviceDesc::FindDevice( const QString &sURI )
{
    return FindDevice( &m_rootDevice, sURI );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpDevice *UPnpDeviceDesc::FindDevice( UPnpDevice    *pDevice,
                                        const QString &sURI )

{
    if ( sURI == pDevice->m_sDeviceType )
        return pDevice;

    // ----------------------------------------------------------------------
    // Check Embedded Devices for a Match
    // ----------------------------------------------------------------------

    UPnpDeviceList::iterator dit = pDevice->m_listDevices.begin();
    for (; dit != pDevice->m_listDevices.end(); ++dit)
    {
        UPnpDevice *pFound = FindDevice(*dit, sURI);

        if (pFound != NULL)
            return pFound;
    }

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpDeviceDesc *UPnpDeviceDesc::Retrieve( QString &sURL, bool bInQtThread )
{
    UPnpDeviceDesc *pDevice = NULL;

    VERBOSE( VB_UPNP, QString( "UPnpDeviceDesc::Retrieve( %1, %2 )" )
                      .arg( sURL ).arg( bInQtThread ));

    QString sXml = HttpComms::getHttp( sURL,
                                       10000, // ms
                                       3,     // retries
                                       0,     // redirects
                                       false, // allow gzip
                                       NULL,  // login
                                       bInQtThread );

    if (sXml.startsWith( QString("<?xml") ))
    {
        QString sErrorMsg;

        QDomDocument xml( "upnp" );

        if ( xml.setContent( sXml, false, &sErrorMsg ))
        {
            pDevice = new UPnpDeviceDesc();

            pDevice->Load( xml );

            pDevice->m_HostUrl   = sURL;
            pDevice->m_sHostName = pDevice->m_HostUrl.host();
        }
        else
        {
            VERBOSE( VB_UPNP,
                     QString( "... Error parsing device description xml [%1]" )
                     .arg( sErrorMsg ));
        }
    }
    else
    {
        VERBOSE( VB_UPNP, QString( "... Invalid response '%1'" ).arg( sXml ));
    }

    return pDevice;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpDeviceDesc::GetHostName()
{
    if (m_sHostName.length() == 0)
    {
        // Get HostName

        char localHostName[1024];

        if (gethostname(localHostName, 1024))
            VERBOSE(VB_IMPORTANT,
                    "UPnpDeviceDesc: Error, could not determine host name."
                    + ENO);

        return UPnp::GetConfiguration()->GetValue( "Settings/HostName",
                                          QString( localHostName ));
    }

    return m_sHostName;
}
