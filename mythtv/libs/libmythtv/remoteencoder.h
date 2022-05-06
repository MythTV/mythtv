#ifndef REMOTEENCODER_H_
#define REMOTEENCODER_H_

#include <cstdint>
#include <utility>

// Qt headers
#include <QHash>
#include <QMap>
#include <QMutex>
#include <QString>

// MythTV headers
#include "libmythbase/mythtimer.h"
#include "libmythbase/mythtypes.h"
#include "libmythbase/programtypes.h"
#include "libmythtv/mythtvexp.h"
#include "libmythtv/tv.h"
#include "libmythtv/videoouttypes.h"

class ProgramInfo;
class MythSocket;

class MTV_PUBLIC RemoteEncoder
{
  public:
    RemoteEncoder(int num, QString host, short port)
    : m_recordernum(num), m_remotehost(std::move(host)), m_remoteport(port) {}
   ~RemoteEncoder(void);

    bool Setup(void);
    bool IsValidRecorder(void) const;
    int GetRecorderNumber(void) const;

    ProgramInfo *GetRecording(void);
    bool IsRecording(bool *ok = nullptr);
    float GetFrameRate(void);
    long long GetFramesWritten(void);
    /// \brief Return value last returned by GetFramesWritten().
    long long GetCachedFramesWritten(void) const { return m_cachedFramesWritten; }
    long long GetFilePosition(void);
    long long GetFreeDiskSpace();
    long long GetMaxBitrate();
    int64_t GetKeyframePosition(uint64_t desired);
    void FillPositionMap(int64_t start, int64_t end,
                         frm_pos_map_t &positionMap);
    void FillDurationMap(int64_t start, int64_t end,
                         frm_pos_map_t &durationMap);
    void StopPlaying(void);
    void SpawnLiveTV(const QString& chainid, bool pip, const QString& startchan);
    void StopLiveTV(void);
    void PauseRecorder(void);
    void FinishRecording(void);
    void FrontendReady(void);
    void CancelNextRecording(bool cancel);

    void SetLiveRecording(bool recording);
    QString GetInput(void);
    QString SetInput(const QString &input);
    int  GetPictureAttribute(PictureAttribute attr);
    int  ChangePictureAttribute(
        PictureAdjustType type, PictureAttribute attr, bool up);
    void ChangeChannel(int channeldirection);
    void ChangeDeinterlacer(int deint_mode);
    void ToggleChannelFavorite(const QString &changroupname);
    void SetChannel(const QString& channel);
    std::chrono::milliseconds SetSignalMonitoringRate(std::chrono::milliseconds rate, int notifyFrontend = 1);
    uint GetSignalLockTimeout(const QString& input);
    bool CheckChannel(const QString& channel);
    bool ShouldSwitchToAnotherCard(const QString& channelid);
    bool CheckChannelPrefix(const QString &prefix, uint &complete_valid_channel_on_rec,
                            bool &is_extra_char_useful, QString &needed_spacer);
    void GetNextProgram(int direction,
                        QString &title, QString &subtitle, QString &desc, 
                        QString &category, QString &starttime, QString &endtime,
                        QString &callsign, QString &iconpath,
                        QString &channelname, QString &chanid,
                        QString &seriesid, QString &programid);
    void GetChannelInfo(InfoMap &infoMap, uint chanid = 0);
    bool SetChannelInfo(const InfoMap &infoMap);
    bool GetErrorStatus(void) { bool v = m_backendError; m_backendError = false; 
                                return v; }
 
  private:
    bool SendReceiveStringList(QStringList &strlist, uint min_reply_length = 0);

    int                 m_recordernum;

    MythSocket         *m_controlSock         {nullptr};
    QMutex              m_lock;

    QString             m_remotehost;
    short               m_remoteport;

    QString             m_lastchannel;
    QString             m_lastinput;

    bool                m_backendError        {false};
    long long           m_cachedFramesWritten {0};
    QMap<QString,uint>  m_cachedTimeout;
    MythTimer           m_lastTimeCheck;
};

#endif

