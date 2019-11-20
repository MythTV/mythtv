#ifndef OSD_H
#define OSD_H

// Qt headers

#include <QCoreApplication>
#include <QHash>

// MythTV headers

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
    explicit OSDHideEvent(enum OSDFunctionalType osdFunctionalType)
        : QEvent(kEventType), m_osdFunctionalType(osdFunctionalType) { }

    int GetFunctionalType() { return m_osdFunctionalType; }

    static Type kEventType;

  private:
    OSDFunctionalType m_osdFunctionalType;
};

class ChannelEditor : public MythScreenType
{
    Q_OBJECT

  public:
    ChannelEditor(QObject *retobject, const char *name)
        : MythScreenType((MythScreenType*)nullptr, name),
          m_retObject(retobject) {}

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType

    void SetText(const InfoMap &map);
    void GetText(InfoMap &map);

  protected:
    MythUITextEdit *m_callsignEdit {nullptr};
    MythUITextEdit *m_channumEdit  {nullptr};
    MythUITextEdit *m_channameEdit {nullptr};
    MythUITextEdit *m_xmltvidEdit  {nullptr};

    QObject        *m_retObject    {nullptr};

    void sendResult(int result);

  public slots:
    void Confirm();
    void Probe();
};

class MythOSDWindow : public MythScreenType
{
    Q_OBJECT
  public:
    MythOSDWindow(MythScreenStack *parent, const QString &name,
                  bool themed)
      : MythScreenType(parent, name, true), m_themed(themed)
    {
    }

    bool Create(void) override // MythScreenType
    {
        if (m_themed)
            return XMLParseBase::LoadWindowFromXML("osd.xml", objectName(),
                                                   this);
        return false;
    }

  private:
    bool m_themed;
};

class OSD
{
    Q_DECLARE_TR_FUNCTIONS(OSD)

  public:
    OSD(MythPlayer *player, QObject *parent, MythPainter *painter)
        : m_parent(player), m_ParentObject(parent), m_CurrentPainter(painter) {}
   ~OSD();

    bool    Init(const QRect &rect, float font_aspect);
    void    SetPainter(MythPainter *painter);
    QRect   Bounds(void) const { return m_Rect; }
    int     GetFontStretch(void) const { return m_fontStretch; }
    void    OverrideUIScale(bool log = true);
    void    RevertUIScale(void);
    bool    Reinit(const QRect &rect, float font_aspect);
    void    SetFunctionalWindow(const QString &window,
                                enum OSDFunctionalType type);
    void    SetTimeouts(int _short, int _medium, int _long);

    bool    IsVisible(void);
    void    HideAll(bool keepsubs = true, MythScreenType *except = nullptr,
                    bool dropnotification = false);

    MythScreenType *GetWindow(const QString &window);
    void    SetExpiry(const QString &window, enum OSDTimeout timeout,
                      int custom_timeout = 0);
    void    HideWindow(const QString &window);
    bool    HasWindow(const QString &window);
    void    ResetWindow(const QString &window);
    void    PositionWindow(MythScreenType *window);
    void    RemoveWindow(const QString &window);

    bool    DrawDirect(MythPainter* painter, QSize size, bool repaint = false);
    QRegion Draw(MythPainter* painter, QPaintDevice *device, QSize size,
                 QRegion &changed, int alignx = 0, int aligny = 0);

    void SetValues(const QString &window, const QHash<QString,int> &map,
                   OSDTimeout timeout);
    void SetValues(const QString &window, const QHash<QString,float> &map,
                   OSDTimeout timeout);
    void SetText(const QString &window, const InfoMap &map,
                 OSDTimeout timeout);
    void SetRegions(const QString &window, frm_dir_map_t &map,
                 long long total);
    void SetGraph(const QString &window, const QString &graph, int64_t timecode);
    bool IsWindowVisible(const QString &window);

    bool DialogVisible(const QString& window = QString());
    bool DialogHandleKeypress(QKeyEvent *e);
    bool DialogHandleGesture(MythGestureEvent *e);
    void DialogQuit(void);
    void DialogShow(const QString &window, const QString &text = "",
          int updatefor = 0);
    void DialogSetText(const QString &text);
    void DialogBack(const QString& text = "", const QVariant& data = 0, bool exit = false);
    void DialogAddButton(const QString& text, QVariant data,
                         bool menu = false, bool current = false);
    void DialogGetText(InfoMap &map);

    TeletextScreen* InitTeletext(void);
    void EnableTeletext(bool enable, int page);
    bool TeletextAction(const QString &action);
    void TeletextReset(void);
    void TeletextClear(void);

    SubtitleScreen* InitSubtitles(void);
    void EnableSubtitles(int type, bool forced_only = false);
    void DisableForcedSubtitles(void);
    void ClearSubtitles(void);
    void DisplayDVDButton(AVSubtitle* dvdButton, QRect &pos);

    void DisplayBDOverlay(BDOverlay *overlay);
    MythPlayer *GetPlayer(void) { return m_parent; }

  private:
    void TearDown(void);
    void LoadWindows(void);

    void CheckExpiry(void);
    void SendHideEvent(void);
    void SetExpiry1(const QString &window, enum OSDTimeout timeout,
                      int custom_timeout);

  private:
    MythPlayer     *m_parent           {nullptr};
    QObject        *m_ParentObject     {nullptr};
    MythPainter    *m_CurrentPainter   {nullptr};
    QRect           m_Rect;
    int             m_FadeTime         {kOSDFadeTime};
    MythScreenType *m_Dialog           {nullptr};
    QString         m_PulsedDialogText;
    QDateTime       m_NextPulseUpdate;
    bool            m_Refresh          {false};
    bool            m_Visible          {false};
    int             m_Timeouts[4]      {-1,3000,5000,13000};

    bool            m_UIScaleOverride  {false};
    float           m_SavedWMult       {1.0F};
    float           m_SavedHMult       {1.0F};
    QRect           m_SavedUIRect;
    int             m_fontStretch      {100};
    int             m_savedFontStretch {100};

    enum OSDFunctionalType m_FunctionalType {kOSDFunctionalType_Default};
    QString                m_FunctionalWindow;

    QMap<QString, MythScreenType*>    m_Children;
    QHash<MythScreenType*, QDateTime> m_ExpireTimes;
};

class OsdNavigation : public MythScreenType
{
    Q_OBJECT

  public:
    OsdNavigation(QObject *retobject, const QString &name, OSD *osd)
        : MythScreenType((MythScreenType*)nullptr, name),
          m_retObject(retobject), m_osd(osd) {}

    bool Create(void) override; // MythScreenType
    bool keyPressEvent(QKeyEvent *event) override; // MythScreenType
    void SetTextFromMap(const InfoMap &infoMap) override; // MythUIComposite
    int getVisibleGroup() {return m_visibleGroup; }
    // Virtual
    void ShowMenu(void) override; // MythScreenType

  protected:

    QObject      *m_retObject       {nullptr};
    OSD          *m_osd             {nullptr};
    MythUIButton *m_playButton      {nullptr};
    MythUIButton *m_pauseButton     {nullptr};
    MythUIButton *m_muteButton      {nullptr};
    MythUIButton *m_unMuteButton    {nullptr};
    char          m_paused          {'X'};
    char          m_muted           {'X'};
    int           m_visibleGroup    {0};
    int           m_maxGroupNum     {-1};
    bool          m_IsVolumeControl {true};

    void sendResult(int result, const QString& action);

  public slots:
    void GeneralAction(void);
    void More(void);
};

#endif
