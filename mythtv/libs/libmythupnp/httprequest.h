//////////////////////////////////////////////////////////////////////////////
// Program Name: httprequest.h
// Created     : Oct. 21, 2005
//
// Purpose     : Http Request/Response
//                                                                            
// Copyright (c) 2005 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef HTTPREQUEST_H_
#define HTTPREQUEST_H_

#include <QFile>
#include <QRegExp>
#include <QBuffer>
#include <QTextStream>
#include <QTcpSocket>

using namespace std;

#include "upnpexp.h"
#include "upnputil.h"
#include "serializers/serializer.h"

#define SOAP_ENVELOPE_BEGIN  "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" " \
                             "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"     \
                             "<s:Body>"
#define SOAP_ENVELOPE_END    "</s:Body>\r\n</s:Envelope>";


/////////////////////////////////////////////////////////////////////////////
// Typedefs / Defines
/////////////////////////////////////////////////////////////////////////////

typedef enum 
{
    RequestTypeUnknown      = 0x0000,
    RequestTypeGet          = 0x0001,
    RequestTypeHead         = 0x0002,
    RequestTypePost         = 0x0004,
    RequestTypeMSearch      = 0x0008,
    RequestTypeSubscribe    = 0x0010,
    RequestTypeUnsubscribe  = 0x0020,
    RequestTypeNotify       = 0x0040,
    RequestTypeResponse     = 0x0080

} RequestType;                

typedef enum 
{
    ContentType_Unknown    = 0,
    ContentType_Urlencoded = 1,
    ContentType_XML        = 2

} ContentType;

typedef enum 
{
    ResponseTypeNone     = -1,
    ResponseTypeUnknown  =  0,
    ResponseTypeXML      =  1,
    ResponseTypeHTML     =  2,
    ResponseTypeJS       =  3,
    ResponseTypeCSS      =  4,
    ResponseTypeText     =  5,
    ResponseTypeSVG      =  6,
    ResponseTypeFile     =  7,
    ResponseTypeOther    =  8

} ResponseType;


typedef struct
{
    const char *pszExtension;
    const char *pszType;

} MIMETypes;

/////////////////////////////////////////////////////////////////////////////

class IPostProcess
{
    public:
        virtual void ExecutePostProcess( ) = 0;
        virtual ~IPostProcess() {};
};

