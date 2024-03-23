// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythscreentype.h"
#include "mythmediaoverlay.h"

// Std
#include <cmath>

#define LOC QString("Overlay: ")

MythOverlayWindow::MythOverlayWindow(MythScreenStack* Parent, MythPainter* Painter,
                             const QString& Name, bool Themed)
  : MythScreenType(Parent, Name, true),
    m_themed(Themed)
{
    m_painter = Painter;
}

bool MythOverlayWindow::Create()
{
    if (m_themed)
        return XMLParseBase::LoadWindowFromXML("osd.xml", objectName(), this);
    return false;
}

MythMediaOverlay::MythMediaOverlay(MythMainWindow *MainWindow, TV *Tv, MythPlayerUI *Player, MythPainter *Painter)
  : m_mainWindow(MainWindow),
    m_tv(Tv),
    m_player(Player),
    m_painter(Painter)
{
}

MythMediaOverlay::~MythMediaOverlay()
{
    MythMediaOverlay::TearDown();
}

void MythMediaOverlay::SetPlayer(MythPlayerUI *Player)
{
    m_player = Player;
}

void MythMediaOverlay::TearDown()
{
    for (MythScreenType * screen : std::as_const(m_children))
        delete screen;
    m_children.clear();
}

QRect MythMediaOverlay::Bounds() const
{
    return m_rect;
}

int MythMediaOverlay::GetFontStretch() const
{
    return m_fontStretch;
}

bool MythMediaOverlay::Init(QRect Rect, float FontAspect)
{
    int newstretch = static_cast<int>(lroundf(FontAspect * 100));
    if (!(Rect == m_rect) || (newstretch != m_fontStretch))
    {
        TearDown();
        m_rect = Rect;
        m_fontStretch = newstretch;
    }
    return true;
}

void MythMediaOverlay::HideWindow(const QString& Window)
{
    if (!m_children.contains(Window))
        return;

    MythScreenType *screen = m_children.value(Window);
    if (screen != nullptr)
    {
        screen->SetVisible(false);
        screen->Close(); // for InteractiveScreen
    }
}

bool MythMediaOverlay::HasWindow(const QString& Window)
{
    return m_children.contains(Window);
}

MythScreenType* MythMediaOverlay::GetWindow(const QString &Window)
{
    if (m_children.contains(Window))
        return m_children.value(Window);

    return InitWindow(Window, new MythOverlayWindow(nullptr, m_painter, Window, false));
}

MythScreenType *MythMediaOverlay::InitWindow(const QString& Window, MythScreenType* Screen)
{
    if (Screen)
    {
        if (Screen->Create())
        {
            m_children.insert(Window, Screen);
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Created window %1").arg(Window));
        }
        else
        {
            delete Screen;
            Screen = nullptr;
        }
    }

    if (!Screen)
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to create window %1").arg(Window));
    return Screen;
}

void MythMediaOverlay::OverrideUIScale(bool Log)
{
    // Avoid unnecessary switches
    QRect uirect = m_mainWindow->GetUIScreenRect();
    if (uirect == m_rect)
        return;

    // Save current data
    m_savedUIRect = uirect;
    m_savedFontStretch = m_mainWindow->GetFontStretch();
    m_mainWindow->GetScalingFactors(m_savedWMult, m_savedHMult);

    // Calculate new
    QSize themesize = m_mainWindow->GetThemeSize();
    float wmult = static_cast<float>(m_rect.size().width()) / static_cast<float>(themesize.width());
    float mult = static_cast<float>(m_rect.size().height()) / static_cast<float>(themesize.height());
    if (Log)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Base theme size: %1x%2")
            .arg(themesize.width()).arg(themesize.height()));
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Scaling factors: %1x%2")
            .arg(static_cast<double>(wmult)).arg(static_cast<double>(mult)));
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Font stretch: saved %1 new %2")
            .arg(m_savedFontStretch).arg(m_fontStretch));
    }
    m_uiScaleOverride = true;

    // Apply new
    m_mainWindow->SetFontStretch(m_fontStretch);
    m_mainWindow->SetScalingFactors(wmult, mult);
    m_mainWindow->SetUIScreenRect(m_rect);
}

void MythMediaOverlay::RevertUIScale()
{
    if (m_uiScaleOverride)
    {
        m_mainWindow->SetFontStretch(m_savedFontStretch);
        m_mainWindow->SetScalingFactors(m_savedWMult, m_savedHMult);
        m_mainWindow->SetUIScreenRect(m_savedUIRect);
    }
    m_uiScaleOverride = false;
}
