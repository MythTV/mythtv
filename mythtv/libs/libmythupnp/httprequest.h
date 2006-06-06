//////////////////////////////////////////////////////////////////////////////
// Program Name: httprequest.h
//                                                                            
// Purpose - Http Request/Response 
//                                                                            
// Created By  : David Blain	                Created On : Oct. 21, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef HTTPREQUEST_H_
#define HTTPREQUEST_H_

#include <iostream>
#include <qptrlist.h>

using namespace std;

#include <qsocket.h>
#include "bufferedsocketdevice.h"

#define SOAP_ENVELOPE_BEGIN  "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" " \
                             "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"     \
                             "<s:Body>"
#define SOAP_ENVELOPE_END    "</s:Body>\r\n</s:Envelope>";


/////////////////////////////////////////////////////////////////////////////
// Typedefs / Defines
/////////////////////////////////////////////////////////////////////////////

typedef enum 
{
    RequestTypeUnknown  = 0x0000,
    RequestTypeGet      = 0x0001,
    RequestTypeHead     = 0x0002,
    RequestTypePost     = 0x0004,
    RequestTypeMSearch  = 0x0008

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
    ResponseTypeFile     =  3

} ResponseType;


typedef QMap< QString, QString >    QStringMap;

typedef struct
{
    char *pszExtension;
    char *pszType;

} MIMETypes;

/////////////////////////////////////////////////////////////////////////////

typedef struct _NameValue
{   
    QString sName;
    QString sValue;

    _NameValue( const QString &name, const QString value ) 
        : sName( name ), sValue( value ) { }

} NameValue;

class NameValueList : public QPtrList< NameValue > 
{
    public:

        NameValueList()
        {
            setAutoDelete( true );
        }       
};

/////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////

class HTTPRequest
{
    protected:

        static const char  *m_szServerHeaders;


        QByteArray          m_aBuffer;


    public:    
        
        RequestType         m_eType;
        ContentType         m_eContentType;

        QString             m_sRawRequest;

        QString             m_sBaseUrl;
        QString             m_sMethod;

        QStringMap          m_mapParams;
        QStringMap          m_mapHeaders;

        QString             m_sPayload;

        QString             m_sProtocol;
        int                 m_nMajor;
        int                 m_nMinor;


        bool                m_bSOAPRequest;
        QString             m_sNameSpace;

        // Reponse

        ResponseType        m_eResponseType;
        long                m_nResponseStatus;
        QStringMap          m_mapRespHeaders;

        QString             m_sFileName;

        QTextStream         m_response;

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

    public:
        
                        HTTPRequest     ();
        virtual        ~HTTPRequest     () {};

        void            Reset           ();

        bool            ParseRequest    ();

        void            FormatErrorReponse ( long nCode, const QString &sDesc );
        void            FormatActionReponse( NameValueList *pArgs );

        long            SendResponse    ( void );
        long            SendResponseFile( QString sFileName );

        static QString &Encode          ( QString &sStr );

        QString         GetHeaderValue  ( const QString &sKey, QString sDefault );

        bool            GetKeepAlive    ();

        static QString  GetMimeType     ( const QString &sFileExtension );
        static long     GetParameters   ( QString  sParams, QStringMap &mapParams );

        // ------------------------------------------------------------------

        virtual Q_LONG  BytesAvailable  () = 0;
        virtual Q_ULONG WaitForMore     ( int msecs, bool *timeout = NULL ) = 0;
        virtual bool    CanReadLine     () = 0;
        virtual QString ReadLine        ( int msecs = 0 ) = 0;
        virtual Q_LONG  ReadBlock       ( char *pData, Q_ULONG nMaxLen, int msecs = 0 ) = 0;
        virtual Q_LONG  WriteBlock      ( char *pData, Q_ULONG nLen    ) = 0;
        virtual QString GetHostAddress  () = 0;
        virtual void    Flush           () = 0;
        virtual bool    IsValid         () = 0;
        virtual int     getSocketHandle () = 0;
};

/*
/////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////

class QSocketRequest : public HTTPRequest
{
    public:    
        
        QSocket            *m_pSocket;

    public:
        
                        QSocketRequest     ( QSocket *pSocket );
        virtual        ~QSocketRequest     () {};

        virtual Q_LONG  BytesAvailable  ();
        virtual Q_ULONG WaitForMore     ( int msecs, bool *timeout = NULL );
        virtual bool    CanReadLine     ();
        virtual QString ReadLine        ();
        virtual Q_LONG  ReadBlock       ( char *pData, Q_ULONG nMaxLen );
        virtual Q_LONG  WriteBlock      ( char *pData, Q_ULONG nLen    );
        virtual QString GetHostAddress  ();
        virtual void    Flush           () {m_pSocket->flush(); }
        virtual bool    IsValid         () {return( m_pSocket->state() == QSocket::Connected ); }
        virtual int     getSocketHandle () {return( m_pSocket->socket() ); }

};

/////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////

class QSocketDeviceRequest : public HTTPRequest
{
    public:    

        QSocketDevice      *m_pSocket;
        QHostAddress       *m_pHost;
        Q_UINT16            m_nPort;

    public:
        
                 QSocketDeviceRequest   ( QSocketDevice *pSocket, 
                                          QHostAddress  *pHost,
                                          Q_UINT16       nPort );
        virtual ~QSocketDeviceRequest   () {};

        virtual Q_LONG  BytesAvailable  ();
        virtual Q_ULONG WaitForMore     ( int msecs, bool *timeout = NULL );
        virtual bool    CanReadLine     ();
        virtual QString ReadLine        ();
        virtual Q_LONG  ReadBlock       ( char *pData, Q_ULONG nMaxLen );
        virtual Q_LONG  WriteBlock      ( char *pData, Q_ULONG nLen    );
        virtual QString GetHostAddress  ();
        virtual void    Flush           () { }
        virtual bool    IsValid         () {return( m_pSocket->isValid() ); }
        virtual int     getSocketHandle () {return( m_pSocket->socket() ); }
};

*/

/////////////////////////////////////////////////////////////////////////////
// 
/////////////////////////////////////////////////////////////////////////////

class BufferedSocketDeviceRequest : public HTTPRequest
{
    public:    

        BufferedSocketDevice    *m_pSocket;

    public:
        
                 BufferedSocketDeviceRequest( BufferedSocketDevice *pSocket );
        virtual ~BufferedSocketDeviceRequest() {};

        virtual Q_LONG  BytesAvailable  ();
        virtual Q_ULONG WaitForMore     ( int msecs, bool *timeout = NULL );
        virtual bool    CanReadLine     ();
        virtual QString ReadLine        ( int msecs = 0 );
        virtual Q_LONG  ReadBlock       ( char *pData, Q_ULONG nMaxLen, int msecs = 0  );
        virtual Q_LONG  WriteBlock      ( char *pData, Q_ULONG nLen    );
        virtual QString GetHostAddress  ();
        virtual void    Flush           () { m_pSocket->Flush(); }
        virtual bool    IsValid         () {return( m_pSocket->IsValid() ); }
        virtual int     getSocketHandle () {return( m_pSocket->socket() ); }
};

#endif
