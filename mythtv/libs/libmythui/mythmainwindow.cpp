#include "mythmainwindow.h"
#include "mythmainwindow_internal.h"

// C headers
#include <cmath>

// C++ headers
#include <algorithm>
#include <utility>
#include <vector>
using namespace std;

// QT headers
#include <QWaitCondition>
#include <QApplication>
#include <QTimer>
#include <QHash>
#include <QFile>
#include <QDir>
#include <QEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QSize>
#include <QWindow>

// Platform headers
#include "unistd.h"

// libmythbase headers
#include "mythdb.h"
#include "mythlogging.h"
#include "mythevent.h"
#include "mythdirs.h"
#include "compat.h"
#include "mythsignalingtimer.h"
#include "mythcorecontext.h"
#include "mythmedia.h"
#include "mythmiscutil.h"
#include "mythdate.h"

// libmythui headers
#include "myththemebase.h"
#include "screensaver.h"
#include "lirc.h"
#include "lircevent.h"
#include "mythudplistener.h"
#include "mythrender_base.h"
#include "mythuistatetracker.h"
#include "mythuiactions.h"
#include "mythrect.h"
#include "mythdisplay.h"

#ifdef USING_APPLEREMOTE
#include "AppleRemoteListener.h"
#endif

#ifdef USE_JOYSTICK_MENU
#include "jsmenu.h"
#include "jsmenuevent.h"
#endif

#ifdef USING_LIBCEC
#include "devices/mythcecadapter.h"
#endif

#include "mythscreentype.h"
#include "mythpainter.h"
#ifdef USING_OPENGL
#include "mythpainteropengl.h"
#endif
#include "mythpainter_qt.h"
#include "mythgesture.h"
#include "mythuihelper.h"
#include "mythdialogbox.h"

#ifdef _WIN32
#include "mythpainter_d3d9.h"
#endif

#ifdef Q_OS_ANDROID
#include <QtAndroid>
#endif

#define GESTURE_TIMEOUT 1000
#define STANDBY_TIMEOUT 90 // Minutes
#define LONGPRESS_INTERVAL 1000

#define LOC      QString("MythMainWindow: ")

class KeyContext
{
  public:
    void AddMapping(int key, const QString& action)
    {
        m_actionMap[key].append(action);
    }

    bool GetMapping(int key, QStringList &actions)
    {
        if (m_actionMap.count(key) > 0)
        {
            actions += m_actionMap[key];
            return true;
        }
        return false;
    }

    QMap<int, QStringList> m_actionMap;
};

// Adding member initializers caused compilation to fail with an error
// that it cannot convert a brace-enclosed initializer list to JumpData.
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct JumpData
{
    void (*m_callback)(void);
    QString m_destination;
    QString m_description;
    bool    m_exittomain;
    QString m_localAction;
};

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct MPData {
    QString           m_description;
    MediaPlayCallback m_playFn;
};

class MythMainWindowPrivate
{
  public:
    MythMainWindowPrivate() :
        m_gesture(MythGesture())
    {
    }

    static int TranslateKeyNum(QKeyEvent *e);

    float                m_wmult                {1.0F};
    float                m_hmult                {1.0F};
    QRect                m_screenRect;
    QRect                m_uiScreenRect;
    bool                 m_doesFillScreen       {false};

    bool                 m_ignoreLircKeys       {false};
    bool                 m_ignoreJoystickKeys   {false};

    LIRC                *m_lircThread           {nullptr};

#ifdef USE_JOYSTICK_MENU
    JoystickMenuThread  *m_joystickThread       {nullptr};
#endif

#ifdef USING_APPLEREMOTE
    AppleRemoteListener *m_appleRemoteListener  {nullptr};
    AppleRemote         *m_appleRemote          {nullptr};
#endif

#ifdef USING_LIBCEC
    MythCECAdapter       m_cecAdapter           { };
#endif

    bool                 m_exitingtomain        {false};
    bool                 m_popwindows           {false};

    /// To allow or prevent database access
    bool                 m_useDB                {true};

    QHash<QString, KeyContext *> m_keyContexts;
    QMap<int, JumpData*> m_jumpMap;
    QMap<QString, JumpData> m_destinationMap;
    QMap<QString, MPData> m_mediaPluginMap;
    QHash<QString, QHash<QString, QString> > m_actionText;

    void (*m_exitMenuCallback)(void) {nullptr};

    void (*m_exitMenuMediaDeviceCallback)(MythMediaDevice* mediadevice) {nullptr};
    MythMediaDevice * m_mediaDeviceForCallback {nullptr};

    int      m_escapekey                 {0};

    QObject *m_sysEventHandler           {nullptr};

    int      m_drawInterval              {1000 / MythMainWindow::drawRefresh};
    MythSignalingTimer *m_drawTimer      {nullptr};
    QVector<MythScreenStack *> m_stackList;
    MythScreenStack *m_mainStack         {nullptr};

    MythDisplay     *m_display           { MythDisplay::AcquireRelease() };
    MythPainter     *m_painter           {nullptr};

    QRegion          m_repaintRegion;

    MythGesture      m_gesture;
    QTimer          *m_gestureTimer      {nullptr};
    QTimer          *m_hideMouseTimer    {nullptr};

    /* compatibility only, FIXME remove */
    std::vector<QWidget *> m_widgetList;
    QMap<QWidget *, bool> m_enabledWidgets;

    MythPainterWindow *m_paintwin        {nullptr};

    QWidget         *m_oldpaintwin       {nullptr};
    MythPainter     *m_oldpainter        {nullptr};

    QMutex           m_drawDisableLock;
    uint             m_drawDisabledDepth {0};
    bool             m_drawEnabled       {true};

    MythThemeBase   *m_themeBase         {nullptr};
    MythUDPListener *m_udpListener       {nullptr};

    MythNotificationCenter *m_nc         {nullptr};
    QTimer          *m_idleTimer         {nullptr};
    int              m_idleTime          {0};
    bool             m_standby           {false};
    bool             m_enteringStandby   {false};
    bool             m_disableIdle       {false};

    bool             m_allowInput        {true};

    bool             m_pendingUpdate     {false};

        // window aspect
    bool             m_firstinit         {true};
    bool             m_bSavedPOS         {false};
    // Support for long press
    int              m_longPressKeyCode  {0};
    ulong            m_longPressTime     {0};
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
            int modnum = Qt::NoModifier;
            if (((modifiers & Qt::ShiftModifier) != 0U) &&
                (keynum > 0x7f) &&
                (keynum != Qt::Key_Backtab))
                modnum |= Qt::SHIFT;
            if ((modifiers & Qt::ControlModifier) != 0U)
                modnum |= Qt::CTRL;
            if ((modifiers & Qt::MetaModifier) != 0U)
                modnum |= Qt::META;
            if ((modifiers & Qt::AltModifier) != 0U)
                modnum |= Qt::ALT;
            modnum &= ~Qt::UNICODE_ACCEL;
            return (keynum | modnum);
        }
    }

    return keynum;
}

static MythMainWindow *mainWin = nullptr;
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
    if (gCoreContext)
        gCoreContext->SetGUIObject(nullptr);
    delete mainWin;
    mainWin = nullptr;
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

MythNotificationCenter *GetNotificationCenter(void)
{
    if (!mainWin ||
        !mainWin->GetCurrentNotificationCenter())
        return nullptr;
    return mainWin->GetCurrentNotificationCenter();
}

MythPainterWindow::MythPainterWindow(MythMainWindow *MainWin)
  : QWidget(MainWin)
{
}

#ifdef USING_OPENGL
MythPainterWindowGL::MythPainterWindowGL(MythMainWindow *MainWin,
                                         MythMainWindowPrivate *MainWinPriv)
  : MythPainterWindow(MainWin),
    m_parent(MainWin),
    d(MainWinPriv)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    winId();
#ifdef Q_OS_MACOS
     // must be visible before OpenGL initialisation on OSX
    setVisible(true);
#endif
    MythRenderOpenGL *render = MythRenderOpenGL::Create(this);
    if (render)
    {
        m_render = render;
        if (render->Init() && render->IsRecommendedRenderer())
            m_valid = true;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create MythRenderOpenGL");
    }
}

QPaintEngine *MythPainterWindowGL::paintEngine() const
{
    return testAttribute(Qt::WA_PaintOnScreen) ? nullptr : m_parent->paintEngine();
}

MythPainterWindowGL::~MythPainterWindowGL()
{
    if (m_render)
        m_render->DecrRef();
}

bool MythPainterWindowGL::IsValid(void)
{
    return m_valid;
}

void MythPainterWindowGL::paintEvent(QPaintEvent *pe)
{
    d->m_repaintRegion = d->m_repaintRegion.united(pe->region());
    m_parent->drawScreen();
}
#endif

#ifdef _WIN32
MythPainterWindowD3D9::MythPainterWindowD3D9(MythMainWindow *win,
                                             MythMainWindowPrivate *priv)
                   : QWidget(win),
                     m_parent(win), d(priv)
{
    setAutoBufferSwap(false);
}

void MythPainterWindowD3D9::paintEvent(QPaintEvent *pe)
{
    d->m_repaintRegion = d->m_repaintRegion.united(pe->region());
    m_parent->drawScreen();
}
#endif

MythPainterWindowQt::MythPainterWindowQt(MythMainWindow *MainWin,
                                         MythMainWindowPrivate *MainWinPriv)
  : MythPainterWindow(MainWin),
    m_parent(MainWin),
    d(MainWinPriv)
{
    setAttribute(Qt::WA_NoSystemBackground);
}

