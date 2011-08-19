#include <QCoreApplication>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QMutex>
#include <QWaitCondition>
#include <QNetworkInterface>
#include <QHostAddress>

#include <cmath>

#include <queue>
#include <algorithm>
using namespace std;

#ifdef USING_MINGW
#include <winsock2.h>
#include <unistd.h>
#endif

#include "compat.h"
#include "mythconfig.h"       // for CONFIG_DARWIN
#include "mythdownloadmanager.h"
#include "mythsocketthread.h"
#include "mythcorecontext.h"
#include "mythsocket.h"
#include "mythsystem.h"
#include "mthreadpool.h"
#include "exitcodes.h"
#include "mythlogging.h"
#include "mythversion.h"
#include "mthread.h"

#define LOC      QString("MythCoreContext: ")

MythCoreContext *gCoreContext = NULL;
QMutex *avcodeclock = new QMutex(QMutex::Recursive);

class MythCoreContextPrivate : public QObject
{
  public:
    MythCoreContextPrivate(MythCoreContext *lparent, QString binversion,
                           QObject *guicontext);
   ~MythCoreContextPrivate();

    bool WaitForWOL(int timeout_ms = INT_MAX);

  public:
    MythCoreContext *m_parent;
    QObject         *m_GUIcontext;
    QObject         *m_GUIobject;
    QString          m_appBinaryVersion;

    QMutex  m_hostnameLock;      ///< Locking for thread-safe copying of:
    QString m_localHostname;     ///< hostname from mysql.txt or gethostname()
    QString m_masterHostname;    ///< master backend hostname

    QMutex      m_sockLock;      ///< protects both m_serverSock and m_eventSock
    MythSocket *m_serverSock;    ///< socket for sending MythProto requests
    MythSocket *m_eventSock;     ///< socket events arrive on

    QMutex         m_WOLInProgressLock;
    QWaitCondition m_WOLInProgressWaitCondition;
    bool           m_WOLInProgress;

    bool m_backend;

    MythDB *m_database;

    QThread *m_UIThread;

    MythLocale *m_locale;
    QString language;

    MythScheduler *m_scheduler;
};

MythCoreContextPrivate::MythCoreContextPrivate(MythCoreContext *lparent,
                                               QString binversion,
                                               QObject *guicontext)
    : m_parent(lparent),
      m_GUIcontext(guicontext), m_GUIobject(NULL),
      m_appBinaryVersion(binversion),
      m_localHostname(QString::null),
      m_masterHostname(QString::null),
      m_sockLock(QMutex::NonRecursive),
      m_serverSock(NULL), m_eventSock(NULL),
      m_WOLInProgress(false),
      m_backend(false),
      m_database(GetMythDB()),
      m_UIThread(QThread::currentThread()),
      m_locale(NULL),
      m_scheduler(NULL)
{
    threadRegister("CoreContext");
}

MythCoreContextPrivate::~MythCoreContextPrivate()
{
    MThreadPool::StopAllPools();
    ShutdownRRT();

    QMutexLocker locker(&m_sockLock);
    if (m_serverSock)
    {
        m_serverSock->DownRef();
        m_serverSock = NULL;
    }
    if (m_eventSock)
    {
        m_eventSock->DownRef();
        m_eventSock = NULL;
    }

    delete m_locale;

    MThreadPool::ShutdownAllPools();

    ShutdownMythSystem();

    ShutdownMythDownloadManager();

    // This has already been run in the MythContext dtor.  Do we need it here
    // too?
#if 0
    logStop(); // need to shutdown db logger before we kill db
#endif

    MThread::Cleanup();

    GetMythDB()->GetDBManager()->CloseDatabases();

    if (m_database) {
        DestroyMythDB();
        m_database = NULL;
    }
    threadDeregister();
}

