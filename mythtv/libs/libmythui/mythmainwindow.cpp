
#include <math.h>
#include <pthread.h>

#include <algorithm>
#include <vector>

#ifdef USE_OPENGL_PAINTER
#include <QGLWidget>
#endif

#include <QApplication>
#include <QTimer>
#include <QDesktopWidget>
#include <QHash>
#include <QFile>
#include <QDir>

#ifdef QWS
#include <qwindowsystem_qws.h>
#endif
#ifdef Q_WS_MACX
#include <HIToolbox/Menus.h>   // For GetMBarHeight()
#endif

using namespace std;

#ifdef USE_LIRC
#include "lirc.h"
#include "lircevent.h"
#endif

#ifdef USING_APPLEREMOTE
#include "AppleRemoteListener.h"
#include "lircevent.h"
#endif

#ifdef USE_JOYSTICK_MENU
#include "jsmenu.h"
#include "jsmenuevent.h"
#endif

#include "mythmainwindow.h"
#include "mythmainwindow_internal.h"
#include "mythscreentype.h"
#include "mythpainter.h"
#ifdef USE_OPENGL_PAINTER
#include "mythpainter_ogl.h"
#endif
#include "mythpainter_qt.h"
#include "mythgesture.h"
#include "mythuihelper.h"

/* from libmythdb */
#include "mythdb.h"
#include "mythverbose.h"
#include "mythevent.h"
#include "mythdirs.h"

/* from libmyth */
#include "screensaver.h"

#define GESTURE_TIMEOUT 1000


class KeyContext
{
  public:
    void AddMapping(int key, QString action)
    {
        actionMap[key].append(action);
    }

    bool GetMapping(int key, QStringList &actions)
    {
        if (actionMap.count(key) > 0)
        {
            actions += actionMap[key];
            return true;
        }
        return false;
    }

    QMap<int, QStringList> actionMap;
};

struct JumpData
{
    void (*callback)(void);
    QString destination;
    QString description;
    bool exittomain;
    QString localAction;
};

struct MPData {
    QString description;
    MediaPlayCallback playFn;
};

class MythMainWindowPrivate
{
  public:
    int TranslateKeyNum(QKeyEvent *e);

    float wmult, hmult;
    int screenwidth, screenheight;

    QRect screenRect;
    QRect uiScreenRect;

    int xbase, ybase;
    bool does_fill_screen;

    bool ignore_lirc_keys;
    bool ignore_joystick_keys;

#ifdef USE_LIRC
    LircThread *lircThread;
#endif

#ifdef USE_JOYSTICK_MENU
    JoystickMenuThread *joystickThread;
#endif

#ifdef USING_APPLEREMOTE
    AppleRemoteListener *appleRemoteListener;
    AppleRemote         *appleRemote;
#endif

    bool exitingtomain;
    bool popwindows;

    bool m_useDB;              ///< To allow or prevent database access

    QHash<QString, KeyContext *> keyContexts;
    QMap<int, JumpData*> jumpMap;
    QMap<QString, JumpData> destinationMap;
    QMap<QString, MPData> mediaPluginMap;

    void (*exitmenucallback)(void);

    void (*exitmenumediadevicecallback)(MythMediaDevice* mediadevice);
    MythMediaDevice * mediadeviceforcallback;

    int escapekey;

    QTimer *drawTimer;
    QVector<MythScreenStack *> stackList;
    MythScreenStack *mainStack;

    MythPainter *painter;

    bool AllowInput;

    QRegion repaintRegion;

    MythGesture gesture;
    QTimer *gestureTimer;

    /* compatability only, FIXME remove */
    vector<QWidget *> widgetList;

    bool bUseGL;
    QWidget *paintwin;
};

// Make keynum in QKeyEvent be equivalent to what's in QKeySequence
int MythMainWindowPrivate::TranslateKeyNum(QKeyEvent* e)
{
    int keynum = e->key();

    if (keynum != Qt::Key_Escape &&
        (keynum <  Qt::Key_Shift || keynum > Qt::Key_ScrollLock))
    {
        Qt::KeyboardModifiers modifiers;
        // if modifiers have been pressed, rebuild keynum
        if ((modifiers = e->modifiers()) != Qt::NoModifier)
        {
            int modnum = (((modifiers & Qt::ShiftModifier) &&
                           keynum > 0x7f) ? Qt::SHIFT : 0) |
                         ((modifiers & Qt::ControlModifier) ? Qt::CTRL : 0) |
                         ((modifiers & Qt::MetaModifier) ? Qt::META : 0) |
                         ((modifiers & Qt::AltModifier) ? Qt::ALT : 0);
            modnum &= ~Qt::UNICODE_ACCEL;
            return (keynum |= modnum);
        }
    }

    return keynum;
}

static MythMainWindow *mainWin = NULL;
static QMutex mainLock;

/**
 * \brief Return the existing main window, or create one
 * \param useDB
 *        If this is a temporary window, which is used to bootstrap
 *        the database, passing false prevents any database access.
 *
 * \sa    MythContextPrivate::TempMainWindow()
 */
MythMainWindow *MythMainWindow::getMainWindow(const bool useDB)
{
    if (mainWin)
        return mainWin;

    QMutexLocker lock(&mainLock);

    if (!mainWin)
        mainWin = new MythMainWindow(useDB);

    return mainWin;
}

void MythMainWindow::destroyMainWindow(void)
{
    if (mainWin)
        delete mainWin;
    mainWin = NULL;
}

MythMainWindow *GetMythMainWindow(void)
{
    return MythMainWindow::getMainWindow();
}

bool HasMythMainWindow(void)
{
    return (mainWin);
}

void DestroyMythMainWindow(void)
{
    MythMainWindow::destroyMainWindow();
}

MythPainter *GetMythPainter(void)
{
    return MythMainWindow::getMainWindow()->GetCurrentPainter();
}

#ifdef USE_OPENGL_PAINTER
MythPainterWindowGL::MythPainterWindowGL(MythMainWindow *win,
                                         MythMainWindowPrivate *priv)
                   : QGLWidget(win),
                     parent(win), d(priv)
{
    setAutoBufferSwap(false);
}

void MythPainterWindowGL::paintEvent(QPaintEvent *pe)
{
    d->repaintRegion = d->repaintRegion.unite(pe->region());
    parent->drawScreen();
}

