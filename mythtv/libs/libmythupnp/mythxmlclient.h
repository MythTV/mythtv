//////////////////////////////////////////////////////////////////////////////
// Program Name: mythxmlclient.h
// Created     : Mar. 19, 2007
//
// Purpose     : Myth XML protocol client
//
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef MYTHXMLCLIENT_H_
#define MYTHXMLCLIENT_H_

#include <QDomDocument>

#include "upnpexp.h"

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

class UPNP_PUBLIC MythXMLClient : public SOAPClient
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

