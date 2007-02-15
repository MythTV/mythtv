//////////////////////////////////////////////////////////////////////////////
// Program Name: httprequest.cpp
//                                                                            
// Purpose - Http Request/Response 
//                                                                            
// Created By  : David Blain	                Created On : Oct. 21, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "httprequest.h"

#include <qregexp.h>
#include <qstringlist.h>
#include <qtextstream.h>
#include <qurl.h>
#include <qfile.h>
#include <qfileinfo.h>

#include "mythconfig.h"
#if defined CONFIG_DARWIN || defined CONFIG_CYGWIN || defined(__FreeBSD__)
#include "darwin-sendfile.h"
#else
#define USE_SETSOCKOPT
#include <sys/sendfile.h>
#endif
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>

#include "util.h"
#include "mythcontext.h"
#include "upnp.h"

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

static MIMETypes g_MIMETypes[] = 
{
    { "gif" , "image/gif"                  },
    { "jpg" , "image/jpeg"                 },
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
    { "mpeg", "video/mpeg"                 },
    { "vob",  "video/mpeg"                 },
    { "nuv" , "video/nupplevideo"          },
    { "wmv" , "video/x-ms-wmv"             }
};

static const int g_nMIMELength = sizeof( g_MIMETypes) / sizeof( MIMETypes );
static const int g_on          = 1;
static const int g_off         = 0;

const char *HTTPRequest::m_szServerHeaders = "Accept-Ranges: bytes\r\n";

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

HTTPRequest::HTTPRequest() : m_eType          ( RequestTypeUnknown ),
                             m_eContentType   ( ContentType_Unknown),
                             m_nMajor         (   0 ),
                             m_nMinor         (   0 ),
                             m_bSOAPRequest   ( false ),
                             m_eResponseType  ( ResponseTypeUnknown),
                             m_nResponseStatus( 200 ),
                             m_response       ( m_aBuffer, IO_WriteOnly ),
                             m_pPostProcess   ( NULL )
{
    m_response.setEncoding( QTextStream::UnicodeUTF8 );
}

/////////////////////////////////////////////////////////////////////////////
//  
/////////////////////////////////////////////////////////////////////////////

void HTTPRequest::Reset()
{
    m_eType          = RequestTypeUnknown;
    m_eContentType   = ContentType_Unknown;
    m_nMajor         = 0;
    m_nMinor         = 0;
    m_bSOAPRequest   = false;
    m_eResponseType  = ResponseTypeUnknown;
    m_nResponseStatus= 200;
    m_pPostProcess   = NULL;

    m_aBuffer.truncate( 0 );

    m_sRawRequest = QString();
    m_sBaseUrl    = QString();
    m_sMethod     = QString();

    m_mapParams.clear();
    m_mapHeaders.clear();

    m_sPayload    = QString();

    m_sProtocol   = QString();
    m_sNameSpace  = QString();

    m_mapRespHeaders.clear();

    m_sFileName   = QString();
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

    if (sType.startsWith( "HTTP/" )) return( m_eType = RequestTypeResponse );

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
                       "Server: %5, UPnP/1.0, MythTv %6\r\n" )
                 .arg( m_nMajor )
                 .arg( m_nMinor )
                 .arg( GetResponseStatus() )
                 .arg( QDateTime::currentDateTime().toString( "d MMM yyyy hh:mm:ss" ) )
                 .arg( UPnp::g_sPlatform )
                 .arg( MYTH_BINARY_VERSION );

    sHeader += GetAdditionalHeaders();

    sHeader += QString( "Connection: %1\r\n"
                        "Content-Type: %2\r\n"
                        "Content-Length: %3\r\n" )
                        .arg( GetKeepAlive() ? "Keep-Alive" : "Close" )
                        .arg( sContentType )
                        .arg( nSize );
    sHeader += "\r\n";

    return sHeader;
}

/////////////////////////////////////////////////////////////////////////////
//  
/////////////////////////////////////////////////////////////////////////////

