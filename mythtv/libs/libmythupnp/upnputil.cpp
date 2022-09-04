//////////////////////////////////////////////////////////////////////////////
// Program Name: upnputil.cpp
// Created     : Jan. 15, 2007
//
// Purpose     : Global Helper Methods...
//                                                                            
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

// POSIX headers
#include <sys/types.h>
#include <sys/time.h>
#include <cerrno>

// Qt headers
#include <QUuid>
#include <QStringList>

// MythTV headers
#include "httprequest.h"
#include "libmythbase/compat.h"
#include "libmythbase/configuration.h"
#include "libmythbase/mythlogging.h"

#include "upnp.h"
#include "upnphelpers.h"
#include "upnputil.h"

// POSIX headers 2, needs to be after compat.h for OS X
#ifndef _WIN32
#include <sys/ioctl.h>
#endif // _WIN32


#include <zlib.h>
#undef Z_NULL
#define Z_NULL nullptr

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString LookupUDN( const QString &sDeviceType )
{
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    QStringList sList = sDeviceType.split(':', QString::SkipEmptyParts);
#else
    QStringList sList = sDeviceType.split(':', Qt::SkipEmptyParts);
#endif
    QString     sLoc  = "LookupUDN(" + sDeviceType + ')';

    if (sList.size() <= 2) 
    { 
        LOG(VB_GENERAL, LOG_ERR, sLoc + "- bad device type '" +
                                 sDeviceType + "', not enough tokens"); 
        return {};
    }

    sList.removeLast();
    auto config = XmlConfiguration(); // read-write
    QString sName = "UPnP/UDN/" + sList.last();
    QString sUDN  = config.GetValue(sName, "");

    LOG(VB_UPNP, LOG_INFO, sLoc + " sName=" + sName + ", sUDN=" + sUDN);

    // Generate new UUID if current is missing or broken
    if (sUDN.isEmpty() || sUDN.startsWith("{"))
    {
        sUDN = QUuid::createUuid().toString();
        // QUuid returns the uuid enclosed with braces {} which is not
        // DLNA compliant, we need to remove them
        sUDN = sUDN.mid(1, 36);

        config.SetValue(sName, sUDN);
        config.Save();
    }

    return( sUDN );
}

/**
 * \brief Return a QStringList containing the supported Source Protocols
 *
 * TODO Extend this to dynamically list all supported protocols (e.g. RTSP)
 *      and any DLNA profile stuff that we can figure out
 */
QStringList GetSourceProtocolInfos()
{
    QStringList mimeTypes = HTTPRequest::GetSupportedMimeTypes();

    QString protocolStr("http-get:*:%1:%2");
    QStringList protocolList;
    QStringList::Iterator it;
    for (it = mimeTypes.begin(); it < mimeTypes.end(); ++it)
    {
        // HACK
        QString flags = DLNA::FlagsString(DLNA::ktm_s | DLNA::ktm_b | DLNA::kv1_5_flag);
        if (*it == "video/mpeg")
        {
            protocolList << protocolStr.arg(*it, "DLNA.ORG_PN=MPEG_PS_PAL;" + flags);
            protocolList << protocolStr.arg(*it, "DLNA.ORG_PN=MPEG_PS_NTSC;" + flags);
            protocolList << protocolStr.arg(*it, "DLNA.ORG_PN=MPEG_PS_SD_DTS;" + flags);
            protocolList << protocolStr.arg(*it, "DLNA.ORG_PN=AVC_TS_NA_ISO;" + flags);
            protocolList << protocolStr.arg(*it, "DLNA.ORG_PN=MPEG_TS_HD_NA_ISO;" + flags);
            protocolList << protocolStr.arg(*it, "DLNA.ORG_PN=MPEG_TS_SD_NA_ISO;" + flags);
            protocolList << protocolStr.arg(*it, "DLNA.ORG_PN=AVC_TS_EU_ISO;" + flags);
            protocolList << protocolStr.arg(*it, "DLNA.ORG_PN=MPEG_TS_SD_EU_ISO;" + flags);
        }
        else if (*it == "audio/mpeg")
        {
            protocolList << protocolStr.arg(*it, "DLNA.ORG_PN=MP3;" + flags); // Technically we don't actually serve these
            protocolList << protocolStr.arg(*it, "DLNA.ORG_PN=MP3X;" + flags);
        }
        else if (*it == "audio/mp4")
        {
            protocolList << protocolStr.arg(*it, "DLNA.ORG_PN=AAC_ISO_320;" + flags);
        }
        else if (*it == "audio/vnd.dolby.dd-raw")
        {
            protocolList << protocolStr.arg(*it, "DLNA.ORG_PN=AC3;" + flags);
        }
        else if (*it == "audio/x-ms-wma")
        {
            protocolList << protocolStr.arg(*it, "DLNA.ORG_PN=WMAFULL;" + flags);
        }
        else
            protocolList << protocolStr.arg(*it, "*");
    }

    return protocolList;
}

/**
 * \brief Return a QStringList containing the supported Sink Protocols
 *
 * TODO Extend this to dynamically list all supported protocols (e.g. RTSP)
 *      and any DLNA profile stuff that we can figure out
 */
QStringList GetSinkProtocolInfos()
{
    QStringList mimeTypes = HTTPRequest::GetSupportedMimeTypes();

    QStringList protocolList;
    QStringList::Iterator it;
    for (it = mimeTypes.begin(); it < mimeTypes.end(); ++it)
    {
        protocolList << QString("http-get:*:%1:*").arg(*it);
    }

    return protocolList;
}

