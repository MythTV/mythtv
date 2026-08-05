// Qt
#include <QWindow>
#include <QGuiApplication>

// MythTV
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythappname.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"
#include "mythmainwindow.h"
#include "mythpainter_qt.h"
#include "mythpainterwindowqt.h"
#include "mythpainterwindow.h"

#if CONFIG_OPENGL
#include "opengl/mythpainterwindowopengl.h"
#include "opengl/mythpainteropengl.h"
#endif

#if CONFIG_VULKAN
#include "vulkan/mythpainterwindowvulkan.h"
#include "vulkan/mythpaintervulkan.h"
#endif

#if CONFIG_WAYLANDEXTRAS
#include "platforms/mythwaylandextras.h"
#endif

#define MYTH_PAINTER_QT QString("Qt")

using TryPainter = bool(*)(MythMainWindow*, MythPainterWindow*&, MythPainter*&, bool&);

QString MythPainterWindow::GetDefaultPainter()
{
#if CONFIG_OPENGL
    return MYTH_PAINTER_OPENGL;
#elif CONFIG_VULKAN
    return MYTH_PAINTER_VULKAN;
#else
    return MYTH_PAINTER_QT;
#endif
}

QStringList MythPainterWindow::GetPainters()
{
    QStringList result;
#if CONFIG_OPENGL
    result.append(MYTH_PAINTER_OPENGL);
#endif
#if CONFIG_VULKAN
    result.append(MYTH_PAINTER_VULKAN);
#endif
    return result;
}

QString MythPainterWindow::CreatePainters(MythMainWindow *MainWin,
                                          MythPainterWindow *&PaintWin,
                                          MythPainter *&Paint)
{
    bool warn = false;
    QString painter = GetMythDB()->GetSetting("PaintEngine", GetDefaultPainter());

    // build a prioritised list of painters to try
    QVector<TryPainter> painterstotry;

#if CONFIG_OPENGL
    auto TryOpenGL = [](MythMainWindow *MainWindow, MythPainterWindow *&PaintWindow,
                        MythPainter *&Painter, bool& /*unused*/)
    {
        auto* glwindow = new MythPainterWindowOpenGL(MainWindow);
        if (glwindow && glwindow->IsValid())
        {
            PaintWindow = glwindow;
            auto *render = dynamic_cast<MythRenderOpenGL*>(glwindow->GetRenderDevice());
            Painter = new MythOpenGLPainter(render, MainWindow);
            return true;
        }
        delete glwindow;
        return false;
    };

    if (painter.contains(MYTH_PAINTER_OPENGL, Qt::CaseInsensitive))
        painterstotry.prepend(TryOpenGL);
    else
        painterstotry.append(TryOpenGL);
#endif

#if CONFIG_VULKAN
    auto TryVulkan = [](MythMainWindow *MainWindow, MythPainterWindow *&PaintWindow,
                        MythPainter *&Painter, bool& /*unused*/)
    {
        auto *vulkan = new MythPainterWindowVulkan(MainWindow);
        if (vulkan && vulkan->IsValid())
        {
            PaintWindow = vulkan;
            auto *render = dynamic_cast<MythRenderVulkan*>(vulkan->GetRenderDevice());
            Painter = new MythPainterVulkan(render, MainWindow);
            return true;
        }
        delete vulkan;
        return false;
    };

    if (painter.contains(MYTH_PAINTER_VULKAN, Qt::CaseInsensitive))
        painterstotry.prepend(TryVulkan);
    else
        painterstotry.append(TryVulkan);
#endif

    // Fallback to Qt painter as the last resort.
    auto TryQt = [](MythMainWindow *MainWindow, MythPainterWindow *&PaintWindow,
                    MythPainter *&Painter, bool& Warn)
    {
        LOG(VB_GENERAL, LOG_INFO, "Using the Qt painter. Video playback will not work!");
        Painter = new MythQtPainter();
        PaintWindow = new MythPainterWindowQt(MainWindow);
        Warn = QCoreApplication::applicationName() == MYTH_APPNAME_MYTHFRONTEND;
        return true;
    };

    // N.B. this won't be selectable as a painter in the UI but can be forced
    // from the command line again (-O PaintEngine=Qt)
    if (painter.contains(MYTH_PAINTER_QT, Qt::CaseInsensitive))
        painterstotry.prepend(TryQt);
    else
        painterstotry.append(TryQt);

    for (auto & trypainter : painterstotry)
        if (trypainter(MainWin, PaintWin, Paint, warn))
            break;

    return warn ? tr("Warning: No GPU acceleration") : QString();
}

void MythPainterWindow::DestroyPainters(MythPainterWindow *&PaintWin, MythPainter *&Painter)
{
    delete Painter;
    delete PaintWin;
    Painter = nullptr;
    PaintWin = nullptr;
}

MythPainterWindow::MythPainterWindow(MythMainWindow *MainWin)
  : QWidget(MainWin)
{
#if CONFIG_WAYLANDEXTRAS
    if (QGuiApplication::platformName().toLower().contains("wayland"))
        m_waylandDev = new MythWaylandDevice(MainWin);
#endif
}

// NOLINTNEXTLINE(modernize-use-equals-default)
MythPainterWindow::~MythPainterWindow()
{
#if CONFIG_WAYLANDEXTRAS
    delete m_waylandDev;
#endif
}

MythRender* MythPainterWindow::GetRenderDevice()
{
    return m_render;
}

bool MythPainterWindow::RenderIsShared()
{
    return m_render && m_render->IsShared();
}

#ifdef DEBUG_PAINTERWIN_EVENTS
bool MythPainterWindow::event(QEvent *Event)
{
    qInfo() << Event;
    return QWidget::event(Event);
}
#endif

void MythPainterWindow::resizeEvent(QResizeEvent* /*ResizeEvent*/)
{
#if CONFIG_WAYLANDEXTRAS
    if (m_waylandDev)
        m_waylandDev->SetOpaqueRegion(rect());
#endif
}

#include "moc_mythpainterwindow.cpp"