#endif

MythPainterWindowQt::MythPainterWindowQt(MythMainWindow *win,
                                         MythMainWindowPrivate *priv)
                   : QWidget(win),
                     parent(win), d(priv)
{
}

void MythPainterWindowQt::paintEvent(QPaintEvent *pe)
{
    d->repaintRegion = d->repaintRegion.unite(pe->region());
    parent->drawScreen();
}

MythMainWindow::MythMainWindow(const bool useDB)
              : QWidget(NULL)
{
    d = new MythMainWindowPrivate;

    setObjectName("mainwindow");

    d->AllowInput = false;

    // This prevents database errors from RegisterKey() when there is no DB:
    d->m_useDB = useDB;

    QString painter = GetMythDB()->GetSetting("ThemePainter", "qt");
#ifdef USE_OPENGL_PAINTER
    if (painter == "opengl")
    {
        VERBOSE(VB_GENERAL, "Using the OpenGL painter");
        d->painter = new MythOpenGLPainter();
        d->bUseGL = true;
    }
    else
#endif
    {
        VERBOSE(VB_GENERAL, "Using the Qt painter");
        d->painter = new MythQtPainter();
        d->bUseGL = false;
    }

    //Init();

    d->ignore_lirc_keys = false;
    d->ignore_joystick_keys = false;
    d->exitingtomain = false;
    d->popwindows = true;
    d->exitmenucallback = false;
    d->exitmenumediadevicecallback = false;
    d->mediadeviceforcallback = NULL;
    d->escapekey = Qt::Key_Escape;
    d->mainStack = NULL;

    installEventFilter(this);

#ifdef USE_LIRC
    QString config_file = GetConfDir() + "/lircrc";
    if (!QFile::exists(config_file))
        config_file = QDir::homePath() + "/.lircrc";

    d->lircThread = NULL;
    d->lircThread = new LircThread(this);
    if (!d->lircThread->Init(config_file, "mythtv"))
        d->lircThread->start();
#endif

#ifdef USE_JOYSTICK_MENU
    d->ignore_joystick_keys = false;

    QString joy_config_file = GetConfDir() + "/joystickmenurc";

    d->joystickThread = NULL;
    d->joystickThread = new JoystickMenuThread(this);
    if (!d->joystickThread->Init(joy_config_file))
        d->joystickThread->start();
#endif

#ifdef USING_APPLEREMOTE
    d->appleRemoteListener = new AppleRemoteListener(this);
    d->appleRemote         = AppleRemote::Get();

    d->appleRemote->setListener(d->appleRemoteListener);
    d->appleRemote->startListening();
    if (d->appleRemote->isListeningToRemote())
        d->appleRemote->start();
#endif

    RegisterKey("Global", "UP", "Up Arrow", "Up");
    RegisterKey("Global", "DOWN", "Down Arrow", "Down");
    RegisterKey("Global", "LEFT", "Left Arrow", "Left");
    RegisterKey("Global", "RIGHT", "Right Arrow", "Right");
    RegisterKey("Global", "SELECT", "Select", "Return,Enter,Space");
    RegisterKey("Global", "BACKSPACE", "Backspace", "Backspace");
    RegisterKey("Global", "ESCAPE", "Escape", "Esc");
    RegisterKey("Global", "MENU", "Pop-up menu", "M");
    RegisterKey("Global", "INFO", "More information", "I");

    RegisterKey("Global", "PAGEUP", "Page Up", "PgUp");
    RegisterKey("Global", "PAGEDOWN", "Page Down", "PgDown");

    RegisterKey("Global", "PREVVIEW", "Previous View", "Home");
    RegisterKey("Global", "NEXTVIEW", "Next View", "End");

    RegisterKey("Global", "HELP", "Help", "F1");
    RegisterKey("Global", "EJECT", "Eject Removable Media", "");

    RegisterKey("Global", "0", "0", "0");
    RegisterKey("Global", "1", "1", "1");
    RegisterKey("Global", "2", "2", "2");
    RegisterKey("Global", "3", "3", "3");
    RegisterKey("Global", "4", "4", "4");
    RegisterKey("Global", "5", "5", "5");
    RegisterKey("Global", "6", "6", "6");
    RegisterKey("Global", "7", "7", "7");
    RegisterKey("Global", "8", "8", "8");
    RegisterKey("Global", "9", "9", "9");

    // these are for the html viewer widget (MythUIWebBrowser)
    RegisterKey("Browser", "ZOOMIN",      "Zoom in on browser window",           ".,>");
    RegisterKey("Browser", "ZOOMOUT",     "Zoom out on browser window",          ",,<");
    RegisterKey("Browser", "TOGGLEINPUT", "Toggle where keyboard input goes to", "F1");

    RegisterKey("Browser", "MOUSEUP",         "Move mouse pointer up",   "2");
    RegisterKey("Browser", "MOUSEDOWN",       "Move mouse pointer down", "8");
    RegisterKey("Browser", "MOUSELEFT",       "Move mouse pointer left", "4");
    RegisterKey("Browser", "MOUSERIGHT",      "Move mouse pointer right","6");
    RegisterKey("Browser", "MOUSELEFTBUTTON", "Mouse Left button click", "5");

    RegisterKey("Browser", "PAGEDOWN",       "Scroll down half a page",  "9");
    RegisterKey("Browser", "PAGEUP",         "Scroll up half a page",    "3");
    RegisterKey("Browser", "PAGELEFT",       "Scroll left half a page",  "7");
    RegisterKey("Browser", "PAGERIGHT",      "Scroll right half a page", "1");

    RegisterKey("Browser", "NEXTLINK",       "Move selection to next link",     "Z");
    RegisterKey("Browser", "PREVIOUSLINK",   "Move selection to previous link", "Q");
    RegisterKey("Browser", "FOLLOWLINK",     "Follow selected link",            "Return,Space,Enter");
    RegisterKey("Browser", "HISTORYBACK",    "Go back to previous page",        "R,Backspace");
    RegisterKey("Browser", "HISTORYFORWARD", "Go forward to previous page",     "F");

    d->gestureTimer = new QTimer(this);
    connect(d->gestureTimer, SIGNAL(timeout()), this, SLOT(mouseTimeout()));

    d->drawTimer = new QTimer(this);
    connect(d->drawTimer, SIGNAL(timeout()), this, SLOT(animate()));
    d->drawTimer->start(1000 / 70);

    d->AllowInput = true;

    d->repaintRegion = QRegion(QRect(0,0,0,0));
}

