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

    void Enable(void);
    void Disable(void);

  public slots:
    virtual void deleteLater(void);

  private slots:
    static void Process(const QByteArray &buf, const QHostAddress& sender,
                 quint16 senderPort);

  private:
    ~MythUDPListener(void) { Disable(); }

    void TeardownAll(void) { Disable(); }

  private:
    ServerPool *m_socketPool {nullptr};
};

#endif // MYTHUDPLISTENER_H
