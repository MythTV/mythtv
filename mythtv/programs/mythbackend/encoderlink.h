#ifndef ENCODERLINK_H_
#define ENCODERLINK_H_

// C++ headers
#include <utility>
#include <vector>                       // for vector

// QT headers
#include <QDateTime>                    // for QDateTime
#include <QMutex>                       // for QMutex
#include <QString>                      // for QString

#include "libmythbase/recordingstatus.h"
#include "libmythtv/tv.h"               // for SleepStatus, etc
#include "libmythtv/videoouttypes.h"    // for PictureAttribute

class TVRec;
class PlaybackSock;
class LiveTVChain;
class InputInfo;
class ProgramInfo;

class EncoderLink
{
  public:
    EncoderLink(int inputid, PlaybackSock *lsock, QString lhostname);
    EncoderLink(int inputid, TVRec *ltv);

   ~EncoderLink();

    /// \brief Used to set the socket for a non-local EncoderLink.
    void SetSocket(PlaybackSock *lsock);
    /// \brief Returns the socket, if set, for a non-local EncoderLink.
    PlaybackSock *GetSocket(void) { return m_sock; }

    /// \brief Used to set the asleep status of an encoder
    void SetSleepStatus(SleepStatus newStatus);
    /// \brief Get the last time the sleep status was changed
    QDateTime GetSleepStatusTime(void) const { return m_sleepStatusTime; }
    /// \brief Get the last time the encoder was put to sleep
    QDateTime GetLastSleepTime(void) const { return m_lastSleepTime; }
    /// \brief Used to set the last wake time of an encoder
    void SetLastWakeTime(QDateTime newTime) { m_lastWakeTime = std::move(newTime); }
    /// \brief Get the last time the encoder was awakened
    QDateTime GetLastWakeTime(void) const { return m_lastWakeTime; }

    /// \brief Returns the remote host for a non-local EncoderLink.
    QString GetHostName(void) const { return m_hostname; }
    /// \brief Returns true for a local EncoderLink.
    bool IsLocal(void) const { return m_local; }
    /// \brief Returns true if the EncoderLink instance is usable.
    bool IsConnected(void) const { return (IsLocal() || m_sock!=nullptr); }
    /// \brief Returns true if the encoder is awake.
    bool IsAwake(void) const { return (m_sleepStatus == sStatus_Awake); }
    /// \brief Returns true if the encoder is asleep.
    bool IsAsleep(void) const { return (m_sleepStatus & sStatus_Asleep) != 0; }
    /// \brief Returns true if the encoder is waking up.
    bool IsWaking(void) const { return (m_sleepStatus == sStatus_Waking); }
    /// \brief Returns true if the encoder is falling asleep.
    bool IsFallingAsleep(void) const
                        { return (m_sleepStatus == sStatus_FallingAsleep); }
    /// \brief Returns true if the encoder can sleep.
    bool CanSleep(void) const { return (m_sleepStatus != sStatus_Undefined); }

    /// \brief Returns the current Sleep Status of the encoder.
    SleepStatus GetSleepStatus(void) const { return (m_sleepStatus); }

    /// \brief Returns the inputid used to refer to the recorder in the DB.
    int GetInputID(void) const { return m_inputid; }
    /// \brief Returns the TVRec used by a local EncoderLink instance.
    TVRec *GetTVRec(void) { return m_tv; }

    /// \brief Tell a slave backend to go to sleep
    bool GoToSleep(void);
    int LockTuner(void);
    /// \brief Unlock the tuner.
    /// \sa LockTuner(), IsTunerLocked()
    void FreeTuner(void) { m_locked = false; }
    /// \brief Returns true iff the tuner is locked.
    /// \sa LockTuner(), FreeTuner()
    bool IsTunerLocked(void) const { return m_locked; }

    bool CheckFile(ProgramInfo *pginfo);
    long long GetMaxBitrate(void);
    std::chrono::milliseconds SetSignalMonitoringRate(std::chrono::milliseconds rate, int notifyFrontend);

    bool IsBusy(InputInfo *busy_input = nullptr, std::chrono::seconds time_buffer = 5s);
    bool IsBusyRecording(void);

    TVState GetState();
    uint GetFlags(void);
    bool IsRecording(const ProgramInfo *rec); // scheduler call only.

    bool MatchesRecording(const ProgramInfo *rec);
    void RecordPending(const ProgramInfo *rec, std::chrono::seconds secsleft, bool hasLater);
    RecStatus::Type StartRecording(ProgramInfo *rec);
    RecStatus::Type GetRecordingStatus(void);
    void StopRecording(bool killFile = false);
    void FinishRecording(void);
    void FrontendReady(void);
    void CancelNextRecording(bool cancel);
    bool WouldConflict(const ProgramInfo *rec);

    bool IsReallyRecording(void);
    ProgramInfo *GetRecording(void);
    float GetFramerate(void);
    long long GetFramesWritten(void);
    long long GetFilePosition(void);
    int64_t GetKeyframePosition(uint64_t desired);
    bool GetKeyframePositions(int64_t start, int64_t end, frm_pos_map_t &map);
    bool GetKeyframeDurations(int64_t start, int64_t end, frm_pos_map_t &map);
    void SpawnLiveTV(LiveTVChain *chain, bool pip, QString startchan);
    QString GetChainID(void);
    void StopLiveTV(void);
    void PauseRecorder(void);
    void SetLiveRecording(int recording);
    void SetNextLiveTVDir(const QString& dir);
    QString GetInput(void) const;
    QString SetInput(QString input);
    void ToggleChannelFavorite(const QString &changroup);
    void ChangeChannel(ChannelChangeDirection channeldirection);
    void SetChannel(const QString &name);
    int  GetPictureAttribute(PictureAttribute attr);
    int  ChangePictureAttribute(PictureAdjustType type,
                                PictureAttribute  attr,
                                bool              direction);
    bool CheckChannel(const QString &name);
    bool ShouldSwitchToAnotherInput(const QString &channelid);
    bool CheckChannelPrefix(const QString &prefix, uint &complete_valid_channel_on_rec,
                            bool &is_extra_char_useful, QString &needed_spacer);
    void GetNextProgram(BrowseDirection direction,
                        QString &title, QString &subtitle, QString &desc,
                        QString &category, QString &starttime,
                        QString &endtime, QString &callsign, QString &iconpath,
                        QString &channelname, uint &chanid,
                        QString &seriesid, QString &programid);
    bool GetChannelInfo(uint &chanid, uint &sourceid,
                        QString &callsign, QString &channum,
                        QString &channame, QString &xmltv) const;
    bool SetChannelInfo(uint chanid, uint sourceid,
                        const QString& oldchannum,
                        const QString& callsign, const QString& channum,
                        const QString& channame, const QString& xmltv);

    bool AddChildInput(uint childid);

  private:
    bool HasSockAndIncrRef();
    bool HasSockAndDecrRef();

    int m_inputid;

    PlaybackSock *m_sock           {nullptr};
    QMutex m_sockLock;
    QString m_hostname;

    TVRec *m_tv                    {nullptr};

    bool m_local                   {false};
    bool m_locked                  {false};

    SleepStatus m_sleepStatus      {sStatus_Undefined};
    QDateTime m_sleepStatusTime;
    QDateTime m_lastSleepTime;
    QDateTime m_lastWakeTime;

    QDateTime m_endRecordingTime;
    QDateTime m_startRecordingTime;
    uint      m_chanid             {0};
};

#endif
