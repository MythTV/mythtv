#ifndef MYTHSOAP_H_
#define MYTHSOAP_H_

#include <q3http.h>
#include <qstring.h>

class MythSoap : public QObject
{
    Q_OBJECT
    public:
        void doSoapRequest(QString, QString, QString, QString);
        QByteArray getResponseData();
        bool isDone();
        bool hasError();
        inline QString getError() const { return http.errorString(); }
        MythSoap();

    private:
        Q3Http http;
        bool  m_done;
        bool m_error;
        QByteArray m_data;

    public slots:
        void httpDone(bool);
};

#endif
