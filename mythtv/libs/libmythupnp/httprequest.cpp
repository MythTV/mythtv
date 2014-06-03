//////////////////////////////////////////////////////////////////////////////
// Program Name: httprequest.cpp
// Created     : Oct. 21, 2005
//
// Purpose     : Http Request/Response
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#include "httprequest.h"

#include <QFile>
#include <QFileInfo>
#include <QTextCodec>
#include <QStringList>
#include <QCryptographicHash>
#include <QDateTime>

#include "mythconfig.h"
#if !( CONFIG_DARWIN || CONFIG_CYGWIN || defined(__FreeBSD__) || defined(_WIN32))
#define USE_SETSOCKOPT
#include <sys/sendfile.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <cerrno>
// FOR DEBUGGING
#include <iostream>

#ifndef _WIN32
#include <netinet/tcp.h>
#endif

#include "upnp.h"

#include "compat.h"
#include "mythlogging.h"
#include "mythversion.h"
#include "mythdate.h"
#include <mythcorecontext.h>

#include "serializers/xmlSerializer.h"
#include "serializers/soapSerializer.h"
#include "serializers/jsonSerializer.h"
#include "serializers/xmlplistSerializer.h"

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

static MIMETypes g_MIMETypes[] =
{
    // Image Mime Types
    { "gif" , "image/gif"                  },
    { "ico" , "image/x-icon"               },
    { "jpeg", "image/jpeg"                 },
    { "jpg" , "image/jpeg"                 },
    { "mng" , "image/x-mng"                },
    { "png" , "image/png"                  },
    { "svg" , "image/svg+xml"              },
    { "svgz", "image/svg+xml"              },
    { "tif" , "image/tiff"                 },
    { "tiff", "image/tiff"                 },
    // Text Mime Types
    { "htm" , "text/html"                  },
    { "html", "text/html"                  },
    { "qsp" , "text/html"                  },
    { "txt" , "text/plain"                 },
    { "xml" , "text/xml"                   },
    { "qxml", "text/xml"                   },
    { "xslt", "text/xml"                   },
    { "css" , "text/css"                   },
    // Application Mime Types
    { "doc" , "application/vnd.ms-word"    },
    { "gz"  , "application/x-tar"          },
    { "js"  , "application/javascript"     },
    { "m3u8", "application/x-mpegurl"      }, // HTTP Live Streaming
    { "ogx" , "application/ogg"            }, // http://wiki.xiph.org/index.php/MIME_Types_and_File_Extensions
    { "pdf" , "application/pdf"            },
    { "qjs" , "application/javascript"     },
    { "rm"  , "application/vnd.rn-realmedia" },
    { "swf" , "application/x-shockwave-flash" },
    { "xls" , "application/vnd.ms-excel"   },
    { "zip" , "application/x-tar"          },
    // Audio Mime Types:
    { "aac" , "audio/mp4"                  },
    { "ac3" , "audio/vnd.dolby.dd-raw"     }, // DLNA?
    { "flac", "audio/x-flac"               }, // This could be audio/flac or application/flac
    { "m4a" , "audio/x-m4a"                },
    { "mid" , "audio/midi"                 },
    { "mka" , "audio/x-matroska"           },
    { "mp3" , "audio/mpeg"                 },
    { "oga" , "audio/ogg"                  }, // Defined: http://wiki.xiph.org/index.php/MIME_Types_and_File_Extensions
    { "ogg" , "audio/ogg"                  }, // Defined: http://wiki.xiph.org/index.php/MIME_Types_and_File_Extensions
    { "wav" , "audio/wav"                  },
    { "wma" , "audio/x-ms-wma"             },
    // Video Mime Types
    { "3gp" , "video/3gpp"                 }, // Also audio/3gpp
    { "3g2" , "video/3gpp2"                }, // Also audio/3gpp2
    { "asf" , "video/x-ms-asf"             },
    { "avi" , "video/avi"                  },
    { "m4v" , "video/mp4"                  },
    { "mpeg", "video/mpeg"                 },
    { "mpeg2","video/mpeg"                 },
    { "mpg" , "video/mpeg"                 },
    { "mpg2", "video/mpeg"                 },
    { "mov" , "video/quicktime"            },
    { "mp4" , "video/mp4"                  },
    { "mkv" , "video/x-matroska"           }, // See http://matroska.org/technical/specs/notes.html#MIME (See NOTE 1)
    { "nuv" , "video/nupplevideo"          },
    { "ogv" , "video/ogg"                  }, // Defined: http://wiki.xiph.org/index.php/MIME_Types_and_File_Extensions
    { "ts"  , "video/mp2t"                 }, // HTTP Live Streaming
    { "vob" , "video/mpeg"                 },
    { "wmv" , "video/x-ms-wmv"             }
};

// NOTE 1
// This formerly was video/x-matroska, but got changed due to #8643
// This was reverted from video/x-mkv, due to #10980
// See http://matroska.org/technical/specs/notes.html#MIME
// If you can't please everyone, may as well be correct as you piss some off

static const char *Static400Error =
    "<!DOCTYPE html>"
    "<HTML>"
      "<HEAD>"
        "<TITLE>Error 400</TITLE>"
        "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=ISO-8859-1\">"
      "</HEAD>"
      "<BODY><H1>400 Bad Request.</H1></BODY>"
    "</HTML>";

static const char *Static401Error =
    "<!DOCTYPE html>"
    "<HTML>"
      "<HEAD>"
        "<TITLE>Error 401</TITLE>"
        "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=ISO-8859-1\">"
      "</HEAD>"
      "<BODY><H1>401 Unauthorized.</H1></BODY>"
    "</HTML>";

static const char *Static505Error =
    "<!DOCTYPE html>"
    "<HTML>"
      "<HEAD>"
        "<TITLE>Error 505</TITLE>"
        "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=ISO-8859-1\">"
      "</HEAD>"
      "<BODY><H1>505 HTTP Version Not Supported.</H1></BODY>"
    "</HTML>";

static const int g_nMIMELength = sizeof( g_MIMETypes) / sizeof( MIMETypes );
static const int g_on          = 1;
static const int g_off         = 0;

