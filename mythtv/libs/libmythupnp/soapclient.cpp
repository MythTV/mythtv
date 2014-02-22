//////////////////////////////////////////////////////////////////////////////
// Program Name: soapclient.cpp
// Created     : Mar. 19, 2007
//
// Purpose     : SOAP client base class
//                                                                            
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#include <QBuffer>

#include "soapclient.h"

#include "mythlogging.h"
#include "httprequest.h"
#include "upnp.h"
#include "mythdownloadmanager.h"

#define LOC      QString("SOAPClient: ")

/** \brief Full SOAPClient constructor. After constructing the client
 *         with this constructor it is ready for SendSOAPRequest().
 *  \param url          The host and port portion of the command URL
 *  \param sNamespace   The part of the action before the # character
 *  \param sControlPath The path portion of the command URL
 */
SOAPClient::SOAPClient(const QUrl    &url,
                       const QString &sNamespace,
                       const QString &sControlPath) :
    m_url(url), m_sNamespace(sNamespace), m_sControlPath(sControlPath)
{
}


/** \brief SOAPClient Initializer. After constructing the client
 *         with the empty constructor call this before calling
 *         SendSOAPRequest() the first time.
 */
bool SOAPClient::Init(const QUrl    &url,
                      const QString &sNamespace,
                      const QString &sControlPath)
{
    bool ok = true;
    if (sNamespace.isEmpty())
    {
        ok = false;
        LOG(VB_GENERAL, LOG_ERR, LOC + "Init() given blank namespace");
    }

    QUrl test(url);
    test.setPath(sControlPath);
    if (!test.isValid())
    {
        ok = false;
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Init() given invalid control URL %1")
                .arg(test.toString()));
    }

    if (ok)
    {
        m_url          = url;
        m_sNamespace   = sNamespace;
        m_sControlPath = sControlPath;
    }
    else
    {
        m_url = QUrl();
        m_sNamespace.clear();
        m_sControlPath.clear();
    }

    return ok;
}

/// Used by GeNodeValue() methods to find the named node.
QDomNode SOAPClient::FindNode(
    const QString &sName, const QDomNode &baseNode) const
{
    QStringList parts = sName.split('/', QString::SkipEmptyParts);
    return FindNodeInternal(parts, baseNode);
}

/// This is an internal function used to implement FindNode
QDomNode SOAPClient::FindNodeInternal(
    QStringList &sParts, const QDomNode &curNode) const
{
    if (sParts.empty())
        return curNode;

    QString sName = sParts.front();
    sParts.pop_front();

    QDomNode child = curNode.namedItem(sName);

    if (child.isNull() )
        sParts.clear();

    return FindNodeInternal(sParts, child);
}

/// Gets the named value using QDomNode as the baseNode in the search,
/// returns default if it is not found.
int SOAPClient::GetNodeValue(
    const QDomNode &node, const QString &sName, int nDefault) const
{
    QString sValue = GetNodeValue(node, sName, QString::number(nDefault));
    return sValue.toInt();
}

/// Gets the named value using QDomNode as the baseNode in the search,
/// returns default if it is not found.
bool SOAPClient::GetNodeValue(
    const QDomNode &node, const QString &sName, bool bDefault) const
{
    QString sDefault = (bDefault) ? "true" : "false";
    QString sValue   = GetNodeValue(node, sName, sDefault);
    if (sValue.isEmpty())
        return bDefault;

    char ret = sValue[0].toLatin1();
    switch (ret)
    {
        case 't': case 'T': case 'y': case 'Y': case '1':
            return true;
        case 'f': case 'F': case 'n': case 'N': case '0':
            return false;
        default:
            return bDefault;
    }
}

/// Gets the named value using QDomNode as the baseNode in the search,
/// returns default if it is not found.
QString SOAPClient::GetNodeValue(
    const QDomNode &node, const QString &sName, const QString &sDefault) const
{
    if (node.isNull())
        return sDefault;

    QString  sValue  = "";
    QDomNode valNode = FindNode(sName, node);

    if (!valNode.isNull())
    {
        // -=>TODO: Assumes first child is Text Node.

        QDomText oText = valNode.firstChild().toText();

        if (!oText.isNull())
            sValue = oText.nodeValue();

        return QUrl::fromPercentEncoding(sValue.toUtf8());
    }

    return sDefault;
}

/** Actually sends the sMethod action to the command URL specified
 *  in the constructor (url+[/]+sControlPath).
 *
 * \param sMethod method to be invoked. e.g. "SetChannel",
 *                "GetConnectionInfoResult"
 *
 * \param list Parsed as a series of key value pairs for the input params
 *             and then cleared and used for the output params.
 *
 * \param nErrCode set to zero on success, non-zero in case of error.
 *
 * \param sErrCode returns error description from device, when applicable.
 *
 * \return Returns a QDomDocument containing output parameters on success.
 */

