#ifndef MYTHPLAYERUI_H
#define MYTHPLAYERUI_H

// MythTV
#include "mythplayervisualiserui.h"
#include "mythvideoscantracker.h"
#include "DetectLetterbox.h"
#include "jitterometer.h"
#include "mythplayer.h"

class MTV_PUBLIC MythPlayerUI : public MythPlayerVisualiserUI, public MythVideoScanTracker
{
    Q_OBJECT

  public:
    MythPlayerUI(MythMainWindow* MainWindow, TV* Tv, PlayerContext* Context, PlayerFlags Flags);

    virtual void EventLoop();
    void ReinitVideo(bool ForceUpdate) override;
    void VideoStart() override;
    virtual bool VideoLoop();
    void ChangeSpeed() override;
    void ReleaseNextVideoFrame(MythVideoFrame* Frame, int64_t Timecode, bool Wrap = true) override;
    void SetVideoParams(int Width, int Height, double FrameRate, float Aspect,
                        bool ForceUpdate, int ReferenceFrames,
                        FrameScanType Scan = kScan_Ignore,
                        const QString& CodecName = QString()) override;
    void GetPlaybackData(InfoMap& Map);
    void GetCodecDescription(InfoMap& Map);
    void ToggleAdjustFill(AdjustFillMode Mode = kAdjustFill_Toggle);
    void EnableFrameRateMonitor(bool Enable = false);
    bool CanSupportDoubleRate();

    // FIXME - should be private
    DetectLetterbox m_detectLetterBox { this };

    // N.B. Editor - keep ringfenced and move into subclass
    bool EnableEdit();
    void DisableEdit(int HowToSave);
    bool HandleProgramEditorActions(QStringList& Actions);
    uint64_t GetNearestMark(uint64_t Frame, bool Right);
    bool IsTemporaryMark(uint64_t Frame);
    bool HasTemporaryMark();
    bool IsCutListSaved();
    bool DeleteMapHasUndo();
    bool DeleteMapHasRedo();
    QString DeleteMapGetUndoMessage();
    QString DeleteMapGetRedoMessage();
    // End editor stuff

  protected:
    void InitFrameInterval() override;
    void DisplayPauseFrame() override;
    virtual void DisplayNormalFrame(bool CheckPrebuffer = true);

    void RefreshPauseFrame();
    void RenderVideoFrame(MythVideoFrame* Frame, FrameScanType Scan, bool Prepare, int64_t Wait);
    void DoDisplayVideoFrame(MythVideoFrame* Frame, int64_t Due);

    Jitterometer    m_outputJmeter { "Player" };

    // N.B. Editor - keep ringfenced and move into subclass
    QElapsedTimer   m_editUpdateTimer;
    float           m_speedBeforeEdit  { 1.0   };
    bool            m_pausedBeforeEdit { false };
};

#endif
