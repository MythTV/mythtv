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

// MythTV headers
#include "upnputil.h"
#include "upnp.h"
#include "compat.h"
#include "mythconfig.h" // for HAVE_GETIFADDRS
#include "mythlogging.h"

// POSIX headers 2, needs to be after compat.h for OS X
#ifndef _WIN32
#include <net/if.h>
#include <sys/ioctl.h>
#endif // _WIN32
#if HAVE_GETIFADDRS
#include <ifaddrs.h>
#endif

#include "zlib.h"

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString LookupUDN( QString sDeviceType )
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

#if HAVE_GETIFADDRS

long GetIPAddressList(QStringList &sStrList)
{
    QString LOC = "GetIPAddressList() - ";

    struct ifaddrs *list, *ifa;


    sStrList.clear();

    if (getifaddrs(&list) == -1)
    {
        LOG(VB_UPNP, LOG_ERR, LOC + "getifaddrs failed: " + ENO);
        return 0;
    }

    for (ifa=list; ifa; ifa=ifa->ifa_next)
    {
        if (!ifa->ifa_addr)
            continue;
        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if (ifa->ifa_flags & IFF_LOOPBACK)
            continue;
        if (!(ifa->ifa_flags & IFF_UP))
            continue;


        char  address[16];

        if (inet_ntop(ifa->ifa_addr->sa_family,
                      &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr,
                      address, sizeof(address)) == NULL)
        {
            LOG(VB_UPNP, LOG_ERR, LOC + "inet_ntop failed: " + ENO);
            continue;
        }

        sStrList.append(address);
        LOG(VB_UPNP, LOG_DEBUG, LOC + QString("Added %1 as %2")
                .arg(ifa->ifa_name).arg(address));
    }

    freeifaddrs(list);

    return(sStrList.count());
}

#else // HAVE_GETIFADDRS

// On some Unixes (e.g. Darwin), this implementation is buggy because
// struct ifreq is variable size. Luckily, most of them have getifaddrs()

long GetIPAddressList( QStringList &sStrList )
{
#ifdef _WIN32
    LOG(VB_UPNP, LOG_NOTICE, "GetIPAddressList() not implemented in MinGW");
    return 0;
#else
    sStrList.clear();

    MSocketDevice socket( MSocketDevice::Datagram );

    struct ifreq  ifReqs[ 16 ];
    struct ifreq  ifReq;
    struct ifconf ifConf;

    // ----------------------------------------------------------------------
    // Get Configuration information...
    // ----------------------------------------------------------------------

    ifConf.ifc_len           = sizeof( struct ifreq ) * sizeof( ifReqs );
    ifConf.ifc_ifcu.ifcu_req = ifReqs;

    if ( ioctl( socket.socket(), SIOCGIFCONF, &ifConf ) < 0)
        return( 0 );

    long nCount = ifConf.ifc_len / sizeof( struct ifreq );

    // ----------------------------------------------------------------------
    // Loop through looking for IP addresses.
    // ----------------------------------------------------------------------

    for (long nIdx = 0; nIdx < nCount; nIdx++ )
    {
        // ------------------------------------------------------------------
        // Is this an interface we want?
        // ------------------------------------------------------------------

        strcpy ( ifReq.ifr_name, ifReqs[ nIdx ].ifr_name );

        if (ioctl ( socket.socket(), SIOCGIFFLAGS, &ifReq ) < 0)
            continue;

        // ------------------------------------------------------------------
        // Skip loopback and down interfaces
        // ------------------------------------------------------------------

        if ((ifReq.ifr_flags & IFF_LOOPBACK) || (!(ifReq.ifr_flags & IFF_UP)))
            continue;

        if ( ifReqs[ nIdx ].ifr_addr.sa_family == AF_INET)
        {
            struct sockaddr_in addr;

            // --------------------------------------------------------------
            // Get a pointer to the address
            // --------------------------------------------------------------

            memcpy (&addr, &(ifReqs[ nIdx ].ifr_addr), sizeof( ifReqs[ nIdx ].ifr_addr ));

            if (addr.sin_addr.s_addr != htonl( INADDR_LOOPBACK ))
            {
                QHostAddress address( htonl( addr.sin_addr.s_addr ));

                sStrList.append( address.toString() ); 
            }
        }
    }

    return( sStrList.count() );
#endif // !_WIN32
}

#endif // HAVE_GETIFADDRS

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
