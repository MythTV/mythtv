#include <QCoreApplication>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QMutex>
#include <QRunnable>
#include <QWaitCondition>
#include <QNetworkInterface>
#include <QAbstractSocket>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QNetworkAddressEntry>
#include <QLocale>
#include <QPair>
#include <QDateTime>

#include <cmath>
#include <cstdarg>

#include <queue>
#include <algorithm>
using namespace std;

#ifdef _WIN32
#include <winsock2.h>
#include <unistd.h>
#else
#include <locale.h>
#endif

#include "compat.h"
#include "mythconfig.h"       // for CONFIG_DARWIN
#include "mythdownloadmanager.h"
#include "mythcorecontext.h"
#include "mythsocket.h"
#include "mythsystemlegacy.h"
#include "mthreadpool.h"
#include "exitcodes.h"
#include "mythlogging.h"
#include "mythversion.h"
#include "logging.h"
#include "mthread.h"
#include "serverpool.h"
#include "mythdate.h"
#include "mythplugin.h"

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

    QMutex  m_localHostLock;     ///< Locking for m_localHostname
    QString m_localHostname;     ///< hostname from config.xml or gethostname()
    QMutex  m_masterHostLock;    ///< Locking for m_masterHostname
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

    bool m_blockingClient;

    QMap<QObject *, QByteArray> m_playbackClients;
    QMutex m_playbackLock;
    bool m_inwanting;
    bool m_intvwanting;

    bool m_announcedProtocol;

    MythPluginManager *m_pluginmanager;

    bool m_isexiting;

    QMap<QString, QPair<int64_t, uint64_t> >  m_fileswritten;
    QMutex m_fileslock;
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
      m_scheduler(NULL),
      m_blockingClient(false),
      m_inwanting(false),
      m_intvwanting(false),
      m_announcedProtocol(false),
      m_pluginmanager(NULL),
      m_isexiting(false)
{
    MThread::ThreadSetup("CoreContext");
    srandom(MythDate::current().toTime_t() ^ QTime::currentTime().msec());
}

static void delete_sock(QMutexLocker &locker, MythSocket **s)
{
    if (*s)
    {
        MythSocket *tmp = *s;
        *s = NULL;
        locker.unlock();
        tmp->DecrRef();
        locker.relock();
    }
}

MythCoreContextPrivate::~MythCoreContextPrivate()
{
    MThreadPool::StopAllPools();

    {
        QMutexLocker locker(&m_sockLock);
        delete_sock(locker, &m_serverSock);
        delete_sock(locker, &m_eventSock);
    }

    delete m_locale;

    MThreadPool::ShutdownAllPools();

    ShutdownMythSystemLegacy();

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

    loggingDeregisterThread();
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

        QString warning = tr("This application is not compatible with the "
                             "installed MythTV libraries. Please recompile "
                             "after a make distclean");
        LOG(VB_GENERAL, LOG_WARNING, warning);

        return false;
    }

#ifndef _WIN32
    QString lang_variables("");
    QString lc_value = setlocale(LC_CTYPE, NULL);
    if (lc_value.isEmpty())
    {
        // try fallback to environment variables for non-glibc systems
        // LC_ALL, then LC_CTYPE
        lc_value = getenv("LC_ALL");
        if (lc_value.isEmpty())
            lc_value = getenv("LC_CTYPE");
    }
    if (!lc_value.contains("UTF-8", Qt::CaseInsensitive))
        lang_variables.append("LC_ALL or LC_CTYPE");
    lc_value = getenv("LANG");
    if (!lc_value.contains("UTF-8", Qt::CaseInsensitive))
    {
        if (!lang_variables.isEmpty())
            lang_variables.append(", and ");
        lang_variables.append("LANG");
    }
    LOG(VB_GENERAL, LOG_INFO, QString("Assumed character encoding: %1")
                                     .arg(lc_value));
    if (!lang_variables.isEmpty())
        LOG(VB_GENERAL, LOG_WARNING, QString("This application expects to "
            "be running a locale that specifies a UTF-8 codeset, and many "
            "features may behave improperly with your current language "
            "settings. Please set the %1 variable(s) in the environment "
            "in which this program is executed to include a UTF-8 codeset "
            "(such as 'en_US.UTF-8').").arg(lang_variables));
