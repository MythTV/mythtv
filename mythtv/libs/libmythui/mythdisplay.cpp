//Qt
#include <QApplication>
#include <QWindow>

// MythTV
#include <mythlogging.h>
#include "compat.h"
#include "mythcorecontext.h"
#include "mythdisplay.h"
#include "mythmainwindow.h"

#ifdef Q_OS_ANDROID
#include "mythdisplayandroid.h"
#endif
#if defined(Q_OS_MAC)
#include "mythdisplayosx.h"
#endif
#ifdef USING_X11
#include "mythdisplayx11.h"
#endif
#if defined(Q_OS_WIN)
#include "mythdisplaywindows.h"
#endif

#define LOC QString("Display: ")

/*! \class MythDisplay
 *
 * MythDisplay is a reference counted, singleton class.
 *
 * Retrieve a reference to the MythDisplay object by calling AcquireRelease().
 *
 * A valid pointer is always returned.
 *
 * Release the reference by calling AcquireRelease(false)
 *
 * \bug When using Xinerama/virtual screens, we get the correct screen at startup
 * and we successfully move to another screen at the first attempt. Subsequently
 * trying to move back to the original screen does not work. This appears to be
 * an issue with MythMainWindow relying on MythUIHelper for screen coordinates
 * that are not updated and/or subsequent window movements moving the window
 * into the wrong screen. Much of the MythUIHelper code needs to move into MythDisplay.
 *
 * \todo Complete handling of screen changes. We need to handle several cases.
 * Firstly, when the main window is moved (dragged) to a new screen. This generally
 * works but need to check if all display details are updated (VideoOutWindow is
 * currently hooked up to the CurrentScreenChanged signal - so video window sizing
 * should work without issue). Various other parameters need updating though e.g.
 * refresh rates, supported rates etc
 * Secondly, when a new screen is added and the user has configured a multiscreen
 * setup - in which case we might want to move the main window to the new screen.
 * There are various complications here. Currently switching windows is triggered
 * from the settings screens - which initiate a teardown of painter/render/UI etc.
 * We cannot do that from random parts of the UI or during video playback.
*/
MythDisplay* MythDisplay::AcquireRelease(bool Acquire)
{
    static QMutex s_lock(QMutex::Recursive);
    static MythDisplay* s_display = nullptr;

    QMutexLocker locker(&s_lock);

    if (Acquire)
    {
        if (s_display)
        {
            s_display->IncrRef();
        }
        else
        {
#ifdef USING_X11
            if (MythDisplayX11::IsAvailable())
                s_display = new MythDisplayX11();
#endif
#if defined(Q_OS_MAC)
            if (!s_display)
                s_display = new MythDisplayOSX();
#endif
#ifdef Q_OS_ANDROID
            if (!s_display)
                s_display = new MythDisplayAndroid();
#endif
#if defined(Q_OS_WIN)
            if (!s_display)
                s_display = new MythDisplayWindows();
#endif
            if (!s_display)
                s_display = new MythDisplay();
        }
    }
    else
    {
        if (s_display)
            if (s_display->DecrRef() == 0)
                s_display = nullptr;
    }
    return s_display;
}

MythDisplay::MythDisplay()
  : QObject(),
    ReferenceCounter("Display")
{
    m_screen = GetDesiredScreen();
    DebugScreen(m_screen, "Using");

    connect(qGuiApp, &QGuiApplication::screenRemoved, this, &MythDisplay::ScreenRemoved);
    connect(qGuiApp, &QGuiApplication::screenAdded, this, &MythDisplay::ScreenAdded);
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, &MythDisplay::PrimaryScreenChanged);
#endif
}

MythDisplay::~MythDisplay()
{
    LOG(VB_GENERAL, LOG_INFO, LOC + "Deleting");
}

void MythDisplay::SetWidget(QWidget *MainWindow)
{
    QMutexLocker locker(&m_screenLock);
    QWidget* old = m_widget;
    m_widget = MainWindow;

    if (m_widget)
    {
        if (m_widget != old)
            LOG(VB_GENERAL, LOG_INFO, LOC + "New main widget");
        QWindow* window = m_widget->windowHandle();
        if (window)
        {
            QScreen *desired = GetDesiredScreen();
            if (desired && (desired != window->screen()))
            {
                DebugScreen(desired, "Moving to");
                window->setScreen(desired);
                if (desired->geometry() != desired->virtualGeometry())
                    m_widget->move(desired->geometry().topLeft());
            }
            connect(window, &QWindow::screenChanged, this, &MythDisplay::ScreenChanged);
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Widget does not have a window");
        }
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Widget removed");
    }
}

int MythDisplay::GetScreenCount(void)
{
    return qGuiApp->screens().size();
}

double MythDisplay::GetPixelAspectRatio(void)
{
    QMutexLocker locker(&m_screenLock);
    DisplayInfo info = GetDisplayInfo();
    if (info.m_size.width() > 0 && info.m_size.height() > 0 &&
        info.m_res.width() > 0 && info.m_res.height() > 0)
    {
        return (info.m_size.width() / static_cast<double>(info.m_res.width())) /
               (info.m_size.height() / static_cast<double>(info.m_res.height()));
    }
    return 1.0;
}

