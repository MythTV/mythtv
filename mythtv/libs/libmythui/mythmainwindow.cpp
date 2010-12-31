#include "mythmainwindow.h"
#include "mythmainwindow_internal.h"

// C headers
#include <cmath>
#include <pthread.h>

// C++ headers
#include <algorithm>
#include <vector>
using namespace std;

// QT headers
#ifdef USE_OPENGL_PAINTER
#include <QGLWidget>
#endif

#include <QWaitCondition>
#include <QApplication>
#include <QTimer>
#include <QDesktopWidget>
#include <QHash>
#include <QFile>
#include <QDir>
#include <QEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QSize>

// Platform headers
#include "unistd.h"
#ifdef QWS
#include <qwindowsystem_qws.h>
#endif
#ifdef Q_WS_MACX_OLDQT
#include <HIToolbox/Menus.h>   // For GetMBarHeight()
#endif

// libmythdb headers
#include "mythdb.h"
#include "mythverbose.h"
#include "mythevent.h"
#include "mythdirs.h"
#include "compat.h"
#include "mythsignalingtimer.h"
#include "mythcorecontext.h"

// Libmythui headers
#include "myththemebase.h"
#include "screensaver.h"
#include "lirc.h"
#include "lircevent.h"
#include "mythudplistener.h"

#ifdef USING_APPLEREMOTE
#include "AppleRemoteListener.h"
#endif

#ifdef USE_JOYSTICK_MENU
#include "jsmenu.h"
#include "jsmenuevent.h"
#endif

#include "mythscreentype.h"
#include "mythpainter.h"
#ifdef USE_OPENGL_PAINTER
#include "mythpainter_ogl.h"
#endif
#include "mythpainter_qt.h"
#include "mythgesture.h"
#include "mythuihelper.h"
#include "mythdialogbox.h"

#ifdef USING_VDPAU
#include "mythpainter_vdpau.h"
#endif

#ifdef USING_MINGW
#include "mythpainter_d3d9.h"
#endif

#define GESTURE_TIMEOUT 1000

#define LOC      QString("MythMainWindow: ")
#define LOC_WARN QString("MythMainWindow, Warning: ")
#define LOC_ERR  QString("MythMainWindow, Error: ")

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
    MythMainWindowPrivate() :
        wmult(1.0f), hmult(1.0f),
        screenwidth(0), screenheight(0),
        xbase(0), ybase(0),
        does_fill_screen(false),
        ignore_lirc_keys(false),
        ignore_joystick_keys(false),
        lircThread(NULL),
#ifdef USE_JOYSTICK_MENU
        joystickThread(NULL),
#endif

#ifdef USING_APPLEREMOTE
        appleRemoteListener(NULL),
        appleRemote(NULL),
#endif
        exitingtomain(false),
        popwindows(false),

        m_useDB(true),

        exitmenucallback(NULL),

        exitmenumediadevicecallback(NULL),
        mediadeviceforcallback(NULL),

        escapekey(0),

        sysEventHandler(NULL),

        drawTimer(NULL),
        mainStack(NULL),

        painter(NULL),

#ifdef USE_OPENGL_PAINTER
        render(NULL),
#endif

        AllowInput(true),

        gesture(MythGesture()),
        gestureTimer(NULL),
        hideMouseTimer(NULL),

        paintwin(NULL),

        oldpaintwin(NULL),
        oldpainter(NULL),

        m_drawDisabledDepth(0),
        m_drawEnabled(true),

        m_themeBase(NULL),

        m_udpListener(NULL)
    {
    }

    int TranslateKeyNum(QKeyEvent *e);

    float wmult, hmult;
    int screenwidth, screenheight;

    QRect screenRect;
    QRect uiScreenRect;

    int xbase, ybase;
    bool does_fill_screen;

    bool ignore_lirc_keys;
    bool ignore_joystick_keys;

    LIRC *lircThread;

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

    QObject *sysEventHandler;

    MythSignalingTimer *drawTimer;
    QVector<MythScreenStack *> stackList;
    MythScreenStack *mainStack;

    MythPainter *painter;

#ifdef USE_OPENGL_PAINTER
    MythRenderOpenGL *render;
#endif

    bool AllowInput;

    QRegion repaintRegion;

    MythGesture gesture;
    QTimer *gestureTimer;
    QTimer *hideMouseTimer;

    /* compatibility only, FIXME remove */
    std::vector<QWidget *> widgetList;

    QWidget *paintwin;

    QWidget *oldpaintwin;
    MythPainter *oldpainter;

    QMutex m_drawDisableLock;
    QMutex m_setDrawEnabledLock;
    QWaitCondition m_setDrawEnabledWait;
    uint m_drawDisabledDepth;
    bool m_drawEnabled;

    MythThemeBase *m_themeBase;
    MythUDPListener *m_udpListener;
};