QDomDocument SOAPClient::SendSOAPRequest(const QString &sMethod,
                                         QStringMap    &list,
                                         int           &nErrCode,
                                         QString       &sErrDesc)
{
    QUrl url(m_url);

    url.setPath(m_sControlPath);

    nErrCode = UPnPResult_Success;
    sErrDesc = "";

    QDomDocument xmlResult;
    if (m_sNamespace.isEmpty())
    {
        nErrCode = UPnPResult_MythTV_NoNamespaceGiven;
        sErrDesc = "No namespace given";
        return xmlResult;
    }

    // --------------------------------------------------------------
    // Add appropriate headers
    // --------------------------------------------------------------
    QHash<QByteArray, QByteArray> headers;

    headers.insert("Content-Type", "text/xml; charset=\"utf-8\"");
    QString soapHeader = QString("\"%1#%2\"").arg(m_sNamespace).arg(sMethod);
    headers.insert("SOAPACTION", soapHeader.toUtf8());
    headers.insert("User-Agent", "Mozilla/9.876 (X11; U; Linux 2.2.12-20 i686, en) "
                                 "Gecko/25250101 Netscape/5.432b1");
    // --------------------------------------------------------------
    // Build request payload
    // --------------------------------------------------------------

    QByteArray  aBuffer;
    QTextStream os( &aBuffer );

    os.setCodec("UTF-8");

    os << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"; 
    os << "<s:Envelope "
        " s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\""
        " xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">\r\n";
    os << " <s:Body>\r\n";
    os << "  <u:" << sMethod << " xmlns:u=\"" << m_sNamespace << "\">\r\n";

    // --------------------------------------------------------------
    // Add parameters from list
    // --------------------------------------------------------------

    for (QStringMap::iterator it = list.begin(); it != list.end(); ++it)
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

    LOG(VB_UPNP, LOG_DEBUG,
        QString("SOAPClient(%1) sending:\n %2").arg(url.toString()).arg(aBuffer.constData()));

    QString sXml;

    if (!GetMythDownloadManager()->postAuth(url.toString(), &aBuffer, NULL, NULL, &headers))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("SOAPClient::SendSOAPRequest: request failed: %1")
                                         .arg(url.toString()));
    }
    else
        sXml = QString(aBuffer);

    // --------------------------------------------------------------
    // Parse response
    // --------------------------------------------------------------

    LOG(VB_UPNP, LOG_DEBUG, "SOAPClient response:\n" +
                            QString("%1\n").arg(sXml));

    // TODO handle timeout without response correctly.

    list.clear();

    QDomDocument doc;

    if (!doc.setContent(sXml, true, &sErrDesc, &nErrCode))
    {
        LOG(VB_UPNP, LOG_ERR,
            QString("SendSOAPRequest( %1 ) - Invalid response from %2")
                .arg(sMethod).arg(url.toString()) + 
            QString("%1: %2").arg(nErrCode).arg(sErrDesc));

        return xmlResult;
    }

    // --------------------------------------------------------------
    // Is this a valid response?
    // --------------------------------------------------------------

    QString      sResponseName = sMethod + "Response";
    QDomNodeList oNodeList     =
        doc.elementsByTagNameNS(m_sNamespace, sResponseName);

    if (oNodeList.count() == 0)
    {
        // --------------------------------------------------------------
        // Must be a fault... parse it to return reason
        // --------------------------------------------------------------

        nErrCode = GetNodeValue(
            doc, "Envelope/Body/Fault/detail/UPnPError/errorCode", 500);
        sErrDesc = GetNodeValue(
            doc, "Envelope/Body/Fault/detail/UPnPError/errorDescription", "");
        if (sErrDesc.isEmpty())
            sErrDesc = QString("Unknown #%1").arg(nErrCode);

        QDomNode oNode  = FindNode( "Envelope/Body/Fault", doc );

        oNode = xmlResult.importNode( oNode, true );
        xmlResult.appendChild( oNode );

        return xmlResult;
    }

    QDomNode oMethod = oNodeList.item(0);
    if (oMethod.isNull())
        return xmlResult;

    QDomNode oNode = oMethod.firstChild(); 
    for (; !oNode.isNull(); oNode = oNode.nextSibling())
    {
        QDomElement e = oNode.toElement();
        if (e.isNull())
            continue;

        QString sName  = e.tagName();
        QString sValue = "";
    
        QDomText  oText = oNode.firstChild().toText();
    
        if (!oText.isNull())
            sValue = oText.nodeValue();

        list.insert(QUrl::fromPercentEncoding(sName.toUtf8()),
                    QUrl::fromPercentEncoding(sValue.toUtf8()));
    }

    // Create copy of oMethod that can be used with xmlResult.

    oMethod = xmlResult.importNode( oMethod.firstChild(), true  );

    // importNode does not attach the new nodes to the document,
    // do it here.

    xmlResult.appendChild( oMethod );

    return xmlResult;
}

