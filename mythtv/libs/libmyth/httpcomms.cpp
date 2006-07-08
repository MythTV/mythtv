#include <iostream>
#include <qapplication.h>
#include <qregexp.h>
#include <unistd.h>

#include "mythcontext.h"

using namespace std;

#include "qmdcodec.h"
#include "httpcomms.h"

HttpComms::HttpComms()
         : http(0)
{
    init();
}


HttpComms::HttpComms(QUrl &url, int timeoutms)
         : http(0)
{
    init();
    request(url, timeoutms);
}

HttpComms::HttpComms(QUrl &url, QHttpRequestHeader &header, int timeoutms)
{
    init();
    request(url, header, timeoutms);
}

HttpComms::~HttpComms()
{
    if (m_timer)
        delete m_timer;

    delete http;
}

void HttpComms::init()
{
 //   m_curRequest = NULL;
    m_authNeeded = false;
    http = new QHttp();
    m_redirectedURL = "";
    m_done = false;
    m_statusCode = 0;
    m_responseReason = "";
    m_timer = NULL;
    m_timeout = false;
    

    connect(http, SIGNAL(done(bool)), this, SLOT(done(bool)));
    connect(http, SIGNAL(stateChanged(int)), this, SLOT(stateChanged(int)));
    connect(http, SIGNAL(responseHeaderReceived(const QHttpResponseHeader &)),
            this, SLOT(headerReceived(const QHttpResponseHeader &)));

}

void HttpComms::request(QUrl &url, int timeoutms, bool allowGzip)
{
    QHttpRequestHeader header("GET", url.encodedPathAndQuery());
    QString userAgent = "Mozilla/9.876 (X11; U; Linux 2.2.12-20 i686, en) "
                        "Gecko/25250101 Netscape/5.432b1";

    header.setValue("Host", url.host());
    header.setValue("User-Agent", userAgent);
    
    if (allowGzip)
        header.setValue( "Accept-Encoding", "gzip");
    
    request(url, header, timeoutms);
}



void HttpComms::request(QUrl &url, QHttpRequestHeader &header, int timeoutms)
{
    Q_UINT16 port = 80;

    if (url.hasPort()) 
        port = url.port();
    
    http->setHost(url.host(), port);

    m_url = url.toString();
    m_curRequest = header;

    if (m_timer)
        m_timer->stop();
    
    if (timeoutms > 0 )
    {
        if (!m_timer)
        {
            m_timer = new QTimer();
            connect(m_timer, SIGNAL(timeout()), SLOT(timeout()));
        }
        m_timeoutInterval = timeoutms;
        m_timer->start(timeoutms, TRUE);
    }        

    if (m_cookie)    
    {
        header.setValue("Cookie", m_cookie);
    }
    
    http->request(header);
}

void HttpComms::stop()
{
    disconnect(http, 0, 0, 0);
    http->abort();

    if (m_timer)
        m_timer->stop();
}

void HttpComms::done(bool error)
{
    if (error)
    {
       VERBOSE(VB_IMPORTANT, QString("HttpComms::done() - NetworkOperation Error on Finish: "
                                     "%1 (%2): url: '%3'")
                                     .arg(http->errorString())
                                     .arg(error)
                                     .arg(m_url.toString().latin1()));
    }
    else if (m_authNeeded)
    {
        VERBOSE(VB_NETWORK, QString("Authentication pending, ignoring done from first request."));
        return;            
    }
    else if (http->bytesAvailable())
    {
        m_data.resize(http->bytesAvailable());
        m_data = http->readAll();
    }
    
    VERBOSE(VB_NETWORK, QString("done: %1 bytes").arg(m_data.size()));

    if (m_timer)
        m_timer->stop();

    m_done = true;
}

void HttpComms::stateChanged(int state)
{
    QString stateStr;
    
    switch (state) 
    {
        case QHttp::Unconnected: stateStr = "unconnected"; break;
        case QHttp::HostLookup: stateStr =  "host lookup"; break;
        case QHttp::Connecting: stateStr = "connecting"; break;
        case QHttp::Sending: stateStr = "sending"; break;
        case QHttp::Reading: stateStr = "reading"; break;
        case QHttp::Connected: stateStr =  "connected"; break;
        case QHttp::Closing: stateStr = "closing"; break;
        default: stateStr =  "unknown state: "; break;
    }

    VERBOSE(VB_NETWORK, QString("HttpComms::stateChanged: %1 (%2)")
                                .arg(stateStr)
                                .arg(state));
}