// Make keynum in QKeyEvent be equivalent to what's in QKeySequence
int MythMainWindowPrivate::TranslateKeyNum(QKeyEvent* e)
{
    int keynum = e->key();

    if ((keynum != Qt::Key_Shift  ) && (keynum !=Qt::Key_Control   ) &&
        (keynum != Qt::Key_Meta   ) && (keynum !=Qt::Key_Alt       ) &&
        (keynum != Qt::Key_Super_L) && (keynum !=Qt::Key_Super_R   ) &&
        (keynum != Qt::Key_Hyper_L) && (keynum !=Qt::Key_Hyper_R   ) &&
        (keynum != Qt::Key_AltGr  ) && (keynum !=Qt::Key_CapsLock  ) &&
        (keynum != Qt::Key_NumLock) && (keynum !=Qt::Key_ScrollLock ))
    {
        Qt::KeyboardModifiers modifiers;
        // if modifiers have been pressed, rebuild keynum
        if ((modifiers = e->modifiers()) != Qt::NoModifier)
        {
            int modnum = (((modifiers & Qt::ShiftModifier) &&
                            (keynum > 0x7f) &&
                            (keynum != Qt::Key_Backtab)) ? Qt::SHIFT : 0) |
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
    {
        mainWin = new MythMainWindow(useDB);
        gCoreContext->SetGUIObject(mainWin);
    }

    return mainWin;
}

void MythMainWindow::destroyMainWindow(void)
{
    gCoreContext->SetGUIObject(NULL);
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
                                         MythMainWindowPrivate *priv,
                                         MythRenderOpenGL *rend)
                   : QGLWidget(rend, win),
                     parent(win), d(priv), render(rend)
{
    setAutoBufferSwap(false);
}

void MythPainterWindowGL::paintEvent(QPaintEvent *pe)
{
    d->repaintRegion = d->repaintRegion.unite(pe->region());
    parent->drawScreen();
}
#endif

#ifdef USING_VDPAU
MythPainterWindowVDPAU::MythPainterWindowVDPAU(MythMainWindow *win,
                                               MythMainWindowPrivate *priv)
                   : QGLWidget(win),
                     parent(win), d(priv)
{
    setAutoBufferSwap(false);
}

void MythPainterWindowVDPAU::paintEvent(QPaintEvent *pe)
{
    d->repaintRegion = d->repaintRegion.unite(pe->region());
    parent->drawScreen();
}
#endif

#ifdef USING_MINGW
MythPainterWindowD3D9::MythPainterWindowD3D9(MythMainWindow *win,
                                             MythMainWindowPrivate *priv)
                   : QGLWidget(win),
                     parent(win), d(priv)
{
    setAutoBufferSwap(false);
}

void MythPainterWindowD3D9::paintEvent(QPaintEvent *pe)
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
    d->painter = NULL;
    d->paintwin = NULL;
    d->oldpainter = NULL;
    d->oldpaintwin = NULL;

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
    d->sysEventHandler = NULL;

    installEventFilter(this);

    d->lircThread = NULL;
    StartLIRC();

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

    d->m_udpListener = new MythUDPListener();

    InitKeys();

    d->gestureTimer = new QTimer(this);
    connect(d->gestureTimer, SIGNAL(timeout()), this, SLOT(mouseTimeout()));
    d->hideMouseTimer = new QTimer(this);
    d->hideMouseTimer->setSingleShot(true);
    d->hideMouseTimer->setInterval(3000); // 3 seconds
    connect(d->hideMouseTimer, SIGNAL(timeout()), SLOT(HideMouseTimeout()));

    d->drawTimer = new MythSignalingTimer(this, SLOT(animate()));
    d->drawTimer->start(1000 / 70);

    d->AllowInput = true;

    d->repaintRegion = QRegion(QRect(0,0,0,0));

    d->m_drawEnabled = true;

    connect(this, SIGNAL(signalRemoteScreenShot(QString,int,int)),
            this, SLOT(doRemoteScreenShot(QString,int,int)),
            Qt::BlockingQueuedConnection);
}

MythMainWindow::~MythMainWindow()
{
    d->drawTimer->stop();

    while (!d->stackList.isEmpty())
    {
        delete d->stackList.back();
        d->stackList.pop_back();
    }

    delete d->m_themeBase;

    while (!d->keyContexts.isEmpty())
    {
        KeyContext *context = *d->keyContexts.begin();
        d->keyContexts.erase(d->keyContexts.begin());
        delete context;
    }

#ifdef USE_LIRC
    if (d->lircThread)
    {
        d->lircThread->deleteLater();
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

void MythMainWindow::RegisterSystemEventHandler(QObject *eventHandler)
{
    d->sysEventHandler = eventHandler;
}

QObject *MythMainWindow::GetSystemEventHandler(void)
{
    return d->sysEventHandler;
}

void MythMainWindow::animate(void)
{
    /* FIXME: remove */
    if (currentWidget() || !d->m_drawEnabled)
        return;

    if (!d->paintwin)
        return;

    d->drawTimer->blockSignals(true);

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
                (*screenit)->ResetNeedsRedraw();
                d->repaintRegion = d->repaintRegion.unite(topDirty);
                redraw = true;
            }
        }
    }

    if (redraw)
        d->paintwin->update(d->repaintRegion);

    for (it = d->stackList.begin(); it != d->stackList.end(); ++it)
        (*it)->ScheduleInitIfNeeded();

    d->drawTimer->blockSignals(false);
}

