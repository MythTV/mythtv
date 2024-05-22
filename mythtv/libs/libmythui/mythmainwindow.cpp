#include "mythmainwindow.h"
#include "mythmainwindowprivate.h"

// C headers
#include <cmath>

// C++ headers
#include <algorithm>
#include <chrono>
#include <utility>
#include <vector>

// QT
#include <QWaitCondition>
#include <QApplication>
#include <QHash>
#include <QFile>
#include <QDir>
#include <QEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QInputMethodEvent>
#include <QSize>
#include <QWindow>

// Platform headers
#include "unistd.h"

// libmythbase headers
#include "libmythbase/compat.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythevent.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmedia.h"
#include "libmythbase/mythmiscutil.h"

// libmythui headers
#include "myththemebase.h"
#include "mythudplistener.h"
#include "mythrender_base.h"
#include "mythuistatetracker.h"
#include "mythuiactions.h"
#include "mythrect.h"
#include "mythdisplay.h"
#include "mythscreentype.h"
#include "mythpainter.h"
#include "mythpainterwindow.h"
#include "mythgesture.h"
#include "mythuihelper.h"
#include "mythdialogbox.h"
#include "mythscreensaver.h"
#include "devices/mythinputdevicehandler.h"


#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#ifdef Q_OS_ANDROID
#include <QtAndroid>
#endif
#endif

static constexpr std::chrono::milliseconds GESTURE_TIMEOUT    { 1s    };
static constexpr std::chrono::minutes      STANDBY_TIMEOUT    { 90min };
static constexpr std::chrono::milliseconds LONGPRESS_INTERVAL { 1s    };

#define LOC QString("MythMainWindow: ")

static MythMainWindow *s_mainWin = nullptr;
static QMutex s_mainLock;

/**
 * \brief Return the existing main window, or create one
 * \param UseDB If this is a temporary window, which is used to bootstrap
 * the database, passing false prevents any database access.
 *
 * \sa    MythContextPrivate::TempMainWindow()
 */
MythMainWindow *MythMainWindow::getMainWindow(const bool UseDB)
{
    if (s_mainWin)
        return s_mainWin;

    QMutexLocker lock(&s_mainLock);

    if (!s_mainWin)
    {
        s_mainWin = new MythMainWindow(UseDB);
        gCoreContext->SetGUIObject(s_mainWin);
    }

    return s_mainWin;
}

void MythMainWindow::destroyMainWindow(void)
{
    if (gCoreContext)
        gCoreContext->SetGUIObject(nullptr);
    delete s_mainWin;
    s_mainWin = nullptr;
}

MythMainWindow *GetMythMainWindow(void)
{
    return MythMainWindow::getMainWindow();
}

bool HasMythMainWindow(void)
{
    return s_mainWin != nullptr;
}

void DestroyMythMainWindow(void)
{
    MythMainWindow::destroyMainWindow();
}

MythPainter *GetMythPainter(void)
{
    return MythMainWindow::getMainWindow()->GetPainter();
}

MythNotificationCenter *GetNotificationCenter(void)
{
    if (!s_mainWin || !s_mainWin->GetCurrentNotificationCenter())
        return nullptr;
    return s_mainWin->GetCurrentNotificationCenter();
}

MythMainWindow::MythMainWindow(const bool UseDB)
  : m_display(MythDisplay::Create(this))
{
    // Switch to desired GUI resolution
    if (m_display->UsingVideoModes())
        m_display->SwitchToGUI(true);

    m_deviceHandler = new MythInputDeviceHandler(this);
    connect(this, &MythMainWindow::SignalWindowReady, m_deviceHandler, &MythInputDeviceHandler::MainWindowReady);

    m_priv = new MythMainWindowPrivate;

    setObjectName("mainwindow");

    m_priv->m_allowInput = false;

    // This prevents database errors from RegisterKey() when there is no DB:
    m_priv->m_useDB = UseDB;

    //Init();

    m_priv->m_exitingtomain = false;
    m_priv->m_popwindows = true;
    m_priv->m_exitMenuCallback = nullptr;
    m_priv->m_exitMenuMediaDeviceCallback = nullptr;
    m_priv->m_mediaDeviceForCallback = nullptr;
    m_priv->m_escapekey = Qt::Key_Escape;
    m_priv->m_mainStack = nullptr;

    installEventFilter(this);

    MythUDP::EnableUDPListener();

    InitKeys();

    m_priv->m_gestureTimer = new QTimer(this);
    connect(m_priv->m_gestureTimer, &QTimer::timeout, this, &MythMainWindow::MouseTimeout);
    m_priv->m_hideMouseTimer = new QTimer(this);
    m_priv->m_hideMouseTimer->setSingleShot(true);
    m_priv->m_hideMouseTimer->setInterval(3s);
    connect(m_priv->m_hideMouseTimer, &QTimer::timeout, this, &MythMainWindow::HideMouseTimeout);
    m_priv->m_allowInput = true;
    connect(&m_refreshTimer, &QTimer::timeout, this, &MythMainWindow::Animate);
    m_refreshTimer.setInterval(17ms);
    m_refreshTimer.start();
    setUpdatesEnabled(true);

    connect(this, &MythMainWindow::SignalRemoteScreenShot,this, &MythMainWindow::DoRemoteScreenShot,
            Qt::BlockingQueuedConnection);
    connect(this, &MythMainWindow::SignalSetDrawEnabled, this, &MythMainWindow::SetDrawEnabled,
            Qt::BlockingQueuedConnection);
#ifdef Q_OS_ANDROID
    connect(qApp, &QApplication::applicationStateChanged, this, &MythMainWindow::OnApplicationStateChange);
#endif

    // We need to listen for playback start/end events
    gCoreContext->addListener(this);

    // Idle timer setup
    m_idleTime =
        gCoreContext->GetDurSetting<std::chrono::minutes>("FrontendIdleTimeout",
                                                         STANDBY_TIMEOUT);
    if (m_idleTime < 0min)
        m_idleTime = 0min;
    m_idleTimer.setInterval(m_idleTime);
    connect(&m_idleTimer, &QTimer::timeout, this, &MythMainWindow::IdleTimeout);
    if (m_idleTime > 0min)
        m_idleTimer.start();

    m_screensaver = new MythScreenSaverControl(this, m_display);
    if (m_screensaver)
    {
        connect(this, &MythMainWindow::SignalRestoreScreensaver, m_screensaver, &MythScreenSaverControl::Restore);
        connect(this, &MythMainWindow::SignalDisableScreensaver, m_screensaver, &MythScreenSaverControl::Disable);
        connect(this, &MythMainWindow::SignalResetScreensaver,   m_screensaver, &MythScreenSaverControl::Reset);
    }
}

MythMainWindow::~MythMainWindow()
{
    delete m_screensaver;

    if (gCoreContext != nullptr)
        gCoreContext->removeListener(this);

    MythUDP::StopUDPListener();

    while (!m_priv->m_stackList.isEmpty())
    {
        MythScreenStack *stack = m_priv->m_stackList.back();
        m_priv->m_stackList.pop_back();

        if (stack == m_priv->m_mainStack)
            m_priv->m_mainStack = nullptr;

        delete stack;
    }

    delete m_themeBase;

    for (auto iter = m_priv->m_keyContexts.begin();
         iter != m_priv->m_keyContexts.end();
         iter = m_priv->m_keyContexts.erase(iter))
    {
        KeyContext *context = *iter;
        delete context;
    }

    delete m_deviceHandler;
    delete m_priv->m_nc;

    MythPainterWindow::DestroyPainters(m_painterWin, m_painter);

    // N.B. we always call this to ensure the desktop mode is restored
    // in case the setting was changed
    m_display->SwitchToDesktop();
    delete m_display;

    delete m_priv;
}

