//Qt
#include <QTimer>
#include <QThread>
#include <QApplication>
#include <QElapsedTimer>
#include <QWindow>

// MythTV
#include "mythlogging.h"
#include "compat.h"
#include "mythcorecontext.h"
#include "mythuihelper.h"
#include "mythdisplay.h"
#include "mythegl.h"
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
#ifdef USING_DRM
#include "mythdisplaydrm.h"
#endif
#if defined(Q_OS_WIN)
#include "mythdisplaywindows.h"
#endif
#ifdef USING_MMAL
#include "mythdisplayrpi.h"
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
#ifdef USING_MMAL
            if (!s_display)
                s_display = new MythDisplayRPI();
#endif
#ifdef USING_DRM
#if QT_VERSION >= QT_VERSION_CHECK(5,9,0)
            // this will only work by validating the screen's serial number
            // - which is only available with Qt 5.9
            if (!s_display)
                s_display = new MythDisplayDRM();
#endif
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

QStringList MythDisplay::GetDescription(void)
{
    QStringList result;
    bool spanall = false;
    int screencount = MythDisplay::GetScreenCount();
    MythDisplay* display = MythDisplay::AcquireRelease();

    if (MythDisplay::SpanAllScreens() && screencount > 1)
    {
        spanall = true;
        result.append(tr("Spanning %1 screens").arg(screencount));
        result.append(tr("Total bounds") + QString("\t: %1x%2")
                      .arg(display->GetScreenBounds().width())
                      .arg(display->GetScreenBounds().height()));
        result.append("");
    }

    QScreen *current = display->GetCurrentScreen();
    QList<QScreen*> screens = qGuiApp->screens();
    bool first = true;
    for (auto it = screens.cbegin(); it != screens.cend(); ++it)
    {
        if (!first)
            result.append("");
        first = false;
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
        QString id = QString("(%1)").arg((*it)->manufacturer());
#else
        QString id;
#endif
        if ((*it) == current && !spanall)
            result.append(tr("Current screen %1 %2:").arg((*it)->name()).arg(id));
        else
            result.append(tr("Screen %1 %2:").arg((*it)->name()).arg(id));
        result.append(tr("Size") + QString("\t\t: %1mmx%2mm")
                .arg((*it)->physicalSize().width()).arg((*it)->physicalSize().height()));
        if ((*it) == current)
        {
            QString source;
            double aspect = display->GetAspectRatio(source);
            result.append(tr("Aspect ratio") + QString("\t: %1 (%2)")
                    .arg(aspect, 0, 'f', 3).arg(source));
            if (!spanall)
            {
                result.append(tr("Current mode") + QString("\t: %1x%2@%3Hz")
                              .arg(display->GetResolution().width()).arg(display->GetResolution().height())
                              .arg(display->GetRefreshRate(), 0, 'f', 2));
            }
        }
    }
    MythDisplay::AcquireRelease(false);
    return result;
}

MythDisplay::MythDisplay()
  : ReferenceCounter("Display")
{
    m_screen = GetDesiredScreen();
    DebugScreen(m_screen, "Using");
    if (m_screen)
    {
        connect(m_screen, &QScreen::geometryChanged, this, &MythDisplay::GeometryChanged);
        connect(m_screen, &QScreen::physicalDotsPerInchChanged, this, &MythDisplay::PhysicalDPIChanged);
    }

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

/*! \brief Set the QWidget and QWindow in use.
 *
 * Certain platform implementations need to know the QWidget and/or QWindow
 * to access display information. We also connect to the QWindow::screenChanged
 * signal so that we are informed when the window has been moved into a new screen.
 *
 * \note We typically call this twice; once before MythMainWindow is shown and
 * once after. This is because we need to try and ensure we are using the correct
 * screen before showing the window, and hence have the correct geometry etc, and
 * once after because the QWindow is sometimes not created until after the widget
 * is shown - and we cannot connect the screenChanged signal until we know the window
 * handle.
*/
void MythDisplay::SetWidget(QWidget *MainWindow)
{
    QWidget* oldwidget = m_widget;
    m_widget = MainWindow;
    if (!m_modeComplete)
        UpdateCurrentMode();

    QWindow* oldwindow = m_window;
    if (m_widget)
        m_window = m_widget->windowHandle();
    else
        m_window = nullptr;

    if (m_widget && (m_widget != oldwidget))
        LOG(VB_GENERAL, LOG_INFO, LOC + "Have main widget");

    if (m_window && (m_window != oldwindow))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Have main window");

        connect(m_window, &QWindow::screenChanged, this, &MythDisplay::ScreenChanged, Qt::UniqueConnection);
        QScreen *desired = GetDesiredScreen();
        // If we have changed the video mode for the old screen then reset
        // it to the default/desktop mode
        SwitchToDesktop();
        // Ensure we completely re-initialise when the new screen is set
        m_initialised = false;
        if (desired != m_screen)
            DebugScreen(desired, "Moving to");
        m_window->setScreen(desired);
        // WaitForNewScreen doesn't work as intended. It successfully filters
        // out unwanted screenChanged signals after moving screens - but always
        //times out. This just delays startup by 500ms - so ignore on startup as it isn't needed.
        if (!m_firstScreenChange)
            WaitForNewScreen();
        m_firstScreenChange = false;
        InitScreenBounds();
        return;
    }
}

