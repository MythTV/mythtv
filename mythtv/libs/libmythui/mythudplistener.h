#ifndef MYTHUDPLISTENER_H
#define MYTHUDPLISTENER_H

#include <QObject>

#include "serverpool.h"

class QByteArray;
class QUdpSocket;
class QDomElement;

class MythUDPListener : public QObject
{
    Q_OBJECT

  public:
    MythUDPListener();

  public slots:
    virtual void deleteLater(void);

  private slots:
    void Process(const QByteArray &buf, QHostAddress sender,
                 quint16 senderPort);

  private:
    ~MythUDPListener(void) { TeardownAll(); }

    void TeardownAll(void);

  private:
    ServerPool *m_socketPool;
};

#endif // MYTHUDPLISTENER_H
