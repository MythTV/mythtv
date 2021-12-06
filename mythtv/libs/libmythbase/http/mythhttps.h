#ifndef MYTHHTTPS_H
#define MYTHHTTPS_H

#ifndef QT_NO_OPENSSL
#include <QSslConfiguration>
#include <QSslCipher>
#include <QSslKey>
#endif

class MythHTTPS
{
#ifndef QT_NO_OPENSSL
  public:
    static bool InitSSLServer(QSslConfiguration& Config);
    static void InitSSLSocket(QSslSocket* Socket, QSslConfiguration& Config);
#endif
};

#endif