int MythDisplay::GetScreenCount(void)
{
    return qGuiApp->screens().size();
}

double MythDisplay::GetPixelAspectRatio(void)
{
    if (m_physicalSize.isEmpty() || m_resolution.isEmpty())
        return 1.0;

    return (m_physicalSize.width() / static_cast<double>(m_resolution.width())) /
           (m_physicalSize.height() / static_cast<double>(m_resolution.height()));
}

QSize MythDisplay::GetGUIResolution(void)
{
    return m_guiMode.Resolution();
}

QRect MythDisplay::GetScreenBounds(void)
{
    return m_screenBounds;
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

    // If geometry is overriden at the command line level, try and determine
    // which screen that applies to (if any).
    // N.B. So many potential issues here e.g. should the geometry override be
    // ignored after first use? (as it will continue to override the screen
    // regardless of changes to screen preference).
    if (MythUIHelper::IsGeometryOverridden())
    {
        // this matches the check in MythMainWindow
        bool windowed = GetMythDB()->GetBoolSetting("RunFrontendInWindow", false) &&
                        !MythMainWindow::WindowIsAlwaysFullscreen();
        QRect override = GetMythUI()->GetGeometryOverride();
        // When windowed, we use topleft as a best guess as to which screen we belong in.
        // When fullscreen, Qt appears to use the reverse - though this may be
        // the window manager rather than Qt. So could be wrong.
        QPoint point = windowed ? override.topLeft() : override.bottomRight();
        foreach (QScreen *screen, qGuiApp->screens())
        {
            if (screen->geometry().contains(point))
            {
                newscreen = screen;
                LOG(VB_GENERAL, LOG_INFO, LOC + QString(
                    "Geometry override places window in screen '%1'").arg(newscreen->name()));
                break;
            }
        }
    }

    // If spanning all screens, then always use the primary display
    if (!newscreen && MythDisplay::SpanAllScreens())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Using primary screen for multiscreen");
        newscreen = qGuiApp->primaryScreen();
    }

    QString name = gCoreContext->GetSetting("XineramaScreen", nullptr);
    // Lookup by name
    if (!newscreen)
    {
        foreach (QScreen *screen, qGuiApp->screens())
        {
            if (!name.isEmpty() && name == screen->name())
            {
                LOG(VB_GENERAL, LOG_INFO, LOC + QString("Found screen '%1'").arg(name));
                newscreen = screen;
            }
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
        if (name.isEmpty() && primary)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Defaulting to primary screen (%1)")
                .arg(primary->name()));
        }
        else if (name != "-1" && primary)
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
    if (m_screen)
        disconnect(m_screen, nullptr, this, nullptr);
    DebugScreen(qScreen, "Changed to");
    m_screen = qScreen;
    connect(m_screen, &QScreen::geometryChanged, this, &MythDisplay::GeometryChanged);
    connect(m_screen, &QScreen::physicalDotsPerInchChanged, this, &MythDisplay::PhysicalDPIChanged);
    Initialise();
    emit CurrentScreenChanged(qScreen);
}

void MythDisplay::PhysicalDPIChanged(qreal DPI)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Qt screen pixel ratio changed to %1")
        .arg(DPI, 2, 'f', 2, '0'));
    emit CurrentDPIChanged(DPI);
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