MythDisplay* MythMainWindow::GetDisplay()
{
    return m_display;
}

MythPainter *MythMainWindow::GetPainter(void)
{
    return m_painter;
}

MythNotificationCenter *MythMainWindow::GetCurrentNotificationCenter(void)
{
    return m_priv->m_nc;
}

QWidget *MythMainWindow::GetPaintWindow(void)
{
    return m_painterWin;
}

void MythMainWindow::ShowPainterWindow(void)
{
    if (m_painterWin)
    {
        m_painterWin->show();
        m_painterWin->raise();
    }
}

void MythMainWindow::HidePainterWindow(void)
{
    if (m_painterWin)
    {
        m_painterWin->clearMask();
        if (!m_painterWin->RenderIsShared())
            m_painterWin->hide();
    }
}

MythRender *MythMainWindow::GetRenderDevice()
{
    return m_painterWin->GetRenderDevice();
}

void MythMainWindow::AddScreenStack(MythScreenStack* Stack, bool Main)
{
    m_priv->m_stackList.push_back(Stack);
    if (Main)
        m_priv->m_mainStack = Stack;
}

void MythMainWindow::PopScreenStack()
{
    MythScreenStack *stack = m_priv->m_stackList.back();
    m_priv->m_stackList.pop_back();
    if (stack == m_priv->m_mainStack)
        m_priv->m_mainStack = nullptr;
    delete stack;
}

int MythMainWindow::GetStackCount()
{
    return m_priv->m_stackList.size();
}

MythScreenStack *MythMainWindow::GetMainStack()
{
    return m_priv->m_mainStack;
}

MythScreenStack *MythMainWindow::GetStack(const QString& Stackname)
{
    for (auto * widget : std::as_const(m_priv->m_stackList))
        if (widget->objectName() == Stackname)
            return widget;
    return nullptr;
}

MythScreenStack* MythMainWindow::GetStackAt(int Position)
{
    if (Position >= 0 && Position < m_priv->m_stackList.size())
        return m_priv->m_stackList.at(Position);
    return nullptr;
}

void MythMainWindow::Animate(void)
{
    if (!(m_painterWin && updatesEnabled()))
        return;

    bool redraw = false;

    if (!m_repaintRegion.isEmpty())
        redraw = true;

    // The call to GetDrawOrder can apparently alter m_stackList.
    // NOLINTNEXTLINE(modernize-loop-convert,readability-qualified-auto) // both, qt6
    for (auto it = m_priv->m_stackList.begin(); it != m_priv->m_stackList.end(); ++it)
    {
        QVector<MythScreenType *> drawList;
        (*it)->GetDrawOrder(drawList);

        for (auto *screen : std::as_const(drawList))
        {
            screen->Pulse();

            if (screen->NeedsRedraw())
            {
                QRegion topDirty = screen->GetDirtyArea();
                screen->ResetNeedsRedraw();
                m_repaintRegion = m_repaintRegion.united(topDirty);
                redraw = true;
            }
        }
    }

    if (redraw && !m_painterWin->RenderIsShared())
        m_painterWin->update(m_repaintRegion);

    for (auto *widget : std::as_const(m_priv->m_stackList))
        widget->ScheduleInitIfNeeded();
}