void HttpComms::headerReceived(const QHttpResponseHeader &resp)
{
    m_statusCode = resp.statusCode();
    m_responseReason = resp.reasonPhrase();

    QString sidkey = "set-cookie";

    if (resp.hasKey(sidkey))
    {
        QRegExp rx("PHPSESSID=(.+);");
        rx.setMinimal(true);
        rx.setCaseSensitive(false);
        if (rx.search(resp.value(sidkey)) >= 0)
        {
            m_cookie = "PHPSESSID=" + rx.cap(1);
            VERBOSE(VB_NETWORK, QString("HttpComms found cookie: %1").arg(m_cookie));
        }
    }
    
    
    VERBOSE(VB_NETWORK, QString("Got HTTP response: %1:%2")
                        .arg(m_statusCode)
                        .arg(m_responseReason));    
    VERBOSE(VB_NETWORK, QString("Keys: %1")
                        .arg(resp.keys().join(",") ));
    

    if (resp.statusCode() >= 300 && resp.statusCode() <= 400) 
    {
        // redirection
        QString uri = resp.value("LOCATION");
        VERBOSE(VB_NETWORK, QString("Redirection to: '%1'").arg(uri));
       
        m_redirectedURL = resp.value("LOCATION");
        m_authNeeded = false;
    }
    else if ((resp.statusCode() == 401))
    {
        // Toggle the sate of our authentication pending flag
        // if we've gotten this after having tried to authenticate
        // once then we've failed authentication and turning the pending off will allow us to exit.
        m_authNeeded = !m_authNeeded;
        if (m_authNeeded)
        {
            QString authHeader(resp.value("www-authenticate"));
            
            if (authHeader.startsWith("Digest") )
            {            
                if (!createDigestAuth(false, authHeader, &m_curRequest) )
                {
                    m_authNeeded = false;
                    return;   
                }
            }
            else
            {
                QCString auth = QCodecs::base64Encode( QCString( m_webCredentials.user +
                                                       ":" + m_webCredentials.pass ) );
                m_curRequest.setValue( "Authorization", QString( "Basic " ).append( auth.data() ) ); 
            } 
            
            if (m_timer)
            {
                m_timer->stop();
                m_timer->start(m_timeoutInterval, TRUE);
            }
            
            // Not sure if it's possible to receive a session ID or other cookie
            // before authenticating or not.
            if (m_cookie)    
            {
                m_curRequest.setValue("Cookie", m_cookie);
            }
            
            http->request(m_curRequest);
        }
    }
    else
    {
        m_authNeeded = false;
    }
}

void HttpComms::timeout() 
{
   VERBOSE(VB_IMPORTANT, QString("HttpComms::Timeout for url: %1")
                               .arg(m_url.toString().latin1()));
   m_timeout = true;
   m_done = true;
}


/** \fn HttpComms::getHttp(QString&,int,int,int,bool,Credentials*,bool)
 *  \brief Static function for grabbing http data for a url.
 *
 *   This is a synchronous function, it will block according to the vars.
 */
