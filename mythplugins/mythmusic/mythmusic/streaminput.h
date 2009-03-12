// Copyright (c) 2000-2001 Brad Hughes <bhughes@trolltech.com>
//
// Use, modification and distribution is allowed without limitation,
// warranty, or liability of any kind.
//

#ifndef INPUT_H
#define INPUT_H

class StreamInput;

#include <QUrl>
#include <QTcpSocket>

class StreamInput : public QObject
{
    Q_OBJECT

  public:
    StreamInput(const QUrl&);

    QIODevice *GetSocket(void) { return sock; }

    void Setup(void);

  private slots:
    void HostFound(void);
    void Connected(void);
    void ReadyRead(void);
    void Error(QAbstractSocket::SocketError);

  private:
    QString     request;
    QUrl        url;
    QTcpSocket *sock;
    int         stage;
};


#endif // INPUT_H