#endif

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

    if (!serverSock->WriteStringList(strlist))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Connecting server socket to "
                                       "master backend, socket write failed");
        return false;
    }

    if (!serverSock->ReadStringList(strlist, MythSocket::kShortTimeout) ||
        strlist.empty() || (strlist[0] == "ERROR"))
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

// Connects to master server safely (i.e. by taking m_sockLock)
bool MythCoreContext::SafeConnectToMasterServer(bool blockingClient,
                                                bool openEventSocket)
{
    QMutexLocker locker(&d->m_sockLock);
    bool success = ConnectToMasterServer(blockingClient, openEventSocket);

    return success;
}

// Assumes that either m_sockLock is held, or the app is still single
// threaded (i.e. during startup).
bool MythCoreContext::ConnectToMasterServer(bool blockingClient,
                                            bool openEventSocket)
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

    if (d->m_serverSock && !d->m_serverSock->IsConnected())
    {
        d->m_serverSock->DecrRef();
        d->m_serverSock = NULL;
    }

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

    d->m_blockingClient = blockingClient;

    if (!openEventSocket)
        return true;


    if (!IsBackend())
    {
        if (d->m_eventSock && !d->m_eventSock->IsConnected())
        {
            d->m_eventSock->DecrRef();
            d->m_eventSock = NULL;
        }
        if (!d->m_eventSock)
            d->m_eventSock = ConnectEventSocket(server, port);

        if (!d->m_eventSock)
        {
            d->m_serverSock->DecrRef();
            d->m_serverSock = NULL;

            QCoreApplication::postEvent(
                d->m_GUIcontext, new MythEvent("CONNECTION_FAILURE"));

            return false;
        }
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
        if (serverSock->ConnectToHost(hostname, port))
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

                serverSock->DecrRef();
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

        serverSock->DecrRef();
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
    MythSocket *eventSock = new MythSocket(-1, this);

    // Assume that since we _just_ connected the command socket,
    // this one won't need multiple retries to work...
    if (!eventSock->ConnectToHost(hostname, port))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to connect event "
                                       "socket to master backend");
        eventSock->DecrRef();
        return NULL;
    }

    QString str = QString("ANN Monitor %1 %2")
        .arg(d->m_localHostname).arg(true);
    QStringList strlist(str);
    eventSock->WriteStringList(strlist);
    bool ok = true;
    if (!eventSock->ReadStringList(strlist) || strlist.empty() ||
        (strlist[0] == "ERROR"))
    {
        if (!strlist.empty())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Problem connecting event socket to master backend");
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Timeout connecting event socket to master backend");
        }
        ok = false;
    }

    if (!ok)
    {
        eventSock->DecrRef();
        eventSock = NULL;
    }

    return eventSock;
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
    d->m_serverSock->SendReceiveStringList(strlist);

    d->m_blockingClient = true;
}

void MythCoreContext::AllowShutdown(void)
{
    QStringList strlist;

    QMutexLocker locker(&d->m_sockLock);
    if (d->m_serverSock == NULL)
        return;

    strlist << "ALLOW_SHUTDOWN";
    d->m_serverSock->SendReceiveStringList(strlist);

    d->m_blockingClient = false;
}

bool MythCoreContext::IsBlockingClient(void) const
{
    return d->m_blockingClient;
}

void MythCoreContext::SetBackend(bool backend)
{
    d->m_backend = backend;
}

bool MythCoreContext::IsBackend(void) const
{
    return d->m_backend;
}

bool MythCoreContext::IsMasterHost(void)
{
    QString host = GetHostName();
    return IsMasterHost(host);
}

bool MythCoreContext::IsMasterHost(const QString &host)
{
    return IsThisHost(GetSetting("MasterServerIP"), host);
}

bool MythCoreContext::IsMasterBackend(void)
{
    return (IsBackend() && IsMasterHost());
}

