//////////////////////////////////////////////////////////////////////////////
// Program Name: httprequest.cpp
// Created     : Oct. 21, 2005
//
// Purpose     : Http Request/Response
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
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

#include "httprequest.h"

#include <QFile>
#include <QFileInfo>
#include <QTextCodec>
#include <QStringList>

#include "mythconfig.h"
#if !( CONFIG_DARWIN || CONFIG_CYGWIN || defined(__FreeBSD__) || defined(USING_MINGW))
#define USE_SETSOCKOPT
#include <sys/sendfile.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <cerrno>

#ifndef USING_MINGW
#include <netinet/tcp.h>
#endif

#include "upnp.h"

#include "compat.h"
#include "mythverbose.h"

#include "serializers/xmlSerializer.h"
#include "serializers/soapSerializer.h"
#include "serializers/jsonSerializer.h"

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

static MIMETypes g_MIMETypes[] =
{
    { "gif" , "image/gif"                  },
    { "jpg" , "image/jpeg"                 },
    { "jpeg", "image/jpeg"                 },
    { "png" , "image/png"                  },
    { "htm" , "text/html"                  },
    { "html", "text/html"                  },
    { "js"  , "text/html"                  },
    { "txt" , "text/plain"                 },
    { "xml" , "text/xml"                   },
    { "pdf" , "application/pdf"            },
    { "avi" , "video/avi"                  },
    { "css" , "text/css"                   },
    { "swf" , "application/futuresplash"   },
    { "xls" , "application/vnd.ms-excel"   },
    { "doc" , "application/vnd.ms-word"    },
    { "mid" , "audio/midi"                 },
    { "mp3" , "audio/mpeg"                 },
    { "rm"  , "application/vnd.rn-realmedia" },
    { "wav" , "audio/wav"                  },
    { "zip" , "application/x-tar"          },
    { "gz"  , "application/x-tar"          },
    { "mpg" , "video/mpeg"                 },
    { "mpg2", "video/mpeg"                 },
    { "mpeg", "video/mpeg"                 },
    { "mpeg2","video/mpeg"                 },
    { "ts"  , "video/mpegts"               },
    { "vob" , "video/mpeg"                 },
    { "asf" , "video/x-ms-asf"             },
    { "nuv" , "video/nupplevideo"          },
    { "mov" , "video/quicktime"            },
    { "mp4" , "video/mp4"                  },
    // This formerly was video/x-matroska, but got changed due to #8643
    { "mkv" , "video/x-mkv"                },
    { "mka" , "audio/x-matroska"           },
    { "wmv" , "video/x-ms-wmv"             },
    // Defined: http://wiki.xiph.org/index.php/MIME_Types_and_File_Extensions
    { "ogg" , "audio/ogg"                  },
    { "ogv" , "video/ogg"                  },
    { "ogx" , "application/ogg"            },
    { "oga" , "audio/ogg"                  },
    // Similarly, this could be audio/flac or application/flac:
    { "flac", "audio/x-flac"               },
    { "m4a" , "audio/x-m4a"                },
};

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

    VERBOSE( VB_UPNP, QString( "HTTPRequest::SentRequestType( %1 ) - returning Unknown." )
                         .arg( sType ) );

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

    sHeader = QString( "HTTP/%1.%2 %3\r\n"
                       "Date: %4\r\n"
                       "Server: %5, UPnP/1.0, MythTV %6\r\n" )
                 .arg( m_nMajor )
                 .arg( m_nMinor )
                 .arg( GetResponseStatus() )
                 .arg( QDateTime::currentDateTime().toString( "d MMM yyyy hh:mm:ss" ) )
                 .arg( HttpServer::g_sPlatform )
                 .arg( MYTH_BINARY_VERSION );

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
        case ResponseTypeUnknown:
        case ResponseTypeNone:
            VERBOSE(VB_UPNP,QString("HTTPRequest::SendResponse( None ) :%1 -> %2:")
                            .arg(GetResponseStatus())
                            .arg(GetPeerAddress()));
            return( -1 );

        case ResponseTypeFile:
            VERBOSE(VB_UPNP,QString("HTTPRequest::SendResponse( File ) :%1 -> %2:")
                            .arg(GetResponseStatus())
                            .arg(GetPeerAddress()));

            return( SendResponseFile( m_sFileName ));

        case ResponseTypeXML:
        case ResponseTypeHTML:
        case ResponseTypeOther:
        default:
            break;
    }

     VERBOSE(VB_UPNP,QString(
                 "HTTPRequest::SendResponse(xml/html) (%1) :%2 -> %3: %4")
             .arg(m_sFileName)
             .arg(GetResponseStatus())
             .arg(GetPeerAddress())
             .arg(m_eResponseType));

    // ----------------------------------------------------------------------
    // Make it so the header is sent with the data
    // ----------------------------------------------------------------------

