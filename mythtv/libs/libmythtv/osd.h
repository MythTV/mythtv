#ifndef OSD_H
#define OSD_H

using namespace std;

#include "programtypes.h"
#include "mythscreentype.h"

class MythDialogBox;
struct AVSubtitle;

#define OSD_DLG_VIDEOEXIT "OSD_VIDEO_EXIT"
#define OSD_DLG_MENU      "OSD_MENU"
#define OSD_DLG_SLEEP     "OSD_SLEEP"
#define OSD_DLG_IDLE      "OSD_IDLE"
#define OSD_DLG_INFO      "OSD_INFO"
#define OSD_DLG_EDITING   "OSD_EDITING"
#define OSD_DLG_ASKALLOW  "OSD_ASKALLOW"
#define OSD_DLG_EDITOR    "OSD_EDITOR"
#define OSD_DLG_CUTPOINT  "OSD_CUTPOINT"
#define OSD_DLG_DELETE    "OSD_DELETE"
#define OSD_WIN_TELETEXT  "OSD_TELETEXT"
#define OSD_WIN_SUBTITLE  "OSD_SUBTITLES"
#define OSD_WIN_INTERACT  "OSD_INTERACTIVE"

class NuppelVideoPlayer;
class TeletextScreen;
class TeletextViewer;
class ccText;
class CC708Service;
class TV;
class SubtitleScreen;
class SubtitleReader;

enum OSDFunctionalType
{
    kOSDFunctionalType_Default = 0,
    kOSDFunctionalType_PictureAdjust,
    kOSDFunctionalType_SmartForward,
    kOSDFunctionalType_TimeStretchAdjust,
    kOSDFunctionalType_AudioSyncAdjust
};

class MPUBLIC OSDHideEvent : public QEvent
{
  public:
    OSDHideEvent(enum OSDFunctionalType osdFunctionalType)
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
    ChannelEditor(const char * name);

    virtual bool Create(void);
    virtual bool keyPressEvent(QKeyEvent *event);

    void SetReturnEvent(QObject *retobject, const QString &resultid);
    void SetText(QHash<QString,QString>&map);
    void GetText(QHash<QString,QString>&map);

  protected:
    MythUITextEdit *m_callsignEdit;
    MythUITextEdit *m_channumEdit;
    MythUITextEdit *m_channameEdit;
    MythUITextEdit *m_xmltvidEdit;

    QObject *m_retObject;

    void sendResult(int result, bool confirm);

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

    virtual bool Create(void)
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
  public:
    OSD(NuppelVideoPlayer *player, QObject *parent);
   ~OSD();

    bool    Init(const QRect &rect, float font_aspect);
    QRect   Bounds(void) { return m_Rect; }
    int     GetFontStretch(void) { return m_fontStretch; }
    void    OverrideUIScale(void);
    void    RevertUIScale(void);
    bool    Reinit(const QRect &rect, float font_aspect);
    void    DisableFade(void) { m_Effects = false; }
    void    SetFunctionalWindow(const QString window,
                                enum OSDFunctionalType type);

    bool    IsVisible(void);
    void    HideAll(bool keepsubs = true);

    MythScreenType *GetWindow(const QString &window);
    void    DisableExpiry(const QString &window);
    void    HideWindow(const QString &window);
    bool    HasWindow(const QString &window);
    void    ResetWindow(const QString &window);
    void    PositionWindow(MythScreenType *window);

    bool    DrawDirect(MythPainter* painter, QSize size, bool repaint = false);
    QRegion Draw(MythPainter* painter, QPaintDevice *device, QSize size,
                 QRegion &changed, int alignx = 0, int aligny = 0);

    void SetValues(const QString &window, QHash<QString,int> &map,
                   bool set_expiry = true);
    void SetValues(const QString &window, QHash<QString,float> &map,
                   bool set_expiry = true);
    void SetText(const QString &window, QHash<QString,QString> &map,
                 bool set_expiry = true);
    void SetRegions(const QString &window, frm_dir_map_t &map,
                 long long total);
    bool IsWindowVisible(const QString &window);

    bool DialogVisible(QString window = QString());
    bool DialogHandleKeypress(QKeyEvent *e);
    void DialogQuit(void);
    void DialogShow(const QString &window, const QString &text = "",
                    int updatefor = 0);
    void DialogSetText(const QString &text);
    void DialogBack(QString text = "", QVariant data = 0, bool exit = false);
    void DialogAddButton(QString text, QVariant data,
                         bool menu = false, bool current = false);
    void DialogGetText(QHash<QString,QString> &map);

    TeletextScreen* InitTeletext(void);
    void EnableTeletext(bool enable, int page);
    bool TeletextAction(const QString &action);
    void TeletextReset(void);

    SubtitleScreen* InitSubtitles(void);
    void EnableSubtitles(int type);
    void ClearSubtitles(void);
    void DisplayDVDButton(AVSubtitle* dvdButton, QRect &pos);

  private:
    void TearDown(void);
    void LoadWindows(void);
    void RemoveWindow(const QString &window);
    void CheckExpiry(void);
    void SetExpiry(MythScreenType *window, int time = 5);
    void SendHideEvent(void);

  private:
    NuppelVideoPlayer *m_parent;
    QObject        *m_ParentObject;
    QRect           m_Rect;
    bool            m_Effects;
    int             m_FadeTime;
    MythScreenType *m_Dialog;
    QString         m_PulsedDialogText;
    QDateTime       m_NextPulseUpdate;
    bool            m_Refresh;

    bool            m_UIScaleOverride;
    float           m_SavedWMult;
    float           m_SavedHMult;
    QRect           m_SavedUIRect;
    int             m_fontStretch;

    enum OSDFunctionalType m_FunctionalType;
    QString                m_FunctionalWindow;

    QHash<QString, MythScreenType*>   m_Children;
    QHash<MythScreenType*, QDateTime> m_ExpireTimes;
};

#endif
