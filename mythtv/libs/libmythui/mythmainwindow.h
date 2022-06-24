#ifndef MYTHMAINWINDOW_H_
#define MYTHMAINWINDOW_H_

// Qt
#include <QTimer>

// MythTV
#include "libmythbase/mythchrono.h"
#include "libmythui/mythnotificationcenter.h"
#include "libmythui/mythrect.h"
#include "libmythui/mythscreenstack.h"
#include "libmythui/mythuiactions.h"
#include "libmythui/mythuiscreenbounds.h"

class QEvent;
class MythThemeBase;
class MythMediaDevice;

using MediaPlayCallback = int (*)(const QString& , const QString& , const QString& , const QString& , const QString& , int, int, const QString& , std::chrono::minutes, const QString& , const QString& , bool);

class MythScreenSaverControl;
class MythDisplay;
class MythInputDeviceHandler;
class MythMainWindowPrivate;
class MythPainterWindow;
class MythRender;

class MUI_PUBLIC MythMainWindow : public MythUIScreenBounds
{
    Q_OBJECT

    friend class MythPainterWindowOpenGL;
    friend class MythPainterWindowVulkan;
    friend class MythPainterWindowQt;

  public:
    void Init(bool MayReInit = true);
    void Show();
    void MoveResize(QRect& Geometry);

    void AddScreenStack(MythScreenStack* Stack, bool Main = false);
    void PopScreenStack();
    int GetStackCount();
    MythScreenStack* GetMainStack();
    MythScreenStack* GetStack(const QString& Stackname);
    MythScreenStack* GetStackAt(int Position);

    bool TranslateKeyPress(const QString& Context, QKeyEvent* Event,
                           QStringList& Actions, bool AllowJumps = true);
    bool KeyLongPressFilter(QEvent** Event, QScopedPointer<QEvent>& NewEvent);

    void ReloadKeys();
    void ClearKey(const QString& Context, const QString& Action);
    void ClearKeyContext(const QString& Context);
    void BindKey(const QString& Context, const QString& Action, const QString& Key);
    void RegisterKey(const QString& Context, const QString& Action,
                     const QString& Description, const QString& Key);
    static QString GetKey(const QString& Context, const QString& Action);
    QObject* GetTarget(QKeyEvent& Key);
    QString GetActionText(const QString& Context, const QString& Action) const;

    void ClearJump(const QString& Destination);
    void BindJump(const QString& Destination, const QString& Key);
    void RegisterJump(const QString& Destination, const QString& Description,
                      const QString& Key, void (*Callback)(void),
                      bool Exittomain = true, QString LocalAction = "");
    void ClearAllJumps();
    void RegisterMediaPlugin(const QString& Name, const QString& Desc,
                             MediaPlayCallback Func);
    bool HandleMedia(const QString& Handler, const QString& Mrl,
                     const QString& Plot="", const QString& Title="",
                     const QString& Subtitle="", const QString& Director="",
                     int Season=0, int Episode=0, const QString& Inetref="",
                     std::chrono::minutes LenMins=2h, const QString& Year="1895",
                     const QString& Id="", bool UseBookmarks = false);
    void HandleTVAction(const QString& Action);

    void JumpTo(const QString& Destination, bool Pop = true);
    bool DestinationExists(const QString& Destination) const;
    QStringList EnumerateDestinations() const;

    bool IsExitingToMain() const;

    static MythMainWindow *getMainWindow(bool UseDB = true);
    static void destroyMainWindow();

    MythDisplay* GetDisplay();
    MythPainter* GetPainter();
    QWidget*     GetPaintWindow();
    MythRender*  GetRenderDevice();
    MythNotificationCenter* GetCurrentNotificationCenter();
    void         ShowPainterWindow();
    void         HidePainterWindow();
    static void  GrabWindow(QImage& Image);
    static bool  SaveScreenShot(const QImage& Image, QString Filename = "");
    static bool  ScreenShot(int Width = 0, int Height = 0, QString Filename = "");
    static void  RestoreScreensaver();
    static void  DisableScreensaver();
    static void  ResetScreensaver();
    static bool  IsScreensaverAsleep();
    static bool  IsTopScreenInitialized();
    void RemoteScreenShot(QString Filename, int Width, int Height);
    void AllowInput(bool Allow);
    void RestartInputHandlers();
    uint PushDrawDisabled();
    uint PopDrawDisabled();
    void SetEffectsEnabled(bool Enable);
    void Draw(MythPainter* Painter = nullptr);
    void ResetIdleTimer();
    void PauseIdleTimer(bool Pause);
    void DisableIdleTimer(bool DisableIdle = true);
    void EnterStandby(bool Manual = true);
    void ExitStandby(bool Manual = true);

