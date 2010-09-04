//////////////////////////////////////////////////////////////////////////////
// Program Name: mythxmlclient.cpp
// Created     : Mar. 19, 2007
//
// Purpose     : MythTV XML protocol client
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

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

MythXMLClient::MythXMLClient( const QUrl &url, bool bInQtThread )
              :   SOAPClient( url,
                              "urn:schemas-mythtv-org:service:MythTv:1",
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

UPnPResultCode MythXMLClient::GetConnectionInfo( const QString &sPin,
                                                 DatabaseParams *pParams,
                                                 QString &sMsg )
{
    if (pParams == NULL)
        return UPnPResult_InvalidArgs;

    int           nErrCode = 0;
    QString       sErrDesc;
    QStringMap    list;

    sMsg.clear();

    list.insert( "Pin", sPin );

    if (SendSOAPRequest("GetConnectionInfo", list, nErrCode, sErrDesc,
                        m_bInQtThread ))
    {
        QString sXml = "<Info>" + list[ "Info" ] + "</Info>";

        sMsg = sErrDesc;

        QDomDocument doc;

        if ( !doc.setContent( sXml, false, &sErrDesc, &nErrCode ))
        {
            sMsg = QObject::tr("Error Requesting Connection Info");

            VERBOSE(VB_UPNP, QString("Error Requesting Connection Info : (%1)"
                                     " - %2").arg( nErrCode ).arg( sErrDesc ));

            return UPnPResult_ActionFailed;
        }

        // --------------------------------------------------------------
        // Is this a valid response?
        // --------------------------------------------------------------

        QDomNode infoNode = doc.namedItem( "Info" );

        if (!infoNode.isNull())
        {
            QDomNode dbNode = infoNode.namedItem( "Database" );

            pParams->dbHostName = GetNodeValue( dbNode, "Host", QString());
            pParams->dbPort     = GetNodeValue( dbNode, "Port", 0);
            pParams->dbUserName = GetNodeValue( dbNode, "UserName", QString());
            pParams->dbPassword = GetNodeValue( dbNode, "Password", QString());
            pParams->dbName     = GetNodeValue( dbNode, "Name", QString());
            pParams->dbType     = GetNodeValue( dbNode, "Type", QString());

            QDomNode wolNode = infoNode.namedItem( "WOL" );

            pParams->wolEnabled = GetNodeValue( wolNode, "Enabled"  , false);
            pParams->wolReconnect = GetNodeValue( wolNode, "Reconnect", 0);
            pParams->wolRetry = GetNodeValue( wolNode, "Retry", 0);
            pParams->wolCommand = GetNodeValue( wolNode, "Command", QString());

            return UPnPResult_Success;
        }
        else
        {
            if (sMsg.isEmpty())
                sMsg = QObject::tr("Unexpected Response");

            VERBOSE(VB_IMPORTANT,
                    QString("MythXMLClient::GetConnectionInfo Failed : "
                            "Unexpected Response - %1").arg(sXml));
        }
    }
    else
    {
        sMsg = sErrDesc;

        if (sMsg.isEmpty())
            sMsg = QObject::tr("Access Denied");

        VERBOSE(VB_IMPORTANT, QString("MythXMLClient::GetConnectionInfo "
                                      "Failed - (%1) %2")
                                      .arg(nErrCode).arg(sErrDesc));
    }

    if (nErrCode == UPnPResult_HumanInterventionRequired ||
        nErrCode == UPnPResult_ActionNotAuthorized)
        return (UPnPResultCode)nErrCode;

    return UPnPResult_ActionFailed;
}