QString HttpComms::getHttp(QString     &url,
                           int          timeoutMS,    int  maxRetries, 
                           int          maxRedirects, bool allowGzip,
                           Credentials *webCred,      bool isInQtEventThread)
{
    int redirectCount = 0;
    int timeoutCount = 0;
    QString res = "";
    HttpComms *httpGrabber = NULL; 
    QString hostname = "";

    while (1) 
    {
        QUrl qurl(url);
        if (hostname == "")
            hostname = qurl.host();  // hold onto original host
        if (!qurl.hasHost())        // can occur on redirects to partial paths
            qurl.setHost(hostname);
        
        VERBOSE(VB_NETWORK, QString("getHttp: grabbing: %1").arg(qurl.toString()));

        if (httpGrabber != NULL)
            delete httpGrabber; 
        
        httpGrabber = new HttpComms;
        
        if (webCred)
            httpGrabber->setCredentials(*webCred, CRED_WEB);
            
        httpGrabber->request(qurl, timeoutMS, allowGzip);            
       

        while (!httpGrabber->isDone())
        {
            if (isInQtEventThread)
                qApp->processEvents();
            usleep(10000);
        }

        // Handle timeout
        if (httpGrabber->isTimedout())
        {
            VERBOSE(VB_NETWORK, QString("timeout for url: %1").arg(url.latin1()));
           
            // Increment the counter and check were not over the limit
            if (timeoutCount++ >= maxRetries)
            {
                VERBOSE(VB_IMPORTANT, QString("Failed to contact server for url: %1").arg(url.latin1()));
                break;
            }

            // Try again
            VERBOSE(VB_NETWORK, QString("Attempt # %1/%2 for url: %3")
                                        .arg(timeoutCount + 1)
                                        .arg(maxRetries)
                                        .arg(url.latin1()));

            continue;
        }

        // Check for redirection
        if (!httpGrabber->getRedirectedURL().isEmpty()) 
        {
            VERBOSE(VB_NETWORK, QString("Redirection: %1, count: %2, max: %3")
                                .arg(httpGrabber->getRedirectedURL().latin1())
                                .arg(redirectCount)
                                .arg(maxRedirects));
            if (redirectCount++ < maxRedirects)
                url = httpGrabber->getRedirectedURL();

            // Try again
            timeoutCount = 0;
            continue;
        }

        res = httpGrabber->getData();
        break;
    }

    delete httpGrabber;

    
    VERBOSE(VB_NETWORK, QString("Got %1 bytes from url: '%2'")
                                .arg(res.length())
                                .arg(url.latin1()));
    VERBOSE(VB_NETWORK, res);

    return res;
}

// getHttpFile - static function for grabbing a file from an http url
//      this is a synchronous function, it will block according to the vars
bool HttpComms::getHttpFile(const QString& filename, QString& url, int timeoutMS, 
                            int maxRetries, int maxRedirects, 
                            bool allowGzip, Credentials* webCred)
{
    int redirectCount = 0;
    int timeoutCount = 0;
    QByteArray data(0);
    bool res = false;
    HttpComms *httpGrabber = NULL;
    QString hostname = "";

    while (1)
    {
        QUrl qurl(url);
        if (hostname == "")
            hostname = qurl.host();  // hold onto original host
        
        if (!qurl.hasHost())        // can occur on redirects to partial paths
            qurl.setHost(hostname);
        
        VERBOSE(VB_NETWORK, QString("getHttp: grabbing: '%1'")
                                    .arg(qurl.toString()));

        if (httpGrabber != NULL)
            delete httpGrabber;
        
        httpGrabber = new HttpComms;
        
        if (webCred)
            httpGrabber->setCredentials(*webCred, CRED_WEB);
            
        httpGrabber->request(qurl, timeoutMS, allowGzip);            

        while (!httpGrabber->isDone())
        {
            qApp->processEvents();
            usleep(10000);
        }

        // Handle timeout
        if (httpGrabber->isTimedout())
        {
            VERBOSE(VB_NETWORK, QString("Timeout for url: '%1'")
                                        .arg(url.latin1()));

            // Increment the counter and check were not over the limit
            if (timeoutCount++ >= maxRetries)
            {
                VERBOSE(VB_IMPORTANT, QString("Failed to contact server for url: '%1'")
                                              .arg(url.latin1()));
                break;
            }

            // Try again
            VERBOSE(VB_NETWORK, QString("Attempt # %1/%2 for url: %3")
                                        .arg(timeoutCount + 1)
                                        .arg(maxRetries)
                                        .arg(url.latin1()));

            continue;
        }

        // Check for redirection
        if (!httpGrabber->getRedirectedURL().isEmpty())
        {
            VERBOSE(VB_NETWORK, QString("redirection: '%1', count: %2, max: %3")
                                        .arg(httpGrabber->getRedirectedURL().latin1())
                                        .arg(redirectCount)
                                        .arg(maxRedirects));
            
            if (redirectCount++ < maxRedirects)
                url = httpGrabber->getRedirectedURL();

            // Try again
            timeoutCount = 0;
            continue;
        }

        data = httpGrabber->getRawData();

        if (data.size() > 0)
        {
            VERBOSE(VB_NETWORK, QString("getHttpFile: saving to file: '%1'")
                                        .arg(filename));

            QFile file(filename);
            if (file.open( IO_WriteOnly ))
            {
                QDataStream stream(& file);
                stream.writeRawBytes( (const char*) (data), data.size() );
                file.close();
                res = true;
                VERBOSE(VB_NETWORK, QString("getHttpFile: File saved OK"));
            }
            else
                VERBOSE(VB_NETWORK, QString("getHttpFile: Failed to open file for writing"));
        }
        else
           VERBOSE(VB_NETWORK, QString("getHttpFile: nothing to save to file!"));

        break;
    }

    VERBOSE(VB_NETWORK, QString("Got %1 bytes from url: '%2'")
                                .arg(data.size())
                                .arg(url.latin1()));

    delete httpGrabber;

    return res;
}



