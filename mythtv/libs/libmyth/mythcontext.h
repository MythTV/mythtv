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

#include <iostream>
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
class LCD;
class MythMainWindow;
class MythPluginManager;
class MediaMonitor;
class MythMediaDevice;
class DisplayRes;

enum VerboseMask {
    VB_IMPORTANT = 0x0001,
    VB_GENERAL   = 0x0002,
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
    VB_NONE      = 0x0000,
    VB_ALL       = 0xffff
};

enum LogPriorities {
    LP_EMERG     = 0,
    LP_ALERT     = 1,
    LP_CRITICAL  = 2,
    LP_ERROR     = 3,
    LP_WARNING   = 4,
    LP_NOTICE    = 5,
    LP_INFO      = 6,
    LP_DEBUG     = 7
};

#define VERBOSE(mask,args...) \
do { \
if ((print_verbose_messages & mask) != 0) \
    cout << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") \
         << " " << args << endl; \
} while (0)

class MythEvent : public QCustomEvent
{
  public:
    enum Type { MythEventMessage = (User + 1000) };

    MythEvent(const QString &lmessage) : QCustomEvent(MythEventMessage)
    {
        message = lmessage;
        extradata = "empty";
    }
    MythEvent(const QString &lmessage, const QStringList &lextradata)
           : QCustomEvent(MythEventMessage)
    {
        message = lmessage;
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

#define MYTH_BINARY_VERSION "0.16.20040906-1"
#define MYTH_PROTO_VERSION "13"

extern int print_verbose_messages;

class MythContextPrivate;

class MythContext : public QObject
{
    Q_OBJECT
  public:
    MythContext(const QString &binversion, bool gui = true, bool lcd = true);
    virtual ~MythContext();

    QString GetMasterHostPrefix(void);

    QString GetHostName(void);

    bool ConnectToMasterServer(void);
    bool ConnectServer(const QString &hostname, int port);
    bool IsConnectedToMaster(void);

    QString GetInstallPrefix(void);
    QString GetShareDir(void);

    QString GetFilePrefix(void);

    bool LoadSettingsFiles(const QString &filename);
    void LoadQtConfig(void);
    void UpdateImageCache(void);

    void RefreshBackendConfig(void);

    void GetScreenSettings(float &wmult, float &hmult);
    void GetScreenSettings(int &width, float &wmult,
                           int &height, float &hmult);
    void GetScreenSettings(int &xbase, int &width, float &wmult,
                           int &ybase, int &height, float &hmult);
   
    QString FindThemeDir(const QString &themename);
    QString GetThemeDir(void);

    QString GetMenuThemeDir(void);

    int OpenDatabase(QSqlDatabase *db);
    static void KickDatabase(QSqlDatabase *db);
    static void DBError(const QString &where, const QSqlQuery& query);
    static QString DBErrorMessage(const QSqlError& err);

    void LogEntry(const QString &module, int priority,
                  const QString &message, const QString &details);

    Settings *settings(void);
    Settings *qtconfig(void);

    void SaveSetting(const QString &key, int newValue);
    void SaveSetting(const QString &key, const QString &newValue);
    QString GetSetting(const QString &key, const QString &defaultval = "");
    int GetNumSetting(const QString &key, int defaultval = 0);

    QString GetSettingOnHost(const QString &key, const QString &host,
                             const QString &defaultval = "");
    int GetNumSettingOnHost(const QString &key, const QString &host,
                            int defaultval = 0);

    void SetSetting(const QString &key, const QString &newValue);

    QFont GetBigFont();
    QFont GetMediumFont();
    QFont GetSmallFont();

    QString GetLanguage(void);

    void ThemeWidget(QWidget *widget);

    QPixmap *LoadScalePixmap(QString filename, bool fromcache = true); 
    QImage *LoadScaleImage(QString filename, bool fromcache = true);

    void addListener(QObject *obj);
    void removeListener(QObject *obj);
    void dispatch(MythEvent &e);
    void dispatchNow(MythEvent &e);

    bool SendReceiveStringList(QStringList &strlist, bool quickTimeout = false);

    QImage *CacheRemotePixmap(const QString &url);

    void SetMainWindow(MythMainWindow *mainwin);
    MythMainWindow *GetMainWindow(void);

    LCD *GetLCDDevice(void);

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
    bool hasPrivRequest() const;
    MythPrivRequest popPrivRequest();

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
};

extern MythContext *gContext;

#endif
