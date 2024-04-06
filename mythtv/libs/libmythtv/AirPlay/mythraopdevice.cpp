#include <limits> // workaround QTBUG-90395

#include <QTimer>
#include <QtEndian>
#include <QNetworkInterface>
#include <QCoreApplication>
#include <QtAlgorithms>
#include <QTcpSocket>

#include "libmythbase/bonjourregister.h"
#include "libmythbase/mthread.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythui/mythmainwindow.h"

#include "mythraopconnection.h"
#include "mythraopdevice.h"
#include "mythairplayserver.h"

MythRAOPDevice *MythRAOPDevice::gMythRAOPDevice = nullptr;
MThread        *MythRAOPDevice::gMythRAOPDeviceThread = nullptr;
QRecursiveMutex *MythRAOPDevice::gMythRAOPDeviceMutex = new QRecursiveMutex();

#define LOC QString("RAOP Device: ")

bool MythRAOPDevice::Create(void)
{
    QMutexLocker locker(gMythRAOPDeviceMutex);

    // don't bother trying to start if there is no private key
    if (!MythRAOPConnection::LoadKey())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Aborting startup - no key found.");
        return false;
    }

    // create the device thread
    if (!gMythRAOPDeviceThread)
        gMythRAOPDeviceThread = new MThread("RAOPDevice");
    if (!gMythRAOPDeviceThread)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create RAOP device thread.");
        return false;
    }

    // create the device object
    if (!gMythRAOPDevice)
        gMythRAOPDevice = new MythRAOPDevice();
    if (!gMythRAOPDevice)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create RAOP device object.");
        return false;
    }

    // start the thread
    if (!gMythRAOPDeviceThread->isRunning())
    {
        gMythRAOPDevice->moveToThread(gMythRAOPDeviceThread->qthread());
        QObject::connect(
            gMythRAOPDeviceThread->qthread(), &QThread::started,
            gMythRAOPDevice,                  &MythRAOPDevice::Start);
        QObject::connect(
            gMythRAOPDeviceThread->qthread(), &QThread::finished,
            gMythRAOPDevice,                  &MythRAOPDevice::Stop);
        gMythRAOPDeviceThread->start(QThread::LowestPriority);
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + "Created RAOP device objects.");
    return true;
}

void MythRAOPDevice::Cleanup(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Cleaning up.");

    QMutexLocker locker(gMythRAOPDeviceMutex);
    if (gMythRAOPDeviceThread)
    {
        gMythRAOPDeviceThread->exit();
        gMythRAOPDeviceThread->wait();
    }
    delete gMythRAOPDeviceThread;
    gMythRAOPDeviceThread = nullptr;

    delete gMythRAOPDevice;
    gMythRAOPDevice = nullptr;
}

MythRAOPDevice::MythRAOPDevice()
    : m_lock(new QRecursiveMutex())
{
    m_hardwareId = QByteArray::fromHex(AirPlayHardwareId().toLatin1());
}

MythRAOPDevice::~MythRAOPDevice()
{
    delete m_lock;
    m_lock = nullptr;
}

void MythRAOPDevice::Teardown(void)
{
    QMutexLocker locker(m_lock);

    // invalidate
    m_valid = false;

    // disconnect from mDNS
    delete m_bonjour;
    m_bonjour = nullptr;

    // disconnect clients
    DeleteAllClients(nullptr);
}

void MythRAOPDevice::Start(void)
{
    QMutexLocker locker(m_lock);

    // already started?
    if (m_valid)
        return;

    // join the dots
    connect(this, &ServerPool::newConnection,
            this, &MythRAOPDevice::newRaopConnection);

    m_basePort = m_setupPort;
    m_setupPort = tryListeningPort(m_setupPort, RAOP_PORT_RANGE);
    // start listening for connections (try a few ports in case the default is in use)
    if (m_setupPort < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Failed to find a port for incoming connections.");
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Listening for connections on port %1").arg(m_setupPort));

        if (!RegisterForBonjour())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to register service.");
            return;
        }
    }

    m_valid = true;
}

void MythRAOPDevice::Stop(void)
{
    Teardown();
}

