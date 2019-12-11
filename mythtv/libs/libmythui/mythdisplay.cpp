//Qt
#include <QTimer>
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
#ifdef USING_DRM
#include "mythdisplaydrm.h"
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

MythDisplay::MythDisplay()
  : ReferenceCounter("Display")
{
    m_screen = GetDesiredScreen();
    DebugScreen(m_screen, "Using");
    if (m_screen)
        connect(m_screen, &QScreen::geometryChanged, this, &MythDisplay::GeometryChanged);

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
        return;
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
        return;
    }

    LOG(VB_GENERAL, LOG_WARNING, LOC + "Widget does not have a window!");
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

void MythDisplay::GeometryChanged(const QRect &Geo)
{
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("New screen geometry: %1x%2+%3+%4")
        .arg(Geo.width()).arg(Geo.height()).arg(Geo.left()).arg(Geo.top()));
}

float MythDisplay::SanitiseRefreshRate(int Rate)
{
    static const float kDefaultRate = 1000000.0F / 60.0F;
    float fixed = kDefaultRate;
    if (Rate > 0)
    {
        fixed = static_cast<float>(Rate) / 2.0F;
        if (fixed < kDefaultRate)
            fixed = kDefaultRate;
    }
    return fixed;
}

DisplayInfo MythDisplay::GetDisplayInfo(int /*VideoRate*/)
{
    // This is the final fallback when no other platform specifics are available
    // It is usually accurate apart from the refresh rate - which is often
    // rounded down.
    QScreen *screen = GetCurrentScreen();
    DisplayInfo ret;
    if (!screen)
        return ret;
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
    if (!qScreen)
        return;

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
    m_mode[DESKTOP] = MythDisplayMode(pixelwidth, pixelheight, mmwidth, mmheight, -1.0, refreshrate);
    LOG(VB_GENERAL, LOG_NOTICE, LOC + QString("Desktop video mode: %1x%2 %3 Hz")
        .arg(pixelwidth).arg(pixelheight).arg(refreshrate, 0, 'f', 3));

    // Initialize GUI mode
    m_mode[GUI].Init();
    pixelwidth = pixelheight = 0;
    double aspectratio = 0.0;
    GetMythDB()->GetResolutionSetting("GuiVidMode", pixelwidth, pixelheight, aspectratio, refreshrate);
    GetMythDB()->GetResolutionSetting("DisplaySize", mmwidth, mmheight);
    m_mode[GUI] = MythDisplayMode(pixelwidth, pixelheight, mmwidth, mmheight, -1.0, refreshrate);

    if (m_mode[DESKTOP] == m_mode[GUI] && qFuzzyIsNull(refreshrate))
    {
        // same resolution as current desktop
        m_last = m_mode[DESKTOP];
    }

    // Initialize default VIDEO mode
    pixelwidth = pixelheight = 0;
    GetMythDB()->GetResolutionSetting("TVVidMode", pixelwidth, pixelheight, aspectratio, refreshrate);
    m_mode[VIDEO] = MythDisplayMode(pixelwidth, pixelheight, mmwidth, mmheight, aspectratio, refreshrate);

    // Initialize video override mode
    m_inSizeToOutputMode.clear();

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

        if (!(iw || ih || !qFuzzyIsNull(irate)))
            break;

        if (!(ih && ow && oh))
            break;

        uint64_t key = MythDisplayMode::CalcKey(iw, ih, irate);

        MythDisplayMode scr(ow, oh, mmwidth, mmheight, oaspect, orate);

        m_inSizeToOutputMode[key] = scr;
    }
}

/*! \brief Check whether the next mode is smaller in size than the current mode.
 *
 * This is used to allow the caller to force an update of the main window to ensure
 * the window is fully resized and is in no way clipped. Not an issue if the next
 * mode is smaller.
*/
bool MythDisplay::NextModeIsLarger(Mode NextMode)
{
    MythDisplayMode next = m_mode[NextMode];
    return next.Width() > m_last.Width() || next.Height() > m_last.Height();
}

