#ifndef PLAYER_CONTEXT_H
#define PLAYER_CONTEXT_H

#include <vector>
#include <deque>

// Qt headers
#include <QWidget>
#include <QString>
#include <QRecursiveMutex>
#include <QHash>
#include <QRect>
#include <QObject>
#include <QDateTime>

// MythTV headers
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdeque.h"
#include "libmythbase/mythtimer.h"
#include "libmythbase/mythtypes.h"
#include "libmythtv/mythtvexp.h"
#include "libmythtv/tv.h"
#include "libmythtv/videoouttypes.h"

class TV;
class RemoteEncoder;
class MythPlayer;
class MythMediaBuffer;
class ProgramInfo;
class LiveTVChain;
class QPainter;
class MythMainWindow;

struct osdInfo
{
    InfoMap             text;
    QHash<QString,int>  values;
};

enum PseudoState : std::uint8_t
{
    kPseudoNormalLiveTV  = 0,
    kPseudoChangeChannel = 1,
    kPseudoRecording     = 2,
};

using StringDeque = std::deque<QString>;

class MTV_PUBLIC PlayerContext
{
  public:
    explicit PlayerContext(QString inUseID = QString("Unknown"));
    ~PlayerContext();

    // Actions
    void TeardownPlayer(void);
    void StopPlaying(void) const;
    void UpdateTVChain(const QStringList &data = QStringList());
    bool ReloadTVChain(void);
    void    PushPreviousChannel(void);
    QString PopPreviousChannel(void);

    void ChangeState(TVState newState);
    void ForceNextStateNone(void);
    TVState DequeueNextState(void);

    bool HandlePlayerSpeedChangeFFRew(void);
    bool HandlePlayerSpeedChangeEOF(void);

    // Locking
    void LockState(void) const;
    void UnlockState(void) const;

    void LockPlayingInfo(const char *file, int line) const;
    void UnlockPlayingInfo(const char *file, int line) const;

    void LockDeletePlayer(const char *file, int line) const;
    void UnlockDeletePlayer(const char *file, int line) const;

    // Sets
    void SetInitialTVState(bool islivetv);
    void SetPlayer(MythPlayer *newplayer);
    void SetRecorder(RemoteEncoder *rec);
    void SetTVChain(LiveTVChain *chain);
    void SetRingBuffer(MythMediaBuffer *Buffer);
    void SetPlayingInfo(const ProgramInfo *info);
    void SetPlayGroup(const QString &group);
    void SetPseudoLiveTV(const ProgramInfo *pi, PseudoState new_state);
    void SetPlayerChangingBuffers(bool val) { m_playerUnsafe = val; }

    // Gets
    QString  GetPreviousChannel(void) const;
    uint     GetCardID(void) const { return m_lastCardid; }
    QString  GetFilters(const QString &baseFilters) const;
    QString  GetPlayMessage(void) const;
    TVState  GetState(void) const;
    bool     GetPlayingInfoMap(InfoMap &infoMap) const;

    // Boolean Gets
    bool IsPlayerChangingBuffers(void) const { return m_playerUnsafe; }
    bool HasPlayer(void) const;
    bool IsPlayerErrored(void) const;
    bool IsPlayerPlaying(void) const;
    bool IsRecorderErrored(void) const;
    bool InStateChange(void) const;
    /// This is set if the player encountered some irrecoverable error.
    bool IsErrored(void) const { return m_errored; }
    bool IsSameProgram(const ProgramInfo &p) const;

  public:
    QString             m_recUsage;
    MythPlayer         *m_player             {nullptr};
    volatile bool       m_playerUnsafe       {false};
    RemoteEncoder      *m_recorder           {nullptr};
    LiveTVChain        *m_tvchain            {nullptr};
    MythMediaBuffer    *m_buffer             {nullptr};
    ProgramInfo        *m_playingInfo        {nullptr}; ///< Currently playing info
    std::chrono::seconds m_playingLen        {0s};  ///< Initial CalculateLength()
    QDateTime           m_playingRecStart;
    int                 m_lastCardid         {-1}; ///< CardID of current/last recorder
    /// 0 == normal, +1 == fast forward, -1 == rewind
    int                 m_ffRewState         {0};
    /// Index into m_ffRewSpeeds for FF and Rewind speeds
    int                 m_ffRewIndex         {0};
    /// Caches value of m_ffRewSpeeds[m_ffRewIndex]
    int                 m_ffRewSpeed         {0};
    TVState             m_playingState       {kState_None};

    bool                m_errored            {false};

    // Previous channel functionality state variables
    StringDeque         m_prevChan; ///< Previous channels

    // Recording to play next, after LiveTV
    ProgramInfo        *m_pseudoLiveTVRec    {nullptr};
    PseudoState         m_pseudoLiveTVState  {kPseudoNormalLiveTV};

    std::chrono::seconds  m_fftime           {0s};
    std::chrono::seconds  m_rewtime          {0s};
    std::chrono::minutes  m_jumptime         {0min};
    /** \brief Time stretch speed, 1.0F for normal playback.
     *
     *  Begins at 1.0F meaning normal playback, but can be increased
     *  or decreased to speedup or slowdown playback.
     *  Ignored when doing Fast Forward or Rewind.
     */
    float               m_tsNormal           {1.0F};
    float               m_tsAlt              {1.5F};

    mutable QRecursiveMutex  m_playingInfoLock;
    mutable QRecursiveMutex  m_deletePlayerLock;
    mutable QRecursiveMutex  m_stateLock;

    // Signal info
    mutable QStringList m_lastSignalMsg;
    mutable MythTimer   m_lastSignalMsgTime;
    mutable InfoMap     m_lastSignalUIInfo;
    mutable MythTimer   m_lastSignalUIInfoTime;

    // tv state related
    MythDeque<TVState>  m_nextState;

    /// Timeout after last Signal Monitor message for ignoring OSD when exiting.
    static constexpr std::chrono::milliseconds kSMExitTimeout { 2s };
    static constexpr uint kMaxChannelHistory { 30 };
};

#endif // PLAYER_CONTEXT_H