MythMainWindow::~MythMainWindow()
{
    while (!d->stackList.isEmpty())
    {
        delete d->stackList.back();
        d->stackList.pop_back();
    }

    while (!d->keyContexts.isEmpty()) {
        KeyContext *context = *d->keyContexts.begin();
        d->keyContexts.erase(d->keyContexts.begin());
        delete context;
    }

#ifdef USE_LIRC
    if (d->lircThread)
    {
        if (d->lircThread->isRunning())
        {
            d->lircThread->Stop();
            d->lircThread->wait();
        }

        delete d->lircThread;
        d->lircThread = NULL;
    }
#endif

#ifdef USE_JOYSTICK_MENU
    if (d->joystickThread)
    {
        if (d->joystickThread->isRunning())
        {
            d->joystickThread->Stop();
            d->joystickThread->wait();
        }

        delete d->joystickThread;
        d->joystickThread = NULL;
    }
#endif

#ifdef USING_APPLEREMOTE
    // We don't delete this, just disable its plumbing. If we create another
    // MythMainWindow later, AppleRemote::get() will retrieve the instance.
    if (d->appleRemote->isRunning())
        d->appleRemote->stopListening();

    delete d->appleRemoteListener;
#endif

    delete d;
}

MythPainter *MythMainWindow::GetCurrentPainter(void)
{
    return d->painter;
}

QWidget *MythMainWindow::GetPaintWindow(void)
{
    return d->paintwin;
}

void MythMainWindow::AddScreenStack(MythScreenStack *stack, bool main)
{
    d->stackList.push_back(stack);
    if (main)
        d->mainStack = stack;
}

void MythMainWindow::PopScreenStack()
{
    delete d->stackList.back();
    d->stackList.pop_back();
}

MythScreenStack *MythMainWindow::GetMainStack(void)
{
    return d->mainStack;
}

MythScreenStack *MythMainWindow::GetStack(const QString &stackname)
{
    QVector<MythScreenStack *>::Iterator it;
    for (it = d->stackList.begin(); it != d->stackList.end(); ++it)
    {
        if ((*it)->objectName() == stackname)
            return *it;
    }
    return NULL;
}

void MythMainWindow::animate(void)
{
    /* FIXME: remove */
    if (currentWidget())
        return;

    bool redraw = false;

    if (!d->repaintRegion.isEmpty())
        redraw = true;

    QVector<MythScreenStack *>::Iterator it;
    for (it = d->stackList.begin(); it != d->stackList.end(); ++it)
    {
        QVector<MythScreenType *> drawList;
        (*it)->GetDrawOrder(drawList);

        QVector<MythScreenType *>::Iterator screenit;
        for (screenit = drawList.begin(); screenit != drawList.end();
             ++screenit)
        {
            (*screenit)->Pulse();

            if ((*screenit)->NeedsRedraw())
            {
                QRegion topDirty = (*screenit)->GetDirtyArea();
                d->repaintRegion = d->repaintRegion.unite(topDirty);
                redraw = true;
            }
        }
    }

    if (!redraw)
    {
        return;
    }

    d->paintwin->update(d->repaintRegion);
}

void MythMainWindow::drawScreen(void)
{
    if (!d->painter->SupportsClipping())
        d->repaintRegion = d->repaintRegion.unite(d->uiScreenRect);
    else
    {
        // Ensure that the region is not larger than the screen which
        // can happen with bad themes
        d->repaintRegion = d->repaintRegion.intersect(d->uiScreenRect);
    }

    d->painter->Begin(d->paintwin);

    QVector<QRect> rects = d->repaintRegion.rects();

    for (int i = 0; i < rects.size(); i++)
    {
        if (rects[i].width() == 0 || rects[i].height() == 0)
            continue;

        if (rects[i] != d->uiScreenRect)
            d->painter->SetClipRect(rects[i]);

        QVector<MythScreenStack *>::Iterator it;
        for (it = d->stackList.begin(); it != d->stackList.end(); ++it)
        {
            QVector<MythScreenType *> redrawList;
            (*it)->GetDrawOrder(redrawList);

            QVector<MythScreenType *>::Iterator screenit;
            for (screenit = redrawList.begin(); screenit != redrawList.end();
                 ++screenit)
            {
                (*screenit)->Draw(d->painter, 0, 0, 255, rects[i]);
            }
        }
    }

    d->painter->End();

    d->repaintRegion = QRegion(QRect(0, 0, 0, 0));
}

void MythMainWindow::closeEvent(QCloseEvent *e)
{
    (void)e;
    qApp->quit();
}

bool MythMainWindow::screenShot(QString fname, int x, int y,
                                int x2, int y2, int w, int h)
{
    bool ret = false;

    QString extension = fname.section('.', -1, -1);
    if (extension == "jpg")
        extension = "JPEG";
    else
        extension = "PNG";

    VERBOSE(VB_GENERAL, "MythMainWindow::screenShot saving winId " +
                        QString("%1 to %2 (%3 x %4) [ %5/%6 - %7/%8] type %9")
                        .arg((long)QApplication::desktop()->winId())
                        .arg(fname)
                        .arg(w).arg(h)
                        .arg(x).arg(y)
                        .arg(x2).arg(y2)
                        .arg(extension));

    QPixmap p;
    p = QPixmap::grabWindow( QApplication::desktop()->winId(), x, y, x2, y2);

    QImage img = p.toImage();

    if ( w == 0 )
        w = img.width();

    if ( h == 0 )
        h = img.height();

    VERBOSE(VB_GENERAL, QString("Scaling to %1 x %2 from %3 x %4")
                        .arg(w)
                        .arg(h)
                        .arg(img.width())
                        .arg(img.height()));

    img = img.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    if (img.save(fname, extension.toAscii(), 100))
    {
        VERBOSE(VB_GENERAL, "MythMainWindow::screenShot succeeded");
        ret = true;
    }
    else
    {
        VERBOSE(VB_GENERAL, "MythMainWindow::screenShot Failed!");
        ret = false;
    }

    return ret;
}

