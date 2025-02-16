//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpdevice.cpp
// Created     : Oct. 24, 2005
//
// Purpose     : UPnp Device Description parser/generator
//
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "upnpdevice.h"

#include <unistd.h> // for gethostname

#include <QFile>
#include <QTextStream>
#include <QHostAddress>
#include <QHostInfo>

// MythDB
#include "libmythbase/configuration.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdownloadmanager.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythversion.h"  // for MYTH_BINARY_VERSION

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

bool UPnpDeviceDesc::Load( const QString &sFileName )
{
    // ----------------------------------------------------------------------
    // Open Supplied XML uPnp Description file.
    // ----------------------------------------------------------------------

    QDomDocument doc ( "upnp" );
    QFile        file( sFileName );

    if ( !file.open( QIODevice::ReadOnly ) )
        return false;

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    QString sErrMsg;
    int     nErrLine = 0;
    int     nErrCol  = 0;
    bool    bSuccess = doc.setContent( &file, false,
                                       &sErrMsg, &nErrLine, &nErrCol );

    file.close();

    if (!bSuccess)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("UPnpDeviceDesc::Load - Error parsing: %1 "
                    "at line: %2  column: %3")
                .arg(sFileName) .arg(nErrLine)
                                .arg(nErrCol));
        LOG(VB_GENERAL, LOG_ERR,
            QString("UPnpDeviceDesc::Load - Error Msg: %1" ) .arg(sErrMsg));
        return false;
    }
#else
    auto parseResult = doc.setContent( &file );

    file.close();

    if (!parseResult)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("UPnpDeviceDesc::Load - Error parsing: %1 "
                    "at line: %2  column: %3")
                .arg(sFileName) .arg(parseResult.errorLine)
                                .arg(parseResult.errorColumn));
        LOG(VB_GENERAL, LOG_ERR,
            QString("UPnpDeviceDesc::Load - Error Msg: %1" ) .arg(parseResult.errorMessage));
        return false;
    }