/// If another thread has already started WOL process, wait on them...
///
/// Note: Caller must be holding m_WOLInProgressLock.
bool MythCoreContextPrivate::WaitForWOL(int timeout_in_ms)
{
    int timeout_remaining = timeout_in_ms;
    while (m_WOLInProgress && (timeout_remaining > 0))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Wake-On-LAN in progress, waiting...");

        int max_wait = min(1000, timeout_remaining);
        m_WOLInProgressWaitCondition.wait(
            &m_WOLInProgressLock, max_wait);
        timeout_remaining -= max_wait;
    }

    return !m_WOLInProgress;
}

MythCoreContext::MythCoreContext(const QString &binversion,
                                 QObject *guiContext)
    : d(NULL)
{
    d = new MythCoreContextPrivate(this, binversion, guiContext);
}

bool MythCoreContext::Init(void)
{
    if (!d)
    {
        LOG(VB_GENERAL, LOG_EMERG, LOC + "Init() Out-of-memory");
        return false;
    }

    if (d->m_appBinaryVersion != MYTH_BINARY_VERSION)
    {
        LOG(VB_GENERAL, LOG_CRIT,
            QString("Application binary version (%1) does not "
                    "match libraries (%2)")
                .arg(d->m_appBinaryVersion) .arg(MYTH_BINARY_VERSION));

        QString warning = QObject::tr(
            "This application is not compatible "
            "with the installed MythTV libraries. "
            "Please recompile after a make distclean");
        LOG(VB_GENERAL, LOG_WARNING, warning);

        return false;
    }

    has_ipv6 = false;

    // If any of the IPs on any interfaces look like IPv6 addresses, assume IPv6
    // is available
    QNetworkInterface qtinterface;
    QList<QHostAddress> IpList = qtinterface.allAddresses();
    for (int i = 0; i < IpList.size(); i++)
    {
        if (IpList.at(i).toString().contains(":"))
            has_ipv6 = true;
    };


    return true;
}

MythCoreContext::~MythCoreContext()
{
    delete d;
    d = NULL;
}

bool MythCoreContext::SetupCommandSocket(MythSocket *serverSock,
                                         const QString &announcement,
                                         uint timeout_in_ms,
                                         bool &proto_mismatch)
{
    proto_mismatch = false;

#ifndef IGNORE_PROTO_VER_MISMATCH
    if (!CheckProtoVersion(serverSock, timeout_in_ms, true))
    {
        proto_mismatch = true;
        return false;
    }
#endif

    QStringList strlist(announcement);

    if (!serverSock->writeStringList(strlist))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Connecting server socket to "
                                       "master backend, socket write failed");
        return false;
    }

    if (!serverSock->readStringList(strlist, true) || strlist.empty() ||
        (strlist[0] == "ERROR"))
    {
        if (!strlist.empty())
            LOG(VB_GENERAL, LOG_ERR, LOC + "Problem connecting "
                                           "server socket to master backend");
        else
            LOG(VB_GENERAL, LOG_ERR, LOC + "Timeout connecting "
                                           "server socket to master backend");
        return false;
    }

    return true;
}

// Assumes that either m_sockLock is held, or the app is still single
// threaded (i.e. during startup).
bool MythCoreContext::ConnectToMasterServer(bool blockingClient)
{
    if (IsMasterBackend())
    {
        // Should never get here unless there is a bug in the code somewhere.
        // If this happens, it can cause endless event loops.
        LOG(VB_GENERAL, LOG_ERR, "ERROR: Master backend tried to connect back "
                "to itself!");
        return false;
    }

    QString server = GetSetting("MasterServerIP", "localhost");
    int     port   = GetNumSetting("MasterServerPort", 6543);
    bool    proto_mismatch = false;

    if (!d->m_serverSock)
    {
        QString ann = QString("ANN %1 %2 %3")
            .arg(blockingClient ? "Playback" : "Monitor")
            .arg(d->m_localHostname).arg(false);
        d->m_serverSock = ConnectCommandSocket(
            server, port, ann, &proto_mismatch);
    }

    if (!d->m_serverSock)
        return false;

    if (!IsBackend() && !d->m_eventSock)
        d->m_eventSock = ConnectEventSocket(server, port);

    if (!IsBackend() && !d->m_eventSock)
    {
        d->m_serverSock->DownRef();
        d->m_serverSock = NULL;

        QCoreApplication::postEvent(
            d->m_GUIcontext, new MythEvent("CONNECTION_FAILURE"));

        return false;
    }

    return true;
}

