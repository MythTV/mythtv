#ifndef MYTHCONTEXT_H_
#define MYTHCONTEXT_H_

#include <qstring.h>
#include <qpixmap.h>
#include <qpalette.h>
#include <qsocket.h>

#include <vector>
using namespace std;

class Settings;
class QSqlDatabase;
class ProgramInfo;

class MythContext
{
  public:
    MythContext(bool gui = true);
   ~MythContext();

    bool ConnectServer(const QString &hostname, int port);

    QString GetInstallPrefix() { return m_installprefix; }
    QString GetFilePrefix();

    void LoadSettingsFiles(const QString &filename);
    void LoadSettingsDatabase(QSqlDatabase *db);
    void LoadQtConfig();

    void GetScreenSettings(int &width, float &wmult, int &height, float &hmult);
   
    QString RunProgramGuide(QString startchannel, bool thread = false);
 
    QString FindThemeDir(QString themename);

    int OpenDatabase(QSqlDatabase *db);

    Settings *settings() { return m_settings; }

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
    void RecorderChangeChannel(int recorder, bool direction);
    void RecorderSetChannel(int recorder, QString channel);
    bool CheckChannel(int recorder, QString channel);
    void GetRecorderChannelInfo(int recorder, QString &title, QString &subtitle,
                                QString &desc, QString &category,
                                QString &starttime, QString &endtime, 
                                QString &callsign, QString &iconpath,
                                QString &channelname);
    void GetRecorderInputName(int recorder, QString &inputname);

    QSocket *SetupRemoteRingBuffer(int recorder, QString url);
    void CloseRemoteRingBuffer(int recorder, QSocket *sock);
    long long SeekRemoteRing(int recorder, QSocket *sock, long long curpos,
                             long long pos, int whence);
    
    QSocket *SetupRemoteFile(QString url);
    void CloseRemoteFile(QSocket *sock);
    int ReadRemoteFile(QSocket *sock, void *data, int size);
    long long SeekRemoteFile(QSocket *Sock, long long curpos, long long pos, 
                             int whence);

  private:
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
};

#endif
