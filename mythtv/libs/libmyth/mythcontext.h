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
class ProgramInfo;
class RemoteEncoder;

class MythEvent : public QCustomEvent
{
  public:
    enum Type { MythEventMessage = (User + 1000) };

    MythEvent(const QString &lmessage) : QCustomEvent(MythEventMessage)
    {
        message = lmessage;
    }
    ~MythEvent() {}

    const QString Message() const { return message; }
 
  private:
    QString message;
};

class MythContext : public QObject
{
    Q_OBJECT
  public:
    MythContext(bool gui = true);
    virtual ~MythContext();

    bool ConnectServer(const QString &hostname, int port);

    QString GetInstallPrefix() { return m_installprefix; }
    QString GetFilePrefix();

    void LoadSettingsFiles(const QString &filename);
    void LoadSettingsDatabase(QSqlDatabase *db);
    void LoadQtConfig();

    void GetScreenSettings(int &width, float &wmult, int &height, float &hmult);
   
    QString RunProgramGuide(QString startchannel, bool thread = false,
                            void (*embedcb)(void *data, unsigned long wid,
                                            int x, int y, int w, int h) = NULL,
                            void *data = NULL);
 
    QString FindThemeDir(QString themename);

    int OpenDatabase(QSqlDatabase *db);
    static void KickDatabase(QSqlDatabase *db);

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

    vector<ProgramInfo *> *GetRecordedList(bool deltype);
    void GetFreeSpace(int &totalspace, int &usedspace);
    void DeleteRecording(ProgramInfo *pginfo);
    bool GetAllPendingRecordings(vector<ProgramInfo *> &recordinglist);
    vector<ProgramInfo *> *GetConflictList(ProgramInfo *pginfo,
                                           bool removenonplaying);

    RemoteEncoder *RequestRecorder(void);
    RemoteEncoder *GetExistingRecorder(ProgramInfo *pginfo);

    void addListener(QObject *obj);
    void removeListener(QObject *obj);
    void dispatch(MythEvent &e);

  private slots:
    void readSocket();

  private:
    void SendReceiveStringList(QStringList &strlist);

    void SetPalette(QWidget *widget);

    Settings *m_settings;
    Settings *m_qtThemeSettings;

    QString m_installprefix;

    bool m_themeloaded;
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
};

#endif