bool MythMainWindow::screenShot(int x, int y, int x2, int y2)
{
    QString fPath = GetMythDB()->GetSetting("ScreenShotPath","/tmp/");
    QString fName = QString("/%1/myth-screenshot-%2.png")
                    .arg(fPath)
                    .arg(QDateTime::currentDateTime()
                         .toString("yyyy-mm-ddThh-mm-ss.zzz"));

    return screenShot(fName, x, y, x2, y2, 0, 0);
}

bool MythMainWindow::screenShot(QString fname, int w, int h)
{
    QWidget *active = QApplication::activeWindow();
    if (active)
    {
        QRect sLoc = active->geometry();
        return screenShot(fname, sLoc.left(),sLoc.top(),
                          sLoc.width(), sLoc.height(), w, h);
    }
    return false;
}


bool MythMainWindow::screenShot(void)
{
    QWidget *active = QApplication::activeWindow();
    if (active)
    {
        QRect sLoc = active->geometry();
        return screenShot(sLoc.left(),sLoc.top(), sLoc.width(), sLoc.height());
    }
    return false;
}

#ifdef USING_APPLEREMOTE
bool MythMainWindow::event(QEvent* e)
{
    if (d->appleRemote)
    {
        if (e->type() == QEvent::WindowActivate)
            d->appleRemote->startListening();

        if (e->type() == QEvent::WindowDeactivate)
            d->appleRemote->stopListening();
    }

    return QWidget::event(e);
}
#endif

void MythMainWindow::Init(void)
{
    fonTweak = GetMythDB()->GetNumSetting("QtFonTweak", 0);

    bool hideCursor = GetMythDB()->GetNumSetting("HideMouseCursor", 1);
#ifdef QWS
    QWSServer::setCursorVisible(!hideCursor);
#endif

    if (GetMythDB()->GetNumSetting("RunFrontendInWindow", 0))
        d->does_fill_screen = false;
    else
        d->does_fill_screen = true;

    // Set window border based on fullscreen attribute
    Qt::WindowFlags flags = Qt::Window;

    if (d->does_fill_screen)
    {
        if (GetMythUI()->IsGeometryOverridden())
            flags |= Qt::FramelessWindowHint;
        else
            setWindowState(Qt::WindowFullScreen);
    }

    // Workarounds for Qt/Mac bugs
#ifdef Q_WS_MACX
    if (d->does_fill_screen)
    {
        flags = Qt::SplashScreen;
    }
#endif

#ifdef WIN32
    flags |= Qt::MSWindowsOwnDC;
#endif

    setWindowFlags(flags);

    GetMythUI()->GetScreenSettings(d->xbase, d->screenwidth, d->wmult,
                                   d->ybase, d->screenheight, d->hmult);

    d->screenRect = QRect(d->xbase, d->ybase, d->screenwidth, d->screenheight);
    d->uiScreenRect = QRect(0, 0, d->screenwidth, d->screenheight);

    setGeometry(d->xbase, d->ybase, d->screenwidth, d->screenheight);
    setFixedSize(QSize(d->screenwidth, d->screenheight));

    /* FIXME these two lines should go away */
    //setFont(GetMythUI()->GetMediumFont());
    GetMythUI()->ThemeWidget(this);

    Show();

    // Set cursor call must come after Show() to work on some systems.
    setCursor((hideCursor) ? (Qt::BlankCursor) : (Qt::ArrowCursor));

    move(d->xbase, d->ybase);

    // allocate painter
#ifdef USE_OPENGL_PAINTER
    if (d->bUseGL)
    {
        d->paintwin = new MythPainterWindowGL(this, d);
    }
    else
#endif
    {
        d->paintwin = new MythPainterWindowQt(this, d);
    }

    d->paintwin->move(0, 0);
    d->paintwin->setFixedSize(size());
    d->paintwin->raise();
    d->paintwin->show();
}

void MythMainWindow::Show(void)
{
    show();
#ifdef Q_WS_MACX
    if (d->does_fill_screen)
        HideMenuBar();
    else
        ShowMenuBar();
#endif

    activateWindow();
    raise();

    //-=>TODO: The following method does not exist in Qt4
    //qApp->wakeUpGuiThread();    // ensures that setActiveWindow() occurs
}

/* FIXME compatability only */
void MythMainWindow::attach(QWidget *child)
{
#ifdef USING_MINGW
#warning TODO FIXME MythMainWindow::attach() does not always work on MS Windows!
    // if windows are created on different threads,
    // or if setFocus() is called from a thread other than the main UI thread,
    // setFocus() hangs the thread that called it
    // currently, it's impossible to switch to program guide from livetv
    VERBOSE(VB_IMPORTANT,
            QString("MythMainWindow::attach old: %1, new: %2, thread: %3")
            .arg(currentWidget() ? currentWidget()->objectName() : "none")
            .arg(child->objectName())
            .arg(::GetCurrentThreadId()));
#endif
    if (currentWidget())
        currentWidget()->setEnabled(false);

    d->widgetList.push_back(child);
    child->winId();
    child->raise();
    child->setFocus();
}

void MythMainWindow::detach(QWidget *child)
{
    vector<QWidget*>::iterator it =
        std::find(d->widgetList.begin(), d->widgetList.end(), child);

    if (it == d->widgetList.end())
    {
        VERBOSE(VB_IMPORTANT, "Could not find widget to detach");
        return;
    }

    d->widgetList.erase(it);
    QWidget *current = currentWidget();

    if (current)
    {
        current->setEnabled(true);
        current->setFocus();
    }

    if (d->exitingtomain)
        QApplication::postEvent(this, new ExitToMainMenuEvent());
}

QWidget *MythMainWindow::currentWidget(void)
{
    if (d->widgetList.size() > 0)
        return d->widgetList.back();
    return NULL;
}
/* FIXME: end compatability */

bool MythMainWindow::IsExitingToMain(void) const
{
    return d->exitingtomain;
}

