#ifndef HTTPCOMMS_H_
#define HTTPCOMMS_H_

#include <qhttp.h>
#include <qfile.h>
#include <qurl.h>
#include <qobject.h>

class HttpComms : public QObject
{
    Q_OBJECT
  public:
    HttpComms(QUrl &url);
    HttpComms(QUrl &url, QHttpRequestHeader &header);
    virtual ~HttpComms();

    bool isDone(void) { return m_done; }

    QString getData(void) { return m_data; }
    void stop();

  protected:
    void init(QUrl &url);
    void init(QUrl &url, QHttpRequestHeader &header);

    
  private slots:
    void done(bool error);
    void stateChanged ( int state );

  private:
    QHttp *http;
    bool m_done;
    QString m_data;
};

#endif

