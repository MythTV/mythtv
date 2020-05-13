#ifndef MYTHMAINWINDOW_H_
#define MYTHMAINWINDOW_H_

// Qt
#include <QWidget>

// MythTV
#include "mythscreenstack.h"
#include "mythnotificationcenter.h"
#include "mythuiactions.h"
#include "mythrect.h"

class QEvent;

class MythMediaDevice;

#define REG_KEY(a, b, c, d) GetMythMainWindow()->RegisterKey(a, b, c, d)
#define GET_KEY(a, b) GetMythMainWindow()->GetKey(a, b)
#define REG_JUMP(a, b, c, d) GetMythMainWindow()->RegisterJump(a, b, c, d)
#define REG_JUMPLOC(a, b, c, d, e) GetMythMainWindow()->RegisterJump(a, b, c, d, true, e)
#define REG_JUMPEX(a, b, c, d, e) GetMythMainWindow()->RegisterJump(a, b, c, d, e)
#define REG_MEDIAPLAYER(a,b,c) GetMythMainWindow()->RegisterMediaPlugin(a, b, c)

using MediaPlayCallback = int (*)(const QString &, const QString &, const QString &, const QString &, const QString &, int, int, const QString &, int, const QString &, const QString &, bool);

class MythDisplay;
class MythInputDeviceHandler;
class MythMainWindowPrivate;
class MythPainterWindow;
class MythRender;

class MUI_PUBLIC MythMainWindow : public QWidget
{
    Q_OBJECT
    friend class MythPainterWindowOpenGL;
    friend class MythPainterWindowVulkan;
    friend class MythPainterWindowQt;

  public:
    enum {drawRefresh = 70};

    void Init(bool mayReInit = true);
    void ReinitDone(void);
    void Show(void);
    void MoveResize(QRect &Geometry);
    static bool WindowIsAlwaysFullscreen(void);

    void AddScreenStack(MythScreenStack *stack, bool main = false);
    void PopScreenStack();
    int GetStackCount(void);
    MythScreenStack *GetMainStack();
    MythScreenStack *GetStack(const QString &stackname);
    MythScreenStack *GetStackAt(int pos);

    bool TranslateKeyPress(const QString &context, QKeyEvent *e,
                           QStringList &actions, bool allowJumps = true);
    bool keyLongPressFilter(QEvent **e,
        QScopedPointer<QEvent> &sNewEvent);

    void ReloadKeys(void);
    void ClearKey(const QString &context, const QString &action);
    void ClearKeyContext(const QString &context);
    void BindKey(const QString &context, const QString &action,
                 const QString &key);
    void RegisterKey(const QString &context, const QString &action,
                     const QString &description, const QString &key);
    static QString GetKey(const QString &context, const QString &action);
    QObject *getTarget(QKeyEvent &key);
    QString GetActionText(const QString &context, const QString &action) const;

    void ClearJump(const QString &destination);
    void BindJump(const QString &destination, const QString &key);
    void RegisterJump(const QString &destination, const QString &description,
                      const QString &key, void (*callback)(void),
                      bool exittomain = true, QString localAction = "");
    void ClearAllJumps();

    void RegisterMediaPlugin(const QString &name, const QString &desc,
                             MediaPlayCallback fn);

    bool HandleMedia(const QString& handler, const QString& mrl,
                     const QString& plot="", const QString& title="",
                     const QString& subtitle="", const QString& director="",
                     int season=0, int episode=0, const QString& inetref="",
                     int lenMins=120, const QString& year="1895",
                     const QString &id="", bool useBookmarks = false);
    void HandleTVAction(const QString &Action);

    void JumpTo(const QString &destination, bool pop = true);
    bool DestinationExists(const QString &destination) const;
    QStringList EnumerateDestinations(void) const;

    bool IsExitingToMain(void) const;

    static MythMainWindow *getMainWindow(bool useDB = true);
    static void destroyMainWindow();

    MythPainter *GetCurrentPainter();
    QWidget     *GetPaintWindow();
    MythRender  *GetRenderDevice();
    MythNotificationCenter *GetCurrentNotificationCenter();
    void         ShowPainterWindow();
    void         HidePainterWindow();

    static void GrabWindow(QImage &image);
    static bool SaveScreenShot(const QImage &image, QString filename = "");
    static bool ScreenShot(int w = 0, int h = 0, QString filename = "");
    void RemoteScreenShot(QString filename, int x, int y);

    void AllowInput(bool allow);

    QRect GetUIScreenRect();
    void  SetUIScreenRect(QRect &rect);

    int GetDrawInterval() const;
    int NormalizeFontSize(int pointSize);
    MythRect NormRect(const MythRect &rect);
    QPoint NormPoint(const QPoint &point);
    QSize NormSize(const QSize &size);
    int NormX(int x);
    int NormY(int y);
    void SetScalingFactors(float wmult, float hmult);
    void RestartInputHandlers(void);
    uint PushDrawDisabled(void);
    uint PopDrawDisabled(void);
    void SetEffectsEnabled(bool enable);
    void Draw(MythPainter *Painter = nullptr);

    void ResetIdleTimer(void);
    void PauseIdleTimer(bool pause);
    void DisableIdleTimer(bool disableIdle = true);
    void EnterStandby(bool manual = true);
    void ExitStandby(bool manual = true);

    QPaintEngine *paintEngine() const override; // QWidget

  public slots:
    void mouseTimeout();
    void HideMouseTimeout();
    void IdleTimeout();

  protected slots:
    void animate();
    void doRemoteScreenShot(const QString& filename, int x, int y);
    void SetDrawEnabled(bool enable);
    void onApplicationStateChange(Qt::ApplicationState state);

  signals:
    void signalRemoteScreenShot(QString filename, int x, int y);
    void signalSetDrawEnabled(bool enable);
    void signalWindowReady(void);

  protected:
    explicit MythMainWindow(bool useDB = true);
    ~MythMainWindow() override;

    void InitKeys(void);

    bool eventFilter(QObject *o, QEvent *e) override; // QWidget
    void customEvent(QEvent *ce) override; // QWidget
    void closeEvent(QCloseEvent *e) override; // QWidget
    void drawScreen(QPaintEvent* Event = nullptr);
    bool event(QEvent* e) override; // QWidget
    void ExitToMainMenu();
    void ShowMouseCursor(bool show);

    MythMainWindowPrivate *d {nullptr}; // NOLINT(readability-identifier-naming)

  private slots:
    void DelayedAction(void);

  private:
    MythDisplay*       m_display       { nullptr };
    QRegion            m_repaintRegion;
    MythPainter*       m_painter       { nullptr };
    MythPainter*       m_oldPainter    { nullptr };
    MythPainterWindow* m_painterWin    { nullptr };
    MythPainterWindow* m_oldPainterWin { nullptr };
    MythInputDeviceHandler* m_deviceHandler { nullptr };
};

MUI_PUBLIC MythMainWindow *GetMythMainWindow();
MUI_PUBLIC bool HasMythMainWindow();
MUI_PUBLIC void DestroyMythMainWindow();

MUI_PUBLIC MythPainter *GetMythPainter();

MUI_PUBLIC MythNotificationCenter *GetNotificationCenter();

#endif
