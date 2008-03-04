#ifndef MYTHCONTEXT_H_
#define MYTHCONTEXT_H_

#include <qstring.h>
#include <qdatetime.h>
#include <qpixmap.h>
#include <qpalette.h>
#include <qobject.h>
#include <qptrlist.h>
#include <qevent.h>
#include <qmutex.h>
#include <qstringlist.h>
#include <qnetwork.h> 
#include <qmap.h>

#include <cerrno>
#include <iostream>
#include <sstream>
#include <vector>

#include "mythexp.h"
#include "mythobservable.h"
#include "mythsocket.h"
#include "mythverbose.h"

using namespace std;

#if (QT_VERSION < 0x030300)
#error You need Qt version >= 3.3.0 to compile MythTV.
#endif

class Settings;
class QSqlDatabase;
class QSqlQuery;
class QSqlError;
class MythMainWindow;
class MythPluginManager;
class MediaMonitor;
class MythMediaDevice;
class DisplayRes;
class MDBManager;
class MythContextPrivate;
class UPnp;

// VERBOSE macros, maps and globals are now in mythverbose.h
//
// This is an "extern" for code in mythcontext.cpp
MPUBLIC int parse_verbose_arg(QString arg);


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

/// Structure containing the basic Database parameters
struct MPUBLIC DatabaseParams
{
    QString dbHostName;         ///< database server
    bool    dbHostPing;         ///< Can we test connectivity using ping?
    int     dbPort;             ///< database port
    QString dbUserName;         ///< DB user name 
    QString dbPassword;         ///< DB password
    QString dbName;             ///< database name
    QString dbType;             ///< database type (MySQL, Postgres, etc.)
    
    bool    localEnabled;       ///< true if localHostName is not default
    QString localHostName;      ///< name used for loading/saving settings
    
