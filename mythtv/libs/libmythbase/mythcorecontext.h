#ifndef MYTHCORECONTEXT_H_
#define MYTHCORECONTEXT_H_

#include <QObject>
#include <QString>
#include <QHostAddress>

#include "mythdb.h"
#include "mythbaseexp.h"
#include "mythobservable.h"
#include "mythsocket_cb.h"
#include "mythlogging.h"
#include "mythlocale.h"

#define MYTH_APPNAME_MYTHBACKEND "mythbackend"
#define MYTH_APPNAME_MYTHJOBQUEUE "mythjobqueue"
#define MYTH_APPNAME_MYTHFRONTEND "mythfrontend"
#define MYTH_APPNAME_MYTHTV_SETUP "mythtv-setup"
#define MYTH_APPNAME_MYTHFILLDATABASE "mythfilldatabase"
#define MYTH_APPNAME_MYTHCOMMFLAG "mythcommflag"
#define MYTH_APPNAME_MYTHCCEXTRACTOR "mythccextractor"
#define MYTH_APPNAME_MYTHPREVIEWGEN "mythpreviewgen"
#define MYTH_APPNAME_MYTHTRANSCODE "mythtranscode"
#define MYTH_APPNAME_MYTHWELCOME "mythwelcome"
#define MYTH_APPNAME_MYTHSHUTDOWN "mythshutdown"
#define MYTH_APPNAME_MYTHMESSAGE "mythmessage"
#define MYTH_APPNAME_MYTHLCDSERVER "mythlcdserver"
#define MYTH_APPNAME_MYTHAVTEST "mythavtest"
#define MYTH_APPNAME_MYTHMEDIASERVER "mythmediaserver"
#define MYTH_APPNAME_MYTHMETADATALOOKUP "mythmetadatalookup"
#define MYTH_APPNAME_MYTHUTIL "mythutil"

class MDBManager;
class MythCoreContextPrivate;
class MythSocket;
class MythScheduler;

/** \class MythCoreContext
 *  \brief This class contains the runtime context for MythTV.
 *
 *   This class can be used to query for and set global and host
 *   settings, and is used to communicate between the frontends
 *   and backends. It also contains helper functions for theming
 *   and for getting system defaults, parsing the command line, etc.
 *   It also contains support for database error printing, and
 *   database message logging.
 */
class MBASE_PUBLIC MythCoreContext : public MythObservable, public MythSocketCBs
{
  public:
    MythCoreContext(const QString &binversion, QObject *eventHandler);
    virtual ~MythCoreContext();

    bool Init(void);

    void SetLocalHostname(const QString &hostname);
    void SetServerSocket(MythSocket *serverSock);
    void SetEventSocket(MythSocket *eventSock);
    void SetScheduler(MythScheduler *sched);

    bool ConnectToMasterServer(bool blockingClient = true,
                               bool openEventSocket = true);

    MythSocket *ConnectCommandSocket(const QString &hostname, int  port,
                                     const QString &announcement,
                                     bool *proto_mismatch = NULL,
                                     bool gui = true, int maxConnTry = -1,
                                     int setup_timeout = -1);

    MythSocket *ConnectEventSocket(const QString &hostname, int port);

    bool SetupCommandSocket(MythSocket *serverSock, const QString &announcement,
                            uint timeout_in_ms, bool &proto_mismatch);

    bool CheckProtoVersion(MythSocket *socket,
                           uint timeout_ms = kMythSocketLongTimeout,
                           bool error_dialog_desired = false);

    void                ConfigureHostAddress(void);
    QList<QHostAddress> MythHostAddress(void);
    QList<QHostAddress> MythHostAddress4(void);
    QList<QHostAddress> MythHostAddress6(void);

    QString GenMythURL(QString host = QString(), QString port = QString(),
                       QString path = QString(), QString storageGroup = QString());

    QString GenMythURL(QString host = QString(), int port = 0,
                       QString path = QString(), QString storageGroup = QString());

