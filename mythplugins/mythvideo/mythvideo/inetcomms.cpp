#include <iostream>
using namespace std;

#include "inetcomms.h"

INETComms::INETComms(QUrl &url)
{
    op = url;
    m_done = false;

    m_data = "";

    connect(&op, SIGNAL(data(const QByteArray &, QNetworkOperation *)),
            this, SLOT(newData(const QByteArray &, QNetworkOperation *)));
    connect(&op, SIGNAL(finished(QNetworkOperation *)),
            this, SLOT(finished(QNetworkOperation *)));

    op.get();
}

void INETComms::newData(const QByteArray &ba, QNetworkOperation *result)
{
    int ecode = result->errorCode();
    if ( ecode == QNetworkProtocol::ErrListChildren || ecode == QNetworkProtocol::ErrParse ||
         ecode == QNetworkProtocol::ErrUnknownProtocol || ecode == QNetworkProtocol::ErrLoginIncorrect ||
         ecode == QNetworkProtocol::ErrValid || ecode == QNetworkProtocol::ErrHostNotFound ||
         ecode == QNetworkProtocol::ErrFileNotExisting ) 
    {
       cout << "MythVideo: NetworkOperation Error on NewData.\n";
    }
    char *tempstore = new char[ba.size() + 1];
    memcpy(tempstore, ba.data(), ba.size());
    tempstore[ba.size()] = 0;

    m_data += tempstore;
}

void INETComms::stop()
{
    disconnect(&op, 0, 0, 0);
    op.stop();
    
}

void INETComms::finished(QNetworkOperation *result)
{
    int ecode = result->errorCode();
    if ( ecode == QNetworkProtocol::ErrListChildren || ecode == QNetworkProtocol::ErrParse ||
         ecode == QNetworkProtocol::ErrUnknownProtocol || ecode == QNetworkProtocol::ErrLoginIncorrect ||
         ecode == QNetworkProtocol::ErrValid || ecode == QNetworkProtocol::ErrHostNotFound ||
         ecode == QNetworkProtocol::ErrFileNotExisting )
    {
       cout << "MythVideo: NetworkOperation Error on Finish.\n";
    }

    m_done = true;
 
}


