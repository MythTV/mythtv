#ifndef ENCODERLINK_H_
#define ENCODERLINK_H_

#include <qsocket.h>
#include <qstring.h>

#include "tv.h"

class EncoderLink
{
  public:
    EncoderLink(QSocket *lsock, QString lhostname);
    EncoderLink(TVRec *ltv);

   ~EncoderLink();

    QSocket *getSocket() { return sock; }
    QString getHostname() { return hostname; }

    TVRec *getTV() { return tv; }

    bool isLocal() { return local; }

    bool isBusy();
    TVState GetState();
    int AllowRecording(ProgramInfo *rec, int timeuntil);
    void StartRecording(ProgramInfo *rec);

    bool IsReallyRecording(void);
    float GetFramerate(void);
    long long GetFramesWritten(void);
    long long GetFilePosition(void);
    long long GetFreeSpace(long long totalreadpos);
    long long GetKeyframePosition(long long desired);
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

    void PauseRingBuffer(void);
    void UnpauseRingBuffer(void);
    void PauseClearRingBuffer(void);
    void RequestRingBufferBlock(int size);
    long long SeekRingBuffer(long long curpos, long long pos, int whence);

  private:
    QSocket *sock;
    QString hostname;

    TVRec *tv;

    bool local;
};

#endif
