#ifndef MYTHCONTEXT_H_
#define MYTHCONTEXT_H_

#include <qstring.h>
#include <qpixmap.h>
#include <qpalette.h>
#include <qsocket.h>
#include <qobject.h>
#include <qptrlist.h>
#include <qevent.h>

#include <vector>
using namespace std;

class Settings;
class QSqlDatabase;
class QSqlQuery;
class LCD;				// thor feb 7 2003

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

class MythContext : public QObject
{
    Q_OBJECT
  public:
    MythContext(bool gui = true);
    virtual ~MythContext();

    QString GetMasterHostPrefix(void);
    bool ConnectServer(const QString &hostname, int port);

    QString GetInstallPrefix() { return m_installprefix; }
    QString GetFilePrefix();

    void LoadSettingsFiles(const QString &filename);
    void LoadSettingsDatabase(QSqlDatabase *db);
    void LoadQtConfig();

    void GetScreenSettings(int &width, float &wmult, int &height, float &hmult);
   
    QString FindThemeDir(QString themename);

    int OpenDatabase(QSqlDatabase *db);
    static void KickDatabase(QSqlDatabase *db);
    static void DBError(QString where, const QSqlQuery& query);

    Settings *settings() { return m_settings; }
    Settings *qtconfig() { return m_qtThemeSettings; }

    QString GetSetting(const QString &key, const QString &defaultval = "");
    int GetNumSetting(const QString &key, int defaultval = 0);

    void SetSetting(const QString &key, const QString &newValue);

    int GetBigFontSize() { return GetNumSetting("QtFontBig", 25); }
    int GetMediumFontSize() { return GetNumSetting("QtFontMedium", 16); }
    int GetSmallFontSize() { return GetNumSetting("QtFontSmall", 12); }

    void ThemeWidget(QWidget *widget);

    QPixmap *LoadScalePixmap(QString filename); 

    void addListener(QObject *obj);
    void removeListener(QObject *obj);
    void dispatch(MythEvent &e);

    void SendReceiveStringList(QStringList &strlist);

    QImage *CacheRemotePixmap(const QString &url, bool needevents = true);

	//
	//	thor feb 7 2003
	//
	//	LCD access methods
	//
	//  Perhaps I should have just made *lcd_device public
	//	and then accessed things directly as in:
	//
	//			gContext->lcd_device->connectToHost()
	//
	//	maybe ... but we might eventually want to configure things
	//	per lcd device from settings.txt within gContext, so I think 
	//	this is better.
	//
	
	void LCDconnectToHost(QString hostname, unsigned int port);
	void LCDswitchToTime();
	void LCDswitchToMusic(QString artist, QString track);
	void LCDsetLevels(int numb_levels, float *levels);
	void LCDswitchToChannel(QString channum, QString title, QString subtitle);
	void LCDsetChannelProgress(float percentViewed);
	void LCDswitchToNothing();
	
  private slots:
    void readSocket();

  private:
    void SetPalette(QWidget *widget);

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

    pthread_mutex_t serverSockLock;
    bool expectingReply;

    QPtrList<QObject> listeners;

    QSqlDatabase* m_db;
    pthread_mutex_t dbLock;

    QMap<QString, QImage> imageCache;

	//
	//	thor feb 7 2003
	//
	//	One LCD device per MythContext?
	//	(this may change)
	//	    

    LCD *lcd_device;

};

extern MythContext *gContext;

#endif
