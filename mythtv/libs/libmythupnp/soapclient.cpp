//////////////////////////////////////////////////////////////////////////////
// Program Name: soapclient.cpp
// Created     : Mar. 19, 2007
//
// Purpose     : SOAP client base class
//                                                                            
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "soapclient.h"

#include <QBuffer>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QStringConverter>
#endif

#include "libmythbase/mythdownloadmanager.h"
#include "libmythbase/mythlogging.h"

#include "httprequest.h"
#include "upnp.h"

#define LOC      QString("SOAPClient: ")

/** \brief Full SOAPClient constructor. After constructing the client
 *         with this constructor it is ready for SendSOAPRequest().
 *  \param url          The host and port portion of the command URL
 *  \param sNamespace   The part of the action before the # character
 *  \param sControlPath The path portion of the command URL
 */
SOAPClient::SOAPClient(QUrl    url,
                       QString sNamespace,
                       QString sControlPath) :
    m_url(std::move(url)), m_sNamespace(std::move(sNamespace)),
    m_sControlPath(std::move(sControlPath))
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
    QStringList parts = sName.split('/', Qt::SkipEmptyParts);
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
 * \param sErrDesc returns error description from device, when applicable.
 *
 * \return Returns a QDomDocument containing output parameters on success.
 */

QDomDocument SOAPClient::SendSOAPRequest(const QString &sMethod,
                                         QStringMap    &list,
                                         int           &nErrCode,
                                         QString       &sErrDesc)
{
    QUrl url(m_url);

    QString path  = m_sControlPath;
    path.append("/");
    path.append(sMethod);

    // Service url port is 6 less than upnp port (see MediaServer::Init)
    url.setPort(m_url.port() - 6);

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
    QString soapHeader = QString("\"%1#%2\"").arg(m_sNamespace, sMethod);
    headers.insert("SOAPACTION", soapHeader.toUtf8());
    headers.insert("User-Agent", "Mozilla/9.876 (X11; U; Linux 2.2.12-20 i686, en) "
                                 "Gecko/25250101 Netscape/5.432b1");
    // --------------------------------------------------------------
    // Build request payload
    // --------------------------------------------------------------

    QByteArray  aBuffer;
    QUrlQuery query;
    for (QStringMap::iterator it = list.begin(); it != list.end(); ++it)
    {
        query.addQueryItem(it.key(),*it);
    }

    url.setPath(path);
    url.setQuery(query);

    // --------------------------------------------------------------
    // Perform Request
    // --------------------------------------------------------------

    LOG(VB_UPNP, LOG_DEBUG,
        QString("SOAPClient(%1) sending:\n %2").arg(url.toString() /*, aBuffer.constData()*/ ));

    QString sXml;

    if (!GetMythDownloadManager()->postAuth(url.toString(), &aBuffer, nullptr, nullptr, &headers))
    {
        LOG(VB_GENERAL, LOG_ERR, QString("SOAPClient::SendSOAPRequest: request failed: %1")
                                         .arg(url.toString()));
    }
    else
    {
        sXml = QString(aBuffer);
    }

    // --------------------------------------------------------------
    // Parse response
    // --------------------------------------------------------------

    LOG(VB_UPNP, LOG_DEBUG, "SOAPClient response:\n" +
                            QString("%1\n").arg(sXml));

    // TODO handle timeout without response correctly.

    list.clear();

    QDomDocument doc;
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
    int ErrLineNum = 0;

    if (!doc.setContent(sXml, true, &sErrDesc, &ErrLineNum))
    {
        nErrCode = UPnPResult_MythTV_XmlParseError;
        LOG(VB_UPNP, LOG_ERR,
            QString("SendSOAPRequest(%1) - Invalid response from %2. Error %3: %4. Response: %5")
                .arg(sMethod, url.toString(),
                     QString::number(nErrCode), sErrDesc, sXml));
        return xmlResult;
    }
#else
    auto parseResult = doc.setContent(sXml,QDomDocument::ParseOption::UseNamespaceProcessing);
    if (!parseResult)
    {
        nErrCode = UPnPResult_MythTV_XmlParseError;
        LOG(VB_UPNP, LOG_ERR,
            QString("SendSOAPRequest(%1) - Invalid response from %2. Error %3: %4. Response: %5")
                .arg(sMethod, url.toString(),
                     QString::number(nErrCode), parseResult.errorMessage, sXml));
        return xmlResult;
    }
#endif
    return doc;
}