    bool    wolEnabled;         ///< true if wake-on-lan params are used
    int     wolReconnect;       ///< seconds to wait for reconnect
    int     wolRetry;           ///< times to retry to reconnect
    QString wolCommand;         ///< command to use for wake-on-lan
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

/// Update this whenever the plug-in API changes.
/// Including changes in the libmythtv class methods used by plug-ins.
#define MYTH_BINARY_VERSION "0.21.20080304-1"

/** \brief Increment this whenever the MythTV network protocol changes.
 *
 *   You must also update this value and any corresponding changes to the
 *   ProgramInfo network protocol layout in the following files:
 *
 *   MythWeb
 *       mythplugins/mythweb/includes/mythbackend.php (version number)
 *       mythplugins/mythweb/modules/tv/includes/objects/Program.php (layout)
 *
 *   MythTV Perl Bindings
 *       mythtv/bindings/perl/MythTV.pm (version number)
 *       mythtv/bindings/perl/MythTV/Program.pm (layout)
 *
 *   MythTV Python Bindings
 *       mythtv/bindings/python/MythTV.py (version number)
 *       mythtv/bindings/python/MythTV.py (layout)
 */
#define MYTH_PROTO_VERSION "40"

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
class MPUBLIC MythContext : public QObject, public MythObservable,
    public MythSocketCBs
{
    Q_OBJECT
  public:
    MythContext(const QString &binversion);
    virtual ~MythContext();

    bool Init(const bool gui = true,
                    UPnp *UPnPclient = NULL,
              const bool promptForBackend = false,
              const bool bypassAutoDiscovery = false);

    QString GetMasterHostPrefix(void);

    QString GetHostName(void);

    void ClearSettingsCache(QString myKey = "", QString newVal = "");
    void ActivateSettingsCache(bool activate = true);
    void OverrideSettingForSession(const QString &key, const QString &newValue);

    bool ConnectToMasterServer(bool blockingClient = true);
    MythSocket *ConnectServer(MythSocket *eventSocket,
                              const QString &hostname,
                              int port,
                              bool blockingClient = false);
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

    QString GetInstallPrefix(void);
    QString GetShareDir(void);
    QString GetLibraryDir(void);
    static QString GetConfDir(void);

    QString GetFilePrefix(void);

    void LoadQtConfig(void);
    void UpdateImageCache(void);

    void RefreshBackendConfig(void);

    // Note that these give the dimensions for the GUI,
    // which the user may have set to be different from the raw screen size
    void GetScreenSettings(float &wmult, float &hmult);
    void GetScreenSettings(int &width, float &wmult,
                           int &height, float &hmult);
    void GetScreenSettings(int &xbase, int &width, float &wmult,
                           int &ybase, int &height, float &hmult);

    // This returns the raw (drawable) screen size
    void GetScreenBounds(int &xbase, int &ybase, int &width, int &height);

    // Parse an X11 style command line (-geometry) string
    bool ParseGeometryOverride(const QString geometry);

    QString FindThemeDir(const QString &themename);
    QString FindMenuThemeDir(const QString &menuname);
    QString GetThemeDir(void);
    QValueList<QString> GetThemeSearchPath(void);

    QString GetMenuThemeDir(void);

    QString GetThemesParentDir(void);

    QString GetPluginsDir(void);
    QString GetPluginsNameFilter(void);
    QString FindPlugin(const QString &plugname);

    QString GetTranslationsDir(void);
    QString GetTranslationsNameFilter(void);
    QString FindTranslation(const QString &translation);

    QString GetFontsDir(void);
    QString GetFontsNameFilter(void);
    QString FindFont(const QString &fontname);

    QString GetFiltersDir(void);

    MDBManager *GetDBManager(void);
    static void DBError(const QString &where, const QSqlQuery &query);
    static QString DBErrorMessage(const QSqlError& err);

    DatabaseParams GetDatabaseParams(void);
    bool SaveDatabaseParams(const DatabaseParams &params);
    
    void LogEntry(const QString &module, int priority,
                  const QString &message, const QString &details);

    Settings *qtconfig(void);

    void SaveSetting(const QString &key, int newValue);
    void SaveSetting(const QString &key, const QString &newValue);
    QString GetSetting(const QString &key, const QString &defaultval = "");
    bool SaveSettingOnHost(const QString &key, const QString &newValue,
                           const QString &host);

    // Convenience setting query methods
    int GetNumSetting(const QString &key, int defaultval = 0);
    double GetFloatSetting(const QString &key, double defaultval = 0.0);
    void GetResolutionSetting(const QString &type, int &width, int &height,
                              double& forced_aspect, short &refreshrate,
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

    QFont GetBigFont();
    QFont GetMediumFont();
    QFont GetSmallFont();

    QString GetLanguage(void);
    QString GetLanguageAndVariant(void);

    void ThemeWidget(QWidget *widget);

    bool FindThemeFile(QString &filename);
    QPixmap *LoadScalePixmap(QString filename, bool fromcache = true); 
    QImage *LoadScaleImage(QString filename, bool fromcache = true);

    bool SendReceiveStringList(QStringList &strlist, bool quickTimeout = false, 
                               bool block = true);

    QImage *CacheRemotePixmap(const QString &url, bool reCache = false);

    void SetMainWindow(MythMainWindow *mainwin);
    MythMainWindow *GetMainWindow(void);

    int  PromptForSchemaUpgrade(const QString &dbver, const QString &current,
                                const QString &backupResult);
    bool TestPopupVersion(const QString &name, const QString &libversion,
                          const QString &pluginversion);

    void SetDisableLibraryPopup(bool check);

    void SetPluginManager(MythPluginManager *pmanager);
    MythPluginManager *getPluginManager(void);

    bool CheckProtoVersion(MythSocket* socket);

    // event wrappers
    void DisableScreensaver(void);
    void RestoreScreensaver(void);
    // Reset screensaver idle time, for input events that X doesn't see
    // (e.g., lirc)
    void ResetScreensaver(void);

    // actually do it
    void DoDisableScreensaver(void);
    void DoRestoreScreensaver(void);
    void DoResetScreensaver(void);

    // get the current status
    bool GetScreensaverEnabled(void);
    bool GetScreenIsAsleep(void);

    void addPrivRequest(MythPrivRequest::Type t, void *data);
    void waitPrivRequest() const;
    MythPrivRequest popPrivRequest();

    void addCurrentLocation(QString location);
    QString removeCurrentLocation(void);
    QString getCurrentLocation(void);

    void dispatch(MythEvent &event);
    void dispatchNow(MythEvent &event);

    void sendPlaybackStart(void);
    void sendPlaybackEnd(void);

    static void SetX11Display(const QString &display);
    static QString GetX11Display(void);

    static QMutex verbose_mutex;
    static QString x11_display;

  private:
    void SetPalette(QWidget *widget);
    void InitializeScreenSettings(void);

    void ClearOldImageCache(void);
    void CacheThemeImages(void);
    void CacheThemeImagesDirectory(const QString &dirname, 
                                   const QString &subdirname = "");
    void RemoveCacheDir(const QString &dirname);

    void connected(MythSocket *sock);
    void connectionClosed(MythSocket *sock);
    void readyRead(MythSocket *sock);
    void connectionFailed(MythSocket *sock) { (void)sock; }

    MythContextPrivate *d;
    QString app_binary_version;

    QMutex locationLock;
    QValueList <QString> currentLocation;
};

/// This global variable contains the MythContext instance for the application
extern MPUBLIC MythContext *gContext;

/// This global variable is used to makes certain calls to avlib threadsafe.
extern MPUBLIC QMutex avcodeclock;

/// Return values for PromptForSchemaUpgrade()
enum MythSchemaUpgrade
{
    MYTH_SCHEMA_EXIT         = 1,
    MYTH_SCHEMA_ERROR        = 2,
    MYTH_SCHEMA_UPGRADE      = 3,
    MYTH_SCHEMA_USE_EXISTING = 4
};

/// Service type for the backend's UPnP server
extern MPUBLIC const QString gBackendURI;

#endif

/* vim: set expandtab tabstop=4 shiftwidth=4: */