void MythMainWindow::drawScreen(void)
{
    /* FIXME: remove */
    if (currentWidget() || !d->m_drawEnabled)
        return;

    if (!d->painter->SupportsClipping())
        d->repaintRegion = d->repaintRegion.unite(d->uiScreenRect);
    else
    {
        // Ensure that the region is not larger than the screen which
        // can happen with bad themes
        d->repaintRegion = d->repaintRegion.intersect(d->uiScreenRect);

        // Check for any widgets that have been updated since we built
        // the dirty region list in ::animate()
        QVector<MythScreenStack *>::Iterator it;
        for (it = d->stackList.begin(); it != d->stackList.end(); ++it)
        {
            QVector<MythScreenType *> redrawList;
            (*it)->GetDrawOrder(redrawList);

            QVector<MythScreenType *>::Iterator screenit;
            for (screenit = redrawList.begin(); screenit != redrawList.end();
                 ++screenit)
            {
                if ((*screenit)->NeedsRedraw())
                {
                    QRegion topDirty = (*screenit)->GetDirtyArea();
                    QVector<QRect> wrects = topDirty.rects();
                    for (int i = 0; i < wrects.size(); i++)
                    {
                        bool foundThisRect = false;
                        QVector<QRect> drects = d->repaintRegion.rects();
                        for (int j = 0; j < drects.size(); j++)
                        {
                            if (drects[j].contains(wrects[i]))
                            {
                                foundThisRect = true;
                                break;
                            }
                        }

                        if (!foundThisRect)
                            return;
                    }
                }
            }
        }
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
    if (e->spontaneous())
    {
        QKeyEvent *key = new QKeyEvent(QEvent::KeyPress, d->escapekey,
                                   Qt::NoModifier);
        QCoreApplication::postEvent(this, key);
        e->ignore();
    }
    else
        QWidget::closeEvent(e);
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
                         .toString("yyyy-MM-ddThh-mm-ss.zzz"));

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

void MythMainWindow::remoteScreenShot(QString fname, int w, int h)
{
    // This will be running from the MythFEXML handler, primarily
    // Since QPixmap must be running in the GUI thread, we need to cross
    // threads here.
    emit signalRemoteScreenShot(fname, w, h);
}

void MythMainWindow::doRemoteScreenShot(QString fname, int w, int h)
{
    // This will be running in the GUI thread
    screenShot(fname, w, h);
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

bool MythMainWindow::event(QEvent *e)
{
    if (e->type() == QEvent::Show && !e->spontaneous())
    {
        QCoreApplication::postEvent(
            this, new QEvent(MythEvent::kMythPostShowEventType));
    }

    if (e->type() == MythEvent::kMythPostShowEventType)
    {
        raise();
        activateWindow();
        return true;
    }

#ifdef USING_APPLEREMOTE
    if (d->appleRemote)
    {
        if (e->type() == QEvent::WindowActivate)
            d->appleRemote->startListening();

        if (e->type() == QEvent::WindowDeactivate)
            d->appleRemote->stopListening();
    }
#endif

    return QWidget::event(e);
}

void MythMainWindow::Init(void)
{
    GetMythUI()->GetScreenSettings(d->xbase, d->screenwidth, d->wmult,
                                   d->ybase, d->screenheight, d->hmult);

    if (GetMythDB()->GetNumSetting("GuiOffsetX") > 0 ||
        GetMythDB()->GetNumSetting("GuiWidth")   > 0 ||
        GetMythDB()->GetNumSetting("GuiOffsetY") > 0 ||
        GetMythDB()->GetNumSetting("GuiHeight")  > 0)
        d->does_fill_screen = false;
    else
        d->does_fill_screen = true;

    // Set window border based on fullscreen attribute
    Qt::WindowFlags flags = Qt::Window;

    if (!GetMythDB()->GetNumSetting("RunFrontendInWindow", 0))
    {
        VERBOSE(VB_GENERAL, "Using Frameless Window");
        flags |= Qt::FramelessWindowHint;
    }

    // Workaround Qt/Windows playback bug?
#ifdef WIN32
    flags |= Qt::MSWindowsOwnDC;
#endif

    setWindowFlags(flags);

    if (d->does_fill_screen && !GetMythUI()->IsGeometryOverridden())
    {
        VERBOSE(VB_GENERAL, "Using Full Screen Window");
        setWindowState(Qt::WindowFullScreen);
    }

    d->screenRect = QRect(d->xbase, d->ybase, d->screenwidth, d->screenheight);
    d->uiScreenRect = QRect(0, 0, d->screenwidth, d->screenheight);

    setGeometry(d->xbase, d->ybase, d->screenwidth, d->screenheight);
    setFixedSize(QSize(d->screenwidth, d->screenheight));

    GetMythUI()->ThemeWidget(this);
    Show();

    if (!GetMythDB()->GetNumSetting("HideMouseCursor", 0))
        setMouseTracking(true); // Required for mouse cursor auto-hide
    // Set cursor call must come after Show() to work on some systems.
    ShowMouseCursor(false);

    move(d->xbase, d->ybase);

    if (d->paintwin)
    {
        d->oldpaintwin = d->paintwin;
        d->paintwin = NULL;
        d->drawTimer->stop();
    }

    if (d->painter)
    {
        d->oldpainter = d->painter;
        d->painter = NULL;
    }

    QString painter = GetMythDB()->GetSetting("ThemePainter", "qt");
#ifdef USE_OPENGL_PAINTER
    if (painter == "opengl")
    {
        VERBOSE(VB_GENERAL, "Using the OpenGL painter");
        d->painter = new MythOpenGLPainter();
        QGLFormat fmt;
        fmt.setDepth(false);
        d->render  = new MythRenderOpenGL(fmt);
        d->paintwin = new MythPainterWindowGL(this, d, d->render);
        QGLWidget *qgl = dynamic_cast<QGLWidget *>(d->paintwin);
        if (qgl && !qgl->isValid())
        {
            VERBOSE(VB_IMPORTANT, "Failed to create OpenGL painter. "
                                  "Check your OpenGL installation.");
            delete d->painter;
            d->painter = NULL;
            delete d->paintwin;
            d->paintwin = NULL;
        }
        else
            d->render->Init();
    }
    else
#endif
#ifdef USING_VDPAU
    if (painter == "vdpau")
    {
        VERBOSE(VB_GENERAL, "Using the VDPAU painter");
        d->painter = new MythVDPAUPainter();
        d->paintwin = new MythPainterWindowVDPAU(this, d);
    }
#endif
#ifdef USING_MINGW
    if (painter == "d3d9")
    {
        VERBOSE(VB_GENERAL, "Using the D3D9 painter");
        d->painter = new MythD3D9Painter();
        d->paintwin = new MythPainterWindowD3D9(this, d);
    }
#endif

    if (!d->painter && !d->paintwin)
    {
        VERBOSE(VB_GENERAL, "Using the Qt painter");
        d->painter = new MythQtPainter();
        d->paintwin = new MythPainterWindowQt(this, d);
    }

    if (!d->paintwin)
    {
        VERBOSE(VB_IMPORTANT, "MythMainWindow failed to create a "
                              "painter window.");
        return;
    }

    d->paintwin->move(0, 0);
    d->paintwin->setFixedSize(size());
    d->paintwin->raise();
    d->paintwin->show();
    if (!GetMythDB()->GetNumSetting("HideMouseCursor", 0))
        d->paintwin->setMouseTracking(true); // Required for mouse cursor auto-hide

    GetMythUI()->UpdateImageCache();
    if (d->m_themeBase)
        d->m_themeBase->Reload();
    else
        d->m_themeBase = new MythThemeBase();
}

void MythMainWindow::InitKeys()
{
    RegisterKey("Global", "UP", QT_TRANSLATE_NOOP("MythControls",
        "Up Arrow"),               "Up");
    RegisterKey("Global", "DOWN", QT_TRANSLATE_NOOP("MythControls",
        "Down Arrow"),           "Down");
    RegisterKey("Global", "LEFT", QT_TRANSLATE_NOOP("MythControls",
        "Left Arrow"),           "Left");
    RegisterKey("Global", "RIGHT", QT_TRANSLATE_NOOP("MythControls",
        "Right Arrow"),         "Right");
    RegisterKey("Global", "NEXT", QT_TRANSLATE_NOOP("MythControls",
        "Move to next widget"),   "Tab");
    RegisterKey("Global", "PREVIOUS", QT_TRANSLATE_NOOP("MythControls",
        "Move to preview widget"), "Backtab");
    RegisterKey("Global", "SELECT", QT_TRANSLATE_NOOP("MythControls",
        "Select"), "Return,Enter,Space");
    RegisterKey("Global", "BACKSPACE", QT_TRANSLATE_NOOP("MythControls",
        "Backspace"),       "Backspace");
    RegisterKey("Global", "ESCAPE", QT_TRANSLATE_NOOP("MythControls",
        "Escape"),                "Esc");
    RegisterKey("Global", "MENU", QT_TRANSLATE_NOOP("MythControls",
        "Pop-up menu"),             "M");
    RegisterKey("Global", "INFO", QT_TRANSLATE_NOOP("MythControls",
        "More information"),        "I");
    RegisterKey("Global", "DELETE", QT_TRANSLATE_NOOP("MythControls",
        "Delete"),                  "D");
    RegisterKey("Global", "EDIT", QT_TRANSLATE_NOOP("MythControls",
        "Edit"),                    "E");

    RegisterKey("Global", "PAGEUP", QT_TRANSLATE_NOOP("MythControls",
        "Page Up"),              "PgUp");
    RegisterKey("Global", "PAGEDOWN", QT_TRANSLATE_NOOP("MythControls",
        "Page Down"),          "PgDown");
    RegisterKey("Global", "PAGETOP", QT_TRANSLATE_NOOP("MythControls",
        "Page to top of list"),      "");
    RegisterKey("Global", "PAGEMIDDLE", QT_TRANSLATE_NOOP("MythControls",
        "Page to middle of list"),   "");
    RegisterKey("Global", "PAGEBOTTOM", QT_TRANSLATE_NOOP("MythControls",
        "Page to bottom of list"),   "");

    RegisterKey("Global", "PREVVIEW", QT_TRANSLATE_NOOP("MythControls",
        "Previous View"),        "Home");
    RegisterKey("Global", "NEXTVIEW", QT_TRANSLATE_NOOP("MythControls",
        "Next View"),             "End");

    RegisterKey("Global", "HELP", QT_TRANSLATE_NOOP("MythControls",
        "Help"),                   "F1");
    RegisterKey("Global", "EJECT", QT_TRANSLATE_NOOP("MythControls"
        ,"Eject Removable Media"),   "");

    RegisterKey("Global", "CUT", QT_TRANSLATE_NOOP("MythControls",
        "Cut text from textedit"), "Ctrl+X");
    RegisterKey("Global", "COPY", QT_TRANSLATE_NOOP("MythControls"
        ,"Copy text from textedit"), "Ctrl+C");
    RegisterKey("Global", "PASTE", QT_TRANSLATE_NOOP("MythControls",
        "Paste text into textedit"), "Ctrl+V");

    RegisterKey("Global", "0", QT_TRANSLATE_NOOP("MythControls","0"), "0");
    RegisterKey("Global", "1", QT_TRANSLATE_NOOP("MythControls","1"), "1");
    RegisterKey("Global", "2", QT_TRANSLATE_NOOP("MythControls","2"), "2");
    RegisterKey("Global", "3", QT_TRANSLATE_NOOP("MythControls","3"), "3");
    RegisterKey("Global", "4", QT_TRANSLATE_NOOP("MythControls","4"), "4");
    RegisterKey("Global", "5", QT_TRANSLATE_NOOP("MythControls","5"), "5");
    RegisterKey("Global", "6", QT_TRANSLATE_NOOP("MythControls","6"), "6");
    RegisterKey("Global", "7", QT_TRANSLATE_NOOP("MythControls","7"), "7");
    RegisterKey("Global", "8", QT_TRANSLATE_NOOP("MythControls","8"), "8");
    RegisterKey("Global", "9", QT_TRANSLATE_NOOP("MythControls","9"), "9");

    RegisterKey("Global", "SYSEVENT01", QT_TRANSLATE_NOOP("MythControls",
        "Trigger System Key Event #1"), "");
    RegisterKey("Global", "SYSEVENT02", QT_TRANSLATE_NOOP("MythControls",
        "Trigger System Key Event #2"), "");
    RegisterKey("Global", "SYSEVENT03", QT_TRANSLATE_NOOP("MythControls",
        "Trigger System Key Event #3"), "");
    RegisterKey("Global", "SYSEVENT04", QT_TRANSLATE_NOOP("MythControls",
        "Trigger System Key Event #4"), "");
    RegisterKey("Global", "SYSEVENT05", QT_TRANSLATE_NOOP("MythControls",
        "Trigger System Key Event #5"), "");
    RegisterKey("Global", "SYSEVENT06", QT_TRANSLATE_NOOP("MythControls",
        "Trigger System Key Event #6"), "");
    RegisterKey("Global", "SYSEVENT07", QT_TRANSLATE_NOOP("MythControls",
        "Trigger System Key Event #7"), "");
    RegisterKey("Global", "SYSEVENT08", QT_TRANSLATE_NOOP("MythControls",
        "Trigger System Key Event #8"), "");
    RegisterKey("Global", "SYSEVENT09", QT_TRANSLATE_NOOP("MythControls",
        "Trigger System Key Event #9"), "");
    RegisterKey("Global", "SYSEVENT10", QT_TRANSLATE_NOOP("MythControls",
        "Trigger System Key Event #10"), "");

    // these are for the html viewer widget (MythUIWebBrowser)
    RegisterKey("Browser", "ZOOMIN",          QT_TRANSLATE_NOOP("MythControls",
        "Zoom in on browser window"),           ".,>");
    RegisterKey("Browser", "ZOOMOUT",         QT_TRANSLATE_NOOP("MythControls",
        "Zoom out on browser window"),          ",,<");
    RegisterKey("Browser", "TOGGLEINPUT",     QT_TRANSLATE_NOOP("MythControls",
        "Toggle where keyboard input goes to"),  "F1");

    RegisterKey("Browser", "MOUSEUP",         QT_TRANSLATE_NOOP("MythControls",
        "Move mouse pointer up"),                 "2");
    RegisterKey("Browser", "MOUSEDOWN",       QT_TRANSLATE_NOOP("MythControls",
        "Move mouse pointer down"),               "8");
    RegisterKey("Browser", "MOUSELEFT",       QT_TRANSLATE_NOOP("MythControls",
        "Move mouse pointer left"),               "4");
    RegisterKey("Browser", "MOUSERIGHT",      QT_TRANSLATE_NOOP("MythControls",
        "Move mouse pointer right"),              "6");
    RegisterKey("Browser", "MOUSELEFTBUTTON", QT_TRANSLATE_NOOP("MythControls",
        "Mouse Left button click"),               "5");

    RegisterKey("Browser", "PAGEDOWN",        QT_TRANSLATE_NOOP("MythControls",
        "Scroll down half a page"),               "9");
    RegisterKey("Browser", "PAGEUP",          QT_TRANSLATE_NOOP("MythControls",
        "Scroll up half a page"),                 "3");
    RegisterKey("Browser", "PAGELEFT",        QT_TRANSLATE_NOOP("MythControls",
        "Scroll left half a page"),               "7");
    RegisterKey("Browser", "PAGERIGHT",       QT_TRANSLATE_NOOP("MythControls",
        "Scroll right half a page"),              "1");

    RegisterKey("Browser", "NEXTLINK",        QT_TRANSLATE_NOOP("MythControls",
        "Move selection to next link"),           "Z");
    RegisterKey("Browser", "PREVIOUSLINK",    QT_TRANSLATE_NOOP("MythControls",
        "Move selection to previous link"),       "Q");
    RegisterKey("Browser", "FOLLOWLINK",      QT_TRANSLATE_NOOP("MythControls",
        "Follow selected link"),            "Return,Space,Enter");
    RegisterKey("Browser", "HISTORYBACK",     QT_TRANSLATE_NOOP("MythControls",
        "Go back to previous page"),        "R,Backspace");
    RegisterKey("Browser", "HISTORYFORWARD",  QT_TRANSLATE_NOOP("MythControls",
        "Go forward to previous page"),     "F");

    RegisterKey("Main Menu",    "EXIT",       QT_TRANSLATE_NOOP("MythControls",
        "System Exit"),                     "Esc");
}

void MythMainWindow::ResetKeys()
{
    ClearKeyContext("Global");
    ClearKeyContext("Browser");
    ClearKeyContext("Main Menu");
    InitKeys();
}

void MythMainWindow::ReinitDone(void)
{
    delete d->oldpainter;
    d->oldpainter = NULL;

    delete d->oldpaintwin;
    d->oldpaintwin = NULL;

    d->paintwin->move(0, 0);
    d->paintwin->setFixedSize(size());
    d->paintwin->raise();
    d->paintwin->show();

    d->drawTimer->start(1000 / 70);
}

void MythMainWindow::Show(void)
{
    show();
#ifdef Q_WS_MACX_OLDQT
    if (d->does_fill_screen)
        HideMenuBar();
    else
        ShowMenuBar();
#endif
}

/* FIXME compatibility only */
void MythMainWindow::attach(QWidget *child)
{
#ifdef USING_MINGW
#warning TODO FIXME MythMainWindow::attach() does not always work on MS Windows!
    // if windows are created on different threads,
    // or if setFocus() is called from a thread other than the main UI thread,
    // setFocus() hangs the thread that called it
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
    child->setMouseTracking(true);
}

void MythMainWindow::detach(QWidget *child)
{
    std::vector<QWidget*>::iterator it =
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
        current->setMouseTracking(true);
    }

    if (d->exitingtomain)
    {
        QCoreApplication::postEvent(
            this, new QEvent(MythEvent::kExitToMainMenuEventType));
    }
}

QWidget *MythMainWindow::currentWidget(void)
{
    if (d->widgetList.size() > 0)
        return d->widgetList.back();
    return NULL;
}
/* FIXME: end compatibility */

uint MythMainWindow::PushDrawDisabled(void)
{
    QMutexLocker locker(&d->m_drawDisableLock);
    d->m_drawDisabledDepth++;
    if (d->m_drawDisabledDepth && d->m_drawEnabled)
        SetDrawEnabled(false);
    return d->m_drawDisabledDepth;
}

uint MythMainWindow::PopDrawDisabled(void)
{
    QMutexLocker locker(&d->m_drawDisableLock);
    if (d->m_drawDisabledDepth)
    {
        d->m_drawDisabledDepth--;
        if (!d->m_drawDisabledDepth && !d->m_drawEnabled)
            SetDrawEnabled(true);
    }
    return d->m_drawDisabledDepth;
}

void MythMainWindow::SetDrawEnabled(bool enable)
{
    QMutexLocker locker(&d->m_setDrawEnabledLock);

    if (!gCoreContext->IsUIThread())
    {
        QCoreApplication::postEvent(
            this, new MythEvent(
                (enable) ?
                MythEvent::kEnableDrawingEventType :
                MythEvent::kDisableDrawingEventType));

        while (QCoreApplication::hasPendingEvents())
            d->m_setDrawEnabledWait.wait(&d->m_setDrawEnabledLock);

        return;
    }

    setUpdatesEnabled(enable);
    d->m_drawEnabled = enable;

    if (enable)
    {
        repaint(); // See #8952
        d->drawTimer->start(1000 / 70);
    }
    else
        d->drawTimer->stop();

    d->m_setDrawEnabledWait.wakeAll();
}

void MythMainWindow::SetEffectsEnabled(bool enable)
{
    QVector<MythScreenStack *>::Iterator it;
    for (it = d->stackList.begin(); it != d->stackList.end(); ++it)
    {
        if (enable)
            (*it)->EnableEffects();
        else
            (*it)->DisableEffects();
    }
}

bool MythMainWindow::IsExitingToMain(void) const
{
    return d->exitingtomain;
}

void MythMainWindow::ExitToMainMenu(void)
{
    bool jumpdone = !(d->popwindows);

    d->exitingtomain = true;

    /* compatibility code, remove, FIXME */
    QWidget *current = currentWidget();
    if (current && d->exitingtomain && d->popwindows)
    {
        if (current->objectName() != QString("mainmenu"))
        {
            if (current->objectName() == QString("video playback window"))
            {
                MythEvent *me = new MythEvent("EXIT_TO_MENU");
                QCoreApplication::postEvent(current, me);
            }
            else if (current->inherits("MythDialog"))
            {
                QKeyEvent *key = new QKeyEvent(QEvent::KeyPress, d->escapekey,
                                               Qt::NoModifier);
                QObject *key_target = getTarget(*key);
                QCoreApplication::postEvent(key_target, key);
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
                QCoreApplication::postEvent(screen, me);
            }
            else
            {
                QKeyEvent *key = new QKeyEvent(QEvent::KeyPress, d->escapekey,
                                               Qt::NoModifier);
                QCoreApplication::postEvent(this, key);
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

/**
 * \brief Get a list of actions for a keypress in the given context
 * \param context The context in which to lookup the keypress for actions.
 * \param e       The keypress event to lookup.
 * \param actions The QStringList that will contain the list of actions.
 * \param allowJumps if true then jump points are allowed
 *
 * \return true if the key event has been handled (the keypress was a jumpoint)
           false if the caller should continue to handle keypress
 */
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
        return true;
    }

    if (allowJumps &&
        d->jumpMap.count(keynum) > 0 && d->exitmenucallback == NULL)
    {
        d->exitingtomain = true;
        d->exitmenucallback = d->jumpMap[keynum]->callback;
        QCoreApplication::postEvent(
            this, new QEvent(MythEvent::kExitToMainMenuEventType));
        return true;
    }

    if (d->keyContexts.value(context))
        d->keyContexts.value(context)->GetMapping(keynum, actions);

    if (context != "Global")
        d->keyContexts.value("Global")->GetMapping(keynum, actions);

    return false;
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

void MythMainWindow::ClearKeyContext(const QString &context)
{
    KeyContext *keycontext = d->keyContexts.value(context);
    if (keycontext != NULL)
        keycontext->actionMap.clear();
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

        if (query.exec() && query.next())
        {
            keybind = query.value(0).toString();
            QString db_description = query.value(1).toString();

            // Update keybinding description if changed
            if (db_description != description)
            {
                VERBOSE(VB_IMPORTANT, "Updating keybinding description...");
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

                if (!query.exec() && !(GetMythDB()->SuppressDBMessages()))
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

            if (!query.exec() && !(GetMythDB()->SuppressDBMessages()))
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

        if (query.exec() && query.next())
        {
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
        QCoreApplication::postEvent(
            this, new QEvent(MythEvent::kExitToMainMenuEventType));
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

bool MythMainWindow::HandleMedia(const QString &handler, const QString &mrl,
                                 const QString &plot, const QString &title,
                                 const QString &subtitle,
                                 const QString &director, int season,
                                 int episode, int lenMins,
                                 const QString &year)
{
    QString lhandler(handler);
    if (lhandler.isEmpty())
        lhandler = "Internal";

    // Let's see if we have a plugin that matches the handler name...
    if (d->mediaPluginMap.count(lhandler))
    {
        d->mediaPluginMap[lhandler].playFn(mrl, plot, title, subtitle,
                                          director, season, episode, lenMins,
                                          year);
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
        QCoreApplication::postEvent(this, e);
}

bool MythMainWindow::eventFilter(QObject *, QEvent *e)
{
    MythGestureEvent *ge;

    /* Don't let anything through if input is disallowed. */
    if (!d->AllowInput)
        return true;

    switch (e->type())
    {
        case QEvent::KeyPress:
        {
            QKeyEvent *ke = dynamic_cast<QKeyEvent*>(e);

            // Work around weird GCC run-time bug. Only manifest on Mac OS X
            if (!ke)
                ke = static_cast<QKeyEvent *>(e);

            if (currentWidget())
            {
                ke->accept();
                QWidget *current = currentWidget();
                if (current && current->isEnabled())
                    qApp->notify(current, ke);

                break;
            }

            QVector<MythScreenStack *>::Iterator it;
            for (it = d->stackList.end()-1; it != d->stackList.begin()-1; --it)
            {
                MythScreenType *top = (*it)->GetTopScreen();
                if (top)
                {
                    if (top->keyPressEvent(ke))
                        return true;

                    // Note:  The following break prevents keypresses being
                    //        sent to windows below popups
                    if ((*it)->objectName() == "popup stack")
                        break;
                }
            }
            break;
        }
        case QEvent::MouseButtonPress:
        {
            ShowMouseCursor(true);
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
            ShowMouseCursor(true);
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
                    QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent*>(e);
                    if (!mouseEvent)
                        return false;

                    QVector<MythScreenStack *>::iterator it;
                    QPoint p = mouseEvent->pos();

                    ge->SetPosition(p);

                    MythGestureEvent::Button button = MythGestureEvent::NoButton;
                    switch (mouseEvent->button())
                    {
                        case Qt::LeftButton :
                            button = MythGestureEvent::LeftButton;
                            break;
                        case Qt::RightButton :
                            button = MythGestureEvent::RightButton;
                            break;
                        case Qt::MidButton :
                            button = MythGestureEvent::MiddleButton;
                            break;
                        case Qt::XButton1 :
                            button = MythGestureEvent::Aux1Button;
                            break;
                        case Qt::XButton2 :
                            button = MythGestureEvent::Aux2Button;
                            break;
                        default :
                            button = MythGestureEvent::NoButton;
                    }

                    ge->SetButton(button);

                    for (it = d->stackList.end()-1; it != d->stackList.begin()-1;
                         --it)
                    {
                        MythScreenType *screen = (*it)->GetTopScreen();

                        if (!screen || !screen->ContainsPoint(p))
                            continue;

                        if (screen->gestureEvent(ge))
                            break;

                        // Note:  The following break prevents clicks being
                        //        sent to windows below popups
                        //
                        //        we want to permit this in some cases, e.g.
                        //        when the music miniplayer is on screen or a
                        //        non-interactive alert/news scroller. So these
                        //        things need to be in a third or more stack
                        if ((*it)->objectName() == "popup stack")
                            break;
                    }

                    delete ge;
                }
                else
                    QCoreApplication::postEvent(this, ge);

                return true;
            }
            break;
        }
        case QEvent::MouseMove:
        {
            ShowMouseCursor(true);
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
            ShowMouseCursor(true);
            QWheelEvent* qmw = dynamic_cast<QWheelEvent*>(e);
            int delta = qmw->delta();
            if (delta>0)
            {
                qmw->accept();
                QKeyEvent *key = new QKeyEvent(QEvent::KeyPress, Qt::Key_Up,
                                               Qt::NoModifier);
                QObject *key_target = getTarget(*key);
                if (!key_target)
                    QCoreApplication::postEvent(this, key);
                else
                    QCoreApplication::postEvent(key_target, key);
            }
            if (delta<0)
            {
                qmw->accept();
                QKeyEvent *key = new QKeyEvent(QEvent::KeyPress, Qt::Key_Down,
                                               Qt::NoModifier);
                QObject *key_target = getTarget(*key);
                if (!key_target)
                    QCoreApplication::postEvent(this, key);
                else
                    QCoreApplication::postEvent(key_target, key);
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
    if (ce->type() == MythGestureEvent::kEventType)
    {
        MythGestureEvent *ge = static_cast<MythGestureEvent*>(ce);
        MythScreenStack *toplevel = GetMainStack();
        if (toplevel && !currentWidget())
        {
            MythScreenType *screen = toplevel->GetTopScreen();
            if (screen)
                screen->gestureEvent(ge);
        }
        VERBOSE(VB_GUI, QString("Gesture: %1")
                .arg(QString(*ge).toLocal8Bit().constData()));
    }
    else if (ce->type() == MythEvent::kExitToMainMenuEventType &&
             d->exitingtomain)
    {
        ExitToMainMenu();
    }
    else if (ce->type() == ExternalKeycodeEvent::kEventType)
    {
        ExternalKeycodeEvent *eke = static_cast<ExternalKeycodeEvent *>(ce);
        int keycode = eke->getKeycode();

        QKeyEvent key(QEvent::KeyPress, keycode, Qt::NoModifier);

        QObject *key_target = getTarget(key);
        if (!key_target)
            QCoreApplication::sendEvent(this, &key);
        else
            QCoreApplication::sendEvent(key_target, &key);
    }
#if defined(USE_LIRC) || defined(USING_APPLEREMOTE)
    else if (ce->type() == LircKeycodeEvent::kEventType &&
             !d->ignore_lirc_keys)
    {
        LircKeycodeEvent *lke = static_cast<LircKeycodeEvent *>(ce);

        if (LircKeycodeEvent::kLIRCInvalidKeyCombo == lke->modifiers())
        {
            VERBOSE(VB_IMPORTANT, QString("MythMainWindow, Warning: ") +
                    QString("Attempt to convert LIRC key sequence '%1' "
                            "to a Qt key sequence failed.")
                    .arg(lke->lirctext()));
        }
        else
        {
            GetMythUI()->ResetScreensaver();
            if (GetMythUI()->GetScreenIsAsleep())
                return;

            QKeyEvent key(lke->keytype(),   lke->key(),
                          lke->modifiers(), lke->text());

            QObject *key_target = getTarget(key);
            if (!key_target)
                QCoreApplication::sendEvent(this, &key);
            else
                QCoreApplication::sendEvent(key_target, &key);
        }
    }
#endif
#ifdef USE_JOYSTICK_MENU
    else if (ce->type() == JoystickKeycodeEvent::kEventType &&
             !d->ignore_joystick_keys)
    {
        JoystickKeycodeEvent *jke = static_cast<JoystickKeycodeEvent *>(ce);
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
                QCoreApplication::sendEvent(this, &key);
            else
                QCoreApplication::sendEvent(key_target, &key);
        }
        else
        {
            VERBOSE(VB_IMPORTANT,
                    QString("JoystickMenuClient warning: attempt to convert "
                            "'%1' to a key sequence failed. Fix your key "
                            "mappings.")
                    .arg(jke->getJoystickMenuText().toLocal8Bit().constData()));
        }
    }
#endif
    else if (ce->type() == ScreenSaverEvent::kEventType)
    {
        ScreenSaverEvent *sse = static_cast<ScreenSaverEvent *>(ce);
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
                VERBOSE(VB_IMPORTANT,
                        QString("Unknown ScreenSaverEvent type: %1")
                        .arg(sse->getSSEventType()));
            }
        }
    }
    else if (ce->type() == MythEvent::kPushDisableDrawingEventType)
    {
        PushDrawDisabled();
    }
    else if (ce->type() == MythEvent::kPopDisableDrawingEventType)
    {
        PopDrawDisabled();
    }
    else if (ce->type() == MythEvent::kDisableDrawingEventType)
    {
        SetDrawEnabled(false);
    }
    else if (ce->type() == MythEvent::kEnableDrawingEventType)
    {
        SetDrawEnabled(true);
    }
    else if (ce->type() == MythEvent::kLockInputDevicesEventType)
    {
        LockInputDevices(true);
    }
    else if (ce->type() == MythEvent::kUnlockInputDevicesEventType)
    {
        LockInputDevices(false);
    }
    else if ((MythEvent::Type)(ce->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)ce;
        QString message = me->Message();

        if (message.left(12) == "HANDLE_MEDIA")
        {
            QStringList tokens = message.split(' ', QString::SkipEmptyParts);
            HandleMedia(tokens[1],
                        message.mid(tokens[0].length() +
                                    tokens[1].length() + 2));
        }
    }
    else if ((MythEvent::Type)(ce->type()) == MythEvent::MythUserMessage)
    {
        MythEvent *me = (MythEvent *)ce;
        QString message = me->Message();

        if (!message.isEmpty())
            ShowOkPopup(message);
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
    // round to the nearest point size
    pointSize = (int)(floatSize + 0.5);

    return pointSize;
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

void MythMainWindow::SetScalingFactors(float wmult, float hmult)
{
    d->wmult = wmult;
    d->hmult = hmult;
}

QRect MythMainWindow::GetUIScreenRect(void)
{
    return d->uiScreenRect;
}

void MythMainWindow::SetUIScreenRect(QRect &rect)
{
    d->uiScreenRect = rect;
}

void MythMainWindow::StartLIRC(void)
{
#ifdef USE_LIRC
    if (d->lircThread)
    {
        d->lircThread->deleteLater();
        d->lircThread = NULL;
    }

    QString config_file = GetConfDir() + "/lircrc";
    if (!QFile::exists(config_file))
        config_file = QDir::homePath() + "/.lircrc";

    /* lircd socket moved from /dev/ to /var/run/lirc/ in lirc 0.8.6 */
    QString lirc_socket = "/dev/lircd";
    if (!QFile::exists(lirc_socket))
        lirc_socket = "/var/run/lirc/lircd";

    d->lircThread = new LIRC(
        this,
        GetMythDB()->GetSetting("LircSocket", lirc_socket),
        "mythtv", config_file);

    if (d->lircThread->Init())
    {
        d->lircThread->start();
    }
    else
    {
        d->lircThread->deleteLater();
        d->lircThread = NULL;
    }
#endif
}

void MythMainWindow::LockInputDevices( bool locked )
{
    if( locked )
        VERBOSE(VB_IMPORTANT, "Locking input devices");
    else
        VERBOSE(VB_IMPORTANT, "Unlocking input devices");

#ifdef USE_LIRC
    d->ignore_lirc_keys = locked;
#endif

#ifdef USE_JOYSTICK_MENU
    d->ignore_joystick_keys = locked;
#endif
}

void MythMainWindow::ShowMouseCursor(bool show)
{
    if (show && GetMythDB()->GetNumSetting("HideMouseCursor", 0))
        return;
#ifdef QWS
    QWSServer::setCursorVisible(show);
#endif
    // Set cursor call must come after Show() to work on some systems.
    setCursor(show ? (Qt::ArrowCursor) : (Qt::BlankCursor));

    if (show)
        d->hideMouseTimer->start();
}

void MythMainWindow::HideMouseTimeout(void)
{
    ShowMouseCursor(false);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