MythSocket *MythCoreContext::ConnectCommandSocket(
    const QString &hostname, int port, const QString &announce,
    bool *p_proto_mismatch, bool gui, int maxConnTry, int setup_timeout)
{
    MythSocket *serverSock = NULL;

    {
        QMutexLocker locker(&d->m_WOLInProgressLock);
        d->WaitForWOL();
    }

    QString WOLcmd = GetSetting("WOLbackendCommand", "");

    if (maxConnTry < 1)
        maxConnTry = max(GetNumSetting("BackendConnectRetry", 1), 1);

    int WOLsleepTime = 0, WOLmaxConnTry = 0;
    if (!WOLcmd.isEmpty())
    {
        WOLsleepTime  = GetNumSetting("WOLbackendReconnectWaitTime", 0);
        WOLmaxConnTry = max(GetNumSetting("WOLbackendConnectRetry", 1), 1);
        maxConnTry    = max(maxConnTry, WOLmaxConnTry);
    }

    bool we_attempted_wol = false;

    if (setup_timeout <= 0)
        setup_timeout = MythSocket::kShortTimeout;

    bool proto_mismatch = false;
    for (int cnt = 1; cnt <= maxConnTry; cnt++)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Connecting to backend server: %1:%2 (try %3 of %4)")
                .arg(hostname).arg(port).arg(cnt).arg(maxConnTry));

        serverSock = new MythSocket();

        int sleepms = 0;
        if (serverSock->connect(hostname, port))
        {
            if (SetupCommandSocket(
                    serverSock, announce, setup_timeout, proto_mismatch))
            {
                break;
            }

            if (proto_mismatch)
            {
                if (p_proto_mismatch)
                    *p_proto_mismatch = true;

                serverSock->DownRef();
                serverSock = NULL;
                break;
            }

            setup_timeout = (int)(setup_timeout * 1.5f);
        }
        else if (!WOLcmd.isEmpty() && (cnt < maxConnTry))
        {
            if (!we_attempted_wol)
            {
                QMutexLocker locker(&d->m_WOLInProgressLock);
                if (d->m_WOLInProgress)
                {
                    d->WaitForWOL();
                    continue;
                }

                d->m_WOLInProgress = we_attempted_wol = true;
            }

            myth_system(WOLcmd, kMSDontDisableDrawing | kMSDontBlockInputDevs |
                                kMSProcessEvents);
            sleepms = WOLsleepTime * 1000;
        }

        serverSock->DownRef();
        serverSock = NULL;

        if (!serverSock && (cnt == 1))
        {
            QCoreApplication::postEvent(
                d->m_GUIcontext, new MythEvent("CONNECTION_FAILURE"));
        }

        if (sleepms)
            usleep(sleepms * 1000);
    }

    if (we_attempted_wol)
    {
        QMutexLocker locker(&d->m_WOLInProgressLock);
        d->m_WOLInProgress = false;
        d->m_WOLInProgressWaitCondition.wakeAll();
    }

    if (!serverSock && !proto_mismatch)
    {
        LOG(VB_GENERAL, LOG_ERR,
                "Connection to master server timed out.\n\t\t\t"
                "Either the server is down or the master server settings"
                "\n\t\t\t"
                "in mythtv-settings does not contain the proper IP address\n");
    }
    else
    {
        QCoreApplication::postEvent(
            d->m_GUIcontext, new MythEvent("CONNECTION_RESTABLISHED"));
    }

    return serverSock;
}

