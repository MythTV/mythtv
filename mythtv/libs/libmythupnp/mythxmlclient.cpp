//////////////////////////////////////////////////////////////////////////////
// Program Name: mythxmlclient.cpp
// Created     : Mar. 19, 2007
//
// Purpose     : Myth XML protocol client
//
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#include "mythversion.h"
#include "mythxmlclient.h"
#include "mythlogging.h"

#include <QObject>

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

MythXMLClient::~MythXMLClient()
{
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

UPnPResultCode MythXMLClient::GetConnectionInfo( const QString &sPin, DatabaseParams *pParams, QString &sMsg )
{
    if (pParams == NULL)
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

    QDomNode oNode = xmlResults.namedItem( "GetConnectionInfoResult" );

    if (UPnPResult_Success == nErrCode && !oNode.isNull())
    {
        QDomNode dbNode = oNode.namedItem( "Database" );

        pParams->dbHostName   = GetNodeValue( dbNode, "Host"     , QString());
        pParams->dbPort       = GetNodeValue( dbNode, "Port"     , 0        );
        pParams->dbUserName   = GetNodeValue( dbNode, "UserName" , QString());
        pParams->dbPassword   = GetNodeValue( dbNode, "Password" , QString());
        pParams->dbName       = GetNodeValue( dbNode, "Name"     , QString());
        pParams->dbType       = GetNodeValue( dbNode, "Type"     , QString());

        QDomNode wolNode = oNode.namedItem( "WOL" );

        pParams->wolEnabled   = GetNodeValue( wolNode, "Enabled"  , false    );
        pParams->wolReconnect = GetNodeValue( wolNode, "Reconnect", 0        );
        pParams->wolRetry     = GetNodeValue( wolNode, "Retry"    , 0        );
        pParams->wolCommand   = GetNodeValue( wolNode, "Command"  , QString());

        QDomNode verNode = oNode.namedItem( "Version" );

        pParams->verVersion   = GetNodeValue( verNode, "Version"  , ""       );
        pParams->verBranch    = GetNodeValue( verNode, "Branch"   , ""       );
        pParams->verProtocol  = GetNodeValue( verNode, "Protocol" , ""       );
        pParams->verBinary    = GetNodeValue( verNode, "Binary"   , ""       );
        pParams->verSchema    = GetNodeValue( verNode, "Schema"   , ""       );

        if ((pParams->verProtocol != MYTH_PROTO_VERSION) ||
            (pParams->verSchema   != MYTH_DATABASE_VERSION))
            // incompatible version, we cannot use this backend
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("MythXMLClient::GetConnectionInfo Failed - "
                        "Version Mismatch (%1,%2) != (%3,%4)")
                .arg(pParams->verProtocol)
                .arg(pParams->verSchema)
                .arg(MYTH_PROTO_VERSION)
                .arg(MYTH_DATABASE_VERSION));
            sMsg = QObject::tr("Version Mismatch", "UPNP Errors");
            return UPnPResult_ActionFailed;
        }

        return UPnPResult_Success;
    }
    else
    {
        sMsg = sErrDesc;

        LOG(VB_GENERAL, LOG_ERR,
            QString("MythXMLClient::GetConnectionInfo Failed - (%1) %2")
                .arg(nErrCode) .arg(sErrDesc));
    }

    if (( nErrCode == UPnPResult_HumanInterventionRequired ) || 
        ( nErrCode == UPnPResult_ActionNotAuthorized       ) ||
        ( nErrCode == 501                                  ))
    {
        // Service calls no longer return UPnPResult codes, 
        // convert standard 501 to UPnPResult code for now.
        sMsg = QObject::tr("Not Authorized", "UPNP Errors");
        return UPnPResult_ActionNotAuthorized;
    }

    sMsg = QObject::tr("Unknown Error", "UPNP Errors");
    return UPnPResult_ActionFailed;
}
