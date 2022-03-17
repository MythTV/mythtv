#ifndef MYTHRAOPDEVICE_H
#define MYTHRAOPDEVICE_H

#include <QObject>
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
#include <QMutex>
#else
#include <QRecursiveMutex>
#endif

#include "libmythbase/serverpool.h"
#include "libmythtv/mythtvexp.h"

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
    void newRaopConnection(QTcpSocket *client);
    void deleteClient();

  private:
    ~MythRAOPDevice(void) override;
    void Teardown(void);
    bool RegisterForBonjour(void);

    // Globals
    static MythRAOPDevice *gMythRAOPDevice;
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    static QMutex         *gMythRAOPDeviceMutex;
#else
    static QRecursiveMutex *gMythRAOPDeviceMutex;
#endif
    static MThread        *gMythRAOPDeviceThread;

    // Members
    QString          m_name       {"MythTV"};
    QByteArray       m_hardwareId;
    BonjourRegister *m_bonjour    {nullptr};
    bool             m_valid      {false};
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    QMutex          *m_lock       {nullptr};
#else
    QRecursiveMutex *m_lock       {nullptr};
#endif
    int              m_setupPort  {5000};
    int              m_basePort   {0};
    QList<MythRAOPConnection*> m_clients;
};


#endif // MYTHRAOPDEVICE_H
