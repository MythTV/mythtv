//Qt
#include <QThread>
#include <QApplication>
#include <QElapsedTimer>
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
 * Release the reference by calling AcquireRelease(false).
 *
 * \note There is no locking in MythDisplay. It should never be used from anywhere
 * other than the main/UI thread.
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
    QWidget* old = m_widget;
    m_widget = MainWindow;

    if (!m_widget)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Widget removed");
        return;
    }

    if (m_widget != old)
        LOG(VB_GENERAL, LOG_INFO, LOC + "New main widget");
    QWindow* window = m_widget->windowHandle();
    if (window)
    {
        QScreen *desired = GetDesiredScreen();
        if (desired && (desired != window->screen()))
        {
            DebugScreen(desired, "Moving to");

            // If this is a virtual desktop, move the window into the screen,
            // otherwise just set the screen - both of which should trigger a
            // screenChanged event.
            // TODO Confirm this check for non-virtual screens (OSX?)
            // TODO If the screens are non-virtual - can we actually safely move?
            // (SetWidget is only called from MythMainWindow before the render
            // device is created - so should be safe).
            if (desired->geometry() == desired->virtualGeometry())
                window->setScreen(desired);
            else
                m_widget->move(desired->geometry().topLeft());
        }
        connect(window, &QWindow::screenChanged, this, &MythDisplay::ScreenChanged);
    }
    else
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Widget does not have a window!");
    }
}

int MythDisplay::GetScreenCount(void)
{
    return qGuiApp->screens().size();
}

double MythDisplay::GetPixelAspectRatio(void)
{
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
    return m_screen;
}

QScreen *MythDisplay::GetDesiredScreen(void)
{
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
    if (qScreen == m_screen)
        return;
    DebugScreen(qScreen, "Changed to");
    m_screen = qScreen;
    m_videoModes.clear();
    InitialiseModes();
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

void MythDisplay::InitialiseModes(void)
{
    m_last.Init();
    m_curMode = GUI;

    // Initialise DESKTOP mode
    DisplayInfo info = GetDisplayInfo();
    int pixelwidth = info.m_res.width();
    int pixelheight = info.m_res.height();
    int mmwidth = info.m_size.width();
    int mmheight = info.m_size.height();
    double refreshrate = 1000000.0 / static_cast<double>(info.m_rate);

    m_mode[DESKTOP].Init();
    m_mode[DESKTOP] = DisplayResScreen(pixelwidth, pixelheight, mmwidth, mmheight, -1.0, refreshrate);
    LOG(VB_GENERAL, LOG_NOTICE, LOC + QString("Desktop video mode: %1x%2 %3 Hz")
        .arg(pixelwidth).arg(pixelheight).arg(refreshrate, 0, 'f', 3));

    // Initialize GUI mode
    m_mode[GUI].Init();
    pixelwidth = pixelheight = 0;
    double aspectratio = 0.0;
    GetMythDB()->GetResolutionSetting("GuiVidMode", pixelwidth, pixelheight, aspectratio, refreshrate);
    GetMythDB()->GetResolutionSetting("DisplaySize", mmwidth, mmheight);
    m_mode[GUI] = DisplayResScreen(pixelwidth, pixelheight, mmwidth, mmheight, -1.0, refreshrate);

    if (m_mode[DESKTOP] == m_mode[GUI] && qFuzzyIsNull(refreshrate))
    {
        // same resolution as current desktop
        m_last = m_mode[DESKTOP];
    }

    // Initialize default VIDEO mode
    pixelwidth = pixelheight = 0;
    GetMythDB()->GetResolutionSetting("TVVidMode", pixelwidth, pixelheight, aspectratio, refreshrate);
    m_mode[VIDEO] = DisplayResScreen(pixelwidth, pixelheight, mmwidth, mmheight, aspectratio, refreshrate);

    // Initialize video override mode
    m_inSizeToOutputMode.clear();

    for (int i = 0; true; ++i)
    {
        int iw = 0, ih = 0, ow = 0, oh = 0;
        double iaspect = 0.0, oaspect = 0.0;
        double irate = 0.0, orate = 0.0;

        GetMythDB()->GetResolutionSetting("VidMode", iw, ih, iaspect, irate, i);
        GetMythDB()->GetResolutionSetting("TVVidMode", ow, oh, oaspect, orate, i);

        if (!(iw || ih || !qFuzzyIsNull(irate)))
            break;

        if (!(ih && ow && oh))
            break;

        uint64_t key = DisplayResScreen::CalcKey(iw, ih, irate);

        DisplayResScreen scr(ow, oh, mmwidth, mmheight, oaspect, orate);

        m_inSizeToOutputMode[key] = scr;
    }
}

/// \brief Return the screen to the original desktop settings
void MythDisplay::SwitchToDesktop()
{
    SwitchToGUI(DESKTOP);
}

/** \brief Switches to the resolution and refresh rate defined in the
 * database for the specified video resolution and frame rate.
*/
bool MythDisplay::SwitchToVideo(int Width, int Height, double Rate)
{
    Mode next_mode = VIDEO; // default VIDEO mode
    DisplayResScreen next = m_mode[next_mode];
    double target_rate = 0.0;

    // try to find video override mode
    uint64_t key = DisplayResScreen::FindBestScreen(m_inSizeToOutputMode,
                   Width, Height, Rate);

    if (key != 0)
    {
        m_mode[next_mode = CUSTOM_VIDEO] = next = m_inSizeToOutputMode[key];
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Found custom screen override %1x%2")
            .arg(next.Width()).arg(next.Height()));
    }

    // If requested refresh rate is 0, attempt to match video fps
    if (qFuzzyIsNull(next.RefreshRate()))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Trying to match best refresh rate %1Hz")
            .arg(Rate, 0, 'f', 3));
        next.SetRefreshRate(Rate);
    }

    // need to change video mode?
    DisplayResScreen::FindBestMatch(GetVideoModes(), next, target_rate);

    bool chg = !(next == m_last) ||
               !(DisplayResScreen::compare_rates(m_last.RefreshRate(),
                                                 target_rate));

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("%1 %2x%3 %4 Hz")
        .arg(chg ? "Changing to" : "Using")
        .arg(next.Width()).arg(next.Height())
        .arg(target_rate, 0, 'f', 3));

    if (chg && !SwitchToVideoMode(next.Width(), next.Height(), target_rate))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("SwitchToVideo: Video size %1 x %2: "
                                               "xrandr failed for %3 x %4")
            .arg(Width).arg(Height).arg(next.Width()).arg(next.Height()));
        return false;
    }

    m_curMode = next_mode;

    m_last = next;
    m_last.SetRefreshRate(target_rate);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("SwitchToVideo: Video size %1x%2")
        .arg(Width).arg(Height));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("%1 displaying resolution %2x%3, %4mmx%5mm")
        .arg((chg) ? "Switched to" : "Already")
        .arg(m_last.Width()).arg(m_last.Height())
        .arg(m_last.Width_mm()).arg(m_last.Height_mm()));

    if (chg)
        PauseForModeSwitch();

    return chg;
}