void MythDisplay::GeometryChanged(const QRect &Geo)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("New screen geometry: %1x%2+%3+%4")
        .arg(Geo.width()).arg(Geo.height()).arg(Geo.left()).arg(Geo.top()));
}

/*! \brief Retrieve screen details.
 *
 * This is the final fallback when no other platform specifics are available
 * It is usually accurate apart from the refresh rate - which is often
 * rounded down.
*/
void MythDisplay::UpdateCurrentMode(void)
{
    // Certain platform implementations do not have a window to access at startup
    // and hence use this implementation. Flag the status as incomplete to ensure
    // we try to retrieve the full details at the first opportunity.
    m_modeComplete = false;
    m_edid = MythEDID();
    QScreen *screen = GetCurrentScreen();
    if (!screen)
    {
        m_refreshRate  = 60.0;
        m_physicalSize = QSize(0, 0);
        m_resolution   = QSize(1920, 1080);
        return;
    }
    m_refreshRate   = screen->refreshRate();
    m_resolution    = screen->size();
    m_physicalSize  = QSize(static_cast<int>(screen->physicalSize().width()),
                            static_cast<int>(screen->physicalSize().height()));
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
    if (!qScreen)
        return;

    QRect geom = qScreen->geometry();
    QString extra = GetExtraScreenInfo(qScreen);

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("%1 screen '%2' %3")
        .arg(Message).arg(qScreen->name()).arg(extra));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Qt screen pixel ratio: %1")
        .arg(qScreen->devicePixelRatio(), 2, 'f', 2, '0'));
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Geometry: %1x%2+%3+%4 Size(Qt): %5mmx%6mm")
        .arg(geom.width()).arg(geom.height()).arg(geom.left()).arg(geom.top())
        .arg(qScreen->physicalSize().width()).arg(qScreen->physicalSize().height()));

    if (qScreen->virtualGeometry() != geom)
    {
        geom = qScreen->virtualGeometry();
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Total virtual geometry: %1x%2+%3+%4")
            .arg(geom.width()).arg(geom.height()).arg(geom.left()).arg(geom.top()));
    }
}

void MythDisplay::Initialise(void)
{
    m_videoModes.clear();
    m_overrideVideoModes.clear();
    UpdateCurrentMode();
    InitScreenBounds();

    // Set the desktop mode - which is the mode at startup. We must always return
    // the screen to this mode.
    if (!m_initialised)
    {
        // Only ever set this once or after a screen change
        m_initialised = true;
        m_desktopMode = MythDisplayMode(m_resolution, m_physicalSize, -1.0, m_refreshRate);
        LOG(VB_GENERAL, LOG_NOTICE, LOC + QString("Desktop video mode: %1x%2 %3Hz")
            .arg(m_resolution.width()).arg(m_resolution.height()).arg(m_refreshRate, 0, 'f', 3));
        if (m_edid.Valid())
        {
            if (m_edid.IsSRGB())
                LOG(VB_GENERAL, LOG_NOTICE, LOC + "Display is using sRGB colourspace");
            else
                LOG(VB_GENERAL, LOG_NOTICE, LOC + "Display has custom colourspace");
        }
    }

    // Set the gui mode from database settings
    int pixelwidth     = m_resolution.width();
    int pixelheight    = m_resolution.height();
    int mmwidth        = m_physicalSize.width();
    int mmheight       = m_physicalSize.height();
    double refreshrate = m_refreshRate;
    double aspectratio = 0.0;
    GetMythDB()->GetResolutionSetting("GuiVidMode", pixelwidth, pixelheight, aspectratio, refreshrate);
    GetMythDB()->GetResolutionSetting("DisplaySize", mmwidth, mmheight);
    m_guiMode = MythDisplayMode(pixelwidth, pixelheight, mmwidth, mmheight, -1.0, refreshrate);

    // Set default video mode
    pixelwidth = pixelheight = 0;
    GetMythDB()->GetResolutionSetting("TVVidMode", pixelwidth, pixelheight, aspectratio, refreshrate);
    m_videoMode = MythDisplayMode(pixelwidth, pixelheight, mmwidth, mmheight, aspectratio, refreshrate);

    // Initialise video override modes
    for (int i = 0; true; ++i)
    {
        int iw = 0;
        int ih = 0;
        int ow = 0;
        int oh = 0;
        double iaspect = 0.0;
        double oaspect = 0.0;
        double irate = 0.0;
        double orate = 0.0;

        GetMythDB()->GetResolutionSetting("VidMode", iw, ih, iaspect, irate, i);
        GetMythDB()->GetResolutionSetting("TVVidMode", ow, oh, oaspect, orate, i);

        if (!(iw || ih || !qFuzzyIsNull(irate)) || !(ih && ow && oh))
            break;

        uint64_t key = MythDisplayMode::CalcKey(QSize(iw, ih), irate);
        MythDisplayMode scr(QSize(ow, oh), QSize(mmwidth, mmheight), oaspect, orate);
        m_overrideVideoModes[key] = scr;
    }
}