MythSocket *MythCoreContext::ConnectEventSocket(const QString &hostname,
                                                int port)
{
    MythSocket *m_eventSock = new MythSocket();

    while (m_eventSock->state() != MythSocket::Idle)
    {
        usleep(5000);
    }

    // Assume that since we _just_ connected the command socket,
    // this one won't need multiple retries to work...
    if (!m_eventSock->connect(hostname, port))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to connect event "
                                       "socket to master backend");
        m_eventSock->DownRef();
        m_eventSock = NULL;
        return NULL;
    }

    m_eventSock->Lock();

    QString str = QString("ANN Monitor %1 %2")
        .arg(d->m_localHostname).arg(true);
    QStringList strlist(str);
    m_eventSock->writeStringList(strlist);
    if (!m_eventSock->readStringList(strlist) || strlist.empty() ||
        (strlist[0] == "ERROR"))
    {
        if (!strlist.empty())
            LOG(VB_GENERAL, LOG_ERR, LOC + "Problem connecting "
                                           "event socket to master backend");
        else
            LOG(VB_GENERAL, LOG_ERR, LOC + "Timeout connecting "
                                           "event socket to master backend");

        m_eventSock->DownRef();
        m_eventSock->Unlock();
        m_eventSock = NULL;
        return NULL;
    }

    m_eventSock->Unlock();
    m_eventSock->setCallbacks(this);

    return m_eventSock;
}

bool MythCoreContext::IsConnectedToMaster(void)
{
    QMutexLocker locker(&d->m_sockLock);
    return d->m_serverSock;
}

void MythCoreContext::BlockShutdown(void)
{
    QStringList strlist;

    QMutexLocker locker(&d->m_sockLock);
    if (d->m_serverSock == NULL)
        return;

    strlist << "BLOCK_SHUTDOWN";
    d->m_serverSock->writeStringList(strlist);
    d->m_serverSock->readStringList(strlist);

    if ((d->m_eventSock == NULL) ||
        (d->m_eventSock->state() != MythSocket::Connected))
        return;

    strlist.clear();
    strlist << "BLOCK_SHUTDOWN";

    d->m_eventSock->Lock();

    d->m_eventSock->writeStringList(strlist);
    d->m_eventSock->readStringList(strlist);

    d->m_eventSock->Unlock();
}

void MythCoreContext::AllowShutdown(void)
{
    QStringList strlist;

    QMutexLocker locker(&d->m_sockLock);
    if (d->m_serverSock == NULL)
        return;

    strlist << "ALLOW_SHUTDOWN";
    d->m_serverSock->writeStringList(strlist);
    d->m_serverSock->readStringList(strlist);

    if ((d->m_eventSock == NULL) ||
        (d->m_eventSock->state() != MythSocket::Connected))
        return;

    strlist.clear();
    strlist << "ALLOW_SHUTDOWN";

    d->m_eventSock->Lock();

    d->m_eventSock->writeStringList(strlist);
    d->m_eventSock->readStringList(strlist);

    d->m_eventSock->Unlock();
}

void MythCoreContext::SetBackend(bool backend)
{
    d->m_backend = backend;
}

bool MythCoreContext::IsBackend(void)
{
    return d->m_backend;
}

bool MythCoreContext::IsMasterHost(void)
{
    QString myip = GetSetting("BackendServerIP");
    QString masterip = GetSetting("MasterServerIP");

    return (masterip == myip);
}

bool MythCoreContext::IsMasterBackend(void)
{
    return (IsBackend() && IsMasterHost());
}

bool MythCoreContext::BackendIsRunning(void)
{
#if CONFIG_DARWIN || (__FreeBSD__) || defined(__OpenBSD__)
    const char *command = "ps -axc | grep -i mythbackend | grep -v grep > /dev/null";
#elif defined USING_MINGW
    const char *command = "%systemroot%\\system32\\tasklist.exe "
       " | %systemroot%\\system32\\find.exe /i \"mythbackend.exe\" ";
#else
    const char *command = "ps ch -C mythbackend -o pid > /dev/null";
#endif
    uint res = myth_system(command, kMSDontBlockInputDevs |
                                    kMSDontDisableDrawing |
                                    kMSProcessEvents);
    return (res == GENERIC_EXIT_OK);
}

