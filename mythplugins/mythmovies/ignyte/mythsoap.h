#ifndef MYTHSOAP_H_
#define MYTHSOAP_H_

#include <QString>
#include <QHttp>

class MythSoap : public QObject
{
    Q_OBJECT

  public:
    MythSoap(QObject *parent);

    void doSoapRequest(QString, QString, QString, QString);
    QByteArray getResponseData(void);
    bool isDone(void);
    bool hasError(void);
    inline QString getError(void) const { return http.errorString(); }

  private:
    ~MythSoap() {}

  private:
    QHttp http;
    bool  m_done;
    bool m_error;
    QByteArray m_data;

  public slots:
    void httpDone(bool);
};

#endif
