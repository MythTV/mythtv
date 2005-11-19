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
#include <qsocketdevice.h>
#include <qstringlist.h>
#include <qnetwork.h> 
#include <qdeepcopy.h>
#include <qmap.h>

#include <cerrno>
#include <iostream>
#include <sstream>
#include <vector>

using namespace std;

#if (QT_VERSION < 0x030100)
#error You need Qt version >= 3.1.0 to compile MythTV.
#endif

class Settings;
class QSqlDatabase;
class QSqlQuery;
class QSqlError;
class QSocket;
class QSocketDevice;
class MythMainWindow;
class MythPluginManager;
class MediaMonitor;
class MythMediaDevice;
class DisplayRes;
class MDBManager;
class MythContextPrivate;

/// These are the various VERBOSITY flags, used to select which
/// messages we want printed to the console.
enum VerboseMask {
    VB_IMPORTANT = 0x0001, ///< Errors or other very important messages
    VB_GENERAL   = 0x0002, ///< General info, printed out by default
    VB_RECORD    = 0x0004,
    VB_PLAYBACK  = 0x0008,
    VB_CHANNEL   = 0x0010,
    VB_OSD       = 0x0020,
    VB_FILE      = 0x0040,
    VB_SCHEDULE  = 0x0080,
    VB_NETWORK   = 0x0100,
    VB_COMMFLAG  = 0x0200,
    VB_AUDIO     = 0x0400,
    VB_LIBAV     = 0x0800,
    VB_JOBQUEUE  = 0x1000,
    VB_SIPARSER  = 0x2000,
    VB_EIT       = 0x4000,
    VB_NONE      = 0x0000,
    VB_ALL       = 0xffff
};

/// This global variable is set at startup with the flags 
/// of the verbose messages we want to see.
extern int print_verbose_messages;

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
struct DatabaseParams
{
    QString dbHostName;         ///< database server
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
    
// The verbose_mutex lock is a recursive lock so it is possible (while
// not recommended) to use a VERBOSE macro within another VERBOSE macro.
// But waiting for another thread to do something is not safe within a 
// VERBOSE macro, since those threads may wish to use the VERBOSE macro
// and this will cause a deadlock.
#ifdef DEBUG

#define VERBOSE(mask,args...) \
do { \
    if ((print_verbose_messages & mask) != 0) \
    { \
        QDateTime dtmp = QDateTime::currentDateTime(); \
        QString dtime = dtmp.toString("yyyy-MM-dd hh:mm:ss.zzz"); \
        MythContext::verbose_mutex.lock(); \
        cout << dtime << " " << args << endl; \
        MythContext::verbose_mutex.unlock(); \
    } \
} while (0)

#else // if !DEBUG

// use a slower non-deadlockable version in release builds
#define VERBOSE(mask,args...) \
do { \
    if ((print_verbose_messages & mask) != 0) \
    { \
        QDateTime dtmp = QDateTime::currentDateTime(); \
        QString dtime = dtmp.toString("yyyy-MM-dd hh:mm:ss.zzz"); \
        ostringstream verbose_macro_tmp; \
        verbose_macro_tmp << dtime << " " << args; \
        MythContext::verbose_mutex.lock(); \
        cout << verbose_macro_tmp.str() << endl; \
        MythContext::verbose_mutex.unlock(); \
    } \
} while (0)

#endif // DEBUG

/// This can be appended to the VERBOSE args with either
/// "+" (with QStrings) or "<<" (with c strings). It uses
/// a thread safe version of strerror to produce the
/// string representation of errno and puts it on the 
/// next line in the verbose output.
#define ENO QString("\n\t\t\teno: ") + safe_eno_to_string(errno)

/// Verbose helper function for ENO macro
QString safe_eno_to_string(int errnum);

/** \class MythEvent
 *  \brief This class is used as a container for messages.
 *
 *   MythEvents can be dispatched to all listeners by calling
 *   gContext->dispatch(MythEvent&) or gContext->dispatchNow(MythEvent&).
 *   The former is much preferred as it uses QApplication::postEvent()
 *   while the latter uses the blocking QApplication::sendEvent().
 */
class MythEvent : public QCustomEvent
{
  public:
    enum Type { MythEventMessage = (User + 1000) };