/*! \brief Get screen size from Qt while respecting the user's multiscreen settings
 *
 * If the windowing system environment has multiple screens, then use
 * QScreen::virtualSize() to get the size of the virtual desktop.
 * Otherwise QScreen::size() or QScreen::availableSize() will provide
 * the size of an individual screen.
*/
void MythDisplay::InitScreenBounds(void)
{
    QList<QScreen*> screens = qGuiApp->screens();
    for (auto it = screens.cbegin(); it != screens.cend(); ++it)
    {
        QRect dim = (*it)->geometry();
        QString extra = MythDisplay::GetExtraScreenInfo(*it);
        LOG(VB_GUI, LOG_INFO, LOC + QString("Screen %1: %2x%3 %4")
            .arg((*it)->name()).arg(dim.width()).arg(dim.height()).arg(extra));
    }

    QScreen *primary = qGuiApp->primaryScreen();
    LOG(VB_GUI, LOG_INFO, LOC +QString("Primary screen: %1.").arg(primary->name()));

    int numScreens = MythDisplay::GetScreenCount();
    QSize dim = primary->virtualSize();
    LOG(VB_GUI, LOG_INFO, LOC + QString("Total desktop dim: %1x%2, over %3 screen[s].")
        .arg(dim.width()).arg(dim.height()).arg(numScreens));

    if (MythDisplay::SpanAllScreens())
    {
        LOG(VB_GUI, LOG_INFO, LOC + QString("Using entire desktop."));
        m_screenBounds = primary->virtualGeometry();
        return;
    }

    if (GetMythDB()->GetBoolSetting("RunFrontendInWindow", false))
    {
        LOG(VB_GUI, LOG_INFO, LOC + "Running in a window");
        // This doesn't include the area occupied by the
        // Windows taskbar, or the Mac OS X menu bar and Dock
        m_screenBounds = m_screen->availableGeometry();
    }
    else
    {
        m_screenBounds = m_screen->geometry();
    }

    LOG(VB_GUI, LOG_INFO, LOC + QString("Using screen %1: %2x%3 at %4+%5")
        .arg(m_screen->name()).arg(m_screenBounds.width()).arg(m_screenBounds.height())
        .arg(m_screenBounds.left()).arg(m_screenBounds.top()));
}

/*! \brief Check whether the next mode is larger in size than the current mode.
 *
 * This is used to allow the caller to force an update of the main window to ensure
 * the window is fully resized and is in no way clipped. Not an issue if the next
 * mode is smaller.
*/
bool MythDisplay::NextModeIsLarger(QSize Size)
{
    return Size.width() > m_resolution.width() || Size.height() > m_resolution.height();
}

/*! \brief Return the screen to the original desktop video mode
 *
 * \note It is assume the app is exiting once this is called.
*/
void MythDisplay::SwitchToDesktop()
{
    MythDisplayMode current(m_resolution, m_physicalSize, -1.0, m_refreshRate);
    if (current == m_desktopMode)
        return;
    SwitchToVideoMode(m_desktopMode.Resolution(), m_desktopMode.RefreshRate());
}