bool MythCoreContext::IsFrontendOnly(void)
{
    // find out if a backend runs on this host...
    bool backendOnLocalhost = false;

    QStringList strlist("QUERY_IS_ACTIVE_BACKEND");
    strlist << GetHostName();

    SendReceiveStringList(strlist);

    if (QString(strlist[0]) == "FALSE")
        backendOnLocalhost = false;
    else
        backendOnLocalhost = true;

    return !backendOnLocalhost;
}

QHostAddress MythCoreContext::MythHostAddressAny(void)
{

    if (has_ipv6)
        return QHostAddress(QHostAddress::AnyIPv6);
    else
        return QHostAddress(QHostAddress::Any);

}

QString MythCoreContext::GenMythURL(QString host, QString port, QString path, QString storageGroup)
{
    return GenMythURL(host,port.toInt(),path,storageGroup);
}

QString MythCoreContext::GenMythURL(QString host, int port, QString path, QString storageGroup) 
{
    QString ret;

    QString m_storageGroup;
    QString m_host;
    QString m_port;

    QHostAddress addr(host);

    if (!storageGroup.isEmpty()) 
        m_storageGroup = storageGroup + "@";

    m_host = host;

#if !defined(QT_NO_IPV6)
    // Basically if it appears to be an IPv6 IP surround the IP with [] otherwise don't bother
    if (( addr.protocol() == QAbstractSocket::IPv6Protocol ) || (host.contains(":"))) 
        m_host = "[" + host + "]";
#endif

    if (port > 0) 
        m_port = QString(":%1").arg(port);
    else
        m_port = "";

    QString seperator = "/";
    if (path.startsWith("/"))
        seperator = "";

    ret = QString("myth://%1%2%3%4%5").arg(m_storageGroup).arg(m_host).arg(m_port).arg(seperator).arg(path);

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC + 
        QString("GenMythURL returning %1").arg(ret));
#endif

    return ret;
}


QString MythCoreContext::GetMasterHostPrefix(QString storageGroup)
{
    QString ret;

    if (IsMasterHost())
    {
        return GenMythURL(GetSetting("MasterServerIP"),
                          GetNumSetting("MasterServerPort", 6543),
                          "",
                          storageGroup);
    }

    QMutexLocker locker(&d->m_sockLock);
    if (!d->m_serverSock)
    {
        bool blockingClient = GetNumSetting("idleTimeoutSecs",0) > 0;
        ConnectToMasterServer(blockingClient);
    }

    if (d->m_serverSock)
    {

         ret = GenMythURL(d->m_serverSock->peerAddress().toString(),
                          d->m_serverSock->peerPort(),
                          "",
                          storageGroup);
    }

    return ret;
}

QString MythCoreContext::GetMasterHostName(void)
{
    QMutexLocker locker(&d->m_hostnameLock);

    if (d->m_masterHostname.isEmpty())
    {
        QStringList strlist("QUERY_HOSTNAME");

        if (SendReceiveStringList(strlist))
            d->m_masterHostname = strlist[0];
    }

    QString ret = d->m_masterHostname;
    ret.detach();

    return ret;
}

void MythCoreContext::ClearSettingsCache(const QString &myKey)
{
    d->m_database->ClearSettingsCache(myKey);
}

void MythCoreContext::ActivateSettingsCache(bool activate)
{
    d->m_database->ActivateSettingsCache(activate);
}

QString MythCoreContext::GetHostName(void)
{
    QMutexLocker (&d->m_hostnameLock);
    QString tmp = d->m_localHostname;
    tmp.detach();
    return tmp;
}

QString MythCoreContext::GetFilePrefix(void)
{
    return GetSetting("RecordFilePrefix");
}

void MythCoreContext::GetResolutionSetting(const QString &type,
                                           int &width, int &height,
                                           double &forced_aspect,
                                           double &refresh_rate,
                                           int index)
{
    d->m_database->GetResolutionSetting(type, width, height, forced_aspect,
                                        refresh_rate, index);
}