#endif

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

    InternalLoad( oNode.namedItem( "device" ), &m_rootDevice );

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpDeviceDesc::InternalLoad( QDomNode oNode, UPnpDevice *pCurDevice )
{
    QString pin = GetMythDB()->GetSetting( "SecurityPin", "");
    pCurDevice->m_securityPin = !(pin.isEmpty() || pin == "0000");

    for ( oNode = oNode.firstChild();
          !oNode.isNull();
          oNode = oNode.nextSibling() )
    {
        QDomElement e = oNode.toElement();

        if (e.isNull())
            continue;

        // TODO: make this table driven (using offset within structure)
        if ( e.tagName() == "deviceType" )
            SetStrValue( e, pCurDevice->m_sDeviceType);
        else if ( e.tagName() == "friendlyName" )
            SetStrValue( e, pCurDevice->m_sFriendlyName );
        else if ( e.tagName() == "manufacturer" )
            SetStrValue( e, pCurDevice->m_sManufacturer );
        else if ( e.tagName() == "manufacturerURL" )
            SetStrValue( e, pCurDevice->m_sManufacturerURL );
        else if ( e.tagName() == "modelDescription" )
            SetStrValue( e, pCurDevice->m_sModelDescription);
        else if ( e.tagName() == "modelName" )
            SetStrValue( e, pCurDevice->m_sModelName );
        else if ( e.tagName() == "modelNumber" )
            SetStrValue( e, pCurDevice->m_sModelNumber );
        else if ( e.tagName() == "modelURL" )
            SetStrValue( e, pCurDevice->m_sModelURL );
        else if ( e.tagName() == "serialNumber" )
            SetStrValue( e, pCurDevice->m_sSerialNumber );
        else if ( e.tagName() == "UPC" )
            SetStrValue( e, pCurDevice->m_sUPC );
        else if ( e.tagName() == "presentationURL" )
            SetStrValue( e, pCurDevice->m_sPresentationURL );
        else if ( e.tagName() == "UDN" )
            SetStrValue( e, pCurDevice->m_sUDN );
        else if ( e.tagName() == "iconList" )
            ProcessIconList( oNode, pCurDevice );
        else if ( e.tagName() == "serviceList" )
            ProcessServiceList( oNode, pCurDevice );
        else if ( e.tagName() == "deviceList" )
            ProcessDeviceList ( oNode, pCurDevice );
        else if ( e.tagName() == "mythtv:X_secure" )
            SetBoolValue( e, pCurDevice->m_securityPin );
        else if ( e.tagName() == "mythtv:X_protocol" )
            SetStrValue( e, pCurDevice->m_protocolVersion );
        else
        {
            // Not one of the expected element names... add to extra list.
            QString sValue = "";
            SetStrValue( e, sValue );
            NameValue node = NameValue(e.tagName(), sValue);
            QDomNamedNodeMap attributes =  e.attributes();
            for (int i = 0; i < attributes.size(); i++)
            {
                node.AddAttribute(attributes.item(i).nodeName(),
                                  attributes.item(i).nodeValue(),
                                  true); // TODO Check whether all attributes are in fact requires for the device xml
            }
            pCurDevice->m_lstExtra.push_back(node);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpDeviceDesc::ProcessIconList( const QDomNode& oListNode, UPnpDevice *pDevice )
{
    for ( QDomNode oNode = oListNode.firstChild();
          !oNode.isNull();
          oNode = oNode.nextSibling() )
    {
        QDomElement e = oNode.toElement();

        if (e.isNull())
            continue;

        if ( e.tagName() == "icon" )
        {
            auto *pIcon = new UPnpIcon();
            pDevice->m_listIcons.append( pIcon );

            SetStrValue( e.namedItem( "mimetype" ), pIcon->m_sMimeType );
            SetNumValue( e.namedItem( "width"    ), pIcon->m_nWidth    );
            SetNumValue( e.namedItem( "height"   ), pIcon->m_nHeight   );
            SetNumValue( e.namedItem( "depth"    ), pIcon->m_nDepth    );
            SetStrValue( e.namedItem( "url"      ), pIcon->m_sURL      );
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpDeviceDesc::ProcessServiceList( const QDomNode& oListNode, UPnpDevice *pDevice )
{
    for ( QDomNode oNode = oListNode.firstChild();
          !oNode.isNull();
          oNode = oNode.nextSibling() )
    {
        QDomElement e = oNode.toElement();

        if (e.isNull())
            continue;

        if ( e.tagName() == "service" )
        {
            auto *pService = new UPnpService();
            pDevice->m_listServices.append( pService );

            SetStrValue(e.namedItem( "serviceType" ), pService->m_sServiceType);
            SetStrValue(e.namedItem( "serviceId" ),   pService->m_sServiceId);
            SetStrValue(e.namedItem( "SCPDURL" ),     pService->m_sSCPDURL);
            SetStrValue(e.namedItem( "controlURL" ),  pService->m_sControlURL);
            SetStrValue(e.namedItem( "eventSubURL" ), pService->m_sEventSubURL);

            LOG(VB_UPNP, LOG_INFO,
                QString("ProcessServiceList adding service : %1 : %2 :")
                    .arg(pService->m_sServiceType,
                         pService->m_sServiceId));
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpDeviceDesc::ProcessDeviceList( const QDomNode&    oListNode,
                                        UPnpDevice *pDevice )
{
    for ( QDomNode oNode = oListNode.firstChild();
          !oNode.isNull();
          oNode = oNode.nextSibling() )
    {
        QDomElement e = oNode.toElement();

        if (e.isNull())
            continue;

        if ( e.tagName() == "device")
        {
            auto *pNewDevice = new UPnpDevice();
            pDevice->m_listDevices.append( pNewDevice );
            InternalLoad( e, pNewDevice );
        }
    }
}

/// \brief Sets sValue param to n.firstChild().toText().nodeValue(), iff
///        neither n.isNull() nor n.firstChild().toText().isNull() is true.
void UPnpDeviceDesc::SetStrValue( const QDomNode &n, QString &sValue )
{
    if (!n.isNull())
    {
        QDomText  oText = n.firstChild().toText();

        if (!oText.isNull())
            sValue = oText.nodeValue();
    }
}

/// \brief Sets nValue param to n.firstChild().toText().nodeValue(), iff
///        neither n.isNull() nor n.firstChild().toText().isNull() is true.
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
            nValue = (s == "yes" || s == "true" || (s.toInt() != 0));
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
    os << Qt::flush;
    return( sXML );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpDeviceDesc::GetValidXML(
    const QString &/*sBaseAddress*/, int /*nPort*/,
    QTextStream &os, const QString &sUserAgent )
{
#if 0
    os.setEncoding( QTextStream::UnicodeUTF8 );
#endif
    os << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
          "<root xmlns=\"urn:schemas-upnp-org:device-1-0\" "
          " xmlns:mythtv=\"mythtv.org\">\n"
            "<specVersion>\n"
              "<major>1</major>\n"
              "<minor>0</minor>\n"
            "</specVersion>\n";

    OutputDevice( os, &m_rootDevice, sUserAgent );

    os << "</root>\n";
    os << Qt::flush;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpDeviceDesc::OutputDevice( QTextStream &os,
                                   UPnpDevice *pDevice,
                                   const QString &sUserAgent )
{
    if (pDevice == nullptr)
        return;

    QString sFriendlyName = QString( "%1: %2" )
                               .arg( GetHostName(),
                                     pDevice->m_sFriendlyName );

    // ----------------------------------------------------------------------
    // Only override the root device
    // ----------------------------------------------------------------------

    if (pDevice == &m_rootDevice)
        sFriendlyName = XmlConfiguration().GetValue("UPnP/FriendlyName", sFriendlyName);

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
        sUserAgent.startsWith(QString("Xbox/2.0"),    Qt::CaseInsensitive) ||
        sUserAgent.startsWith(QString("Mozilla/4.0"), Qt::CaseInsensitive);

    os << FormatValue( "manufacturer" , pDevice->m_sManufacturer    );
    os << FormatValue( "modelURL"     , pDevice->m_sModelURL        );

    // HACK
//     if ( bIsXbox360 )
//     {
//         os << FormatValue( "modelName"    ,
//                            "Windows Media Connect Compatible (MythTV)");
//     }
//     else
//     {
        os << FormatValue( "modelName"    , pDevice->m_sModelName       );
//     }

    os << FormatValue( "manufacturerURL"  , pDevice->m_sManufacturerURL );
    os << FormatValue( "modelDescription" , pDevice->m_sModelDescription);
    os << FormatValue( "modelNumber"      , pDevice->m_sModelNumber     );
    os << FormatValue( "serialNumber"     , pDevice->m_sSerialNumber    );
    os << FormatValue( "UPC"              , pDevice->m_sUPC             );
    os << FormatValue( "presentationURL"  , pDevice->m_sPresentationURL );

    // MythTV Custom information
    os << FormatValue( "mythtv:X_secure" ,
                       pDevice->m_securityPin ? "true" : "false");
    os << FormatValue( "mythtv:X_protocol", pDevice->m_protocolVersion );

    for (const auto & nit : std::as_const(pDevice->m_lstExtra))
    {
        os << FormatValue( nit );
    }

    // ----------------------------------------------------------------------
    // Output Any Icons.
    // ----------------------------------------------------------------------

    if (pDevice->m_listIcons.count() > 0)
    {
        os << "<iconList>\n";

        for (auto *icon : std::as_const(pDevice->m_listIcons))
        {
            os << "<icon>\n";
            os << FormatValue( "mimetype", icon->m_sMimeType );
            os << FormatValue( "width"   , icon->m_nWidth    );
            os << FormatValue( "height"  , icon->m_nHeight   );
            os << FormatValue( "depth"   , icon->m_nDepth    );
            os << FormatValue( "url"     , icon->m_sURL      );
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

#if 0
        bool bDSM = sUserAgent.startsWith("INTEL_NMPR/2.1 DLNADOC/1.00", false);
#endif

        os << "<serviceList>\n";

        for (auto *service : std::as_const(pDevice->m_listServices))
        {
            if (!bIsXbox360 && service->m_sServiceType.startsWith(
                    "urn:microsoft.com:service:X_MS_MediaReceiverRegistrar",
                    Qt::CaseInsensitive))
            {
                continue;
            }

            os << "<service>\n";
            os << FormatValue( "serviceType", service->m_sServiceType );
            os << FormatValue( "serviceId"  , service->m_sServiceId   );
            os << FormatValue( "SCPDURL"    , service->m_sSCPDURL     );
            os << FormatValue( "controlURL" , service->m_sControlURL  );
            os << FormatValue( "eventSubURL", service->m_sEventSubURL );
            os << "</service>\n";
        }
        os << "</serviceList>\n";
    }

    // ----------------------------------------------------------------------
    // Output any Embedded Devices
    // ----------------------------------------------------------------------

    if (pDevice->m_listDevices.count() > 0)
    {
        os << "<deviceList>";

        UPnpDeviceList::iterator it;
        for ( it = pDevice->m_listDevices.begin();
              it != pDevice->m_listDevices.end();
              ++it )
        {
            UPnpDevice *pEmbeddedDevice = (*it);
            OutputDevice( os, pEmbeddedDevice );
        }
        os << "</deviceList>";
    }
    os << "</device>\n";
    os << Qt::flush;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpDeviceDesc::FormatValue(const NameValue& node)
{
    QString sStr;

    QString sAttributes;
    NameValues::iterator it;
    for (it = node.m_pAttributes->begin(); it != node.m_pAttributes->end(); ++it)
    {
        sAttributes += QString(" %1='%2'").arg((*it).m_sName, (*it).m_sValue);
    }
    sStr = QString("<%1%2>%3</%1>\n").arg(node.m_sName, sAttributes, node.m_sValue);

    return( sStr );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpDeviceDesc::FormatValue( const QString &sName,
                                     const QString &sValue )
{
    QString sStr;

    if (sValue.length() > 0)
        sStr = QString("<%1>%2</%1>\n") .arg(sName, sValue);

    return( sStr );
}

/////////////////////////////////////////////////////////////////////////////

QString UPnpDeviceDesc::FormatValue( const QString &sName, int nValue )
{
    return( QString("<%1>%2</%1>\n") .arg(sName) .arg(nValue) );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpDeviceDesc::FindDeviceUDN( UPnpDevice *pDevice, QString sST )
{
    // Ignore device version, UPnP is backwards compatible
    if (sST.section(':', 0, -2) == pDevice->m_sDeviceType.section(':', 0, -2))
        return pDevice->GetUDN();

    if (sST.section(':', 0, -2) == pDevice->GetUDN().section(':', 0, -2))
        return sST;

    // ----------------------------------------------------------------------
    // Check for matching Service
    // ----------------------------------------------------------------------

    for (auto sit = pDevice->m_listServices.cbegin();
         sit != pDevice->m_listServices.cend(); ++sit)
    {
        // Ignore the service version, UPnP is backwards compatible
        if (sST.section(':', 0, -2) == (*sit)->m_sServiceType.section(':', 0, -2))
            return pDevice->GetUDN();
    }

    // ----------------------------------------------------------------------
    // Check Embedded Devices for a Match
    // ----------------------------------------------------------------------

    for (auto *device : std::as_const(pDevice->m_listDevices))
    {
        QString sUDN = FindDeviceUDN( device, sST );
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
    // Ignore device version, UPnP is backwards compatible
    if ( sURI.section(':', 0, -2) == pDevice->m_sDeviceType.section(':', 0, -2) )
        return pDevice;

    // ----------------------------------------------------------------------
    // Check Embedded Devices for a Match
    // ----------------------------------------------------------------------

    for (const auto & dev : std::as_const(pDevice->m_listDevices))
    {
        UPnpDevice *pFound = FindDevice(dev, sURI);

        if (pFound != nullptr)
            return pFound;
    }

    return nullptr;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpDeviceDesc *UPnpDeviceDesc::Retrieve( QString &sURL )
{
    UPnpDeviceDesc *pDevice = nullptr;

    LOG(VB_UPNP, LOG_DEBUG, QString("UPnpDeviceDesc::Retrieve( %1 )")
        .arg(sURL));

    QByteArray buffer;

    bool ok = GetMythDownloadManager()->download(sURL, &buffer);

    QString sXml(buffer);

    if (ok && sXml.startsWith( QString("<?xml") ))
    {
        QDomDocument xml( "upnp" );

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
        QString sErrorMsg;
        bool success = xml.setContent( sXml, false, &sErrorMsg );
#else
        auto parseResult = xml.setContent( sXml );
        bool success { parseResult };
        QString sErrorMsg { parseResult.errorMessage };
#endif
        if ( success )
        {
            pDevice = new UPnpDeviceDesc();
            pDevice->Load( xml );
            pDevice->m_hostUrl   = sURL;
            pDevice->m_sHostName = pDevice->m_hostUrl.host();
        }
        else
        {
            LOG(VB_UPNP, LOG_ERR,
                QString("Error parsing device description xml [%1]")
                     .arg(sErrorMsg));
        }
    }
    else
    {
        LOG(VB_UPNP, LOG_ERR, QString("Invalid response '%1'").arg(sXml));
    }

    return pDevice;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpDeviceDesc::GetHostName() const
{
    if (m_sHostName.length() == 0)
    {
        // Get HostName

        QString localHostName = QHostInfo::localHostName();
        if (localHostName.isEmpty())
            LOG(VB_GENERAL, LOG_ERR,
                "UPnpDeviceDesc: Error, could not determine host name." + ENO);

        return XmlConfiguration().GetValue("Settings/HostName", localHostName);
    }

    return m_sHostName;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// UPnpDevice Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

UPnpDevice::UPnpDevice() :
    m_sModelNumber(MYTH_BINARY_VERSION),
    m_sSerialNumber(GetMythSourceVersion()),
    m_protocolVersion(MYTH_PROTO_VERSION)
{

// FIXME: UPnPDevice is created via the static initialisation of UPnPDeviceDesc
//        in upnp.cpp ln 21. This means it's created before the core context is
//        even intialised so we can't use settings, or any 'core' methods to
//        help in populating the Device metadata. That is why the URLs below
//        are relative, not absolute since at this stage we can't determine
//        which IP or port to use!

// NOTE: The icons are defined here and not in devicemaster.xml because they
//       are also used for the MediaRenderer device and other possible future
//       devices too.

    // Large PNG Icon
    auto *pngIconLrg = new UPnpIcon();
    pngIconLrg->m_nDepth = 24;
    pngIconLrg->m_nHeight = 120;
    pngIconLrg->m_nWidth = 120;
    pngIconLrg->m_sMimeType = "image/png";
    pngIconLrg->m_sURL = "/images/icons/upnp_large_icon.png";
    m_listIcons.append(pngIconLrg);

    // Large JPG Icon
    auto *jpgIconLrg = new UPnpIcon();
    jpgIconLrg->m_nDepth = 24;
    jpgIconLrg->m_nHeight = 120;
    jpgIconLrg->m_nWidth = 120;
    jpgIconLrg->m_sMimeType = "image/jpeg";
    jpgIconLrg->m_sURL = "/images/icons/upnp_large_icon.jpg";
    m_listIcons.append(jpgIconLrg);

    // Small PNG Icon
    auto *pngIconSm = new UPnpIcon();
    pngIconSm->m_nDepth = 24;
    pngIconSm->m_nHeight = 48;
    pngIconSm->m_nWidth = 48;
    pngIconSm->m_sMimeType = "image/png";
    pngIconSm->m_sURL = "/images/icons/upnp_small_icon.png";
    m_listIcons.append(pngIconSm);

    // Small JPG Icon
    auto *jpgIconSm = new UPnpIcon();
    jpgIconSm->m_nDepth = 24;
    jpgIconSm->m_nHeight = 48;
    jpgIconSm->m_nWidth = 48;
    jpgIconSm->m_sMimeType = "image/jpeg";
    jpgIconSm->m_sURL = "/images/icons/upnp_small_icon.jpg";
    m_listIcons.append(jpgIconSm);
}

UPnpDevice::~UPnpDevice()
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

QString UPnpDevice::GetUDN(void) const
{
    if (m_sUDN.isEmpty())
        m_sUDN = "uuid:" + LookupUDN( m_sDeviceType );

    return m_sUDN;
}

void UPnpDevice::toMap(InfoMap &map) const
{
    map["name"] = m_sFriendlyName;
    map["modelname"] = m_sModelName;
    map["modelnumber"] = m_sModelNumber;
    map["modelurl"] = m_sModelURL;
    map["modeldescription"] = m_sModelDescription;
    map["manufacturer"] = m_sManufacturer;
    map["manufacturerurl"] = m_sManufacturerURL;
    map["devicetype"] = m_sDeviceType;
    map["serialnumber"] = m_sSerialNumber;
    map["UDN"] = m_sUDN;
    map["UPC"] = m_sUPC;
    map["protocolversion"] = m_protocolVersion;
}

UPnpService UPnpDevice::GetService(const QString &urn, bool *found) const
{
    UPnpService srv;

    bool done = false;

    for (auto *service : std::as_const(m_listServices))
    {
        // Ignore the service version
        if (service->m_sServiceType.section(':', 0, -2) == urn.section(':', 0, -2))
        {
            srv = *service;
            done = true;
            break;
        }
    }

    if (!done)
    {
        UPnpDeviceList::const_iterator dit = m_listDevices.begin();
        for (; dit != m_listDevices.end() && !done; ++dit)
            srv = (*dit)->GetService(urn, &done);
    }

    if (found)
        *found = done;

    return srv;
}

QString UPnpDevice::toString(uint padding) const
{
    QString ret =
        QString("UPnP Device\n"
                "===========\n"
                "deviceType:       %1\n"
                "friendlyName:     %2\n"
                "manufacturer:     %3\n"
                "manufacturerURL:  %4\n"
                "modelDescription: %5\n"
                "modelName:        %6\n"
                "modelNumber:      %7\n"
                "modelURL:         %8\n")
        .arg(m_sDeviceType,
             m_sFriendlyName,
             m_sManufacturer,
             m_sManufacturerURL,
             m_sModelDescription,
             m_sModelName,
             m_sModelNumber,
             m_sModelURL) +
        QString("serialNumber:     %1\n"
                "UPC:              %2\n"
                "presentationURL:  %3\n"
                "UDN:              %4\n")
        .arg(m_sSerialNumber,
             m_sUPC,
             m_sPresentationURL,
             m_sUDN);

    if (!m_lstExtra.empty())
    {
        ret += "Extra key value pairs\n";
        for (const auto & extra : std::as_const(m_lstExtra))
        {
            ret += extra.m_sName;
            ret += ":";
            int int_padding = 18 - (extra.m_sName.length() + 1);
            for (int i = 0; i < int_padding; i++)
                ret += " ";
            ret += QString("%1\n").arg(extra.m_sValue);
        }
    }

    if (!m_listIcons.empty())
    {
        ret += "Icon List:\n";
        for (auto *icon : std::as_const(m_listIcons))
            ret += icon->toString(padding+2) + "\n";
    }

    if (!m_listServices.empty())
    {
        ret += "Service List:\n";
        for (auto *service : std::as_const(m_listServices))
            ret += service->toString(padding+2) + "\n";
    }

    if (!m_listDevices.empty())
    {
        ret += "Device List:\n";
        for (auto *device : std::as_const(m_listDevices))
            ret += device->toString(padding+2) + "\n";
        ret += "\n";
    }

    // remove trailing newline
    if (ret.endsWith("\n"))
        ret = ret.left(ret.length()-1);

    // add any padding as necessary
    if (padding)
    {
        QString pad;
        for (uint i = 0; i < padding; i++)
            pad += " ";
        ret = pad + ret.replace("\n", QString("\n%1").arg(pad));
    }

    return ret;
}