void MythMainWindow::ExitToMainMenu(void)
{
    bool jumpdone = !(d->popwindows);

    d->exitingtomain = true;

    /* compatability code, remove, FIXME */
    QWidget *current = currentWidget();
    if (current && d->exitingtomain && d->popwindows)
    {
        if (current->objectName() != QString("mainmenu"))
        {
            if (current->objectName() == QString("video playback window"))
            {
                MythEvent *me = new MythEvent("EXIT_TO_MENU");
                QApplication::postEvent(current, me);
            }
            else if (current->inherits("MythDialog"))
            {
                QKeyEvent *key = new QKeyEvent(QEvent::KeyPress, d->escapekey,
                                               Qt::NoModifier);
                QObject *key_target = getTarget(*key);
                QApplication::postEvent(key_target, key);
            }
            return;
        }
        else
            jumpdone = true;
    }

    MythScreenStack *toplevel = GetMainStack();
    if (toplevel && d->popwindows)
    {
        MythScreenType *screen = toplevel->GetTopScreen();
        if (screen && screen->objectName() != QString("mainmenu"))
        {
            if (screen->objectName() == QString("video playback window"))
            {
                MythEvent *me = new MythEvent("EXIT_TO_MENU");
                QApplication::postEvent(screen, me);
            }
            else
            {
                QKeyEvent *key = new QKeyEvent(QEvent::KeyPress, d->escapekey,
                                               Qt::NoModifier);
                QApplication::postEvent(this, key);
            }
            return;
        }
        else
            jumpdone = true;
    }

    if (jumpdone)
    {
        d->exitingtomain = false;
        d->popwindows = true;
        if (d->exitmenucallback)
        {
            void (*callback)(void) = d->exitmenucallback;
            d->exitmenucallback = NULL;
            callback();
        }
        else if (d->exitmenumediadevicecallback)
        {
            void (*callback)(MythMediaDevice*) = d->exitmenumediadevicecallback;
            MythMediaDevice * mediadevice = d->mediadeviceforcallback;
            d->mediadeviceforcallback = NULL;
            callback(mediadevice);
        }
    }
}

bool MythMainWindow::TranslateKeyPress(const QString &context,
                                       QKeyEvent *e, QStringList &actions,
                                       bool allowJumps)
{
    actions.clear();
    int keynum = d->TranslateKeyNum(e);

    QStringList localActions;
    if (allowJumps && (d->jumpMap.count(keynum) > 0) &&
        (!d->jumpMap[keynum]->localAction.isEmpty()) &&
        (d->keyContexts.value(context)) &&
        (d->keyContexts.value(context)->GetMapping(keynum, localActions)))
    {
        if (localActions.contains(d->jumpMap[keynum]->localAction))
            allowJumps = false;
    }

    if (allowJumps && d->jumpMap.count(keynum) > 0 &&
            !d->jumpMap[keynum]->exittomain && d->exitmenucallback == NULL)
    {
        void (*callback)(void) = d->jumpMap[keynum]->callback;
        callback();
        return false;
    }

    if (allowJumps &&
        d->jumpMap.count(keynum) > 0 && d->exitmenucallback == NULL)
    {
        d->exitingtomain = true;
        d->exitmenucallback = d->jumpMap[keynum]->callback;
        QApplication::postEvent(this, new ExitToMainMenuEvent());
        return false;
    }

    bool retval = false;

    if (d->keyContexts.value(context))
    {
        if (d->keyContexts.value(context)->GetMapping(keynum, actions))
            retval = true;
    }

    if (context != "Global" &&
        d->keyContexts.value("Global")->GetMapping(keynum, actions))
    {
        retval = true;
    }

    return retval;
}

void MythMainWindow::ClearKey(const QString &context, const QString &action)
{
    KeyContext * keycontext = d->keyContexts.value(context);
    if (keycontext == NULL) return;

    QMutableMapIterator<int, QStringList> it(keycontext->actionMap);
    while (it.hasNext())
    {
        it.next();
        QStringList list = it.value();

        list.removeAll(action);
        if (list.isEmpty())
            it.remove();
    }
}

void MythMainWindow::BindKey(const QString &context, const QString &action,
                             const QString &key)
{
    QKeySequence keyseq(key);

    if (!d->keyContexts.contains(context))
        d->keyContexts.insert(context, new KeyContext());

    for (unsigned int i = 0; i < keyseq.count(); i++)
    {
        int keynum = keyseq[i];
        keynum &= ~Qt::UNICODE_ACCEL;

        QStringList dummyaction("");
        if (d->keyContexts.value(context)->GetMapping(keynum, dummyaction))
        {
            VERBOSE(VB_GENERAL, QString("Key %1 is bound to multiple actions "
                                        "in context %2.")
                    .arg(key).arg(context));
        }

        d->keyContexts.value(context)->AddMapping(keynum, action);
        //VERBOSE(VB_GENERAL, QString("Binding: %1 to action: %2 (%3)")
        //                           .arg(key).arg(action)
        //                           .arg(context));

        if (action == "ESCAPE" && context == "Global" && i == 0)
            d->escapekey = keynum;
    }
}

void MythMainWindow::RegisterKey(const QString &context, const QString &action,
                                 const QString &description, const QString &key)
{
    QString keybind = key;

    MSqlQuery query(MSqlQuery::InitCon());

    if (d->m_useDB && query.isConnected())
    {
        query.prepare("SELECT keylist, description FROM keybindings WHERE "
                      "context = :CONTEXT AND action = :ACTION AND "
                      "hostname = :HOSTNAME ;");
        query.bindValue(":CONTEXT", context);
        query.bindValue(":ACTION", action);
        query.bindValue(":HOSTNAME", GetMythDB()->GetHostName());

        bool ok = query.exec() && query.isActive();

        if (ok && query.next())
        {
            keybind = query.value(0).toString();
            QString db_description = query.value(1).toString();

            // Update keybinding description if changed
            if (db_description != description)
            {
                VERBOSE(VB_IMPORTANT, "Updating description...");
                query.prepare(
                    "UPDATE keybindings "
                    "SET description = :DESCRIPTION "
                    "WHERE context   = :CONTEXT AND "
                    "      action    = :ACTION  AND "
                    "      hostname  = :HOSTNAME");

                query.bindValue(":DESCRIPTION", description);
                query.bindValue(":CONTEXT",     context);
                query.bindValue(":ACTION",      action);
                query.bindValue(":HOSTNAME",    GetMythDB()->GetHostName());

                if (!query.exec())
                {
                    MythDB::DBError("Update Keybinding", query);
                }
            }
        }
        else
        {
            QString inskey = keybind;

            query.prepare("INSERT INTO keybindings (context, action, "
                          "description, keylist, hostname) VALUES "
                          "( :CONTEXT, :ACTION, :DESCRIPTION, :KEYLIST, "
                          ":HOSTNAME );");
            query.bindValue(":CONTEXT", context);
            query.bindValue(":ACTION", action);
            query.bindValue(":DESCRIPTION", description);
            query.bindValue(":KEYLIST", inskey);
            query.bindValue(":HOSTNAME", GetMythDB()->GetHostName());

            if (!query.exec() || !query.isActive())
            {
                MythDB::DBError("Insert Keybinding", query);
            }
        }
    }

    BindKey(context, action, keybind);
}

