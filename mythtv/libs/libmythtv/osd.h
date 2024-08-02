#ifndef OSD_H
#define OSD_H

// MythTV
#include "libmythbase/mythtypes.h"
#include "libmythbase/programtypes.h"
#include "libmythtv/mythmediaoverlay.h"
#include "libmythtv/mythplayerstate.h"
#include "libmythui/mythscreentype.h"

// Screen names are prepended with alphanumerics to force the correct ordering
// when displayed. This is slightly complicated by the default windows
// (e.g. osd_window) whose names are hard coded into existing themes.

// menu dialogs should always be on top
static constexpr const char* OSD_DLG_VIDEOEXIT { "xx_OSD_VIDEO_EXIT" };
static constexpr const char* OSD_DLG_MENU      { "xx_OSD_MENU"       };
static constexpr const char* OSD_DLG_SLEEP     { "xx_OSD_SLEEP"      };
static constexpr const char* OSD_DLG_IDLE      { "xx_OSD_IDLE"       };
static constexpr const char* OSD_DLG_INFO      { "xx_OSD_INFO"       };
static constexpr const char* OSD_DLG_EDITING   { "xx_OSD_EDITING"    };
static constexpr const char* OSD_DLG_ASKALLOW  { "xx_OSD_ASKALLOW"   };
static constexpr const char* OSD_DLG_EDITOR    { "xx_OSD_EDITOR"     };
static constexpr const char* OSD_DLG_CUTPOINT  { "xx_OSD_CUTPOINT"   };
static constexpr const char* OSD_DLG_DELETE    { "xx_OSD_DELETE"     };
static constexpr const char* OSD_DLG_NAVIGATE  { "xx_OSD_NAVIGATE"   };
static constexpr const char* OSD_DLG_CONFIRM   { "mythconfirmpopup"  };

static constexpr const char* OSD_WIN_MESSAGE  { "osd_message"        };
static constexpr const char* OSD_WIN_INPUT    { "osd_input"          };
static constexpr const char* OSD_WIN_PROGINFO { "program_info"       };
static constexpr const char* OSD_WIN_STATUS   { "osd_status"         };
static constexpr const char* OSD_WIN_DEBUG    { "osd_debug"          };
static constexpr const char* OSD_WIN_BROWSE   { "browse_info"        };
static constexpr const char* OSD_WIN_PROGEDIT { "osd_program_editor" };

static constexpr std::chrono::milliseconds kOSDFadeTime { 1s };

class TV;
class MythMainWindow;
class MythPlayerUI;


enum OSDFunctionalType : std::uint8_t
{
    kOSDFunctionalType_Default = 0,
    kOSDFunctionalType_PictureAdjust,
    kOSDFunctionalType_SmartForward,
    kOSDFunctionalType_TimeStretchAdjust,
    kOSDFunctionalType_AudioSyncAdjust,
    kOSDFunctionalType_SubtitleZoomAdjust,
    kOSDFunctionalType_SubtitleDelayAdjust
};

enum OSDTimeout : std::int8_t
{
    kOSDTimeout_Ignore = -1, // Don't update existing timeout
    kOSDTimeout_None   = 0,  // Don't timeout
    kOSDTimeout_Short  = 1,
    kOSDTimeout_Med    = 2,
    kOSDTimeout_Long   = 3,
};

class MythOSDDialogData
{
  public:
    class MythOSDDialogButton
    {
      public:
        QString  m_text;
        QVariant m_data;
        bool     m_menu    { false };
        bool     m_current { false };
    };

    class MythOSDBackButton
    {
      public:
        QString  m_text;
        QVariant m_data { 0 };
        bool     m_exit { false };
    };

    QString m_dialogName;
    QString m_message { }; //NOLINT(readability-redundant-member-init)
    std::chrono::milliseconds m_timeout { 0ms };
    std::vector<MythOSDDialogButton> m_buttons { }; //NOLINT(readability-redundant-member-init)
    MythOSDBackButton m_back { };
};

Q_DECLARE_METATYPE(MythOSDDialogData)

class OSD : public MythMediaOverlay
{
    Q_OBJECT

  signals:
    void HideOSD(OSDFunctionalType Type);

  public slots:
    void SetText(const QString& Window, const InfoMap& Map, OSDTimeout Timeout);
    void DialogQuit();
    void HideAll(bool KeepSubs = true, MythScreenType* Except = nullptr, bool DropNotification = false);
    void Embed(bool Embedding);

  protected slots:
    void ShowDialog(const MythOSDDialogData& Data);
    void IsOSDVisible(bool& Visible);

  public:
    OSD(MythMainWindow* MainWindow, TV* Tv, MythPlayerUI* Player, MythPainter* Painter);
   ~OSD() override;

    bool Init(QRect Rect, float FontAspect) override;
    void HideWindow(const QString &Window) override;
    void SetFunctionalWindow(const QString &Window, enum OSDFunctionalType Type);

    void SetExpiry(const QString &Window, enum OSDTimeout Timeout, std::chrono::milliseconds CustomTimeout = 0ms);
    void ResetWindow(const QString &Window);
    void Draw();

    void SetValues(const QString &Window, const QHash<QString,int> &Map, OSDTimeout Timeout);
    void SetValues(const QString &Window, const QHash<QString,float> &Map, OSDTimeout Timeout);
    void SetRegions(const QString &Window, frm_dir_map_t &Map, long long Total);
    void SetGraph(const QString &Window, const QString &Graph, std::chrono::milliseconds Timecode);
    bool IsWindowVisible(const QString &Window);

    bool DialogVisible(const QString& Window = QString());
    bool DialogHandleKeypress(QKeyEvent *Event);
    bool DialogHandleGesture(MythGestureEvent *Event);
    void DialogGetText(InfoMap &Map);

  private:
    void PositionWindow(MythScreenType* Window);
    void RemoveWindow(const QString& Window);
    void DialogShow(const QString& Window, const QString& Text = "", std::chrono::milliseconds UpdateFor = 0ms);
    void DialogAddButton(const QString& Text, QVariant Data, bool Menu = false, bool Current = false);
    void DialogBack(const QString& Text = "", const QVariant& Data = 0, bool Exit = false);
    void TearDown() override;
    void LoadWindows();
    void CheckExpiry();
    void SetExpiryPriv(const QString &Window, enum OSDTimeout Timeout, std::chrono::milliseconds CustomTimeout);

  private:
    bool            m_embedded          { false };
    std::chrono::milliseconds m_fadeTime { kOSDFadeTime };
    MythScreenType* m_dialog            { nullptr };
    QString         m_pulsedDialogText;
    QDateTime       m_nextPulseUpdate;
    std::array<std::chrono::milliseconds,4> m_timeouts  { -1ms, 3s, 5s, 13s };
    enum OSDFunctionalType m_functionalType { kOSDFunctionalType_Default };
    QString                m_functionalWindow;
    QHash<MythScreenType*, QDateTime> m_expireTimes;
};

#endif
