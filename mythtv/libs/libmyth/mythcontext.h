#ifndef MYTHCONTEXT_H_
#define MYTHCONTEXT_H_

#include <qstring.h>
#include <qpixmap.h>
#include <qpalette.h>
#include <qobject.h>
#include <qptrlist.h>
#include <qevent.h>
#include <qmutex.h>

#include <vector>
using namespace std;

class Settings;
class QSqlDatabase;
class QSqlQuery;
class QSqlError;
class QSocket;
class LCD;
class MythMainWindow;

#define VERBOSE(args...) \
if (print_verbose_messages) \
    cout << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") \
         << " " << args << endl;

class MythEvent : public QCustomEvent
{
  public:
    enum Type { MythEventMessage = (User + 1000) };

    MythEvent(const QString &lmessage) : QCustomEvent(MythEventMessage)
    {
        message = lmessage;
        extradata = "empty";
    }
    MythEvent(const QString &lmessage, const QString &lextradata)
           : QCustomEvent(MythEventMessage)
    {
        message = lmessage;
        extradata = lextradata;
    }
    
    ~MythEvent() {}

    const QString Message() const { return message; }
    const QString ExtraData() const { return extradata; } 

  private:
    QString message;
    QString extradata;
};

#define MYTH_BINARY_VERSION "0.11.08132003-1"
#define MYTH_SCHEMA_VERSION "1003"

extern bool print_verbose_messages;

class MythContext : public QObject
{
    Q_OBJECT
  public:
    MythContext(const QString &binversion, bool gui = true);
    virtual ~MythContext();

    QString GetMasterHostPrefix(void);

    QString GetHostName(void) { return m_localhostname; }

    void ConnectToMasterServer(void);
    bool ConnectServer(const QString &hostname, int port);

    QString GetInstallPrefix() { return m_installprefix; }
    QString GetShareDir() { return m_installprefix + "/share/mythtv/"; }

    QString GetFilePrefix();

    bool LoadSettingsFiles(const QString &filename);
    void LoadQtConfig(void);
    void UpdateImageCache(void);

    void GetScreenSettings(int &width, float &wmult, int &height, float &hmult);
   
    QString FindThemeDir(QString themename);
    QString GetThemeDir(void) { return m_themepathname; }

    int OpenDatabase(QSqlDatabase *db);
    static void KickDatabase(QSqlDatabase *db);
    static void DBError(QString where, const QSqlQuery& query);
    static QString DBErrorMessage(const QSqlError& err);

    bool CheckDBVersion(void);

    Settings *settings() { return m_settings; }
    Settings *qtconfig() { return m_qtThemeSettings; }

    void SaveSetting(QString key, int newValue);
    void SaveSetting(QString key, QString newValue);
    QString GetSetting(const QString &key, const QString &defaultval = "");
    int GetNumSetting(const QString &key, int defaultval = 0);

    QString GetSettingOnHost(const QString &key, const QString &host,
                             const QString &defaultval = "");
    int GetNumSettingOnHost(const QString &key, const QString &host,
                            int defaultval = 0);

    void SetSetting(const QString &key, const QString &newValue);

    int GetBigFontSize() { return GetNumSetting("QtFontBig", 25); }
    int GetMediumFontSize() { return GetNumSetting("QtFontMedium", 16); }
    int GetSmallFontSize() { return GetNumSetting("QtFontSmall", 12); }

    QString GetLanguage(void);

    void ThemeWidget(QWidget *widget);

    QPixmap *LoadScalePixmap(QString filename, bool fromcache = true); 
    QImage *LoadScaleImage(QString filename, bool fromcache = true);

    void addListener(QObject *obj);
    void removeListener(QObject *obj);
    void dispatch(MythEvent &e);
    void dispatchNow(MythEvent &e);

    void SendReceiveStringList(QStringList &strlist);

    QImage *CacheRemotePixmap(const QString &url, bool needevents = true);

    void SetMainWindow(MythMainWindow *mainwin);
    MythMainWindow *GetMainWindow(void);

    void LCDconnectToHost(QString hostname, unsigned int port);
    void LCDswitchToTime();
    void LCDswitchToMusic(QString artist, QString track);
    void LCDsetLevels(int numb_levels, float *levels);
    void LCDswitchToChannel(QString channum = "", QString title = "", 
                            QString subtitle = "");
    void LCDsetChannelProgress(float percentViewed);
    void LCDswitchToNothing();
    void LCDpopMenu(QString menu_choice, QString menu_title);
    void LCDdestroy();

    bool TestPopupVersion(const QString &name, const QString &libversion,
                          const QString &pluginversion);	
  private slots:
    void readSocket();

  private:
    void SetPalette(QWidget *widget);
    void InitializeScreenSettings(void);

    void ClearOldImageCache(void);
    void CacheThemeImages(void);
    void CacheThemeImagesDirectory(const QString &dir);
    void RemoveCacheDir(const QString &dir);

    Settings *m_settings;
    Settings *m_qtThemeSettings;

    QString m_installprefix;

    bool m_themeloaded;
    QString m_themepathname;
    QPixmap m_backgroundimage;
    QPalette m_palette;

    int m_height, m_width;

    QSocket *serverSock;
    QString m_localhostname;

    QMutex serverSockLock;
    bool expectingReply;

    QPtrList<QObject> listeners;

    QSqlDatabase* m_db;
    QMutex dbLock;

    QMap<QString, QImage> imageCache;
    QMap<QString, QPixmap> pixmapCache;

    LCD *lcd_device;

    QString language;

    MythMainWindow *mainWindow;

    float m_wmult, m_hmult;
    int m_screenwidth, m_screenheight;

    QString themecachedir;
};

extern MythContext *gContext;

#if (QT_VERSION < 0x030100)
class QMutexLocker
{
  public:
    QMutexLocker(QMutex *);
   ~QMutexLocker();

    QMutex *mutex() const;

  private:
    QMutex *mtx;
};

inline QMutexLocker::QMutexLocker(QMutex *m)
{
    mtx = m;
    if (mtx)
        mtx->lock();
}

inline QMutexLocker::~QMutexLocker()
{
    if (mtx)
        mtx->unlock();
}

inline QMutex *QMutexLocker::mutex() const
{
    return mtx;
}
#endif

#endif
