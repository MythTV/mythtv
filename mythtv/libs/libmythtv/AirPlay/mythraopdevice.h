#ifndef MYTHRAOPDEVICE_H
#define MYTHRAOPDEVICE_H

#include <QObject>

#include "serverpool.h"
#include "mythtvexp.h"

class QMutex;
class MThread;
class BonjourRegister;
class MythRAOPConnection;

#define RAOP_PORT_RANGE 100
#define RAOP_HARDWARE_ID_SIZE 6

class MTV_PUBLIC MythRAOPDevice : public ServerPool
{
    Q_OBJECT

  public:
    static bool Create(void);
    static void Cleanup(void);

    MythRAOPDevice();
    bool NextInAudioQueue(MythRAOPConnection* conn);
    static QString HardwareId();

  private slots:
    void Start();
    void newConnection(QTcpSocket *client);
    void deleteClient();

  private:
    virtual ~MythRAOPDevice(void);
    void Teardown(void);

    // Globals
    static MythRAOPDevice *gMythRAOPDevice;
    static QMutex         *gMythRAOPDeviceMutex;
    static MThread        *gMythRAOPDeviceThread;

    // Members
    QString          m_name;
    QByteArray       m_hardwareId;
    BonjourRegister *m_bonjour;
    bool             m_valid;
    QMutex          *m_lock;
    int              m_setupPort;
    QList<MythRAOPConnection*> m_clients;
};


#endif // MYTHRAOPDEVICE_H