    QPaintEngine* paintEngine() const override;

  public slots:
    void MouseTimeout();
    void HideMouseTimeout();
    void IdleTimeout();

  protected slots:
    void Animate();
    void DoRemoteScreenShot(const QString& Filename, int Width, int Height);
    void SetDrawEnabled(bool Enable);
    void OnApplicationStateChange(Qt::ApplicationState State);

  signals:
    void SignalRemoteScreenShot(QString Filename, int Width, int Height);
    void SignalSetDrawEnabled(bool Enable);
    void SignalWindowReady();
    void SignalRestoreScreensaver();
    void SignalDisableScreensaver();
    void SignalResetScreensaver();

  protected:
    explicit MythMainWindow(bool UseDB = true);
    ~MythMainWindow() override;

    static void LoadQtConfig();
    void InitKeys();

    bool eventFilter(QObject* Watched, QEvent* Event) override;
    void customEvent(QEvent* Event) override;
    void closeEvent(QCloseEvent* Event) override;
    void drawScreen(QPaintEvent* Event = nullptr);
    bool event(QEvent* Event) override;
    void ExitToMainMenu();
    void ShowMouseCursor(bool Show);

  private slots:
    void DelayedAction();

  private:
    MythMainWindowPrivate* m_priv      { nullptr };
    MythDisplay*       m_display       { nullptr };
    QRegion            m_repaintRegion;
    QTimer             m_refreshTimer;
    MythThemeBase*     m_themeBase     { nullptr };
    MythPainter*       m_painter       { nullptr };
    MythPainterWindow* m_painterWin    { nullptr };
    MythInputDeviceHandler* m_deviceHandler { nullptr };
    MythScreenSaverControl* m_screensaver   { nullptr };
    QTimer             m_idleTimer;
    std::chrono::minutes m_idleTime    { 0min };
};

MUI_PUBLIC MythMainWindow* GetMythMainWindow();
MUI_PUBLIC bool HasMythMainWindow();
MUI_PUBLIC void DestroyMythMainWindow();
MUI_PUBLIC MythPainter* GetMythPainter();
MUI_PUBLIC MythNotificationCenter* GetNotificationCenter();

static inline void
REG_KEY(const QString& Context, const QString& Action,
        const QString& Description, const QString& Key)
{
    GetMythMainWindow()->RegisterKey(Context, Action, Description, Key);
}

static inline QString
GET_KEY(const QString& Context, const QString& Action)
{
    return MythMainWindow::GetKey(Context, Action);
}

static inline void
REG_JUMP(const QString& Destination, const QString& Description,
         const QString& Key, void (*Callback)(void))
{
    GetMythMainWindow()->RegisterJump(Destination, Description, Key, Callback);
}

static inline void
REG_JUMPLOC(const QString& Destination, const QString& Description,
            const QString& Key, void (*Callback)(void), const QString& LocalAction)
{
    GetMythMainWindow()->RegisterJump(Destination, Description, Key, Callback, true, LocalAction);
}

static inline void
REG_JUMPEX(const QString& Destination, const QString& Description,
            const QString& Key, void (*Callback)(void), bool ExitToMain)
{
    GetMythMainWindow()->RegisterJump(Destination, Description, Key, Callback, ExitToMain);
}

static inline void
REG_MEDIAPLAYER(const QString& Name, const QString& Desc, MediaPlayCallback Func)
{
    GetMythMainWindow()->RegisterMediaPlugin(Name, Desc, Func);
}

#endif