#ifdef USE_SETSOCKOPT
    // Never send out partially complete segments
    setsockopt( getSocketHandle(), SOL_TCP, TCP_CORK, &g_on, sizeof( g_on ));
#endif

    // ----------------------------------------------------------------------
    // Write out Header.
    // ----------------------------------------------------------------------
    const QByteArray &buffer = m_response.buffer();

    QString    rHeader = BuildHeader( buffer.length() );
    QByteArray sHeader = rHeader.toUtf8();
    nBytes  = WriteBlockDirect( sHeader.constData(), sHeader.length() );

    // ----------------------------------------------------------------------
    // Write out Response buffer.
    // ----------------------------------------------------------------------

    if (( m_eType != RequestTypeHead ) && ( buffer.length() > 0 ))
    {
#if 0
        VERBOSE(VB_UPNP, QString("HTTPRequest::SendResponse : DATA : %1 : ")
                .arg( m_aBuffer.size() ));
        for (uint i = 0; i < (uint)m_aBuffer.size(); i++)
            cout << m_aBuffer.data()[i];

        cout << endl;
#endif
        nBytes += SendData( &m_response, 0, m_response.size() );
    }

    // ----------------------------------------------------------------------
    // Turn off the option so any small remaining packets will be sent
    // ----------------------------------------------------------------------

#ifdef USE_SETSOCKOPT
    setsockopt( getSocketHandle(), SOL_TCP, TCP_CORK, &g_off, sizeof( g_off ));
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

    VERBOSE(VB_UPNP, QString("SendResponseFile ( %1 )").arg(sFileName));

    m_eResponseType     = ResponseTypeOther;
    m_sResponseTypeText = "text/plain";

    /*
        Dump request header
    for ( QStringMap::iterator it  = m_mapHeaders.begin();
                               it != m_mapHeaders.end();
                             ++it )
    {
        cout << it.key().toLatin1().constData() << ": "
             << (*it).toLatin1().constData() << endl;
    }
    */

    // ----------------------------------------------------------------------
    // Make it so the header is sent with the data
    // ----------------------------------------------------------------------