/////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC HTTPRequest
{
    protected:

        static const char  *m_szServerHeaders;

        QRegExp             m_procReqLineExp;
        QRegExp             m_parseRangeExp;

    public:    
        
        RequestType         m_eType;
        ContentType         m_eContentType;

        QString             m_sRawRequest;

        QString             m_sBaseUrl;
        QString             m_sResourceUrl;
        QString             m_sMethod;

        QStringMap          m_mapParams;
        QStringMap          m_mapHeaders;

        QString             m_sPayload;

        QString             m_sProtocol;
        int                 m_nMajor;
        int                 m_nMinor;

        bool                m_bProtected;

        bool                m_bSOAPRequest;
        QString             m_sNameSpace;

        // Response

        ResponseType        m_eResponseType;
        QString             m_sResponseTypeText;    // used for ResponseTypeOther

        long                m_nResponseStatus;
        QStringMap          m_mapRespHeaders;

        QString             m_sFileName;

        QBuffer             m_response;

        IPostProcess       *m_pPostProcess;

    private:

        bool                m_bKeepAlive;
        uint                m_nKeepAliveTimeout;

    protected:

        RequestType     SetRequestType      ( const QString &sType  );
        void            SetRequestProtocol  ( const QString &sLine  );
        ContentType     SetContentType      ( const QString &sType  );

        void            SetServerHeaders    ( void );

        void            ProcessRequestLine  ( const QString &sLine  );
        bool            ProcessSOAPPayload  ( const QString &sSOAPAction );
        void            ExtractMethodFromURL( );

        QString         GetResponseStatus   ( void );
        QString         GetResponseType     ( void );
        QString         GetAdditionalHeaders( void );

        bool            ParseRange          ( QString sRange, 
                                              long long   llSize, 
                                              long long *pllStart, 
                                              long long *pllEnd   );

        bool            ParseKeepAlive      ( void );

        QString         BuildResponseHeader ( long long nSize );

        qint64          SendData            ( QIODevice *pDevice, qint64 llStart, qint64 llBytes );
        qint64          SendFile            ( QFile &file, qint64 llStart, qint64 llBytes );

        bool            IsUrlProtected      ( const QString &sBaseUrl );
        bool            Authenticated       ();

    public:
        
                        HTTPRequest     ();
        virtual        ~HTTPRequest     () {};

        bool            ParseRequest    ();

        void            FormatErrorResponse ( bool  bServerError, 
                                              const QString &sFaultString, 
                                              const QString &sDetails );

        void            FormatActionResponse( Serializer *ser );
        void            FormatActionResponse( const NameValues &pArgs );
        void            FormatFileResponse  ( const QString &sFileName );
        void            FormatRawResponse   ( const QString &sXML );

        qint64          SendResponse    ( void );
        qint64          SendResponseFile( QString sFileName );

        QString         GetHeaderValue  ( const QString &sKey, QString sDefault );

        bool            GetKeepAlive () { return m_bKeepAlive; }

        Serializer *    GetSerializer   ();

        QString         GetRequestProtocol  () const;
        QString         GetResponseProtocol () const;

        static QString  GetMimeType     ( const QString &sFileExtension );
        static QStringList GetSupportedMimeTypes ();
        static QString  TestMimeType    ( const QString &sFileName );
        static long     GetParameters   ( QString  sParams, QStringMap &mapParams );
        static QString  Encode          ( const QString &sIn );
        static QString  Decode          ( const QString &sIn );
        static QString  GetETagHash     ( const QByteArray &data );

        void            SetKeepAliveTimeout ( int nTimeout ) { m_nKeepAliveTimeout = nTimeout; }

        // ------------------------------------------------------------------

        virtual QString ReadLine        ( int msecs ) = 0;
        virtual qint64  ReadBlock       ( char *pData, qint64 nMaxLen, int msecs = 0 ) = 0;
        virtual qint64  WriteBlock      ( const char *pData,
                                          qint64 nLen    ) = 0;
        virtual QString GetHostAddress  () = 0;
        virtual QString GetPeerAddress  () = 0;
        virtual int     getSocketHandle () = 0;
};

/////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////

class BufferedSocketDeviceRequest : public HTTPRequest
{
    public:    

        QTcpSocket    *m_pSocket;

    public:
        
                 BufferedSocketDeviceRequest( QTcpSocket *pSocket );
        virtual ~BufferedSocketDeviceRequest() {};

        virtual QString ReadLine        ( int msecs );
        virtual qint64  ReadBlock       ( char *pData, qint64 nMaxLen, int msecs = 0  );
        virtual qint64  WriteBlock      ( const char *pData, qint64 nLen    );
        virtual QString GetHostAddress  ();
        virtual QString GetPeerAddress  ();
        virtual int     getSocketHandle () {return( m_pSocket->socketDescriptor() ); }

};

/////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC HttpException
{
    public:
        int     code;
        QString msg;

        HttpException( int nCode = -1, const QString &sMsg = "") 
               : code( nCode ), msg ( sMsg  ) 
        {}

        // Needed to force a v-table.
        virtual ~HttpException() 
        {}
};

class UPNP_PUBLIC HttpRedirectException : public HttpException
{
    public:

        QString hostName;
      //int     port;

        HttpRedirectException( const QString &sHostName = "", 
                                     int      nCode     = -1, 
                               const QString &sMsg      = "" ) 
               : HttpException( nCode, sMsg ), hostName( sHostName )
        {}

        virtual ~HttpRedirectException() 
        {}

};

#endif