void MythMainWindow::drawScreen(QPaintEvent* Event)
{
    if (!(m_painterWin && updatesEnabled()))
        return;

    if (Event)
        m_repaintRegion = m_repaintRegion.united(Event->region());

    if (!m_painter->SupportsClipping())
    {
        m_repaintRegion = m_repaintRegion.united(m_uiScreenRect);
    }
    else
    {
        // Ensure that the region is not larger than the screen which
        // can happen with bad themes
        m_repaintRegion = m_repaintRegion.intersected(m_uiScreenRect);

        // Check for any widgets that have been updated since we built
        // the dirty region list in ::animate()
        // The call to GetDrawOrder can apparently alter m_stackList.
        // NOLINTNEXTLINE(modernize-loop-convert,readability-qualified-auto) // both, qt6
        for (auto it = m_priv->m_stackList.begin(); it != m_priv->m_stackList.end(); ++it)
        {
            QVector<MythScreenType *> redrawList;
            (*it)->GetDrawOrder(redrawList);

            for (const auto *screen : std::as_const(redrawList))
            {
                if (screen->NeedsRedraw())
                {
                    for (const QRect& wrect: screen->GetDirtyArea())
                    {
                        bool foundThisRect = false;
                        for (const QRect& drect: m_repaintRegion)
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
                }
            }
        }
    }

    if (!m_painterWin->RenderIsShared())
        Draw();

    m_repaintRegion = QRegion();
}

void MythMainWindow::Draw(MythPainter* Painter)
{
    if (!Painter)
        Painter = m_painter;
    if (!Painter)
        return;

    Painter->Begin(m_painterWin);

    if (!Painter->SupportsClipping())
        m_repaintRegion = QRegion(m_uiScreenRect);

    for (const QRect& rect : m_repaintRegion)
    {
        if (rect.width() == 0 || rect.height() == 0)
            continue;

        if (rect != m_uiScreenRect)
            Painter->SetClipRect(rect);

        // The call to GetDrawOrder can apparently alter m_stackList.
        // NOLINTNEXTLINE(modernize-loop-convert,readability-qualified-auto) // both, qt6
        for (auto it = m_priv->m_stackList.begin(); it != m_priv->m_stackList.end(); ++it)
        {
            QVector<MythScreenType *> redrawList;
            (*it)->GetDrawOrder(redrawList);
            for (auto *screen : std::as_const(redrawList))
                screen->Draw(Painter, 0, 0, 255, rect);
        }
    }

    Painter->End();
    m_repaintRegion = QRegion();
}

// virtual
QPaintEngine *MythMainWindow::paintEngine() const
{
    return testAttribute(Qt::WA_PaintOnScreen) ? nullptr : QWidget::paintEngine();
}

void MythMainWindow::closeEvent(QCloseEvent* Event)
{
    if (Event->spontaneous())
    {
        auto * key = new QKeyEvent(QEvent::KeyPress, m_priv->m_escapekey, Qt::NoModifier);
        QCoreApplication::postEvent(this, key);
        Event->ignore();
        return;
    }

    QWidget::closeEvent(Event);
}

void MythMainWindow::GrabWindow(QImage& Image)
{
    WId winid = 0;
    auto * active = QApplication::activeWindow();
    if (active)
    {
        winid = active->winId();
    }
    else
    {
        // According to the following we page, you "just pass 0 as the
        // window id, indicating that we want to grab the entire screen".
        //
        // https://doc.qt.io/qt-5/qtwidgets-desktop-screenshot-example.html#screenshot-class-implementation
        winid = 0;
    }

    auto * display = GetMythMainWindow()->GetDisplay();
    if (auto * screen = display->GetCurrentScreen(); screen)
    {
        QPixmap image = screen->grabWindow(winid);
        Image = image.toImage();
    }
}

/* This is required to allow a screenshot to be requested by another thread
 * other than the UI thread, and to wait for the screenshot before returning.
 * It is used by mythweb for the remote access screenshots
 */
void MythMainWindow::DoRemoteScreenShot(const QString& Filename, int Width, int Height)
{
    // This will be running in the UI thread, as is required by QPixmap
    QStringList args;
    args << QString::number(Width);
    args << QString::number(Height);
    args << Filename;
    MythEvent me(MythEvent::kMythEventMessage, ACTION_SCREENSHOT, args);
    QCoreApplication::sendEvent(this, &me);
}

void MythMainWindow::RemoteScreenShot(QString Filename, int Width, int Height)
{
    // This will be running in a non-UI thread and is used to trigger a
    // function in the UI thread, and waits for completion of that handler
    emit SignalRemoteScreenShot(std::move(Filename), Width, Height);
}

bool MythMainWindow::SaveScreenShot(const QImage& Image, QString Filename)
{
    if (Filename.isEmpty())
    {
        QString fpath = GetMythDB()->GetSetting("ScreenShotPath", "/tmp");
        Filename = QString("%1/myth-screenshot-%2.png")
            .arg(fpath, MythDate::toString(MythDate::current(), MythDate::kScreenShotFilename));
    }

    QString extension = Filename.section('.', -1, -1);
    if (extension == "jpg")
        extension = "JPEG";
    else
        extension = "PNG";

    LOG(VB_GENERAL, LOG_INFO, QString("Saving screenshot to %1 (%2x%3)")
                       .arg(Filename).arg(Image.width()).arg(Image.height()));

    if (Image.save(Filename, extension.toLatin1(), 100))
    {
        LOG(VB_GENERAL, LOG_INFO, "MythMainWindow::screenShot succeeded");
        return true;
    }

    LOG(VB_GENERAL, LOG_INFO, "MythMainWindow::screenShot Failed!");
    return false;
}

bool MythMainWindow::ScreenShot(int Width, int Height, QString Filename)
{
    QImage img;
    GrabWindow(img);
    if (Width <= 0)
        Width = img.width();
    if (Height <= 0)
        Height = img.height();
    img = img.scaled(Width, Height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return SaveScreenShot(img, std::move(Filename));
}

void MythMainWindow::RestoreScreensaver()
{
    if (HasMythMainWindow())
        emit GetMythMainWindow()->SignalRestoreScreensaver();
}

void MythMainWindow::DisableScreensaver()
{
    if (HasMythMainWindow())
        emit GetMythMainWindow()->SignalDisableScreensaver();
}

void MythMainWindow::ResetScreensaver()
{
    if (HasMythMainWindow())
        emit GetMythMainWindow()->SignalResetScreensaver();
}

bool MythMainWindow::IsScreensaverAsleep()
{
    if (HasMythMainWindow())
    {
        MythMainWindow* window = GetMythMainWindow();
        if (window->m_screensaver)
            return window->m_screensaver->Asleep();
    }
    return false;
}

bool MythMainWindow::IsTopScreenInitialized()
{
    if (HasMythMainWindow())
        return GetMythMainWindow()->GetMainStack()->GetTopScreen()->IsInitialized();
    return false;
}

bool MythMainWindow::event(QEvent *Event)
{
    if (!updatesEnabled() && (Event->type() == QEvent::UpdateRequest))
        m_priv->m_pendingUpdate = true;

    if (Event->type() == QEvent::Show && !Event->spontaneous())
        QCoreApplication::postEvent(this, new QEvent(MythEvent::kMythPostShowEventType));

    if (Event->type() == MythEvent::kMythPostShowEventType)
    {
        raise();
        activateWindow();
        return true;
    }

    if ((Event->type() == QEvent::WindowActivate) || (Event->type() == QEvent::WindowDeactivate))
        m_deviceHandler->Event(Event);

    return QWidget::event(Event);
}

void MythMainWindow::LoadQtConfig()
{
    gCoreContext->ResetLanguage();
    gCoreContext->ResetAudioLanguage();
    GetMythUI()->ClearThemeCacheDir();
    QApplication::setStyle("Windows");
}

void MythMainWindow::Init(bool MayReInit)
{
    LoadQtConfig();
    m_display->SetWidget(nullptr);
    m_priv->m_useDB = ! gCoreContext->GetDB()->SuppressDBMessages();

    if (!(MayReInit || m_priv->m_firstinit))
        return;

    // Set window border based on fullscreen attribute
    Qt::WindowFlags flags = Qt::Window;

    InitScreenBounds();
    bool inwindow   = m_wantWindow && !m_qtFullScreen;
    bool fullscreen = m_wantFullScreen && !GeometryIsOverridden();

    // On Compiz/Unit, when the window is fullscreen and frameless changing
    // screen position ends up stuck. Adding a border temporarily prevents this
    setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);

    if (!inwindow)
    {
        LOG(VB_GENERAL, LOG_INFO, "Using Frameless Window");
        flags |= Qt::FramelessWindowHint;
    }

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
        if (m_priv->m_firstinit)
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

    if (m_alwaysOnTop && !WindowIsAlwaysFullscreen())
        flags |= Qt::WindowStaysOnTopHint;

    setWindowFlags(flags);

    // SetWidget may move the widget into a new screen.
    m_display->SetWidget(this);
    QTimer::singleShot(1s, this, &MythMainWindow::DelayedAction);

    // Ensure we have latest screen bounds if we have moved
    UpdateScreenSettings(m_display);
    SetUIScreenRect({{0, 0}, m_screenRect.size()});
    MoveResize(m_screenRect);
    Show();

    // The window is sometimes not created until Show has been called - so try
    // MythDisplay::setWidget again to ensure we listen for QScreen changes
    m_display->SetWidget(this);

    if (!GetMythDB()->GetBoolSetting("HideMouseCursor", false))
        setMouseTracking(true); // Required for mouse cursor auto-hide
    // Set cursor call must come after Show() to work on some systems.
    ShowMouseCursor(false);

    move(m_screenRect.topLeft());

    if (m_painterWin || m_painter)
    {
        LOG(VB_GENERAL, LOG_INFO, "Destroying painter and painter window");
        MythPainterWindow::DestroyPainters(m_painterWin, m_painter);
    }

    QString warningmsg = MythPainterWindow::CreatePainters(this, m_painterWin, m_painter);
    if (!warningmsg.isEmpty())
    {
        LOG(VB_GENERAL, LOG_WARNING, warningmsg);
    }

    if (!m_painterWin)
    {
        LOG(VB_GENERAL, LOG_ERR, "MythMainWindow failed to create a painter window.");
        return;
    }

    if (m_painter && m_painter->GetName() != "Qt")
    {
        setAttribute(Qt::WA_NoSystemBackground);
        setAutoFillBackground(false);
    }
    setAttribute(Qt::WA_InputMethodEnabled);

    MoveResize(m_screenRect);
    ShowPainterWindow();

    // Redraw the window now to avoid race conditions in EGLFS (Qt5.4) if a
    // 2nd window (e.g. TVPlayback) is created before this is redrawn.
#ifdef Q_OS_ANDROID
    static const QLatin1String EARLY_SHOW_PLATFORM_NAME_CHECK { "android" };
#else
    static const QLatin1String EARLY_SHOW_PLATFORM_NAME_CHECK { "egl" };
#endif
    if (QGuiApplication::platformName().contains(EARLY_SHOW_PLATFORM_NAME_CHECK))
        QCoreApplication::processEvents();

    if (!GetMythDB()->GetBoolSetting("HideMouseCursor", false))
        m_painterWin->setMouseTracking(true); // Required for mouse cursor auto-hide

    GetMythUI()->UpdateImageCache();
    if (m_themeBase)
        m_themeBase->Reload();
    else
        m_themeBase = new MythThemeBase(this);

    if (!m_priv->m_nc)
        m_priv->m_nc = new MythNotificationCenter();

    emit SignalWindowReady();

    if (!warningmsg.isEmpty())
    {
        MythNotification notification(warningmsg, "");
        m_priv->m_nc->Queue(notification);
    }
}

void MythMainWindow::DelayedAction()
{
    MoveResize(m_screenRect);
    Show();

#ifdef Q_OS_ANDROID
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QtAndroid::hideSplashScreen();
#else
    QNativeInterface::QAndroidApplication::hideSplashScreen();
#endif
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
        "Escape"),                "Esc,Back");
    RegisterKey("Global", "MENU", QT_TRANSLATE_NOOP("MythControls",
        "Pop-up menu"),             "M,Meta+Enter,Ctrl+M,Menu");
    RegisterKey("Global", "INFO", QT_TRANSLATE_NOOP("MythControls",
        "More information"),        "I,Ctrl+I,Home Page");
    RegisterKey("Global", "DELETE", QT_TRANSLATE_NOOP("MythControls",
        "Delete"),                  "D,Ctrl+E");
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
        "Previous View"),        "Home,Media Previous");
    RegisterKey("Global", "NEXTVIEW", QT_TRANSLATE_NOOP("MythControls",
        "Next View"),             "End,Media Next");

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
        "Show incremental search dialog"), "Ctrl+S,Search");

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
        "Zoom in on browser window"),           ".,>,Ctrl+F,Media Fast Forward");
    RegisterKey("Browser", "ZOOMOUT",         QT_TRANSLATE_NOOP("MythControls",
        "Zoom out on browser window"),          ",,<,Ctrl+B,Media Rewind");
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
        "Display System Exit Prompt"),      "Esc,Back");
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

