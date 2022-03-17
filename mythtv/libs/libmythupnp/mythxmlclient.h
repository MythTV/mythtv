//////////////////////////////////////////////////////////////////////////////
// Program Name: mythxmlclient.h
// Created     : Mar. 19, 2007
//
// Purpose     : Myth XML protocol client
//
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef MYTHXMLCLIENT_H_
#define MYTHXMLCLIENT_H_

#include <QDomDocument>

#include "libmythbase/mythdbparams.h"
#include "libmythupnp/upnpexp.h"
#include "libmythupnp/soapclient.h"
#include "libmythupnp/upnp.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
//
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC MythXMLClient : public SOAPClient
{
    public:

        explicit MythXMLClient( const QUrl &url );
        ~MythXMLClient( ) override = default;

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