bool MythCoreContext::BackendIsRunning(void)
{
#if CONFIG_DARWIN || (__FreeBSD__) || defined(__OpenBSD__)
    const char *command = "ps -axc | grep -i mythbackend | grep -v grep > /dev/null";
#elif defined _WIN32
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

bool MythCoreContext::IsThisHost(const QString &addr)
{
    return IsThisHost(addr, GetHostName());
}

bool MythCoreContext::IsThisHost(const QString &addr, const QString &host)
{
    QString thisip = GetSettingOnHost("BackendServerIP", host);
    QString thisip6 = GetSettingOnHost("BackendServerIP6", host);

    return ((addr == thisip) || (addr == thisip6));
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

    // IPv6 addresses may contain % followed by a digit which causes .arg()
    // to fail, so use append instead.
    ret = QString("myth://").append(m_storageGroup).append(m_host).append(m_port).append(seperator).append(path);

#if 0
    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("GenMythURL returning %1").arg(ret));
#endif

    return ret;
}

QString MythCoreContext::GetMasterHostPrefix(const QString &storageGroup,
                                             const QString &path)
{
    return GenMythURL(GetSetting("MasterServerIP"),
                        GetNumSetting("MasterServerPort", 6543),
                        path,
                        storageGroup);
}

QString MythCoreContext::GetMasterHostName(void)
{
    QMutexLocker locker(&d->m_masterHostLock);

    if (d->m_masterHostname.isEmpty())
    {

        if (IsMasterBackend())
            d->m_masterHostname = d->m_localHostname;
        else
        {
            QStringList strlist("QUERY_HOSTNAME");

            if (SendReceiveStringList(strlist))
                d->m_masterHostname = strlist[0];
        }
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
    QMutexLocker (&d->m_localHostLock);
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

#if 0
QString MythCoreContext::GetMasterServerIP(void)
{
    QString saddr = GetSetting("MasterServerIP");
    QHostAddress addr(saddr);

    if (!d->m_IPv6.empty() &&
            (addr.protocol() == QAbstractSocket::IPv6Protocol))
        // we have IPv6 addresses, assume we can connect to them
        return saddr;
    else if (!d->m_IPv4.empty() &&
            (addr.protocol() == QAbstractSocket::IPv4Protocol))
        // we have IPv4 addresses, assume we can connect to them
        return saddr;
    else
        return GetBackendServerIP(GetMasterHostName());
}
#endif

QString MythCoreContext::GetBackendServerIP(void)
{
    return GetBackendServerIP(d->m_localHostname);
}

QString MythCoreContext::GetBackendServerIP(const QString &host)
{
    QString addr4, addr6;
#if !defined(QT_NO_IPV6)
    if (!ServerPool::DefaultListenIPv6().isEmpty())
        // we have IPv6 addresses, assume we can connect to them
        addr6 = GetSettingOnHost("BackendServerIP6", host, "");
#endif
    if (!ServerPool::DefaultListenIPv4().isEmpty())
        addr4 = GetSettingOnHost("BackendServerIP", host, "");

    if (addr6.isEmpty())
    {
        if (addr4.isEmpty())
        {
            LOG(VB_GENERAL, LOG_ERR, "No address defined for host: "+host);
            return "";
        }

        // IPv6 is empty, so return this regardless
        return addr4;
    }
    else if ((QHostAddress(addr6) == QHostAddress::LocalHostIPv6) &&
              !addr4.isEmpty() &&
             (QHostAddress(addr4) != QHostAddress::LocalHost))
        // IPv6 set to localhost, but IPv4 address if network accessible
        return addr4;
    else
        return addr6;
}

void MythCoreContext::OverrideSettingForSession(const QString &key,
                                                const QString &value)
{
    d->m_database->OverrideSettingForSession(key, value);
}

void MythCoreContext::ClearOverrideSettingForSession(const QString &key)
{
    d->m_database->ClearOverrideSettingForSession(key);
}

bool MythCoreContext::IsUIThread(void)
{
    return is_current_thread(d->m_UIThread);
}

bool MythCoreContext::SendReceiveStringList(
    QStringList &strlist, bool quickTimeout, bool block)
{
    QString msg;
    if (HasGUI() && IsUIThread())
    {
        msg = "SendReceiveStringList(";
        for (uint i=0; i<(uint)strlist.size() && i<2; i++)
            msg += (i?",":"") + strlist[i];
        msg += (strlist.size() > 2) ? "...)" : ")";
        LOG(VB_GENERAL, LOG_DEBUG, msg + " called from UI thread");
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
        uint timeout = quickTimeout ?
            MythSocket::kShortTimeout : MythSocket::kLongTimeout;
        ok = d->m_serverSock->SendReceiveStringList(strlist, 0, timeout);

        if (!ok)
        {
            LOG(VB_GENERAL, LOG_NOTICE,
                QString("Connection to backend server lost"));
            d->m_serverSock->DecrRef();
            d->m_serverSock = NULL;

            if (d->m_eventSock)
            {
                d->m_eventSock->DecrRef();
                d->m_eventSock = NULL;
            }

            bool blockingClient = GetNumSetting("idleTimeoutSecs",0);
            ConnectToMasterServer(blockingClient);

            if (d->m_serverSock)
            {
                ok = d->m_serverSock->SendReceiveStringList(
                    strlist, 0, timeout);
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

            ok = d->m_serverSock->ReadStringList(strlist, timeout);
        }

        if (!ok)
        {
            if (d->m_serverSock)
            {
                d->m_serverSock->DecrRef();
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

class SendAsyncMessage : public QRunnable
{
  public:
    SendAsyncMessage(const QString &msg, const QStringList &extra) :
        m_message(msg), m_extraData(extra)
    {
    }

    SendAsyncMessage(const QString &msg) : m_message(msg) { }

    void run(void)
    {
        QStringList strlist("MESSAGE");
        strlist << m_message;
        strlist << m_extraData;
        gCoreContext->SendReceiveStringList(strlist);
    }

  private:
    QString m_message;
    QStringList m_extraData;
};

void MythCoreContext::SendMessage(const QString &message)
{
    if (IsBackend())
    {
        dispatch(MythEvent(message));
    }
    else
    {
        MThreadPool::globalInstance()->start(
            new SendAsyncMessage(message), "SendMessage");
    }
}

void MythCoreContext::SendEvent(const MythEvent &event)
{
    if (IsBackend())
    {
        dispatch(event);
    }
    else
    {
        MThreadPool::globalInstance()->start(
            new SendAsyncMessage(event.Message(), event.ExtraDataList()),
            "SendEvent");
    }
}

void MythCoreContext::SendSystemEvent(const QString &msg)
{
    if (QCoreApplication::applicationName() == MYTH_APPNAME_MYTHTV_SETUP)
        return;

    SendMessage(QString("SYSTEM_EVENT %1 SENDER %2")
                        .arg(msg).arg(GetHostName()));
}

void MythCoreContext::SendHostSystemEvent(const QString &msg,
                                          const QString &hostname,
                                          const QString &args)
{
    SendSystemEvent(QString("%1 HOST %2 %3").arg(msg).arg(hostname).arg(args));
}


void MythCoreContext::readyRead(MythSocket *sock)
{
    do
    {
        QStringList strlist;
        if (!sock->ReadStringList(strlist))
            continue;

        if (strlist.size() < 2)
            continue;

        QString prefix = strlist[0];
        QString message = strlist[1];
        QStringList tokens = message.split(" ", QString::SkipEmptyParts);

        if (prefix == "OK")
        {
        }
        else if (prefix != "BACKEND_MESSAGE")
        {
            LOG(VB_NETWORK, LOG_ERR,
                    QString("Received a: %1 message from the backend "
                            "but I don't know what to do with it.")
                        .arg(prefix));
        }
        else if (message == "CLEAR_SETTINGS_CACHE")
        {
            // No need to dispatch this message to ourself, so handle it
            LOG(VB_NETWORK, LOG_INFO, "Received remote 'Clear Cache' request");
            ClearSettingsCache();
        }
        else if (message.startsWith("FILE_WRITTEN"))
        {
            QString file;
            uint64_t size;
            int NUMTOKENS = 3; // Number of tokens expected

            if (tokens.size() == NUMTOKENS)
            {
                file = tokens[1];
                size = tokens[2].toULongLong();
            }
            else
            {
                LOG(VB_NETWORK, LOG_ERR, LOC +
                    QString("FILE_WRITTEN event received "
                            "with invalid number of arguments, "
                            "%1 expected, %2 actual")
                    .arg(NUMTOKENS-1)
                    .arg(tokens.size()-1));
                return;
            }
            // No need to dispatch this message to ourself, so handle it
            LOG(VB_NETWORK, LOG_INFO, LOC +
                QString("Received remote 'FILE_WRITTEN %1' request").arg(file));
            RegisterFileForWrite(file, size);
        }
        else if (message.startsWith("FILE_CLOSED"))
        {
            QString file;
            int NUMTOKENS = 2; // Number of tokens expected

            if (tokens.size() == NUMTOKENS)
            {
                file = tokens[1];
            }
            else
            {
                LOG(VB_NETWORK, LOG_ERR, LOC +
                    QString("FILE_CLOSED event received "
                            "with invalid number of arguments, "
                            "%1 expected, %2 actual")
                    .arg(NUMTOKENS-1)
                    .arg(tokens.size()-1));
                return;
            }
            // No need to dispatch this message to ourself, so handle it
            LOG(VB_NETWORK, LOG_INFO, LOC +
                QString("Received remote 'FILE_CLOSED %1' request").arg(file));
            UnregisterFileForWrite(file);
        }
        else
        {
            strlist.pop_front();
            strlist.pop_front();
            MythEvent me(message, strlist);
            dispatch(me);
        }
    }
    while (sock->IsDataAvailable());
}

void MythCoreContext::connectionClosed(MythSocket *sock)
{
    (void)sock;

    LOG(VB_GENERAL, LOG_NOTICE,
        "Event socket closed.  No connection to the backend.");

    dispatch(MythEvent("BACKEND_SOCKETS_CLOSED"));
}

bool MythCoreContext::CheckProtoVersion(MythSocket *socket, uint timeout_ms,
                                        bool error_dialog_desired)
{
    if (!socket)
        return false;

    QStringList strlist(QString("MYTH_PROTO_VERSION %1 %2")
                        .arg(MYTH_PROTO_VERSION).arg(MYTH_PROTO_TOKEN));
    socket->WriteStringList(strlist);

    if (!socket->ReadStringList(strlist, timeout_ms) || strlist.empty())
    {
        LOG(VB_GENERAL, LOG_CRIT, "Protocol version check failure.\n\t\t\t"
                "The response to MYTH_PROTO_VERSION was empty.\n\t\t\t"
                "This happens when the backend is too busy to respond,\n\t\t\t"
                "or has deadlocked due to bugs or hardware failure.");

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
        if (!d->m_announcedProtocol)
        {
            d->m_announcedProtocol = true;
            LOG(VB_GENERAL, LOG_INFO, QString("Using protocol version %1")
                                              .arg(MYTH_PROTO_VERSION));
        }

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

void MythCoreContext::SetLocalHostname(const QString &hostname)
{
    QMutexLocker locker(&d->m_localHostLock);
    d->m_localHostname = hostname;
    d->m_database->SetLocalHostname(hostname);
}

void MythCoreContext::SetGUIObject(QObject *gui)
{
    d->m_GUIobject = gui;
}

bool MythCoreContext::HasGUI(void) const
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

MythLocale *MythCoreContext::GetLocale(void) const
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

    QString localeCode = d->m_locale->GetLocaleCode();
    LOG(VB_GENERAL, LOG_NOTICE, QString("Setting QT default locale to %1")
                                                            .arg(localeCode));
    QLocale::setDefault(d->m_locale->ToQLocale());
}

void MythCoreContext::ReInitLocale(void)
{
    if (!d->m_locale)
        d->m_locale = new MythLocale();
    else
        d->m_locale->ReInit();

    QString localeCode = d->m_locale->GetLocaleCode();
    LOG(VB_GENERAL, LOG_NOTICE, QString("Setting QT default locale to %1")
                                                            .arg(localeCode));
    QLocale::setDefault(d->m_locale->ToQLocale());
}

const QLocale MythCoreContext::GetQLocale(void)
{
    if (!d->m_locale)
        InitLocale();

    return d->m_locale->ToQLocale();
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

/**
 * \fn void MythCoreContext::WaitUntilSignals(const char *signal1, ...)
 * Wait until either of the provided signals have been received.
 * signal1 being declared as SIGNAL(SignalName(args,..))
 */
void MythCoreContext::WaitUntilSignals(const char *signal1, ...)
{
    if (!signal1)
        return;

    const char *s;
    QEventLoop eventLoop;
    va_list vl;

    LOG(VB_GENERAL, LOG_DEBUG, LOC +
        QString("Waiting for signal %1")
        .arg(signal1));
    connect(this, signal1, &eventLoop, SLOT(quit()));

    va_start(vl, signal1);
    s = va_arg(vl, const char *);
    while (s)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC +
            QString("Waiting for signal %1")
            .arg(s));
        connect(this, s, &eventLoop, SLOT(quit()));
        s = va_arg(vl, const char *);
    }
    va_end(vl);

    eventLoop.exec(QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers);
}

/**
 * \fn void MythCoreContext::RegisterForPlayback(QObject *sender, const char *method)
 * Register sender for TVPlaybackAboutToStart signal. Method will be called upon
 * the signal being emitted.
 * sender must call MythCoreContext::UnregisterForPlayback upon deletion
 */
void MythCoreContext::RegisterForPlayback(QObject *sender, const char *method)
{
    if (!sender || !method)
        return;

    QMutexLocker lock(&d->m_playbackLock);

    if (!d->m_playbackClients.contains(sender))
    {
        d->m_playbackClients.insert(sender, QByteArray(method));
        connect(this, SIGNAL(TVPlaybackAboutToStart()),
                sender, method,
                Qt::BlockingQueuedConnection);
    }
}

/**
 * \fn void MythCoreContext::UnregisterForPlayback(QObject *sender)
 * Unregister sender from being called when TVPlaybackAboutToStart signal
 * is emitted
 */
void MythCoreContext::UnregisterForPlayback(QObject *sender)
{
    QMutexLocker lock(&d->m_playbackLock);

    if (d->m_playbackClients.contains(sender))
    {
        QByteArray ba = d->m_playbackClients.value(sender);
        const char *method = ba.constData();
        disconnect(this, SIGNAL(TVPlaybackAboutToStart()),
                   sender, method);
        d->m_playbackClients.remove(sender);
    }
}

/**
 * All the objects that have registered using
 * MythCoreContext::RegisterForPlayback
 * but sender will be called. The object's registered method will be called
 * in a blocking fashion, each of them being called one after the other
 */
void MythCoreContext::WantingPlayback(QObject *sender)
{
    QMutexLocker lock(&d->m_playbackLock);
    QByteArray ba;
    const char *method = NULL;
    d->m_inwanting = true;

    // If any registered client are in the same thread, they will deadlock, so rebuild
    // connections for any clients in the same thread as non-blocking connection
    QThread *currentThread = QThread::currentThread();

    QMap<QObject *, QByteArray>::iterator it = d->m_playbackClients.begin();
    for (; it != d->m_playbackClients.end(); ++it)
    {
        if (it.key() == sender)
            continue;   // will be done separately, no need to do it again

        QThread *thread = it.key()->thread();

        if (thread != currentThread)
            continue;

        disconnect(this, SIGNAL(TVPlaybackAboutToStart()), it.key(), it.value());
        connect(this, SIGNAL(TVPlaybackAboutToStart()), it.key(), it.value());
    }

    // disconnect sender so it won't receive the message
    if (d->m_playbackClients.contains(sender))
    {
        ba = d->m_playbackClients.value(sender);
        method = ba.constData();
        disconnect(this, SIGNAL(TVPlaybackAboutToStart()), sender, method);
    }

    // emit signal
    emit TVPlaybackAboutToStart();

    // reconnect sender
    if (method)
    {
        connect(this, SIGNAL(TVPlaybackAboutToStart()),
                sender, method,
                Qt::BlockingQueuedConnection);
    }
    // Restore blocking connections
    for (; it != d->m_playbackClients.end(); ++it)
    {
        if (it.key() == sender)
            continue;   // already done above, no need to do it again

        QThread *thread = it.key()->thread();

        if (thread != currentThread)
            continue;

        disconnect(this, SIGNAL(TVPlaybackAboutToStart()), it.key(), it.value());
        connect(this, SIGNAL(TVPlaybackAboutToStart()),
                it.key(), it.value(), Qt::BlockingQueuedConnection);
    }
    d->m_inwanting = false;
}

/**
 * Let the TV class tell us if we was interrupted following a call to
 * WantingPlayback(). TV playback will later issue a TVPlaybackStopped() signal
 * which we want to be able to filter
 */
void MythCoreContext::TVInWantingPlayback(bool b)
{
    // when called, it will be while the m_playbackLock is held
    // following a call to WantingPlayback
    d->m_intvwanting = b;
}

/**
 * Returns true if a client has requested playback.
 * this can be used when one of the TVPlayback* is emitted to find out if you
 * can assume playback has stopped
 */
bool MythCoreContext::InWantingPlayback(void)
{
    bool locked = d->m_playbackLock.tryLock();
    bool intvplayback = d->m_intvwanting;

    if (!locked && d->m_inwanting)
        return true; // we're in the middle of WantingPlayback

    if (!locked)
        return false;

    d->m_playbackLock.unlock();

    return intvplayback;
}

bool MythCoreContext::TestPluginVersion(const QString &name,
                                   const QString &libversion,
                                   const QString &pluginversion)
{
    if (libversion == pluginversion)
        return true;

    LOG(VB_GENERAL, LOG_EMERG,
             QString("Plugin %1 (%2) binary version does not "
                     "match libraries (%3)")
                 .arg(name).arg(pluginversion).arg(libversion));
    return false;
}

void MythCoreContext::SetPluginManager(MythPluginManager *pmanager)
{
    if (d->m_pluginmanager == pmanager)
        return;

    if (d->m_pluginmanager)
    {
        delete d->m_pluginmanager;
        d->m_pluginmanager = NULL;
    }

    d->m_pluginmanager = pmanager;
}

MythPluginManager *MythCoreContext::GetPluginManager(void)
{
    return d->m_pluginmanager;
}

void MythCoreContext::SetExiting(bool exiting)
{
    d->m_isexiting = exiting;
}

bool MythCoreContext::IsExiting(void)
{
    return d->m_isexiting;
}

void MythCoreContext::RegisterFileForWrite(const QString& file, uint64_t size)
{
    QMutexLocker lock(&d->m_fileslock);

    QPair<int64_t, uint64_t> pair(QDateTime::currentMSecsSinceEpoch(), size);
    d->m_fileswritten.insert(file, pair);

    if (IsBackend())
    {
        QString message = QString("FILE_WRITTEN %1 %2").arg(file).arg(size);
        MythEvent me(message);
        dispatch(me);
    }

    LOG(VB_FILE, LOG_DEBUG, LOC +
        QString("Registering File %1 for write").arg(file));
}

void MythCoreContext::UnregisterFileForWrite(const QString& file)
{
    QMutexLocker lock(&d->m_fileslock);

    d->m_fileswritten.remove(file);

    if (IsBackend())
    {
        QString message = QString("FILE_CLOSED %1").arg(file);
        MythEvent me(message);
        dispatch(me);
    }

    LOG(VB_FILE, LOG_DEBUG, LOC +
        QString("Unregistering File %1 for write").arg(file));
}

bool MythCoreContext::IsRegisteredFileForWrite(const QString& file)
{
    QMutexLocker lock(&d->m_fileslock);

    return d->m_fileswritten.contains(file);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