QString MythMainWindow::GetKey(const QString &context,
                               const QString &action) const
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return "?";

    query.prepare("SELECT keylist "
                  "FROM keybindings "
                  "WHERE context  = :CONTEXT AND "
                  "      action   = :ACTION  AND "
                  "      hostname = :HOSTNAME");
    query.bindValue(":CONTEXT", context);
    query.bindValue(":ACTION", action);
    query.bindValue(":HOSTNAME", GetMythDB()->GetHostName());

    if (!query.exec() || !query.isActive() || !query.next())
        return "?";

    return query.value(0).toString();
}

void MythMainWindow::ClearJump(const QString &destination)
{
    /* make sure that the jump point exists (using [] would add it)*/
    if (d->destinationMap.find(destination) == d->destinationMap.end())
    {
       VERBOSE(VB_GENERAL, "Cannot clear ficticious jump point"+destination);
       return;
    }

    QMutableMapIterator<int, JumpData*> it(d->jumpMap);
    while (it.hasNext())
    {
        it.next();
        JumpData *jd = it.value();
        if (jd->destination == destination)
            it.remove();
    }
}


void MythMainWindow::BindJump(const QString &destination, const QString &key)
{
    /* make sure the jump point exists */
    if (d->destinationMap.find(destination) == d->destinationMap.end())
    {
       VERBOSE(VB_GENERAL,"Cannot bind to ficticious jump point"+destination);
       return;
    }

    QKeySequence keyseq(key);

    for (unsigned int i = 0; i < keyseq.count(); i++)
    {
        int keynum = keyseq[i];
        keynum &= ~Qt::UNICODE_ACCEL;

        if (d->jumpMap.count(keynum) == 0)
        {
            //VERBOSE(VB_GENERAL, QString("Binding: %1 to JumpPoint: %2")
            //                           .arg(keybind).arg(destination));

            d->jumpMap[keynum] = &d->destinationMap[destination];
        }
        else
        {
            VERBOSE(VB_GENERAL, QString("Key %1 is already bound to a jump "
                                        "point.").arg(key));
        }
    }
    //else
    //    VERBOSE(VB_GENERAL, QString("JumpPoint: %2 exists, no keybinding")
    //                               .arg(destination));

}

void MythMainWindow::RegisterJump(const QString &destination,
                                  const QString &description,
                                  const QString &key, void (*callback)(void),
                                  bool exittomain, QString localAction)
{
    QString keybind = key;

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        query.prepare("SELECT keylist FROM jumppoints WHERE "
                      "destination = :DEST and hostname = :HOST ;");
        query.bindValue(":DEST", destination);
        query.bindValue(":HOST", GetMythDB()->GetHostName());

        if (query.exec() && query.isActive() && query.size() > 0)
        {
            query.next();
            keybind = query.value(0).toString();
        }
        else
        {
            QString inskey = keybind;

            query.prepare("INSERT INTO jumppoints (destination, description, "
                          "keylist, hostname) VALUES ( :DEST, :DESC, :KEYLIST, "
                          ":HOST );");
            query.bindValue(":DEST", destination);
            query.bindValue(":DESC", description);
            query.bindValue(":KEYLIST", inskey);
            query.bindValue(":HOST", GetMythDB()->GetHostName());

            if (!query.exec() || !query.isActive())
            {
                MythDB::DBError("Insert Jump Point", query);
            }
        }
    }

    JumpData jd =
        { callback, destination, description, exittomain, localAction };
    d->destinationMap[destination] = jd;

    BindJump(destination, keybind);
}

void MythMainWindow::JumpTo(const QString& destination, bool pop)
{
    if (destination == "ScreenShot")
	screenShot();
    else if (d->destinationMap.count(destination) > 0 && d->exitmenucallback == NULL)
    {
        d->exitingtomain = true;
        d->popwindows = pop;
        d->exitmenucallback = d->destinationMap[destination].callback;
        QApplication::postEvent(this, new ExitToMainMenuEvent());
        return;
    }
}

bool MythMainWindow::DestinationExists(const QString& destination) const
{
    return (d->destinationMap.count(destination) > 0) ? true : false;
}

void MythMainWindow::RegisterMediaPlugin(const QString &name,
                                         const QString &desc,
                                         MediaPlayCallback fn)
{
    if (d->mediaPluginMap.count(name) == 0)
    {
        VERBOSE(VB_GENERAL, QString("Registering %1 as a media playback "
                                    "plugin.").arg(name));
        MPData mpd = {desc, fn};
        d->mediaPluginMap[name] = mpd;
    }
    else
    {
        VERBOSE(VB_GENERAL, QString("%1 is already registered as a media "
                                    "playback plugin.").arg(name));
    }
}

bool MythMainWindow::HandleMedia(QString &handler, const QString &mrl,
                                 const QString &plot, const QString &title,
                                 const QString &director, int lenMins,
                                 const QString &year)
{
    if (handler.length() < 1)
        handler = "Internal";

    // Let's see if we have a plugin that matches the handler name...
    if (d->mediaPluginMap.count(handler))
    {
        d->mediaPluginMap[handler].playFn(mrl, plot, title,
                                          director, lenMins, year);
        return true;
    }

    return false;
}

void MythMainWindow::AllowInput(bool allow)
{
    d->AllowInput = allow;
}