void MythMainWindow::Show()
{
    bool inwindow = m_wantWindow && !m_qtFullScreen;
    bool fullscreen = m_wantFullScreen && !GeometryIsOverridden();
    if (fullscreen && !inwindow && !m_priv->m_firstinit)
        showFullScreen();
    else
        show();
    m_priv->m_firstinit = false;
}

void MythMainWindow::MoveResize(QRect& Geometry)
{
    setFixedSize(Geometry.size());
    setGeometry(Geometry);
    if (m_painterWin)
    {
        m_painterWin->setFixedSize(Geometry.size());
        m_painterWin->setGeometry(0, 0, Geometry.width(), Geometry.height());
    }
}

uint MythMainWindow::PushDrawDisabled()
{
    QMutexLocker locker(&m_priv->m_drawDisableLock);
    m_priv->m_drawDisabledDepth++;
    if (m_priv->m_drawDisabledDepth && updatesEnabled())
        SetDrawEnabled(false);
    return m_priv->m_drawDisabledDepth;
}

uint MythMainWindow::PopDrawDisabled()
{
    QMutexLocker locker(&m_priv->m_drawDisableLock);
    if (m_priv->m_drawDisabledDepth)
    {
        m_priv->m_drawDisabledDepth--;
        if (!m_priv->m_drawDisabledDepth && !updatesEnabled())
            SetDrawEnabled(true);
    }
    return m_priv->m_drawDisabledDepth;
}

void MythMainWindow::SetDrawEnabled(bool Enable)
{
    if (!gCoreContext->IsUIThread())
    {
        emit SignalSetDrawEnabled(Enable);
        return;
    }

    setUpdatesEnabled(Enable);
    if (Enable)
    {
        if (m_priv->m_pendingUpdate)
        {
            QApplication::postEvent(this, new QEvent(QEvent::UpdateRequest), Qt::LowEventPriority);
            m_priv->m_pendingUpdate = false;
        }
        ShowPainterWindow();
    }
    else
    {
        HidePainterWindow();
    }
}

void MythMainWindow::SetEffectsEnabled(bool Enable)
{
    for (auto *widget : std::as_const(m_priv->m_stackList))
    {
        if (Enable)
            widget->EnableEffects();
        else
            widget->DisableEffects();
    }
}

bool MythMainWindow::IsExitingToMain() const
{
    return m_priv->m_exitingtomain;
}

void MythMainWindow::ExitToMainMenu(void)
{
    bool jumpdone = !(m_priv->m_popwindows);

    m_priv->m_exitingtomain = true;

    MythScreenStack *toplevel = GetMainStack();
    if (toplevel && m_priv->m_popwindows)
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
                auto *key = new QKeyEvent(QEvent::KeyPress, m_priv->m_escapekey,
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
        m_priv->m_exitingtomain = false;
        m_priv->m_popwindows = true;
        if (m_priv->m_exitMenuCallback)
        {
            void (*callback)(void) = m_priv->m_exitMenuCallback;
            m_priv->m_exitMenuCallback = nullptr;
            callback();
        }
        else if (m_priv->m_exitMenuMediaDeviceCallback)
        {
            void (*callback)(MythMediaDevice*) = m_priv->m_exitMenuMediaDeviceCallback;
            MythMediaDevice * mediadevice = m_priv->m_mediaDeviceForCallback;
            m_priv->m_mediaDeviceForCallback = nullptr;
            callback(mediadevice);
        }
    }
}

/**
 * \brief Get a list of actions for a keypress in the given context
 * \param Context The context in which to lookup the keypress for actions.
 * \param Event   The keypress event to lookup.
 * \param Actions The QStringList that will contain the list of actions.
 * \param AllowJumps if true then jump points are allowed
 *
 * \return true if the key event has been handled (the keypress was a jumpoint)
           false if the caller should continue to handle keypress
 */
bool MythMainWindow::TranslateKeyPress(const QString& Context, QKeyEvent* Event,
                                       QStringList& Actions, bool AllowJumps)
{
    Actions.clear();

    // Special case for custom QKeyEvent where the action is embedded directly
    // in the QKeyEvent text property. Used by MythFEXML http extension
    if (Event && Event->key() == 0 &&
        !Event->text().isEmpty() &&
        Event->modifiers() == Qt::NoModifier)
    {
        QString action = Event->text();
        // check if it is a jumppoint
        if (!m_priv->m_destinationMap.contains(action))
        {
            Actions.append(action);
            return false;
        }

        if (AllowJumps)
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

    int keynum = MythMainWindowPrivate::TranslateKeyNum(Event);

    QStringList localActions;
    auto * keycontext = m_priv->m_keyContexts.value(Context);
    if (AllowJumps && (m_priv->m_jumpMap.count(keynum) > 0) &&
        (!m_priv->m_jumpMap[keynum]->m_localAction.isEmpty()) &&
        keycontext && (keycontext->GetMapping(keynum, localActions)))
    {
        if (localActions.contains(m_priv->m_jumpMap[keynum]->m_localAction))
            AllowJumps = false;
    }

    if (AllowJumps && m_priv->m_jumpMap.count(keynum) > 0 &&
            !m_priv->m_jumpMap[keynum]->m_exittomain && m_priv->m_exitMenuCallback == nullptr)
    {
        void (*callback)(void) = m_priv->m_jumpMap[keynum]->m_callback;
        callback();
        return true;
    }

    if (AllowJumps &&
        m_priv->m_jumpMap.count(keynum) > 0 && m_priv->m_exitMenuCallback == nullptr)
    {
        m_priv->m_exitingtomain = true;
        m_priv->m_exitMenuCallback = m_priv->m_jumpMap[keynum]->m_callback;
        QCoreApplication::postEvent(
            this, new QEvent(MythEvent::kExitToMainMenuEventType));
        return true;
    }

    if (keycontext)
        keycontext->GetMapping(keynum, Actions);

    if (Context != "Global")
    {
        auto * keycontextG = m_priv->m_keyContexts.value("Global");
        if (keycontextG)
            keycontextG->GetMapping(keynum, Actions);
    }

    return false;
}

void MythMainWindow::ClearKey(const QString& Context, const QString& Action)
{
    auto * keycontext = m_priv->m_keyContexts.value(Context);
    if (keycontext == nullptr)
        return;

    QMutableMapIterator<int, QStringList> it(keycontext->m_actionMap);
    while (it.hasNext())
    {
        it.next();
        QStringList list = it.value();
        list.removeAll(Action);
        if (list.isEmpty())
            it.remove();
    }
}

void MythMainWindow::ClearKeyContext(const QString& Context)
{
    auto * keycontext = m_priv->m_keyContexts.value(Context);
    if (keycontext != nullptr)
        keycontext->m_actionMap.clear();
}

void MythMainWindow::BindKey(const QString& Context, const QString& Action, const QString& Key)
{
    auto * keycontext = m_priv->m_keyContexts.value(Context);
    if (keycontext == nullptr)
    {
        keycontext = new KeyContext();
        if (keycontext == nullptr)
            return;
        m_priv->m_keyContexts.insert(Context, keycontext);
    }

    QKeySequence keyseq(Key);
    for (unsigned int i = 0; i < static_cast<uint>(keyseq.count()); i++)
    {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        int keynum = keyseq[i];
#else
        int keynum = keyseq[i].toCombined();
#endif

        QStringList dummyaction("");
        if (keycontext->GetMapping(keynum, dummyaction))
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("Key %1 is bound to multiple actions in context %2.")
                .arg(Key, Context));
        }

        keycontext->AddMapping(keynum, Action);
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, QString("Binding: %1 to action: %2 (%3)")
                                   .arg(Key).arg(Action).arg(Context));
