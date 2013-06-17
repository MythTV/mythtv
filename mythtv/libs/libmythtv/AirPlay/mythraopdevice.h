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

class MTV_PUBLIC MythRAOPDevice : public ServerPool
{
    Q_OBJECT

  public:
    static bool Create(void);
    static void Cleanup(void);
    static MythRAOPDevice *RAOPSharedInstance(void) { return gMythRAOPDevice; }

    MythRAOPDevice();
    void DeleteAllClients(MythRAOPConnection *keep);

  public slots:
    void TVPlaybackStarting(void);

  private slots:
    void Start();
    void Stop();
    void newConnection(QTcpSocket *client);
    void deleteClient();

  private:
    virtual ~MythRAOPDevice(void);
    void Teardown(void);
    bool RegisterForBonjour(void);

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
    int              m_basePort;
    QList<MythRAOPConnection*> m_clients;
};


#endif // MYTHRAOPDEVICE_H
