#ifndef MYTHUDPLISTENER_H
#define MYTHUDPLISTENER_H

// Qt
#include <QObject>

// MythTV
#include "serverpool.h"

class MythUDPListener : public QObject
{
    Q_OBJECT

  public:
    MythUDPListener();
   ~MythUDPListener() override;
    void Enable();
    void Disable();

  private slots:
    static void Process(const QByteArray &Buffer, const QHostAddress& /*Sender*/, quint16 /*SenderPort*/);

  private:
    ServerPool* m_socketPool { nullptr };
};

#endif
