#ifndef MYTHDIALOGS_H_
#define MYTHDIALOGS_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qevent.h>
#include <qfont.h>
#include <qgl.h>

#include "mythuitype.h"
#include "mythscreenstack.h"

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

typedef  int (*MediaPlayCallback)(const char*);

class MythMainWindowPrivate;

class MythMainWindow : public QGLWidget
{
    Q_OBJECT
  public:
    virtual ~MythMainWindow();

    void Init(void);
    void Show(void);

    void AddScreenStack(MythScreenStack *stack, bool main = false);
    MythScreenStack *GetMainStack();

    bool TranslateKeyPress(const QString &context, QKeyEvent *e, 
                           QStringList &actions);

    void RegisterKey(const QString &context, const QString &action,
                     const QString &description, const QString &key);
    void RegisterJump(const QString &destination, const QString &description,
                      const QString &key, void (*callback)(void));
/*
    void RegisterMediaHandler(const QString &destination, 
                              const QString &description, const QString &key, 
                              void (*callback)(MythMediaDevice* mediadevice), 
                              int mediaType);

    void RegisterMediaPlugin(const QString &name, const QString &desc, 
                             MediaPlayCallback fn);
*/
    bool HandleMedia(QString &handler, const QString &mrl);

    void JumpTo(const QString &destination);
    bool DestinationExists(const QString &destination) const;

    static MythMainWindow *getMainWindow();

    MythPainter *GetCurrentPainter();

    void AllowInput(bool allow);

    QRect GetUIScreenRect();

    QFont CreateFont(const QString &face, int pointSize = 12,
                     int weight = QFont::Normal, bool italic = FALSE);
    QRect NormRect(const QRect &rect);
    QPoint NormPoint(const QPoint &point);
    int NormX(const int x);
    int NormY(const int y);
 
  public slots:
    void drawTimeout();

  protected:
    MythMainWindow();

    void keyPressEvent(QKeyEvent *e);
    void customEvent(QCustomEvent *ce);
    void closeEvent(QCloseEvent *e);
    void paintEvent(QPaintEvent *e);
    
    void ExitToMainMenu();

    QObject *getTarget(QKeyEvent &key);

    MythMainWindowPrivate *d;
};

MythMainWindow *GetMythMainWindow();
MythPainter *GetMythPainter();

QFont CreateFont(const QString &face, int pointSize = 12, 
                 int weight = QFont::Normal, bool italic = FALSE);

#endif

