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
    HttpComms(QUrl &url);
    HttpComms(QUrl &url, int timeoutms);
    HttpComms(QUrl &url, QHttpRequestHeader &header);

    virtual ~HttpComms();

    bool isDone(void) { return m_done; }
    
    int getStatusCode(void) { return m_statusCode; }
    QString getResponseReason(void) { return m_responseReason; }

    QString getData(void) { return QString(m_data); }
    QByteArray getRawData(void) { return m_data; }

    QString getRedirectedURL(void)  { return m_redirectedURL; }

    void stop();

    bool isTimedout(void) { return m_timeout; }

    static QString getHttp(QString& url, int timeoutMS = 10000, 
                           int maxRetries = 3, int maxRedirects = 3);
    static bool getHttpFile(QString& file, QString& url, int timeoutMS = 10000,
                           int maxRetries = 3, int maxRedirects = 3);
  protected:
    void init(QUrl &url);
    void init(QUrl &url, QHttpRequestHeader &header);
    
  private slots:
    void timeout();
    void done(bool error);
    void stateChanged(int state);

    void headerReceived(const QHttpResponseHeader &resp);

  private:
    int m_statusCode;
    QString m_redirectedURL;
    QString m_responseReason;
    QHttp *http;
    bool m_done;
    QByteArray m_data;
    QString m_url;
    QTimer* m_timer;
    bool m_timeout;
    int  m_debug;
};

#endif

