#ifndef REMOTEENCODER_H_
#define REMOTEENCODER_H_

#include <qstringlist.h>
#include <pthread.h>
#include <qmap.h>

class ProgramInfo;
class QSocketDevice;

class RemoteEncoder
{
  public:
    RemoteEncoder(int num, const QString &host, short port);
   ~RemoteEncoder(void);

    void Setup(void);
    bool IsValidRecorder(void);
    int GetRecorderNumber(void);

    ProgramInfo *GetRecording(void);
    bool IsRecording(void);
    float GetFrameRate(void);
    long long GetFramesWritten(void);
    /// \brief Return value last returned by GetFramesWritten().
    long long GetCachedFramesWritten(void) const { return cachedFramesWritten; }
    long long GetFilePosition(void);
    long long GetFreeDiskSpace();
    long long GetMaxBitrate();
    long long GetKeyframePosition(long long desired);
    void FillPositionMap(int start, int end,
                         QMap<long long, long long> &positionMap);
    void StopPlaying(void);
    void SpawnLiveTV(QString chainid, bool pip);
    void StopLiveTV(void);
    void PauseRecorder(void);
    void FinishRecording(void);
    void FrontendReady(void);
    void CancelNextRecording(bool cancel);

    void ToggleInputs(void);
    int ChangeContrast(bool direction);
    int ChangeBrightness(bool direction);
    int ChangeColour(bool direction);
    int ChangeHue(bool direction);
    void ChangeChannel(int channeldirection);
    void ChangeDeinterlacer(int deint_mode);
    void ToggleChannelFavorite(void);
    void SetChannel(QString channel);
    int SetSignalMonitoringRate(int msec, bool notifyFrontend = true);
    bool CheckChannel(QString channel);
    bool ShouldSwitchToAnotherCard(QString channelid);
    bool CheckChannelPrefix(QString channel, bool &unique);
    void GetNextProgram(int direction,
                        QString &title, QString &subtitle, QString &desc, 
                        QString &category, QString &starttime, QString &endtime,
                        QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid,
                        QString &seriesid, QString &programid);
                        
    bool GetErrorStatus(void) { bool v = backendError; backendError = false; 
                                return v; }
 
  private:
    QSocketDevice *openControlSocket(const QString &host, short port);
    void SendReceiveStringList(QStringList &strlist);

    int recordernum;

    QSocketDevice *controlSock;
    pthread_mutex_t lock;

    QString remotehost;
    short remoteport;

    QString lastchannel;

    bool backendError;
    long long cachedFramesWritten;
};

#endif