/** \brief Switches to the resolution and refresh rate defined in the
 * database for the specified video resolution and frame rate.
*/
bool MythDisplay::SwitchToVideo(QSize Size, double Rate)
{
    if (!m_modeComplete)
        UpdateCurrentMode();

    MythDisplayMode next = m_videoMode;
    MythDisplayMode current(m_resolution, m_physicalSize, -1.0, m_refreshRate);
    double targetrate = 0.0;
    double aspectoverride = 0.0;

    // try to find video override mode
    uint64_t key = MythDisplayMode::FindBestScreen(m_overrideVideoModes,
                   Size.width(), Size.height(), Rate);

    if (key != 0)
    {
        next = m_overrideVideoModes[key];
        if (next.AspectRatio() > 0.0)
            aspectoverride = next.AspectRatio();
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Found custom screen override %1x%2 Aspect %3")
            .arg(next.Width()).arg(next.Height()).arg(aspectoverride));
    }

    // If requested refresh rate is 0, attempt to match video fps
    if (qFuzzyIsNull(next.RefreshRate()))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Trying to match best refresh rate %1Hz")
            .arg(Rate, 0, 'f', 3));
        next.SetRefreshRate(Rate);
    }

    // need to change video mode?
    MythDisplayMode::FindBestMatch(GetVideoModes(), next, targetrate);
    if ((next == current) && (MythDisplayMode::CompareRates(current.RefreshRate(), targetrate)))
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Using current mode %1x%2@%3Hz")
            .arg(m_resolution.width()).arg(m_resolution.height()).arg(m_refreshRate));
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + QString("Trying mode %1x%2@%3Hz")
        .arg(next.Width()).arg(next.Height()).arg(next.RefreshRate(), 0, 'f', 3));

    if (!SwitchToVideoMode(next.Resolution(), targetrate))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to change mode to %1x%2@%3Hz")
            .arg(next.Width()).arg(next.Height()).arg(next.RefreshRate(), 0, 'f', 3));
        return false;
    }

    if (next.Resolution() != m_resolution)
        WaitForScreenChange();

    // N.B. We used a computed aspect ratio unless overridden
    m_aspectRatioOverride = aspectoverride > 0.0 ? aspectoverride : 0.0;
    UpdateCurrentMode();
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Switched to %1x%2@%3Hz for video %4x%5")
        .arg(m_resolution.width()).arg(m_resolution.height())
        .arg(m_refreshRate, 0, 'f', 3).arg(Size.width()).arg(Size.height()));
    PauseForModeSwitch();
    return true;
}

/** \brief Switches to the GUI resolution.
 */
bool MythDisplay::SwitchToGUI(bool Wait)
{
    if (!m_modeComplete)
        UpdateCurrentMode();

    // If the current resolution is the same as the GUI resolution then do nothing
    // as refresh rate should not be critical for the GUI.
    if (m_resolution == m_guiMode.Resolution())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Using %1x%2@%3Hz for GUI")
            .arg(m_resolution.width()).arg(m_resolution.height()).arg(m_refreshRate));
        return true;
    }

    if (!SwitchToVideoMode(m_guiMode.Resolution(), m_guiMode.RefreshRate()))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to change mode to %1x%2@%3Hz")
            .arg(m_guiMode.Width()).arg(m_guiMode.Height()).arg(m_guiMode.RefreshRate(), 0, 'f', 3));
        return false;
    }

    if (Wait && (m_resolution != m_guiMode.Resolution()))
        WaitForScreenChange();

    UpdateCurrentMode();
    m_aspectRatioOverride = 0.0;
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Switched to %1x%2@%3Hz")
        .arg(m_resolution.width()).arg(m_resolution.height()).arg(m_refreshRate, 0, 'f', 3));
    return true;
}

double MythDisplay::GetRefreshRate(void)
{
    return m_refreshRate;
}

int MythDisplay::GetRefreshInterval(int Fallback)
{
    if (m_refreshRate > 20.0 && m_refreshRate < 200.0)
        return static_cast<int>(lround(1000000.0 / m_refreshRate));
    if (Fallback > 33000) // ~30Hz
        Fallback /= 2;
    return Fallback;
}

std::vector<double> MythDisplay::GetRefreshRates(QSize Size)
{
    auto targetrate = static_cast<double>(NAN);
    const MythDisplayMode mode(Size, QSize(0, 0), -1.0, 0.0);
    const vector<MythDisplayMode>& modes = GetVideoModes();
    int match = MythDisplayMode::FindBestMatch(modes, mode, targetrate);
    if (match < 0)
        return std::vector<double>();
    return modes[static_cast<size_t>(match)].RefreshRates();
}

