#ifndef REMOTEENCODER_H_
#define REMOTEENCODER_H_

#include <qstringlist.h>
#include <pthread.h>
#include <qmap.h>

class ProgramInfo;
class QSocket;

class RemoteEncoder
{
  public:
    RemoteEncoder(int num, const QString &host, short port);
   ~RemoteEncoder(void);

    void Setup(void);
    bool IsValidRecorder(void);
    int GetRecorderNumber(void);

    bool IsRecording(void);
    float GetFrameRate(void);
    long long GetFramesWritten(void);
    long long GetFilePosition(void);
    long long GetFreeSpace(long long totalreadpos);
    long long GetKeyframePosition(long long desired);
    void FillPositionMap(int start, int end,
                         QMap<long long, long long> &positionMap);
    void TriggerRecordingTransition(void);
    void StopPlaying(void);
    void SetupRingBuffer(QString &path, long long &filesize, 
                         long long &fillamount, bool pip = false);
    void SpawnLiveTV(void);
    void StopLiveTV(void);
    void Pause(void);
    void FinishRecording(void);

    void ToggleInputs(void);
    int ChangeContrast(bool direction);
    int ChangeBrightness(bool direction);
    int ChangeColour(bool direction);
    void ChangeChannel(int channeldirection);
    void ChangeDeinterlacer(int deint_mode);
    void ToggleChannelFavorite(void);
    void SetChannel(QString channel);
    bool CheckChannel(QString channel);
    void GetNextProgram(int direction,
                        QString &title, QString &subtitle, QString &desc, 
                        QString &category, QString &starttime, QString &endtime,
                        QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid);
    void GetChannelInfo(QString &title, QString &subtitle, QString &desc, 
                        QString &category, QString &starttime, QString &endtime,
                        QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid);
    void GetInputName(QString &inputname);
  
  private:
    QSocket *openControlSocket(const QString &host, short port);
    void SendReceiveStringList(QStringList &strlist);

    int recordernum;

    QSocket *controlSock;
    pthread_mutex_t lock;

    QString remotehost;
    short remoteport;
};

#endif

