#include <QCoreApplication>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>

#include <cmath>

#include <queue>
#include <algorithm>
using namespace std;

#include "mythconfig.h"       // for CONFIG_DARWIN
#include "mythcorecontext.h"
#include "mythsocket.h"
#include "mythsystem.h"

#include "mythversion.h"

#ifdef USING_MINGW
#include <winsock2.h>
#include <unistd.h>
#include "compat.h"
#endif

#define LOC      QString("MythCoreContext: ")
#define LOC_WARN QString("MythCoreContext, Warning: ")
#define LOC_ERR  QString("MythCoreContext, Error: ")

MythCoreContext *gCoreContext = NULL;
QMutex *avcodeclock = new QMutex(QMutex::Recursive);

class MythCoreContextPrivate : public QObject
{
  public:
    MythCoreContextPrivate(MythCoreContext *lparent, QString binversion,
                           QObject *guicontext);
   ~MythCoreContextPrivate();

    bool WaitForWOL(int timeout_ms = INT_MAX);

    void LoadLogSettings(void);

  public:
    MythCoreContext *m_parent;
    QObject         *m_GUIcontext;
    QObject         *m_GUIobject;
    QString          m_appBinaryVersion;
    QString          m_appName;

    QMutex  m_hostnameLock;      ///< Locking for thread-safe copying of:
    QString m_localHostname;     ///< hostname from mysql.txt or gethostname()
    QString m_masterHostname;    ///< master backend hostname

    QMutex      m_sockLock;      ///< protects both m_serverSock and m_eventSock
    MythSocket *m_serverSock;    ///< socket for sending MythProto requests
    MythSocket *m_eventSock;     ///< socket events arrive on

    int                   m_logenable;
    int                   m_logmaxcount;
    int                   m_logprintlevel;
    QMap<QString,int>     m_lastLogCounts;
    QMap<QString,QString> m_lastLogStrings;

    QMutex         m_WOLInProgressLock;
    QWaitCondition m_WOLInProgressWaitCondition;
    bool           m_WOLInProgress;

    bool m_backend;

    MythDB *m_database;

    QThread *m_UIThread;

    MythLocale *m_locale;
    QString language;

    QMutex                 m_privMutex;
    queue<MythPrivRequest> m_privRequests;
    QWaitCondition         m_privQueued;

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
      m_logenable(-1), m_logmaxcount(-1), m_logprintlevel(-1),
      m_WOLInProgress(false),
      m_backend(false),
      m_database(GetMythDB()),
      m_UIThread(QThread::currentThread()),
      m_locale(new MythLocale())
{
}

MythCoreContextPrivate::~MythCoreContextPrivate()
{
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

    if (m_database)
        DestroyMythDB();
}

/// If another thread has already started WOL process, wait on them...
///
/// Note: Caller must be holding m_WOLInProgressLock.
bool MythCoreContextPrivate::WaitForWOL(int timeout_in_ms)
{
    int timeout_remaining = timeout_in_ms;
    while (m_WOLInProgress && (timeout_remaining > 0))
    {
        VERBOSE(VB_GENERAL, LOC + "Wake-On-LAN in progress, waiting...");

        int max_wait = min(1000, timeout_remaining);
        m_WOLInProgressWaitCondition.wait(
            &m_WOLInProgressLock, max_wait);
        timeout_remaining -= max_wait;
    }

    return !m_WOLInProgress;
}

void MythCoreContextPrivate::LoadLogSettings(void)
{
    m_logenable = m_parent->GetNumSetting("LogEnabled", 0);
    m_logmaxcount = m_parent->GetNumSetting("LogMaxCount", 0);
    m_logprintlevel = m_parent->GetNumSetting("LogPrintLevel", LP_ERROR);
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
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Init() Out-of-memory");
        return false;
    }

    if (d->m_appBinaryVersion != MYTH_BINARY_VERSION)
    {
        VERBOSE(VB_GENERAL, QString("Application binary version (%1) does not "
                                    "match libraries (%2)")
                                    .arg(d->m_appBinaryVersion)
                                    .arg(MYTH_BINARY_VERSION));

        QString warning = QObject::tr(
            "This application is not compatible "
            "with the installed MythTV libraries. "
            "Please recompile after a make distclean");
        VERBOSE(VB_IMPORTANT, warning);

        return false;
    }

    return true;
}

MythCoreContext::~MythCoreContext()
{
    delete d;
}

void MythCoreContext::SetAppName(QString appName)
{
    d->m_appName = appName;
}