#endif

        if (Action == "ESCAPE" && Context == "Global" && i == 0)
            m_priv->m_escapekey = keynum;
    }
}

void MythMainWindow::RegisterKey(const QString& Context, const QString& Action,
                                 const QString& Description, const QString& Key)
{
    QString keybind = Key;

    MSqlQuery query(MSqlQuery::InitCon());

    if (m_priv->m_useDB && query.isConnected())
    {
        query.prepare("SELECT keylist, description FROM keybindings WHERE "
                      "context = :CONTEXT AND action = :ACTION AND "
                      "hostname = :HOSTNAME ;");
        query.bindValue(":CONTEXT", Context);
        query.bindValue(":ACTION", Action);
        query.bindValue(":HOSTNAME", GetMythDB()->GetHostName());

        if (query.exec() && query.next())
        {
            keybind = query.value(0).toString();
            QString db_description = query.value(1).toString();

            // Update keybinding description if changed
            if (db_description != Description)
            {
                LOG(VB_GENERAL, LOG_NOTICE,
                    "Updating keybinding description...");
                query.prepare(
                    "UPDATE keybindings "
                    "SET description = :DESCRIPTION "
                    "WHERE context   = :CONTEXT AND "
                    "      action    = :ACTION  AND "
                    "      hostname  = :HOSTNAME");

                query.bindValue(":DESCRIPTION", Description);
                query.bindValue(":CONTEXT",     Context);
                query.bindValue(":ACTION",      Action);
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
            query.bindValue(":CONTEXT", Context);
            query.bindValue(":ACTION", Action);
            query.bindValue(":DESCRIPTION", Description);
            query.bindValue(":KEYLIST", inskey);
            query.bindValue(":HOSTNAME", GetMythDB()->GetHostName());

            if (!query.exec() && !(GetMythDB()->SuppressDBMessages()))
            {
                MythDB::DBError("Insert Keybinding", query);
            }
        }
    }

    BindKey(Context, Action, keybind);
    m_priv->m_actionText[Context][Action] = Description;
}

QString MythMainWindow::GetKey(const QString& Context, const QString& Action)
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (!query.isConnected())
        return "?";

    query.prepare("SELECT keylist "
                  "FROM keybindings "
                  "WHERE context  = :CONTEXT AND "
                  "      action   = :ACTION  AND "
                  "      hostname = :HOSTNAME");
    query.bindValue(":CONTEXT", Context);
    query.bindValue(":ACTION", Action);
    query.bindValue(":HOSTNAME", GetMythDB()->GetHostName());

    if (!query.exec() || !query.isActive() || !query.next())
        return "?";

    return query.value(0).toString();
}

QString MythMainWindow::GetActionText(const QString& Context,
                                      const QString& Action) const
{
    if (m_priv->m_actionText.contains(Context))
    {
        QHash<QString, QString> entry = m_priv->m_actionText.value(Context);
        if (entry.contains(Action))
            return entry.value(Action);
    }
    return "";
}

void MythMainWindow::ClearJump(const QString& Destination)
{
    // make sure that the jump point exists (using [] would add it)
    if (!m_priv->m_destinationMap.contains(Destination))
    {
       LOG(VB_GENERAL, LOG_ERR, "Cannot clear ficticious jump point" + Destination);
       return;
    }

    QMutableMapIterator<int, JumpData*> it(m_priv->m_jumpMap);
    while (it.hasNext())
    {
        it.next();
        JumpData *jd = it.value();
        if (jd->m_destination == Destination)
            it.remove();
    }
}


void MythMainWindow::BindJump(const QString& Destination, const QString& Key)
{
    // make sure the jump point exists
    if (!m_priv->m_destinationMap.contains(Destination))
    {
       LOG(VB_GENERAL, LOG_ERR, "Cannot bind to ficticious jump point" + Destination);
       return;
    }

    QKeySequence keyseq(Key);

    for (unsigned int i = 0; i < static_cast<uint>(keyseq.count()); i++)
    {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
        int keynum = keyseq[i];
#else
        int keynum = keyseq[i].toCombined();
#endif

        if (m_priv->m_jumpMap.count(keynum) == 0)
        {
#if 0
            LOG(VB_GENERAL, LOG_DEBUG, QString("Binding: %1 to JumpPoint: %2")
                                       .arg(keybind).arg(destination));
#endif

            m_priv->m_jumpMap[keynum] = &m_priv->m_destinationMap[Destination];
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, QString("Key %1 is already bound to a jump point.")
                .arg(Key));
        }
    }
#if 0
    else
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("JumpPoint: %2 exists, no keybinding") .arg(destination));
#endif
}

void MythMainWindow::RegisterJump(const QString& Destination, const QString& Description,
                                  const QString& Key, void (*Callback)(void),
                                  bool Exittomain, QString LocalAction)
{
    QString keybind = Key;

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        query.prepare("SELECT keylist FROM jumppoints WHERE destination = :DEST and hostname = :HOST ;");
        query.bindValue(":DEST", Destination);
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
            query.bindValue(":DEST", Destination);
            query.bindValue(":DESC", Description);
            query.bindValue(":KEYLIST", inskey);
            query.bindValue(":HOST", GetMythDB()->GetHostName());
            if (!query.exec() || !query.isActive())
                MythDB::DBError("Insert Jump Point", query);
        }
    }

    JumpData jd = { Callback, Destination, Description, Exittomain, std::move(LocalAction) };
    m_priv->m_destinationMap[Destination] = jd;
    BindJump(Destination, keybind);
}

void MythMainWindow::ClearAllJumps()
{
    QList<QString> destinations = m_priv->m_destinationMap.keys();
    QList<QString>::Iterator it;
    for (it = destinations.begin(); it != destinations.end(); ++it)
        ClearJump(*it);
}

void MythMainWindow::JumpTo(const QString& Destination, bool Pop)
{
    if (m_priv->m_destinationMap.count(Destination) > 0 && m_priv->m_exitMenuCallback == nullptr)
    {
        m_priv->m_exitingtomain = true;
        m_priv->m_popwindows = Pop;
        m_priv->m_exitMenuCallback = m_priv->m_destinationMap[Destination].m_callback;
        QCoreApplication::postEvent(
            this, new QEvent(MythEvent::kExitToMainMenuEventType));
        return;
    }
}

bool MythMainWindow::DestinationExists(const QString& Destination) const
{
    return m_priv->m_destinationMap.count(Destination) > 0;
}

QStringList MythMainWindow::EnumerateDestinations() const
{
    return m_priv->m_destinationMap.keys();
}

void MythMainWindow::RegisterMediaPlugin(const QString& Name, const QString& Desc,
                                         MediaPlayCallback Func)
{
    if (m_priv->m_mediaPluginMap.count(Name) == 0)
    {
        LOG(VB_GENERAL, LOG_NOTICE, QString("Registering %1 as a media playback plugin.")
            .arg(Name));
        m_priv->m_mediaPluginMap[Name] = { Desc, Func };
    }
    else
    {
        LOG(VB_GENERAL, LOG_NOTICE, QString("%1 is already registered as a media playback plugin.")
                .arg(Name));
    }
}

