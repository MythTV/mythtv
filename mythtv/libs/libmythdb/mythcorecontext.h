#ifndef MYTHCORECONTEXT_H_
#define MYTHCORECONTEXT_H_

#include <QObject>
#include <QString>

#include "mythdb.h"
#include "mythexp.h"
#include "mythobservable.h"
#include "mythsocket_cb.h"
#include "mythverbose.h"

class MDBManager;
class MythCoreContextPrivate;
class MythSocket;

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
 *   These requests are made by calling gCoreContext->addPrivRequest().
 *
 *  \sa NuppelVideoPlayer::StartPlaying(void)
 *  \sa MythCoreContext:addPrivRequest(MythPrivRequest::Type, void*)
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
class MPUBLIC MythCoreContext : public MythObservable, public MythSocketCBs
{
  public:
    MythCoreContext(const QString &binversion, QObject *eventHandler);
    virtual ~MythCoreContext();

    bool Init(void);

    void SetLocalHostname(const QString &hostname);
    void SetServerSocket(MythSocket *serverSock);
    void SetEventSocket(MythSocket *eventSock);

    bool ConnectToMasterServer(bool blockingClient = true);

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

    QString GetMasterHostPrefix(void);
    QString GetMasterHostName(void);
    QString GetHostName(void);
    QString GetFilePrefix(void);

    void RefreshBackendConfig(void);

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

    bool SendReceiveStringList(QStringList &strlist, bool quickTimeout = false,
                               bool block = true);

    void SetGUIObject(QObject *gui);
    QObject *GetGUIObject(void);
    bool HasGUI(void);
    bool IsUIThread(void);

    MythDB *GetDB(void);
    MDBManager *GetDBManager(void);

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

    void SetSetting(const QString &key, const QString &newValue);

    void ClearSettingsCache(const QString &myKey = QString(""));
    void ActivateSettingsCache(bool activate = true);
    void OverrideSettingForSession(const QString &key, const QString &value);

    void addPrivRequest(MythPrivRequest::Type t, void *data);
    void waitPrivRequest() const;
    MythPrivRequest popPrivRequest();

    void dispatch(const MythEvent &event);
    void dispatchNow(const MythEvent &event) MDEPRECATED;

    void LogEntry(const QString &module, int priority,
                  const QString &message, const QString &details);

  private:
    MythCoreContextPrivate *d;

    void connected(MythSocket *sock)         { (void)sock; }
    void connectionFailed(MythSocket *sock)  { (void)sock; }
    void connectionClosed(MythSocket *sock);
    void readyRead(MythSocket *sock);
};

/// This global variable contains the MythCoreContext instance for the app
extern MPUBLIC MythCoreContext *gCoreContext;

/// This global variable is used to makes certain calls to avlib threadsafe.
extern MPUBLIC QMutex *avcodeclock;

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