void MythPainterWindowQt::paintEvent(QPaintEvent *pe)
{
    d->m_repaintRegion = d->m_repaintRegion.united(pe->region());
    m_parent->drawScreen();
}

MythMainWindow::MythMainWindow(const bool useDB)
  : QWidget(nullptr)
{
    d = new MythMainWindowPrivate;

    setObjectName("mainwindow");

    d->m_allowInput = false;

    // This prevents database errors from RegisterKey() when there is no DB:
    d->m_useDB = useDB;
    d->m_painter = nullptr;
    d->m_paintwin = nullptr;
    d->m_oldpainter = nullptr;
    d->m_oldpaintwin = nullptr;

    //Init();

    d->m_ignoreLircKeys = false;
    d->m_ignoreJoystickKeys = false;
    d->m_exitingtomain = false;
    d->m_popwindows = true;
    d->m_exitMenuCallback = nullptr;
    d->m_exitMenuMediaDeviceCallback = nullptr;
    d->m_mediaDeviceForCallback = nullptr;
    d->m_escapekey = Qt::Key_Escape;
    d->m_mainStack = nullptr;
    d->m_sysEventHandler = nullptr;

    installEventFilter(this);

    d->m_lircThread = nullptr;
    StartLIRC();

#ifdef USE_JOYSTICK_MENU
    d->m_ignoreJoystickKeys = false;

    QString joy_config_file = GetConfDir() + "/joystickmenurc";

    d->m_joystickThread = nullptr;
    d->m_joystickThread = new JoystickMenuThread(this);
    if (d->m_joystickThread->Init(joy_config_file))
        d->m_joystickThread->start();
#endif

#ifdef USING_APPLEREMOTE
    d->m_appleRemoteListener = new AppleRemoteListener(this);
    d->m_appleRemote         = AppleRemote::Get();

    d->m_appleRemote->setListener(d->m_appleRemoteListener);
    d->m_appleRemote->startListening();
    if (d->m_appleRemote->isListeningToRemote())
    {
        d->m_appleRemote->start();
    }
    else
    {
        // start listening failed, no remote receiver present
        delete d->m_appleRemote;
        delete d->m_appleRemoteListener;
        d->m_appleRemote = nullptr;
        d->m_appleRemoteListener = nullptr;
    }
#endif

    d->m_udpListener = new MythUDPListener();

    InitKeys();

    d->m_gestureTimer = new QTimer(this);
    connect(d->m_gestureTimer, SIGNAL(timeout()), this, SLOT(mouseTimeout()));
    d->m_hideMouseTimer = new QTimer(this);
    d->m_hideMouseTimer->setSingleShot(true);
    d->m_hideMouseTimer->setInterval(3000); // 3 seconds
    connect(d->m_hideMouseTimer, SIGNAL(timeout()), SLOT(HideMouseTimeout()));

    d->m_drawTimer = new MythSignalingTimer(this, SLOT(animate()));
    d->m_drawTimer->start(d->m_drawInterval);

    d->m_allowInput = true;

    d->m_repaintRegion = QRegion(QRect(0,0,0,0));

    d->m_drawEnabled = true;

    connect(this, SIGNAL(signalRemoteScreenShot(QString,int,int)),
            this, SLOT(doRemoteScreenShot(QString,int,int)),
            Qt::BlockingQueuedConnection);
    connect(this, SIGNAL(signalSetDrawEnabled(bool)),
            this, SLOT(SetDrawEnabled(bool)),
            Qt::BlockingQueuedConnection);
#ifdef Q_OS_ANDROID
    connect(qApp, SIGNAL(applicationStateChanged(Qt::ApplicationState)),
            this, SLOT(onApplicationStateChange(Qt::ApplicationState)));
#endif

    // We need to listen for playback start/end events
    gCoreContext->addListener(this);

    d->m_idleTime = gCoreContext->GetNumSetting("FrontendIdleTimeout",
                                              STANDBY_TIMEOUT);

    if (d->m_idleTime < 0)
        d->m_idleTime = 0;

    d->m_idleTimer = new QTimer(this);
    d->m_idleTimer->setSingleShot(false);
    d->m_idleTimer->setInterval(1000 * 60 * d->m_idleTime);
    connect(d->m_idleTimer, SIGNAL(timeout()), SLOT(IdleTimeout()));
    if (d->m_idleTime > 0)
        d->m_idleTimer->start();
}

MythMainWindow::~MythMainWindow()
{
    gCoreContext->removeListener(this);

    d->m_drawTimer->stop();

    while (!d->m_stackList.isEmpty())
    {
        MythScreenStack *stack = d->m_stackList.back();
        d->m_stackList.pop_back();

        if (stack == d->m_mainStack)
            d->m_mainStack = nullptr;

        delete stack;
    }

    delete d->m_themeBase;

    while (!d->m_keyContexts.isEmpty())
    {
        KeyContext *context = *d->m_keyContexts.begin();
        d->m_keyContexts.erase(d->m_keyContexts.begin());
        delete context;
    }

#ifdef USE_LIRC
    if (d->m_lircThread)
    {
        d->m_lircThread->deleteLater();
        d->m_lircThread = nullptr;
    }
#endif

#ifdef USE_JOYSTICK_MENU
    if (d->m_joystickThread)
    {
        if (d->m_joystickThread->isRunning())
        {
            d->m_joystickThread->Stop();
            d->m_joystickThread->wait();
        }

        delete d->m_joystickThread;
        d->m_joystickThread = nullptr;
    }
#endif

#ifdef USING_APPLEREMOTE
    delete d->m_appleRemote;
    delete d->m_appleRemoteListener;
#endif

#ifdef USING_LIBCEC
    d->m_cecAdapter.Close();
#endif

    delete d->m_nc;

    delete d->m_painter;
    delete d->m_paintwin;

    MythDisplay::AcquireRelease(false);

    delete d;
}

MythPainter *MythMainWindow::GetCurrentPainter(void)
{
    return d->m_painter;
}

MythNotificationCenter *MythMainWindow::GetCurrentNotificationCenter(void)
{
    return d->m_nc;
}

QWidget *MythMainWindow::GetPaintWindow(void)
{
    return d->m_paintwin;
}

void MythMainWindow::ShowPainterWindow(void)
{
    if (d->m_paintwin)
    {
        d->m_paintwin->show();
        d->m_paintwin->raise();
    }
}

void MythMainWindow::HidePainterWindow(void)
{
    if (d->m_paintwin)
    {
        d->m_paintwin->clearMask();
        if (!d->m_paintwin->RenderIsShared())
            d->m_paintwin->hide();
    }
}

MythRender *MythMainWindow::GetRenderDevice()
{
    return d->m_paintwin->GetRenderDevice();
}

void MythMainWindow::AddScreenStack(MythScreenStack *stack, bool main)
{
    d->m_stackList.push_back(stack);
    if (main)
        d->m_mainStack = stack;
}

void MythMainWindow::PopScreenStack()
{
    MythScreenStack *stack = d->m_stackList.back();
    d->m_stackList.pop_back();
    if (stack == d->m_mainStack)
        d->m_mainStack = nullptr;
    delete stack;
}

int MythMainWindow::GetStackCount(void)
{
    return d->m_stackList.size();
}

MythScreenStack *MythMainWindow::GetMainStack(void)
{
    return d->m_mainStack;
}

MythScreenStack *MythMainWindow::GetStack(const QString &stackname)
{
    foreach (auto & widget, d->m_stackList)
    {
        if (widget->objectName() == stackname)
            return widget;
    }
    return nullptr;
}

MythScreenStack* MythMainWindow::GetStackAt(int pos)
{
    if (pos >= 0 && pos < d->m_stackList.size())
        return d->m_stackList.at(pos);

    return nullptr;
}

void MythMainWindow::animate(void)
{
    if (!d->m_drawEnabled || !d->m_paintwin)
        return;

    d->m_drawTimer->blockSignals(true);

    bool redraw = false;

    if (!d->m_repaintRegion.isEmpty())
        redraw = true;

    foreach (auto & widget, d->m_stackList)
    {
        QVector<MythScreenType *> drawList;
        widget->GetDrawOrder(drawList);

        foreach (auto & screen, drawList)
        {
            screen->Pulse();

            if (screen->NeedsRedraw())
            {
                QRegion topDirty = screen->GetDirtyArea();
                screen->ResetNeedsRedraw();
                d->m_repaintRegion = d->m_repaintRegion.united(topDirty);
                redraw = true;
            }
        }
    }

    if (redraw && !d->m_paintwin->RenderIsShared())
        d->m_paintwin->update(d->m_repaintRegion);

    foreach (auto & widget, d->m_stackList)
        widget->ScheduleInitIfNeeded();

    d->m_drawTimer->blockSignals(false);
}

