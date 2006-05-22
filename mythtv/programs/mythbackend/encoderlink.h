#ifndef ENCODERLINK_H_
#define ENCODERLINK_H_

#include <qstring.h>

#include "tv.h"
#include "programinfo.h"

class TVRec;
class MainServer;
class PlaybackSock;
class LiveTVChain;

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
    QString GetHostName(void) const { return hostname; }
    /// \brief Returns true for a local EncoderLink.
    bool IsLocal(void) const { return local; }
    /// \brief Returns true if the EncoderLink instance is usable.
    bool IsConnected(void) const { return (IsLocal() || sock!=NULL); }

    /// \brief Returns the cardid used to refer to the recorder in the DB.
    int GetCardID(void) const { return m_capturecardnum; }
    /// \brief Returns the TVRec used by a local EncoderLink instance.
    TVRec *GetTVRec(void) { return tv; }

    int LockTuner(void);
    /// \brief Unlock the tuner.
    /// \sa LockTuner(), IsTunerLocked()
    void FreeTuner(void) { locked = false; }
    /// \brief Returns true iff the tuner is locked.
    /// \sa LockTuner(), FreeTuner()
    bool IsTunerLocked(void) const { return locked; }
    
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
    RecStatusType StartRecording(const ProgramInfo *rec);
    void StopRecording(void);
    void FinishRecording(void);
    void FrontendReady(void);
    void CancelNextRecording(bool);
    bool WouldConflict(const ProgramInfo *rec);

    bool IsReallyRecording(void);
    ProgramInfo *GetRecording(void);
    float GetFramerate(void);
    long long GetFramesWritten(void);
    long long GetFilePosition(void);
    long long GetKeyframePosition(long long desired);
    void SpawnLiveTV(LiveTVChain *chain, bool pip = false);
    QString GetChainID(void);
    void StopLiveTV(void);
    void PauseRecorder(void);
    void SetLiveRecording(int);
    QStringList GetConnectedInputs(void) const;
    QString GetInput(void) const;
    QString SetInput(QString);
    void ToggleChannelFavorite(void);
    void ChangeChannel(int channeldirection);
    void SetChannel(const QString &name);
    int  GetPictureAttribute(PictureAttribute attr);
    int  ChangePictureAttribute(PictureAdjustType type,
                                PictureAttribute  attr,
                                bool              direction);
    bool CheckChannel(const QString &name);
    bool ShouldSwitchToAnotherCard(const QString &channelid);
    bool CheckChannelPrefix(const QString&,uint&,bool&,QString&);
    void GetNextProgram(int direction,
                        QString &title, QString &subtitle, QString &desc,
                        QString &category, QString &starttime,
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid,
                        QString &seriesid, QString &programid);
    bool GetChannelInfo(uint &chanid, uint &sourceid,
                        QString &callsign, QString &channum,
                        QString &channame, QString &xmltv) const;
    bool SetChannelInfo(uint chanid, uint sourceid,
                        QString oldchannum,
                        QString callsign, QString channum,
                        QString channame, QString xmltv);

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