long HTTPRequest::SendResponse( void )
{
    long      nBytes    = 0;
    QCString  sHeader;

    switch( m_eResponseType )
    {
        case ResponseTypeUnknown:
        case ResponseTypeNone:
//            VERBOSE(VB_UPNP,QString("HTTPRequest::SendResponse( None ) :%1 -> %2:")
//                            .arg(GetResponseStatus())
//                            .arg(GetPeerAddress()));
            return( -1 );

        case ResponseTypeFile:
//            VERBOSE(VB_UPNP,QString("HTTPRequest::SendResponse( File ) :%1 -> %2:")
//                            .arg(GetResponseStatus())
//                            .arg(GetPeerAddress()));

            return( SendResponseFile( m_sFileName ));

        case ResponseTypeXML: 
        case ResponseTypeHTML:
        case ResponseTypeOther:
        default:
            break;
    }

    // VERBOSE(VB_UPNP,QString("HTTPRequest::SendResponse(xml/html) :%1 -> %2:")
    //                    .arg(GetResponseStatus())
    //                    .arg(GetPeerAddress()));

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

    sHeader = BuildHeader     ( m_aBuffer.size() ).utf8();
    nBytes  = WriteBlockDirect( sHeader.data(), sHeader.length() );

    // ----------------------------------------------------------------------
    // Write out Response buffer.
    // ----------------------------------------------------------------------

    if (( m_eType != RequestTypeHead ) && ( m_aBuffer.size() > 0 ))
    {
        //VERBOSE(VB_UPNP,QString("HTTPRequest::SendResponse : DATA : %1 : ")
        //                .arg(m_aBuffer.data()));
        
        nBytes += WriteBlockDirect( m_aBuffer.data(), m_aBuffer.size() );
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
    QCString    sHeader;
    long        nBytes  = 0;
    long long   llSize  = 0;
    long long   llStart = 0;
    long long   llEnd   = 0;

    m_eResponseType     = ResponseTypeOther;
    m_sResponseTypeText = "text/plain";

    /*
        Dump request header
    for ( QStringMap::iterator it  = m_mapHeaders.begin(); 
                               it != m_mapHeaders.end(); 
                             ++it ) 
    {  
        cout << it.key() << ": " << it.data() << endl;
    }
    */

    // ----------------------------------------------------------------------
    // Make it so the header is sent with the data
    // ----------------------------------------------------------------------

#ifdef USE_SETSOCKOPT
    // Never send out partially complete segments
    setsockopt( getSocketHandle(), SOL_TCP, TCP_CORK, &g_on, sizeof( g_on ));
#endif

    if (QFile::exists( sFileName ))
    {
        QFileInfo info( sFileName );

        m_sResponseTypeText = GetMimeType( info.extension( FALSE ).lower() );

        // ------------------------------------------------------------------
        // Get File size
        // ------------------------------------------------------------------

        struct stat st;

        if (stat( sFileName.ascii(), &st ) == 0)
            llSize = llEnd = st.st_size;

        m_nResponseStatus = 200;

        // ------------------------------------------------------------------
        // The Content-Range header is apparently a problem for the 
        // AVeL LinkPlayer2 and probably other hardware players with 
        // Syabas firmware. 
        //
        // -=>TODO: Need conformation
        // ------------------------------------------------------------------

        bool    bRange     = false;
        QString sUserAgent = GetHeaderValue( "User-Agent", "");

        if ( sUserAgent.contains( "Syabas", false ) == 0 )
        {
            // --------------------------------------------------------------
            // Process any Range Header
            // --------------------------------------------------------------

            QString sRange = GetHeaderValue( "range", "" ); 

            if (sRange.length() > 0)
            {
                if ( bRange = ParseRange( sRange, llSize, &llStart, &llEnd ) )
                {
                    m_nResponseStatus = 206;
                    m_mapRespHeaders[ "Content-Range" ] = QString("bytes %1-%2/%3")
                                                                  .arg( llStart )
                                                                  .arg( llEnd   )
                                                                  .arg( llSize  );
                    llSize = (llEnd - llStart) + 1;
                }
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
        m_nResponseStatus = 404;

    // -=>TODO: Should set "Content-Length: *" if file is still recording

    // ----------------------------------------------------------------------
    // Write out Header.
    // ----------------------------------------------------------------------

    sHeader  = BuildHeader( llSize ).utf8();
    nBytes = WriteBlockDirect( sHeader.data(), sHeader.length() );

    // ----------------------------------------------------------------------
    // Write out File.
    // ----------------------------------------------------------------------

    if (( m_eType != RequestTypeHead ) && (llSize != 0))
    {
        __off64_t offset = llStart;
        int       file   = open( sFileName.ascii(), O_RDONLY | O_LARGEFILE );
        ssize_t   sent   = 0;  

        do 
        {  
            // SSIZE_MAX should work in kernels 2.6.16 and later.  
            // The loop is needed in any case.  

            sent = sendfile64( getSocketHandle(), file, &offset, 
                                (size_t)(llSize > INT_MAX ? INT_MAX : llSize));  

            llSize  = llEnd - offset;  
        } 
        while (( sent >= 0 ) && ( llSize > 0 ));  

        if (sent < 0)
            nBytes = sent;

        close( file );
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

void HTTPRequest::FormatErrorReponse( long nCode, const QString &sDesc )
{
    m_eResponseType   = ResponseTypeXML;
    m_nResponseStatus = 500;

    m_response << "<?xml version=\"1.0\" encoding=\"utf-8\"?>";

    if (m_bSOAPRequest)
    {
        m_mapRespHeaders[ "EXT" ] = "";

        m_response <<  "<s:Fault>"
                       "<faultcode>s:Client</faultcode>"
                       "<faultstring>UPnPError</faultstring>";
    }

    m_response << "<detail>";

    if (m_bSOAPRequest)     
        m_response << "<UPnpError xmlns=\"urn:schemas-upnp-org:control-1-0\">";

    m_response << "<errorCode>" << nCode << "</errorCode>";
    m_response << "<errorDescription>" << sDesc << "</errorDescription>";

    if (m_bSOAPRequest)     
        m_response << "</UPnpError>";

    m_response << "</detail>";

    if (m_bSOAPRequest)
        m_response << "</s:Fault>" << SOAP_ENVELOPE_END;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HTTPRequest::FormatActionReponse( NameValueList *pArgs )
{
    m_eResponseType   = ResponseTypeXML;
    m_nResponseStatus = 200;

    m_response << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n";

    if (m_bSOAPRequest)
    {
        m_mapRespHeaders[ "EXT" ] = "";

        m_response << SOAP_ENVELOPE_BEGIN 
                   << "<u:" << m_sMethod << "Response xmlns:u=\"" << m_sNameSpace << "\">\r\n";
    }
    else
        m_response << "<" << m_sMethod << "Response>\r\n";

    for (NameValue *pNV = pArgs->first(); pNV != NULL; pNV = pArgs->next())
    {                                                               
        m_response << "<" << pNV->sName << ">";
        m_response << pNV->sValue;
        m_response << "</" << pNV->sName << ">\r\n";
    }

    if (m_bSOAPRequest) 
    { 
        m_response << "</u:" << m_sMethod << "Response>\r\n"
                   << SOAP_ENVELOPE_END;
    }
    else
        m_response << "</" << m_sMethod << "Response>\r\n";
}

/////////////////////////////////////////////////////////////////////////////
//                  
/////////////////////////////////////////////////////////////////////////////

void HTTPRequest::FormatFileResponse( const QString &sFileName )
{
    m_eResponseType   = ResponseTypeHTML;
    m_nResponseStatus = 404;

    m_sFileName = sFileName;

    if (QFile::exists( m_sFileName ))
    {

        m_eResponseType                     = ResponseTypeFile;
        m_nResponseStatus                   = 200;
        m_mapRespHeaders[ "Cache-Control" ] = "no-cache=\"Ext\", max-age = 5000";
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void HTTPRequest::SetRequestProtocol( const QString &sLine )
{
    m_sProtocol      = sLine.section( '/', 0, 0 ).stripWhiteSpace();
    QString sVersion = sLine.section( '/', 1    ).stripWhiteSpace();

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

        if ( sFileExtension.upper() == ext.upper() )
            return( g_MIMETypes[i].pszType );
    }

    return( "text/plain" );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

long HTTPRequest::GetParameters( QString sParams, QStringMap &mapParams  )
{
    long nCount = 0;

    sParams.replace( "%26", "&" );

    if (sParams.length() > 0)
    { 
        QStringList params = QStringList::split( "&", sParams );
    
        for ( QStringList::Iterator it  = params.begin(); 
                                    it != params.end();  ++it ) 
        {
            QString sName  = (*it).section( '=', 0, 0 );
            QString sValue = (*it).section( '=', 1 );

            if ((sName.length() != 0) && (sValue.length() !=0))
            {
                QUrl::decode( sName  );
                QUrl::decode( sValue );

                mapParams.insert( sName.stripWhiteSpace(), sValue );
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
    QStringMap::iterator it = m_mapHeaders.find( sKey.lower() );

    if ( it == m_mapHeaders.end())
        return( sDefault );

    return( it.data() );
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
        sHeader += it.data() + "\r\n";
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

    QString sConnection = GetHeaderValue( "connection", "default" ).lower();

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
                QString sName  = sLine.section( ':', 0, 0 ).stripWhiteSpace();
                QString sValue = sLine.section( ':', 1 );

                sValue.truncate( sValue.length() - 2 );

                if ((sName.length() != 0) && (sValue.length() !=0))
                    m_mapHeaders.insert( sName.lower(), sValue.stripWhiteSpace()  );

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

            delete pszPayload;
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
    QStringList tokens = QStringList::split(QRegExp("[ \r\n][ \r\n]*"), sLine );
    int         nCount = tokens.count();

    // ----------------------------------------------------------------------

    if ( sLine.startsWith( "HTTP/" )) 
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
            SetRequestType( tokens[0].stripWhiteSpace() );

        if (nCount > 1)
        {
            m_sBaseUrl = tokens[1].section( '?', 0, 0).stripWhiteSpace();

            // Process any Query String Parameters

            QString sQueryStr = tokens[1].section( '?', 1, 1   );

            if (sQueryStr.length() > 0)
                GetParameters( sQueryStr, m_mapParams );

        }

        if (nCount > 2)
            SetRequestProtocol( tokens[2].stripWhiteSpace() );
    }
    else
    {
        // ------------------------------------------------------------------
        // Process as a Response
        // ------------------------------------------------------------------

        if (nCount > 0)
            SetRequestProtocol( tokens[0].stripWhiteSpace() );

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

    int nIdx = sRange.find( QRegExp( "(\\d|\\-)") );

    if (nIdx < 0)
        return false;

    if (nIdx > 0)
        sRange.remove( 0, nIdx );

    // ----------------------------------------------------------------------        
    // Split multiple ranges
    // ----------------------------------------------------------------------        

    QStringList ranges = QStringList::split( ",", sRange );

    if (ranges.count() == 0)
        return false;

    // ----------------------------------------------------------------------        
    // Split first range into its components
    // ----------------------------------------------------------------------        

    QStringList parts = QStringList::split( "-", ranges[0], true );

    if (parts.count() != 2) 
        return false;

    if (parts[0].isNull() && parts[1].isNull())
        return false;

    // ----------------------------------------------------------------------        
    // 
    // ----------------------------------------------------------------------        

    if (parts[0].isNull())
    {
        // ------------------------------------------------------------------
        // Does it match "-####"
        // ------------------------------------------------------------------

        long long llValue = strtoll( parts[1], NULL, 10 );

        *pllStart = llSize - llValue;
        *pllEnd   = llSize - 1;
    }
    else if (parts[1].isNull())
    {
        // ------------------------------------------------------------------
        // Does it match "####-"
        // ------------------------------------------------------------------

        *pllStart = strtoll( parts[0], NULL, 10 );

        if (*pllStart == 0)
            return false;

        *pllEnd   = llSize - 1;
    }
    else
    {
        // ------------------------------------------------------------------
        // Must be  "####-####"
        // ------------------------------------------------------------------

        *pllStart = strtoll( parts[0], NULL, 10 );
        *pllEnd   = strtoll( parts[1], NULL, 10 );

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
    QStringList sList = QStringList::split( "/", m_sBaseUrl, false );
    
    m_sMethod = "";

    if (sList.size() > 0)
    {
        m_sMethod = sList.last();
        sList.pop_back();
    }

    m_sBaseUrl = "/" + sList.join( "/" ); 
    //VERBOSE(VB_UPNP, QString("ExtractMethodFromURL : %1 : ").arg(m_sMethod));
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

//    VERBOSE(VB_UPNP, QString("HTTPRequest::ProcessSOAPPayload : %1 : ").arg(sSOAPAction));
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

    m_sNameSpace    = sSOAPAction.section( "#", 0, 0).remove( 0, 1);
    m_sMethod       = sSOAPAction.section( "#", 1 );
    m_sMethod.remove( m_sMethod.length()-1, 1 );

    QDomNodeList oNodeList = doc.elementsByTagNameNS( m_sNameSpace, m_sMethod );

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
                     
                    QUrl::decode( sName  );
                    QUrl::decode( sValue );

                    m_mapParams.insert( sName.stripWhiteSpace(), sValue );
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

QString &HTTPRequest::Encode( QString &sStr )
{
    sStr.replace(QRegExp( "&"), "&amp;" ); // This _must_ come first
    sStr.replace(QRegExp( "<"), "&lt;"  );
    sStr.replace(QRegExp( ">"), "&gt;"  );
    sStr.replace(QRegExp("\""), "&quot;");
    sStr.replace(QRegExp( "'"), "&apos;");

    return( sStr );
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

Q_LONG BufferedSocketDeviceRequest::BytesAvailable()
{
    if (m_pSocket)
        return( m_pSocket->BytesAvailable() );

    return( 0 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_ULONG BufferedSocketDeviceRequest::WaitForMore( int msecs, bool *timeout )
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

Q_LONG  BufferedSocketDeviceRequest::ReadBlock( char *pData, Q_ULONG nMaxLen, int msecs )
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

Q_LONG BufferedSocketDeviceRequest::WriteBlock( char *pData, Q_ULONG nLen )
{
    if (m_pSocket)
        return( m_pSocket->WriteBlock( pData, nLen ));

    return( -1 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_LONG BufferedSocketDeviceRequest::WriteBlockDirect( char *pData, Q_ULONG nLen )
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
