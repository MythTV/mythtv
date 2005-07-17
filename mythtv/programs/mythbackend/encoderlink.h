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

    /// \brief Used to set the socket for a non-local EncoderLink.
    void SetSocket(PlaybackSock *lsock);
    /// \brief Returns the socket, if set, for a non-local EncoderLink.
    PlaybackSock *GetSocket(void) { return sock; }

    /// \brief Returns the remote host for a non-local EncoderLink.
    QString GetHostName(void) { return hostname; }
    /// \brief Returns true for a local EncoderLink.
    bool IsLocal(void) { return local; }
    /// \brief Returns true if the EncoderLink instance is usable.
    bool IsConnected(void) { return (IsLocal() || GetSocket()!=NULL); }

    /// \brief Returns the cardid used to refer to the recorder in the DB.
    int GetCardID(void) { return m_capturecardnum; }
    /// \brief Returns the TVRec used by a local EncoderLink instance.
    TVRec *GetTVRec(void) { return tv; }

    int LockTuner(void);
    /// \brief Unlock the tuner.
    /// \sa LockTuner(), IsTunerLocked()
    void FreeTuner(void) { locked = false; }
    /// \brief Returns true iff the tuner is locked.
    /// \sa LockTuner(), FreeTuner()
    bool IsTunerLocked(void) { return locked; }
    
    long long GetFreeDiskSpace(long long &total, long long &used,
                               bool from_cache=false);
    long long GetFreeDiskSpace(bool from_cache=false);
    long long GetMaxBitrate(void);
    int SetSignalMonitoringRate(int rate, int notifyFrontend);

    bool IsBusy(void);
    bool IsBusyRecording(void);

    TVState GetState();
    bool IsRecording(const ProgramInfo *rec); // scheduler call only.

    bool HasEnoughFreeSpace(const ProgramInfo *rec,
                            bool try_to_use_cache = false);
    bool MatchesRecording(const ProgramInfo *rec);
    void RecordPending(const ProgramInfo *rec, int secsleft);
    int StartRecording(const ProgramInfo *rec);
    void StopRecording(void);
    void FinishRecording(void);
    void FrontendReady(void);
    void CancelNextRecording(void);
    bool WouldConflict(const ProgramInfo *rec);

    bool IsReallyRecording(void);
    ProgramInfo *GetRecording(void);
    float GetFramerate(void);
    long long GetFramesWritten(void);
    long long GetFilePosition(void);
    long long GetFreeSpace(long long totalreadpos);
    long long GetKeyframePosition(long long desired);
    void StopPlaying(void);
    bool SetupRingBuffer(QString &path, long long &filesize,
                         long long &fillamount, bool pip = false);
    void SpawnLiveTV(void);
    void StopLiveTV(void);
    void PauseRecorder(void);
    void ToggleInputs(void);
    void ToggleChannelFavorite(void);
    void ChangeChannel(int channeldirection);
    void SetChannel(const QString &name);
    int ChangeContrast(bool direction);
    int ChangeBrightness(bool direction);
    int ChangeColour(bool direction);
    int ChangeHue(bool direction);
    bool CheckChannel(const QString &name);
    bool ShouldSwitchToAnotherCard(const QString &channelid);
    bool CheckChannelPrefix(const QString &name, bool &unique);
    void GetNextProgram(int direction,
                        QString &title, QString &subtitle, QString &desc,
                        QString &category, QString &starttime,
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid,
                        QString &seriesid, QString &programid);
    void GetChannelInfo(QString &title, QString &subtitle, QString &desc,
                        QString &category, QString &starttime,
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid,
                        QString &seriesid, QString &programid, QString &chanFilters, 
                        QString &repeat, QString &airdate, QString &stars);
    QString GetInputName();

    void SetReadThreadSock(QSocket *rsock);
    QSocket *GetReadThreadSocket(void);

    int RequestRingBufferBlock(int size);
    long long SeekRingBuffer(long long curpos, long long pos, int whence);

    char *GetScreenGrab(const ProgramInfo *pginfo, const QString &filename,
                        int secondsin, int &bufferlen,
                        int &video_width, int &video_height,
                        float &video_aspect);

  private:
    int m_capturecardnum;

    PlaybackSock *sock;
    QString hostname;

    long long freeDiskSpaceKB;

    TVRec *tv;

    bool local;
    bool locked;

    QDateTime endRecordingTime;
    QDateTime startRecordingTime;
    QString chanid;
    QString recordfileprefix;
};

#endif