void MythCoreContext::GetResolutionSetting(const QString &t, int &w,
                                           int &h, int i)
{
    d->m_database->GetResolutionSetting(t, w, h, i);
}

MDBManager *MythCoreContext::GetDBManager(void)
{
    return d->m_database->GetDBManager();
}

/** /brief Returns true if database is being ignored.
 *
 *  This was created for some command line only programs which
 *  still need MythTV libraries, such as channel scanners, channel
 *  change programs, and the off-line commercial flagger.
 */
bool MythCoreContext::IsDatabaseIgnored(void) const
{
    return d->m_database->IsDatabaseIgnored();
}

void MythCoreContext::SaveSetting(const QString &key, int newValue)
{
    d->m_database->SaveSetting(key, newValue);
}

void MythCoreContext::SaveSetting(const QString &key, const QString &newValue)
{
    d->m_database->SaveSetting(key, newValue);
}

bool MythCoreContext::SaveSettingOnHost(const QString &key,
                                        const QString &newValue,
                                        const QString &host)
{
    return d->m_database->SaveSettingOnHost(key, newValue, host);
}

QString MythCoreContext::GetSetting(const QString &key,
                                    const QString &defaultval)
{
    return d->m_database->GetSetting(key, defaultval);
}

int MythCoreContext::GetNumSetting(const QString &key, int defaultval)
{
    return d->m_database->GetNumSetting(key, defaultval);
}

double MythCoreContext::GetFloatSetting(const QString &key, double defaultval)
{
    return d->m_database->GetFloatSetting(key, defaultval);
}

QString MythCoreContext::GetSettingOnHost(const QString &key,
                                          const QString &host,
                                          const QString &defaultval)
{
    return d->m_database->GetSettingOnHost(key, host, defaultval);
}

int MythCoreContext::GetNumSettingOnHost(const QString &key,
                                         const QString &host,
                                         int defaultval)
{
    return d->m_database->GetNumSettingOnHost(key, host, defaultval);
}

double MythCoreContext::GetFloatSettingOnHost(const QString &key,
                                              const QString &host,
                                              double defaultval)
{
    return d->m_database->GetFloatSettingOnHost(key, host, defaultval);
}

void MythCoreContext::SetSetting(const QString &key, const QString &newValue)
{
    d->m_database->SetSetting(key, newValue);
}

void MythCoreContext::OverrideSettingForSession(const QString &key,
                                                const QString &value)
{
    d->m_database->OverrideSettingForSession(key, value);
}

bool MythCoreContext::IsUIThread(void)
{
    return is_current_thread(d->m_UIThread);
}

