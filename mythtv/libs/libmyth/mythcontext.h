#ifndef MYTHCONTEXT_H_
#define MYTHCONTEXT_H_

#include <QObject>
#include <QString>

#include "mythexp.h"
#include "mythobservable.h"
#include "mythsocket_cb.h"
#include "mythverbose.h"

class QFont;
class QKeyEvent;

class Settings;
class MythMainWindow;
class MythPluginManager;
class MDBManager;
class MythContextPrivate;
class UPnp;
class MythImage;
class MythSocket;
struct DatabaseParams;

/// These are the database logging priorities used for filterig the logs.
enum LogPriorities
{
    LP_EMERG     = 0,
    LP_ALERT     = 1,
    LP_CRITICAL  = 2,
    LP_ERROR     = 3,
    LP_WARNING   = 4,
    LP_NOTICE    = 5,
    LP_INFO      = 6,
    LP_DEBUG     = 7
};

/** \class MythPrivRequest
 *  \brief Container for requests that require privledge escalation.
 *
 *   Currently this is used for just one thing, increasing the
 *   priority of the video output thread to a real-time priority.
 *   These requests are made by calling gContext->addPrivRequest().
 *
 *  \sa NuppelVideoPlayer::StartPlaying(void)
 *  \sa MythContext:addPrivRequest(MythPrivRequest::Type, void*)
 */
class MPUBLIC MythPrivRequest
{
  public:
    typedef enum { MythRealtime, MythExit, PrivEnd } Type;
    MythPrivRequest(Type t, void *data) : m_type(t), m_data(data) {}
    Type getType() const { return m_type; }
    void *getData() const { return m_data; }
  private:
    Type m_type;
    void *m_data;
};

class MythContextPrivate;
class MythContextSlotHandler : public QObject
{
    Q_OBJECT

  public:
    MythContextSlotHandler(MythContextPrivate *x) : d(x) { }

  private slots:
    void ConnectFailurePopupClosed(void);
    void VersionMismatchPopupClosed(void);

  private:
    ~MythContextSlotHandler() {}

    MythContextPrivate *d;
};

/** \class MythContext
 *  \brief This class contains the runtime context for MythTV.
 *
 *   This class can be used to query for and set global and host
 *   settings, and is used to communicate between the frontends
 *   and backends. It also contains helper functions for theming
 *   and for getting system defaults, parsing the command line, etc.
 *   It also contains support for database error printing, and
 *   database message logging.
 */
class MPUBLIC MythContext : public MythObservable, public MythSocketCBs
{
  public:
    MythContext(const QString &binversion);
    virtual ~MythContext();

    bool Init(const bool gui = true,
              UPnp *UPnPclient = NULL,
              const bool promptForBackend = false,
              const bool bypassAutoDiscovery = false,
              const bool ignoreDB = false);

    QString GetMasterHostPrefix(void);
    QString GetMasterHostName(void);

    QString GetHostName(void);

    void ClearSettingsCache(const QString &myKey = QString(""));
    void ActivateSettingsCache(bool activate = true);
    void OverrideSettingForSession(const QString &key, const QString &newValue);

    bool ConnectToMasterServer(bool blockingClient = true);

    MythSocket *ConnectCommandSocket(
        const QString &hostname, int  port, const QString &announcement,
        bool *proto_mismatch = NULL, bool gui = true,
        int maxConnTry = -1, int setup_timeout = -1);

    MythSocket *ConnectEventSocket(const QString &hostname, int port);

    bool IsConnectedToMaster(void);
    void SetBackend(bool backend);

    bool IsBackend(void);        ///< is this process a backend process
    bool IsFrontendOnly(void);   ///< is there a frontend, but no backend,
                                 ///  running on this host
    bool IsMasterHost(void);     ///< is this the same host as the master
    bool IsMasterBackend(void);  ///< is this the actual MBE process
    bool BackendIsRunning(void); ///< a backend process is running on this host

    void BlockShutdown(void);
    void AllowShutdown(void);

    QString GetFilePrefix(void);

    void RefreshBackendConfig(void);

    MDBManager *GetDBManager(void);

    DatabaseParams GetDatabaseParams(void);
    bool SaveDatabaseParams(const DatabaseParams &params);

    void LogEntry(const QString &module, int priority,
                  const QString &message, const QString &details);

    bool IsDatabaseIgnored(void) const;

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

    void SetSetting(const QString &key, const QString &newValue);

    bool SendReceiveStringList(QStringList &strlist, bool quickTimeout = false,
                               bool block = true);

    void SetMainWindow(MythMainWindow *mainwin);
    MythMainWindow *GetMainWindow(void);

    // deprecate
    bool TranslateKeyPress(const QString &context, QKeyEvent *e,
                           QStringList &actions, bool allowJumps = true);


    bool TestPopupVersion(const QString &name, const QString &libversion,
                          const QString &pluginversion);

    void SetDisableLibraryPopup(bool check);

    void SetPluginManager(MythPluginManager *pmanager);
    MythPluginManager *getPluginManager(void);

    static bool CheckProtoVersion(
        MythSocket *socket,
        uint timeout_ms = kMythSocketLongTimeout,
        MythContextPrivate *error_dialog_desired = NULL);

    void addPrivRequest(MythPrivRequest::Type t, void *data);
    void waitPrivRequest() const;
    MythPrivRequest popPrivRequest();

    void dispatch(const MythEvent &event);
    void dispatchNow(const MythEvent &event) MDEPRECATED;

    void sendPlaybackStart(void);
    void sendPlaybackEnd(void);

  private:
    void connected(MythSocket *sock);
    void connectionClosed(MythSocket *sock);
    void readyRead(MythSocket *sock);
    void connectionFailed(MythSocket *sock) { (void)sock; }

    MythContextPrivate *d;
    QString app_binary_version;
};

/// This global variable contains the MythContext instance for the application
extern MPUBLIC MythContext *gContext;

/// This global variable is used to makes certain calls to avlib threadsafe.
extern MPUBLIC QMutex *avcodeclock;

/// Service type for the backend's UPnP server
extern MPUBLIC const QString gBackendURI;

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