void MythMainWindow::drawScreen(void)
{
    if (!d->m_drawEnabled)
        return;

    if (!d->m_painter->SupportsClipping())
        d->m_repaintRegion = d->m_repaintRegion.united(d->m_uiScreenRect);
    else
    {
        // Ensure that the region is not larger than the screen which
        // can happen with bad themes
        d->m_repaintRegion = d->m_repaintRegion.intersected(d->m_uiScreenRect);

        // Check for any widgets that have been updated since we built
        // the dirty region list in ::animate()
        foreach (auto & widget, d->m_stackList)
        {
            QVector<MythScreenType *> redrawList;
            widget->GetDrawOrder(redrawList);

            foreach (auto & screen, redrawList)
            {
                if (screen->NeedsRedraw())
                {
#if QT_VERSION < QT_VERSION_CHECK(5, 8, 0)
                    QRegion topDirty = screen->GetDirtyArea();
                    QVector<QRect> wrects = topDirty.rects();
                    for (int i = 0; i < wrects.size(); i++)
                    {
                        bool foundThisRect = false;
                        QVector<QRect> drects = d->m_repaintRegion.rects();
                        for (int j = 0; j < drects.size(); j++)
                        {
                            // Can't use QRegion::contains because it only
                            // checks for overlap.  QRect::contains checks
                            // if fully contained.
                            if (drects[j].contains(wrects[i]))
                            {
                                foundThisRect = true;
                                break;
                            }
                        }

                        if (!foundThisRect)
                            return;
                    }
#else
                    for (const QRect& wrect: screen->GetDirtyArea())
                    {
                        bool foundThisRect = false;
                        for (const QRect& drect: d->m_repaintRegion)
                        {
                            // Can't use QRegion::contains because it only
                            // checks for overlap.  QRect::contains checks
                            // if fully contained.
                            if (drect.contains(wrect))
                            {
                                foundThisRect = true;
                                break;
                            }
                        }

                        if (!foundThisRect)
                            return;
                    }
#endif
                }
            }
        }
    }

    if (!d->m_paintwin->RenderIsShared())
        draw();

    d->m_repaintRegion = QRegion(QRect(0, 0, 0, 0));
}

void MythMainWindow::draw(MythPainter *painter /* = 0 */)
{
    if (!painter)
        painter = d->m_painter;

    if (!painter)
        return;

    painter->Begin(d->m_paintwin);

    if (!painter->SupportsClipping())
        d->m_repaintRegion = QRegion(d->m_uiScreenRect);

#if QT_VERSION < QT_VERSION_CHECK(5, 8, 0)
    QVector<QRect> rects = d->m_repaintRegion.rects();
    for (int i = 0; i < rects.size(); i++)
    {
        const QRect& r = rects[i];
#else
    for (const QRect& r : d->m_repaintRegion)
    {
#endif
        if (r.width() == 0 || r.height() == 0)
            continue;

        if (r != d->m_uiScreenRect)
            painter->SetClipRect(r);

        foreach (auto & widget, d->m_stackList)
        {
            QVector<MythScreenType *> redrawList;
            widget->GetDrawOrder(redrawList);

            foreach (auto & screen, redrawList)
            {
                screen->Draw(painter, 0, 0, 255, r);
            }
        }
    }

    painter->End();
    d->m_repaintRegion = QRegion();
}

// virtual
QPaintEngine *MythMainWindow::paintEngine() const
{
    return testAttribute(Qt::WA_PaintOnScreen) ? nullptr : QWidget::paintEngine();
}

void MythMainWindow::closeEvent(QCloseEvent *e)
{
    if (e->spontaneous())
    {
        auto *key = new QKeyEvent(QEvent::KeyPress, d->m_escapekey,
                                  Qt::NoModifier);
        QCoreApplication::postEvent(this, key);
        e->ignore();
    }
    else
        QWidget::closeEvent(e);
}

void MythMainWindow::GrabWindow(QImage &image)
{
    WId winid = 0;
    QWidget *active = QApplication::activeWindow();
    if (active)
        winid = active->winId();
    else
    {
        // According to the following we page, you "just pass 0 as the
        // window id, indicating that we want to grab the entire screen".
        //
        // https://doc.qt.io/qt-5/qtwidgets-desktop-screenshot-example.html#screenshot-class-implementation
        winid = 0;
    }

    QScreen *screen = MythDisplay::AcquireRelease()->GetCurrentScreen();
    MythDisplay::AcquireRelease(false);
    if (screen)
    {
        QPixmap p = screen->grabWindow(winid);
        image = p.toImage();
    }
}

/* This is required to allow a screenshot to be requested by another thread
 * other than the UI thread, and to wait for the screenshot before returning.
 * It is used by mythweb for the remote access screenshots
 */
void MythMainWindow::doRemoteScreenShot(const QString& filename, int x, int y)
{
    // This will be running in the UI thread, as is required by QPixmap
    QStringList args;
    args << QString::number(x);
    args << QString::number(y);
    args << filename;

    MythEvent me(MythEvent::MythEventMessage, ACTION_SCREENSHOT, args);
    qApp->sendEvent(this, &me);
}

void MythMainWindow::RemoteScreenShot(QString filename, int x, int y)
{
    // This will be running in a non-UI thread and is used to trigger a
    // function in the UI thread, and waits for completion of that handler
    emit signalRemoteScreenShot(std::move(filename), x, y);
}

bool MythMainWindow::SaveScreenShot(const QImage &image, QString filename)
{
    if (filename.isEmpty())
    {
        QString fpath = GetMythDB()->GetSetting("ScreenShotPath", "/tmp");
        filename = QString("%1/myth-screenshot-%2.png").arg(fpath)
            .arg(MythDate::toString(
                     MythDate::current(), MythDate::kScreenShotFilename));
    }

    QString extension = filename.section('.', -1, -1);
    if (extension == "jpg")
        extension = "JPEG";
    else
        extension = "PNG";

    LOG(VB_GENERAL, LOG_INFO, QString("Saving screenshot to %1 (%2x%3)")
                       .arg(filename).arg(image.width()).arg(image.height()));

    if (image.save(filename, extension.toLatin1(), 100))
    {
        LOG(VB_GENERAL, LOG_INFO, "MythMainWindow::screenShot succeeded");
        return true;
    }

    LOG(VB_GENERAL, LOG_INFO, "MythMainWindow::screenShot Failed!");
    return false;
}

bool MythMainWindow::ScreenShot(int w, int h, QString filename)
{
    QImage img;
    GrabWindow(img);
    if (w <= 0)
        w = img.width();
    if (h <= 0)
        h = img.height();

    img = img.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return SaveScreenShot(img, std::move(filename));
}

bool MythMainWindow::event(QEvent *e)
{
    if (!updatesEnabled() && (e->type() == QEvent::UpdateRequest))
        d->m_pendingUpdate = true;

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
    if (d->m_appleRemote)
    {
        if (e->type() == QEvent::WindowActivate)
            d->m_appleRemote->startListening();

        if (e->type() == QEvent::WindowDeactivate)
            d->m_appleRemote->stopListening();
    }
#endif

    return QWidget::event(e);
}

void MythMainWindow::Init(bool mayReInit)
{
    d->m_display->SetWidget(nullptr);
    d->m_useDB = ! gCoreContext->GetDB()->SuppressDBMessages();

    if (!(mayReInit || d->m_firstinit))
        return;

    d->m_doesFillScreen =
        (GetMythDB()->GetNumSetting("GuiOffsetX") == 0 &&
         GetMythDB()->GetNumSetting("GuiWidth")   == 0 &&
         GetMythDB()->GetNumSetting("GuiOffsetY") == 0 &&
         GetMythDB()->GetNumSetting("GuiHeight")  == 0);

    // Set window border based on fullscreen attribute
    Qt::WindowFlags flags = Qt::Window;

    bool inwindow = GetMythDB()->GetBoolSetting("RunFrontendInWindow", false) &&
                    !WindowIsAlwaysFullscreen();
    bool fullscreen = d->m_doesFillScreen && !MythUIHelper::IsGeometryOverridden();

    // On Compiz/Unit, when the window is fullscreen and frameless changing
    // screen position ends up stuck. Adding a border temporarily prevents this
    setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);

    // Workaround Qt/Windows playback bug?
#ifdef _WIN32
    flags |= Qt::MSWindowsOwnDC;
#endif

    // NOTE if running fullscreen AND windowed (i.e. borders etc) then we do not
    // have any idea at this time of the size of the borders/decorations.
    // Typically, on linux, this means we create the UI slightly larger than
    // required - as X adds the decorations at a later point.

    if (fullscreen && !inwindow)
    {
        LOG(VB_GENERAL, LOG_INFO, "Using Full Screen Window");
        if (d->m_firstinit)
        {
            // During initialization, we force being fullscreen using setWindowState
            // otherwise, in ubuntu's unity, the side bar often stays visible
            setWindowState(Qt::WindowFullScreen);
        }
    }
    else
    {
        // reset type
        setWindowState(Qt::WindowNoState);
    }

    if (gCoreContext->GetBoolSetting("AlwaysOnTop", false) &&
        !WindowIsAlwaysFullscreen())
    {
        flags |= Qt::WindowStaysOnTopHint;
    }

    setWindowFlags(flags);

    // SetWidget may move the widget into a new screen.
    d->m_display->SetWidget(this);
    // Ensure MythUIHelper has latest screen bounds if we have moved
    GetMythUI()->UpdateScreenSettings();
    // And use them
    GetMythUI()->GetScreenSettings(d->m_screenRect, d->m_wmult, d->m_hmult);

    QTimer::singleShot(1000, this, SLOT(DelayedAction()));

    d->m_uiScreenRect = QRect(QPoint(0, 0), d->m_screenRect.size());
    LOG(VB_GENERAL, LOG_INFO, QString("UI Screen Resolution: %1 x %2")
        .arg(d->m_screenRect.width()).arg(d->m_screenRect.height()));
    MoveResize(d->m_screenRect);
    Show();
    // The window is sometimes not created until Show has been called - so try
    // MythDisplay::setWidget again to ensure we listen for QScreen changes
    d->m_display->SetWidget(this);

    if (!GetMythDB()->GetBoolSetting("HideMouseCursor", false))
        setMouseTracking(true); // Required for mouse cursor auto-hide
    // Set cursor call must come after Show() to work on some systems.
    ShowMouseCursor(false);

    move(d->m_screenRect.topLeft());

    if (d->m_paintwin)
    {
        d->m_oldpaintwin = d->m_paintwin;
        d->m_paintwin = nullptr;
        d->m_drawTimer->stop();
    }

    if (d->m_painter)
    {
        d->m_oldpainter = d->m_painter;
        d->m_painter = nullptr;
    }

#ifdef USING_OPENGL
    // always use OpenGL by default. Only fallback to Qt painter as a last resort.
    if (!d->m_painter && !d->m_paintwin)
    {
        MythPainterWindowGL* glwindow = new MythPainterWindowGL(this, d);
        if (glwindow && glwindow->IsValid())
        {
            d->m_paintwin = glwindow;
            MythRenderOpenGL *render = dynamic_cast<MythRenderOpenGL*>(glwindow->GetRenderDevice());
            d->m_painter = new MythOpenGLPainter(render, this);
        }
        else if (glwindow)
        {
            delete glwindow;
        }
    }
#endif

    // NOLINTNEXTLINE(readability-misleading-indentation)
    bool openglwarn = false;
    if (!d->m_painter && !d->m_paintwin)
    {
        LOG(VB_GENERAL, LOG_INFO, "Using the Qt painter. Video playback will not work!");
        d->m_painter = new MythQtPainter();
        d->m_paintwin = new MythPainterWindowQt(this, d);
        openglwarn = QCoreApplication::applicationName() == MYTH_APPNAME_MYTHFRONTEND;
    }

    if (!d->m_paintwin)
    {
        LOG(VB_GENERAL, LOG_ERR, "MythMainWindow failed to create a painter window.");
        return;
    }

    if (d->m_painter->GetName() != "Qt")
    {
        setAttribute(Qt::WA_NoSystemBackground);
        setAutoFillBackground(false);
    }

    MoveResize(d->m_screenRect);
    ShowPainterWindow();

    // Redraw the window now to avoid race conditions in EGLFS (Qt5.4) if a
    // 2nd window (e.g. TVPlayback) is created before this is redrawn.
#ifdef ANDROID
    LOG(VB_GENERAL, LOG_INFO, QString("Platform name is %1").arg(qApp->platformName()));
#   define EARLY_SHOW_PLATFORM_NAME_CHECK "android"
#else
#   define EARLY_SHOW_PLATFORM_NAME_CHECK "egl"
#endif
    if (qApp->platformName().contains(EARLY_SHOW_PLATFORM_NAME_CHECK))
        qApp->processEvents();

    if (!GetMythDB()->GetBoolSetting("HideMouseCursor", false))
        d->m_paintwin->setMouseTracking(true); // Required for mouse cursor auto-hide

    GetMythUI()->UpdateImageCache();
    if (d->m_themeBase)
        d->m_themeBase->Reload();
    else
        d->m_themeBase = new MythThemeBase();

    if (!d->m_nc)
        d->m_nc = new MythNotificationCenter();

#ifdef USING_LIBCEC
    // Open any adapter after the window has been created to ensure we capture
    // the EDID if available. This will close any existing adapter in the event
    // that the window has been re-init'ed.
    d->m_cecAdapter.Open();
#endif

    if (openglwarn)
    {
        MythNotification notification(tr("Warning: OpenGL is not available."), "");
        d->m_nc->Queue(notification);
    }
}

