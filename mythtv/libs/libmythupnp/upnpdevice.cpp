//////////////////////////////////////////////////////////////////////////////
// Program Name: upnpdevice.cpp
//                                                                            
// Purpose - UPnp Device Description parser/generator
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "upnpdevice.h"

#include <qfile.h>

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
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnpDeviceDesc::~UPnpDeviceDesc()
{
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

    if ( !file.open( IO_ReadOnly ) )
        return false;

    QString sErrMsg;
    int     nErrLine = 0;
    int     nErrCol  = 0;
    bool    bSuccess = doc.setContent( &file, false, &sErrMsg, &nErrLine, &nErrCol );

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

    QDomNode  oNode = doc.documentElement();

    _InternalLoad( oNode.namedItem( "device" ), &m_rootDevice );

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpDeviceDesc::_InternalLoad( QDomNode oNode, UPnpDevice *pCurDevice )
{
    for ( oNode = oNode.firstChild(); !oNode.isNull(); oNode = oNode.nextSibling() )
    {
        QDomElement e = oNode.toElement();

        if (!e.isNull())
        {
            if ( e.tagName() == "deviceType"       ) { SetStrValue( e, pCurDevice->m_sDeviceType      ); continue; }
            if ( e.tagName() == "friendlyName"     ) { SetStrValue( e, pCurDevice->m_sFriendlyName    ); continue; }
            if ( e.tagName() == "manufacturer"     ) { SetStrValue( e, pCurDevice->m_sManufacturer    ); continue; }
            if ( e.tagName() == "manufacturerURL"  ) { SetStrValue( e, pCurDevice->m_sManufacturerURL ); continue; }
            if ( e.tagName() == "modelDescription" ) { SetStrValue( e, pCurDevice->m_sModelDescription); continue; }
            if ( e.tagName() == "modelName"        ) { SetStrValue( e, pCurDevice->m_sModelName       ); continue; }
            if ( e.tagName() == "modelNumber"      ) { SetStrValue( e, pCurDevice->m_sModelNumber     ); continue; }
            if ( e.tagName() == "modelURL"         ) { SetStrValue( e, pCurDevice->m_sModelURL        ); continue; }
            if ( e.tagName() == "serialNumber"     ) { SetStrValue( e, pCurDevice->m_sSerialNumber    ); continue; }
            if ( e.tagName() == "UPC"              ) { SetStrValue( e, pCurDevice->m_sUPC             ); continue; }
            if ( e.tagName() == "presentationURL"  ) { SetStrValue( e, pCurDevice->m_sPresentationURL ); continue; }

            if ( e.tagName() == "iconList"         ) { ProcessIconList   ( oNode, pCurDevice ); continue; }
            if ( e.tagName() == "serviceList"      ) { ProcessServiceList( oNode, pCurDevice ); continue; }
            if ( e.tagName() == "deviceList"       ) { ProcessDeviceList ( oNode, pCurDevice ); continue; }

            // Not one of the expected element names... add to extra list.

            QString sValue = "";

            SetStrValue( e, sValue );

            pCurDevice->m_lstExtra.append( new NameValue( e.tagName(), sValue ));
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpDeviceDesc::ProcessIconList( QDomNode oListNode, UPnpDevice *pDevice )
{
    for ( QDomNode oNode = oListNode.firstChild(); !oNode.isNull(); oNode = oNode.nextSibling() )
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
    for ( QDomNode oNode = oListNode.firstChild(); !oNode.isNull(); oNode = oNode.nextSibling() )
    {
        QDomElement e = oNode.toElement();

        if (!e.isNull())
        {
            if ( e.tagName() == "service" )
            {
                UPnpService *pService = new UPnpService(); 
                pDevice->m_listServices.append( pService );

                SetStrValue( e.namedItem( "serviceType" ), pService->m_sServiceType );
                SetStrValue( e.namedItem( "serviceId"   ), pService->m_sServiceId   );
                SetStrValue( e.namedItem( "SCPDURL"     ), pService->m_sSCPDURL     );
                SetStrValue( e.namedItem( "controlURL"  ), pService->m_sControlURL  );
                SetStrValue( e.namedItem( "eventSubURL" ), pService->m_sEventSubURL );

                VERBOSE(VB_UPNP,QString("ProcessServiceList adding service : %1 : %2 :")
                                .arg(pService->m_sServiceType)
                                .arg(pService->m_sServiceId ));
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpDeviceDesc::ProcessDeviceList( QDomNode oListNode, UPnpDevice *pDevice )
{
    for ( QDomNode oNode = oListNode.firstChild(); !oNode.isNull(); oNode = oNode.nextSibling() )
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

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString  UPnpDeviceDesc::GetValidXML( const QString &sBaseAddress )
{
    QString     sXML;
    QTextStream os( sXML, IO_WriteOnly );

    GetValidXML( sBaseAddress, os );

    return( sXML );
}
    
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void UPnpDeviceDesc::GetValidXML( const QString &sBaseAddress, QTextStream &os, const QString &sUserAgent )
{
    int nStatusPort = gContext->GetNumSetting("BackendStatusPort", 6544 );

//    os.setEncoding( QTextStream::UnicodeUTF8 );

    os << "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
          "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
            "<specVersion>"
              "<major>1</major>"
              "<minor>0</minor>"
            "</specVersion>"
            "<URLBase>http://" << sBaseAddress << ":" << nStatusPort << "/</URLBase>";

    OutputDevice( os, &m_rootDevice, sUserAgent );

    os << "</root>";
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

    os << "<device>";
    os << FormatValue( "deviceType"       , pDevice->m_sDeviceType      );

    QString sFriendlyName = pDevice->m_sFriendlyName;

    // ----------------------------------------------------------------------
    // Only override the root device
    // ----------------------------------------------------------------------

    if (pDevice == &m_rootDevice)
        sFriendlyName = gContext->GetSetting( "upnpFriendlyName", sFriendlyName );
    
    os << FormatValue( "friendlyName" , sFriendlyName );

    // ----------------------------------------------------------------------
    // XBox 360 needs specific values in the Device Description.
    //
    // -=>TODO: This should be externalized in a more generic/extension 
    //          kind of way.
    // ----------------------------------------------------------------------

    if (sUserAgent.startsWith( "Xbox/2.0", false ))
    {
        os << FormatValue( "manufacturer" , "Microsoft"                    );
        os << FormatValue( "modelName"    , "Windows Media Player Sharing" );
        os << FormatValue( "modelURL"     , "http://www.microsoft.com/"    );
    }
    else
    {
        os << FormatValue( "manufacturer" , pDevice->m_sManufacturer    );
        os << FormatValue( "modelName"    , pDevice->m_sModelName       );
        os << FormatValue( "modelURL"     , pDevice->m_sModelURL        );
    }

    os << FormatValue( "manufacturerURL"  , pDevice->m_sManufacturerURL );
    os << FormatValue( "modelDescription" , pDevice->m_sModelDescription);
    os << FormatValue( "modelNumber"      , pDevice->m_sModelNumber     );
    os << FormatValue( "serialNumber"     , pDevice->m_sSerialNumber    );
    os << FormatValue( "UDN"              , pDevice->GetUDN()           );
    os << FormatValue( "UPC"              , pDevice->m_sUPC             );
    os << FormatValue( "presentationURL"  , pDevice->m_sPresentationURL );

    for (NameValue *pNV  = pDevice->m_lstExtra.first(); 
                    pNV != NULL; 
                    pNV  = pDevice->m_lstExtra.next() )
    {
        // -=>TODO: Hack to handle one element with attributes... need to 
        //          handle attributes in a more generic way.

        if (pNV->sName == "dlna:X_DLNADOC")
            os << QString( "<dlna:X_DLNADOC xmlns:dlna=\"urn:schemas-dlna-org:device-1-0\">%1</dlna:X_DLNADOC>" ).arg( pNV->sValue);
        else
            os << FormatValue( pNV->sName, pNV->sValue );
    }

    // ----------------------------------------------------------------------
    // Output Any Icons.
    // ----------------------------------------------------------------------

    if (pDevice->m_listIcons.count() > 0)
    {
        os << "<iconList>";

        for ( UPnpIcon *pIcon  = pDevice->m_listIcons.first(); 
                        pIcon != NULL;
                        pIcon  = pDevice->m_listIcons.next() )
        {

            os << "<icon>";
            os << FormatValue( "mimetype", pIcon->m_sMimeType );
            os << FormatValue( "width"   , pIcon->m_nWidth    );
            os << FormatValue( "height"  , pIcon->m_nHeight   );
            os << FormatValue( "depth"   , pIcon->m_nDepth    );
            os << FormatValue( "url"     , pIcon->m_sURL      );
            os << "</icon>";
        }
        os << "</iconList>";
    }

    // ----------------------------------------------------------------------
    // Output any Services
    // ----------------------------------------------------------------------

    if (pDevice->m_listServices.count() > 0)
    {
        os << "<serviceList>";

        for ( UPnpService *pService  = pDevice->m_listServices.first(); 
                           pService != NULL;
                           pService  = pDevice->m_listServices.next() )
        {
            os << "<service>";
            os << FormatValue( "serviceType", pService->m_sServiceType );
            os << FormatValue( "serviceId"  , pService->m_sServiceId   );
            os << FormatValue( "SCPDURL"    , pService->m_sSCPDURL     );
            os << FormatValue( "controlURL" , pService->m_sControlURL  );
            os << FormatValue( "eventSubURL", pService->m_sEventSubURL );
            os << "</service>";
        }
        os << "</serviceList>";
    }

    // ----------------------------------------------------------------------
    // Output any Embedded Devices
    // ----------------------------------------------------------------------

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
    os << "</device>";
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString UPnpDeviceDesc::FormatValue( const QString &sName, const QString &sValue )
{
    QString sStr;

    if (sValue.length() > 0)
        sStr = QString( "<%1>%2</%3>" ).arg( sName ).arg(sValue).arg( sName );

    return( sStr );
}

/////////////////////////////////////////////////////////////////////////////
              
QString UPnpDeviceDesc::FormatValue( const QString &sName, int nValue )
{
    return( QString( "<%1>%2</%1>" ).arg( sName ).arg(nValue) );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString  UPnpDeviceDesc::FindDeviceUDN( UPnpDevice *pDevice, QString sST )
{
    if (sST == pDevice->m_sDeviceType)
        return( pDevice->GetUDN() );

    if (sST == pDevice->GetUDN())
        return( sST );
            
    // ----------------------------------------------------------------------
    // Check for matching Service
    // ----------------------------------------------------------------------

    for ( UPnpService *pService  = pDevice->m_listServices.first(); 
                       pService != NULL;
                       pService  = pDevice->m_listServices.next() )
    {
        if (sST == pService->m_sServiceType)
            return( pDevice->GetUDN() );
    }

    // ----------------------------------------------------------------------
    // Check Embedded Devices for a Match
    // ----------------------------------------------------------------------

    for ( UPnpDevice *pEmbeddedDevice  = pDevice->m_listDevices.first(); 
                      pEmbeddedDevice != NULL;
                      pEmbeddedDevice  = pDevice->m_listDevices.next() )
    {
        QString sUDN = FindDeviceUDN( pEmbeddedDevice, sST );

        if (sUDN.length() > 0)
            return( sUDN );
    }

    return( "" );
}
