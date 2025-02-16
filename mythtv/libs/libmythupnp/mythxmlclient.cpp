//////////////////////////////////////////////////////////////////////////////
// Program Name: mythxmlclient.cpp
// Created     : Mar. 19, 2007
//
// Purpose     : Myth XML protocol client
//
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////
#include "mythxmlclient.h"

#include <QObject>

#include "libmythbase/mythlogging.h"
#include "libmythbase/mythversion.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MythXMLClient::MythXMLClient( const QUrl &url )
              :   SOAPClient( url,
                              "urn:schemas-mythtv-org:service:MythTV:1",
                              "/Myth")
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnPResultCode MythXMLClient::GetConnectionInfo( const QString &sPin, DatabaseParams *pParams, QString &sMsg )
{
    if (pParams == nullptr)
        return UPnPResult_InvalidArgs;

    int           nErrCode = 0;
    QString       sErrDesc;
    QStringMap    list;

    sMsg.clear();

    list.insert( "Pin", sPin );

    QDomDocument xmlResults = SendSOAPRequest(
        "GetConnectionInfo", list, nErrCode, sErrDesc);

    // --------------------------------------------------------------
    // Is this a valid response?
    // --------------------------------------------------------------

    QDomNode oNode = xmlResults.namedItem( "ConnectionInfo" );

    if (UPnPResult_Success == nErrCode && !oNode.isNull())
    {
        QDomNode dbNode = oNode.namedItem( "Database" );

        pParams->m_dbHostName   = GetNodeValue( dbNode, "Host"     , QString());
        pParams->m_dbPort       = GetNodeValue( dbNode, "Port"     , 0        );
        pParams->m_dbUserName   = GetNodeValue( dbNode, "UserName" , QString());
        pParams->m_dbPassword   = GetNodeValue( dbNode, "Password" , QString());
        pParams->m_dbName       = GetNodeValue( dbNode, "Name"     , QString());
        pParams->m_dbType       = GetNodeValue( dbNode, "Type"     , QString());

        QDomNode wolNode = oNode.namedItem( "WOL" );

        pParams->m_wolEnabled   = GetNodeValue( wolNode, "Enabled"  , false    );
        pParams->m_wolReconnect = std::chrono::seconds(GetNodeValue( wolNode, "Reconnect", 0 ));
        pParams->m_wolRetry     = GetNodeValue( wolNode, "Retry"    , 0        );
        pParams->m_wolCommand   = GetNodeValue( wolNode, "Command"  , QString());

        QDomNode verNode = oNode.namedItem( "Version" );

        pParams->m_verVersion   = GetNodeValue( verNode, "Version"  , ""       );
        pParams->m_verBranch    = GetNodeValue( verNode, "Branch"   , ""       );
        pParams->m_verProtocol  = GetNodeValue( verNode, "Protocol" , ""       );
        pParams->m_verBinary    = GetNodeValue( verNode, "Binary"   , ""       );
        pParams->m_verSchema    = GetNodeValue( verNode, "Schema"   , ""       );

        if ((pParams->m_verProtocol != MYTH_PROTO_VERSION) ||
            (pParams->m_verSchema   != MYTH_DATABASE_VERSION))
            // incompatible version, we cannot use this backend
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("MythXMLClient::GetConnectionInfo Failed - "
                        "Version Mismatch (%1,%2) != (%3,%4)")
                .arg(pParams->m_verProtocol,
                     pParams->m_verSchema,
                     MYTH_PROTO_VERSION,
                     MYTH_DATABASE_VERSION));
            sMsg = QObject::tr("Version Mismatch", "UPNP Errors");
            return UPnPResult_ActionFailed;
        }

        return UPnPResult_Success;
    }

    sMsg = sErrDesc;

    LOG(VB_GENERAL, LOG_ERR,
        QString("MythXMLClient::GetConnectionInfo Failed - (%1) %2")
        .arg(nErrCode) .arg(sErrDesc));

    if (( nErrCode == UPnPResult_HumanInterventionRequired ) || 
        ( nErrCode == UPnPResult_ActionNotAuthorized       ) ||
        ( nErrCode == UPnPResult_MythTV_XmlParseError      ) ||
        ( nErrCode == 501                                  ) )
    {
        // Service calls no longer return UPnPResult codes, 
        // convert standard 501 to UPnPResult code for now.
        sMsg = QObject::tr("Not Authorized", "UPNP Errors");
        return UPnPResult_ActionNotAuthorized;
    }

    sMsg = QObject::tr("Unknown Error", "UPNP Errors");
    return UPnPResult_ActionFailed;
}
