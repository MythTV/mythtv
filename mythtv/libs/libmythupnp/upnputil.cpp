//////////////////////////////////////////////////////////////////////////////
// Program Name: upnputil.cpp
// Created     : Jan. 15, 2007
//
// Purpose     : Global Helper Methods...
//                                                                            
// Copyright (c) 2007 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
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
#include "upnputil.h"
#include "upnp.h"
#include "compat.h"
#include "mythconfig.h" // for HAVE_GETIFADDRS
#include "mythlogging.h"
#include "httprequest.h"

// POSIX headers 2, needs to be after compat.h for OS X
#ifndef _WIN32
#include <sys/ioctl.h>
#endif // _WIN32


#include "zlib.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString LookupUDN( const QString &sDeviceType )
{
    QStringList sList = sDeviceType.split(':', QString::SkipEmptyParts);
    QString     sLoc  = "LookupUDN(" + sDeviceType + ')';

    if (sList.size() <= 2) 
    { 
        LOG(VB_GENERAL, LOG_ERR, sLoc + "- bad device type '" +
                                 sDeviceType + "', not enough tokens"); 
        return QString();
    }

    sList.removeLast();
    QString sName = "UPnP/UDN/" + sList.last();
    QString sUDN  = UPnp::GetConfiguration()->GetValue( sName, "" );

    LOG(VB_UPNP, LOG_INFO, sLoc + " sName=" + sName + ", sUDN=" + sUDN);

    // Generate new UUID if current is missing or broken
    if (sUDN.isEmpty() || sUDN.startsWith("{"))
    {
        sUDN = QUuid::createUuid().toString();
        // QUuid returns the uuid enclosed with braces {} which is not
        // DLNA compliant, we need to remove them
        sUDN = sUDN.mid(1, 36);

        Configuration *pConfig = UPnp::GetConfiguration();

        pConfig->SetValue( sName, sUDN );
        pConfig->Save();
    }

    return( sUDN );
}

/////////////////////////////////////////////////////////////////////////////
//           
/////////////////////////////////////////////////////////////////////////////

bool operator< ( TaskTime t1, TaskTime t2 )
{
    if ( (t1.tv_sec  < t2.tv_sec) || 
        ((t1.tv_sec == t2.tv_sec) && (t1.tv_usec < t2.tv_usec)))
    {
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//           
/////////////////////////////////////////////////////////////////////////////

bool operator== ( TaskTime t1, TaskTime t2 )
{
    if ((t1.tv_sec == t2.tv_sec) && (t1.tv_usec == t2.tv_usec))
        return true;

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//           
/////////////////////////////////////////////////////////////////////////////

void AddMicroSecToTaskTime( TaskTime &t, suseconds_t uSecs )
{
    uSecs += t.tv_usec;

    t.tv_sec  += (uSecs / 1000000);
    t.tv_usec  = (uSecs % 1000000);
}

/////////////////////////////////////////////////////////////////////////////
//           
/////////////////////////////////////////////////////////////////////////////

void AddSecondsToTaskTime( TaskTime &t, long nSecs )
{
    t.tv_sec  += nSecs;
}

/////////////////////////////////////////////////////////////////////////////
//           
/////////////////////////////////////////////////////////////////////////////

QByteArray gzipCompress( const QByteArray &data )
{
    if (data.length() == 0)
        return QByteArray();
    
    static const int CHUNK_SIZE = 1024;
    char out[ CHUNK_SIZE ];

    // allocate inflate state 
    z_stream strm;

    strm.zalloc   = Z_NULL;
    strm.zfree    = Z_NULL;
    strm.opaque   = Z_NULL;
    strm.avail_in = data.length();
    strm.next_in  = (Bytef*)(data.data());

    int ret = deflateInit2( &strm,
                            Z_DEFAULT_COMPRESSION,
                            Z_DEFLATED,
                            15 + 16,
                            8,
                            Z_DEFAULT_STRATEGY ); // gzip encoding
    if (ret != Z_OK)
        return QByteArray();

    QByteArray result;

    // run deflate()
    do 
    {
        strm.avail_out = CHUNK_SIZE;
        strm.next_out  = (Bytef*)(out);

        ret = deflate(&strm, Z_FINISH);

        Q_ASSERT(ret != Z_STREAM_ERROR);  // state not clobbered

        switch (ret) 
        {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     // and fall through
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)deflateEnd(&strm);
                return QByteArray();
        }

        result.append( out, CHUNK_SIZE - strm.avail_out );
    } 
    while (strm.avail_out == 0);

    // clean up and return

    deflateEnd(&strm);

    return result;
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

    QStringList protocolList;
    QStringList::Iterator it;
    for (it = mimeTypes.begin(); it < mimeTypes.end(); ++it)
    {
        protocolList << QString("http-get:*:%1:*").arg(*it);
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