bool HttpComms::createDigestAuth ( bool isForProxy, const QString& authStr, QHttpRequestHeader* request)
{
    const char *p;
    QString header;
    QString auth;
    QCString opaque;
    QCString Response;

    DigestAuthInfo info;

    opaque = "";
      
    if ( isForProxy )
    {
        header = "Proxy-Authorization";
        auth = "Digest ";
        info.username = m_proxyCredentials.user.latin1();
        info.password = m_proxyCredentials.pass.latin1();
        p = authStr.latin1();
    }
    else
    {
        header = "Authorization";
        auth = "Digest ";
        info.username = m_webCredentials.user.latin1();
        info.password = m_webCredentials.pass.latin1();
        p = authStr.latin1();
    }
  
    if (!p || !*p)
        return false;

    p += 6; // Skip "Digest"

    if ( info.username.isEmpty() || info.password.isEmpty() || !p )
        return false;

  
    info.realm = "";
    info.algorithm = "MD5";
    info.nonce = "";
    info.qop = "";

    // cnonce is recommended to contain about 64 bits of entropy
    // TODO: Fix this
    info.cnonce =  "QPDExMTQ";
  
    // HACK: Should be fixed according to RFC 2617 section 3.2.2
    info.nc = "00000001";

    // Set the method used...
    info.method = request->method();
  
    // Parse the Digest response....
    while (*p)
    {
        int i = 0;
        while ( (*p == ' ') || (*p == ',') || (*p == '\t')) {p++;}
    
        if (strncasecmp(p, "realm=", 6 )==0)
        {
            p+=6;
            while ( *p == '"' ) p++;  // Go past any number of " mark(s) first
            while ( p[i] != '"' ) i++;  // Read everything until the last " mark
            info.realm = QCString( p, i+1 );
        }
        else if (strncasecmp(p, "algorith=", 9)==0)
        {
            p+=9;
            while ( *p == '"' ) p++;  // Go past any number of " mark(s) first
            while ( ( p[i] != '"' ) && ( p[i] != ',' ) && ( p[i] != '\0' ) ) i++;
            info.algorithm = QCString(p, i+1);
        }
        else if (strncasecmp(p, "algorithm=", 10)==0)
        {
            p+=10;
            while ( *p == '"' ) p++;  // Go past any " mark(s) first
            while ( ( p[i] != '"' ) && ( p[i] != ',' ) && ( p[i] != '\0' ) ) i++;
            info.algorithm = QCString(p,i+1);
        }
        else if (strncasecmp(p, "domain=", 7)==0)
        {
            p+=7;
            while ( *p == '"' ) p++;  // Go past any " mark(s) first
            while ( p[i] != '"' ) i++;  // Read everything until the last " mark
            int pos;
            int idx = 0;
            QCString uri = QCString(p,i+1);
            do
            {
                pos = uri.find( ' ', idx );
     
                if ( pos != -1 )
                {
                    QUrl u (m_url, uri.mid(idx, pos-idx));
                    if (u.isValid ())
                        info.digestURI.append( u.toString().latin1() );
                }
                else
                {
                    QUrl u (m_url, uri.mid(idx, uri.length()-idx));
                    if (u.isValid ())
                        info.digestURI.append( u.toString().latin1() );
                }
                idx = pos+1;
            } while ( pos != -1 );
        }
        else if (strncasecmp(p, "nonce=", 6)==0)
        {
            p+=6;
            while ( *p == '"' ) p++;  // Go past any " mark(s) first
            while ( p[i] != '"' ) i++;  // Read everything until the last " mark
            info.nonce = QCString(p,i+1);
        }
        else if (strncasecmp(p, "opaque=", 7)==0)
        {
            p+=7;
            while ( *p == '"' ) p++;  // Go past any " mark(s) first
            while ( p[i] != '"' ) i++;  // Read everything until the last " mark
            opaque = QCString(p,i+1);
        }
        else if (strncasecmp(p, "qop=", 4)==0)
        {
            p+=4;
            while ( *p == '"' ) p++;  // Go past any " mark(s) first
            while ( p[i] != '"' ) i++;  // Read everything until the last " mark
            info.qop = QCString(p,i+1);
        }
        p+=(i+1);
    }

    if (info.realm.isEmpty() || info.nonce.isEmpty())
        return false;
  
    info.digestURI.append (m_url.encodedPathAndQuery());
  
    
//     cerr << " RESULT OF PARSING:" << endl;
//     cerr << "   algorithm: " << info.algorithm << endl;
//     cerr << "   realm:     " << info.realm << endl;
//     cerr << "   nonce:     " << info.nonce << endl;
//     cerr << "   opaque:    " << opaque << endl;
//     cerr << "   qop:       " << info.qop << endl;
    
    
    // Calculate the response...
    calculateDigestResponse( info, Response );

    auth += "username=\"";
    auth += info.username;

    auth += "\", realm=\"";
    auth += info.realm;
    auth += "\"";

    auth += ", nonce=\"";
    auth += info.nonce;

    auth += "\", uri=\"";
    auth += m_url.encodedPathAndQuery();

    auth += "\", algorithm=\"";
    auth += info.algorithm;
    auth +="\"";

    if ( !info.qop.isEmpty() )
    {
        auth += ", qop=\"";
        auth += info.qop;
        auth += "\", cnonce=\"";
        auth += info.cnonce;
        auth += "\", nc=";
        auth += info.nc;
    }

    auth += ", response=\"";
    auth += Response;
    if ( !opaque.isEmpty() )
    {
        auth += "\", opaque=\"";
        auth += opaque;
    }
  
    auth += "\"";
    
    
    VERBOSE(VB_NETWORK, QString("Setting auth header %1 to '%2'")
                                .arg(header).arg(auth));

    
    if (request)
        request->setValue(header, auth);
        
    return true;
}


