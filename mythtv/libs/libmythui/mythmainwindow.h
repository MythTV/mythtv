#ifndef MYTHMAINWINDOW_H_
#define MYTHMAINWINDOW_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qevent.h>
#include <qfont.h>
#ifdef USE_OPENGL_PAINTER
#include <qgl.h>
#endif

#include "mythuitype.h"
#include "mythscreenstack.h"

class MythMediaDevice;

const int kExternalKeycodeEventType = 33213;
const int kExitToMainMenuEventType = 33214;

class ExternalKeycodeEvent : public QCustomEvent
{
  public:
    ExternalKeycodeEvent(const int key) 
           : QCustomEvent(kExternalKeycodeEventType), keycode(key) {}

    int getKeycode() { return keycode; }

  private:
    int keycode;
};

class ExitToMainMenuEvent : public QCustomEvent
{
  public:
    ExitToMainMenuEvent(void) : QCustomEvent(kExitToMainMenuEventType) {}
};

#define REG_KEY(a, b, c, d) GetMythMainWindow()->RegisterKey(a, b, c, d)
#define GET_KEY(a, b) GetMythMainWindow()->GetKey(a, b)
#define REG_JUMP(a, b, c, d) GetMythMainWindow()->RegisterJump(a, b, c, d)
#define REG_MEDIA_HANDLER(a, b, c, d, e, f) GetMythMainWindow()->RegisterMediaHandler(a, b, c, d, e, f)
#define REG_MEDIAPLAYER(a,b,c) GetMythMainWindow()->RegisterMediaPlugin(a, b, c)

typedef  int (*MediaPlayCallback)(const char*,  const char*, const char*, const char*, int, const char*);

class MythMainWindowPrivate;

// Cheat moc
#ifdef USE_OPENGL_PAINTER
#define QWidget QGLWidget
#endif

class MythMainWindow : public QWidget
{
    Q_OBJECT
#undef QWidget
  public:
    void Init(void);
    void Show(void);

    void AddScreenStack(MythScreenStack *stack, bool main = false);
    MythScreenStack *GetMainStack();

    bool TranslateKeyPress(const QString &context, QKeyEvent *e, 
                           QStringList &actions, bool allowJumps = true);

    void ClearKey(const QString &context, const QString &action);
    void BindKey(const QString &context, const QString &action,
                 const QString &key);
    void RegisterKey(const QString &context, const QString &action,
                     const QString &description, const QString &key);
    QString GetKey(const QString &context, const QString &action) const;

    void ClearJump(const QString &destination);
    void BindJump(const QString &destination, const QString &key);
    void RegisterJump(const QString &destination, const QString &description,
                      const QString &key, void (*callback)(void));
    void RegisterMediaHandler(const QString &destination,
                              const QString &description, const QString &key,
                              void (*callback)(MythMediaDevice* mediadevice),
                              int mediaType, const QString &extensions);

    void RegisterMediaPlugin(const QString &name, const QString &desc,
                             MediaPlayCallback fn);

    bool HandleMedia(QString& handler, const QString& mrl, 
                     const QString& plot="", const QString& title="", 
                     const QString& director="", int lenMins=120,
                     const QString& year="1895");

    void JumpTo(const QString &destination, bool pop = true);
    bool DestinationExists(const QString &destination) const;

    bool IsExitingToMain(void) const;

    static MythMainWindow *getMainWindow();
    static void destroyMainWindow();

    MythPainter *GetCurrentPainter();

    void AllowInput(bool allow);

    QRect GetUIScreenRect();

    int NormalizeFontSize(int pointSize);
    QFont CreateFont(const QString &face, int pointSize = 12,
                     int weight = QFont::Normal, bool italic = FALSE);
    QRect NormRect(const QRect &rect);
    QPoint NormPoint(const QPoint &point);
    QSize NormSize(const QSize &size);
    int NormX(const int x);
    int NormY(const int y);
 
    /* compatability functions, to go away once everything's mythui */
    void attach(QWidget *child);
    void detach(QWidget *child);

    QWidget *currentWidget(void);

  public slots:
    void drawTimeout();
    void mouseTimeout();

  protected:
    MythMainWindow();
    virtual ~MythMainWindow();

    bool eventFilter(QObject *o, QEvent *e);
    void customEvent(QCustomEvent *ce);
    void closeEvent(QCloseEvent *e);
    void paintEvent(QPaintEvent *e);
    
    void ExitToMainMenu();

    QObject *getTarget(QKeyEvent &key);

    MythMainWindowPrivate *d;
};

MythMainWindow *GetMythMainWindow();
void DestroyMythMainWindow();

MythPainter *GetMythPainter();

#endif