void MythMainWindow::DelayedAction(void)
{
    MoveResize(d->m_screenRect);
    Show();

#ifdef Q_OS_ANDROID
    QtAndroid::hideSplashScreen();
#endif
}

void MythMainWindow::InitKeys()
{
    RegisterKey("Global", ACTION_UP, QT_TRANSLATE_NOOP("MythControls",
        "Up Arrow"),               "Up");
    RegisterKey("Global", ACTION_DOWN, QT_TRANSLATE_NOOP("MythControls",
        "Down Arrow"),           "Down");
    RegisterKey("Global", ACTION_LEFT, QT_TRANSLATE_NOOP("MythControls",
        "Left Arrow"),           "Left");
    RegisterKey("Global", ACTION_RIGHT, QT_TRANSLATE_NOOP("MythControls",
        "Right Arrow"),         "Right");
    RegisterKey("Global", "NEXT", QT_TRANSLATE_NOOP("MythControls",
        "Move to next widget"),   "Tab");
    RegisterKey("Global", "PREVIOUS", QT_TRANSLATE_NOOP("MythControls",
        "Move to preview widget"), "Backtab");
    RegisterKey("Global", ACTION_SELECT, QT_TRANSLATE_NOOP("MythControls",
        "Select"), "Return,Enter,Space");
    RegisterKey("Global", "BACKSPACE", QT_TRANSLATE_NOOP("MythControls",
        "Backspace"),       "Backspace");
    RegisterKey("Global", "ESCAPE", QT_TRANSLATE_NOOP("MythControls",
        "Escape"),                "Esc");
    RegisterKey("Global", "MENU", QT_TRANSLATE_NOOP("MythControls",
        "Pop-up menu"),             "M,Meta+Enter");
    RegisterKey("Global", "INFO", QT_TRANSLATE_NOOP("MythControls",
        "More information"),        "I");
    RegisterKey("Global", "DELETE", QT_TRANSLATE_NOOP("MythControls",
        "Delete"),                  "D");
    RegisterKey("Global", "EDIT", QT_TRANSLATE_NOOP("MythControls",
        "Edit"),                    "E");
    RegisterKey("Global", ACTION_SCREENSHOT, QT_TRANSLATE_NOOP("MythControls",
         "Save screenshot"), "");
    RegisterKey("Global", ACTION_HANDLEMEDIA, QT_TRANSLATE_NOOP("MythControls",
         "Play a media resource"), "");

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
    RegisterKey("Global", "NEWLINE", QT_TRANSLATE_NOOP("MythControls",
        "Insert newline into textedit"), "Ctrl+Return");
    RegisterKey("Global", "UNDO", QT_TRANSLATE_NOOP("MythControls",
        "Undo"), "Ctrl+Z");
    RegisterKey("Global", "REDO", QT_TRANSLATE_NOOP("MythControls",
        "Redo"), "Ctrl+Y");
    RegisterKey("Global", "SEARCH", QT_TRANSLATE_NOOP("MythControls",
        "Show incremental search dialog"), "Ctrl+S");

    RegisterKey("Global", ACTION_0, QT_TRANSLATE_NOOP("MythControls","0"), "0");
    RegisterKey("Global", ACTION_1, QT_TRANSLATE_NOOP("MythControls","1"), "1");
    RegisterKey("Global", ACTION_2, QT_TRANSLATE_NOOP("MythControls","2"), "2");
    RegisterKey("Global", ACTION_3, QT_TRANSLATE_NOOP("MythControls","3"), "3");
    RegisterKey("Global", ACTION_4, QT_TRANSLATE_NOOP("MythControls","4"), "4");
    RegisterKey("Global", ACTION_5, QT_TRANSLATE_NOOP("MythControls","5"), "5");
    RegisterKey("Global", ACTION_6, QT_TRANSLATE_NOOP("MythControls","6"), "6");
    RegisterKey("Global", ACTION_7, QT_TRANSLATE_NOOP("MythControls","7"), "7");
    RegisterKey("Global", ACTION_8, QT_TRANSLATE_NOOP("MythControls","8"), "8");
    RegisterKey("Global", ACTION_9, QT_TRANSLATE_NOOP("MythControls","9"), "9");

    RegisterKey("Global", ACTION_TVPOWERON,  QT_TRANSLATE_NOOP("MythControls",
        "Turn the display on"),   "");
    RegisterKey("Global", ACTION_TVPOWEROFF, QT_TRANSLATE_NOOP("MythControls",
        "Turn the display off"),  "");

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

    RegisterKey("Main Menu",    "EXITPROMPT", QT_TRANSLATE_NOOP("MythControls",
        "Display System Exit Prompt"),      "Esc");
    RegisterKey("Main Menu",    "EXIT",       QT_TRANSLATE_NOOP("MythControls",
        "System Exit"),                     "");
    RegisterKey("Main Menu",    "STANDBYMODE",QT_TRANSLATE_NOOP("MythControls",
        "Enter Standby Mode"),              "");
    RegisterKey("Long Press",    "LONGPRESS1",QT_TRANSLATE_NOOP("MythControls",
        "Up to 16 Keys that allow Long Press"),      "");
    RegisterKey("Long Press",    "LONGPRESS2",QT_TRANSLATE_NOOP("MythControls",
        "Up to 16 Keys that allow Long Press"),      "");
    RegisterKey("Long Press",    "LONGPRESS3",QT_TRANSLATE_NOOP("MythControls",
        "Up to 16 Keys that allow Long Press"),      "");
    RegisterKey("Long Press",    "LONGPRESS4",QT_TRANSLATE_NOOP("MythControls",
        "Up to 16 Keys that allow Long Press"),      "");
}

void MythMainWindow::ReloadKeys()
{
    ClearKeyContext("Global");
    ClearKeyContext("Browser");
    ClearKeyContext("Main Menu");
    InitKeys();
}

