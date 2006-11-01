// -*- Mode: c++ -*-

#ifndef _IPTV_URL_FETCHER_H_
#define _IPTV_URL_FETCHER_H_

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <qnetworkprotocol.h>
#include <qcstring.h>
#include <qstring.h>

class QNetworkOperation;
class QUrlOperator;

class URLFetcher : public QObject
{
    Q_OBJECT

  private:
    URLFetcher(const QString &url);
    ~URLFetcher() {}

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
    vector<unsigned char>    buf;
};

#endif // _IPTV_URL_FETCHER_H_
