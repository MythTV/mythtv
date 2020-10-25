#ifndef OSD_H
#define OSD_H

// Qt
#include <QCoreApplication>
#include <QHash>

// MythTV
#include "mythtvexp.h"
#include "programtypes.h"
#include "mythscreentype.h"
#include "mythtypes.h"
#include "mythplayerstate.h"

// Screen names are prepended with alphanumerics to force the correct ordering
// when displayed. This is slightly complicated by the default windows
// (e.g. osd_window) whose names are hard coded into existing themes.

// menu dialogs should always be on top
#define OSD_DLG_VIDEOEXIT "xx_OSD_VIDEO_EXIT"
#define OSD_DLG_MENU      "xx_OSD_MENU"
#define OSD_DLG_SLEEP     "xx_OSD_SLEEP"
#define OSD_DLG_IDLE      "xx_OSD_IDLE"
#define OSD_DLG_INFO      "xx_OSD_INFO"
#define OSD_DLG_EDITING   "xx_OSD_EDITING"
#define OSD_DLG_ASKALLOW  "xx_OSD_ASKALLOW"
#define OSD_DLG_EDITOR    "xx_OSD_EDITOR"
#define OSD_DLG_CUTPOINT  "xx_OSD_CUTPOINT"
#define OSD_DLG_DELETE    "xx_OSD_DELETE"
#define OSD_DLG_NAVIGATE  "xx_OSD_NAVIGATE"
#define OSD_DLG_CONFIRM   "mythconfirmpopup"
// subtitles are always painted first
#define OSD_WIN_TELETEXT  "aa_OSD_TELETEXT"
#define OSD_WIN_SUBTITLE  "aa_OSD_SUBTITLES"
// MHEG and blu-ray overlay should cover subtitles
#define OSD_WIN_INTERACT  "bb_OSD_INTERACTIVE"
#define OSD_WIN_BDOVERLAY "bb_OSD_BDOVERLAY"

#define OSD_WIN_MESSAGE  "osd_message"
#define OSD_WIN_INPUT    "osd_input"
#define OSD_WIN_PROGINFO "program_info"
#define OSD_WIN_STATUS   "osd_status"
#define OSD_WIN_DEBUG    "osd_debug"
#define OSD_WIN_BROWSE   "browse_info"
#define OSD_WIN_PROGEDIT "osd_program_editor"

#define kOSDFadeTime 1000

class TV;
class MythMainWindow;
class MythPlayerUI;
class TeletextScreen;
class SubtitleScreen;
class MythBDOverlay;
struct AVSubtitle;

enum OSDFunctionalType
{
    kOSDFunctionalType_Default = 0,
    kOSDFunctionalType_PictureAdjust,
    kOSDFunctionalType_SmartForward,
    kOSDFunctionalType_TimeStretchAdjust,
    kOSDFunctionalType_AudioSyncAdjust,
    kOSDFunctionalType_SubtitleZoomAdjust,
    kOSDFunctionalType_SubtitleDelayAdjust
};

enum OSDTimeout
{
    kOSDTimeout_Ignore = -1, // Don't update existing timeout
    kOSDTimeout_None   = 0,  // Don't timeout
    kOSDTimeout_Short  = 1,
    kOSDTimeout_Med    = 2,
    kOSDTimeout_Long   = 3,
};

class MythOSDWindow : public MythScreenType
{
    Q_OBJECT

  public:
    MythOSDWindow(MythScreenStack* Parent, MythPainter* Painter, const QString& Name, bool Themed);
    bool Create() override;

  private:
    bool m_themed { false };
};

class MythOSDDialogData
{
  public:
    class MythOSDDialogButton
    {
      public:
        QString  m_text    { };
        QVariant m_data    { };
        bool     m_menu    { false };
        bool     m_current { false };
    };

    class MythOSDBackButton
    {
      public:
        QString  m_text { };
        QVariant m_data { 0 };
        bool     m_exit { false };
    };

    QString m_dialogName;
    QString m_message { };
    int     m_timeout { kOSDTimeout_None };
    std::vector<MythOSDDialogButton> m_buttons { };
    MythOSDBackButton m_back { };
};

Q_DECLARE_METATYPE(MythOSDDialogData)

class OSD : public QObject
{
    Q_OBJECT

  signals:
    void HideOSD(OSDFunctionalType Type);

  public slots:
    void ShowDialog(const MythOSDDialogData& Data);
    void SetText(const QString& Window, const InfoMap& Map, OSDTimeout Timeout);

  public:
    OSD(MythMainWindow* MainWindow, TV* Tv, MythPlayerUI* Player, MythPainter* Painter);
   ~OSD() override;

    bool    Init(const QRect &Rect, float FontAspect);
    QRect   Bounds() const { return m_rect; }
    int     GetFontStretch() const { return m_fontStretch; }
    void    OverrideUIScale(bool Log = true);
    void    RevertUIScale();
    bool    Reinit(const QRect &Rect, float FontAspect);
    void    SetFunctionalWindow(const QString &Window, enum OSDFunctionalType Type);
    void    SetTimeouts(int Short, int Medium, int Long);
    bool    IsVisible();
    void    HideAll(bool KeepSubs = true, MythScreenType *Except = nullptr, bool DropNotification = false);
    MythScreenType *GetWindow(const QString &Window);
    void    SetExpiry(const QString &Window, enum OSDTimeout Timeout, int CustomTimeout = 0);
    void    HideWindow(const QString &Window);
    bool    HasWindow(const QString &Window);
    void    ResetWindow(const QString &Window);
    void    PositionWindow(MythScreenType *Window);
    void    RemoveWindow(const QString &Window);
    bool    Draw(MythPainter* Painter, QSize Size, bool Repaint = false);