bool MythRAOPDevice::RegisterForBonjour(void)
{
    // announce service
    m_bonjour = new BonjourRegister(this);

    // give each frontend a unique name
    int multiple = m_setupPort - m_basePort;
    if (multiple > 0)
        m_name += QString::number(multiple);

    QByteArray name = m_hardwareId.toHex();
    name.append("@");
    name.append(m_name.toUtf8());
    name.append(" on ");
    name.append(gCoreContext->GetHostName().toUtf8());
    QByteArray type = "_raop._tcp";
    QByteArray txt;
    txt.append(6); txt.append("tp=UDP");
    txt.append(8); txt.append("sm=false");
    txt.append(8); txt.append("sv=false");
    txt.append(4); txt.append("ek=1");      //
    txt.append(6); txt.append("et=0,1");    // encryption type: no, RSA
    txt.append(6); txt.append("cn=0,1");    // audio codec: pcm, alac
    txt.append(4); txt.append("ch=2");      // audio channels
    txt.append(5); txt.append("ss=16");     // sample size
    txt.append(8); txt.append("sr=44100");  // sample rate
    if (gCoreContext->GetBoolSetting("AirPlayPasswordEnabled"))
    {
        txt.append(7); txt.append("pw=true");
    }
    else
    {
        txt.append(8); txt.append("pw=false");
    }
    txt.append(4); txt.append("vn=3");
    txt.append(9); txt.append("txtvers=1"); // TXT record version 1
    txt.append(8); txt.append("md=0,1,2");  // metadata-type: text, artwork, progress
    txt.append(8); txt.append("vs=115.2");
    txt.append(7); txt.append("da=true");
    txt.append(11); txt.append("am=MythTV,1");

    LOG(VB_GENERAL, LOG_INFO, QString("Registering service %1.%2 port %3 TXT %4")
        .arg(QString(name), QString(type), QString::number(m_setupPort), QString(txt)));
    return m_bonjour->Register(m_setupPort, type, name, txt);
}

void MythRAOPDevice::TVPlaybackStarting(void)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Receiving new playback message"));
    DeleteAllClients(nullptr);
}

void MythRAOPDevice::newRaopConnection(QTcpSocket *client)
{
    QMutexLocker locker(m_lock);
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("New connection from %1:%2")
        .arg(client->peerAddress().toString()).arg(client->peerPort()));

    gCoreContext->SendSystemEvent(QString("AIRTUNES_NEW_CONNECTION"));
    MythNotification n(tr("New Connection"), tr("AirTunes"),
                       tr("from %1:%2").arg(client->peerAddress().toString()).arg(client->peerPort()));
    // Don't show it during playback
    n.SetVisibility(n.GetVisibility() & ~MythNotification::kPlayback);
    GetNotificationCenter()->Queue(n);

    auto *obj = new MythRAOPConnection(this, client, m_hardwareId, 6000);

    if (obj->Init())
    {
        m_clients.append(obj);
        connect(client, &QAbstractSocket::disconnected, this, &MythRAOPDevice::deleteClient);
        gCoreContext->RegisterForPlayback(this, &MythRAOPDevice::TVPlaybackStarting);
        return;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC +
        "Failed to initialise client connection - closing.");
    delete obj;
    client->disconnectFromHost();
    delete client;
}

void MythRAOPDevice::deleteClient(void)
{
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "Entering DeleteClient.");
    QMutexLocker locker(m_lock);
    QList<MythRAOPConnection *>::iterator it = m_clients.begin();

    MythNotification n(tr("Client disconnected"), tr("AirTunes"));
    gCoreContext->SendSystemEvent(QString("AIRTUNES_DELETE_CONNECTION"));
    // Don't show it during playback
    n.SetVisibility(n.GetVisibility() & ~MythNotification::kPlayback);
    GetNotificationCenter()->Queue(n);

    while (it != m_clients.end())
    {
        if ((*it)->GetSocket()->state() == QTcpSocket::UnconnectedState)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "Removing client connection.");
            delete *it;
            it = m_clients.erase(it);
            break;
        }
        ++it;
    }
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "Exiting DeleteClient.");
}

void MythRAOPDevice::DeleteAllClients(MythRAOPConnection *keep)
{
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "Entering DeleteAllClients.");
    QMutexLocker locker(m_lock);

    QList<MythRAOPConnection*>::iterator it = m_clients.begin();

    while (it != m_clients.end())
    {
        MythRAOPConnection *client = *it;
        if (client == keep)
        {
            ++it;
            continue;
        }
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Removing client connection %1:%2")
            .arg(client->GetSocket()->peerAddress().toString())
            .arg(client->GetSocket()->peerPort()));
        delete *it;
        it = m_clients.erase(it);
    }
    LOG(VB_GENERAL, LOG_DEBUG, LOC + "Exiting DeleteAllClients.");
}