const char *HTTPRequest::m_szServerHeaders = "Accept-Ranges: bytes\r\n";

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HTTPRequest::HTTPRequest() : m_procReqLineExp ( "[ \r\n][ \r\n]*"  ),
                             m_parseRangeExp  ( "(\\d|\\-)"        ),
                             m_eType          ( RequestTypeUnknown ),
                             m_eContentType   ( ContentType_Unknown),
                             m_nMajor         (   0 ),
                             m_nMinor         (   0 ),
                             m_bProtected     ( false ),
                             m_bSOAPRequest   ( false ),
                             m_eResponseType  ( ResponseTypeUnknown),
                             m_nResponseStatus( 200 ),
                             m_pPostProcess   ( NULL )
{
    m_response.open( QIODevice::ReadWrite );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

RequestType HTTPRequest::SetRequestType( const QString &sType )
{
    if (sType == "GET"        ) return( m_eType = RequestTypeGet         );
    if (sType == "HEAD"       ) return( m_eType = RequestTypeHead        );
    if (sType == "POST"       ) return( m_eType = RequestTypePost        );
    if (sType == "M-SEARCH"   ) return( m_eType = RequestTypeMSearch     );

    if (sType == "SUBSCRIBE"  ) return( m_eType = RequestTypeSubscribe   );
    if (sType == "UNSUBSCRIBE") return( m_eType = RequestTypeUnsubscribe );
    if (sType == "NOTIFY"     ) return( m_eType = RequestTypeNotify      );

    if (sType.startsWith( QString("HTTP/") )) return( m_eType = RequestTypeResponse );

    LOG(VB_UPNP, LOG_INFO,
        QString("HTTPRequest::SentRequestType( %1 ) - returning Unknown.")
            .arg(sType));

    return( m_eType = RequestTypeUnknown);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::BuildHeader( long long nSize )
{
    QString sHeader;
    QString sContentType = (m_eResponseType == ResponseTypeOther) ?
                            m_sResponseTypeText : GetResponseType();

    sHeader = QString( "%1 %2\r\n"
                       "Date: %3\r\n"
                       "Server: %4\r\n" )
        .arg(GetResponseProtocol()).arg(GetResponseStatus())
        .arg(MythDate::current().toString("d MMM yyyy hh:mm:ss"))
        .arg(HttpServer::GetServerVersion());

    sHeader += GetAdditionalHeaders();

    sHeader += QString( "Connection: %1\r\n"
                        "Content-Type: %2\r\n"
                        "Content-Length: %3\r\n" )
                        .arg( GetKeepAlive() ? "Keep-Alive" : "Close" )
                        .arg( sContentType )
                        .arg( nSize );

    // ----------------------------------------------------------------------
    // Temp Hack to process DLNA header
                             
    QString sValue = GetHeaderValue( "getcontentfeatures.dlna.org", "0" );

    if (sValue == "1")
        sHeader += "contentFeatures.dlna.org: DLNA.ORG_OP=01;DLNA.ORG_CI=0;"
                   "DLNA.ORG_FLAGS=01500000000000000000000000000000\r\n";

    // ----------------------------------------------------------------------

    sHeader += "\r\n";

    return sHeader;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

long HTTPRequest::SendResponse( void )
{
    long      nBytes    = 0;

    switch( m_eResponseType )
    {
        // The following are all eligable for gzip compression
        case ResponseTypeUnknown:
        case ResponseTypeNone:
            LOG(VB_UPNP, LOG_INFO,
                QString("HTTPRequest::SendResponse( None ) :%1 -> %2:")
                    .arg(GetResponseStatus()) .arg(GetPeerAddress()));
            return( -1 );
        case ResponseTypeJS:
        case ResponseTypeCSS:
        case ResponseTypeText:
        case ResponseTypeSVG:
        case ResponseTypeXML:
        case ResponseTypeHTML:
            // If the reponse isn't already in the buffer, then load it
            if (!m_sFileName.isEmpty() &&
                m_response.buffer().isEmpty())
            {
                QByteArray fileBuffer;
                QFile file(m_sFileName);
                if (file.exists() && file.size() < (2 * 1024 * 1024) && // For security/stability, limit size of files read into buffer to 2MiB
                    file.open(QIODevice::ReadOnly | QIODevice::Text))
                    m_response.buffer() = file.readAll();

                if (!m_response.buffer().isEmpty())
                    break;

                // Let SendResponseFile try or send a 404
                m_eResponseType = ResponseTypeFile;
            }
            else
                break;
        case ResponseTypeFile: // Binary files
            LOG(VB_UPNP, LOG_INFO,
                QString("HTTPRequest::SendResponse( File ) :%1 -> %2:")
                    .arg(GetResponseStatus()) .arg(GetPeerAddress()));
            return( SendResponseFile( m_sFileName ));
        case ResponseTypeOther:
        default:
            break;
    }

    LOG(VB_UPNP, LOG_INFO,
        QString("HTTPRequest::SendResponse(xml/html) (%1) :%2 -> %3: %4")
             .arg(m_sFileName) .arg(GetResponseStatus())
             .arg(GetPeerAddress()) .arg(m_eResponseType));

    // ----------------------------------------------------------------------
    // Make it so the header is sent with the data
    // ----------------------------------------------------------------------

#ifdef USE_SETSOCKOPT
    // Never send out partially complete segments
    if (setsockopt(getSocketHandle(), SOL_TCP, TCP_CORK, 
                   &g_on, sizeof( g_on )) < 0)
    {
        LOG(VB_UPNP, LOG_INFO,
            QString("HTTPRequest::SendResponse(xml/html) "
                    "setsockopt error setting TCP_CORK on ") + ENO);
    }
#endif

    // ----------------------------------------------------------------------
    // Check for ETag match...
    // ----------------------------------------------------------------------

    QString sETag = GetHeaderValue( "If-None-Match", "" );

    if ( !sETag.isEmpty() && sETag == m_mapRespHeaders[ "ETag" ] )
    {
        LOG(VB_UPNP, LOG_INFO,
            QString("HTTPRequest::SendResponse(%1) - Cached")
                .arg(sETag));

        m_nResponseStatus = 304;

        // no content can be returned.
        m_response.buffer().clear();
    }

    // ----------------------------------------------------------------------

    int nContentLen = m_response.buffer().length();

    QBuffer *pBuffer = &m_response;

    // ----------------------------------------------------------------------
    // DEBUGGING
    if (getenv("HTTPREQUEST_DEBUG"))
        cout << m_response.buffer().constData() << endl;
    // ----------------------------------------------------------------------

    LOG(VB_UPNP, LOG_DEBUG, QString("Reponse Content Length: %1").arg(nContentLen));

    // ----------------------------------------------------------------------
    // Should we try to return data gzip'd?
    // ----------------------------------------------------------------------

    QBuffer compBuffer;

    if (( nContentLen > 0 ) && m_mapHeaders[ "accept-encoding" ].contains( "gzip" ))
    {
        QByteArray compressed = gzipCompress( m_response.buffer() );
        compBuffer.setData( compressed );

        if (!compBuffer.buffer().isEmpty())
        {
            pBuffer = &compBuffer;

            m_mapRespHeaders[ "Content-Encoding" ] = "gzip";
            LOG(VB_UPNP, LOG_DEBUG, QString("Reponse Compressed Content Length: %1").arg(compBuffer.buffer().length()));
        }
    }

    // ----------------------------------------------------------------------
    // NOTE: Access-Control-Allow-Origin Wildcard
    //
    // This is a REALLY bad idea, so bad in fact that I'm including it here but
    // commented out in the hope that anyone thinking of adding it in the future
    // will see it and then read this comment.
    //
    // Browsers do not verify that the origin is on the same network. This means
    // that a malicious script embedded or included into ANY webpage you visit
    // could then access servers on your local network including MythTV. They
    // can grab data, delete data including recordings and videos, schedule
    // recordings and generally ruin your day.
    //
    // This might seem paranoid and a remote possibility, but then that's how
    // a lot of exploits are born. Do NOT allow wildcards.
    //
    //m_mapRespHeaders[ "Access-Control-Allow-Origin" ] = "*";
    // ----------------------------------------------------------------------

    // ----------------------------------------------------------------------
    // Allow the WebFrontend on the Master backend and ONLY this machine
    // to access resources on a frontend or slave web server
    //
    // http://www.w3.org/TR/cors/#introduction
    // ----------------------------------------------------------------------
    QString masterAddrPort = QString("%1:%2").arg(gCoreContext->GetMasterServerIP())
                                            .arg(gCoreContext->GetMasterServerStatusPort());

    QStringList allowedOrigins;
    allowedOrigins << QString("http://%1").arg(masterAddrPort);
    allowedOrigins << QString("https://%2").arg(masterAddrPort);

    if (!m_mapHeaders[ "origin" ].isEmpty())
    {
        if (allowedOrigins.contains(m_mapHeaders[ "origin" ]))
            m_mapRespHeaders[ "Access-Control-Allow-Origin" ] = m_mapHeaders[ "origin" ];
        else
            LOG(VB_GENERAL, LOG_CRIT, QString("HTTPRequest: Cross-origin request "
                                              "received with origin (%1)")
                                                 .arg(m_mapHeaders[ "origin" ]));
    }

    // ----------------------------------------------------------------------
    // Force IE into 'standards' mode
    // ----------------------------------------------------------------------
    m_mapRespHeaders[ "X-UA-Compatible" ] = "IE=Edge";

    // ----------------------------------------------------------------------
    // Write out Header.
    // ----------------------------------------------------------------------

    nContentLen = pBuffer->buffer().length();

    QString    rHeader = BuildHeader( nContentLen );

    QByteArray sHeader = rHeader.toUtf8();
    nBytes  = WriteBlockDirect( sHeader.constData(), sHeader.length() );

    // ----------------------------------------------------------------------
    // Write out Response buffer.
    // ----------------------------------------------------------------------

    if (( m_eType != RequestTypeHead ) && ( nContentLen > 0 ))
    {
        nBytes += SendData( pBuffer, 0, nContentLen );
    }

    // ----------------------------------------------------------------------
    // Turn off the option so any small remaining packets will be sent
    // ----------------------------------------------------------------------

#ifdef USE_SETSOCKOPT
    if (setsockopt(getSocketHandle(), SOL_TCP, TCP_CORK, 
                   &g_off, sizeof( g_off )) < 0)
    {
        LOG(VB_UPNP, LOG_INFO,
            QString("HTTPRequest::SendResponse(xml/html) "
                    "setsockopt error setting TCP_CORK off ") + ENO);
    }
#endif

    return( nBytes );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

long HTTPRequest::SendResponseFile( QString sFileName )
{
    long        nBytes  = 0;
    long long   llSize  = 0;
    long long   llStart = 0;
    long long   llEnd   = 0;

    LOG(VB_UPNP, LOG_INFO, QString("SendResponseFile ( %1 )").arg(sFileName));

    m_eResponseType     = ResponseTypeOther;
    m_sResponseTypeText = "text/plain";

#if 0
    // Dump request header
    for ( QStringMap::iterator it  = m_mapHeaders.begin();
                               it != m_mapHeaders.end();
                             ++it )
    {
        LOG(VB_GENERAL, LOG_DEBUG, it.key() + ": " + *it);
    }
#endif

    // ----------------------------------------------------------------------
    // Make it so the header is sent with the data
    // ----------------------------------------------------------------------

#ifdef USE_SETSOCKOPT
    // Never send out partially complete segments
    if (setsockopt(getSocketHandle(), SOL_TCP, TCP_CORK, 
                   &g_on, sizeof( g_on )) < 0)
    {
        LOG(VB_UPNP, LOG_INFO,
            QString("HTTPRequest::SendResponseFile(%1) "
                    "setsockopt error setting TCP_CORK on " ).arg(sFileName) +
            ENO);
    }
#endif

    QFile tmpFile( sFileName );
    if (tmpFile.exists( ) && tmpFile.open( QIODevice::ReadOnly ))
    {

        m_sResponseTypeText = TestMimeType( sFileName );

        // ------------------------------------------------------------------
        // Get File size
        // ------------------------------------------------------------------

        llSize = llEnd = tmpFile.size( );

        m_nResponseStatus = 200;

        // ------------------------------------------------------------------
        // Process any Range Header
        // ------------------------------------------------------------------

        bool    bRange = false;
        QString sRange = GetHeaderValue( "range", "" );

        if (!sRange.isEmpty())
        {
            bRange = ParseRange( sRange, llSize, &llStart, &llEnd );

            // Adjust ranges that are too long.  

            if (llEnd >= llSize) 
                llEnd = llSize-1; 

            if ((llSize > llStart) && (llSize > llEnd) && (llEnd > llStart))
            {
                if (bRange)
                {
                    m_nResponseStatus = 206;
                    m_mapRespHeaders[ "Content-Range" ] = QString("bytes %1-%2/%3")
                                                              .arg( llStart )
                                                              .arg( llEnd   )
                                                              .arg( llSize  );
                    llSize = (llEnd - llStart) + 1;
                }
            }
            else
            {
                m_nResponseStatus = 416;
                llSize = 0;
                LOG(VB_UPNP, LOG_INFO,
                    QString("HTTPRequest::SendResponseFile(%1) - "
                            "invalid byte range %2-%3/%4")
                            .arg(sFileName) .arg(llStart) .arg(llEnd)
                            .arg(llSize));
            }
        }

        // DSM-?20 specific response headers
        if (bRange == false)
            m_mapRespHeaders[ "User-Agent"    ] = "redsonic";

        // ------------------------------------------------------------------
        //
        // ------------------------------------------------------------------

    }
    else
    {
        LOG(VB_UPNP, LOG_INFO,
            QString("HTTPRequest::SendResponseFile(%1) - cannot find file!")
                .arg(sFileName));
        m_nResponseStatus = 404;
    }

    // -=>TODO: Should set "Content-Length: *" if file is still recording

    // ----------------------------------------------------------------------
    // Write out Header.
    // ----------------------------------------------------------------------

    QString    rHeader = BuildHeader( llSize );
    QByteArray sHeader = rHeader.toUtf8();
    nBytes = WriteBlockDirect( sHeader.constData(), sHeader.length() );

    // ----------------------------------------------------------------------
    // Write out File.
    // ----------------------------------------------------------------------

#if 0
    LOG(VB_UPNP, LOG_DEBUG,
        QString("SendResponseFile : size = %1, start = %2, end = %3")
            .arg(llSize).arg(llStart).arg(llEnd));
#endif
    if (( m_eType != RequestTypeHead ) && (llSize != 0))
    {
        long long sent = SendFile( tmpFile, llStart, llSize );

        if (sent == -1)
        {
            LOG(VB_UPNP, LOG_INFO,
                QString("SendResponseFile( %1 ) Error: %2 [%3]" )
                    .arg(sFileName) .arg(errno) .arg(strerror(errno)));

            nBytes = -1;
        }
    }

    // ----------------------------------------------------------------------
    // Turn off the option so any small remaining packets will be sent
    // ----------------------------------------------------------------------

#ifdef USE_SETSOCKOPT
    if (setsockopt(getSocketHandle(), SOL_TCP, TCP_CORK,
                   &g_off, sizeof( g_off )) < 0)
    {
        LOG(VB_UPNP, LOG_INFO,
            QString("HTTPRequest::SendResponseFile(%1) "
                    "setsockopt error setting TCP_CORK off ").arg(sFileName) +
            ENO);
    }
#endif

    // -=>TODO: Only returns header length...
    //          should we change to return total bytes?

    return nBytes;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

#define SENDFILE_BUFFER_SIZE 65536

qint64 HTTPRequest::SendData( QIODevice *pDevice, qint64 llStart, qint64 llBytes )
{
    bool   bShouldClose = false;
    qint64 sent = 0;

    if (!pDevice->isOpen())
    {
        pDevice->open( QIODevice::ReadOnly );
        bShouldClose = true;
    }

    // ----------------------------------------------------------------------
    // Set out file position to requested start location.
    // ----------------------------------------------------------------------

    if ( pDevice->seek( llStart ) == false)
        return -1;

    char   aBuffer[ SENDFILE_BUFFER_SIZE ];

    qint64 llBytesRemaining = llBytes;
    qint64 llBytesToRead    = 0;
    qint64 llBytesRead      = 0;
    qint64 llBytesWritten   = 0;

    while ((sent < llBytes) && !pDevice->atEnd())
    {
        llBytesToRead  = std::min( (qint64)SENDFILE_BUFFER_SIZE, llBytesRemaining );
        
        if (( llBytesRead = pDevice->read( aBuffer, llBytesToRead )) != -1 )
        {
            if (( llBytesWritten = WriteBlockDirect( aBuffer, llBytesRead )) == -1)
                return -1;

            // -=>TODO: We don't handle the situation where we read more than was sent.

            sent             += llBytesRead;
            llBytesRemaining -= llBytesRead;
        }
    }

    if (bShouldClose)
        pDevice->close();

    return sent;
}

/////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////

qint64 HTTPRequest::SendFile( QFile &file, qint64 llStart, qint64 llBytes )
{
    qint64 sent = 0;

#ifndef __linux__
    sent = SendData( (QIODevice *)(&file), llStart, llBytes );
#else
    __off64_t  offset = llStart; 
    int        fd     = file.handle( ); 
  
    if ( fd == -1 ) 
    { 
        LOG(VB_UPNP, LOG_INFO,
            QString("SendResponseFile( %1 ) Error: %2 [%3]") 
                .arg(file.fileName()) .arg(file.error()) 
                .arg(strerror(file.error()))); 
        sent = -1; 
    } 
    else 
    { 
        qint64     llSent = 0;

        do 
        { 
            // SSIZE_MAX should work in kernels 2.6.16 and later. 
            // The loop is needed in any case. 
  
            sent = sendfile64(getSocketHandle(), fd, &offset, 
                              (size_t)MIN(llBytes, INT_MAX)); 
  
            if (sent >= 0)
            {
                llBytes -= sent; 
                llSent  += sent;
                LOG(VB_UPNP, LOG_INFO,
                    QString("SendResponseFile : --- size = %1, "
                            "offset = %2, sent = %3") 
                        .arg(llBytes).arg(offset).arg(sent)); 
            }
        } 
        while (( sent >= 0 ) && ( llBytes > 0 )); 

        sent = llSent;
    } 
#endif

    return( sent );
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HTTPRequest::FormatErrorResponse( bool  bServerError,
                                       const QString &sFaultString,
                                       const QString &sDetails )
{
    m_eResponseType   = ResponseTypeXML;
    m_nResponseStatus = 500;

    QTextStream stream( &m_response );

    stream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>";

    QString sWhere = ( bServerError ) ? "s:Server" : "s:Client";

    if (m_bSOAPRequest)
    {
        m_mapRespHeaders[ "EXT" ] = "";

        stream << SOAP_ENVELOPE_BEGIN
               << "<s:Fault>"
               << "<faultcode>"   << sWhere       << "</faultcode>"
               << "<faultstring>" << sFaultString << "</faultstring>";
    }

    if (!sDetails.isEmpty())
    {
        stream << "<detail>" << sDetails << "</detail>";
    }

    if (m_bSOAPRequest)
        stream << "</s:Fault>" << SOAP_ENVELOPE_END;

    stream.flush();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HTTPRequest::FormatActionResponse( Serializer *pSer )
{
    m_eResponseType     = ResponseTypeOther;
    m_sResponseTypeText = pSer->GetContentType();
    m_nResponseStatus   = 200;

    pSer->AddHeaders( m_mapRespHeaders );

    //m_response << pFormatter->ToString();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HTTPRequest::FormatActionResponse(const NameValues &args)
{
    m_eResponseType   = ResponseTypeXML;
    m_nResponseStatus = 200;

    QTextStream stream( &m_response );

    stream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n";

    if (m_bSOAPRequest)
    {
        m_mapRespHeaders[ "EXT" ] = "";

        stream << SOAP_ENVELOPE_BEGIN
               << "<u:" << m_sMethod << "Response xmlns:u=\""
               << m_sNameSpace << "\">\r\n";
    }
    else
        stream << "<" << m_sMethod << "Response>\r\n";

    NameValues::const_iterator nit = args.begin();
    for (; nit != args.end(); ++nit)
    {
        stream << "<" << (*nit).sName;

        if ((*nit).pAttributes)
        {
            NameValues::const_iterator nit2 = (*nit).pAttributes->begin();
            for (; nit2 != (*nit).pAttributes->end(); ++nit2)
            {
                stream << " " << (*nit2).sName << "='"
                       << Encode( (*nit2).sValue ) << "'";
            }
        }

        stream << ">";

        if (m_bSOAPRequest)
            stream << Encode( (*nit).sValue );
        else
            stream << (*nit).sValue;

        stream << "</" << (*nit).sName << ">\r\n";
    }

    if (m_bSOAPRequest)
    {
        stream << "</u:" << m_sMethod << "Response>\r\n"
                   << SOAP_ENVELOPE_END;
    }
    else
        stream << "</" << m_sMethod << "Response>\r\n";

    stream.flush();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HTTPRequest::FormatRawResponse(const QString &sXML)
{
    m_eResponseType   = ResponseTypeXML;
    m_nResponseStatus = 200;

    QTextStream stream( &m_response );

    stream << sXML;

    stream.flush();
}
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HTTPRequest::FormatFileResponse( const QString &sFileName )
{
    m_sFileName = sFileName;

    if (QFile::exists( m_sFileName ))
    {

        if (m_eResponseType == ResponseTypeUnknown)
            m_eResponseType               = ResponseTypeFile;
        m_nResponseStatus                 = 200;
        m_mapRespHeaders["Cache-Control"] = "no-cache=\"Ext\", max-age = 5000";
    }
    else
    {
        m_eResponseType   = ResponseTypeHTML;
        m_nResponseStatus = 404;
        LOG(VB_UPNP, LOG_INFO,
            QString("HTTPRequest::FormatFileResponse(%1) - cannot find file")
                .arg(sFileName));
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HTTPRequest::SetRequestProtocol( const QString &sLine )
{
    m_sProtocol      = sLine.section( '/', 0, 0 ).trimmed();
    QString sVersion = sLine.section( '/', 1    ).trimmed();

    m_nMajor = sVersion.section( '.', 0, 0 ).toInt();
    m_nMinor = sVersion.section( '.', 1    ).toInt();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::GetRequestProtocol() const
{
    return QString("%1/%2.%3").arg(m_sProtocol)
                              .arg(QString::number(m_nMajor))
                              .arg(QString::number(m_nMinor));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::GetResponseProtocol() const
{
    // RFC 2145
    //
    // An HTTP server SHOULD send a response version equal to the highest
    // version for which the server is at least conditionally compliant, and
    // whose major version is less than or equal to the one received in the
    // request.

//     if (m_nMajor == 1)
//         QString("HTTP/1.1");
//     else if (m_nMajor == 2)
//         QString("HTTP/2.0");

    return QString("HTTP/1.1");
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

ContentType HTTPRequest::SetContentType( const QString &sType )
{
    if ((sType == "application/x-www-form-urlencoded"          ) ||
        (sType.startsWith("application/x-www-form-urlencoded;")))
        return( m_eContentType = ContentType_Urlencoded );

    if ((sType == "text/xml"                                   ) ||
        (sType.startsWith("text/xml;")                         ))
        return( m_eContentType = ContentType_XML        );

    return( m_eContentType = ContentType_Unknown );
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::GetResponseStatus( void )
{
    switch( m_nResponseStatus )
    {
        case 200:   return( "200 OK"                               );
        case 201:   return( "201 Created"                          );
        case 202:   return( "202 Accepted"                         );
        case 206:   return( "206 Partial Content"                  );
        case 304:   return( "304 Not Modified"                     );
        case 400:   return( "400 Bad Request"                      );
        case 401:   return( "401 Unauthorized"                     );
        case 403:   return( "403 Forbidden"                        );
        case 404:   return( "404 Not Found"                        );
        case 405:   return( "405 Method Not Allowed"               );
        case 406:   return( "406 Not Acceptable"                   );
        case 408:   return( "408 Request Timeout"                  );
        case 412:   return( "412 Precondition Failed"              );
        case 413:   return( "413 Request Entity Too Large"         );
        case 414:   return( "414 Request-URI Too Long"             );
        case 415:   return( "415 Unsupported Media Type"           );
        case 416:   return( "416 Requested Range Not Satisfiable"  );
        case 417:   return( "417 Expectation Failed"               );
        case 500:   return( "500 Internal Server Error"            );
        case 501:   return( "501 Not Implemented"                  );
        case 502:   return( "502 Bad Gateway"                      );
        case 503:   return( "503 Service Unavailable"              );
        case 504:   return( "504 Gateway Timeout"                  );
        case 505:   return( "505 HTTP Version Not Supported"       );
        case 510:   return( "510 Not Extended"                     );
    }

    return( QString( "%1 Unknown" ).arg( m_nResponseStatus ));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::GetResponseType( void )
{
    switch( m_eResponseType )
    {
        case ResponseTypeXML    : return( "text/xml; charset=\"UTF-8\"" );
        case ResponseTypeHTML   : return( "text/html; charset=\"UTF-8\"" );
        case ResponseTypeCSS    : return( "text/css; charset=\"UTF-8\"" );
        case ResponseTypeJS     : return( "application/javascript" );
        case ResponseTypeText   : return( "text/plain; charset=\"UTF-8\"" );
        case ResponseTypeSVG    : return( "image/svg+xml" );
        default: break;
    }

    return( "text/plain" );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::GetMimeType( const QString &sFileExtension )
{
    QString ext;

    for (int i = 0; i < g_nMIMELength; i++)
    {
        ext = g_MIMETypes[i].pszExtension;

        if ( sFileExtension.toUpper() == ext.toUpper() )
            return( g_MIMETypes[i].pszType );
    }

    return( "text/plain" );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::TestMimeType( const QString &sFileName )
{
    QFileInfo info( sFileName );
    QString   sLOC    = "HTTPRequest::TestMimeType(" + sFileName + ") - ";
    QString   sSuffix = info.suffix().toLower();
    QString   sMIME   = GetMimeType( sSuffix );

    if ( sSuffix == "nuv" )     // If a very old recording, might be an MPEG?
    {
        // Read the header to find out:
        QFile file( sFileName );

        if ( file.open(QIODevice::ReadOnly | QIODevice::Text) )
        {
            QByteArray head = file.read(8);
            QString    sHex = head.toHex();

            LOG(VB_UPNP, LOG_DEBUG, sLOC + "file starts with " + sHex);

            if ( sHex == "000001ba44000400" )  // MPEG2 PS
                sMIME = "video/mpeg";

            if ( head == "MythTVVi" )
            {
                file.seek(100);
                head = file.read(4);

                if ( head == "DIVX" )
                {
                    LOG(VB_UPNP, LOG_DEBUG, sLOC + "('MythTVVi...DIVXLAME')");
                    sMIME = "video/mp4";
                }
                // NuppelVideo is "RJPG" at byte 612
                // We could also check the audio (LAME or RAWA),
                // but since most UPnP clients choke on Nuppel, no need
            }

            file.close();
        }
        else
            LOG(VB_GENERAL, LOG_ERR, sLOC + "Could not read file");
    }

    LOG(VB_UPNP, LOG_INFO, sLOC + "type is " + sMIME);
    return sMIME;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

long HTTPRequest::GetParameters( QString sParams, QStringMap &mapParams  )
{
    long nCount = 0;

    LOG(VB_UPNP, LOG_DEBUG, QString("sParams: '%1'").arg(sParams));

    // This looks odd, but it is here to cope with stupid UPnP clients that
    // forget to de-escape the URLs.  We can't map %26 here as well, as that
    // breaks anything that is trying to pass & as part of a name or value.
    sParams.replace( "&amp;", "&" );

    if (!sParams.isEmpty())
    {
        QStringList params = sParams.split('&', QString::SkipEmptyParts);

        for ( QStringList::Iterator it  = params.begin();
                                    it != params.end();  ++it )
        {
            QString sName  = (*it).section( '=', 0, 0 );
            QString sValue = (*it).section( '=', 1 );
            sValue.replace("+"," ");

            if (!sName.isEmpty() &&
                !sValue.isEmpty())
            {
                sName  = QUrl::fromPercentEncoding(sName.toUtf8());
                sValue = QUrl::fromPercentEncoding(sValue.toUtf8());

                // Make Parameter Names all lower case
                mapParams.insert( sName.trimmed().toLower(), sValue );
                nCount++;
            }
        }
    }

    return nCount;
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::GetHeaderValue( const QString &sKey, QString sDefault )
{
    QStringMap::iterator it = m_mapHeaders.find( sKey.toLower() );

    if ( it == m_mapHeaders.end())
        return( sDefault );

    return *it;
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::GetAdditionalHeaders( void )
{
    QString sHeader = m_szServerHeaders;

    // Override the cache-control header on protected resources.

    if (m_bProtected)
        m_mapRespHeaders[ "Cache-control" ] = "no-cache";

    for ( QStringMap::iterator it  = m_mapRespHeaders.begin();
                               it != m_mapRespHeaders.end();
                             ++it )
    {
        sHeader += it.key()  + ": ";
        sHeader += *it + "\r\n";
    }

    return( sHeader );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool HTTPRequest::GetKeepAlive()
{
    bool bKeepAlive = true;

    // if HTTP/1.0... must default to false

    if ((m_nMajor == 1) && (m_nMinor == 0))
        bKeepAlive = false;

    // Read Connection Header...

    QString sConnection = GetHeaderValue( "connection", "default" ).toLower();

    if ( sConnection == "close" )
        bKeepAlive = false;
    else if (sConnection == "keep-alive")
        bKeepAlive = true;

   return bKeepAlive;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool HTTPRequest::ParseRequest()
{
    bool bSuccess = false;

    try
    {
        // Read first line to determin requestType
        QString sRequestLine = ReadLine( 2000 );

        if ( sRequestLine.isEmpty() )
        {
            LOG(VB_GENERAL, LOG_ERR, "Timeout reading first line of request." );
            return false;
        }

        // -=>TODO: Should read lines until a valid request???

        ProcessRequestLine( sRequestLine );

        if (m_nMajor > 1 || m_nMajor < 0)
        {
            m_eResponseType   = ResponseTypeHTML;
            m_nResponseStatus = 505;

            m_response.write( Static505Error );

            return true;
        }

        // Make sure there are a few default values

        m_mapHeaders[ "content-length" ] = "0";
        m_mapHeaders[ "content-type"   ] = "unknown";

        // Read Header

        bool    bDone = false;
        QString sLine = ReadLine( 2000 );

        while (( !sLine.isEmpty() ) && !bDone )
        {
            if (sLine != "\r\n")
            {
                QString sName  = sLine.section( ':', 0, 0 ).trimmed();
                QString sValue = sLine.section( ':', 1 );

                sValue.truncate( sValue.length() - 2 );

                if (!sName.isEmpty() &&
                    !sValue.isEmpty())
                {
                    m_mapHeaders.insert(sName.toLower(), sValue.trimmed());

                    if (sName.contains( "dlna", Qt::CaseInsensitive ))
                    {
                        LOG(VB_UPNP, LOG_INFO,
                           QString( "HTTPRequest::ParseRequest - Header: %1:%2")
                               .arg(sName) .arg(sValue));
                    }
                }

                sLine = ReadLine( 2000 );
            }
            else
                bDone = true;
        }

        // Check to see if we found the end of the header or we timed out.

        if (!bDone)
        {
            LOG(VB_GENERAL, LOG_INFO, "Timeout waiting for request header." );
            return false;
        }

        // HTTP/1.1 requires that the Host header be present, even if empty
        if ((m_nMinor == 1) && !m_mapHeaders.contains("host"))
        {
            m_eResponseType   = ResponseTypeHTML;
            m_nResponseStatus = 400;

            m_response.write( Static400Error );

            return true;
        }

        m_bProtected = false;

        if (IsUrlProtected( m_sBaseUrl ))
        {
            if (!Authenticated())
            {
                m_eResponseType   = ResponseTypeHTML;
                m_nResponseStatus = 401;
                m_mapRespHeaders["WWW-Authenticate"] = "Basic realm=\"MythTV\"";

                m_response.write( Static401Error );

                return true;
            }

            m_bProtected = true;
        }

        bSuccess = true;

        SetContentType( m_mapHeaders[ "content-type" ] );

        // Lets load payload if any.
        long nPayloadSize = m_mapHeaders[ "content-length" ].toLong();

        if (nPayloadSize > 0)
        {
            char *pszPayload = new char[ nPayloadSize + 2 ];
            long  nBytes     = 0;

            nBytes = ReadBlock( pszPayload, nPayloadSize, 5000 );
            if (nBytes == nPayloadSize )
            {
                m_sPayload = QString::fromUtf8( pszPayload, nPayloadSize );

                // See if the payload is just data from a form post
                if (m_eContentType == ContentType_Urlencoded)
                    GetParameters( m_sPayload, m_mapParams );
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR,
                  QString("Unable to read entire payload (read %1 of %2 bytes)")
                      .arg( nBytes ) .arg( nPayloadSize ) );
                bSuccess = false;
            }

            delete [] pszPayload;
        }

        // Check to see if this is a SOAP encoded message

        QString sSOAPAction = GetHeaderValue( "SOAPACTION", "" );

        if (!sSOAPAction.isEmpty())
            bSuccess = ProcessSOAPPayload( sSOAPAction );
        else
            ExtractMethodFromURL();

#if 0
        if (m_sMethod != "*" )
            LOG(VB_UPNP, LOG_DEBUG,
                QString("HTTPRequest::ParseRequest - Socket (%1) Base (%2) "
                        "Method (%3) - Bytes in Socket Buffer (%4)")
                    .arg(getSocketHandle()) .arg(m_sBaseUrl)
                    .arg(m_sMethod) .arg(BytesAvailable()));
#endif
    }
    catch(...)
    {
        LOG(VB_GENERAL, LOG_WARNING,
            "Unexpected exception in HTTPRequest::ParseRequest" );
    }

    return bSuccess;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HTTPRequest::ProcessRequestLine( const QString &sLine )
{
    m_sRawRequest = sLine;

    QString     sToken;
    QStringList tokens = sLine.split(m_procReqLineExp, QString::SkipEmptyParts);
    int         nCount = tokens.count();

    // ----------------------------------------------------------------------

    if ( sLine.startsWith( QString("HTTP/") ))
        m_eType = RequestTypeResponse;
    else
        m_eType = RequestTypeUnknown;

    // ----------------------------------------------------------------------
    // if this is actually a response, then sLine's format will be:
    //      HTTP/m.n <response code> <response text>
    // otherwise:
    //      <method> <Resource URI> HTTP/m.n
    // ----------------------------------------------------------------------

    if (m_eType != RequestTypeResponse)
    {
        // ------------------------------------------------------------------
        // Process as a request
        // ------------------------------------------------------------------

        if (nCount > 0)
            SetRequestType( tokens[0].trimmed() );

        if (nCount > 1)
        {
            //m_sBaseUrl = tokens[1].section( '?', 0, 0).trimmed();
            m_sBaseUrl = (QUrl::fromPercentEncoding(tokens[1].toUtf8()))
                              .section( '?', 0, 0).trimmed();

            m_sResourceUrl = m_sBaseUrl; // Save complete url without parameters

            // Process any Query String Parameters
            QString sQueryStr = tokens[1].section( '?', 1, 1 );

            if (!sQueryStr.isEmpty())
                GetParameters( sQueryStr, m_mapParams );
        }

        if (nCount > 2)
            SetRequestProtocol( tokens[2].trimmed() );
    }
    else
    {
        // ------------------------------------------------------------------
        // Process as a Response
        // ------------------------------------------------------------------
        if (nCount > 0)
            SetRequestProtocol( tokens[0].trimmed() );

        if (nCount > 1)
            m_nResponseStatus = tokens[1].toInt();
    }


}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool HTTPRequest::ParseRange( QString sRange,
                              long long   llSize,
                              long long *pllStart,
                              long long *pllEnd   )
{
    // ----------------------------------------------------------------------
    // -=>TODO: Only handle 1 range at this time... 
    //          should make work with full spec.
    // ----------------------------------------------------------------------

    if (sRange.isEmpty())
        return false;

    // ----------------------------------------------------------------------
    // remove any "bytes="
    // ----------------------------------------------------------------------
    int nIdx = sRange.indexOf(m_parseRangeExp);

    if (nIdx < 0)
        return false;

    if (nIdx > 0)
        sRange.remove( 0, nIdx );

    // ----------------------------------------------------------------------
    // Split multiple ranges
    // ----------------------------------------------------------------------

    QStringList ranges = sRange.split(',', QString::SkipEmptyParts);

    if (ranges.count() == 0)
        return false;

    // ----------------------------------------------------------------------
    // Split first range into its components
    // ----------------------------------------------------------------------

    QStringList parts = ranges[0].split('-');

    if (parts.count() != 2)
        return false;

    if (parts[0].isNull() && parts[1].isNull())
        return false;

    // ----------------------------------------------------------------------
    //
    // ----------------------------------------------------------------------

    bool conv_ok;
    if (parts[0].isNull())
    {
        // ------------------------------------------------------------------
        // Does it match "-####"
        // ------------------------------------------------------------------

        long long llValue = parts[1].toLongLong(&conv_ok);
        if (!conv_ok)    return false;

        *pllStart = llSize - llValue;
        *pllEnd   = llSize - 1;
    }
    else if (parts[1].isNull())
    {
        // ------------------------------------------------------------------
        // Does it match "####-"
        // ------------------------------------------------------------------

        *pllStart = parts[0].toLongLong(&conv_ok);

        if (!conv_ok)
            return false;

        *pllEnd   = llSize - 1;
    }
    else
    {
        // ------------------------------------------------------------------
        // Must be  "####-####"
        // ------------------------------------------------------------------

        *pllStart = parts[0].toLongLong(&conv_ok);
        if (!conv_ok)    return false;
        *pllEnd   = parts[1].toLongLong(&conv_ok);
        if (!conv_ok)    return false;

        if (*pllStart > *pllEnd)
            return false;
    }

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, QString("%1 Range Requested %2 - %3")
        .arg(getSocketHandle()) .arg(*pllStart) .arg(*pllEnd));
#endif

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HTTPRequest::ExtractMethodFromURL()
{
    // Strip out leading http://192.168.1.1:6544/ -> /
    // Should fix #8678
    QRegExp sRegex("^http://.*/");
    sRegex.setMinimal(true);
    m_sBaseUrl.replace(sRegex, "/");

    QStringList sList = m_sBaseUrl.split('/', QString::SkipEmptyParts);

    m_sMethod = "";

    if (!sList.isEmpty())
    {
        m_sMethod = sList.last();
        sList.pop_back();
    }

    m_sBaseUrl = '/' + sList.join( "/" );
    LOG(VB_UPNP, LOG_INFO, QString("ExtractMethodFromURL(end) : %1 : %2")
                               .arg(m_sMethod).arg(m_sBaseUrl));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool HTTPRequest::ProcessSOAPPayload( const QString &sSOAPAction )
{
    bool bSuccess = false;

    // ----------------------------------------------------------------------
    // Open Supplied XML uPnp Description file.
    // ----------------------------------------------------------------------

    LOG(VB_UPNP, LOG_DEBUG,
        QString("HTTPRequest::ProcessSOAPPayload : %1 : ").arg(sSOAPAction));
    QDomDocument doc ( "request" );

    QString sErrMsg;
    int     nErrLine = 0;
    int     nErrCol  = 0;

    if (!doc.setContent( m_sPayload, true, &sErrMsg, &nErrLine, &nErrCol ))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString( "Error parsing request at line: %1 column: %2 : %3" )
                .arg(nErrLine) .arg(nErrCol) .arg(sErrMsg));
        return( false );
    }

    // --------------------------------------------------------------
    // XML Document Loaded... now parse it
    // --------------------------------------------------------------

    QString sService;

    if (sSOAPAction.contains( '#' ))
    {
        m_sNameSpace    = sSOAPAction.section( '#', 0, 0).remove( 0, 1);
        m_sMethod       = sSOAPAction.section( '#', 1 );
        m_sMethod.remove( m_sMethod.length()-1, 1 );
    }
    else
    {
        if (sSOAPAction.contains( '/' ))
        {
            int nPos      = sSOAPAction.lastIndexOf( '/' );
            m_sNameSpace  = sSOAPAction.mid(1, nPos);
            m_sMethod     = sSOAPAction.mid(nPos + 1,
                                            sSOAPAction.length() - nPos - 2);

            nPos          = m_sNameSpace.lastIndexOf( '/', -2);
            sService      = m_sNameSpace.mid(nPos + 1,
                                             m_sNameSpace.length() - nPos - 2);
            m_sNameSpace  = m_sNameSpace.mid( 0, nPos );
        }
        else
        {
            m_sNameSpace = QString::null;
            m_sMethod    = sSOAPAction;
            m_sMethod.remove( QChar( '\"' ) );
        }
    }

    QDomNodeList oNodeList = doc.elementsByTagNameNS( m_sNameSpace, m_sMethod );

    if (oNodeList.count() == 0)
        oNodeList = 
            doc.elementsByTagNameNS("http://schemas.xmlsoap.org/soap/envelope/",
                                    "Body");

    if (oNodeList.count() > 0)
    {
        QDomNode oMethod = oNodeList.item(0);

        if (!oMethod.isNull())
        {
            m_bSOAPRequest = true;

            for ( QDomNode oNode = oMethod.firstChild(); !oNode.isNull();
                           oNode = oNode.nextSibling() )
            {
                QDomElement e = oNode.toElement();

                if (!e.isNull())
                {
                    QString sName  = e.tagName();
                    QString sValue = "";

                    QDomText  oText = oNode.firstChild().toText();

                    if (!oText.isNull())
                        sValue = oText.nodeValue();

                    sName  = QUrl::fromPercentEncoding(sName.toUtf8());
                    sValue = QUrl::fromPercentEncoding(sValue.toUtf8());

                    m_mapParams.insert( sName.trimmed().toLower(), sValue );
                }
            }

            bSuccess = true;
        }
    }

    return bSuccess;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Serializer *HTTPRequest::GetSerializer()
{
    Serializer *pSerializer = NULL;

    if (m_bSOAPRequest) 
        pSerializer = (Serializer *)new SoapSerializer(&m_response,
                                                       m_sNameSpace, m_sMethod);
    else
    {
        QString sAccept = GetHeaderValue( "Accept", "*/*" );
        
        if (sAccept.contains( "application/json", Qt::CaseInsensitive ))    
            pSerializer = (Serializer *)new JSONSerializer(&m_response,
                                                           m_sMethod);
        else if (sAccept.contains( "text/javascript", Qt::CaseInsensitive ))    
            pSerializer = (Serializer *)new JSONSerializer(&m_response,
                                                           m_sMethod);
        else if (sAccept.contains( "text/x-apple-plist+xml", Qt::CaseInsensitive ))
            pSerializer = (Serializer *)new XmlPListSerializer(&m_response);
    }

    // Default to XML

    if (pSerializer == NULL)
        pSerializer = (Serializer *)new XmlSerializer(&m_response, m_sMethod);

    return pSerializer;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::Encode(const QString &sIn)
{
    QString sStr = sIn;
#if 0
    LOG(VB_UPNP, LOG_DEBUG, 
        QString("HTTPRequest::Encode Input : %1").arg(sStr));
#endif
    sStr.replace('&', "&amp;" ); // This _must_ come first
    sStr.replace('<', "&lt;"  );
    sStr.replace('>', "&gt;"  );
    sStr.replace('"', "&quot;");
    sStr.replace("'", "&apos;");

#if 0
    LOG(VB_UPNP, LOG_DEBUG,
        QString("HTTPRequest::Encode Output : %1").arg(sStr));
#endif
    return sStr;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::GetETagHash(const QByteArray &data)
{
    QByteArray hash = QCryptographicHash::hash( data.data(), QCryptographicHash::Sha1);

    return ("\"" + hash.toHex() + "\"");
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool HTTPRequest::IsUrlProtected( const QString &sBaseUrl )
{
    QString sProtected = UPnp::GetConfiguration()->GetValue( "HTTP/Protected/Urls", "/setup;/Config" );

    QStringList oList = sProtected.split( ';' );

    for( int nIdx = 0; nIdx < oList.count(); nIdx++)
    {
        if (sBaseUrl.startsWith( oList[nIdx], Qt::CaseInsensitive ))
            return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool HTTPRequest::Authenticated()
{
    QStringList oList = m_mapHeaders[ "authorization" ].split( ' ' );

    if (oList.count() < 2)
        return false;

    if (oList[0].compare( "basic", Qt::CaseInsensitive ) != 0)
        return false;

    QString sCredentials = QByteArray::fromBase64( oList[1].toUtf8() );
    
    oList = sCredentials.split( ':' );

    if (oList.count() < 2)
        return false;

    QString sUserName = UPnp::GetConfiguration()->GetValue( "HTTP/Protected/UserName", "admin" );
    

    if (oList[0].compare( sUserName, Qt::CaseInsensitive ) != 0)
        return false;

    QString sPassword = UPnp::GetConfiguration()->GetValue( "HTTP/Protected/Password", 
                                 /* mythtv */ "8hDRxR1+E/n3/s3YUOhF+lUw7n4=" );

    QCryptographicHash crypto( QCryptographicHash::Sha1 );

    crypto.addData( oList[1].toUtf8() );

    QString sPasswordHash( crypto.result().toBase64() );

    if (sPasswordHash != sPassword )
        return false;
    
    return true;
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// BufferedSocketDeviceRequest Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

BufferedSocketDeviceRequest::BufferedSocketDeviceRequest( BufferedSocketDevice *pSocket )
{
    m_pSocket  = pSocket;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

qlonglong BufferedSocketDeviceRequest::BytesAvailable(void)
{
    if (m_pSocket)
        return( m_pSocket->BytesAvailable() );

    return( 0 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

qulonglong BufferedSocketDeviceRequest::WaitForMore( int msecs, bool *timeout )
{
    if (m_pSocket)
        return( m_pSocket->WaitForMore( msecs, timeout ));

    return( 0 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool BufferedSocketDeviceRequest::CanReadLine()
{
    if (m_pSocket)
        return( m_pSocket->CanReadLine() );

    return( false );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString BufferedSocketDeviceRequest::ReadLine( int msecs )
{
    QString sLine;

    if (m_pSocket)
        sLine = m_pSocket->ReadLine( msecs );

    return( sLine );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

qlonglong BufferedSocketDeviceRequest::ReadBlock(
    char *pData, qulonglong nMaxLen, int msecs)
{
    if (m_pSocket)
    {
        if (msecs == 0)
            return( m_pSocket->ReadBlock( pData, nMaxLen ));
        else
        {
            bool bTimeout = false;

            while ( (BytesAvailable() < (int)nMaxLen) && !bTimeout )
                m_pSocket->WaitForMore( msecs, &bTimeout );

            // Just return what we have even if timed out.

            return( m_pSocket->ReadBlock( pData, nMaxLen ));
        }
    }

    return( -1 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

qlonglong BufferedSocketDeviceRequest::WriteBlock(
    const char *pData, qulonglong nLen)
{
    if (m_pSocket)
        return( m_pSocket->WriteBlock( pData, nLen ));

    return( -1 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

qlonglong BufferedSocketDeviceRequest::WriteBlockDirect(
    const char *pData, qulonglong nLen)
{
    if (m_pSocket)
        return( m_pSocket->WriteBlockDirect( pData, nLen ));

    return( -1 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString BufferedSocketDeviceRequest::GetHostAddress()
{
    return( m_pSocket->SocketDevice()->address().toString() );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString BufferedSocketDeviceRequest::GetPeerAddress()
{
    return( m_pSocket->SocketDevice()->peerAddress().toString() );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void BufferedSocketDeviceRequest::SetBlocking( bool bBlock )
{
    if (m_pSocket)
        return( m_pSocket->SocketDevice()->setBlocking( bBlock ));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool BufferedSocketDeviceRequest::IsBlocking()
{
    if (m_pSocket)
        return( m_pSocket->SocketDevice()->blocking());

    return false;
}

