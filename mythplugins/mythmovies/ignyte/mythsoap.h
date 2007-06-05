#ifndef MYTHSOAP_H_
#define MYTHSOAP_H_

#include <qhttp.h>
#include <qstring.h>

class MythSoap : public QObject
{
    Q_OBJECT
    public:
        void doSoapRequest(QString, QString, QString, QString);
        QByteArray getResponseData();
        bool isDone();
        bool hasError();
        MythSoap();

    private:
        QHttp http;
        bool  m_done;
        bool m_error;
        QByteArray m_data;

    public slots:
        void httpDone(bool);
};

#endif