bool MythDisplay::SwitchToVideoMode(QSize /*Size*/, double /*Framerate*/)
{
    return false;
}

const vector<MythDisplayMode> &MythDisplay::GetVideoModes(void)
{
    return m_videoModes;
}

/** \brief Returns current screen aspect ratio.
 *
 * For the vast majority of cases we should be using the physical display size to
 * compute the actual aspect ratio.
 *
 * This is currently overridden by:-
 *  - a custom, user specified aspect ratio for a given display mode. This feature
 *    is considered legacy as I don't believe it should be needed anymore,
 *    complicates both the code and the user 'experience' and offers very
 *    limited values for the override (4:3 and 16:9).
 *  - a general override for setups where the aspect ratio is not correctly detected.
 *    This generally means exotic multiscreen setups and displays with invalid and/or
 *    misleading EDIDs.
 *
 * If no override is specified and a valid physical aspect ratio cannot be
 * calculated, then we fallback to the screen resolution (i.e. assume square pixels)
 * and finally a guess at 16:9.
*/
double MythDisplay::GetAspectRatio(QString &Source, bool IgnoreModeOverride)
{
    auto valid = [](double Aspect) { return (Aspect > 0.1 && Aspect < 10.0); };

    // Override for this video mode
    // Is this behaviour still needed?
    if (!IgnoreModeOverride && valid(m_aspectRatioOverride))
    {
        Source = tr("Video mode override");
        return m_aspectRatioOverride;
    }

    // General override for invalid/misleading EDIDs or multiscreen setups
    bool multiscreen = MythDisplay::SpanAllScreens() && GetScreenCount() > 1;
    double override = gCoreContext->GetFloatSettingOnHost("XineramaMonitorAspectRatio",
                                                          gCoreContext->GetHostName(), 0.0);

    // Zero (not valid) indicates auto
    if (valid(override))
    {
        Source = tr("Override");
        return override;
    }
    // Auto for multiscreen is a best guess
    else if (multiscreen)
    {
        double aspect = EstimateVirtualAspectRatio();
        if (valid(aspect))
        {
            Source = tr("Multiscreen estimate");
            return aspect;
        }
    }

    // Based on actual physical size if available
    if (!m_physicalSize.isEmpty())
    {
        double aspect = static_cast<double>(m_physicalSize.width()) / m_physicalSize.height();
        if (valid(aspect))
        {
            Source = tr("Detected");
            return aspect;
        }
    }

    // Assume pixel aspect ratio is 1 (square pixels)
    if (!m_resolution.isEmpty())
    {
        double aspect = static_cast<double>(m_resolution.width()) / m_resolution.height();
        if (valid(aspect))
        {
            Source = tr("Fallback");
            return aspect;
        }
    }

    // the aspect ratio of last resort
    Source = tr("Guessed");
    return 16.0 / 9.0;
}

MythEDID& MythDisplay::GetEDID(void)
{
    return m_edid;
}

