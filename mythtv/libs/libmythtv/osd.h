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

#define kOSDFadeTime 1000

class MythPlayer;
class TeletextScreen;
class SubtitleScreen;
struct AVSubtitle;
class BDOverlay;

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

class MTV_PUBLIC OSDHideEvent : public QEvent
{
  public:
    explicit OSDHideEvent(enum OSDFunctionalType OsdFunctionalType)
      : QEvent(kEventType),
        m_osdFunctionalType(OsdFunctionalType) { }

    int GetFunctionalType() { return m_osdFunctionalType; }

    static Type kEventType;

  private:
    OSDFunctionalType m_osdFunctionalType;
};

class ChannelEditor : public MythScreenType
{
    Q_OBJECT

  public:
    ChannelEditor(QObject *RetObject, const char * Name)
        : MythScreenType((MythScreenType*)nullptr, Name),
          m_retObject(RetObject) {}

    bool Create(void) override;
    bool keyPressEvent(QKeyEvent *Event) override;
    void SetText(const InfoMap &Map);
    void GetText(InfoMap &Map);

  public slots:
    void Confirm();
    void Probe();

  protected:
    void SendResult(int result);

    MythUITextEdit *m_callsignEdit { nullptr };
    MythUITextEdit *m_channumEdit  { nullptr };
    MythUITextEdit *m_channameEdit { nullptr };
    MythUITextEdit *m_xmltvidEdit  { nullptr };
    QObject        *m_retObject    { nullptr };
};

class MythOSDWindow : public MythScreenType
{
    Q_OBJECT

  public:
    MythOSDWindow(MythScreenStack *Parent, const QString &Name, bool Themed)
      : MythScreenType(Parent, Name, true),
        m_themed(Themed)
    {
    }

    bool Create(void) override
    {
        if (m_themed)
            return XMLParseBase::LoadWindowFromXML("osd.xml", objectName(), this);
        return false;
    }

  private:
    bool m_themed { false };
};

class OSD
{
    Q_DECLARE_TR_FUNCTIONS(OSD)

  public:
    OSD(MythPlayer *Player, QObject *Parent, MythPainter *Painter)
        : m_parent(Player), m_ParentObject(Parent), m_CurrentPainter(Painter) {}
   ~OSD();

    bool    Init(const QRect &Rect, float FontAspect);
    void    SetPainter(MythPainter *Painter);
    QRect   Bounds(void) const { return m_Rect; }
    int     GetFontStretch(void) const { return m_fontStretch; }
    void    OverrideUIScale(bool Log = true);
    void    RevertUIScale(void);
    bool    Reinit(const QRect &Rect, float FontAspect);
    void    SetFunctionalWindow(const QString &Window, enum OSDFunctionalType Type);
    void    SetTimeouts(int Short, int Medium, int Long);
    bool    IsVisible(void);
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
    void SetText(const QString &Window, const InfoMap &Map, OSDTimeout Timeout);
    void SetRegions(const QString &Window, frm_dir_map_t &Map, long long Total);
    void SetGraph(const QString &Window, const QString &Graph, int64_t Timecode);
    bool IsWindowVisible(const QString &Window);

    bool DialogVisible(const QString& Window = QString());
    bool DialogHandleKeypress(QKeyEvent *Event);
    bool DialogHandleGesture(MythGestureEvent *Event);
    void DialogQuit(void);
    void DialogShow(const QString &Window, const QString &Text = "", int UpdateFor = 0);
    void DialogSetText(const QString &Text);
    void DialogBack(const QString& Text = "", const QVariant& Data = 0, bool Exit = false);
    void DialogAddButton(const QString& Text, QVariant Data, bool Menu = false, bool Current = false);
    void DialogGetText(InfoMap &Map);

    TeletextScreen* InitTeletext(void);
    void EnableTeletext(bool Enable, int Page);
    bool TeletextAction(const QString &Action);
    void TeletextReset(void);
    void TeletextClear(void);

    SubtitleScreen* InitSubtitles(void);
    void EnableSubtitles(int Type, bool ForcedOnly = false);
    void DisableForcedSubtitles(void);
    void ClearSubtitles(void);
    void DisplayDVDButton(AVSubtitle* DVDButton, QRect &Pos);

    void DisplayBDOverlay(BDOverlay *Overlay);
    MythPlayer *GetPlayer(void) { return m_parent; }

  private:
    void TearDown(void);
    void LoadWindows(void);

    void CheckExpiry(void);
    void SendHideEvent(void);
    void SetExpiryPriv(const QString &Window, enum OSDTimeout Timeout, int CustomTimeout);

  private:
    MythPlayer     *m_parent            { nullptr };
    QObject        *m_ParentObject      { nullptr };
    MythPainter    *m_CurrentPainter    { nullptr };
    QRect           m_Rect              { };
    int             m_FadeTime          { kOSDFadeTime };
    MythScreenType *m_Dialog            { nullptr };
    QString         m_PulsedDialogText  { };
    QDateTime       m_NextPulseUpdate   { };
    bool            m_Refresh           { false };
    bool            m_Visible           { false };
    int             m_Timeouts[4]       { -1,3000,5000,13000 };
    bool            m_UIScaleOverride   { false };
    float           m_SavedWMult        { 1.0F };
    float           m_SavedHMult        { 1.0F };
    QRect           m_SavedUIRect       { };
    int             m_fontStretch       { 100 };
    int             m_savedFontStretch  { 100 };
    enum OSDFunctionalType m_FunctionalType { kOSDFunctionalType_Default };
    QString                m_FunctionalWindow { };
    QMap<QString, MythScreenType*>    m_Children { };
    QHash<MythScreenType*, QDateTime> m_ExpireTimes { };
};

class OsdNavigation : public MythScreenType
{
    Q_OBJECT

  public:
    OsdNavigation(QObject *RetObject, const QString &Name, OSD *Osd)
        : MythScreenType((MythScreenType*)nullptr, Name),
          m_retObject(RetObject), m_osd(Osd) {}
    bool Create(void) override;
    bool keyPressEvent(QKeyEvent *Event) override;
    void SetTextFromMap(const InfoMap &Map) override;
    void ShowMenu(void) override;

    int getVisibleGroup() { return m_visibleGroup; }

  public slots:
    void GeneralAction(void);
    void More(void);

  protected:
    void SendResult(int Result, const QString& Action);

    QObject      *m_retObject       { nullptr };
    OSD          *m_osd             { nullptr };
    MythUIButton *m_playButton      { nullptr };
    MythUIButton *m_pauseButton     { nullptr };
    MythUIButton *m_muteButton      { nullptr };
    MythUIButton *m_unMuteButton    { nullptr };
    char          m_paused          { 'X' };
    char          m_muted           { 'X' };
    int           m_visibleGroup    { 0 };
    int           m_maxGroupNum     { -1 };
    bool          m_IsVolumeControl { true };
};

#endif