void MythMainWindow::mouseTimeout(void)
{
    MythGestureEvent *e;

    /* complete the stroke if its our first timeout */
    if (d->gesture.recording())
    {
        d->gesture.stop();
    }

    /* get the last gesture */
    e = d->gesture.gesture();

    if (e->gesture() < MythGestureEvent::Click)
        QApplication::postEvent(this, e);
}

bool MythMainWindow::eventFilter(QObject *, QEvent *e)
{
    MythGestureEvent *ge;

    /* dont let anything through if input is disallowed. */
    if (!d->AllowInput)
        return true;

    switch (e->type())
    {
        case QEvent::KeyPress:
        {
            QKeyEvent *ke = dynamic_cast<QKeyEvent*>(e);

            // Work around weird GCC run-time bug. Only manifest on Mac OS X
            if (!ke)
                ke = (QKeyEvent *)e;

            if (currentWidget())
            {
                ke->accept();
                QWidget *current = currentWidget();
                if (current && current->isEnabled())
                    qApp->notify(current, ke);
                //else
                //    QDialog::keyPressEvent(ke);

                break;
            }

            QVector<MythScreenStack *>::Iterator it;
            for (it = d->stackList.end()-1; it != d->stackList.begin()-1; --it)
            {
                MythScreenType *top = (*it)->GetTopScreen();
                if (top)
                {
                    if (top->keyPressEvent(ke))
                    {
                        return true;
                    }
                }
            }
            break;
        }
        case QEvent::MouseButtonPress:
        {
            if (!d->gesture.recording())
            {
                d->gesture.start();
                d->gesture.record(dynamic_cast<QMouseEvent*>(e)->pos());

                /* start a single shot timer */
                d->gestureTimer->start(GESTURE_TIMEOUT);

                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease:
        {
            if (d->gestureTimer->isActive())
                d->gestureTimer->stop();

            if (currentWidget())
                break;

            if (d->gesture.recording())
            {
                d->gesture.stop();
                ge = d->gesture.gesture();

                /* handle clicks separately */
                if (ge->gesture() == MythGestureEvent::Click)
                {
                    MythUIType *clicked;
                    QVector<MythScreenStack *>::iterator it;
                    QPoint p = dynamic_cast<QMouseEvent*>(e)->pos();

                    ge->SetPosition(p);

                    for (it = d->stackList.end()-1; it != d->stackList.begin()-1;
                         --it)
                    {
                        MythScreenType *screen = (*it)->GetTopScreen();
                        if (screen && (clicked = screen->GetChildAt(p)) != NULL)
                        {
                            screen->SetFocusWidget(clicked);
                            clicked->gestureEvent(clicked, ge);
                            break;
                        }
                    }

                    delete ge;
                }
                else
                    QApplication::postEvent(this, ge);

                return true;
            }
            break;
        }
        case QEvent::MouseMove:
        {
            if (d->gesture.recording())
            {
                /* reset the timer */
                d->gestureTimer->stop();
                d->gestureTimer->start(GESTURE_TIMEOUT);

                d->gesture.record(dynamic_cast<QMouseEvent*>(e)->pos());
                return true;
            }
            break;
        }
        case QEvent::Wheel:
        {
            QWheelEvent* qmw = dynamic_cast<QWheelEvent*>(e);
            int delta = qmw->delta();
            if (delta>0)
            {
                qmw->accept();
                QKeyEvent *key = new QKeyEvent(QEvent::KeyPress, Qt::Key_Up,
                                               Qt::NoModifier);
                QObject *key_target = getTarget(*key);
                if (!key_target)
                    QApplication::postEvent(this, key);
                else
                    QApplication::postEvent(key_target, key);
            }
            if (delta<0)
            {
                qmw->accept();
                QKeyEvent *key = new QKeyEvent(QEvent::KeyPress, Qt::Key_Down,
                                               Qt::NoModifier);
                QObject *key_target = getTarget(*key);
                if (!key_target)
                    QApplication::postEvent(this, key);
                else
                    QApplication::postEvent(key_target, key);
            }
            break;
        }
        default:
            break;
    }

    return false;
}

void MythMainWindow::customEvent(QEvent *ce)
{
    if (ce->type() == MythGestureEventType)
    {
        MythGestureEvent *ge = dynamic_cast<MythGestureEvent*>(ce);
        if (ge != NULL)
        {
            MythScreenStack *toplevel = GetMainStack();
            if (toplevel && !currentWidget())
            {
                MythScreenType *screen = toplevel->GetTopScreen();
                if (screen)
                    screen->gestureEvent(NULL, ge);
            }
            cout << "Gesture: " << QString(*ge).toLocal8Bit().constData() << endl;
        }
    }
    else if (ce->type() == kExitToMainMenuEventType && d->exitingtomain)
    {
        ExitToMainMenu();
    }
    else if (ce->type() == kExternalKeycodeEventType)
    {
        ExternalKeycodeEvent *eke = (ExternalKeycodeEvent *)ce;
        int keycode = eke->getKeycode();

        QKeyEvent key(QEvent::KeyPress, keycode, Qt::NoModifier);

        QObject *key_target = getTarget(key);
        if (!key_target)
            QApplication::sendEvent(this, &key);
        else
            QApplication::sendEvent(key_target, &key);
    }
#if defined(USE_LIRC) || defined(USING_APPLEREMOTE)
    else if (ce->type() == kLircKeycodeEventType && !d->ignore_lirc_keys)
    {
        LircKeycodeEvent *lke = (LircKeycodeEvent *)ce;
        int keycode = lke->getKeycode();

        if (keycode)
        {
            GetMythUI()->ResetScreensaver();
            if (GetMythUI()->GetScreenIsAsleep())
                return;

            Qt::KeyboardModifiers mod = (Qt::KeyboardModifiers)(keycode & Qt::MODIFIER_MASK);
            int k = keycode & ~Qt::MODIFIER_MASK; /* trim off the mod */
            QString text;

            if (k & Qt::UNICODE_ACCEL)
            {
                QChar c(k & ~Qt::UNICODE_ACCEL);
                text = QString(c);
            }

            mod = ((mod & Qt::CTRL) ? Qt::ControlModifier : Qt::NoModifier) |
                  ((mod & Qt::META) ? Qt::MetaModifier : Qt::NoModifier) |
                  ((mod & Qt::ALT) ? Qt::AltModifier : Qt::NoModifier) |
                  ((mod & Qt::SHIFT) ? Qt::ShiftModifier : Qt::NoModifier);

            QKeyEvent key(lke->isKeyDown() ? QEvent::KeyPress :
                          QEvent::KeyRelease, k, mod, text);

            QObject *key_target = getTarget(key);
            if (!key_target)
                QApplication::sendEvent(this, &key);
            else
                QApplication::sendEvent(key_target, &key);
        }
        else
        {
            cerr << "LircClient warning: attempt to convert '"
                 << lke->getLircText().toLocal8Bit().constData()
                 << "' to a key sequence failed. Fix your key mappings.\n";
        }
    }
    else if (ce->type() == kLircMuteEventType)
    {
        LircMuteEvent *lme = (LircMuteEvent *)ce;
        d->ignore_lirc_keys = lme->eventsMuted();
    }
#endif
#ifdef USE_JOYSTICK_MENU
    else if (ce->type() == kJoystickKeycodeEventType && !d->ignore_joystick_keys)
    {
        JoystickKeycodeEvent *jke = (JoystickKeycodeEvent *)ce;
        int keycode = jke->getKeycode();

        if (keycode)
        {
            GetMythUI()->ResetScreensaver();
            if (GetMythUI()->GetScreenIsAsleep())
                return;

            Qt::KeyboardModifiers mod = Qt::KeyboardModifiers(keycode & Qt::MODIFIER_MASK);
            int k = (keycode & ~Qt::MODIFIER_MASK); /* trim off the mod */
            QString text;

            if (k & Qt::UNICODE_ACCEL)
            {
                QChar c(k & ~Qt::UNICODE_ACCEL);
                text = QString(c);
            }

            QKeyEvent key(jke->isKeyDown() ? QEvent::KeyPress :
                          QEvent::KeyRelease, k, mod, text);

            QObject *key_target = getTarget(key);
            if (!key_target)
                QApplication::sendEvent(this, &key);
            else
                QApplication::sendEvent(key_target, &key);
        }
        else
        {
            cerr << "JoystickMenuClient warning: attempt to convert '"
                 << jke->getJoystickMenuText().toLocal8Bit().constData()
                 << "' to a key sequence failed. Fix your key mappings.\n";
        }
    }
    else if (ce->type() == kJoystickMuteEventType)
    {
        JoystickMenuMuteEvent *jme = (JoystickMenuMuteEvent *)ce;
        d->ignore_joystick_keys = jme->eventsMuted();
    }
#endif
    else if (ce->type() == ScreenSaverEvent::kScreenSaverEventType)
    {
        ScreenSaverEvent *sse = (ScreenSaverEvent *)ce;
        switch (sse->getSSEventType())
        {
            case ScreenSaverEvent::ssetDisable:
            {
                GetMythUI()->DoDisableScreensaver();
                break;
            }
            case ScreenSaverEvent::ssetRestore:
            {
                GetMythUI()->DoRestoreScreensaver();
                break;
            }
            case ScreenSaverEvent::ssetReset:
            {
                GetMythUI()->DoResetScreensaver();
                break;
            }
            default:
            {
                cerr << "Unknown ScreenSaverEvent type: " <<
                        sse->getSSEventType() << std::endl;
            }
        }
    }
}

QObject *MythMainWindow::getTarget(QKeyEvent &key)
{
    QObject *key_target = NULL;

    if (!currentWidget())
        return key_target;

    key_target = QWidget::keyboardGrabber();

    if (!key_target)
    {
        QWidget *focus_widget = qApp->focusWidget();
        if (focus_widget && focus_widget->isEnabled())
        {
            key_target = focus_widget;

            // Yes this is special code for handling the
            // the escape key.
            if (key.key() == d->escapekey && focus_widget->topLevelWidget())
                key_target = focus_widget->topLevelWidget();
        }
    }

    if (!key_target)
        key_target = this;

    return key_target;
}

int MythMainWindow::NormalizeFontSize(int pointSize)
{
    float floatSize = pointSize;
    float desired = 100.0;

#ifdef USING_MINGW
    // logicalDpiY not supported in QT3/win.
    int logicalDpiY = 100;
    HDC hdc = GetDC(NULL);
    if (hdc)
    {
        logicalDpiY = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(NULL, hdc);
    }
#else
    int logicalDpiY = this->logicalDpiY();
#endif

    // adjust for screen resolution relative to 100 dpi
    floatSize = floatSize * desired / logicalDpiY;
    // adjust for myth GUI size relative to 800x600
    floatSize = floatSize * d->hmult;
    // adjust by the configurable fine tuning percentage
    floatSize = floatSize * ((100.0 + fonTweak) / 100.0);
    // round to the nearest point size
    pointSize = (int)(floatSize + 0.5);

    return pointSize;
}

QFont MythMainWindow::CreateQFont(const QString &face, int pointSize,
                                  int weight, bool italic)
{
    QFont font = QFont(face);
    if (!font.exactMatch())
        font = QFont(QApplication::font()).family();
    font.setPointSize(NormalizeFontSize(pointSize));
    font.setWeight(weight);
    font.setItalic(italic);

    return font;
}

MythRect MythMainWindow::NormRect(const MythRect &rect)
{
    MythRect ret;
    ret.setWidth((int)(rect.width() * d->wmult));
    ret.setHeight((int)(rect.height() * d->hmult));
    ret.moveTopLeft(QPoint((int)(rect.x() * d->wmult),
                           (int)(rect.y() * d->hmult)));
    ret = ret.normalized();

    return ret;
}

QPoint MythMainWindow::NormPoint(const QPoint &point)
{
    QPoint ret;
    ret.setX((int)(point.x() * d->wmult));
    ret.setY((int)(point.y() * d->hmult));

    return ret;
}

QSize MythMainWindow::NormSize(const QSize &size)
{
    QSize ret;
    ret.setWidth((int)(size.width() * d->wmult));
    ret.setHeight((int)(size.height() * d->hmult));

    return ret;
}

int MythMainWindow::NormX(const int x)
{
    return (int)(x * d->wmult);
}

int MythMainWindow::NormY(const int y)
{
    return (int)(y * d->hmult);
}

QRect MythMainWindow::GetUIScreenRect(void)
{
    return d->uiScreenRect;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