bool MythCoreContext::SendReceiveStringList(QStringList &strlist,
                                        bool quickTimeout, bool block)
{
    if (HasGUI() && IsUIThread())
    {
        QString msg = "SendReceiveStringList(";
        for (uint i=0; i<(uint)strlist.size() && i<2; i++)
            msg += (i?",":"") + strlist[i];
        msg += (strlist.size() > 2) ? "...)" : ")";
        msg += " called from UI thread";
        LOG(VB_GENERAL, LOG_DEBUG, msg);
    }

    QString query_type = "UNKNOWN";

    if (!strlist.isEmpty())
        query_type = strlist[0];

    QMutexLocker locker(&d->m_sockLock);
    if (!d->m_serverSock)
    {
        bool blockingClient = GetNumSetting("idleTimeoutSecs",0) > 0;
        ConnectToMasterServer(blockingClient);
    }

    bool ok = false;

    if (d->m_serverSock)
    {
        QStringList sendstrlist = strlist;
        d->m_serverSock->writeStringList(sendstrlist);
        ok = d->m_serverSock->readStringList(strlist, quickTimeout);

        if (!ok)
        {
            LOG(VB_GENERAL, LOG_NOTICE,
                QString("Connection to backend server lost"));
            d->m_serverSock->DownRef();
            d->m_serverSock = NULL;

            if (d->m_eventSock)
            {
                d->m_eventSock->DownRef();
                d->m_eventSock = NULL;
            }

            bool blockingClient = GetNumSetting("idleTimeoutSecs",0);
            ConnectToMasterServer(blockingClient);

            if (d->m_serverSock)
            {
                d->m_serverSock->writeStringList(sendstrlist);
                ok = d->m_serverSock->readStringList(strlist, quickTimeout);
            }
        }

        // this should not happen
        while (ok && strlist[0] == "BACKEND_MESSAGE")
        {
            // oops, not for us
            LOG(VB_GENERAL, LOG_EMERG, "SRSL you shouldn't see this!!");
            QString message = strlist[1];
            strlist.pop_front(); strlist.pop_front();

            MythEvent me(message, strlist);
            dispatch(me);

            ok = d->m_serverSock->readStringList(strlist, quickTimeout);
        }

        if (!ok)
        {
            if (d->m_serverSock)
            {
                d->m_serverSock->DownRef();
                d->m_serverSock = NULL;
            }

            LOG(VB_GENERAL, LOG_CRIT,
                QString("Reconnection to backend server failed"));

            QCoreApplication::postEvent(d->m_GUIcontext,
                                new MythEvent("PERSISTENT_CONNECTION_FAILURE"));
        }
    }

    if (ok)
    {
        if (strlist.isEmpty())
            ok = false;
        else if (strlist[0] == "ERROR")
        {
            if (strlist.size() == 2)
                LOG(VB_GENERAL, LOG_INFO,
                    QString("Protocol query '%1' responded with the error '%2'")
                        .arg(query_type).arg(strlist[1]));
            else
                LOG(VB_GENERAL, LOG_INFO, 
                    QString("Protocol query '%1' responded with an error, but "
                            "no error message.") .arg(query_type));

            ok = false;
        }
    }

    return ok;
}

void MythCoreContext::readyRead(MythSocket *sock)
{
    while (sock->state() == MythSocket::Connected &&
           sock->bytesAvailable() > 0)
    {
        QStringList strlist;
        if (!sock->readStringList(strlist))
            continue;

        QString prefix = strlist[0];
        QString message = strlist[1];

        if (prefix == "OK")
        {
        }
        else if (prefix != "BACKEND_MESSAGE")
        {
            LOG(VB_GENERAL, LOG_ERR,
                    QString("Received a: %1 message from the backend "
                            "but I don't know what to do with it.")
                        .arg(prefix));
        }
        else if (message == "CLEAR_SETTINGS_CACHE")
        {
            // No need to dispatch this message to ourself, so handle it
            LOG(VB_GENERAL, LOG_INFO, "Received remote 'Clear Cache' request");
            ClearSettingsCache();
        }
        else
        {
            strlist.pop_front();
            strlist.pop_front();
            MythEvent me(message, strlist);
            dispatch(me);
        }
    }
}

void MythCoreContext::connectionClosed(MythSocket *sock)
{
    (void)sock;

    LOG(VB_GENERAL, LOG_NOTICE,
        "Event socket closed.  No connection to the backend.");

    QMutexLocker locker(&d->m_sockLock);
    if (d->m_serverSock)
    {
        d->m_serverSock->DownRef();
        d->m_serverSock = NULL;
    }

    if (d->m_eventSock)
    {
        d->m_eventSock->DownRef();
        d->m_eventSock = NULL;
    }

    dispatch(MythEvent(QString("BACKEND_SOCKETS_CLOSED")));
}