void MythMainWindow::ReinitDone(void)
{
    delete d->m_oldpainter;
    d->m_oldpainter = nullptr;
    delete d->m_oldpaintwin;
    d->m_oldpaintwin = nullptr;


    ShowPainterWindow();
    MoveResize(d->m_screenRect);

    d->m_drawTimer->start(1000 / drawRefresh);
}

void MythMainWindow::Show(void)
{
    bool inwindow = GetMythDB()->GetBoolSetting("RunFrontendInWindow", false);
    bool fullscreen = d->m_doesFillScreen && !MythUIHelper::IsGeometryOverridden();
    if (fullscreen && !inwindow && !d->m_firstinit)
        showFullScreen();
    else
        show();
    d->m_firstinit = false;
}

void MythMainWindow::MoveResize(QRect &Geometry)
{
    setFixedSize(Geometry.size());
    setGeometry(Geometry);

    if (d->m_paintwin)
    {
        d->m_paintwin->setFixedSize(Geometry.size());
        d->m_paintwin->setGeometry(0, 0, Geometry.width(), Geometry.height());
    }
}

/// \brief Return true if the current platform only supports fullscreen windows
bool MythMainWindow::WindowIsAlwaysFullscreen(void)
{
#ifdef Q_OS_ANDROID
    return true;
#else
    // this may need to cover other platform plugins
    return qApp->platformName().toLower().contains("eglfs");
#endif
}

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
    if (!gCoreContext->IsUIThread())
    {
        emit signalSetDrawEnabled(enable);
        return;
    }

    setUpdatesEnabled(enable);
    d->m_drawEnabled = enable;

    if (enable)
    {
        if (d->m_pendingUpdate)
        {
            QApplication::postEvent(this, new QEvent(QEvent::UpdateRequest), Qt::LowEventPriority);
            d->m_pendingUpdate = false;
        }
        d->m_drawTimer->start(1000 / drawRefresh);
        ShowPainterWindow();
    }
    else
    {
        HidePainterWindow();
        d->m_drawTimer->stop();
    }
}

void MythMainWindow::SetEffectsEnabled(bool enable)
{
    foreach (auto & widget, d->m_stackList)
    {
        if (enable)
            widget->EnableEffects();
        else
            widget->DisableEffects();
    }
}

bool MythMainWindow::IsExitingToMain(void) const
{
    return d->m_exitingtomain;
}

void MythMainWindow::ExitToMainMenu(void)
{
    bool jumpdone = !(d->m_popwindows);

    d->m_exitingtomain = true;

    MythScreenStack *toplevel = GetMainStack();
    if (toplevel && d->m_popwindows)
    {
        MythScreenType *screen = toplevel->GetTopScreen();
        if (screen && screen->objectName() != QString("mainmenu"))
        {
            MythEvent xe("EXIT_TO_MENU");
            gCoreContext->dispatch(xe);
            if (screen->objectName() == QString("video playback window"))
            {
                auto *me = new MythEvent("EXIT_TO_MENU");
                QCoreApplication::postEvent(screen, me);
            }
            else
            {
                auto *key = new QKeyEvent(QEvent::KeyPress, d->m_escapekey,
                                          Qt::NoModifier);
                QCoreApplication::postEvent(this, key);
                MythNotificationCenter *nc =  MythNotificationCenter::GetInstance();
                // Notifications have their own stack. We need to continue
                // the propagation of the escape here if there are notifications.
                int num = nc->DisplayedNotifications();
                if (num > 0)
                    QCoreApplication::postEvent(
                        this, new QEvent(MythEvent::kExitToMainMenuEventType));
            }
            return;
        }
        jumpdone = true;
    }

    if (jumpdone)
    {
        d->m_exitingtomain = false;
        d->m_popwindows = true;
        if (d->m_exitMenuCallback)
        {
            void (*callback)(void) = d->m_exitMenuCallback;
            d->m_exitMenuCallback = nullptr;
            callback();
        }
        else if (d->m_exitMenuMediaDeviceCallback)
        {
            void (*callback)(MythMediaDevice*) = d->m_exitMenuMediaDeviceCallback;
            MythMediaDevice * mediadevice = d->m_mediaDeviceForCallback;
            d->m_mediaDeviceForCallback = nullptr;
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

    // Special case for custom QKeyEvent where the action is embedded directly
    // in the QKeyEvent text property. Used by MythFEXML http extension
    if (e->key() == 0 && !e->text().isEmpty() &&
        e->modifiers() == Qt::NoModifier)
    {
        QString action = e->text();
        // check if it is a jumppoint
        if (!d->m_destinationMap.contains(action))
        {
            actions.append(action);
            return false;
        }

        if (allowJumps)
        {
            // This does not filter the jump based on the current location but
            // is consistent with handling of other actions that do not need
            // a keybinding. The network control code tries to match
            // GetCurrentLocation with the jumppoint but matching is utterly
            // inconsistent e.g. mainmenu<->Main Menu, Playback<->Live TV
            JumpTo(action);
            return true;
        }

        return false;
    }

    int keynum = d->TranslateKeyNum(e);

    QStringList localActions;
    if (allowJumps && (d->m_jumpMap.count(keynum) > 0) &&
        (!d->m_jumpMap[keynum]->m_localAction.isEmpty()) &&
        (d->m_keyContexts.value(context)) &&
        (d->m_keyContexts.value(context)->GetMapping(keynum, localActions)))
    {
        if (localActions.contains(d->m_jumpMap[keynum]->m_localAction))
            allowJumps = false;
    }

    if (allowJumps && d->m_jumpMap.count(keynum) > 0 &&
            !d->m_jumpMap[keynum]->m_exittomain && d->m_exitMenuCallback == nullptr)
    {
        void (*callback)(void) = d->m_jumpMap[keynum]->m_callback;
        callback();
        return true;
    }

    if (allowJumps &&
        d->m_jumpMap.count(keynum) > 0 && d->m_exitMenuCallback == nullptr)
    {
        d->m_exitingtomain = true;
        d->m_exitMenuCallback = d->m_jumpMap[keynum]->m_callback;
        QCoreApplication::postEvent(
            this, new QEvent(MythEvent::kExitToMainMenuEventType));
        return true;
    }

    if (d->m_keyContexts.value(context))
        d->m_keyContexts.value(context)->GetMapping(keynum, actions);

    if (context != "Global")
        d->m_keyContexts.value("Global")->GetMapping(keynum, actions);

    return false;
}

void MythMainWindow::ClearKey(const QString &context, const QString &action)
{
    KeyContext * keycontext = d->m_keyContexts.value(context);
    if (keycontext == nullptr) return;

    QMutableMapIterator<int, QStringList> it(keycontext->m_actionMap);
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
    KeyContext *keycontext = d->m_keyContexts.value(context);
    if (keycontext != nullptr)
        keycontext->m_actionMap.clear();
}

void MythMainWindow::BindKey(const QString &context, const QString &action,
                             const QString &key)
{
    QKeySequence keyseq(key);

    if (!d->m_keyContexts.contains(context))
        d->m_keyContexts.insert(context, new KeyContext());

    for (unsigned int i = 0; i < (uint)keyseq.count(); i++)
    {
        int keynum = keyseq[i];
        keynum &= ~Qt::UNICODE_ACCEL;

        QStringList dummyaction("");
        if (d->m_keyContexts.value(context)->GetMapping(keynum, dummyaction))
        {
            LOG(VB_GENERAL, LOG_WARNING,
                QString("Key %1 is bound to multiple actions in context %2.")
                    .arg(key).arg(context));
        }

        d->m_keyContexts.value(context)->AddMapping(keynum, action);
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, QString("Binding: %1 to action: %2 (%3)")
                                   .arg(key).arg(action).arg(context));
#endif

        if (action == "ESCAPE" && context == "Global" && i == 0)
            d->m_escapekey = keynum;
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
                LOG(VB_GENERAL, LOG_NOTICE,
                    "Updating keybinding description...");
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
            const QString& inskey = keybind;

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
    d->m_actionText[context][action] = description;
}

QString MythMainWindow::GetKey(const QString &context,
                               const QString &action)
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

QString MythMainWindow::GetActionText(const QString &context,
                                      const QString &action) const
{
    if (d->m_actionText.contains(context))
    {
        QHash<QString, QString> entry = d->m_actionText.value(context);
        if (entry.contains(action))
            return entry.value(action);
    }
    return "";
}

void MythMainWindow::ClearJump(const QString &destination)
{
    /* make sure that the jump point exists (using [] would add it)*/
    if (d->m_destinationMap.find(destination) == d->m_destinationMap.end())
    {
       LOG(VB_GENERAL, LOG_ERR,
           "Cannot clear ficticious jump point" + destination);
       return;
    }

    QMutableMapIterator<int, JumpData*> it(d->m_jumpMap);
    while (it.hasNext())
    {
        it.next();
        JumpData *jd = it.value();
        if (jd->m_destination == destination)
            it.remove();
    }
}


void MythMainWindow::BindJump(const QString &destination, const QString &key)
{
    /* make sure the jump point exists */
    if (d->m_destinationMap.find(destination) == d->m_destinationMap.end())
    {
       LOG(VB_GENERAL, LOG_ERR,
           "Cannot bind to ficticious jump point" + destination);
       return;
    }

    QKeySequence keyseq(key);

    for (unsigned int i = 0; i < (uint)keyseq.count(); i++)
    {
        int keynum = keyseq[i];
        keynum &= ~Qt::UNICODE_ACCEL;

        if (d->m_jumpMap.count(keynum) == 0)
        {
#if 0
            LOG(VB_GENERAL, LOG_DEBUG, QString("Binding: %1 to JumpPoint: %2")
                                       .arg(keybind).arg(destination));
#endif

            d->m_jumpMap[keynum] = &d->m_destinationMap[destination];
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING,
                QString("Key %1 is already bound to a jump point.").arg(key));
        }
    }
#if 0
    else
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("JumpPoint: %2 exists, no keybinding") .arg(destination));
#endif
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
            const QString& inskey = keybind;

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
        { callback, destination, description, exittomain, std::move(localAction) };
    d->m_destinationMap[destination] = jd;

    BindJump(destination, keybind);
}