/*! \brief Return a pointer to the screen to use.
 *
 * This function looks at the users screen preference, and will return
 * that screen if possible.  If not, i.e. the screen isn't plugged in,
 * then this function returns the system's primary screen.
 *
 * Note: There is no special case here for the case of MythTV spanning
 * all screens, as all screen have access to the virtual desktop
 * attributes.  The check for spanning screens must be made when the
 * screen size/geometry accessed, and the proper physical/virtual
 * size/geometry retrieved.
*/
QScreen* MythDisplay::GetCurrentScreen(void)
{
    QMutexLocker locker(&m_screenLock);
    return m_screen;
}

QScreen *MythDisplay::GetDesiredScreen(void)
{
    QMutexLocker locker(&m_screenLock);
    QScreen* newscreen = nullptr;

    // Lookup by name
    QString name = gCoreContext->GetSetting("XineramaScreen", nullptr);
    foreach (QScreen *screen, qGuiApp->screens())
    {
        if (!name.isEmpty() && name == screen->name())
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Found screen '%1'").arg(name));
            newscreen = screen;
        }
    }

    // No name match.  These were previously numbers.
    if (!newscreen)
    {
        bool ok = false;
        int screen_num = name.toInt(&ok);
        QList<QScreen *>screens = qGuiApp->screens();
        if (ok && (screen_num >= 0) && (screen_num < screens.size()))
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Found screen number %1 (%2)")
                .arg(name).arg(screens[screen_num]->name()));
            newscreen = screens[screen_num];
        }
    }

    // For anything else, return the primary screen.
    if (!newscreen)
    {
        QScreen *primary = qGuiApp->primaryScreen();
        if (name.isEmpty())
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Defaulting to primary screen (%1)")
                .arg(primary->name()));
        }
        else if (name != "-1")
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Screen '%1' not found, defaulting to primary screen (%2)")
                .arg(name).arg(primary->name()));
        }
        newscreen = primary;
    }

    return newscreen;
}

/*! \brief The actual screen in use has changed. We must use it.
*/
void MythDisplay::ScreenChanged(QScreen *qScreen)
{
    QMutexLocker locker(&m_screenLock);
    if (qScreen == m_screen)
        return;
    DebugScreen(qScreen, "Changed to");
    m_screen = qScreen;
    emit CurrentScreenChanged(qScreen);
}

void MythDisplay::PrimaryScreenChanged(QScreen* qScreen)
{
    DebugScreen(qScreen, "New primary");
}

void MythDisplay::ScreenAdded(QScreen* qScreen)
{
    DebugScreen(qScreen, "New");
    emit ScreenCountChanged(qGuiApp->screens().size());
}

void MythDisplay::ScreenRemoved(QScreen* qScreen)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Screen '%1' removed").arg(qScreen->name()));
    emit ScreenCountChanged(qGuiApp->screens().size());
}

float MythDisplay::SanitiseRefreshRate(int Rate)
{
    static const float default_rate = 1000000.0F / 60.0F;
    float fixed = default_rate;
    if (Rate > 0)
    {
        fixed = static_cast<float>(Rate) / 2.0F;
        if (fixed < default_rate)
            fixed = default_rate;
    }
    return fixed;
}

DisplayInfo MythDisplay::GetDisplayInfo(int)
{
    // This is the final fallback when no other platform specifics are available
    // It is usually accurate apart from the refresh rate - which is often
    // rounded down.
    QMutexLocker locker(&m_screenLock);
    QScreen *screen = GetCurrentScreen();
    DisplayInfo ret;
    ret.m_rate = 1000000.0F / static_cast<float>(screen->refreshRate());
    ret.m_res  = screen->size();
    ret.m_size = QSize(static_cast<int>(screen->physicalSize().width()),
                       static_cast<int>(screen->physicalSize().height()));
    return ret;
}

/// \brief Return true if the MythTV windows should span all screens.
bool MythDisplay::SpanAllScreens(void)
{
    return gCoreContext->GetSetting("XineramaScreen", nullptr) == "-1";
}

QString MythDisplay::GetExtraScreenInfo(QScreen *qScreen)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    QString mfg = qScreen->manufacturer();
    if (mfg.isEmpty())
        mfg = "Unknown";
    QString model = qScreen->model();
    if (model.isEmpty())
        model = "Unknown";
    return QString("(Make: %1 Model: %2)").arg(mfg).arg(model);
#else
    Q_UNUSED(qScreen);
    return QString();
#endif
}

void MythDisplay::DebugScreen(QScreen *qScreen, const QString &Message)
{
    QRect geom = qScreen->geometry();
    QString extra = GetExtraScreenInfo(qScreen);

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("%1 screen '%2' %3")
        .arg(Message).arg(qScreen->name()).arg(extra));

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Geometry %1x%2+%3+%4 Size %5mmx%6mm")
        .arg(geom.width()).arg(geom.height()).arg(geom.left()).arg(geom.top())
        .arg(qScreen->physicalSize().width()).arg(qScreen->physicalSize().height()));

    if (qScreen->virtualGeometry() != geom)
    {
        geom = qScreen->virtualGeometry();
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Total virtual geometry: %1x%2+%3+%4")
            .arg(geom.width()).arg(geom.height()).arg(geom.left()).arg(geom.top()));
    }
}
