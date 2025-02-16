//////////////////////////////////////////////////////////////////////////////
// Program Name: httprequest.cpp
// Created     : Oct. 21, 2005
//
// Purpose     : Http Request/Response
//
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#include "httprequest.h"

#include <QFile>
#include <QFileInfo>
#include <QHostInfo>
#include <QStringList>
#include <QCryptographicHash>
#include <QDateTime>
#include <Qt>
#include <QUrl>

#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h> // for gethostname
// FOR DEBUGGING
#include <iostream>

#ifndef _WIN32
#include <netinet/tcp.h>
#endif

#include "libmythbase/compat.h"
#include "libmythbase/configuration.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythtimer.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/unziputil.h"

#include "serializers/xmlSerializer.h"
#include "serializers/soapSerializer.h"
#include "serializers/jsonSerializer.h"
#include "serializers/xmlplistSerializer.h"

#include "httpserver.h"

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

static std::array<const MIMETypes,66> g_MIMETypes
{{
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
    { "crt" , "application/x-x509-ca-cert" },
    { "doc" , "application/vnd.ms-word"    },
    { "gz"  , "application/x-tar"          },
    { "js"  , "application/javascript"     },
    { "m3u" , "application/x-mpegurl"      }, // HTTP Live Streaming
    { "m3u8", "application/x-mpegurl"      }, // HTTP Live Streaming
    { "ogx" , "application/ogg"            }, // http://wiki.xiph.org/index.php/MIME_Types_and_File_Extensions
    { "pdf" , "application/pdf"            },
    { "pem" , "application/x-x509-ca-cert" },
    { "qjs" , "application/javascript"     },
    { "rm"  , "application/vnd.rn-realmedia" },
    { "swf" , "application/x-shockwave-flash" },
    { "xls" , "application/vnd.ms-excel"   },
    { "zip" , "application/x-tar"          },
    // Audio Mime Types:
    { "aac" , "audio/mp4"                  },
    { "ac3" , "audio/vnd.dolby.dd-raw"     }, // DLNA?
    { "flac", "audio/x-flac"               }, // This may become audio/flac in the future
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
    { "asx" , "video/x-ms-asf"             },
    { "asf" , "video/x-ms-asf"             },
    { "avi" , "video/x-msvideo"            }, // Also video/avi
    { "m2p" , "video/mp2p"                 }, // RFC 3555
    { "m4v" , "video/mp4"                  },
    { "mpeg", "video/mp2p"                 }, // RFC 3555
    { "mpeg2","video/mp2p"                 }, // RFC 3555
    { "mpg" , "video/mp2p"                 }, // RFC 3555
    { "mpg2", "video/mp2p"                 }, // RFC 3555
    { "mov" , "video/quicktime"            },
    { "mp4" , "video/mp4"                  },
    { "mkv" , "video/x-matroska"           }, // See http://matroska.org/technical/specs/notes.html#MIME (See NOTE 1)
    { "nuv" , "video/nupplevideo"          },
    { "ogv" , "video/ogg"                  }, // Defined: http://wiki.xiph.org/index.php/MIME_Types_and_File_Extensions
    { "ps"  , "video/mp2p"                 }, // RFC 3555
    { "ts"  , "video/mp2t"                 }, // RFC 3555
    { "vob" , "video/mpeg"                 }, // Also video/dvd
    { "wmv" , "video/x-ms-wmv"             },
    // Font Mime Types
    { "ttf"  , "font/ttf"                  },
    { "woff" , "font/woff"                 },
    { "woff2", "font/woff2"                }
}};

// NOTE 1
// This formerly was video/x-matroska, but got changed due to #8643
// This was reverted from video/x-mkv, due to #10980
// See http://matroska.org/technical/specs/notes.html#MIME
// If you can't please everyone, may as well be correct as you piss some off

static QString StaticPage =
    "<!DOCTYPE html>"
    "<HTML>"
      "<HEAD>"
        "<TITLE>Error %1</TITLE>"
        "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=ISO-8859-1\">"
      "</HEAD>"
      "<BODY><H1>%2.</H1></BODY>"
    "</HTML>";

