#ifndef MYTHUDPLISTENER_H
#define MYTHUDPLISTENER_H

// Qt
#include <QObject>

// MythTV
#include "libmythbase/mthread.h"
#include "libmythbase/serverpool.h"

class MythUDPListener : public QObject
{
    friend class MythUDP;

    Q_OBJECT

  signals:
    void EnableUDPListener(bool Enable = true);

  protected:
    MythUDPListener();
   ~MythUDPListener() override;

  private slots:
    void DoEnable(bool Enable = true);
    static void Process(const QByteArray &Buffer, const QHostAddress& /*Sender*/, quint16 /*SenderPort*/);

  private:
    Q_DISABLE_COPY(MythUDPListener)
    ServerPool* m_socketPool { nullptr };
};

class MythUDP
{
  public:
    static void EnableUDPListener(bool Enable = true);
    static void StopUDPListener();

  private:
    Q_DISABLE_COPY(MythUDP)
    static MythUDP& Instance();
    MythUDP();
   ~MythUDP();

    MythUDPListener* m_listener { nullptr };
    MThread*         m_thread   { nullptr };
};

#endif