bool MythDisplay::NextModeIsLarger(int Width, int Height)
{
    return Width > m_last.Width() || Height > m_last.Height();
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
    Mode nextmode = VIDEO; // default VIDEO mode
    MythDisplayMode next = m_mode[nextmode];
    double targetrate = 0.0;

    // try to find video override mode
    uint64_t key = MythDisplayMode::FindBestScreen(m_inSizeToOutputMode,
                   Width, Height, Rate);

    if (key != 0)
    {
        m_mode[nextmode = CUSTOM_VIDEO] = next = m_inSizeToOutputMode[key];
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
    MythDisplayMode::FindBestMatch(GetVideoModes(), next, targetrate);

    bool chg = !(next == m_last) || !(MythDisplayMode::CompareRates(m_last.RefreshRate(), targetrate));

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("%1 %2x%3 %4 Hz")
        .arg(chg ? "Changing to" : "Using")
        .arg(next.Width()).arg(next.Height())
        .arg(targetrate, 0, 'f', 3));

    if (chg && !SwitchToVideoMode(next.Width(), next.Height(), targetrate))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("SwitchToVideo: Video size %1 x %2: "
                                               "xrandr failed for %3 x %4")
            .arg(Width).arg(Height).arg(next.Width()).arg(next.Height()));
        return false;
    }

    m_curMode = nextmode;

    m_last = next;
    m_last.SetRefreshRate(targetrate);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("SwitchToVideo: Video size %1x%2")
        .arg(Width).arg(Height));
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("%1 displaying resolution %2x%3, %4mmx%5mm")
        .arg((chg) ? "Switched to" : "Already")
        .arg(m_last.Width()).arg(m_last.Height())
        .arg(m_last.WidthMM()).arg(m_last.HeightMM()));

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
bool MythDisplay::SwitchToGUI(Mode NextMode, bool Wait)
{
    MythDisplayMode next = m_mode[NextMode];
    auto targetrate = static_cast<double>(NAN);

    // need to change video mode?
    // If GuiVidModeRefreshRate is 0, assume any refresh rate is good enough.
    if (qFuzzyIsNull(next.RefreshRate()))
        next.SetRefreshRate(m_last.RefreshRate());

    MythDisplayMode::FindBestMatch(GetVideoModes(), next, targetrate);
    bool chg = !(next == m_last) || !(MythDisplayMode::CompareRates(m_last.RefreshRate(), targetrate));

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("%1 %2x%3 %4 Hz")
        .arg(chg ? "Changing to" : "Using")
        .arg(next.Width()).arg(next.Height())
        .arg(targetrate, 0, 'f', 3));

    if (chg && !SwitchToVideoMode(next.Width(), next.Height(), targetrate))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("SwitchToGUI: xrandr failed for %1x%2 %3  Hz")
            .arg(next.Width()).arg(next.Height())
            .arg(next.RefreshRate(), 0, 'f', 3));
        return false;
    }

    if (Wait && (next.Width() != m_last.Width() || next.Height() != m_last.Height()))
        WaitForScreenChange();

    m_curMode = NextMode;

    m_last = next;
    m_last.SetRefreshRate(targetrate);

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

    const MythDisplayMode drs(Width, Height, 0, 0, -1.0, 0.0);
    const DisplayModeVector& drv = GetVideoModes();
    int t = MythDisplayMode::FindBestMatch(drv, drs, tr);

    if (t < 0)
        return empty;

    return drv[static_cast<size_t>(t)].RefreshRates();
}

bool MythDisplay::SwitchToVideoMode(int /*Width*/, int /*Height*/, double /*FrameRate*/)
{
    return false;
}

const DisplayModeVector& MythDisplay::GetVideoModes(void)
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
    auto result = GetAspectRatio();

    QList<QScreen*> screens;
    if (m_screen)
        screens = m_screen->virtualSiblings();
    if (screens.size() < 2)
        return result;

    // N.B. This sorting may not be needed
    std::sort(screens.begin(), screens.end(), sortscreens);
    QList<double> aspectratios;
    int lasttop = 0;
    int lastleft = 0;
    int rows = 1;
    int columns = 1;
    for (auto it = screens.constBegin() ; it != screens.constEnd(); ++it)
    {
        QRect geom = (*it)->geometry();
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
            LOC + QString("Unsupported layout - defaulting to %1")
            .arg(result));
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
                QString("Inconsistent aspect ratios - defaulting to %1")
                .arg(result));
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
    return QSize(m_last.Width(), m_last.Height());
}

QSize MythDisplay::GetPhysicalSize(void)
{
    return QSize(m_last.WidthMM(), m_last.HeightMM());
}

void MythDisplay::WaitForScreenChange(void)
{
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
        auto it = m_videoModes.crbegin();
        for ( ; it != m_videoModes.crend(); ++it)
        {
            auto rates = (*it).RefreshRates();
            QStringList rateslist;
            auto it2 = rates.crbegin();
            for ( ; it2 != rates.crend(); ++it2)
                rateslist.append(QString("%1").arg(*it2, 2, 'f', 2, '0'));
            LOG(VB_PLAYBACK, LOG_INFO, QString("%1x%2\t%3")
                .arg((*it).Width()).arg((*it).Height()).arg(rateslist.join("\t")));
        }
    }
}