const char *HTTPRequest::s_szServerHeaders = "Accept-Ranges: bytes\r\n";

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::GetLastHeader( const QString &sType ) const
{
    QStringList values = m_mapHeaders.values( sType );
    if (!values.isEmpty())
        return values.last();
    return {};
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HttpRequestType HTTPRequest::SetRequestType( const QString &sType )
{
    // HTTP
    if (sType == "GET"        ) return( m_eType = RequestTypeGet         );
    if (sType == "HEAD"       ) return( m_eType = RequestTypeHead        );
    if (sType == "POST"       ) return( m_eType = RequestTypePost        );
    if (sType == "OPTIONS"    ) return( m_eType = RequestTypeOptions     );

    // UPnP
    if (sType == "M-SEARCH"   ) return( m_eType = RequestTypeMSearch     );
    if (sType == "NOTIFY"     ) return( m_eType = RequestTypeNotify      );
    if (sType == "SUBSCRIBE"  ) return( m_eType = RequestTypeSubscribe   );
    if (sType == "UNSUBSCRIBE") return( m_eType = RequestTypeUnsubscribe );

    if (sType.startsWith( QString("HTTP/") )) return( m_eType = RequestTypeResponse );

    LOG(VB_HTTP, LOG_INFO,
        QString("HTTPRequest::SentRequestType( %1 ) - returning Unknown.")
            .arg(sType));

    return( m_eType = RequestTypeUnknown);
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::BuildResponseHeader( long long nSize )
{
    QString sHeader;
    QString sContentType = (m_eResponseType == ResponseTypeOther) ?
                            m_sResponseTypeText : GetResponseType();
    //-----------------------------------------------------------------------
    // Headers describing the connection
    //-----------------------------------------------------------------------

    // The protocol string
    sHeader = QString( "%1 %2\r\n" ).arg(GetResponseProtocol(),
                                         GetResponseStatus());

    SetResponseHeader("Date", MythDate::toString(MythDate::current(), MythDate::kRFC822)); // RFC 822
    SetResponseHeader("Server", HttpServer::GetServerVersion());

    SetResponseHeader("Connection", m_bKeepAlive ? "Keep-Alive" : "Close" );
    if (m_bKeepAlive)
    {
        if (m_nKeepAliveTimeout == 0s) // Value wasn't passed in by the server, so go with the configured value
            m_nKeepAliveTimeout = gCoreContext->GetDurSetting<std::chrono::seconds>("HTTP/KeepAliveTimeoutSecs", 10s);
        SetResponseHeader("Keep-Alive", QString("timeout=%1").arg(m_nKeepAliveTimeout.count()));
    }

    //-----------------------------------------------------------------------
    // Entity Headers - Describe the content and allowed methods
    // RFC 2616 Section 7.1
    //-----------------------------------------------------------------------
    if (m_eResponseType != ResponseTypeHeader) // No entity headers
    {
        SetResponseHeader("Content-Language", gCoreContext->GetLanguageAndVariant().replace("_", "-"));
        SetResponseHeader("Content-Type", sContentType);

        // Default to 'inline' but we should support 'attachment' when it would
        // be appropriate i.e. not when streaming a file to a upnp player or browser
        // that can support it natively
        if (!m_sFileName.isEmpty())
        {
            // TODO: Add support for utf8 encoding - RFC 5987
            QString filename = QFileInfo(m_sFileName).fileName(); // Strip any path
            SetResponseHeader("Content-Disposition", QString("inline; filename=\"%2\"").arg(QString(filename.toLatin1())));
        }

        SetResponseHeader("Content-Length", QString::number(nSize));

        // See DLNA  7.4.1.3.11.4.3 Tolerance to unavailable contentFeatures.dlna.org header
        //
        // It is better not to return this header, than to return it containing
        // invalid or incomplete information. We are unable to currently determine
        // this information at this stage, so do not return it. Only older devices
        // look for it. Newer devices use the information provided in the UPnP
        // response

//         QString sValue = GetHeaderValue( "getContentFeatures.dlna.org", "0" );
//
//         if (sValue == "1")
//             sHeader += "contentFeatures.dlna.org: DLNA.ORG_OP=01;DLNA.ORG_CI=0;"
//                     "DLNA.ORG_FLAGS=01500000000000000000000000000000\r\n";


        // DLNA 7.5.4.3.2.33 MT transfer mode indication
        QString sTransferMode = GetRequestHeader( "transferMode.dlna.org", "" );

        if (sTransferMode.isEmpty())
        {
            if (m_sResponseTypeText.startsWith("video/") ||
                m_sResponseTypeText.startsWith("audio/"))
                sTransferMode = "Streaming";
            else
                sTransferMode = "Interactive";
        }

        if (sTransferMode == "Streaming")
            SetResponseHeader("transferMode.dlna.org", "Streaming");
        else if (sTransferMode == "Background")
            SetResponseHeader("transferMode.dlna.org", "Background");
        else if (sTransferMode == "Interactive")
            SetResponseHeader("transferMode.dlna.org", "Interactive");

        // HACK Temporary hack for Samsung TVs - Needs to be moved later as it's not entirely DLNA compliant
        if (!GetRequestHeader( "getcontentFeatures.dlna.org", "" ).isEmpty())
            SetResponseHeader("contentFeatures.dlna.org", "DLNA.ORG_OP=01;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=01500000000000000000000000000000");
    }

    auto values = m_mapHeaders.values("origin");
    for (const auto & value : std::as_const(values))
        AddCORSHeaders(value);

    if (qEnvironmentVariableIsSet("HTTPREQUEST_DEBUG"))
    {
        // Dump response header
        QMap<QString, QString>::iterator it;
        for ( it = m_mapRespHeaders.begin(); it != m_mapRespHeaders.end(); ++it )
        {
            LOG(VB_HTTP, LOG_INFO, QString("(Response Header) %1: %2").arg(it.key(), it.value()));
        }
    }

    sHeader += GetResponseHeaders();
    sHeader += "\r\n";

    return sHeader;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

qint64 HTTPRequest::SendResponse( void )
{
    qint64      nBytes    = 0;

    switch( m_eResponseType )
    {
        // The following are all eligable for gzip compression
        case ResponseTypeUnknown:
        case ResponseTypeNone:
            LOG(VB_HTTP, LOG_INFO,
                QString("HTTPRequest::SendResponse( None ) :%1 -> %2:")
                    .arg(GetResponseStatus(), GetPeerAddress()));
            return( -1 );
        case ResponseTypeJS:
        case ResponseTypeCSS:
        case ResponseTypeText:
        case ResponseTypeSVG:
        case ResponseTypeXML:
        case ResponseTypeHTML:
            // If the reponse isn't already in the buffer, then load it
            if (m_sFileName.isEmpty() || !m_response.buffer().isEmpty())
                break;
            {
                QFile file(m_sFileName);
                if (file.exists() && file.size() < (2LL * 1024 * 1024) && // For security/stability, limit size of files read into buffer to 2MiB
                    file.open(QIODevice::ReadOnly | QIODevice::Text))
                    m_response.buffer() = file.readAll();

                if (!m_response.buffer().isEmpty())
                    break;

                // Let SendResponseFile try or send a 404
                m_eResponseType = ResponseTypeFile;
            }
            [[fallthrough]];
        case ResponseTypeFile: // Binary files
            LOG(VB_HTTP, LOG_INFO,
                QString("HTTPRequest::SendResponse( File ) :%1 -> %2:")
                    .arg(GetResponseStatus(), GetPeerAddress()));
            return( SendResponseFile( m_sFileName ));
        case ResponseTypeOther:
        case ResponseTypeHeader:
        default:
            break;
    }

    LOG(VB_HTTP, LOG_INFO,
        QString("HTTPRequest::SendResponse(xml/html) (%1) :%2 -> %3: %4")
             .arg(m_sFileName, GetResponseStatus(), GetPeerAddress(),
                  QString::number(m_eResponseType)));

    // ----------------------------------------------------------------------
    // Check for ETag match...
    // ----------------------------------------------------------------------

    QString sETag = GetRequestHeader( "If-None-Match", "" );

    if ( !sETag.isEmpty() && sETag == m_mapRespHeaders[ "ETag" ] )
    {
        LOG(VB_HTTP, LOG_INFO,
            QString("HTTPRequest::SendResponse(%1) - Cached")
                .arg(sETag));

        m_nResponseStatus = 304;
        m_eResponseType = ResponseTypeHeader; // No entity headers

        // no content can be returned.
        m_response.buffer().clear();
    }

    // ----------------------------------------------------------------------

    int nContentLen = m_response.buffer().length();

    QBuffer *pBuffer = &m_response;

    // ----------------------------------------------------------------------
    // DEBUGGING
    if (qEnvironmentVariableIsSet("HTTPREQUEST_DEBUG"))
        std::cout << m_response.buffer().constData() << std::endl;
    // ----------------------------------------------------------------------

    LOG(VB_HTTP, LOG_DEBUG, QString("Reponse Content Length: %1").arg(nContentLen));

    // ----------------------------------------------------------------------
    // Should we try to return data gzip'd?
    // ----------------------------------------------------------------------

    QBuffer compBuffer;

    auto values = m_mapHeaders.values("accept-encoding");
    bool gzip_found = std::any_of(values.cbegin(), values.cend(),
                                  [](const auto & value)
                                      {return value.contains( "gzip" ); });

    if (( nContentLen > 0 ) && gzip_found)
    {
        QByteArray compressed = gzipCompress( m_response.buffer() );
        compBuffer.setData( compressed );

        if (!compBuffer.buffer().isEmpty())
        {
            pBuffer = &compBuffer;

            SetResponseHeader( "Content-Encoding", "gzip" );
            LOG(VB_HTTP, LOG_DEBUG, QString("Reponse Compressed Content Length: %1").arg(compBuffer.buffer().length()));
        }
    }

    // ----------------------------------------------------------------------
    // Write out Header.
    // ----------------------------------------------------------------------

    nContentLen = pBuffer->buffer().length();

    QString    rHeader = BuildResponseHeader( nContentLen );

    QByteArray sHeader = rHeader.toUtf8();
    LOG(VB_HTTP, LOG_DEBUG, QString("Response header size: %1 bytes").arg(sHeader.length()));
    nBytes  = WriteBlock( sHeader.constData(), sHeader.length() );

    if (nBytes < sHeader.length())
    {
        LOG( VB_HTTP, LOG_ERR, QString("HttpRequest::SendResponse(): "
                                       "Incomplete write of header, "
                                       "%1 written of %2")
                                        .arg(nBytes).arg(sHeader.length()));
    }

    // ----------------------------------------------------------------------
    // Write out Response buffer.
    // ----------------------------------------------------------------------

    if (( m_eType != RequestTypeHead ) &&
        ( nContentLen > 0 ))
    {
        qint64 bytesWritten = SendData( pBuffer, 0, nContentLen );
        //qint64 bytesWritten = WriteBlock( pBuffer->buffer(), pBuffer->buffer().length() );

        if (bytesWritten != nContentLen)
            LOG(VB_HTTP, LOG_ERR, "HttpRequest::SendResponse(): Error occurred while writing response body.");
        else
            nBytes += bytesWritten;
    }

    return( nBytes );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

qint64 HTTPRequest::SendResponseFile( const QString& sFileName )
{
    qint64      nBytes  = 0;
    long long   llSize  = 0;
    long long   llStart = 0;
    long long   llEnd   = 0;

    LOG(VB_HTTP, LOG_INFO, QString("SendResponseFile ( %1 )").arg(sFileName));

    m_eResponseType     = ResponseTypeOther;
    m_sResponseTypeText = "text/plain";

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
        QString sRange = GetRequestHeader( "range", "" );

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
                // RFC 7233 - A server generating a 416 (Range Not Satisfiable)
                // response to a byte-range request SHOULD send a Content-Range
                // header field with an unsatisfied-range value
                m_mapRespHeaders[ "Content-Range" ] = QString("bytes */%3")
                                                              .arg( llSize );
                llSize = 0;
                LOG(VB_HTTP, LOG_INFO,
                    QString("HTTPRequest::SendResponseFile(%1) - "
                            "invalid byte range %2-%3/%4")
                            .arg(sFileName) .arg(llStart) .arg(llEnd)
                            .arg(llSize));
            }
        }

        // HACK: D-Link DSM-320
        // The following headers are only required by servers which don't support
        // http keep alive. We do support it, so we don't need it. Keeping it in
        // place to prevent someone re-adding it in future
        //m_mapRespHeaders[ "X-User-Agent"    ] = "redsonic";

        // ------------------------------------------------------------------
        //
        // ------------------------------------------------------------------

    }
    else
    {
        LOG(VB_HTTP, LOG_INFO,
            QString("HTTPRequest::SendResponseFile(%1) - cannot find file!")
                .arg(sFileName));
        m_nResponseStatus = 404;
        m_response.write( GetResponsePage() );
    }

    // -=>TODO: Should set "Content-Length: *" if file is still recording

    // ----------------------------------------------------------------------
    // Write out Header.
    // ----------------------------------------------------------------------

    QString    rHeader = BuildResponseHeader( llSize );
    QByteArray sHeader = rHeader.toUtf8();
    LOG(VB_HTTP, LOG_DEBUG, QString("Response header size: %1 bytes").arg(sHeader.length()));
    nBytes = WriteBlock( sHeader.constData(), sHeader.length() );

    if (nBytes < sHeader.length())
    {
        LOG( VB_HTTP, LOG_ERR, QString("HttpRequest::SendResponseFile(): "
                                       "Incomplete write of header, "
                                       "%1 written of %2")
                                        .arg(nBytes).arg(sHeader.length()));
    }

    // ----------------------------------------------------------------------
    // Write out File.
    // ----------------------------------------------------------------------

#if 0
    LOG(VB_HTTP, LOG_DEBUG,
        QString("SendResponseFile : size = %1, start = %2, end = %3")
            .arg(llSize).arg(llStart).arg(llEnd));
#endif
    if (( m_eType != RequestTypeHead ) && (llSize != 0))
    {
        long long sent = SendFile( tmpFile, llStart, llSize );

        if (sent == -1)
        {
            LOG(VB_HTTP, LOG_INFO,
                QString("SendResponseFile( %1 ) Error: %2 [%3]" )
                    .arg(sFileName) .arg(errno) .arg(strerror(errno)));

            nBytes = -1;
        }
    }

    // -=>TODO: Only returns header length...
    //          should we change to return total bytes?

    return nBytes;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

static constexpr size_t SENDFILE_BUFFER_SIZE { 65536 };

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

    if ( !pDevice->seek( llStart ))
        return -1;

    std::array<char,SENDFILE_BUFFER_SIZE> aBuffer {};

    qint64 llBytesRemaining = llBytes;
    qint64 llBytesToRead    = 0;
    qint64 llBytesRead      = 0;

    while ((sent < llBytes) && !pDevice->atEnd())
    {
        llBytesToRead  = std::min( (qint64)SENDFILE_BUFFER_SIZE, llBytesRemaining );
        llBytesRead = pDevice->read( aBuffer.data(), llBytesToRead );
        if ( llBytesRead != -1 )
        {
            if ( WriteBlock( aBuffer.data(), llBytesRead ) == -1)
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
    qint64 sent = SendData( (QIODevice *)(&file), llStart, llBytes );

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

    stream << R"(<?xml version="1.0" encoding="utf-8"?>)";

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
    {
        stream << "</s:Fault>" << SOAP_ENVELOPE_END;
    }

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
    {
        stream << "<" << m_sMethod << "Response>\r\n";
    }

    for (const auto & arg : std::as_const(args))
    {
        stream << "<" << arg.m_sName;

        if (arg.m_pAttributes)
        {
            for (const auto & attr : std::as_const(*arg.m_pAttributes))
            {
                stream << " " << attr.m_sName << "='"
                       << Encode( attr.m_sValue ) << "'";
            }
        }

        stream << ">";

        if (m_bSOAPRequest)
            stream << Encode( arg.m_sValue );
        else
            stream << arg.m_sValue;

        stream << "</" << arg.m_sName << ">\r\n";
    }

    if (m_bSOAPRequest)
    {
        stream << "</u:" << m_sMethod << "Response>\r\n"
                   << SOAP_ENVELOPE_END;
    }
    else
    {
        stream << "</" << m_sMethod << "Response>\r\n";
    }

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
    QFileInfo file(m_sFileName);

    if (!m_sFileName.isEmpty() && file.exists())
    {
        QDateTime ims = QDateTime::fromString(GetRequestHeader("if-modified-since", ""), Qt::RFC2822Date);
#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
        ims.setTimeSpec(Qt::UTC);
#else
        ims.setTimeZone(QTimeZone(QTimeZone::UTC));
#endif
        if (ims.isValid() && ims <= file.lastModified()) // Strong validator
        {
            m_eResponseType = ResponseTypeHeader;
            m_nResponseStatus = 304; // Not Modified
        }
        else
        {
            if (m_eResponseType == ResponseTypeUnknown)
                m_eResponseType = ResponseTypeFile;
            m_nResponseStatus   = 200; // OK
            SetResponseHeader("Last-Modified", MythDate::toString(file.lastModified(),
                                                                  (MythDate::kOverrideUTC |
                                                                   MythDate::kRFC822))); // RFC 822
            SetResponseHeader("Cache-Control", "no-cache=\"Ext\", max-age = 7200"); // 2 Hours
        }
    }
    else
    {
        m_eResponseType   = ResponseTypeHTML;
        m_nResponseStatus = 404; // Resource not found
        m_response.write( GetResponsePage() );
        LOG(VB_HTTP, LOG_INFO,
            QString("HTTPRequest::FormatFileResponse('%1') - cannot find file")
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
    return QString("%1/%2.%3").arg(m_sProtocol,
                                   QString::number(m_nMajor),
                                   QString::number(m_nMinor));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::GetResponseProtocol()
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

    return {"HTTP/1.1"};
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HttpContentType HTTPRequest::SetContentType( const QString &sType )
{
    if ((sType == "application/x-www-form-urlencoded"          ) ||
        (sType.startsWith("application/x-www-form-urlencoded;")))
        return( m_eContentType = ContentType_Urlencoded );

    if ((sType == "text/xml"                                   ) ||
        (sType.startsWith("text/xml;")                         ))
        return( m_eContentType = ContentType_XML        );

    if ((sType == "application/json") ||
        sType.startsWith("application/json;"))
        return( m_eContentType = ContentType_JSON);

    return( m_eContentType = ContentType_Unknown );
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::GetResponseStatus( void ) const
{
    switch( m_nResponseStatus )
    {
        case 200:   return( "200 OK"                               );
        case 201:   return( "201 Created"                          );
        case 202:   return( "202 Accepted"                         );
        case 204:   return( "204 No Content"                       );
        case 205:   return( "205 Reset Content"                    );
        case 206:   return( "206 Partial Content"                  );
        case 300:   return( "300 Multiple Choices"                 );
        case 301:   return( "301 Moved Permanently"                );
        case 302:   return( "302 Found"                            );
        case 303:   return( "303 See Other"                        );
        case 304:   return( "304 Not Modified"                     );
        case 305:   return( "305 Use Proxy"                        );
        case 307:   return( "307 Temporary Redirect"               );
        case 308:   return( "308 Permanent Redirect"               );
        case 400:   return( "400 Bad Request"                      );
        case 401:   return( "401 Unauthorized"                     );
        case 403:   return( "403 Forbidden"                        );
        case 404:   return( "404 Not Found"                        );
        case 405:   return( "405 Method Not Allowed"               );
        case 406:   return( "406 Not Acceptable"                   );
        case 408:   return( "408 Request Timeout"                  );
        case 410:   return( "410 Gone"                             );
        case 411:   return( "411 Length Required"                  );
        case 412:   return( "412 Precondition Failed"              );
        case 413:   return( "413 Request Entity Too Large"         );
        case 414:   return( "414 Request-URI Too Long"             );
        case 415:   return( "415 Unsupported Media Type"           );
        case 416:   return( "416 Requested Range Not Satisfiable"  );
        case 417:   return( "417 Expectation Failed"               );
        // I'm a teapot
        case 428:   return( "428 Precondition Required"            ); // RFC 6585
        case 429:   return( "429 Too Many Requests"                ); // RFC 6585
        case 431:   return( "431 Request Header Fields Too Large"  ); // RFC 6585
        case 500:   return( "500 Internal Server Error"            );
        case 501:   return( "501 Not Implemented"                  );
        case 502:   return( "502 Bad Gateway"                      );
        case 503:   return( "503 Service Unavailable"              );
        case 504:   return( "504 Gateway Timeout"                  );
        case 505:   return( "505 HTTP Version Not Supported"       );
        case 510:   return( "510 Not Extended"                     );
        case 511:   return( "511 Network Authentication Required"  ); // RFC 6585
    }

    return( QString( "%1 Unknown" ).arg( m_nResponseStatus ));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QByteArray HTTPRequest::GetResponsePage( void )
{
    return StaticPage.arg(QString::number(m_nResponseStatus), GetResponseStatus()).toUtf8();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::GetResponseType( void ) const
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

    for (const auto & type : g_MIMETypes)
    {
        ext = type.pszExtension;

        if ( sFileExtension.compare(ext, Qt::CaseInsensitive) == 0 )
            return( type.pszType );
    }

    return( "text/plain" );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QStringList HTTPRequest::GetSupportedMimeTypes()
{
    QStringList mimeTypes;

    for (const auto & type : g_MIMETypes)
    {
        if (!mimeTypes.contains( type.pszType ))
            mimeTypes.append( type.pszType );
    }

    return mimeTypes;
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

            LOG(VB_HTTP, LOG_DEBUG, sLOC + "file starts with " + sHex);

            if ( sHex == "000001ba44000400" )  // MPEG2 PS
                sMIME = "video/mp2p";

            if ( head == "MythTVVi" )
            {
                file.seek(100);
                head = file.read(4);

                if ( head == "DIVX" )
                {
                    LOG(VB_HTTP, LOG_DEBUG, sLOC + "('MythTVVi...DIVXLAME')");
                    sMIME = "video/mp4";
                }
                // NuppelVideo is "RJPG" at byte 612
                // We could also check the audio (LAME or RAWA),
                // but since most UPnP clients choke on Nuppel, no need
            }

            file.close();
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, sLOC + "Could not read file");
        }
    }

    LOG(VB_HTTP, LOG_INFO, sLOC + "type is " + sMIME);
    return sMIME;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

long HTTPRequest::GetParameters( QString sParams, QStringMap &mapParams  )
{
    long nCount = 0;

    LOG(VB_HTTP, LOG_INFO, QString("sParams: '%1'").arg(sParams));

    // This looks odd, but it is here to cope with stupid UPnP clients that
    // forget to de-escape the URLs.  We can't map %26 here as well, as that
    // breaks anything that is trying to pass & as part of a name or value.
    sParams.replace( "&amp;", "&" );

    if (!sParams.isEmpty())
    {
        QStringList params = sParams.split('&', Qt::SkipEmptyParts);
        for (const auto & param : std::as_const(params))
        {
            QString sName  = param.section( '=', 0, 0 );
            QString sValue = param.section( '=', 1 );
            sValue.replace("+"," ");

            if (!sName.isEmpty())
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

QString HTTPRequest::GetRequestHeader( const QString &sKey, const QString &sDefault )
{
    auto it = m_mapHeaders.find( sKey.toLower() );

    if ( it == m_mapHeaders.end())
        return( sDefault );

    return *it;
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::GetResponseHeaders( void )
{
    QString sHeader = s_szServerHeaders;

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

bool HTTPRequest::ParseKeepAlive()
{
    // TODO: Think about whether we should use a longer timeout if the client
    //       has explicitly specified 'Keep-alive'

    // HTTP 1.1 ... server may assume keep-alive
    bool bKeepAlive = true;

    // if HTTP/1.0... must default to false
    if ((m_nMajor == 1) && (m_nMinor == 0))
        bKeepAlive = false;

    // Read Connection Header to see whether the client has explicitly
    // asked for the connection to be kept alive or closed after the response
    // is sent
    QString sConnection = GetRequestHeader( "connection", "default" ).toLower();

    QStringList sValueList = sConnection.split(",");

    if ( sValueList.contains("close") )
    {
        LOG(VB_HTTP, LOG_DEBUG, "Client requested the connection be closed");
        bKeepAlive = false;
    }
    else if (sValueList.contains("keep-alive"))
    {
        bKeepAlive = true;
    }

   return bKeepAlive;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HTTPRequest::ParseCookies(void)
{
    QStringList sCookieList = m_mapHeaders.values("cookie");

    QStringList::iterator it;
    for (it = sCookieList.begin(); it != sCookieList.end(); ++it)
    {
        QString key = (*it).section('=', 0, 0);
        QString value = (*it).section('=', 1);

        m_mapCookies.insert(key, value);
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool HTTPRequest::ParseRequest()
{
    bool bSuccess = false;

    try
    {
        // Read first line to determine requestType
        QString sRequestLine = ReadLine( 2s );

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
            m_response.write( GetResponsePage() );

            return true;
        }

        if (m_eType == RequestTypeUnknown)
        {
            m_eResponseType   = ResponseTypeHTML;
            m_nResponseStatus = 501; // Not Implemented
            // Conservative list, we can't really know what methods we
            // actually allow for an arbitrary resource without some sort of
            // high maintenance database
            SetResponseHeader("Allow",  "GET, HEAD");
            m_response.write( GetResponsePage() );
            return true;
        }

        // Read Header
        bool    bDone = false;
        QString sLine = ReadLine( 2s );

        while (( !sLine.isEmpty() ) && !bDone )
        {
            if (sLine != "\r\n")
            {
                QString sName  = sLine.section( ':', 0, 0 ).trimmed();
                QString sValue = sLine.section( ':', 1 );

                sValue.truncate( sValue.length() - 2 );

                if (!sName.isEmpty() && !sValue.isEmpty())
                {
                    m_mapHeaders.insert(sName.toLower(), sValue.trimmed());
                }

                sLine = ReadLine( 2s );
            }
            else
            {
                bDone = true;
            }
        }

        // Dump request header
        for ( auto it = m_mapHeaders.begin(); it != m_mapHeaders.end(); ++it )
        {
            LOG(VB_HTTP, LOG_INFO, QString("(Request Header) %1: %2")
                                            .arg(it.key(), *it));
        }

        // Parse Cookies
        ParseCookies();

        // Parse out keep alive
        m_bKeepAlive = ParseKeepAlive();

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
            m_response.write( GetResponsePage() );

            return true;
        }

        // Destroy session if requested
        if (m_mapHeaders.contains("x-myth-clear-session"))
        {
            SetCookie("sessionToken", "", MythDate::current().addDays(-2), true);
            m_mapCookies.remove("sessionToken");
        }

        // Allow session resumption for TLS connections
        if (m_mapCookies.contains("sessionToken"))
        {
            QString sessionToken = m_mapCookies["sessionToken"];
            MythSessionManager *sessionManager = gCoreContext->GetSessionManager();
            MythUserSession session = sessionManager->GetSession(sessionToken);

            if (session.IsValid())
                m_userSession = session;
        }

        if (IsUrlProtected( m_sBaseUrl ))
        {
            if (!Authenticated())
            {
                m_eResponseType   = ResponseTypeHTML;
                m_nResponseStatus = 401;
                m_response.write( GetResponsePage() );
                // Since this may not be the first attempt at authentication,
                // Authenticated may have set the header with the appropriate
                // stale attribute
                SetResponseHeader("WWW-Authenticate", GetAuthenticationHeader(false));

                return true;
            }

            m_bProtected = true;
        }

        bSuccess = true;

        SetContentType( GetLastHeader( "content-type" ) );
        // Lets load payload if any.
        long nPayloadSize = GetLastHeader( "content-length" ).toLong();

        if (nPayloadSize > 0)
        {
            char *pszPayload = new char[ nPayloadSize + 2 ];
            long  nBytes     = 0;

            nBytes = ReadBlock( pszPayload, nPayloadSize, 5s );
            if (nBytes == nPayloadSize )
            {
                m_sPayload = QString::fromUtf8( pszPayload, nPayloadSize );

                // See if the payload is just data from a form post
                if (m_eContentType == ContentType_Urlencoded)
                    GetParameters( m_sPayload, m_mapParams );
                if (m_eContentType == ContentType_JSON)
                    m_mapParams.insert( "json", m_sPayload );
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
        QString sSOAPAction = GetRequestHeader( "SOAPACTION", "" );

        if (!sSOAPAction.isEmpty())
            bSuccess = ProcessSOAPPayload( sSOAPAction );
        else
            ExtractMethodFromURL();

#if 0
        if (m_sMethod != "*" )
            LOG(VB_HTTP, LOG_DEBUG,
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

    QStringList tokens = sLine.split(m_procReqLineExp, Qt::SkipEmptyParts);
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
            m_sOriginalUrl = tokens[1].toUtf8(); // Used by authorization check
            m_sRequestUrl = QUrl::fromPercentEncoding(tokens[1].toUtf8());
            m_sBaseUrl = m_sRequestUrl.section( '?', 0, 0).trimmed();

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

    QStringList ranges = sRange.split(',', Qt::SkipEmptyParts);
    if (ranges.count() == 0)
        return false;

    // ----------------------------------------------------------------------
    // Split first range into its components
    // ----------------------------------------------------------------------

    QStringList parts = ranges[0].split('-');

    if (parts.count() != 2)
        return false;

    if (parts[0].isEmpty() && parts[1].isEmpty())
        return false;

    // ----------------------------------------------------------------------
    //
    // ----------------------------------------------------------------------

    bool conv_ok = false;
    if (parts[0].isEmpty())
    {
        // ------------------------------------------------------------------
        // Does it match "-####"
        // ------------------------------------------------------------------

        long long llValue = parts[1].toLongLong(&conv_ok);
        if (!conv_ok)    return false;

        *pllStart = llSize - llValue;
        *pllEnd   = llSize - 1;
    }
    else if (parts[1].isEmpty())
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

    LOG(VB_HTTP, LOG_DEBUG, QString("%1 Range Requested %2 - %3")
        .arg(getSocketHandle()) .arg(*pllStart) .arg(*pllEnd));

    return true;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HTTPRequest::ExtractMethodFromURL()
{
    // Strip out leading http://192.168.1.1:6544/ -> /
    // Should fix #8678
    // FIXME what about https?
    static const QRegularExpression re {"^http[s]?://.*?/"};
    m_sBaseUrl.replace(re, "/");

    QStringList sList = m_sBaseUrl.split('/', Qt::SkipEmptyParts);
    m_sMethod = "";

    if (!sList.isEmpty())
    {
        m_sMethod = sList.last();
        sList.pop_back();
    }

    m_sBaseUrl = '/' + sList.join( "/" );
    LOG(VB_HTTP, LOG_INFO, QString("ExtractMethodFromURL(end) : %1 : %2")
                               .arg(m_sMethod, m_sBaseUrl));
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

    LOG(VB_HTTP, LOG_INFO,
        QString("HTTPRequest::ProcessSOAPPayload : %1 : ").arg(sSOAPAction));
    QDomDocument doc ( "request" );

#if QT_VERSION < QT_VERSION_CHECK(6,5,0)
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
#else
    auto parseResult =doc.setContent( m_sPayload,
                                      QDomDocument::ParseOption::UseNamespaceProcessing );
    if (parseResult)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString( "Error parsing request at line: %1 column: %2 : %3" )
                .arg(parseResult.errorLine).arg(parseResult.errorColumn)
                .arg(parseResult.errorMessage));
        return( false );
    }
#endif

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
            m_sNameSpace.clear();
            m_sMethod    = sSOAPAction;
            m_sMethod.remove( QChar( '\"' ) );
        }
    }

    QDomNodeList oNodeList = doc.elementsByTagNameNS( m_sNameSpace, m_sMethod );

    if (oNodeList.count() == 0)
    {
        oNodeList =
            doc.elementsByTagNameNS("http://schemas.xmlsoap.org/soap/envelope/",
                                    "Body");
    }

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
    Serializer *pSerializer = nullptr;

    if (m_bSOAPRequest)
    {
        pSerializer = (Serializer *)new SoapSerializer(&m_response,
                                                       m_sNameSpace, m_sMethod);
    }
    else
    {
        QString sAccept = GetRequestHeader( "Accept", "*/*" );

        if (sAccept.contains( "application/json", Qt::CaseInsensitive ) ||
            sAccept.contains( "text/javascript", Qt::CaseInsensitive ))
        {
            pSerializer = (Serializer *)new JSONSerializer(&m_response,
                                                           m_sMethod);
        }
        else if (sAccept.contains( "text/x-apple-plist+xml", Qt::CaseInsensitive ))
        {
            pSerializer = (Serializer *)new XmlPListSerializer(&m_response);
        }
    }

    // Default to XML

    if (pSerializer == nullptr)
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
    LOG(VB_HTTP, LOG_DEBUG,
        QString("HTTPRequest::Encode Input : %1").arg(sStr));
#endif
    sStr.replace('&', "&amp;" ); // This _must_ come first
    sStr.replace('<', "&lt;"  );
    sStr.replace('>', "&gt;"  );
    sStr.replace('"', "&quot;");
    sStr.replace("'", "&apos;");

#if 0
    LOG(VB_HTTP, LOG_DEBUG,
        QString("HTTPRequest::Encode Output : %1").arg(sStr));
#endif
    return sStr;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::Decode(const QString& sIn)
{
    QString sStr = sIn;
    sStr.replace("&amp;", "&");
    sStr.replace("&lt;", "<");
    sStr.replace("&gt;", ">");
    sStr.replace("&quot;", "\"");
    sStr.replace("&apos;", "'");

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
    QString sProtected = XmlConfiguration().GetValue("HTTP/Protected/Urls", "/setup;/Config");

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

QString HTTPRequest::GetAuthenticationHeader(bool isStale)
{
    QString authHeader;

    // For now we support a single realm, that will change
    QString realm = "MythTV";

    // Always use digest authentication where supported, it may be available
    // with HTTP 1.0 client as an extension, but we can't tell if that's the
    // case. It's guaranteed to be available for HTTP 1.1+
    if (m_nMajor >= 1 && m_nMinor > 0)
    {
        QString nonce = CalculateDigestNonce(MythDate::current_iso_string());
        QString stale = isStale ? "true" : "false"; // FIXME
        authHeader = QString("Digest realm=\"%1\",nonce=\"%2\","
                             "qop=\"auth\",stale=\"%3\",algorithm=\"MD5\"")
                        .arg(realm, nonce, stale);
    }
    else
    {
        authHeader = QString("Basic realm=\"%1\"").arg(realm);
    }

    return authHeader;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::CalculateDigestNonce(const QString& timeStamp) const
{
    QString uniqueID = QString("%1:%2").arg(timeStamp, m_sPrivateToken);
    QString hash = QCryptographicHash::hash( uniqueID.toLatin1(), QCryptographicHash::Sha1).toHex(); // TODO: Change to Sha2 with QT5?
    QString nonce = QString("%1%2").arg(timeStamp, hash); // Note: since this is going in a header it should avoid illegal chars
    return nonce;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool HTTPRequest::BasicAuthentication()
{
    LOG(VB_HTTP, LOG_NOTICE, "Attempting HTTP Basic Authentication");
    QStringList oList = GetLastHeader( "authorization" ).split( ' ' );

    if (m_nMajor == 1 && m_nMinor == 0) // We only support Basic auth for http 1.0 clients
    {
        LOG(VB_GENERAL, LOG_WARNING, "Basic authentication is only allowed for HTTP 1.0");
        return false;
    }

    QString sCredentials = QByteArray::fromBase64( oList[1].toUtf8() );

    oList = sCredentials.split( ':' );

    if (oList.count() < 2)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Authorization attempt with invalid number of tokens");
        return false;
    }

    QString sUsername = oList[0];
    QString sPassword = oList[1];

    if (sUsername == "nouser") // Special logout username
        return false;

    MythSessionManager *sessionManager = gCoreContext->GetSessionManager();
    if (!MythSessionManager::IsValidUser(sUsername))
    {
        LOG(VB_GENERAL, LOG_WARNING, "Authorization attempt with invalid username");
        return false;
    }

    QString client = QString("WebFrontend_%1").arg(GetPeerAddress());
    MythUserSession session = sessionManager->LoginUser(sUsername, sPassword,
                                                        client);

    if (!session.IsValid())
    {
        LOG(VB_GENERAL, LOG_WARNING, "Authorization attempt with invalid password");
        return false;
    }

    LOG(VB_HTTP, LOG_NOTICE, "Valid Authorization received");

    if (IsEncrypted()) // Only set a session cookie for encrypted connections, not safe otherwise
        SetCookie("sessionToken", session.GetSessionToken(),
                    session.GetSessionExpires(), true);

    m_userSession = session;

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool HTTPRequest::DigestAuthentication()
{
    LOG(VB_HTTP, LOG_NOTICE, "Attempting HTTP Digest Authentication");
    QString realm = "MythTV"; // TODO Check which realm applies for the request path

    QString authMethod = GetLastHeader( "authorization" ).section(' ', 0, 0).toLower();

    if (authMethod != "digest")
    {
        LOG(VB_GENERAL, LOG_WARNING, "Invalid method in Authorization header");
        return false;
    }

    QString parameterStr = GetLastHeader( "authorization" ).section(' ', 1);

    QMap<QString, QString> paramMap;
    QStringList paramList = parameterStr.split(',');
    QStringList::iterator it;
    for (it = paramList.begin(); it != paramList.end(); ++it)
    {
        QString key = (*it).section('=', 0, 0).toLower().trimmed();
        // Since the value may contain '=' return everything after first occurence
        QString value = (*it).section('=', 1).trimmed();
        // Remove any quotes surrounding the value
        value.remove("\"");
        paramMap[key] = value;
    }

    if (paramMap.size() < 8)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Invalid number of parameters in Authorization header");
        return false;
    }

    if (paramMap["nonce"].isEmpty()    || paramMap["username"].isEmpty() ||
        paramMap["realm"].isEmpty()    || paramMap["uri"].isEmpty() ||
        paramMap["response"].isEmpty() || paramMap["qop"].isEmpty() ||
        paramMap["cnonce"].isEmpty()   || paramMap["nc"].isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, "Missing required parameters in Authorization header");
        return false;
    }

    if (paramMap["username"] == "nouser") // Special logout username
        return false;

    if (paramMap["uri"] != m_sOriginalUrl)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Authorization URI doesn't match the "
                                     "request URI");
        m_nResponseStatus = 400; // Bad Request
        return false;
    }

    if (paramMap["realm"] != realm)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Authorization realm doesn't match the "
                                  "realm of the requested content");
        return false;
    }

    QByteArray nonce = paramMap["nonce"].toLatin1();
    if (nonce.length() < 20)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Authorization nonce is too short");
        return false;
    }

    QString  nonceTimeStampStr = nonce.left(20); // ISO 8601 fixed length
    if (nonce != CalculateDigestNonce(nonceTimeStampStr))
    {
        LOG(VB_GENERAL, LOG_WARNING, "Authorization nonce doesn't match reference");
        LOG(VB_HTTP, LOG_DEBUG, QString("%1  vs  %2").arg(QString(nonce),
                                                          CalculateDigestNonce(nonceTimeStampStr)));
        return false;
    }

    constexpr std::chrono::seconds AUTH_TIMEOUT { 2min }; // 2 Minute timeout to login, to reduce replay attack window
    QDateTime nonceTimeStamp = MythDate::fromString(nonceTimeStampStr);
    if (!nonceTimeStamp.isValid())
    {
        LOG(VB_GENERAL, LOG_WARNING, "Authorization nonce timestamp is invalid.");
        LOG(VB_HTTP, LOG_DEBUG, QString("Timestamp was '%1'").arg(nonceTimeStampStr));
        return false;
    }

    if (MythDate::secsInPast(nonceTimeStamp) > AUTH_TIMEOUT)
    {
        LOG(VB_HTTP, LOG_NOTICE, "Authorization nonce timestamp is invalid or too old.");
        // Tell the client that the submitted nonce has expired at which
        // point they may wish to try again with a fresh nonce instead of
        // telling the user that their credentials were invalid
        SetResponseHeader("WWW-Authenticate", GetAuthenticationHeader(true), true);
        return false;
    }

    MythSessionManager *sessionManager = gCoreContext->GetSessionManager();
    if (!MythSessionManager::IsValidUser(paramMap["username"]))
    {
        LOG(VB_GENERAL, LOG_WARNING, "Authorization attempt with invalid username");
        return false;
    }

    if (paramMap["response"].length() != 32)
    {
        LOG(VB_GENERAL, LOG_WARNING, "Authorization response field is invalid length");
        return false;
    }

    // If you're still reading this, well done, not far to go now

    QByteArray a1 = MythSessionManager::GetPasswordDigest(paramMap["username"]).toLatin1();
    //QByteArray a1 = "bcd911b2ecb15ffbd6d8e6e744d60cf6";
    QString methodDigest = QString("%1:%2").arg(GetRequestType(), paramMap["uri"]);
    QByteArray a2 = QCryptographicHash::hash(methodDigest.toLatin1(),
                                          QCryptographicHash::Md5).toHex();

    QString responseDigest = QString("%1:%2:%3:%4:%5:%6").arg(a1,
                                                              paramMap["nonce"],
                                                              paramMap["nc"],
                                                              paramMap["cnonce"],
                                                              paramMap["qop"],
                                                              a2);
    QByteArray kd = QCryptographicHash::hash(responseDigest.toLatin1(),
                                             QCryptographicHash::Md5).toHex();

    if (paramMap["response"].toLatin1() == kd)
    {
        LOG(VB_HTTP, LOG_NOTICE, "Valid Authorization received");
        QString client = QString("WebFrontend_%1").arg(GetPeerAddress());
        MythUserSession session = sessionManager->LoginUser(paramMap["username"],
                                                            a1,
                                                            client);
        if (!session.IsValid())
        {
            LOG(VB_GENERAL, LOG_ERR, "Valid Authorization received, but we "
                                     "failed to create a valid session");
            return false;
        }

        if (IsEncrypted()) // Only set a session cookie for encrypted connections, not safe otherwise
            SetCookie("sessionToken", session.GetSessionToken(),
                      session.GetSessionExpires(), true);

        m_userSession = session;

        return true;
    }

    LOG(VB_GENERAL, LOG_WARNING, "Authorization attempt with invalid password digest");
    LOG(VB_HTTP, LOG_DEBUG, QString("Received hash was '%1', calculated hash was '%2'")
                            .arg(paramMap["response"], QString(kd)));

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool HTTPRequest::Authenticated()
{
    // Check if the existing user has permission to access this resource
    if (m_userSession.IsValid()) //m_userSession.CheckPermission())
        return true;

    QStringList oList = GetLastHeader( "authorization" ).split( ' ' );

    if (oList.count() < 2)
        return false;

    if (oList[0].compare( "basic", Qt::CaseInsensitive ) == 0)
        return BasicAuthentication();
    if (oList[0].compare( "digest", Qt::CaseInsensitive ) == 0)
        return DigestAuthentication();

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HTTPRequest::SetResponseHeader(const QString& sKey, const QString& sValue,
                                    bool replace)
{
    if (!replace && m_mapRespHeaders.contains(sKey))
        return;

    m_mapRespHeaders[sKey] = sValue;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HTTPRequest::SetCookie(const QString &sKey, const QString &sValue,
                            const QDateTime &expiryDate, bool secure)
{
    if (secure && !IsEncrypted())
    {
        LOG(VB_GENERAL, LOG_WARNING, QString("HTTPRequest::SetCookie(%1=%2): "
                  "A secure cookie cannot be set on an unencrypted connection.")
                    .arg(sKey, sValue));
        return;
    }

    QStringList cookieAttributes;

    // Key=Value
    cookieAttributes.append(QString("%1=%2").arg(sKey, sValue));

    // Domain - Most browsers have problems with a hostname, so it's better to omit this
//     cookieAttributes.append(QString("Domain=%1").arg(GetHostName()));

    // Path - Fix to root, no call for restricting to other paths yet
    cookieAttributes.append("Path=/");

    // Expires - Expiry date, always set one, just good practice
    QString expires = MythDate::toString(expiryDate, MythDate::kRFC822); // RFC 822
    cookieAttributes.append(QString("Expires=%1").arg(expires)); // Cookie Expiry date

    // Secure - Only send this cookie over encrypted connections, it contains
    // sensitive info SECURITY
    if (secure)
        cookieAttributes.append("Secure");

    // HttpOnly - No cookie stealing javascript SECURITY
    cookieAttributes.append("HttpOnly");

    SetResponseHeader("Set-Cookie", cookieAttributes.join("; "));
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString HTTPRequest::GetHostName()
{
    // TODO: This only deals with the HTTP 1.1 case, 1.0 should be rare but we
    //       should probably still handle it

    // RFC 3875 - The is the hostname or ip address in the client request, not
    //            the name or ip we might otherwise know for this server
    QString hostname = GetLastHeader("host");
    if (!hostname.isEmpty())
    {
        // Strip the port
        if (hostname.contains("]:")) // IPv6 port
        {
            return hostname.section("]:", 0 , 0);
        }
        if (hostname.contains(":")) // IPv4 port
        {
            return hostname.section(":", 0 , 0);
        }
        return hostname;
    }

    return GetHostAddress();
}


QString HTTPRequest::GetRequestType( ) const
{
    QString type;
    switch ( m_eType )
    {
        case RequestTypeUnknown :
            type = "UNKNOWN";
            break;
        case RequestTypeGet :
            type = "GET";
            break;
        case RequestTypeHead :
            type = "HEAD";
            break;
        case RequestTypePost :
            type = "POST";
            break;
        case RequestTypeOptions:
            type = "OPTIONS";
            break;
        case RequestTypeMSearch:
            type = "M-SEARCH";
            break;
        case RequestTypeNotify:
            type = "NOTIFY";
            break;
        case RequestTypeSubscribe :
            type = "SUBSCRIBE";
            break;
        case RequestTypeUnsubscribe :
            type = "UNSUBSCRIBE";
            break;
        case RequestTypeResponse :
            type = "RESPONSE";
            break;
    }

    return type;
}

void HTTPRequest::AddCORSHeaders( const QString &sOrigin )
{
    // ----------------------------------------------------------------------
    // SECURITY: Access-Control-Allow-Origin Wildcard
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
    // SECURITY: Allow the WebFrontend on the Master backend and ONLY this
    // machine to access resources on a frontend or slave web server
    //
    // http://www.w3.org/TR/cors/#introduction
    // ----------------------------------------------------------------------

    QStringList allowedOrigins;

    int serverStatusPort = gCoreContext->GetMasterServerStatusPort();
    int backendSSLPort = gCoreContext->GetNumSetting( "BackendSSLPort",
                         serverStatusPort + 10);

    QString masterAddrPort = QString("%1:%2")
        .arg(gCoreContext->GetMasterServerIP())
        .arg(serverStatusPort);
    QString masterTLSAddrPort = QString("%1:%2")
        .arg(gCoreContext->GetMasterServerIP())
        .arg(backendSSLPort);

    allowedOrigins << QString("http://%1").arg(masterAddrPort);
    allowedOrigins << QString("https://%2").arg(masterTLSAddrPort);

    QString localhostname = QHostInfo::localHostName();
    if (!localhostname.isEmpty())
    {
        allowedOrigins << QString("http://%1:%2")
            .arg(localhostname).arg(serverStatusPort);
        allowedOrigins << QString("https://%1:%2")
            .arg(localhostname).arg(backendSSLPort);
    }

    QStringList allowedOriginsList =
        gCoreContext->GetSetting("AllowedOriginsList", QString(
            "https://chromecast.mythtv.org")).split(",");

    for (const auto & origin : std::as_const(allowedOriginsList))
    {
         if (origin.isEmpty())
            continue;

        if (origin == "*" || (!origin.startsWith("http://") &&
            !origin.startsWith("https://")))
        {
            LOG(VB_GENERAL, LOG_ERR, QString("Illegal AllowedOriginsList"
                " entry '%1'. Must start with http[s]:// and not be *")
                .arg(origin));
        }
        else
        {
            allowedOrigins << origin;
        }
    }

    if (VERBOSE_LEVEL_CHECK(VB_HTTP, LOG_DEBUG))
    {
        for (const auto & origin : std::as_const(allowedOrigins))
            LOG(VB_HTTP, LOG_DEBUG, QString("Will allow Origin: %1").arg(origin));
    }

    if (allowedOrigins.contains(sOrigin))
    {
        SetResponseHeader( "Access-Control-Allow-Origin" , sOrigin);
        SetResponseHeader( "Access-Control-Allow-Credentials" , "true");
        SetResponseHeader( "Access-Control-Allow-Headers" , "Content-Type");
        LOG(VB_HTTP, LOG_DEBUG, QString("Allow-Origin: %1)").arg(sOrigin));
    }
    else
    {
        LOG(VB_GENERAL, LOG_CRIT, QString("HTTPRequest: Cross-origin request "
                                          "received with origin (%1)")
                                          .arg(sOrigin));
    }
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// BufferedSocketDeviceRequest Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

QString BufferedSocketDeviceRequest::ReadLine( std::chrono::milliseconds msecs )
{
    QString sLine;

    if (m_pSocket && m_pSocket->isValid() &&
        m_pSocket->state() == QAbstractSocket::ConnectedState)
    {
        bool timeout = false;
        MythTimer timer;
        timer.start();
        while (!m_pSocket->canReadLine() && !timeout)
        {
            timeout = !(m_pSocket->waitForReadyRead( msecs.count() ));

            if ( timer.elapsed() >= msecs )
            {
                timeout = true;
                LOG(VB_HTTP, LOG_INFO, "BufferedSocketDeviceRequest::ReadLine() - Exceeded Total Elapsed Wait Time." );
            }
        }

        if (!timeout)
            sLine = m_pSocket->readLine();
    }

    return( sLine );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

qint64 BufferedSocketDeviceRequest::ReadBlock(char *pData, qint64 nMaxLen,
                                              std::chrono::milliseconds msecs)
{
    if (m_pSocket && m_pSocket->isValid() &&
        m_pSocket->state() == QAbstractSocket::ConnectedState)
    {
        if (msecs == 0ms)
            return( m_pSocket->read( pData, nMaxLen ));

        bool bTimeout = false;
        MythTimer timer;
        timer.start();
        while ( (m_pSocket->bytesAvailable() < (int)nMaxLen) && !bTimeout ) // This can end up waiting far longer than msecs
        {
            bTimeout = !(m_pSocket->waitForReadyRead( msecs.count() ));

            if ( timer.elapsed() >= msecs )
            {
                bTimeout = true;
                LOG(VB_HTTP, LOG_INFO, "BufferedSocketDeviceRequest::ReadBlock() - Exceeded Total Elapsed Wait Time." );
            }
        }

        // Just return what we have even if timed out.

        return( m_pSocket->read( pData, nMaxLen ));
    }

    return( -1 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

qint64 BufferedSocketDeviceRequest::WriteBlock(const char *pData, qint64 nLen)
{
    qint64 bytesWritten = -1;
    if (m_pSocket && m_pSocket->isValid() &&
        m_pSocket->state() == QAbstractSocket::ConnectedState)
    {
        bytesWritten = m_pSocket->write( pData, nLen );
        m_pSocket->waitForBytesWritten();
    }

    return( bytesWritten );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString BufferedSocketDeviceRequest::GetHostAddress()
{
    return( m_pSocket->localAddress().toString() );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

quint16 BufferedSocketDeviceRequest::GetHostPort()
{
    return( m_pSocket->localPort() );
}


/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString BufferedSocketDeviceRequest::GetPeerAddress()
{
    return( m_pSocket->peerAddress().toString() );
}
