#ifndef MYTHCONTEXT_H_
#define MYTHCONTEXT_H_

#include <qstring.h>
#include <qpixmap.h>
#include <qpalette.h>
#include <qsocket.h>
#include <qobject.h>
#include <qptrlist.h>
#include <qevent.h>

#include <list>
#include <vector>
using namespace std;

class Settings;
class QSqlDatabase;
class ProgramInfo;


class MythEvent : public QCustomEvent
{
  public:
    enum Type { MythEventType = (User + 1000) };

    MythEvent(const QString &lmessage) : QCustomEvent(MythEventType)
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

    int GetBigFontSize() { return qtfontbig; }
    int GetMediumFontSize() { return qtfontmed; }
    int GetSmallFontSize() { return qtfontsmall; }

    void ThemeWidget(QWidget *widget);

    QPixmap *LoadScalePixmap(QString filename); 

    vector<ProgramInfo *> *GetRecordedList(bool deltype);
    void GetFreeSpace(int &totalspace, int &usedspace);
    void DeleteRecording(ProgramInfo *pginfo);
    bool GetAllPendingRecordings(list<ProgramInfo *> &recordinglist);
    list<ProgramInfo *> *GetConflictList(ProgramInfo *pginfo,
                                         bool removenonplaying);

    int RequestRecorder(void);
    bool RecorderIsRecording(int recorder);
    float GetRecorderFrameRate(int recorder);
    long long GetRecorderFramesWritten(int recorder);
    long long GetRecorderFilePosition(int recorder);
    long long GetRecorderFreeSpace(int recorder, long long totalreadpos);
    long long GetKeyframePosition(int recorder, long long desired);
    void SetupRecorderRingBuffer(int recorder, QString &path, 
                                 long long &filesize, long long &fillamount,
                                 bool pip = false);
    void SpawnLiveTVRecording(int recorder);
    void StopLiveTVRecording(int recorder);
    void PauseRecorder(int recorder);
    void ToggleRecorderInputs(int recorder);
    void RecorderChangeContrast(int recorder, bool direction);
    void RecorderChangeBrightness(int recorder, bool direction);
    void RecorderChangeColour(int recorder, bool direction);
    void RecorderChangeChannel(int recorder, bool direction);
    void RecorderSetChannel(int recorder, QString channel);
    bool CheckChannel(int recorder, QString channel);
    void GetRecorderChannelInfo(int recorder, QString &title, QString &subtitle,
                                QString &desc, QString &category,
                                QString &starttime, QString &endtime, 
                                QString &callsign, QString &iconpath,
                                QString &channelname);
    void GetRecorderInputName(int recorder, QString &inputname);

    void addListener(QObject *obj);
    void removeListener(QObject *obj);
    void dispatch(MythEvent &e);

  private slots:
    void readSocket();

  private:
    void SendReceiveStringList(QStringList &strlist);

    void SetPalette(QWidget *widget);

    // font sizes
    int qtfontbig, qtfontmed, qtfontsmall;

    Settings *m_settings;
    Settings *m_qtThemeSettings;

    QString m_installprefix;

    bool m_themeloaded;
    QPixmap m_backgroundimage;
    QPalette m_palette;

    int m_height, m_width;
    float m_wmult, m_hmult;

    QSocket *serverSock;
    QString m_localhostname;

    pthread_mutex_t serverSockLock;
    bool expectingReply;

    QPtrList<QObject> listeners;
};

#endif