void MythMainWindow::ClearAllJumps()
{
    QList<QString> destinations = d->m_destinationMap.keys();
    QList<QString>::Iterator it;
    for (it = destinations.begin(); it != destinations.end(); ++it)
        ClearJump(*it);
}

void MythMainWindow::JumpTo(const QString& destination, bool pop)
{
    if (d->m_destinationMap.count(destination) > 0 && d->m_exitMenuCallback == nullptr)
    {
        d->m_exitingtomain = true;
        d->m_popwindows = pop;
        d->m_exitMenuCallback = d->m_destinationMap[destination].m_callback;
        QCoreApplication::postEvent(
            this, new QEvent(MythEvent::kExitToMainMenuEventType));
        return;
    }
}

bool MythMainWindow::DestinationExists(const QString& destination) const
{
    return d->m_destinationMap.count(destination) > 0;
}

QStringList MythMainWindow::EnumerateDestinations(void) const
{
    return d->m_destinationMap.keys();
}

void MythMainWindow::RegisterMediaPlugin(const QString &name,
                                         const QString &desc,
                                         MediaPlayCallback fn)
{
    if (d->m_mediaPluginMap.count(name) == 0)
    {
        LOG(VB_GENERAL, LOG_NOTICE,
            QString("Registering %1 as a media playback plugin.").arg(name));
        MPData mpd = {desc, fn};
        d->m_mediaPluginMap[name] = mpd;
    }
    else
    {
        LOG(VB_GENERAL, LOG_NOTICE,
            QString("%1 is already registered as a media playback plugin.")
                .arg(name));
    }
}

bool MythMainWindow::HandleMedia(const QString &handler, const QString &mrl,
                                 const QString &plot, const QString &title,
                                 const QString &subtitle,
                                 const QString &director, int season,
                                 int episode, const QString &inetref,
                                 int lenMins, const QString &year,
                                 const QString &id, bool useBookmarks)
{
    QString lhandler(handler);
    if (lhandler.isEmpty())
        lhandler = "Internal";

    // Let's see if we have a plugin that matches the handler name...
    if (d->m_mediaPluginMap.count(lhandler))
    {
        d->m_mediaPluginMap[lhandler].m_playFn(mrl, plot, title, subtitle,
                                           director, season, episode,
                                           inetref, lenMins, year, id,
                                           useBookmarks);
        return true;
    }

    return false;
}

void MythMainWindow::HandleTVPower(bool poweron)
{
#ifdef USING_LIBCEC
    d->m_cecAdapter.Action((poweron) ? ACTION_TVPOWERON : ACTION_TVPOWEROFF);
#else
    (void) poweron;
#endif
}

void MythMainWindow::AllowInput(bool allow)
{
    d->m_allowInput = allow;
}

void MythMainWindow::mouseTimeout(void)
{
    /* complete the stroke if its our first timeout */
    if (d->m_gesture.recording())
    {
        d->m_gesture.stop();
    }

    /* get the last gesture */
    MythGestureEvent *e = d->m_gesture.gesture();

    if (e->gesture() < MythGestureEvent::Click)
        QCoreApplication::postEvent(this, e);
}

// Return code = true to skip further processing, false to continue
// sNewEvent: Caller must pass in a QScopedPointer that will be used
// to delete a new event if one is created.
bool MythMainWindow::keyLongPressFilter(QEvent **e,
    QScopedPointer<QEvent> &sNewEvent)
{
    QEvent *newEvent = nullptr;
    auto *ke = dynamic_cast<QKeyEvent*>(*e);
    if (!ke)
        return false;
    int keycode = ke->key();
    // Ignore unknown key codes
    if (keycode == 0)
        return false;

    switch ((*e)->type())
    {
        case QEvent::KeyPress:
        {
            // Check if we are in the middle of a long press
            if (keycode == d->m_longPressKeyCode)
            {
                if (ke->timestamp() - d->m_longPressTime < LONGPRESS_INTERVAL
                    || d->m_longPressTime == 0)
                {
                    // waiting for release of key.
                    return true; // discard the key press
                }

                // expired log press - generate long key
                newEvent = new QKeyEvent(QEvent::KeyPress, keycode,
                                         ke->modifiers() | Qt::MetaModifier, ke->nativeScanCode(),
                                         ke->nativeVirtualKey(), ke->nativeModifiers(),
                                         ke->text(), false,1);
                *e = newEvent;
                sNewEvent.reset(newEvent);
                d->m_longPressTime = 0;   // indicate we have generated the long press
                return false;
            }
            // If we got a keycode different from the long press keycode it
            // may have been injected by a jump point. Ignore it.
            if (d->m_longPressKeyCode != 0)
                return false;

            // Process start of possible new long press.
            d->m_longPressKeyCode = 0;
            QStringList actions;
            bool handled = TranslateKeyPress("Long Press",
                               ke, actions,false);
            if (handled)
            {
                // This shoudl never happen,, because we passed in false
                // to say do not process jump points and yet it returned true
                // to say it processed a jump point.
                LOG(VB_GUI, LOG_ERR, QString("TranslateKeyPress Long Press Invalid Response"));
                return true;
            }
            if (!actions.empty() && actions[0].startsWith("LONGPRESS"))
            {
                // Beginning of a press
                d->m_longPressKeyCode = keycode;
                d->m_longPressTime = ke->timestamp();
                return true; // discard the key press
            }
            break;
        }
        case QEvent::KeyRelease:
        {
            if (keycode == d->m_longPressKeyCode)
            {
                if (ke->isAutoRepeat())
                    return true;
                if (d->m_longPressTime > 0)
                {
                    // short press or non-repeating keyboard - generate key
                    Qt::KeyboardModifiers modifier = Qt::NoModifier;
                    if (ke->timestamp() - d->m_longPressTime >= LONGPRESS_INTERVAL)
                    {
                        // non-repeatng keyboard
                        modifier = Qt::MetaModifier;
                    }
                    newEvent = new QKeyEvent(QEvent::KeyPress, keycode,
                        ke->modifiers() | modifier, ke->nativeScanCode(),
                        ke->nativeVirtualKey(), ke->nativeModifiers(),
                        ke->text(), false,1);
                    *e = newEvent;
                    sNewEvent.reset(newEvent);
                    d->m_longPressKeyCode = 0;
                    return false;
                }

                // end of long press
                d->m_longPressKeyCode = 0;
                return true;
            }
            break;
        }
        default:
          break;
    }
    return false;
}

