//////////////////////////////////////////////////////////////////////////////
// Program Name: soapclient.cpp
// Created     : Mar. 19, 2007
//
// Purpose     : SOAP client base class
//                                                                            
// Copyright (c) 2007 David Blain <mythtv@theblains.net>
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

#include <QBuffer>

#include "soapclient.h"

#include "mythverbose.h"
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
    QStringList parts = sName.split('/', QString::SkipEmptyParts);

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

    if (sValue.startsWith( 'T' , Qt::CaseInsensitive ) || 
        sValue.startsWith( 'Y' , Qt::CaseInsensitive ) ||
        sValue.startsWith( '1' , Qt::CaseInsensitive ) )
    {
        return true;
    }

    if (sValue.startsWith( 'F' , Qt::CaseInsensitive ) || 
        sValue.startsWith( 'N' , Qt::CaseInsensitive ) ||
        sValue.startsWith( '0' , Qt::CaseInsensitive ) )
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

        return QUrl::fromPercentEncoding(sValue.toUtf8());
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
    QTextStream os( &aBuffer );

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
        os << HTTPRequest::Encode( *it );
        os << "</"   << it.key() << ">\r\n";
    }

    os << "  </u:" << sMethod << ">\r\n";
    os << " </s:Body>\r\n";
    os << "</s:Envelope>\r\n";

    os.flush();

    // --------------------------------------------------------------
    // Perform Request
    // --------------------------------------------------------------

    QBuffer     buff( &aBuffer );

    QString sXml = HttpComms::postHttp( url, 
                                        &header,
                                        (QIODevice *)&buff,
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

                    list.insert(QUrl::fromPercentEncoding(sName.toUtf8()),
                                QUrl::fromPercentEncoding(sValue.toUtf8()));
                }
            }
        }

        return true;
    }

    // --------------------------------------------------------------
    // Must be a fault... parse it to return reason
    // --------------------------------------------------------------

    nErrCode = GetNodeValue( doc, "Envelope/Body/Fault/detail/UPnPResult/errorCode"       , 500 );
    sErrDesc = GetNodeValue( doc, "Envelope/Body/Fault/detail/UPnPResult/errorDescription", QString( "Unknown" ));

    return false;
}