bool MythCoreContext::CheckProtoVersion(MythSocket *socket, uint timeout_ms,
                                        bool error_dialog_desired)
{
    if (!socket)
        return false;

    QStringList strlist(QString("MYTH_PROTO_VERSION %1 %2")
                        .arg(MYTH_PROTO_VERSION).arg(MYTH_PROTO_TOKEN));
    socket->writeStringList(strlist);

    if (!socket->readStringList(strlist, timeout_ms) || strlist.empty())
    {
        LOG(VB_GENERAL, LOG_CRIT, "Protocol version check failure.\n\t\t\t"
                "The response to MYTH_PROTO_VERSION was empty.\n\t\t\t"
                "This happens when the backend is too busy to respond,\n\t\t\t"
                "or has deadlocked in due to bugs or hardware failure.");

        return false;
    }
    else if (strlist[0] == "REJECT" && strlist.size() >= 2)
    {
        LOG(VB_GENERAL, LOG_CRIT, QString("Protocol version or token mismatch "
                                          "(frontend=%1/%2,backend=%3/\?\?)\n")
                                      .arg(MYTH_PROTO_VERSION)
                                      .arg(MYTH_PROTO_TOKEN)
                                      .arg(strlist[1]));

        if (error_dialog_desired && d->m_GUIcontext)
        {
            QStringList list(strlist[1]);
            QCoreApplication::postEvent(
                d->m_GUIcontext, new MythEvent("VERSION_MISMATCH", list));
        }

        return false;
    }
    else if (strlist[0] == "ACCEPT")
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Using protocol version %1")
                                      .arg(MYTH_PROTO_VERSION));
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR,
        QString("Unexpected response to MYTH_PROTO_VERSION: %1")
            .arg(strlist[0]));
    return false;
}

void MythCoreContext::dispatch(const MythEvent &event)
{
    LOG(VB_NETWORK, LOG_INFO, QString("MythEvent: %1").arg(event.Message()));

    MythObservable::dispatch(event);
}

void MythCoreContext::dispatchNow(const MythEvent &event)
{
    LOG(VB_NETWORK, LOG_INFO, QString("MythEvent: %1").arg(event.Message()));

    MythObservable::dispatchNow(event);
}

void MythCoreContext::SetLocalHostname(const QString &hostname)
{
    QMutexLocker locker(&d->m_hostnameLock);
    d->m_localHostname = hostname;
    d->m_database->SetLocalHostname(hostname);
}

void MythCoreContext::SetGUIObject(QObject *gui)
{
    d->m_GUIobject = gui;
}

bool MythCoreContext::HasGUI(void)
{
    return d->m_GUIobject;
}

QObject *MythCoreContext::GetGUIObject(void)
{
    return d->m_GUIobject;
}

MythDB *MythCoreContext::GetDB(void)
{
    return d->m_database;
}

const MythLocale *MythCoreContext::GetLocale(void)
{
    return d->m_locale;
}

/**
 *  \brief Returns two character ISO-639 language descriptor for UI language.
 *  \sa iso639_get_language_list()
 */
QString MythCoreContext::GetLanguage(void)
{
    return GetLanguageAndVariant().left(2);
}

/**
 *  \brief Returns the user-set language and variant.
 *
 *   The string has the form ll or ll_vv, where ll is the two character
 *   ISO-639 language code, and vv (which may not exist) is the variant.
 *   Examples include en_AU, en_CA, en_GB, en_US, fr_CH, fr_DE, pt_BR, pt_PT.
 */
QString MythCoreContext::GetLanguageAndVariant(void)
{
    if (d->language.isEmpty())
        d->language = GetSetting("Language", "en_US").toLower();

    return d->language;
}

void MythCoreContext::ResetLanguage(void)
{
    d->language.clear();
}

void MythCoreContext::InitLocale(void )
{
    if (!d->m_locale)
        d->m_locale = new MythLocale();
}

void MythCoreContext::SaveLocaleDefaults(void)
{
    if (!d->m_locale)
        InitLocale();

    if (!d->m_locale->GetLocaleCode().isEmpty())
    {
        LOG(VB_GENERAL, LOG_INFO,
            QString("Current locale %1") .arg(d->m_locale->GetLocaleCode()));

        d->m_locale->SaveLocaleDefaults();
        return;
    }

    LOG(VB_GENERAL, LOG_ERR,
        "No locale defined! We weren't able to set locale defaults.");
}

void MythCoreContext::SetScheduler(MythScheduler *sched)
{
    d->m_scheduler = sched;
}

MythScheduler *MythCoreContext::GetScheduler(void)
{
    return d->m_scheduler;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
