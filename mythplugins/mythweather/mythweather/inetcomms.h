#ifndef INETCOMMS_H_
#define INETCOMMS_H_

#include <qurloperator.h>
#include <qfile.h>
#include <qurl.h>
#include <qobject.h>

class INETComms : public QObject
{
    Q_OBJECT
  public:
    INETComms(QUrl &url);
    virtual ~INETComms() {}

    bool isDone(void) { return m_done; }

    QString getData(void) { return m_data; }
    void stop();
    
  private slots:
    void newData(const QByteArray &ba, QNetworkOperation *result);
    void finished(QNetworkOperation *result);

  private:
    QUrlOperator op;
    bool m_done;
    QString m_data;
};

#endif