bool MythMainWindow::HandleMedia(const QString& Handler, const QString& Mrl,
                                 const QString& Plot, const QString& Title,
                                 const QString& Subtitle,
                                 const QString& Director, int Season,
                                 int Episode, const QString& Inetref,
                                 std::chrono::minutes LenMins, const QString& Year,
                                 const QString& Id, bool UseBookmarks)
{
    QString lhandler(Handler);
    if (lhandler.isEmpty())
        lhandler = "Internal";

    // Let's see if we have a plugin that matches the handler name...
    if (m_priv->m_mediaPluginMap.contains(lhandler))
    {
        m_priv->m_mediaPluginMap[lhandler].second(Mrl, Plot, Title, Subtitle,
                                                  Director, Season, Episode,
                                                  Inetref, LenMins, Year, Id,
                                                  UseBookmarks);
        return true;
    }

    return false;
}

void MythMainWindow::HandleTVAction(const QString& Action)
{
    m_deviceHandler->Action(Action);
}

void MythMainWindow::AllowInput(bool Allow)
{
    m_priv->m_allowInput = Allow;
}

void MythMainWindow::MouseTimeout()
{
    // complete the stroke if its our first timeout
    if (m_priv->m_gesture.Recording())
        m_priv->m_gesture.Stop(true);

    // get the last gesture
    auto * event = m_priv->m_gesture.GetGesture();
    if (event->GetGesture() < MythGestureEvent::Click)
        QCoreApplication::postEvent(this, event);
}

// Return code = true to skip further processing, false to continue
// sNewEvent: Caller must pass in a QScopedPointer that will be used
// to delete a new event if one is created.
bool MythMainWindow::KeyLongPressFilter(QEvent** Event, QScopedPointer<QEvent>& NewEvent)
{
    auto * keyevent = dynamic_cast<QKeyEvent*>(*Event);
    if (!keyevent)
        return false;
    int keycode = keyevent->key();
    // Ignore unknown key codes
    if (keycode == 0)
        return false;

    QEvent *newevent = nullptr;
    switch ((*Event)->type())
    {
        case QEvent::KeyPress:
        {
            // Check if we are in the middle of a long press
            if (keycode == m_priv->m_longPressKeyCode)
            {
                if (std::chrono::milliseconds(keyevent->timestamp()) - m_priv->m_longPressTime < LONGPRESS_INTERVAL
                    || m_priv->m_longPressTime == 0ms)
                {
                    // waiting for release of key.
                    return true; // discard the key press
                }

                // expired log press - generate long key
                newevent = new QKeyEvent(QEvent::KeyPress, keycode,
                                         keyevent->modifiers() | Qt::MetaModifier, keyevent->nativeScanCode(),
                                         keyevent->nativeVirtualKey(), keyevent->nativeModifiers(),
                                         keyevent->text(), false,1);
                *Event = newevent;
                NewEvent.reset(newevent);
                m_priv->m_longPressTime = 0ms;   // indicate we have generated the long press
                return false;
            }
            // If we got a keycode different from the long press keycode it
            // may have been injected by a jump point. Ignore it.
            if (m_priv->m_longPressKeyCode != 0)
                return false;

            // Process start of possible new long press.
            m_priv->m_longPressKeyCode = 0;
            QStringList actions;
            bool handled = TranslateKeyPress("Long Press", keyevent, actions,false);
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
                m_priv->m_longPressKeyCode = keycode;
                m_priv->m_longPressTime = std::chrono::milliseconds(keyevent->timestamp());
                return true; // discard the key press
            }
            break;
        }
        case QEvent::KeyRelease:
        {
            if (keycode == m_priv->m_longPressKeyCode)
            {
                if (keyevent->isAutoRepeat())
                    return true;
                if (m_priv->m_longPressTime > 0ms)
                {
                    // short press or non-repeating keyboard - generate key
                    Qt::KeyboardModifiers modifier = Qt::NoModifier;
                    if (std::chrono::milliseconds(keyevent->timestamp()) - m_priv->m_longPressTime >= LONGPRESS_INTERVAL)
                    {
                        // non-repeatng keyboard
                        modifier = Qt::MetaModifier;
                    }
                    newevent = new QKeyEvent(QEvent::KeyPress, keycode,
                        keyevent->modifiers() | modifier, keyevent->nativeScanCode(),
                        keyevent->nativeVirtualKey(), keyevent->nativeModifiers(),
                        keyevent->text(), false,1);
                    *Event = newevent;
                    NewEvent.reset(newevent);
                    m_priv->m_longPressKeyCode = 0;
                    return false;
                }

                // end of long press
                m_priv->m_longPressKeyCode = 0;
                return true;
            }
            break;
        }
        default:
            break;
    }
    return false;
}

