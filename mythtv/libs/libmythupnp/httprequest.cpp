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
#ifdef CONFIG_DARWIN
#include "darwin-sendfile.h"
#else
#include <sys/sendfile.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

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
    { "mpeg", "video/mpeg"                 }
};

static const int     g_nMIMELength    = sizeof( g_MIMETypes) / sizeof( MIMETypes );

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
                             m_response       ( m_aBuffer, IO_WriteOnly )
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
    if (sType == "GET"     )    return( m_eType = RequestTypeGet      );
    if (sType == "HEAD"    )    return( m_eType = RequestTypeHead     );
    if (sType == "POST"    )    return( m_eType = RequestTypePost     );
    if (sType == "M-SEARCH")    return( m_eType = RequestTypeMSearch  );

    return( m_eType = RequestTypeUnknown);
}

/////////////////////////////////////////////////////////////////////////////
//  
/////////////////////////////////////////////////////////////////////////////

long HTTPRequest::SendResponse( void )
{
    long     nBytes    = 0;
    QCString sHeader;

    switch( m_eResponseType )
    {
        case ResponseTypeNone:
            return( 0 );

        case ResponseTypeFile:
            return( SendResponseFile( m_sFileName ));

        case ResponseTypeXML: 
        case ResponseTypeHTML:
        {
            QString sDate = QDateTime::currentDateTime()
                                        .toString( "d MMM yyyy hh:mm:ss" );  

            sHeader   = QString("HTTP/1.1 %1\r\n"
                                "DATE: %2\r\n" )
                                .arg( GetResponseStatus())
                                .arg( sDate ).utf8();

            QString sAddlHeaders = GetAdditionalHeaders();

            sHeader += sAddlHeaders.utf8();
            
            sHeader += QString( "Content-Type: %1\r\n"
                                "Content-Length: %2\r\n" )
                                .arg( GetResponseType()  )
                                .arg( m_aBuffer.size()   ).utf8();
            
             sHeader += QString("\r\n").utf8();

            //cout << "Response =====" << endl;
            //cout << sHeader;

            // Write out Header.

            nBytes = WriteBlock( sHeader.data(), sHeader.length() );

            break;
        }
        default:
            break;
    }

    // Write out Response buffer.

    if (( m_eType != RequestTypeHead ) && ( m_aBuffer.size() > 0 ))
        nBytes += WriteBlock( m_aBuffer.data(), m_aBuffer.size() );

    Flush();

    return( nBytes );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

long HTTPRequest::SendResponseFile( QString sFileName )
{
    QCString    sHeader,
                sContentType = "text/plain";
    long        nBytes  = 0;
    long long   llSize  = 0;
    long long   llStart = 0;
    long long   llEnd   = 0;

    if (QFile::exists( sFileName ))
    {
        QFileInfo info( sFileName );

        sContentType = GetMimeType( info.extension( FALSE ).lower() );

        // ------------------------------------------------------------------
        // Get File size
        // ------------------------------------------------------------------

        struct stat st;

        if (stat( sFileName.ascii(), &st ) == 0)
            llSize = st.st_size;

        m_nResponseStatus = 200;

        // ------------------------------------------------------------------
        // Process any Range Header
        // ------------------------------------------------------------------

        QString sRange = GetHeaderValue( "range", "" );

        bool bRange = false;

        if (sRange.length() > 0)
        {
            if ( bRange = ParseRange( sRange, llSize, &llStart, &llEnd ) )
            {
                // sContentType="video/x-msvideo";
                m_nResponseStatus = 206;
                m_mapRespHeaders[ "Content-Range" ] = QString("%1-%2/%3")
                                                              .arg( llStart )
                                                              .arg( llEnd   )
                                                              .arg( llSize  );
                llSize = (llEnd - llStart) + 1;
            }
        }
        
        if (bRange == false)
        {
            // DSM-?20 specific response headers

            m_mapRespHeaders[ "User-Agent"    ] = "redsonic";
        }

        // ------------------------------------------------------------------
        //
        // ------------------------------------------------------------------

    }
    else
        m_nResponseStatus = 404;

    sHeader   = QString("HTTP/%1.%2 %3\r\n"
                        "Content-Type: %4\r\n"
                        "Content-Length: %5\r\n" )
                        .arg( m_nMajor )
                        .arg( m_nMinor )
                        .arg( GetResponseStatus())
                        .arg( sContentType )
                        .arg( llSize       ).utf8();


    sHeader += GetAdditionalHeaders() + "\r\n";

    // Write out Header.

    nBytes = WriteBlock( sHeader.data(), sHeader.length() );

    Flush();

    // Write out File.

    if (( m_eType != RequestTypeHead ) && (llSize != 0))
    {
        __off64_t offset = llStart;
        int       file   = open( sFileName.ascii(), O_RDONLY | O_LARGEFILE );
        sendfile64( getSocketHandle(), file, &offset, llSize );

        close( file );
    }

    // -=>TODO: Only returns header length... 
    //          should we change to return total bytes?

    return( nBytes );   
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
        m_mapRespHeaders[ "SERVER" ] = QString( "%1, UPnP/1.0, MythTv %2" )
                                          .arg( UPnp::g_sPlatform )
                                          .arg( MYTH_BINARY_VERSION );

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
        m_mapRespHeaders[ "SERVER" ] = QString( "%1, UPnP/1.0, MythTv %2" )
                                          .arg( UPnp::g_sPlatform )
                                          .arg( MYTH_BINARY_VERSION );

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
    for (int i = 0; i < g_nMIMELength; i++)
    {
        if ( sFileExtension == g_MIMETypes[i].pszExtension )
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

            if ( ReadBlock( pszPayload, nPayloadSize, 5000 ) == nPayloadSize )
            {
                m_sPayload = QString::fromUtf8( pszPayload, nPayloadSize );

                // See if the payload is just data from a form post

                if ( m_eContentType == ContentType_Urlencoded)
                    GetParameters( m_sPayload, m_mapParams );
            }
            else
                bSuccess = false;

            delete pszPayload;
        }

        // Check to see if this is a SOAP encoded message

        QString sSOAPAction = GetHeaderValue( "SOAPACTION", "" );

        if (sSOAPAction.length() > 0)
            bSuccess = ProcessSOAPPayload( sSOAPAction );
        else
            ExtractMethodFromURL();
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

    QStringList tokens = QStringList::split(QRegExp("[ \r\n][ \r\n]*"), sLine );

    for (unsigned int nIdx = 0; nIdx < tokens.count(); nIdx++)
    {
        switch( nIdx )
        {
            case 0: 
            {
                SetRequestType( tokens[0].stripWhiteSpace()  ); 
                break;
            }

            case 1: 
            {
                m_sBaseUrl = tokens[1].section( '?', 0, 0).stripWhiteSpace();

                // Process any Query String Parameters

                QString sQueryStr = tokens[1].section( '?', 1, 1   );

                if (sQueryStr.length() > 0)
                    GetParameters( sQueryStr, m_mapParams );

                break;
            }

            case 2:
            {
                SetRequestProtocol( tokens[2].stripWhiteSpace() );
                break;
            }
        }
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

/*
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// 
// QSocketRequest Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

QSocketRequest::QSocketRequest( QSocket *pSocket )
{
    m_pSocket = pSocket;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_LONG QSocketRequest::BytesAvailable()
{
    if (m_pSocket)
        return( m_pSocket->bytesAvailable() );

    return( 0 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_ULONG QSocketRequest::WaitForMore( int msecs, bool *timeout )
{
    if (m_pSocket)
        return( m_pSocket->waitForMore( msecs, timeout ));

    return( 0 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString QSocketRequest::ReadLine( int msecs )
{
    QString sLine;

    if (m_pSocket)
    {
        if (CanReadLine())
            return( m_pSocket->readLine() );
        
        if (nTimeout != 0)
            m_pSocket->waitForMore( msecs, NULL );
            
        if (CanReadLine())
            return( m_pSocket->readLine() );
    }

    return( sLine );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_LONG  QSocketRequest::ReadBlock( char *pData, Q_ULONG nMaxLen, int msecs )
{
    if (m_pSocket)
    {
        if (msecs == 0)
            return( m_pSocket->readBlock( pData, nMaxLen ));

        
    }

    return( -1 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_LONG  QSocketRequest::WriteBlock( char *pData, Q_ULONG nLen )
{
    if (m_pSocket)
        return( m_pSocket->writeBlock( pData, nLen ));

    return( -1 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString QSocketRequest::GetHostAddress()
{
    return( m_pSocket->address().toString() );    
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// QSocketDeviceRequest Class Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

QSocketDeviceRequest::QSocketDeviceRequest( QSocketDevice *pSocket, 
                                            QHostAddress  *pHost,
                                            Q_UINT16       nPort ) 
{
    m_pSocket = pSocket;
    m_pHost   = pHost;
    m_nPort   = nPort;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_LONG QSocketDeviceRequest::BytesAvailable()
{
    return( m_pSocket->bytesAvailable() );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_ULONG QSocketDeviceRequest::WaitForMore( int msecs, bool *timeout )
{
    if (m_pSocket)
        return( m_pSocket->waitForMore( msecs, timeout ));

    return( 0 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

bool QSocketDeviceRequest::CanReadLine()
{
    return( BytesAvailable() );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString QSocketDeviceRequest::ReadLine()
{
    char szLine[ 1024 ];
    
    m_pSocket->readLine( szLine, sizeof( szLine ));

    return( szLine );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_LONG  QSocketDeviceRequest::ReadBlock( char *pData, Q_ULONG nMaxLen )
{
    return( m_pSocket->readBlock( pData, nMaxLen ));

    return( -1 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Q_LONG  QSocketDeviceRequest::WriteBlock( char *pData, Q_ULONG nLen )
{
    if (m_pSocket)
    {
        if (m_pHost)
            return( m_pSocket->writeBlock( pData, nLen, *m_pHost, m_nPort ));
        else
            return( m_pSocket->writeBlock( pData, nLen ));
    }

    return( -1 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString QSocketDeviceRequest::GetHostAddress()
{
    return( m_pSocket->address().toString() );    
}
*/

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
    {

        if (m_pSocket->CanReadLine())
            return( m_pSocket->ReadLine() );
        
        // If the user supplied a timeout, lets loop until we can read a line 
        // or timeout.

        if ( msecs != 0)
        {
            bool bTimeout = false;

            while ( !m_pSocket->CanReadLine() && !bTimeout )
            {
                m_pSocket->WaitForMore( msecs, &bTimeout );
            }
            
            if (!bTimeout)
                sLine = m_pSocket->ReadLine();
        }
    }

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
        return( m_pSocket->WriteBlockDirect( pData, nLen ));
//        return( m_pSocket->WriteBlock( pData, nLen ));

    return( -1 );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QString BufferedSocketDeviceRequest::GetHostAddress()
{
    return( m_pSocket->SocketDevice()->address().toString() );    
}