/*! \brief Estimate the overall display aspect ratio for multi screen setups.
 *
 * \note This will only work where screens are configured either as a grid (e.g. 2x2)
 * or as a single 'row' or 'column' of screens. Likewise it will fail if the aspect
 * ratios of the displays are not similar and may not work if the display
 * resolutions are significantly different (in a grid type setup).
 *
 * \note Untested with a grid layout - anyone have a card with 4 outputs?
*/
double MythDisplay::EstimateVirtualAspectRatio(void)
{
    auto sortscreens = [](const QScreen* First, const QScreen* Second)
    {
        if (First->geometry().left() < Second->geometry().left())
            return true;
        if (First->geometry().top() < Second->geometry().top())
            return true;
        return false;
    };

    // default
    double result = 16.0 / 9.0;

    QList<QScreen*> screens;
    if (m_screen)
        screens = m_screen->virtualSiblings();
    if (screens.empty())
        return result;

    // N.B. This sorting may not be needed
    std::sort(screens.begin(), screens.end(), sortscreens);
    QList<double> aspectratios;
    QSize totalresolution;
    int lasttop = 0;
    int lastleft = 0;
    int rows = 1;
    int columns = 1;
    for (auto it = screens.constBegin() ; it != screens.constEnd(); ++it)
    {
        QRect geom = (*it)->geometry();
        totalresolution += geom.size();
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("%1x%2+%3+%4 %5")
            .arg(geom.width()).arg(geom.height()).arg(geom.left()).arg(geom.top())
            .arg((*it)->physicalSize().width() / (*it)->physicalSize().height()));
        if (lastleft < geom.left())
        {
            columns++;
            lastleft = geom.left();
        }
        if (lasttop < geom.top())
        {
            rows++;
            lasttop = geom.top();
            lastleft = 0;
        }
        aspectratios << (*it)->physicalSize().width() / (*it)->physicalSize().height();
    }

    // If all else fails, use the total resolution and assume pixel aspect ratio
    // equals display aspect ratio
    if (!totalresolution.isEmpty())
        result = totalresolution.width() / totalresolution.height();

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Screen layout: %1x%2").arg(rows).arg(columns));
    if (rows == columns)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Grid layout");
    }
    else if (rows == 1 && columns > 1)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Horizontal layout");
    }
    else if (columns == 1 && rows > 1)
    {
        LOG(VB_GENERAL, LOG_DEBUG, LOC + "Vertical layout");
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO,
            LOC + QString("Unsupported layout - defaulting to %1 (%2/%3)")
            .arg(result).arg(totalresolution.width()).arg(totalresolution.height()));
        return result;
    }

    // validate aspect ratios - with a little fuzzyness
    double aspectratio = 0.0;
    double average = 0.0;
    int count = 1;
    for (auto it2 = aspectratios.constBegin() ; it2 != aspectratios.constEnd(); ++it2, ++count)
    {
        aspectratio += *it2;
        average = aspectratio / count;
        if (qAbs(*it2 - average) > 0.1)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                QString("Inconsistent aspect ratios - defaulting to %1 (%2/%3)")
                .arg(result).arg(totalresolution.width()).arg(totalresolution.height()));
            return result;
        }
    }

    aspectratio = (average * columns) / rows;
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Estimated aspect ratio: %1")
        .arg(aspectratio));
    return aspectratio;
}

QSize MythDisplay::GetResolution(void)
{
    return m_resolution;
}

QSize MythDisplay::GetPhysicalSize(void)
{
    return m_physicalSize;
}

void MythDisplay::WaitForScreenChange(void)
{
    // Some implementations may have their own mechanism for ensuring the mode
    // is updated before continuing
    if (!m_waitForModeChanges)
        return;

    LOG(VB_GENERAL, LOG_INFO, LOC + "Waiting for resolution change");
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, [](){ LOG(VB_GENERAL, LOG_WARNING, LOC + "Timed out wating for screen change"); });
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(m_screen, &QScreen::geometryChanged, &loop, &QEventLoop::quit);
    // 500ms maximum wait
    timer.start(500);
    loop.exec();
}

void MythDisplay::WaitForNewScreen(void)
{
    // N.B. This isn't working as intended as it always times out rather than
    // exiting deliberately. It does however somehow filter out unwanted screenChanged
    // events that otherwise often put the widget in the wrong screen.
    // Needs more investigation - but for now it works:)
    if (!m_widget || (m_widget && !m_widget->windowHandle()))
        return;
    LOG(VB_GENERAL, LOG_INFO, LOC + "Waiting for new screen");
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, [](){ LOG(VB_GENERAL, LOG_WARNING, LOC + "Timed out waiting for new screen"); });
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(m_widget->windowHandle(), &QWindow::screenChanged, &loop, &QEventLoop::quit);
    // 500ms maximum wait
    timer.start(500);
    loop.exec();
}

void MythDisplay::PauseForModeSwitch(void)
{
    int pauselengthinms = gCoreContext->GetNumSetting("VideoModeChangePauseMS", 0);
    if (pauselengthinms)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("Pausing %1ms for video mode switch").arg(pauselengthinms));
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        // 500ms maximum wait
        timer.start(pauselengthinms);
        loop.exec();
    }
}