bool MythMainWindow::eventFilter(QObject* Watched, QEvent* Event)
{
    /* Don't let anything through if input is disallowed. */
    if (!m_priv->m_allowInput)
        return true;

    QScopedPointer<QEvent> newevent(nullptr);
    if (KeyLongPressFilter(&Event, newevent))
        return true;

    switch (Event->type())
    {
        case QEvent::KeyPress:
        {
            ResetIdleTimer();
            auto * event = dynamic_cast<QKeyEvent*>(Event);

            // Work around weird GCC run-time bug. Only manifest on Mac OS X
            if (!event)
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
                event = static_cast<QKeyEvent*>(Event);

#ifdef Q_OS_LINUX
            // Fixups for _some_ linux native codes that QT doesn't know
            if (event && event->key() <= 0)
            {
                int keycode = 0;
                switch (event->nativeScanCode())
                {
                    case 209: // XF86AudioPause
                        keycode = Qt::Key_MediaPause;
                        break;
                    default:
                        break;
                }

                if (keycode > 0)
                {
                    auto * key = new QKeyEvent(QEvent::KeyPress, keycode, event->modifiers());
                    if (auto * target = GetTarget(*key); target)
                        QCoreApplication::postEvent(target, key);
                    else
                        QCoreApplication::postEvent(this, key);
                    return true;
                }
            }
#endif

            QVector<MythScreenStack *>::const_reverse_iterator it;
            for (it = m_priv->m_stackList.rbegin(); it != m_priv->m_stackList.rend(); it++)
            {
                if (auto * top = (*it)->GetTopScreen(); top)
                {
                    if (top->keyPressEvent(event))
                        return true;
                    // Note:  The following break prevents keypresses being
                    //        sent to windows below popups
                    if ((*it)->objectName() == "popup stack")
                        break;
                }
            }
            break;
        }
        case QEvent::InputMethod:
        {
            ResetIdleTimer();
            auto *ie = dynamic_cast<QInputMethodEvent*>(Event);
            if (!ie)
                return MythUIScreenBounds::eventFilter(Watched, Event);
            QWidget *widget = QApplication::focusWidget();
            if (widget)
            {
                ie->accept();
                if (widget->isEnabled())
                    QCoreApplication::instance()->notify(widget, ie);
                break;
            }
            QVector<MythScreenStack *>::const_reverse_iterator it;
            for (it = m_priv->m_stackList.rbegin(); it != m_priv->m_stackList.rend(); it++)
            {
                MythScreenType *top = (*it)->GetTopScreen();
                if (top == nullptr)
                    continue;
                if (top->inputMethodEvent(ie))
                    return true;
                // Note:  The following break prevents keypresses being
                //        sent to windows below popups
                if ((*it)->objectName() == "popup stack")
                    break;
            }
            break;
        }
        case QEvent::MouseButtonPress:
        {
            ResetIdleTimer();
            ShowMouseCursor(true);
            if (!m_priv->m_gesture.Recording())
            {
                m_priv->m_gesture.Start();
                auto * mouseEvent = dynamic_cast<QMouseEvent*>(Event);
                if (!mouseEvent)
                    return MythUIScreenBounds::eventFilter(Watched, Event);
                m_priv->m_gesture.Record(mouseEvent->pos(), mouseEvent->button());
                /* start a single shot timer */
                m_priv->m_gestureTimer->start(GESTURE_TIMEOUT);
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease:
        {
            ResetIdleTimer();
            ShowMouseCursor(true);
            if (m_priv->m_gestureTimer->isActive())
                m_priv->m_gestureTimer->stop();

            if (m_priv->m_gesture.Recording())
            {
                m_priv->m_gesture.Stop();
                auto * gesture = m_priv->m_gesture.GetGesture();
                QPoint point { -1, -1 };
                auto * mouseevent = dynamic_cast<QMouseEvent*>(Event);
                if (mouseevent)
                {
                    point = mouseevent->pos();
                    gesture->SetPosition(point);
                }

                // Handle clicks separately
                if (gesture->GetGesture() == MythGestureEvent::Click)
                {
                    if (!mouseevent)
                        return MythUIScreenBounds::eventFilter(Watched, Event);

                    QVector<MythScreenStack *>::const_reverse_iterator it;
                    for (it = m_priv->m_stackList.rbegin(); it != m_priv->m_stackList.rend(); it++)
                    {
                        auto * screen = (*it)->GetTopScreen();
                        if (!screen || !screen->ContainsPoint(point))
                            continue;

                        if (screen->gestureEvent(gesture))
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
                    delete gesture;
                }
                else
                {
                    bool handled = false;
                    
                    if (!mouseevent)
                    {
                        QCoreApplication::postEvent(this, gesture);
                        return true;
                    }
                    
                    QVector<MythScreenStack *>::const_reverse_iterator it;
                    for (it = m_priv->m_stackList.rbegin(); it != m_priv->m_stackList.rend(); it++)
                    {
                        MythScreenType *screen = (*it)->GetTopScreen();
                        if (!screen || !screen->ContainsPoint(point))
                            continue;

                        if (screen->gestureEvent(gesture))
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
                        delete gesture;
                    else
                        QCoreApplication::postEvent(this, gesture);
                }

                return true;
            }
            break;
        }
        case QEvent::MouseMove:
        {
            ResetIdleTimer();
            ShowMouseCursor(true);
            if (m_priv->m_gesture.Recording())
            {
                // Reset the timer
                m_priv->m_gestureTimer->stop();
                m_priv->m_gestureTimer->start(GESTURE_TIMEOUT);
                auto * mouseevent = dynamic_cast<QMouseEvent*>(Event);
                if (!mouseevent)
                    return MythUIScreenBounds::eventFilter(Watched, Event);
                m_priv->m_gesture.Record(mouseevent->pos(), mouseevent->button());
                return true;
            }
            break;
        }
        case QEvent::Wheel:
        {
            ResetIdleTimer();
            ShowMouseCursor(true);
            auto * wheel = dynamic_cast<QWheelEvent*>(Event);
            if (wheel == nullptr)
                return MythUIScreenBounds::eventFilter(Watched, Event);
            int delta = wheel->angleDelta().y();
            if (delta>0)
            {
                wheel->accept();
                auto *key = new QKeyEvent(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier);
                if (auto * target = GetTarget(*key); target)
                    QCoreApplication::postEvent(target, key);
                else
                    QCoreApplication::postEvent(this, key);
            }
            if (delta < 0)
            {
                wheel->accept();
                auto * key = new QKeyEvent(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
                if (auto * target = GetTarget(*key); !target)
                    QCoreApplication::postEvent(target, key);
                else
                    QCoreApplication::postEvent(this, key);
            }
            break;
        }
        default:
            break;
    }

    return MythUIScreenBounds::eventFilter(Watched, Event);
}

void MythMainWindow::customEvent(QEvent* Event)
{
    if (Event->type() == MythGestureEvent::kEventType)
    {
        auto * gesture = dynamic_cast<MythGestureEvent*>(Event);
        if (gesture == nullptr)
            return;
        if (auto * toplevel = GetMainStack(); toplevel)
            if (auto * screen = toplevel->GetTopScreen(); screen)
                screen->gestureEvent(gesture);
        LOG(VB_GUI, LOG_DEBUG, QString("Gesture: %1 (Button: %2)")
            .arg(gesture->GetName(), gesture->GetButtonName()));
    }
    else if (Event->type() == MythEvent::kExitToMainMenuEventType && m_priv->m_exitingtomain)
    {
        ExitToMainMenu();
    }
    else if (Event->type() == ExternalKeycodeEvent::kEventType)
    {
        auto * event = dynamic_cast<ExternalKeycodeEvent *>(Event);
        if (event == nullptr)
            return;
        auto * key = new QKeyEvent(QEvent::KeyPress, event->getKeycode(), Qt::NoModifier);
        if (auto * target = GetTarget(*key); target)
            QCoreApplication::sendEvent(target, key);
        else
            QCoreApplication::sendEvent(this, key);
    }
    else if (Event->type() == MythMediaEvent::kEventType)
    {
        auto *me = dynamic_cast<MythMediaEvent*>(Event);
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
        for (auto * widget : std::as_const(m_priv->m_stackList))
        {
            QVector<MythScreenType*> screenList;
            widget->GetScreenList(screenList);
            for (auto * screen : std::as_const(screenList))
                if (screen)
                    screen->mediaEvent(me);
        }

        // Debugging
        if (MythMediaDevice* device = me->getDevice(); device)
        {
            LOG(VB_GENERAL, LOG_DEBUG, QString("Media Event: %1 - %2")
                    .arg(device->getDevicePath()).arg(device->getStatus()));
        }
    }
    else if (Event->type() == MythEvent::kPushDisableDrawingEventType)
    {
        PushDrawDisabled();
    }
    else if (Event->type() == MythEvent::kPopDisableDrawingEventType)
    {
        PopDrawDisabled();
    }
    else if (Event->type() == MythEvent::kLockInputDevicesEventType)
    {
        m_deviceHandler->IgnoreKeys(true);
        PauseIdleTimer(true);
    }
    else if (Event->type() == MythEvent::kUnlockInputDevicesEventType)
    {
        m_deviceHandler->IgnoreKeys(false);
        PauseIdleTimer(false);
    }
    else if (Event->type() == MythEvent::kDisableUDPListenerEventType)
    {
        MythUDP::EnableUDPListener(false);
    }
    else if (Event->type() == MythEvent::kEnableUDPListenerEventType)
    {
        MythUDP::EnableUDPListener(true);
    }
    else if (Event->type() == MythEvent::kMythEventMessage)
    {
        auto * event = dynamic_cast<MythEvent *>(Event);
        if (event == nullptr)
            return;

        QString message = event->Message();
        if (message.startsWith(ACTION_HANDLEMEDIA))
        {
            if (event->ExtraDataCount() == 1)
                HandleMedia("Internal", event->ExtraData(0));
            else if (event->ExtraDataCount() >= 11)
            {
                bool usebookmark = true;
                if (event->ExtraDataCount() >= 12)
                    usebookmark = (event->ExtraData(11).toInt() != 0);
                HandleMedia("Internal", event->ExtraData(0),
                    event->ExtraData(1), event->ExtraData(2),
                    event->ExtraData(3), event->ExtraData(4),
                    event->ExtraData(5).toInt(), event->ExtraData(6).toInt(),
                    event->ExtraData(7), std::chrono::minutes(event->ExtraData(8).toInt()),
                    event->ExtraData(9), event->ExtraData(10),
                    usebookmark);
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, "Failed to handle media");
            }
        }
        else if (message.startsWith(ACTION_SCREENSHOT))
        {
            int width = 0;
            int height = 0;
            QString filename;
            if (event->ExtraDataCount() >= 2)
            {
                width  = event->ExtraData(0).toInt();
                height = event->ExtraData(1).toInt();
                if (event->ExtraDataCount() == 3)
                    filename = event->ExtraData(2);
            }
            ScreenShot(width, height, filename);
        }
        else if (message == ACTION_GETSTATUS)
        {
            QVariantMap state;
            state.insert("state", "idle");
            state.insert("menutheme", GetMythDB()->GetSetting("menutheme", "defaultmenu"));
            state.insert("currentlocation", GetMythUI()->GetCurrentLocation());
            MythUIStateTracker::SetState(state);
        }
        else if (message == "CLEAR_SETTINGS_CACHE")
        {
            // update the idle time
            m_idleTime =
                gCoreContext->GetDurSetting<std::chrono::minutes>("FrontendIdleTimeout",
                                                                  STANDBY_TIMEOUT);

            if (m_idleTime < 0min)
                m_idleTime = 0min;
            m_idleTimer.stop();
            if (m_idleTime > 0min)
            {
                m_idleTimer.setInterval(m_idleTime);
                m_idleTimer.start();
                LOG(VB_GENERAL, LOG_INFO, QString("Updating the frontend idle time to: %1 mins").arg(m_idleTime.count()));
            }
            else
            {
                LOG(VB_GENERAL, LOG_INFO, "Frontend idle timeout is disabled");
            }
        }
        else if (message == "NOTIFICATION")
        {
            MythNotification mn(*event);
            MythNotificationCenter::GetInstance()->Queue(mn);
            return;
        }
        else if (message == "RECONNECT_SUCCESS" && m_priv->m_standby)
        {
            // If the connection to the master backend has just been (re-)established
            // but we're in standby, make sure the backend is not blocked from
            // shutting down.
            gCoreContext->AllowShutdown();
        }
    }
    else if (Event->type() == MythEvent::kMythUserMessage)
    {
        if (auto * event = dynamic_cast<MythEvent *>(Event); event != nullptr)
            if (const QString& message = event->Message(); !message.isEmpty())
                ShowOkPopup(message);
    }
    else if (Event->type() == MythNotificationCenterEvent::kEventType)
    {
        GetNotificationCenter()->ProcessQueue();
    }
}

QObject* MythMainWindow::GetTarget(QKeyEvent& Key)
{
    auto * target = QWidget::keyboardGrabber();
    if (!target)
    {
        if (auto * widget = QApplication::focusWidget(); widget && widget->isEnabled())
        {
            target = widget;
            // Yes this is special code for handling the
            // the escape key.
            if (Key.key() == m_priv->m_escapekey && widget->topLevelWidget())
                target = widget->topLevelWidget();
        }
    }

    if (!target)
        target = this;
    return target;
}

void MythMainWindow::RestartInputHandlers()
{
    m_deviceHandler->Reset();
}

void MythMainWindow::ShowMouseCursor(bool Show)
{
    if (Show && GetMythDB()->GetBoolSetting("HideMouseCursor", false))
        return;

    // Set cursor call must come after Show() to work on some systems.
    setCursor(Show ? (Qt::ArrowCursor) : (Qt::BlankCursor));
    if (Show)
        m_priv->m_hideMouseTimer->start();
}

void MythMainWindow::HideMouseTimeout()
{
    ShowMouseCursor(false);
}

/*! \brief Disable the idle timeout timer
 * \note This should only be called from the main thread.
*/
void MythMainWindow::DisableIdleTimer(bool DisableIdle)
{
    m_priv->m_disableIdle = DisableIdle;
    if (m_priv->m_disableIdle)
        m_idleTimer.stop();
    else
        m_idleTimer.start();
}

/*! \brief Reset the idle timeout timer
 * \note This should only be called from the main thread.
*/
void MythMainWindow::ResetIdleTimer()
{
    if (m_priv->m_disableIdle)
        return;

    if (m_idleTime == 0min || !m_idleTimer.isActive() || (m_priv->m_standby && m_priv->m_enteringStandby))
        return;

    if (m_priv->m_standby)
        ExitStandby(false);

    m_idleTimer.start();
}

/*! \brief Pause the idle timeout timer
 * \note This should only be called from the main thread.
*/
void MythMainWindow::PauseIdleTimer(bool Pause)
{
    if (m_priv->m_disableIdle)
        return;

    // don't do anything if the idle timer is disabled
    if (m_idleTime == 0min)
        return;

    if (Pause)
    {
        LOG(VB_GENERAL, LOG_NOTICE, "Suspending idle timer");
        m_idleTimer.stop();
    }
    else
    {
        LOG(VB_GENERAL, LOG_NOTICE, "Resuming idle timer");
        m_idleTimer.start();
    }

    // ResetIdleTimer();
}

void MythMainWindow::IdleTimeout()
{
    if (m_priv->m_disableIdle)
        return;

    m_priv->m_enteringStandby = false;

    if (m_idleTime > 0min && !m_priv->m_standby)
    {
        LOG(VB_GENERAL, LOG_NOTICE,
            QString("Entering standby mode after %1 minutes of inactivity").arg(m_idleTime.count()));
        EnterStandby(false);
        if (gCoreContext->GetNumSetting("idleTimeoutSecs", 0) > 0)
        {
            m_priv->m_enteringStandby = true;
            JumpTo("Standby Mode");
        }
    }
}

void MythMainWindow::EnterStandby(bool Manual)
{
    if (Manual && m_priv->m_enteringStandby)
        m_priv->m_enteringStandby = false;

    if (m_priv->m_standby)
        return;

    // We've manually entered standby mode and we want to pause the timer
    // to prevent it being Reset
    if (Manual)
    {
        PauseIdleTimer(true);
        LOG(VB_GENERAL, LOG_NOTICE, QString("Entering standby mode"));
    }

    m_priv->m_standby = true;
    gCoreContext->AllowShutdown();

    QVariantMap state;
    state.insert("state", "standby");
    state.insert("menutheme", GetMythDB()->GetSetting("menutheme", "defaultmenu"));
    state.insert("currentlocation", GetMythUI()->GetCurrentLocation());
    MythUIStateTracker::SetState(state);

    // Cache WOL settings in case DB goes down
    QString masterserver = gCoreContext->GetSetting("MasterServerName");
    gCoreContext->GetSettingOnHost("BackendServerAddr", masterserver);
    MythCoreContext::GetMasterServerPort();
    gCoreContext->GetSetting("WOLbackendCommand", "");

    // While in standby do not attempt to wake the backend
    gCoreContext->SetWOLAllowed(false);
}

void MythMainWindow::ExitStandby(bool Manual)
{
    if (m_priv->m_enteringStandby)
        return;

    if (Manual)
        PauseIdleTimer(false);
    else if (gCoreContext->GetNumSetting("idleTimeoutSecs", 0) > 0)
        JumpTo("Main Menu");

    if (!m_priv->m_standby)
        return;

    LOG(VB_GENERAL, LOG_NOTICE, "Leaving standby mode");
    m_priv->m_standby = false;

    // We may attempt to wake the backend
    gCoreContext->SetWOLAllowed(true);
    gCoreContext->BlockShutdown();

    QVariantMap state;
    state.insert("state", "idle");
    state.insert("menutheme", GetMythDB()->GetSetting("menutheme", "defaultmenu"));
    state.insert("currentlocation", GetMythUI()->GetCurrentLocation());
    MythUIStateTracker::SetState(state);
}

void MythMainWindow::OnApplicationStateChange(Qt::ApplicationState State)
{
    LOG(VB_GENERAL, LOG_NOTICE, QString("Application State Changed to %1").arg(State));
    switch (State)
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