QString MythCoreContext::GetAppName(void)
{
    return d->m_appName;
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
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Connecting server socket to "
                "master backend, socket write failed");
        return false;
    }

    if (!serverSock->readStringList(strlist, true) || strlist.empty() ||
        (strlist[0] == "ERROR"))
    {
        if (!strlist.empty())
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Problem connecting "
                    "server socket to master backend");
        else
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Timeout connecting "
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
        VERBOSE(VB_IMPORTANT, "ERROR: Master backend tried to connect back "
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

    if (!d->m_eventSock)
        d->m_eventSock = ConnectEventSocket(server, port);

    if (!d->m_eventSock)
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
    MythSocket *m_serverSock = NULL;

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
        VERBOSE(VB_GENERAL, LOC +
                QString("Connecting to backend server: %1:%2 (try %3 of %4)")
                .arg(hostname).arg(port).arg(cnt).arg(maxConnTry));

        m_serverSock = new MythSocket();

        int sleepms = 0;
        if (m_serverSock->connect(hostname, port))
        {
            if (SetupCommandSocket(
                    m_serverSock, announce, setup_timeout, proto_mismatch))
            {
                break;
            }

            if (proto_mismatch)
            {
                if (p_proto_mismatch)
                    *p_proto_mismatch = true;

                m_serverSock->DownRef();
                m_serverSock = NULL;
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

            myth_system(WOLcmd);
            sleepms = WOLsleepTime * 1000;
        }

        m_serverSock->DownRef();
        m_serverSock = NULL;

        if (!m_serverSock && (cnt == 1))
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

    if (!m_serverSock && !proto_mismatch)
    {
        VERBOSE(VB_IMPORTANT,
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

    return m_serverSock;
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
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to connect event "
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
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Problem connecting "
                    "event socket to master backend");
        else
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Timeout connecting "
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
    const char *command = "ps -ae | grep mythbackend > /dev/null";
#endif
    bool res = myth_system(command, kMSDontBlockInputDevs);
    return !res;
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

QString MythCoreContext::GetMasterHostPrefix(QString storageGroup)
{
    QString ret;

    if (IsMasterHost())
    {
        if (storageGroup.isEmpty())
            return QString("myth://%1:%2/").arg(GetSetting("MasterServerIP"))
                           .arg(GetNumSetting("MasterServerPort", 6543));
        else
            return QString("myth://%1@%2:%3/").arg(storageGroup)
                           .arg(GetSetting("MasterServerIP"))
                           .arg(GetNumSetting("MasterServerPort", 6543));
    }

    QMutexLocker locker(&d->m_sockLock);
    if (!d->m_serverSock)
    {
        bool blockingClient = GetNumSetting("idleTimeoutSecs",0) > 0;
        ConnectToMasterServer(blockingClient);
    }

    if (d->m_serverSock)
    {
        if (storageGroup.isEmpty())
            ret = QString("myth://%1:%2/")
                         .arg(d->m_serverSock->peerAddress().toString())
                         .arg(d->m_serverSock->peerPort());
        else
            ret = QString("myth://%1@%2:%3/")
                         .arg(storageGroup)
                         .arg(d->m_serverSock->peerAddress().toString())
                         .arg(d->m_serverSock->peerPort());
    }

    return ret;
}

QString MythCoreContext::GetMasterHostName(void)
{
    QMutexLocker locker(&d->m_hostnameLock);

    if (d->m_masterHostname.isEmpty())
    {
        QStringList strlist("QUERY_HOSTNAME");

        SendReceiveStringList(strlist);

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

void MythCoreContext::RefreshBackendConfig(void)
{
    d->LoadLogSettings();
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
    return (QThread::currentThread() == d->m_UIThread);
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
        VERBOSE(VB_IMPORTANT, msg);
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
            VERBOSE(VB_IMPORTANT, QString("Connection to backend server lost"));
            d->m_serverSock->DownRef();
            d->m_serverSock = NULL;

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
            VERBOSE(VB_IMPORTANT, "SRSL you shouldn't see this!!");
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

            VERBOSE(VB_IMPORTANT,
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
                VERBOSE(VB_GENERAL, QString("Protocol query '%1' reponded "
                                            "with the error '%2'")
                                            .arg(query_type).arg(strlist[1]));
            else
                VERBOSE(VB_GENERAL, QString("Protocol query '%1' reponded "
                                        "with an error, but no error message.")
                                        .arg(query_type));

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
            VERBOSE(VB_IMPORTANT,
                    QString("Received a: %1 message from the backend"
                    "\n\t\t\tBut I don't know what to do with it.")
                    .arg(prefix));
        }
        else if (message == "CLEAR_SETTINGS_CACHE")
        {
            // No need to dispatch this message to ourself, so handle it
            VERBOSE(VB_GENERAL, "Received a remote 'Clear Cache' request");
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

    VERBOSE(VB_IMPORTANT, QString("Event socket closed. "
            "No connection to the backend."));

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
        VERBOSE(VB_IMPORTANT, "Protocol version check failure.\n\t\t\t"
                "The response to MYTH_PROTO_VERSION was empty.\n\t\t\t"
                "This happens when the backend is too busy to respond,\n\t\t\t"
                "or has deadlocked in due to bugs or hardware failure.");

        return false;
    }
    else if (strlist[0] == "REJECT" && strlist.size() >= 2)
    {
        VERBOSE(VB_GENERAL, QString("Protocol version mismatch (frontend=%1,"
                                    "backend=%2)\n")
                                    .arg(MYTH_PROTO_VERSION).arg(strlist[1]));

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
        VERBOSE(VB_IMPORTANT, QString("Using protocol version %1")
                               .arg(MYTH_PROTO_VERSION));
        return true;
    }

    VERBOSE(VB_GENERAL, QString("Unexpected response to MYTH_PROTO_VERSION: %1")
                               .arg(strlist[0]));
    return false;
}

void MythCoreContext::LogEntry(const QString &module, int priority,
                               const QString &message, const QString &details)
{
    unsigned int logid;
    int howmany;

    if (IsDatabaseIgnored())
        return;

    if (d->m_logenable == -1) // Haven't grabbed the settings yet
        d->LoadLogSettings();
    if (d->m_logenable == 1)
    {
        QString fullMsg = message;
        if (!details.isEmpty())
            fullMsg += ": " + details;

        if (message.left(21) != "Last message repeated")
        {
            if (fullMsg == d->m_lastLogStrings[module])
            {
                d->m_lastLogCounts[module] += 1;
                return;
            }
            else
            {
                if (0 < d->m_lastLogCounts[module])
                {
                    LogEntry(module, priority,
                             QString("Last message repeated %1 times")
                                    .arg(d->m_lastLogCounts[module]),
                             d->m_lastLogStrings[module]);
                }

                d->m_lastLogCounts[module] = 0;
                d->m_lastLogStrings[module] = fullMsg;
            }
        }


        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("INSERT INTO mythlog (module, priority, "
                      "logdate, host, message, details) "
                      "values (:MODULE, :PRIORITY, now(), :HOSTNAME, "
                      ":MESSAGE, :DETAILS );");

        query.bindValue(":MODULE", module);
        query.bindValue(":PRIORITY", priority);
        query.bindValue(":HOSTNAME", d->m_localHostname);
        query.bindValue(":MESSAGE", message);
        query.bindValue(":DETAILS", details);

        if (!query.exec() || !query.isActive())
            MythDB::DBError("LogEntry", query);

        if (d->m_logmaxcount > 0)
        {
            query.prepare("SELECT logid FROM mythlog WHERE "
                          "module= :MODULE ORDER BY logdate ASC ;");
            query.bindValue(":MODULE", module);
            if (!query.exec() || !query.isActive())
            {
                MythDB::DBError("DelLogEntry#1", query);
            }
            else
            {
                howmany = query.size();
                if (howmany > d->m_logmaxcount)
                {
                    MSqlQuery delquery(MSqlQuery::InitCon());
                    while (howmany > d->m_logmaxcount)
                    {
                        query.next();
                        logid = query.value(0).toUInt();
                        delquery.prepare("DELETE FROM mythlog WHERE "
                                         "logid= :LOGID ;");
                        delquery.bindValue(":LOGID", logid);

                        if (!delquery.exec() || !delquery.isActive())
                        {
                            MythDB::DBError("DelLogEntry#2", delquery);
                        }
                        howmany--;
                    }
                }
            }
        }

        if (priority <= d->m_logprintlevel)
        {
            QByteArray tmp =
                QString("%1: %2").arg(module).arg(fullMsg).toUtf8();
            VERBOSE(VB_IMPORTANT, tmp.constData());
        }
    }
}

void MythCoreContext::addPrivRequest(MythPrivRequest::Type t, void *data)
{
    QMutexLocker lockit(&d->m_privMutex);
    d->m_privRequests.push(MythPrivRequest(t, data));
    d->m_privQueued.wakeAll();
}

void MythCoreContext::waitPrivRequest() const
{
    d->m_privMutex.lock();
    while (d->m_privRequests.empty())
        d->m_privQueued.wait(&d->m_privMutex);
    d->m_privMutex.unlock();
}

MythPrivRequest MythCoreContext::popPrivRequest()
{
    QMutexLocker lockit(&d->m_privMutex);
    MythPrivRequest ret_val(MythPrivRequest::PrivEnd, NULL);
    if (!d->m_privRequests.empty()) {
        ret_val = d->m_privRequests.front();
        d->m_privRequests.pop();
    }
    return ret_val;
}

void MythCoreContext::dispatch(const MythEvent &event)
{
    VERBOSE(VB_NETWORK, QString("MythEvent: %1").arg(event.Message()));

    MythObservable::dispatch(event);
}

void MythCoreContext::dispatchNow(const MythEvent &event)
{
    VERBOSE(VB_NETWORK, QString("MythEvent: %1").arg(event.Message()));

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
        d->language = GetSetting("Language", "EN_US").toLower();

    return d->language;
}

void MythCoreContext::ResetLanguage(void)
{
    d->language.clear();
}

void MythCoreContext::SaveLocaleDefaults(void)
{
    if (!d->m_locale->GetLocaleCode().isEmpty())
    {
        VERBOSE(VB_GENERAL, QString("Current locale %1")
                                        .arg(d->m_locale->GetLocaleCode()));

        d->m_locale->SaveLocaleDefaults();
        return;
    }

    VERBOSE(VB_IMPORTANT, QString("No locale defined! We weren't able to "
                                    "set locale defaults."));
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
