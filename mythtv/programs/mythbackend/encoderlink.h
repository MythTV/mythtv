#ifndef ENCODERLINK_H_
#define ENCODERLINK_H_

#include <qstring.h>

#include "tv.h"

class MainServer;
class PlaybackSock;

class EncoderLink
{
  public:
    EncoderLink(int capturecardnum, PlaybackSock *lsock, QString lhostname);
    EncoderLink(int capturecardnum, TVRec *ltv);

   ~EncoderLink();

    void setSocket(PlaybackSock *lsock) { sock = lsock; }

    PlaybackSock *getSocket() { return sock; }
    QString getHostname() { return hostname; }

    TVRec *getTV() { return tv; }

    bool isLocal() { return local; }

    bool isConnected();
    int getCardId() { return m_capturecardnum; }

    bool isBusy();
    TVState GetState();
    bool MatchesRecording(ProgramInfo *rec);
    int AllowRecording(ProgramInfo *rec, int timeuntil);
    void StartRecording(ProgramInfo *rec);
    void StopRecording(void);

    bool IsReallyRecording(void);
    float GetFramerate(void);
    long long GetFramesWritten(void);
    long long GetFilePosition(void);
    long long GetFreeSpace(long long totalreadpos);
    long long GetKeyframePosition(long long desired);
    void TriggerRecordingTransition(void);
    void StopPlaying(void);
    void SetupRingBuffer(QString &path, long long &filesize,
                         long long &fillamount, bool pip = false);
    void SpawnLiveTV(void);
    void StopLiveTV(void);
    void PauseRecorder(void);
    void ToggleInputs(void);
    void ChangeChannel(bool direction);
    void SetChannel(QString name);
    void ChangeContrast(bool direction);
    void ChangeBrightness(bool direction);
    void ChangeColour(bool direction);
    bool CheckChannel(QString name);
    void GetChannelInfo(QString &title, QString &subtitle, QString &desc,
                        QString &category, QString &starttime,
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname);
    void GetInputName(QString &inputname);

    void SpawnReadThread(QSocket *sock);
    void KillReadThread(void);
    QSocket *GetReadThreadSocket(void);

    void PauseRingBuffer(void);
    void UnpauseRingBuffer(void);
    void PauseClearRingBuffer(void);
    void RequestRingBufferBlock(int size);
    long long SeekRingBuffer(long long curpos, long long pos, int whence);

    char *GetScreenGrab(QString filename, int secondsin, int &bufferlen,
                        int &video_width, int &video_height);
  private:
    int m_capturecardnum;

    PlaybackSock *sock;
    QString hostname;

    TVRec *tv;

    bool local;
};

#endif
