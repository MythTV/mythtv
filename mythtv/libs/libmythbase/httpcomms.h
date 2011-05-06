#ifndef HTTPCOMMS_H_
#define HTTPCOMMS_H_

#include <QByteArray>
#include <QString>
#include <QObject>
#include <QHttp>
#include <QUrl>

#include "mythbaseexp.h"

class QTimer;
class MBASE_PUBLIC HttpComms : public QObject
{
    Q_OBJECT
  public:
    HttpComms();
    HttpComms(QUrl &url, int timeoutms = -1);
    HttpComms(QUrl &url, QHttpRequestHeader &header, int timeoutms = -1);
    virtual ~HttpComms();

    bool isDone(void) const { return m_done; }
    int getProgress(void) const { return m_progress; }
    int getTotal(void) const { return m_total; }

    int getStatusCode(void) const { return m_statusCode; }
    QString getResponseReason(void) const { return m_responseReason; }

    QString getData(void) const { return QString(m_data); }
    QByteArray getRawData(void) const { return m_data; }

    QString getRedirectedURL(void) const { return m_redirectedURL; }

    void stop();

    bool isTimedout(void) const { return m_timeout; }

    class Credentials
    {
      public:
        explicit Credentials(const QString &_user = "",
                             const QString &_pass = "")
            : user(_user), pass(_pass) { }

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
    
    static QString postHttp(
        QUrl               &url, 
        QHttpRequestHeader *pAddlHdr          = NULL, 
        QIODevice          *pData             = NULL,
        int                 timeoutMS         = 10000, 
        int                 maxRetries        = 3, 
        int                 maxRedirects      = 3, 
        bool                allowGzip         = false,
        Credentials        *webCred           = NULL, 
        bool                isInQtEventThread = true,
        QString             userAgent         = "<default>");

    void request(QUrl &url, int timeoutms = -1, bool allowGzip = false);
    void request(QUrl &url, QHttpRequestHeader &header, int timeoutms = -1, QIODevice *pData = NULL );
    
    void setCookie( const QString& cookie ) { m_cookie = cookie; }
    const QString& getCookie() const { return m_cookie; }
        
  protected:
    struct DigestAuthInfo
    {
        QByteArray nc;
        QByteArray qop;
        QByteArray realm;
        QByteArray nonce;
        QByteArray method;
        QByteArray cnonce;
        QByteArray username;
        QByteArray password;
        QStringList digestURI;
        QByteArray algorithm;
        QByteArray entityBody;
    };

    void init();

    void calculateDigestResponse( DigestAuthInfo& info, QByteArray& Response );
    bool createDigestAuth( bool isForProxy, const QString& authStr, QHttpRequestHeader* request );

  private slots:
    void timeout();
    void done(bool error);
    void stateChanged(int state);
    void dataReadProgress(int done, int total);
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
    int m_progress;
    int m_total;

    QHttpRequestHeader m_curRequest;
};

#endif

