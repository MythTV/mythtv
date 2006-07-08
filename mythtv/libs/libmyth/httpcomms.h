#ifndef HTTPCOMMS_H_
#define HTTPCOMMS_H_

#include <qhttp.h>
#include <qfile.h>
#include <qurl.h>
#include <qobject.h>
#include <qtimer.h>

class HttpComms : public QObject
{
    Q_OBJECT
  public:
    HttpComms();
    HttpComms(QUrl &url, int timeoutms = -1);
    HttpComms(QUrl &url, QHttpRequestHeader &header, int timeoutms = -1);
    virtual ~HttpComms();

    bool isDone(void) { return m_done; }
    
    int getStatusCode(void) { return m_statusCode; }
    QString getResponseReason(void) { return m_responseReason; }

    QString getData(void) { return QString(m_data); }
    QByteArray getRawData(void) { return m_data; }

    QString getRedirectedURL(void)  { return m_redirectedURL; }

    void stop();

    bool isTimedout(void) { return m_timeout; }

    class Credentials
    {
        public:
            Credentials( const QString& _user="", const QString& _pass="") { user = _user; pass = _pass; }
            QString user;
            QString pass;
    };                           
    
    enum CredentialTypes { CRED_WEB, CRED_PROXY };
    
    void setCredentials(const Credentials& cred, int credType)
    { 
        if (credType == CRED_PROXY)
            m_proxyCredentials = cred;
        else
            m_webCredentials = cred;
    }
    
    
    static QString getHttp(QString& url, int timeoutMS = 10000, 
                           int maxRetries = 3, int maxRedirects = 3,
                           bool allowGzip = false,
                           Credentials* webCred = NULL,
                           bool isInQtEventThread = true);
    
    static bool getHttpFile(const QString& file, QString& url, int timeoutMS = 10000,
                            int maxRetries = 3, int maxRedirects = 3, 
                            bool allowGzip = false, Credentials* webCred = NULL);
    
    
    void request(QUrl &url, int timeoutms = -1, bool allowGzip = false);
    void request(QUrl &url, QHttpRequestHeader &header, int timeoutms = -1);
    
    void setCookie( const QString& cookie ) { m_cookie = cookie; }
    const QString& getCookie() const { return m_cookie; }
        
  protected:
    struct DigestAuthInfo
    {
        QCString nc;
        QCString qop;
        QCString realm;
        QCString nonce;
        QCString method;
        QCString cnonce;
        QCString username;
        QCString password;
        QStrList digestURI;
        QCString algorithm;
        QCString entityBody;
    };
    
    void init();
    
    void calculateDigestResponse( DigestAuthInfo& info, QCString& Response );
    bool createDigestAuth( bool isForProxy, const QString& authStr, QHttpRequestHeader* request );
    
  private slots:
    void timeout();
    void done(bool error);
    void stateChanged(int state);

    void headerReceived(const QHttpResponseHeader &resp);

  private:
    int m_statusCode;
    QString m_redirectedURL;
    QString m_responseReason;
    Credentials m_webCredentials;
    Credentials m_proxyCredentials;
    QHttp *http;
    bool m_done;
    QByteArray m_data;
    QUrl m_url;
    QTimer* m_timer;
    bool m_timeout;
    bool m_authNeeded;
    int m_timeoutInterval;
    QString m_cookie;
    
    QHttpRequestHeader m_curRequest;
};

#endif

