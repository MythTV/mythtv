#ifndef MYTHUDPLISTENER_H
#define MYTHUDPLISTENER_H

#include <QObject>

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
    void ReadPending(void);

  private:
    ~MythUDPListener(void) { TeardownAll(); }

    void Process(const QByteArray &buf);
    void TeardownAll(void);

  private:
    QUdpSocket *m_socket;
};

#endif // MYTHUDPLISTENER_H
