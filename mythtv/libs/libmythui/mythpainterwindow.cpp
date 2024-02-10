// Qt
#include <QWindow>
#include <QGuiApplication>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "mythmainwindow.h"
#include "mythpainter_qt.h"
#include "mythpainterwindowqt.h"
#include "mythpainterwindow.h"

#ifdef USING_OPENGL
#include "opengl/mythpainterwindowopengl.h"
#include "opengl/mythpainteropengl.h"
#endif

#ifdef USING_VULKAN
#include "vulkan/mythpainterwindowvulkan.h"
#include "vulkan/mythpaintervulkan.h"
#endif

#ifdef USING_WAYLANDEXTRAS
#include "platforms/mythwaylandextras.h"
#endif

#define MYTH_PAINTER_QT QString("Qt")

using TryPainter = bool(*)(MythMainWindow*, MythPainterWindow*&, MythPainter*&, bool&);

QString MythPainterWindow::GetDefaultPainter()
{
#ifdef USING_OPENGL
    return MYTH_PAINTER_OPENGL;
#elif USING_VULKAN
    return MYTH_PAINTER_VULKAN;
#else
    return MYTH_PAINTER_QT;
#endif
}

QStringList MythPainterWindow::GetPainters()
{
    QStringList result;
#ifdef USING_OPENGL
    result.append(MYTH_PAINTER_OPENGL);
#endif
#ifdef USING_VULKAN
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

#ifdef USING_OPENGL
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

#ifdef USING_VULKAN
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
    // from the command line again (-O ThemePainter=Qt)
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
#ifdef USING_WAYLANDEXTRAS
    if (QGuiApplication::platformName().toLower().contains("wayland"))
        m_waylandDev = new MythWaylandDevice(MainWin);
#endif
}

// NOLINTNEXTLINE(modernize-use-equals-default)
MythPainterWindow::~MythPainterWindow()
{
#ifdef USING_WAYLANDEXTRAS
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

#if defined(DEBUG_PAINTERWIN_EVENTS)
bool MythPainterWindow::event(QEvent *Event)
{
    qInfo() << Event;
    return QWidget::event(Event);
}
#endif

void MythPainterWindow::resizeEvent(QResizeEvent* /*ResizeEvent*/)
{
#ifdef USING_WAYLANDEXTRAS
    if (m_waylandDev)
        m_waylandDev->SetOpaqueRegion(rect());
#endif
}
