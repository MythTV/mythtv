#ifndef MYTHPLAYERUI_H
#define MYTHPLAYERUI_H

// MythTV
#include "mythplayereditorui.h"
#include "mythvideoscantracker.h"
#include "jitterometer.h"
#include "mythplayer.h"

class MythDisplay;

class MTV_PUBLIC MythPlayerUI : public MythPlayerEditorUI, public MythVideoScanTracker
{
    Q_OBJECT

  public slots:
    void OSDDebugVisibilityChanged(bool Visible);

  protected slots:
    void InitialiseState() override;
    void ChangeOSDDebug();
    void UpdateOSDDebug();
    virtual void SetBookmark(bool Clear = false);
    virtual void SetLastPlayPosition(uint64_t frame = 0);

  public:
    MythPlayerUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);

    bool StartPlaying();
    virtual void InitialSeek();
    virtual void EventLoop();
    void         ReinitVideo(bool ForceUpdate) override;
    virtual void VideoStart();
    virtual void EventStart();
    virtual bool VideoLoop();
    virtual void PreProcessNormalFrame();
    void ChangeSpeed() override;
    void SetVideoParams(int Width, int Height, double FrameRate, float Aspect,
                        bool ForceUpdate, int ReferenceFrames,
                        FrameScanType Scan = kScan_Ignore,
                        const QString& CodecName = QString()) override;
    void GetPlaybackData(InfoMap& Map);
    void GetCodecDescription(InfoMap& Map);
    bool CanSupportDoubleRate();
    void SetWatched(bool ForceWatched = false);

  protected:
    void InitFrameInterval() override;
    virtual void DisplayPauseFrame();
    virtual bool DisplayNormalFrame(bool CheckPrebuffer = true);

    void FileChanged();
    void RefreshPauseFrame();
    void RenderVideoFrame(MythVideoFrame* Frame, FrameScanType Scan, bool Prepare, std::chrono::microseconds Wait);
    void DoDisplayVideoFrame(MythVideoFrame* Frame, std::chrono::microseconds Due);
    void EnableFrameRateMonitor(bool Enable = false);
    void EnableBitrateMonitor(bool Enable = false);

    Jitterometer    m_outputJmeter { "Player" };
    std::chrono::microseconds m_refreshInterval { 0us };

  private:
    Q_DISABLE_COPY(MythPlayerUI)

    void  SwitchToProgram();
    void  JumpToProgram();
    void  JumpToStream(const QString &stream);

    bool    m_osdDebug { false };
    QTimer  m_osdDebugTimer;

    MythDisplay *m_display { nullptr };
};

#endif