    QString GetMasterHostPrefix(QString storageGroup = QString());
    QString GetMasterHostName(void);
    QString GetHostName(void);
    QString GetFilePrefix(void);

    bool IsConnectedToMaster(void);
    void SetBackend(bool backend);
    bool IsBackend(void) const;        ///< is this process a backend process
    bool IsFrontendOnly(void);   ///< is there a frontend, but no backend,
                                 ///  running on this host
    bool IsMasterHost(void);     ///< is this the same host as the master
    bool IsMasterHost(const QString &host); //< is specified host the master
    bool IsMasterBackend(void);  ///< is this the actual MBE process
    bool BackendIsRunning(void); ///< a backend process is running on this host

    bool IsThisHost(const QString &addr); //< is this address mapped to this host
    bool IsThisHost(const QString &addr, const QString &host);

    void BlockShutdown(void);
    void AllowShutdown(void);
    bool IsBlockingClient(void) const; ///< is this client blocking shutdown

    bool SendReceiveStringList(QStringList &strlist, bool quickTimeout = false,
                               bool block = true);
    void SendMessage(const QString &message);
    void SendEvent(const MythEvent &event);
    void SendSystemEvent(const QString &msg);
    void SendHostSystemEvent(const QString &msg, const QString &hostname,
                             const QString &args);

    void SetGUIObject(QObject *gui);
    QObject *GetGUIObject(void);
    bool HasGUI(void) const;
    bool IsUIThread(void);

    MythDB *GetDB(void);
    MDBManager *GetDBManager(void);
    MythScheduler *GetScheduler(void);

    bool IsDatabaseIgnored(void) const;
    DatabaseParams GetDatabaseParams(void)
        { return GetDB()->GetDatabaseParams(); }

    void SaveSetting(const QString &key, int newValue);
    void SaveSetting(const QString &key, const QString &newValue);
    QString GetSetting(const QString &key, const QString &defaultval = "");
    bool SaveSettingOnHost(const QString &key, const QString &newValue,
                           const QString &host);

    // Convenience setting query methods
    int GetNumSetting(const QString &key, int defaultval = 0);
    double GetFloatSetting(const QString &key, double defaultval = 0.0);
    void GetResolutionSetting(const QString &type, int &width, int &height,
                              double& forced_aspect, double &refreshrate,
                              int index=-1);
    void GetResolutionSetting(const QString &type, int &width, int &height,
                              int index=-1);

    QString GetSettingOnHost(const QString &key, const QString &host,
                             const QString &defaultval = "");
    int GetNumSettingOnHost(const QString &key, const QString &host,
                            int defaultval = 0);
    double GetFloatSettingOnHost(const QString &key, const QString &host,
                                 double defaultval = 0.0);

    QString GetBackendServerIP(void);
    QString GetBackendServerIP(const QString &host);

    void SetSetting(const QString &key, const QString &newValue);

    void ClearSettingsCache(const QString &myKey = QString(""));
    void ActivateSettingsCache(bool activate = true);
    void OverrideSettingForSession(const QString &key, const QString &value);

    void dispatch(const MythEvent &event);
    void dispatchNow(const MythEvent &event); // MDEPRECATED;

    void InitLocale(void);
    const MythLocale *GetLocale(void) const;
    void SaveLocaleDefaults(void);
    QString GetLanguage(void);
    QString GetLanguageAndVariant(void);
    void ResetLanguage(void);

  private:
    MythCoreContextPrivate *d;

    void connected(MythSocket *sock)         { (void)sock; }
    void connectionFailed(MythSocket *sock)  { (void)sock; }
    void connectionClosed(MythSocket *sock);
    void readyRead(MythSocket *sock);
};

/// This global variable contains the MythCoreContext instance for the app
extern MBASE_PUBLIC MythCoreContext *gCoreContext;

/// This global variable is used to makes certain calls to avlib threadsafe.
extern MBASE_PUBLIC QMutex *avcodeclock;

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
