#ifndef HTTPCOMMS_H_
#define HTTPCOMMS_H_

#include <qhttp.h>
#include <qfile.h>
#include <qurl.h>
#include <qobject.h>

#if (QT_VERSION < 0x030100)
#define ANCIENT_QT
#warning You really should upgrade to qt 3.1.
#endif

class HttpComms : public QObject
{
    Q_OBJECT
  public:
    HttpComms(QUrl &url);
#ifndef ANCIENT_QT
    HttpComms(QUrl &url, QHttpRequestHeader &header);
#endif
    virtual ~HttpComms();

#ifndef ANCIENT_QT
    bool isDone(void) { return m_done; }
#else
    bool isDone(void) { return true; }
#endif
    
    QString getData(void) { return m_data; }
    void stop();

  protected:
    void init(QUrl &url);
#ifndef ANCIENT_QT
    void init(QUrl &url, QHttpRequestHeader &header);
#endif
    
  private slots:
    void done(bool error);
    void stateChanged ( int state );

  private:
    QHttp *http;
    bool m_done;
    QString m_data;
};

#endif