#ifdef USE_SETSOCKOPT
    // Never send out partially complete segments
    setsockopt( getSocketHandle(), SOL_TCP, TCP_CORK, &g_on, sizeof( g_on ));
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

        if (sRange.length() > 0)
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
                VERBOSE(VB_UPNP,
                    QString("HTTPRequest::SendResponseFile(%1) - invalid byte range %2-%3/%4")
                            .arg(sFileName)
                            .arg(llStart)
                            .arg(llEnd)
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
        VERBOSE(VB_UPNP,
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

    //VERBOSE(VB_UPNP, QString("SendResponseFile : size = %1, start = %2, end = %3").arg(llSize).arg(llStart).arg(llEnd));
    if (( m_eType != RequestTypeHead ) && (llSize != 0))
    {
        long long sent = SendFile( tmpFile, llStart, llSize );

        if (sent == -1)
        {
            VERBOSE(VB_UPNP,QString("SendResponseFile( %1 ) Error: %2 [%3]" )
                               .arg( sFileName )
                               .arg( errno     )
                               .arg( strerror( errno ) ));

            nBytes = -1;
        }
    }

    // ----------------------------------------------------------------------
    // Turn off the option so any small remaining packets will be sent
    // ----------------------------------------------------------------------

#ifdef USE_SETSOCKOPT
    setsockopt( getSocketHandle(), SOL_TCP, TCP_CORK, &g_off, sizeof( g_off ));
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
    qint64 sent = 0;

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

    return sent;
}

/////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////

qint64 HTTPRequest::SendFile( QFile &file, qint64 llStart, qint64 llBytes )
{
    qint64 sent = 0;

#if CONFIG_DARWIN || CONFIG_CYGWIN || defined(__FreeBSD__) || defined(USING_MINGW)

    sent = SendData( (QIODevice *)(&file), llStart, llBytes );

#else

    __off64_t  offset = llStart; 
    int        fd     = file.handle( ); 
  
    if ( fd == -1 ) 
    { 
        VERBOSE(VB_UPNP, QString("SendResponseFile( %1 ) Error: %2 [%3]" ) 
                             .arg( file.fileName() ) 
                             .arg( file.error( )) 
                             .arg( strerror( file.error( ) ))); 
        sent = -1; 
    } 
    else 
    { 
        qint64     llSent = 0;

        do 
        { 
            // SSIZE_MAX should work in kernels 2.6.16 and later. 
            // The loop is needed in any case. 
  
            sent = sendfile64( 
                getSocketHandle(), fd, &offset, 
                (size_t) ((llBytes > INT_MAX) ? INT_MAX : llBytes)); 
  
            if (sent >= 0)
            {
                llBytes -= sent; 
                llSent  += sent;
                VERBOSE(VB_UPNP, QString("SendResponseFile : --- " 
                        "size = %1, offset = %2, sent = %3") 
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

    if (sDetails.length() > 0)
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

        m_eResponseType                     = ResponseTypeFile;
        m_nResponseStatus                   = 200;
        m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";
    }
    else
    {
        m_eResponseType   = ResponseTypeHTML;
        m_nResponseStatus = 404;
        VERBOSE(VB_UPNP, QString("HTTPRequest::FormatFileResponse(%1) - cannot find file").arg(sFileName));
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

ContentType HTTPRequest::SetContentType( const QString &sType )
{
    if (sType == "application/x-www-form-urlencoded") return( m_eContentType = ContentType_Urlencoded );
    if (sType == "text/xml"                         ) return( m_eContentType = ContentType_XML        );

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

    if ( sSuffix == "nuv"      // If a very old recording, might be an MPEG?
       //|| sSuffix == "blah"
       )
    {
        // Read the header to find out:

        QFile file( sFileName );

        if ( file.open(QIODevice::ReadOnly | QIODevice::Text) )
        {
            QByteArray head = file.read(8);
            QString    sHex = head.toHex();

            VERBOSE(VB_UPNP+VB_EXTRA, sLOC + "file starts with " + sHex);

            if ( sHex == "000001ba44000400" )  // MPEG2 PS
                sMIME = "video/mpeg";

            if ( head == "MythTVVi" )
            {
                file.seek(100);
                head = file.read(4);

                if ( head == "DIVX" )
                {
                    VERBOSE(VB_UPNP+VB_EXTRA, sLOC + "('MythTVVi...DIVXLAME')");
                    sMIME = "video/mp4";
                }
                // NuppelVideo is "RJPG" at byte 612
                // We could also check the audio (LAME or RAWA),
                // but since most UPnP clients choke on Nuppel, no need
            }

            file.close();
        }
        else VERBOSE(VB_IMPORTANT, sLOC + "Could not read file");
    }

    VERBOSE(VB_UPNP, sLOC + "type is " + sMIME);
    return sMIME;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

long HTTPRequest::GetParameters( QString sParams, QStringMap &mapParams  )
{
    long nCount = 0;

    VERBOSE(VB_UPNP|VB_EXTRA, QString("sParams: '%1'").arg(sParams));

    // This looks odd, but it is here to cope with stupid UPnP clients that
    // forget to de-escape the URLs.  We can't map %26 here as well, as that
    // breaks anything that is trying to pass & as part of a name or value.
    sParams.replace( "&amp;", "&" );

    if (sParams.length() > 0)
    {
        QStringList params = sParams.split('&', QString::SkipEmptyParts);

        for ( QStringList::Iterator it  = params.begin();
                                    it != params.end();  ++it )
        {
            QString sName  = (*it).section( '=', 0, 0 );
            QString sValue = (*it).section( '=', 1 );
            sValue.replace("+"," ");

            if ((sName.length() != 0) && (sValue.length() !=0))
            {
                sName  = QUrl::fromPercentEncoding(sName.toUtf8());
                sValue = QUrl::fromPercentEncoding(sValue.toUtf8());

                mapParams.insert( sName.trimmed(), sValue );
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

        if ( sRequestLine.length() == 0)
        {
            VERBOSE(VB_IMPORTANT, "HTTPRequest::ParseRequest - Timeout reading first line of request." );
            return false;
        }

        // -=>TODO: Should read lines until a valid request???

        ProcessRequestLine( sRequestLine );

        // Make sure there are a few default values

        m_mapHeaders[ "content-length" ] = "0";
        m_mapHeaders[ "content-type"   ] = "unknown";

        // Read Header

        bool    bDone = false;
        QString sLine = ReadLine( 2000 );

        while (( sLine.length() > 0 ) && !bDone )
        {
            if (sLine != "\r\n")
            {
                QString sName  = sLine.section( ':', 0, 0 ).trimmed();
                QString sValue = sLine.section( ':', 1 );

                sValue.truncate( sValue.length() - 2 );

                if ((sName.length() != 0) && (sValue.length() !=0))
                {
                    m_mapHeaders.insert(
                        sName.toLower(), sValue.trimmed());

                    if (sName.contains( "dlna", Qt::CaseInsensitive ))
                    {
                        VERBOSE(VB_UPNP, QString( "HTTPRequest::ParseRequest - Header: %1:%2")
                                            .arg( sName  )
                                            .arg( sValue ));
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
            VERBOSE(VB_IMPORTANT, "HTTPRequest::ParseRequest - Timeout waiting for request header." );
            return false;
        }

        bSuccess = true;

        SetContentType( m_mapHeaders[ "content-type" ] );

        // Lets load payload if any.

        long nPayloadSize = m_mapHeaders[ "content-length" ].toLong();

        if (nPayloadSize > 0)
        {
            char *pszPayload = new char[ nPayloadSize + 2 ];
            long  nBytes     = 0;

            if (( nBytes = ReadBlock( pszPayload, nPayloadSize, 5000 )) == nPayloadSize )
            {
                m_sPayload = QString::fromUtf8( pszPayload, nPayloadSize );

                // See if the payload is just data from a form post

                if ( m_eContentType == ContentType_Urlencoded)
                    GetParameters( m_sPayload, m_mapParams );
            }
            else
            {
                VERBOSE( VB_IMPORTANT, QString( "HTTPRequest::ParseRequest - Unable to read entire payload (read %1 of %2 bytes" )
                                        .arg( nBytes )
                                        .arg( nPayloadSize ) );
                bSuccess = false;
            }

            delete [] pszPayload;
        }

        // Check to see if this is a SOAP encoded message

        QString sSOAPAction = GetHeaderValue( "SOAPACTION", "" );

        if (sSOAPAction.length() > 0)
            bSuccess = ProcessSOAPPayload( sSOAPAction );
        else
            ExtractMethodFromURL();

/*
        if (m_sMethod != "*" )
            VERBOSE( VB_UPNP, QString("HTTPRequest::ParseRequest - Socket (%1) Base (%2) Method (%3) - Bytes in Socket Buffer (%4)")
                                 .arg( getSocketHandle() )
                                 .arg( m_sBaseUrl )
                                 .arg( m_sMethod  )
                                 .arg( BytesAvailable()));
*/
    }
    catch( ... )
    {
        VERBOSE(VB_IMPORTANT, "Unexpected exception in HTTPRequest::ParseRequest" );
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
            m_sBaseUrl = (QUrl::fromPercentEncoding(tokens[1].toUtf8())).section( '?', 0, 0).trimmed();

            m_sResourceUrl = m_sBaseUrl;  // Save complete url without paramaters

            // Process any Query String Parameters

            //QString sQueryStr = tokens[1].section( '?', 1, 1   );
            QString sQueryStr = (QUrl::fromPercentEncoding(tokens[1].toUtf8())).section( '?', 1, 1 );

            if (sQueryStr.length() > 0)
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
    // -=>TODO: Only handle 1 range at this time... should make work with full spec.
    // ----------------------------------------------------------------------

    if (sRange.length() == 0)
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

    //cout << getSocketHandle() << "Range Requested " << *pllStart << " - " << *pllEnd << endl;

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

    if (sList.size() > 0)
    {
        m_sMethod = sList.last();
        sList.pop_back();
    }

    m_sBaseUrl = '/' + sList.join( "/" );
    VERBOSE(VB_UPNP, QString("ExtractMethodFromURL(end) : %1 : %2").arg(m_sMethod).arg(m_sBaseUrl));
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

    VERBOSE(VB_UPNP, QString("HTTPRequest::ProcessSOAPPayload : %1 : ").arg(sSOAPAction));
    QDomDocument doc ( "request" );

    QString sErrMsg;
    int     nErrLine = 0;
    int     nErrCol  = 0;

    if (!doc.setContent( m_sPayload, true, &sErrMsg, &nErrLine, &nErrCol ))
    {
        VERBOSE(VB_IMPORTANT, QString( "Error parsing request at line: %1 column: %2 : %3" )
                                .arg( nErrLine )
                                .arg( nErrCol  )
                                .arg( sErrMsg  ));
        return( false );
    }

    // --------------------------------------------------------------
    // XML Document Loaded... now parse it
    // --------------------------------------------------------------

    if (sSOAPAction.contains( '#' ))
    {
        m_sNameSpace    = sSOAPAction.section( '#', 0, 0).remove( 0, 1);
        m_sMethod       = sSOAPAction.section( '#', 1 );
        m_sMethod.remove( m_sMethod.length()-1, 1 );
    }
    if (sSOAPAction.contains( '/' ))
    {
        int nPos       = sSOAPAction.lastIndexOf( '/' );
        m_sNameSpace   = sSOAPAction.mid( 1, nPos );
        m_sMethod      = sSOAPAction.mid( nPos + 1, sSOAPAction.length() - nPos - 2  );
    }
    else
    {
        m_sNameSpace = QString::null;
        m_sMethod    = sSOAPAction;
        m_sMethod.remove( QChar( '\"' ) );
    }

    QDomNodeList oNodeList = doc.elementsByTagNameNS( m_sNameSpace, m_sMethod );

    if (oNodeList.count() == 0)
        oNodeList = doc.elementsByTagNameNS( "http://schemas.xmlsoap.org/soap/envelope/", "Body" );

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

                    m_mapParams.insert( sName.trimmed(), sValue );
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
        pSerializer = (Serializer *)new SoapSerializer( &m_response, m_sNameSpace, m_sMethod );
    else
    {
        QString sAccept = GetHeaderValue( "Accept", "*/*" );
        
        if (sAccept.contains( "application/json", Qt::CaseInsensitive ))    
            pSerializer = (Serializer *)new JSONSerializer( &m_response, m_sMethod );
        else if (sAccept.contains( "text/javascript", Qt::CaseInsensitive ))    
            pSerializer = (Serializer *)new JSONSerializer( &m_response, m_sMethod );
    }

    // Default to XML

    if (pSerializer == NULL)
        pSerializer = (Serializer *)new XmlSerializer( &m_response, m_sMethod );

    return pSerializer;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::Encode(const QString &sIn)
{
    QString sStr = sIn;
    //VERBOSE(VB_UPNP, QString("HTTPRequest::Encode Input : %1").arg(sStr));
    sStr.replace('&', "&amp;" ); // This _must_ come first
    sStr.replace('<', "&lt;"  );
    sStr.replace('>', "&gt;"  );
    sStr.replace('"', "&quot;");
    sStr.replace("'", "&apos;");

    //VERBOSE(VB_UPNP, QString("HTTPRequest::Encode Output : %1").arg(sStr));
    return sStr;
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