    MythEvent(const QString lmessage) : QCustomEvent(MythEventMessage)
    {
        message = QDeepCopy<QString>(lmessage);
        extradata = "empty";
    }
    MythEvent(const QString lmessage, const QStringList &lextradata)
           : QCustomEvent(MythEventMessage)
    {
        message = QDeepCopy<QString>(lmessage);
        extradata = lextradata;
    }
    
    ~MythEvent() {}

    const QString& Message() const { return message; }
    const QString& ExtraData(int idx = 0) const { return extradata[idx]; } 
    const QStringList& ExtraDataList() const { return extradata; } 
    int ExtraDataCount() const { return extradata.size(); }

  private:
    QString message;
    QStringList extradata;
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
class MythPrivRequest
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
#define MYTH_BINARY_VERSION "0.19.20051119-1"

/** \brief Increment this whenever the MythTV network protocol changes.
 *
 *   You must also update this value in
 *   mythplugins/mythweb/includes/mythbackend.php
 */
#define MYTH_PROTO_VERSION "22"

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
class MythContext : public QObject
{
    Q_OBJECT
  public:
    MythContext(const QString &binversion);
    virtual ~MythContext();

    bool Init(bool gui = true);

    QString GetMasterHostPrefix(void);

    QString GetHostName(void);

    void ClearSettingsCache(QString myKey = "");
    void ActivateSettingsCache(bool activate = true);

    bool ConnectToMasterServer(bool blockingClient = true);
    QSocketDevice *ConnectServer(QSocket *eventSocket,
                                 const QString &hostname,
                                 int port,
                                 bool blockingClient = false);
    bool IsConnectedToMaster(void);
    void SetBackend(bool backend);
    bool IsBackend(void);

    void BlockShutdown(void);
    void AllowShutdown(void);
    
    QString GetInstallPrefix(void);
    QString GetShareDir(void);
    QString GetLibraryDir(void);
    static QString GetConfDir(void);

    QString GetFilePrefix(void);

    bool LoadSettingsFiles(const QString &filename);
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
    QString GetThemeDir(void);

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

    Settings *settings(void);
    Settings *qtconfig(void);

    void SaveSetting(const QString &key, int newValue);
    void SaveSetting(const QString &key, const QString &newValue);
    QString GetSetting(const QString &key, const QString &defaultval = "");
    void SaveSettingOnHost(const QString &key, const QString &newValue,
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

    void ThemeWidget(QWidget *widget);

    bool FindThemeFile(QString &filename);
    QPixmap *LoadScalePixmap(QString filename, bool fromcache = true); 
    QImage *LoadScaleImage(QString filename, bool fromcache = true);

    void addListener(QObject *obj);
    void removeListener(QObject *obj);
    void dispatch(MythEvent &e);
    void dispatchNow(MythEvent &e);

    bool SendReceiveStringList(QStringList &strlist, bool quickTimeout = false);

    QImage *CacheRemotePixmap(const QString &url, bool reCache = false);

    void SetMainWindow(MythMainWindow *mainwin);
    MythMainWindow *GetMainWindow(void);

    bool TestPopupVersion(const QString &name, const QString &libversion,
                          const QString &pluginversion);

    void SetDisableLibraryPopup(bool check);

    void SetPluginManager(MythPluginManager *pmanager);
    MythPluginManager *getPluginManager(void);

    bool CheckProtoVersion(QSocketDevice* socket);
    bool CheckProtoVersion(QSocket* socket);

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

    void addPrivRequest(MythPrivRequest::Type t, void *data);
    void waitPrivRequest() const;
    MythPrivRequest popPrivRequest();

    static QMutex verbose_mutex;

  private slots:
    void EventSocketRead();
    void EventSocketConnected();
    void EventSocketClosed();

  private:
    void SetPalette(QWidget *widget);
    void InitializeScreenSettings(void);

    void ClearOldImageCache(void);
    void CacheThemeImages(void);
    void CacheThemeImagesDirectory(const QString &dirname, 
                                   const QString &subdirname = "");
    void RemoveCacheDir(const QString &dirname);

    MythContextPrivate *d;
    QString app_binary_version;

    bool useSettingsCache;
    QMutex cacheLock;
    QMap <QString, QString> settingsCache;
};

/// This global variable contains the MythContext instance for the application
extern MythContext *gContext;

#endif