void HttpComms::calculateDigestResponse( DigestAuthInfo& info, QCString& Response )
{
    QMD5 md;
    QCString HA1;
    QCString HA2;

    // Calculate H(A1)
    QCString authStr = info.username;
    authStr += ':';
    authStr += info.realm;
    authStr += ':';
    authStr += info.password;
    md.update( authStr );

    if ( info.algorithm.lower() == "md5-sess" )
    {
        authStr = md.hexDigest();
        authStr += ':';
        authStr += info.nonce;
        authStr += ':';
        authStr += info.cnonce;
        md.reset();
        md.update( authStr );
    }
    
    HA1 = md.hexDigest();

    //cerr << " calculateResponse(): A1 => " << HA1 << endl;

    // Calculate H(A2)
    authStr = info.method;
    authStr += ':';
    authStr += m_url.encodedPathAndQuery().latin1();
    if ( info.qop == "auth-int" )
    {
        authStr += ':';
        authStr += info.entityBody;
    }
    
    md.reset();
    md.update( authStr );
    HA2 = md.hexDigest();

    //cerr << " calculateResponse(): A2 => " << HA2 << endl;

    // Calculate the response.
    authStr = HA1;
    authStr += ':';
    authStr += info.nonce;
    authStr += ':';
    if ( !info.qop.isEmpty() )
    {
        authStr += info.nc;
        authStr += ':';
        authStr += info.cnonce;
        authStr += ':';
        authStr += info.qop;
        authStr += ':';
    }
  
    authStr += HA2;
    md.reset();
    md.update( authStr );
    Response = md.hexDigest();

    //cerr << " calculateResponse(): Response => " << Response << endl;
}


