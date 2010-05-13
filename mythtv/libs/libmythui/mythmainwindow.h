#ifndef MYTHMAINWINDOW_H_
#define MYTHMAINWINDOW_H_

#include <QWidget>

#include "mythuitype.h"
#include "mythscreenstack.h"

class QEvent;

class MythMediaDevice;

#define REG_KEY(a, b, c, d) GetMythMainWindow()->RegisterKey(a, b, c, d)
#define GET_KEY(a, b) GetMythMainWindow()->GetKey(a, b)
#define REG_JUMP(a, b, c, d) GetMythMainWindow()->RegisterJump(a, b, c, d)
#define REG_JUMPLOC(a, b, c, d, e) GetMythMainWindow()->RegisterJump(a, b, c, d, true, e)
#define REG_JUMPEX(a, b, c, d, e) GetMythMainWindow()->RegisterJump(a, b, c, d, e)
#define REG_MEDIAPLAYER(a,b,c) GetMythMainWindow()->RegisterMediaPlugin(a, b, c)

typedef int (*MediaPlayCallback)(const QString &, const QString &, const QString &, const QString &, const QString &, int, int, int, const QString &);

class MythMainWindowPrivate;

class MythPainterWindowGL;
class MythPainterWindowQt;
class MythPainterWindowVDPAU;

class MPUBLIC MythMainWindow : public QWidget
{
    Q_OBJECT
    friend class MythPainterWindowGL;
    friend class MythPainterWindowQt;
    friend class MythPainterWindowVDPAU;

  public:
    void Init(void);
    void ReinitDone(void);
    void Show(void);

    void AddScreenStack(MythScreenStack *stack, bool main = false);
    void PopScreenStack();
    MythScreenStack *GetMainStack();
    MythScreenStack *GetStack(const QString &stackname);

    bool TranslateKeyPress(const QString &context, QKeyEvent *e,
                           QStringList &actions, bool allowJumps = true) 
                           __attribute__ ((warn_unused_result));

    void ClearKey(const QString &context, const QString &action);
    void BindKey(const QString &context, const QString &action,
                 const QString &key);
    void RegisterKey(const QString &context, const QString &action,
                     const QString &description, const QString &key);
    QString GetKey(const QString &context, const QString &action) const;

    void ClearJump(const QString &destination);
    void BindJump(const QString &destination, const QString &key);
    void RegisterJump(const QString &destination, const QString &description,
                      const QString &key, void (*callback)(void),
                      bool exittomain = true, QString localAction = "");

    void RegisterMediaPlugin(const QString &name, const QString &desc,
                             MediaPlayCallback fn);

    void RegisterSystemEventHandler(QObject *eventHandler);
    QObject *GetSystemEventHandler(void);

    bool HandleMedia(const QString& handler, const QString& mrl,
                     const QString& plot="", const QString& title="",
                     const QString& subtitle="", const QString& director="", 
                     int season=0, int episode=0, int lenMins=120, 
                     const QString& year="1895");

    void JumpTo(const QString &destination, bool pop = true);
    bool DestinationExists(const QString &destination) const;

    bool IsExitingToMain(void) const;

    static MythMainWindow *getMainWindow(const bool useDB = true);
    static void destroyMainWindow();

    MythPainter *GetCurrentPainter();
    QWidget     *GetPaintWindow();

    bool screenShot(QString fname, int x, int y, int x2, int y2, int w, int h);
    bool screenShot(int x, int y, int x2, int y2);
    bool screenShot(QString fname, int w, int h);
    bool screenShot(void);

    void AllowInput(bool allow);

    QRect GetUIScreenRect();

    int NormalizeFontSize(int pointSize);
    MythRect NormRect(const MythRect &rect);
    QPoint NormPoint(const QPoint &point);
    QSize NormSize(const QSize &size);
    int NormX(const int x);
    int NormY(const int y);
    int fonTweak;

    void StartLIRC(void);

    /* compatibility functions, to go away once everything's mythui */
    void attach(QWidget *child);
    void detach(QWidget *child);

    QWidget *currentWidget(void);

    void SetDrawEnabled(bool enable);
    void SetEffectsEnabled(bool enable);

  public slots:
    void mouseTimeout();

  protected slots:
    void animate();

  protected:
    MythMainWindow(const bool useDB = true);
    virtual ~MythMainWindow();

    bool eventFilter(QObject *o, QEvent *e);
    void customEvent(QEvent *ce);
    void closeEvent(QCloseEvent *e);

    void drawScreen();

    bool event(QEvent* e);

    void ExitToMainMenu();

    QObject *getTarget(QKeyEvent &key);

    MythMainWindowPrivate *d;
};

MPUBLIC MythMainWindow *GetMythMainWindow();
MPUBLIC bool HasMythMainWindow();
MPUBLIC void DestroyMythMainWindow();

MPUBLIC MythPainter *GetMythPainter();

#endif