    void SetValues(const QString &Window, const QHash<QString,int> &Map, OSDTimeout Timeout);
    void SetValues(const QString &Window, const QHash<QString,float> &Map, OSDTimeout Timeout);
    void SetRegions(const QString &Window, frm_dir_map_t &Map, long long Total);
    void SetGraph(const QString &Window, const QString &Graph, int64_t Timecode);
    bool IsWindowVisible(const QString &Window);

    bool DialogVisible(const QString& Window = QString());
    bool DialogHandleKeypress(QKeyEvent *Event);
    bool DialogHandleGesture(MythGestureEvent *Event);
    void DialogQuit();
    void DialogShow(const QString &Window, const QString &Text = "", int UpdateFor = 0);
    void DialogBack(const QString& Text = "", const QVariant& Data = 0, bool Exit = false);
    void DialogAddButton(const QString& Text, QVariant Data, bool Menu = false, bool Current = false);
    void DialogGetText(InfoMap &Map);

    TeletextScreen* InitTeletext();
    void EnableTeletext(bool Enable, int Page);
    bool TeletextAction(const QString &Action);
    void TeletextReset();
    void TeletextClear();

    SubtitleScreen* InitSubtitles();
    void EnableSubtitles(int Type, bool ForcedOnly = false);
    void DisableForcedSubtitles();
    void ClearSubtitles();
    void DisplayDVDButton(AVSubtitle* DVDButton, QRect &Pos);

    void DisplayBDOverlay(MythBDOverlay *Overlay);

  private:
    void TearDown();
    void LoadWindows();
    void CheckExpiry();
    void SetExpiryPriv(const QString &Window, enum OSDTimeout Timeout, int CustomTimeout);

  private:
    MythMainWindow* m_mainWindow        { nullptr };
    TV*             m_tv                { nullptr };
    MythPlayerUI*   m_player            { nullptr };
    MythPainter*    m_painter           { nullptr };
    QRect           m_rect              { };
    int             m_fadeTime          { kOSDFadeTime };
    MythScreenType* m_dialog            { nullptr };
    QString         m_pulsedDialogText  { };
    QDateTime       m_nextPulseUpdate   { };
    bool            m_refresh           { false };
    bool            m_visible           { false };
    std::array<int,4> m_timeouts        { -1, 3000, 5000, 13000 };
    bool            m_uiScaleOverride   { false };
    float           m_savedWMult        { 1.0F };
    float           m_savedHMult        { 1.0F };
    QRect           m_savedUIRect       { };
    int             m_fontStretch       { 100 };
    int             m_savedFontStretch  { 100 };
    enum OSDFunctionalType m_functionalType { kOSDFunctionalType_Default };
    QString                m_functionalWindow { };
    QMap<QString, MythScreenType*>    m_children { };
    QHash<MythScreenType*, QDateTime> m_expireTimes { };
};

class ChannelEditor : public MythScreenType
{
    Q_OBJECT

  public:
    ChannelEditor(MythMainWindow* MainWindow, TV* Tv, const QString& Name);
    bool Create() override;
    bool keyPressEvent(QKeyEvent* Event) override;
    void SetText(const InfoMap& Map);
    void GetText(InfoMap& Map);

  public slots:
    void Confirm();
    void Probe();

  protected:
    void SendResult(int result);

    MythUITextEdit* m_callsignEdit { nullptr };
    MythUITextEdit* m_channumEdit  { nullptr };
    MythUITextEdit* m_channameEdit { nullptr };
    MythUITextEdit* m_xmltvidEdit  { nullptr };
    MythMainWindow* m_mainWindow   { nullptr };
    TV*             m_tv           { nullptr };
};

class OsdNavigation : public MythScreenType
{
    Q_OBJECT

  public:
    OsdNavigation(MythMainWindow* MainWindow, TV* Tv, MythPlayerUI* Player, const QString& Name, OSD* Osd);
    bool Create() override;
    bool keyPressEvent(QKeyEvent* Event) override;
    void ShowMenu() override;

  public slots:
    void AudioStateChanged(MythAudioState AudioState);
    void PauseChanged(bool Paused);
    void GeneralAction();
    void More();

  protected:
    void SendResult(int Result, const QString& Action);

    MythMainWindow* m_mainWindow    { nullptr };
    TV*           m_tv              { nullptr };
    MythPlayerUI* m_player          { nullptr };
    OSD*          m_osd             { nullptr };
    MythUIButton* m_playButton      { nullptr };
    MythUIButton* m_pauseButton     { nullptr };
    MythUIButton* m_muteButton      { nullptr };
    MythUIButton* m_unMuteButton    { nullptr };
    bool          m_paused          { false   };
    MythAudioState m_audioState     { };
    int           m_visibleGroup    { 0 };
    int           m_maxGroupNum     { -1 };
};

#endif
