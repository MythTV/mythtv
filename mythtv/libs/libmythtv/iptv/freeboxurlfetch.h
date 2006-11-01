// -*- Mode: c++ -*-

#ifndef _FREEBOX_URL_FETCHER_H_
#define _FREEBOX_URL_FETCHER_H_

// Qt headers
#include <qnetworkprotocol.h>
#include <qcstring.h>
#include <qstring.h>

class QNetworkOperation;
class QUrlOperator;

class FreeboxUrlFetch : public QObject
{
    Q_OBJECT

  private:
    FreeboxUrlFetch(const QString &url);
    ~FreeboxUrlFetch() {}

  public:
    static QString FetchData(const QString &url, bool inQtThread);

  public slots:
    void deleteLater(void);

  private slots:
    void Finished(QNetworkOperation *op);
    void Data(const QByteArray &data, QNetworkOperation *op);

  private:
    QUrlOperator            *op;
    QNetworkProtocol::State  state;
    QString                  str;
};

#endif // _FREEBOX_URL_FETCHER_H_
