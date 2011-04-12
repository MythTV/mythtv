//////////////////////////////////////////////////////////////////////////////
// Program Name: mythxmlclient.cpp
// Created     : Mar. 19, 2007
//
// Purpose     : Myth XML protocol client
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

#include "mythxmlclient.h"

#include <QObject>
#include "mythverbose.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MythXMLClient::MythXMLClient( const QUrl &url, bool bInQtThread )
              :   SOAPClient( url,
                              "urn:schemas-mythtv-org:service:MythTV:1",
                              "/Myth")
{
    m_bInQtThread = bInQtThread;
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

    QDomDocument xmlResults = SendSOAPRequest( "GetConnectionInfo", list, nErrCode, sErrDesc, m_bInQtThread );

    // --------------------------------------------------------------
    // Is this a valid response?
    // --------------------------------------------------------------

    QDomNode oNode = xmlResults.namedItem( "GetConnectionInfoResult" );

    if (!oNode.isNull())
    {
        QDomNode dbNode = oNode.namedItem( "Database" );

        pParams->dbHostName     = GetNodeValue( dbNode, "Host"     , QString( ));
        pParams->dbPort         = GetNodeValue( dbNode, "Port"     , 0         );
        pParams->dbUserName     = GetNodeValue( dbNode, "UserName" , QString( ));
        pParams->dbPassword     = GetNodeValue( dbNode, "Password" , QString( ));
        pParams->dbName         = GetNodeValue( dbNode, "Name"     , QString( ));
        pParams->dbType         = GetNodeValue( dbNode, "Type"     , QString( ));

        QDomNode wolNode = oNode.namedItem( "WOL" );

        pParams->wolEnabled     = GetNodeValue( wolNode, "Enabled"  , false     );
        pParams->wolReconnect   = GetNodeValue( wolNode, "Reconnect", 0         );
        pParams->wolRetry       = GetNodeValue( wolNode, "Retry"    , 0         );
        pParams->wolCommand     = GetNodeValue( wolNode, "Command"  , QString( ));

        return UPnPResult_Success;
    }
    else
    {
        
        nErrCode = GetNodeValue( xmlResults, "Fault/detail/UPnPResult/errorCode"       , 500 );
        sErrDesc = GetNodeValue( xmlResults, "Fault/detail/UPnPResult/errorDescription", QString( "Unknown" ));

        sMsg = sErrDesc;

        if (sMsg.isEmpty())
            sMsg = QObject::tr("Access Denied");

        VERBOSE( VB_IMPORTANT, QString( "MythXMLClient::GetConnectionInfo Failed - (%1) %2" )
                             .arg( nErrCode )
                             .arg( sErrDesc ));
    }

    if (( nErrCode == UPnPResult_HumanInterventionRequired ) || 
        ( nErrCode == UPnPResult_ActionNotAuthorized       ) ||
        ( nErrCode == 501                                  ))
    {
        // Service calls no longer return UPnPResult codes, 
        // convert standard 501 to UPnPResult code for now.
        return UPnPResult_ActionNotAuthorized;
    }

    return UPnPResult_ActionFailed;
}
