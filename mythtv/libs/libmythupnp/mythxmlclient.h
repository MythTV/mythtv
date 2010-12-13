//////////////////////////////////////////////////////////////////////////////
// Program Name: mythxmlclient.h
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

#ifndef MYTHXMLCLIENT_H_
#define MYTHXMLCLIENT_H_

#include <QDomDocument>

#include "httpcomms.h"

#include "mythexp.h"

#include "mythdbparams.h"
#include "upnp.h"
#include "soapclient.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
//
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class MPUBLIC MythXMLClient : public SOAPClient
{
    protected:

        bool    m_bInQtThread;

    public:

        explicit MythXMLClient( const QUrl &url, bool bInQtThread = true );
        virtual ~MythXMLClient( );

        UPnPResultCode GetConnectionInfo( const QString &sPin, DatabaseParams *pParams, QString &sMsg );

        // GetServiceDescription
        // GetProgramGuide
        // GetHosts
        // GetKeys
        // GetSetting
        // PutSetting
        // GetChannelIcon
        // GetRecorded
        // GetPreviewImage
        // GetRecording
        // GetMusic
        // GetExpiring
        // GetProgramDetails
        // GetVideo

};

#endif