/** \brief Switches to the GUI resolution specified.
 *
 * If which_gui is GUI then this switches to the resolution and refresh rate set
 * in the database for the GUI. If which_gui is set to CUSTOM_GUI then we switch
 * to the resolution and refresh rate specified in the last call to SwitchToCustomGUI.
 *
 * \param which_gui either regular GUI or CUSTOM_GUI
 * \sa SwitchToCustomGUI
 */
bool MythDisplay::SwitchToGUI(Mode NextMode)
{
    DisplayResScreen next = m_mode[NextMode];
    auto target_rate = static_cast<double>(NAN);

    // need to change video mode?
    // If GuiVidModeRefreshRate is 0, assume any refresh rate is good enough.
    if (qFuzzyIsNull(next.RefreshRate()))
        next.SetRefreshRate(m_last.RefreshRate());

    DisplayResScreen::FindBestMatch(GetVideoModes(), next, target_rate);
    bool chg = !(next == m_last) ||
               !(DisplayResScreen::compare_rates(m_last.RefreshRate(),
                                                 target_rate));

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("%1 %2x%3 %4 Hz")
        .arg(chg ? "Changing to" : "Using")
        .arg(next.Width()).arg(next.Height())
        .arg(target_rate, 0, 'f', 3));

    if (chg && !SwitchToVideoMode(next.Width(), next.Height(), target_rate))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("SwitchToGUI: xrandr failed for %1x%2 %3  Hz")
            .arg(next.Width()).arg(next.Height())
            .arg(next.RefreshRate(), 0, 'f', 3));
        return false;
    }

    m_curMode = NextMode;

    m_last = next;
    m_last.SetRefreshRate(target_rate);

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("SwitchToGUI: Switched to %1x%2 %3 Hz")
        .arg(m_last.Width()).arg(m_last.Height()).arg(GetRefreshRate(), 0, 'f', 3));

    return chg;
}

double MythDisplay::GetRefreshRate(void)
{
    return m_last.RefreshRate();
}

std::vector<double> MythDisplay::GetRefreshRates(int Width, int Height)
{
    auto tr = static_cast<double>(NAN);
    std::vector<double> empty;

    const DisplayResScreen drs(Width, Height, 0, 0, -1.0, 0.0);
    const DisplayResVector& drv = GetVideoModes();
    int t = DisplayResScreen::FindBestMatch(drv, drs, tr);

    if (t < 0)
        return empty;

    return drv[static_cast<size_t>(t)].RefreshRates();
}

bool MythDisplay::SwitchToVideoMode(int, int, double)
{
    return false;
}

const DisplayResVector& MythDisplay::GetVideoModes(void)
{
    return m_videoModes;
}

/** \brief Returns current screen aspect ratio.
 *
 * If there is an aspect overide in the database that aspect ratio is returned
 * instead of the actual screen aspect ratio.
*/
double MythDisplay::GetAspectRatio(void)
{
    return m_last.AspectRatio();
}

QSize MythDisplay::GetResolution(void)
{
    return QSize(m_last.Width(), m_last.Height());
}

QSize MythDisplay::GetPhysicalSize(void)
{
    return QSize(m_last.Width_mm(), m_last.Height_mm());
}

void MythDisplay::PauseForModeSwitch(void)
{
    int pauselengthinms = gCoreContext->GetNumSetting("VideoModeChangePauseMS", 0);
    if (pauselengthinms)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Pausing %1ms for video mode switch").arg(pauselengthinms));
        QElapsedTimer timer;
        timer.start();
        while (!timer.hasExpired(pauselengthinms))
        {
            QThread::msleep(100);
            qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
        }
    }
}