void MythDisplay::DebugModes(void) const
{
    // This is intentionally formatted to match the output of xrandr for comparison
    if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_INFO))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Available modes:");
        for (auto it = m_videoModes.crbegin(); it != m_videoModes.crend(); ++it)
        {
            auto rates = (*it).RefreshRates();
            QStringList rateslist;
            for (auto it2 = rates.crbegin(); it2 != rates.crend(); ++it2)
                rateslist.append(QString("%1").arg(*it2, 2, 'f', 2, '0'));
            if (rateslist.empty())
                rateslist.append("Variable rate?");
            LOG(VB_PLAYBACK, LOG_INFO, QString("%1x%2\t%3")
                .arg((*it).Width()).arg((*it).Height()).arg(rateslist.join("\t")));
        }
    }
}

/*! \brief Shared static initialistaion code for all MythTV GUI applications.
 *
 * \note This function must be called before Qt/QPA is initialised i.e. before
 * any call to QApplication.
*/
void MythDisplay::ConfigureQtGUI(int SwapInterval)
{
    // Set the default surface format. Explicitly required on some platforms.
    QSurfaceFormat format;
    format.setAlphaBufferSize(0);
    format.setDepthBufferSize(0);
    format.setStencilBufferSize(0);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setSwapInterval(SwapInterval);
    QSurfaceFormat::setDefaultFormat(format);

#ifdef Q_OS_MAC
    // Without this, we can't set focus to any of the CheckBoxSetting, and most
    // of the MythPushButton widgets, and they don't use the themed background.
    QApplication::setDesktopSettingsAware(false);
#endif

    // If Wayland decorations are enabled, the default framebuffer format is forced
    // to use alpha. This framebuffer is rendered with alpha blending by the wayland
    // compositor - so any translucent areas of our UI will allow the underlying
    // window to bleed through.
    // N.B. this is probably not the most performant solution as compositors MAY
    // still render hidden windows. A better solution is probably to call
    // wl_surface_set_opaque_region on the wayland surface. This is confirmed to work
    // and should allow the compositor to optimise rendering for opaque areas. It does
    // however require linking to libwayland-client AND including private Qt headers
    // to retrieve the surface and compositor structures (the latter being a significant issue).
    // see also setAlphaBufferSize above
    setenv("QT_WAYLAND_DISABLE_WINDOWDECORATION", "1", 0);

#if defined (Q_OS_LINUX) && defined (USING_EGL)
    // We want to use EGL for VAAPI/MMAL/DRMPRIME rendering to ensure we
    // can use zero copy video buffers for the best performance (N.B. not tested
    // on AMD desktops). To force Qt to use EGL we must set 'QT_XCB_GL_INTEGRATION'
    // to 'xcb_egl' and this must be done before any GUI is created. If the platform
    // plugin is not xcb then this should have no effect.
    // This does however break when using NVIDIA drivers - which do not support
    // EGL like other drivers so we try to check the EGL vendor - and we currently
    // have no need for EGL with NVIDIA (that may change however).
    // NOTE force using EGL by setting MYTHTV_FORCE_EGL
    // NOTE disable using EGL by setting MYTHTV_NO_EGL
    // NOTE We have no Qt platform information, window/surface or logging when this is called.
    QString soft = qgetenv("LIBGL_ALWAYS_SOFTWARE");
    bool ignore = soft == "1" || soft.compare("true", Qt::CaseInsensitive) == 0;
    bool allow = qgetenv("MYTHTV_NO_EGL").isEmpty() && !ignore;
    bool force = !qgetenv("MYTHTV_FORCE_EGL").isEmpty();
    if (force || allow)
    {
        // N.B. By default, ignore EGL if vendor string is not returned
        QString vendor = MythEGL::GetEGLVendor();
        if (vendor.contains("nvidia", Qt::CaseInsensitive) && !force)
        {
            qInfo() << LOC + QString("Not requesting EGL for vendor '%1'").arg(vendor);
        }
        else if (!vendor.isEmpty() || force)
        {
            qInfo() << LOC + QString("Requesting EGL for '%1'").arg(vendor);
            setenv("QT_XCB_GL_INTEGRATION", "xcb_egl", 0);
        }
    }

    // This makes Xlib calls thread-safe which seems to be required for hardware
    // accelerated Flash playback to work without causing mythfrontend to abort.
    QApplication::setAttribute(Qt::AA_X11InitThreads);
#endif
#ifdef Q_OS_ANDROID
    //QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    // Ignore desktop scaling
    QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
#endif
}