bool MythMainWindow::eventFilter(QObject * /*watched*/, QEvent *e)
{
    /* Don't let anything through if input is disallowed. */
    if (!d->m_allowInput)
        return true;

    QScopedPointer<QEvent> sNewEvent(nullptr);
    if (keyLongPressFilter(&e, sNewEvent))
        return true;

    switch (e->type())
    {
        case QEvent::KeyPress:
        {
            ResetIdleTimer();
            auto *ke = dynamic_cast<QKeyEvent*>(e);

            // Work around weird GCC run-time bug. Only manifest on Mac OS X
            if (!ke)
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
                ke = static_cast<QKeyEvent *>(e);

#ifdef Q_OS_LINUX
            // Fixups for _some_ linux native codes that QT doesn't know
            if (ke->key() <= 0)
            {
                int keycode = 0;
                switch(ke->nativeScanCode())
                {
                    case 209: // XF86AudioPause
                        keycode = Qt::Key_MediaPause;
                        break;
                    default:
                      break;
                }

                if (keycode > 0)
                {
                    auto *key = new QKeyEvent(QEvent::KeyPress, keycode,
                                              ke->modifiers());
                    QObject *key_target = getTarget(*key);
                    if (!key_target)
                        QCoreApplication::postEvent(this, key);
                    else
                        QCoreApplication::postEvent(key_target, key);

                    return true;
                }
            }
#endif

            for (auto *it = d->m_stackList.end()-1; it != d->m_stackList.begin()-1; --it)
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
            ResetIdleTimer();
            ShowMouseCursor(true);
            if (!d->m_gesture.recording())
            {
                d->m_gesture.start();
                auto *mouseEvent = dynamic_cast<QMouseEvent*>(e);
                if (!mouseEvent)
                    return false;
                d->m_gesture.record(mouseEvent->pos());

                /* start a single shot timer */
                d->m_gestureTimer->start(GESTURE_TIMEOUT);

                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease:
        {
            ResetIdleTimer();
            ShowMouseCursor(true);
            if (d->m_gestureTimer->isActive())
                d->m_gestureTimer->stop();

            if (d->m_gesture.recording())
            {
                d->m_gesture.stop();
                MythGestureEvent *ge = d->m_gesture.gesture();

                auto *mouseEvent = dynamic_cast<QMouseEvent*>(e);

                /* handle clicks separately */
                if (ge->gesture() == MythGestureEvent::Click)
                {
                    if (!mouseEvent)
                        return false;

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

                    for (auto *it = d->m_stackList.end()-1;
                         it != d->m_stackList.begin()-1;
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
                {
                    bool handled = false;
                    
                    if (!mouseEvent)
                    {
                        QCoreApplication::postEvent(this, ge);
                        return true;
                    }

                    QPoint p = mouseEvent->pos();

                    ge->SetPosition(p);
                    
                    for (auto *it = d->m_stackList.end()-1;
                         it != d->m_stackList.begin()-1;
                         --it)
                    {
                        MythScreenType *screen = (*it)->GetTopScreen();

                        if (!screen || !screen->ContainsPoint(p))
                            continue;

                        if (screen->gestureEvent(ge))
                        {
                            handled = true;
                            break;
                        }

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

                    if (handled)
                    {
                        delete ge;
                    }
                    else
                    {
                        QCoreApplication::postEvent(this, ge);
                    }
                }

                return true;
            }
            break;
        }
        case QEvent::MouseMove:
        {
            ResetIdleTimer();
            ShowMouseCursor(true);
            if (d->m_gesture.recording())
            {
                /* reset the timer */
                d->m_gestureTimer->stop();
                d->m_gestureTimer->start(GESTURE_TIMEOUT);

                auto *mouseEvent = dynamic_cast<QMouseEvent*>(e);
                if (!mouseEvent)
                    return false;
                d->m_gesture.record(mouseEvent->pos());
                return true;
            }
            break;
        }
        case QEvent::Wheel:
        {
            ResetIdleTimer();
            ShowMouseCursor(true);
            auto* qmw = dynamic_cast<QWheelEvent*>(e);
            if (qmw == nullptr)
                return false;
            int delta = qmw->delta();
            if (delta>0)
            {
                qmw->accept();
                auto *key = new QKeyEvent(QEvent::KeyPress, Qt::Key_Up,
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
                auto *key = new QKeyEvent(QEvent::KeyPress, Qt::Key_Down,
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
        auto *ge = dynamic_cast<MythGestureEvent*>(ce);
        if (ge == nullptr)
            return;
        MythScreenStack *toplevel = GetMainStack();
        if (toplevel)
        {
            MythScreenType *screen = toplevel->GetTopScreen();
            if (screen)
                screen->gestureEvent(ge);
        }
        LOG(VB_GUI, LOG_DEBUG, QString("Gesture: %1") .arg(QString(*ge)));
    }
    else if (ce->type() == MythEvent::kExitToMainMenuEventType &&
             d->m_exitingtomain)
    {
        ExitToMainMenu();
    }
    else if (ce->type() == ExternalKeycodeEvent::kEventType)
    {
        auto *eke = dynamic_cast<ExternalKeycodeEvent *>(ce);
        if (eke == nullptr)
            return;
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
             !d->m_ignoreLircKeys)
    {
        auto *lke = dynamic_cast<LircKeycodeEvent *>(ce);
        if (lke == nullptr)
            return;

        if (LircKeycodeEvent::kLIRCInvalidKeyCombo == lke->modifiers())
        {
            LOG(VB_GENERAL, LOG_WARNING,
                    QString("Attempt to convert LIRC key sequence '%1' "
                            "to a Qt key sequence failed.")
                    .arg(lke->lirctext()));
        }
        else
        {
            MythUIHelper::ResetScreensaver();
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
             !d->m_ignoreJoystickKeys)
    {
        auto *jke = dynamic_cast<JoystickKeycodeEvent *>(ce);
        if (jke == nullptr)
            return;

        int keycode = jke->getKeycode();
        if (keycode)
        {
            MythUIHelper::ResetScreensaver();
            if (GetMythUI()->GetScreenIsAsleep())
                return;

            Qt::KeyboardModifiers mod = Qt::KeyboardModifiers(keycode & Qt::MODIFIER_MASK);
            int k = (keycode & ~Qt::MODIFIER_MASK); /* trim off the mod */
            QString text;

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
            LOG(VB_GENERAL, LOG_WARNING,
                    QString("attempt to convert '%1' to a key sequence failed. "
                            "Fix your key mappings.")
                    .arg(jke->getJoystickMenuText()));
        }
    }
#endif
    else if (ce->type() == MythMediaEvent::kEventType)
    {
        auto *me = dynamic_cast<MythMediaEvent*>(ce);
        if (me == nullptr)
            return;

        // A listener based system might be more efficient, but we should never
        // have that many screens open at once so impact should be minimal.
        //
        // This approach is simpler for everyone to follow. Plugin writers
        // don't have to worry about adding their screens to the list because
        // all screens receive media events.
        //
        // Events are even sent to hidden or backgrounded screens, this avoids
        // the need for those to poll for changes when they become visible again
        // however this needs to be kept in mind if media changes trigger
        // actions which would not be appropriate when the screen doesn't have
        // focus. It is the programmers responsibility to ignore events when
        // necessary.
        foreach (auto & widget, d->m_stackList)
        {
            QVector<MythScreenType *> screenList;
            widget->GetScreenList(screenList);
            foreach (auto screen, screenList)
            {
                if (screen)
                    screen->mediaEvent(me);
            }
        }

        // Debugging
        MythMediaDevice *device = me->getDevice();
        if (device)
        {
            LOG(VB_GENERAL, LOG_DEBUG, QString("Media Event: %1 - %2")
                    .arg(device->getDevicePath()).arg(device->getStatus()));
        }
    }
    else if (ce->type() == ScreenSaverEvent::kEventType)
    {
        auto *sse = dynamic_cast<ScreenSaverEvent *>(ce);
        if (sse == nullptr)
            return;
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
                LOG(VB_GENERAL, LOG_ERR,
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
    else if (ce->type() == MythEvent::kLockInputDevicesEventType)
    {
        LockInputDevices(true);
        PauseIdleTimer(true);
    }
    else if (ce->type() == MythEvent::kUnlockInputDevicesEventType)
    {
        LockInputDevices(false);
        PauseIdleTimer(false);
    }
    else if (ce->type() == MythEvent::kDisableUDPListenerEventType)
    {
        d->m_udpListener->Disable();
    }
    else if (ce->type() == MythEvent::kEnableUDPListenerEventType)
    {
        d->m_udpListener->Enable();
    }
    else if (ce->type() == MythEvent::MythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(ce);
        if (me == nullptr)
            return;

        QString message = me->Message();
        if (message.startsWith(ACTION_HANDLEMEDIA))
        {
            if (me->ExtraDataCount() == 1)
                HandleMedia("Internal", me->ExtraData(0));
            else if (me->ExtraDataCount() >= 11)
            {
                bool usebookmark = true;
                if (me->ExtraDataCount() >= 12)
                    usebookmark = (me->ExtraData(11).toInt() != 0);
                HandleMedia("Internal", me->ExtraData(0),
                    me->ExtraData(1), me->ExtraData(2),
                    me->ExtraData(3), me->ExtraData(4),
                    me->ExtraData(5).toInt(), me->ExtraData(6).toInt(),
                    me->ExtraData(7), me->ExtraData(8).toInt(),
                    me->ExtraData(9), me->ExtraData(10),
                    usebookmark);
            }
            else
                LOG(VB_GENERAL, LOG_ERR, "Failed to handle media");
        }
        else if (message.startsWith(ACTION_SCREENSHOT))
        {
            int width = 0;
            int height = 0;
            QString filename;

            if (me->ExtraDataCount() >= 2)
            {
                width  = me->ExtraData(0).toInt();
                height = me->ExtraData(1).toInt();

                if (me->ExtraDataCount() == 3)
                    filename = me->ExtraData(2);
            }
            ScreenShot(width, height, filename);
        }
        else if (message == ACTION_GETSTATUS)
        {
            QVariantMap state;
            state.insert("state", "idle");
            state.insert("menutheme",
                 GetMythDB()->GetSetting("menutheme", "defaultmenu"));
            state.insert("currentlocation", GetMythUI()->GetCurrentLocation());
            MythUIStateTracker::SetState(state);
        }
        else if (message == "CLEAR_SETTINGS_CACHE")
        {
            // update the idle time
            d->m_idleTime = gCoreContext->GetNumSetting("FrontendIdleTimeout",
                                                      STANDBY_TIMEOUT);

            if (d->m_idleTime < 0)
                d->m_idleTime = 0;

            bool isActive = d->m_idleTimer->isActive();

            if (isActive)
                d->m_idleTimer->stop();

            if (d->m_idleTime > 0)
            {
                d->m_idleTimer->setInterval(1000 * 60 * d->m_idleTime);

                if (isActive)
                    d->m_idleTimer->start();

                LOG(VB_GENERAL, LOG_INFO, QString("Updating the frontend idle time to: %1 mins").arg(d->m_idleTime));
            }
            else
                LOG(VB_GENERAL, LOG_INFO, "Frontend idle timeout is disabled");
        }
        else if (message == "NOTIFICATION")
        {
            MythNotification mn(*me);
            MythNotificationCenter::GetInstance()->Queue(mn);
            return;
        }
        else if (message == "RECONNECT_SUCCESS" && d->m_standby)
        {
            // If the connection to the master backend has just been (re-)established
            // but we're in standby, make sure the backend is not blocked from
            // shutting down.
            gCoreContext->AllowShutdown();
        }
    }
    else if (ce->type() == MythEvent::MythUserMessage)
    {
        auto *me = dynamic_cast<MythEvent *>(ce);
        if (me == nullptr)
            return;

        const QString& message = me->Message();
        if (!message.isEmpty())
            ShowOkPopup(message);
    }
    else if (ce->type() == MythNotificationCenterEvent::kEventType)
    {
        GetNotificationCenter()->ProcessQueue();
    }
}

QObject *MythMainWindow::getTarget(QKeyEvent &key)
{
    QObject *key_target = nullptr;

    key_target = QWidget::keyboardGrabber();

    if (!key_target)
    {
        QWidget *focus_widget = qApp->focusWidget();
        if (focus_widget && focus_widget->isEnabled())
        {
            key_target = focus_widget;

            // Yes this is special code for handling the
            // the escape key.
            if (key.key() == d->m_escapekey && focus_widget->topLevelWidget())
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

#ifdef _WIN32
    // logicalDpiY not supported in Windows.
    int logicalDpiY = 100;
    HDC hdc = GetDC(nullptr);
    if (hdc)
    {
        logicalDpiY = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(nullptr, hdc);
    }
#else
    int logicalDpiY = this->logicalDpiY();
#endif

    // adjust for screen resolution relative to 100 dpi
    floatSize = floatSize * desired / logicalDpiY;
    // adjust for myth GUI size relative to 800x600
    floatSize = floatSize * d->m_hmult;
    // round to the nearest point size
    pointSize = lroundf(floatSize);

    return pointSize;
}

MythRect MythMainWindow::NormRect(const MythRect &rect)
{
    MythRect ret;
    ret.setWidth((int)(rect.width() * d->m_wmult));
    ret.setHeight((int)(rect.height() * d->m_hmult));
    ret.moveTopLeft(QPoint((int)(rect.x() * d->m_wmult),
                           (int)(rect.y() * d->m_hmult)));
    ret = ret.normalized();

    return ret;
}

QPoint MythMainWindow::NormPoint(const QPoint &point)
{
    QPoint ret;
    ret.setX((int)(point.x() * d->m_wmult));
    ret.setY((int)(point.y() * d->m_hmult));

    return ret;
}

QSize MythMainWindow::NormSize(const QSize &size)
{
    QSize ret;
    ret.setWidth((int)(size.width() * d->m_wmult));
    ret.setHeight((int)(size.height() * d->m_hmult));

    return ret;
}

int MythMainWindow::NormX(const int x)
{
    return qRound(x * d->m_wmult);
}

int MythMainWindow::NormY(const int y)
{
    return qRound(y * d->m_hmult);
}

void MythMainWindow::SetScalingFactors(float wmult, float hmult)
{
    d->m_wmult = wmult;
    d->m_hmult = hmult;
}

QRect MythMainWindow::GetUIScreenRect(void)
{
    return d->m_uiScreenRect;
}

void MythMainWindow::SetUIScreenRect(QRect &rect)
{
    d->m_uiScreenRect = rect;
}

int MythMainWindow::GetDrawInterval() const
{
    return d->m_drawInterval;
}

void MythMainWindow::StartLIRC(void)
{
#ifdef USE_LIRC
    if (d->m_lircThread)
    {
        d->m_lircThread->deleteLater();
        d->m_lircThread = nullptr;
    }

    QString config_file = GetConfDir() + "/lircrc";
    if (!QFile::exists(config_file))
        config_file = QDir::homePath() + "/.lircrc";

    /* lircd socket moved from /dev/ to /var/run/lirc/ in lirc 0.8.6 */
    QString lirc_socket = "/dev/lircd";
    if (!QFile::exists(lirc_socket))
        lirc_socket = "/var/run/lirc/lircd";

    d->m_lircThread = new LIRC(
        this,
        GetMythDB()->GetSetting("LircSocket", lirc_socket),
        "mythtv", config_file);

    if (d->m_lircThread->Init())
    {
        d->m_lircThread->start();
    }
    else
    {
        d->m_lircThread->deleteLater();
        d->m_lircThread = nullptr;
    }
#endif
}

void MythMainWindow::LockInputDevices( bool locked )
{
    if( locked )
        LOG(VB_GENERAL, LOG_INFO, "Locking input devices");
    else
        LOG(VB_GENERAL, LOG_INFO, "Unlocking input devices");

#ifdef USE_LIRC
    d->m_ignoreLircKeys = locked;
#endif

#ifdef USE_JOYSTICK_MENU
    d->m_ignoreJoystickKeys = locked;
#endif
}

void MythMainWindow::ShowMouseCursor(bool show)
{
    if (show && GetMythDB()->GetBoolSetting("HideMouseCursor", false))
        return;

    // Set cursor call must come after Show() to work on some systems.
    setCursor(show ? (Qt::ArrowCursor) : (Qt::BlankCursor));

    if (show)
        d->m_hideMouseTimer->start();
}

void MythMainWindow::HideMouseTimeout(void)
{
    ShowMouseCursor(false);
}

void MythMainWindow::ResetIdleTimer(void)
{
    if (d->m_disableIdle)
        return;

    if (d->m_idleTime == 0 ||
        !d->m_idleTimer->isActive() ||
        (d->m_standby && d->m_enteringStandby))
        return;

    if (d->m_standby)
        ExitStandby(false);

    QMetaObject::invokeMethod(d->m_idleTimer, "start");
}

void MythMainWindow::PauseIdleTimer(bool pause)
{
    if (d->m_disableIdle)
        return;

    // don't do anything if the idle timer is disabled
    if (d->m_idleTime == 0)
        return;

    if (pause)
    {
        LOG(VB_GENERAL, LOG_NOTICE, "Suspending idle timer");
        QMetaObject::invokeMethod(d->m_idleTimer, "stop");
    }
    else
    {
        LOG(VB_GENERAL, LOG_NOTICE, "Resuming idle timer");
        QMetaObject::invokeMethod(d->m_idleTimer, "start");
    }

    // ResetIdleTimer();
}

void MythMainWindow::IdleTimeout(void)
{
    if (d->m_disableIdle)
        return;

    d->m_enteringStandby = false;

    if (d->m_idleTime > 0 && !d->m_standby)
    {
        LOG(VB_GENERAL, LOG_NOTICE, QString("Entering standby mode after "
                                        "%1 minutes of inactivity")
                                        .arg(d->m_idleTime));
        EnterStandby(false);
        if (gCoreContext->GetNumSetting("idleTimeoutSecs", 0) > 0)
        {
            d->m_enteringStandby = true;
            JumpTo("Standby Mode");
        }
    }
}

void MythMainWindow::EnterStandby(bool manual)
{
    if (manual && d->m_enteringStandby)
        d->m_enteringStandby = false;

    if (d->m_standby)
        return;

    // We've manually entered standby mode and we want to pause the timer
    // to prevent it being Reset
    if (manual)
    {
        PauseIdleTimer(true);
        LOG(VB_GENERAL, LOG_NOTICE, QString("Entering standby mode"));
    }

    d->m_standby = true;
    gCoreContext->AllowShutdown();

    QVariantMap state;
    state.insert("state", "standby");
    state.insert("menutheme",
        GetMythDB()->GetSetting("menutheme", "defaultmenu"));
    state.insert("currentlocation", GetMythUI()->GetCurrentLocation());
    MythUIStateTracker::SetState(state);

    // Cache WOL settings in case DB goes down
    QString masterserver = gCoreContext->GetSetting
                    ("MasterServerName");
    gCoreContext->GetSettingOnHost
                    ("BackendServerAddr", masterserver);
    MythCoreContext::GetMasterServerPort();
    gCoreContext->GetSetting("WOLbackendCommand", "");

    // While in standby do not attempt to wake the backend
    gCoreContext->SetWOLAllowed(false);
}

void MythMainWindow::ExitStandby(bool manual)
{
    if (d->m_enteringStandby)
        return;

    if (manual)
        PauseIdleTimer(false);
    else if (gCoreContext->GetNumSetting("idleTimeoutSecs", 0) > 0)
        JumpTo("Main Menu");

    if (!d->m_standby)
        return;

    LOG(VB_GENERAL, LOG_NOTICE, "Leaving standby mode");

    d->m_standby = false;

    // We may attempt to wake the backend
    gCoreContext->SetWOLAllowed(true);

    gCoreContext->BlockShutdown();

    QVariantMap state;
    state.insert("state", "idle");
    state.insert("menutheme",
         GetMythDB()->GetSetting("menutheme", "defaultmenu"));
    state.insert("currentlocation", GetMythUI()->GetCurrentLocation());
    MythUIStateTracker::SetState(state);
}

void MythMainWindow::DisableIdleTimer(bool disableIdle)
{
    if ((d->m_disableIdle = disableIdle))
        QMetaObject::invokeMethod(d->m_idleTimer, "stop");
    else
        QMetaObject::invokeMethod(d->m_idleTimer, "start");
}

void MythMainWindow::onApplicationStateChange(Qt::ApplicationState state)
{
    LOG(VB_GENERAL, LOG_NOTICE, QString("Application State Changed to %1").arg(state));
    switch (state)
    {
        case Qt::ApplicationState::ApplicationActive:
            PopDrawDisabled();
            break;
        case Qt::ApplicationState::ApplicationSuspended:
            PushDrawDisabled();
            break;
        default:
            break;
    }
}
/* vim: set expandtab tabstop=4 shiftwidth=4: */
