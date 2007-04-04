//////////////////////////////////////////////////////////////////////////////
// Program Name: soapclient.cpp
//                                                                            
// Purpose - SOAP client base class
//                                                                            
// Created By  : David Blain                    Created On : Mar. 19, 2007
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "soapclient.h"

#include "mythcontext.h"
#include "httprequest.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SOAPClient::SOAPClient( const QUrl    &url,
                        const QString &sNamespace,
                        const QString &sControlPath )
{
    m_url          = url;
    m_sNamespace   = sNamespace;
    m_sControlPath = sControlPath;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

SOAPClient::~SOAPClient() 
{
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QDomNode SOAPClient::FindNode( const QString &sName, QDomNode &baseNode )
{
    QStringList parts = QStringList::split( "/", sName );

    return FindNode( parts, baseNode );

}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

QDomNode SOAPClient::FindNode( QStringList &sParts, QDomNode &curNode )
{
    if (sParts.empty())
        return curNode;

    QString sName = sParts.front();
    sParts.pop_front();

    QDomNode child = curNode.namedItem( sName );

    if (child.isNull() )
        sParts.clear();

    return FindNode( sParts, child );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

int SOAPClient::GetNodeValue( QDomNode &node, const QString &sName, int nDefault )
{
    QString  sValue = GetNodeValue( node, sName, QString::number( nDefault ) );
    
    return sValue.toInt();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool SOAPClient::GetNodeValue( QDomNode &node, const QString &sName, bool bDefault )
{
    QString sDefault = (bDefault) ? "true" : "false";
    QString sValue   = GetNodeValue( node, sName, sDefault );

    if (sValue.startsWith( "T" , false ) || 
        sValue.startsWith( "Y" , false ) ||
        sValue.startsWith( "1" , false ) )
    {
        return true;
    }

    if (sValue.startsWith( "F" , false ) || 
        sValue.startsWith( "N" , false ) ||
        sValue.startsWith( "0" , false ) )
    {
        return false;
    }

    return bDefault;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString SOAPClient::GetNodeValue( QDomNode &node, const QString &sName, const QString &sDefault )
{
    if (node.isNull())
        return sDefault;

    QString  sValue  = "";
    QDomNode valNode = FindNode( sName, node );

    if (!valNode.isNull())
    {
        // -=>TODO: Assumes first child is Text Node.

        QDomText  oText = valNode.firstChild().toText();

        if (!oText.isNull())
            sValue = oText.nodeValue();

        QUrl::decode( sValue );
    
        return sValue;
    }

    return sDefault;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool SOAPClient::SendSOAPRequest( const QString    &sMethod, 
                                        QStringMap &list, 
                                        int        &nErrCode, 
                                        QString    &sErrDesc,
                                        bool        bInQtThread )
{
    QUrl url( m_url );

    url.setPath( m_sControlPath );

    // --------------------------------------------------------------
    // Add appropriate headers
    // --------------------------------------------------------------

    QHttpRequestHeader header;

    header.setValue("CONTENT-TYPE", "text/xml; charset=\"utf-8\"" );
    header.setValue("SOAPACTION"  , QString( "\"%1#GetConnectionInfo\"" )
                                       .arg( m_sNamespace ));

    // --------------------------------------------------------------
    // Build request payload
    // --------------------------------------------------------------

    QByteArray  aBuffer;
    QTextStream os( aBuffer, IO_WriteOnly );

    os << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"; 
    os << "<s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">\r\n";
    os << " <s:Body>\r\n";
    os << "  <u:" << sMethod << " xmlns:u=\"" << m_sNamespace << "\">\r\n";

    // --------------------------------------------------------------
    // Add parameters from list
    // --------------------------------------------------------------

    for ( QStringMap::iterator it  = list.begin(); 
                               it != list.end(); 
                             ++it ) 
    {                                                               
        os << "   <" << it.key() << ">";
        os << HTTPRequest::Encode( it.data() );
        os << "</"   << it.key() << ">\r\n";
    }

    os << "  </u:" << sMethod << ">\r\n";
    os << " </s:Body>\r\n";
    os << "</s:Envelope>\r\n";

    // --------------------------------------------------------------
    // Perform Request
    // --------------------------------------------------------------

    QBuffer buff( aBuffer );

    QString sXml = HttpComms::postHttp( url, 
                                        &header,
                                        &buff,
                                        10000, // ms
                                        3,     // retries
                                        0,     // redirects
                                        false, // allow gzip
                                        NULL,  // login
                                        bInQtThread );

    // --------------------------------------------------------------
    // Parse response
    // --------------------------------------------------------------

    list.clear();

    QDomDocument doc;

    if ( !doc.setContent( sXml, true, &sErrDesc, &nErrCode ))
    {
        VERBOSE( VB_UPNP, QString( "MythXMLClient::SendSOAPRequest( %1 ) - Invalid response from %2" )
                             .arg( sMethod   )
                             .arg( url.toString() ));
        return false;
    }

    // --------------------------------------------------------------
    // Is this a valid response?
    // --------------------------------------------------------------

    QString      sResponseName = sMethod + "Response";
    QDomNodeList oNodeList     = doc.elementsByTagNameNS( m_sNamespace, sResponseName );

    if (oNodeList.count() > 0)
    {
        QDomNode oMethod = oNodeList.item(0);

        if (!oMethod.isNull())
        {

            for ( QDomNode oNode = oMethod.firstChild(); !oNode.isNull(); 
                           oNode = oNode.nextSibling() )
            {
                QDomElement e = oNode.toElement();

                if (!e.isNull())
                {
                    QString sName  = e.tagName();
                    QString sValue = "";
    
                    QDomText  oText = oNode.firstChild().toText();
    
                    if (!oText.isNull())
                        sValue = oText.nodeValue();
             
                    QUrl::decode( sName  );
                    QUrl::decode( sValue );

                    list.insert( sName, sValue );
                }
            }
        }

        return true;
    }

    // --------------------------------------------------------------
    // Must be a fault... parse it to return reason
    // --------------------------------------------------------------

    nErrCode = GetNodeValue( doc, "Envelope/Body/Fault/detail/UPnPResult/errorCode"       , 500 );
    sErrDesc = GetNodeValue( doc, "Envelope/Body/Fault/detail/UPnPResult/errorDescription", "Unknown" );

    return false;
}
