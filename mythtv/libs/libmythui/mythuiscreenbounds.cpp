// Qt
#include <QApplication>
#include <QRegularExpression>

// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"

#include "mythuihelper.h"
#include "mythdisplay.h"
#include "mythuiscreenbounds.h"

int MythUIScreenBounds::s_XOverride = -1;
int MythUIScreenBounds::s_YOverride = -1;
int MythUIScreenBounds::s_WOverride = -1;
int MythUIScreenBounds::s_HOverride = -1;

#define LOC QString("UIBounds: ")

bool MythUIScreenBounds::GeometryIsOverridden()
{
    return (s_XOverride >= 0 || s_YOverride >= 0 || s_WOverride >= 0 || s_HOverride >= 0);
}

QRect MythUIScreenBounds::GetGeometryOverride()
{
    // NB Call GeometryIsOverridden first to ensure this is valid
    return { s_XOverride, s_YOverride, s_WOverride, s_HOverride};
}

/**
 * \brief Parse an X11 style command line geometry string
 *
 * Accepts strings like
 *  -geometry 800x600
 * or
 *  -geometry 800x600+112+22
 * to override the fullscreen and user default screen dimensions
 */
void MythUIScreenBounds::ParseGeometryOverride(const QString& Geometry)
{
    static const QRegularExpression sre("^(\\d+)x(\\d+)$");
    static const QRegularExpression lre(R"(^(\d+)x(\d+)([+-]\d+)([+-]\d+)$)");
    QStringList geometry;
    bool longform = false;

    auto smatch = sre.match(Geometry);
    auto lmatch = lre.match(Geometry);
    if (smatch.hasMatch())
    {
        geometry = smatch.capturedTexts();
    }
    else if (lmatch.hasMatch())
    {
        geometry = lmatch.capturedTexts();
        longform = true;
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Geometry does not match either form - "
                                       "WIDTHxHEIGHT or WIDTHxHEIGHT+XOFF+YOFF");
        return;
    }

    bool parsed = false;
    int height = 0;
    int width = geometry[1].toInt(&parsed);
    if (!parsed)
        LOG(VB_GENERAL, LOG_ERR, LOC + "Could not parse width of geometry override");

    if (parsed)
    {
        height = geometry[2].toInt(&parsed);
        if (!parsed)
            LOG(VB_GENERAL, LOG_ERR, LOC + "Could not parse height of geometry override");
    }

    if (parsed)
    {
        s_WOverride = width;
        s_HOverride = height;
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Overriding GUI size: %1x%2")
            .arg(width).arg(height));
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to override GUI size.");
    }

    if (longform)
    {
        int x = geometry[3].toInt(&parsed);
        if (!parsed)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Could not parse horizontal offset of geometry override");
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to override GUI offset.");
            return;
        }

        int y = geometry[4].toInt(&parsed);
        if (!parsed)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Could not parse vertical offset of geometry override");
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to override GUI offset.");
            return;
        }

        s_XOverride = x;
        s_YOverride = y;
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Overriding GUI offset: %1+%2").arg(x).arg(y));
    }
}

/// \brief Return true if the current platform only supports fullscreen windows
bool MythUIScreenBounds::WindowIsAlwaysFullscreen()
{
#ifdef Q_OS_ANDROID
    return true;
#else
    // this may need to cover other platform plugins
    return QGuiApplication::platformName().toLower().contains("eglfs");
#endif
}

MythUIScreenBounds::MythUIScreenBounds()
{
    InitScreenBounds();
}

void MythUIScreenBounds::InitScreenBounds()
{
    m_forceFullScreen = gCoreContext->GetBoolSetting("ForceFullScreen", false);
    m_wantFullScreen = m_forceFullScreen ||
                       (gCoreContext->GetNumSetting("GuiOffsetX") == 0 &&
                        gCoreContext->GetNumSetting("GuiWidth")   == 0 &&
                        gCoreContext->GetNumSetting("GuiOffsetY") == 0 &&
                        gCoreContext->GetNumSetting("GuiHeight")  == 0);
    m_wantWindow   = gCoreContext->GetBoolSetting("RunFrontendInWindow", false) && !m_forceFullScreen;
    m_qtFullScreen = WindowIsAlwaysFullscreen();
    m_alwaysOnTop  = gCoreContext->GetBoolSetting("AlwaysOnTop", false);
    m_themeSize    = GetMythUI()->GetBaseSize();
}

void MythUIScreenBounds::UpdateScreenSettings(MythDisplay *mDisplay)
{
    if (s_XOverride >= 0 && s_YOverride >= 0)
    {
        gCoreContext->OverrideSettingForSession("GuiOffsetX", QString::number(s_XOverride));
        gCoreContext->OverrideSettingForSession("GuiOffsetY", QString::number(s_YOverride));
    }

    if (s_WOverride > 0 && s_HOverride > 0)
    {
        gCoreContext->OverrideSettingForSession("GuiWidth", QString::number(s_WOverride));
        gCoreContext->OverrideSettingForSession("GuiHeight", QString::number(s_HOverride));
    }

    int x = gCoreContext->GetNumSetting("GuiOffsetX");
    int y = gCoreContext->GetNumSetting("GuiOffsetY");
    int width = 0;
    int height = 0;
    gCoreContext->GetResolutionSetting("Gui", width, height);

    QRect screenbounds = mDisplay->GetScreenBounds();

    // As per MythMainWindow::Init, fullscreen is indicated by all zero's in settings
    if (m_forceFullScreen || (x == 0 && y == 0 && width == 0 && height == 0))
        m_screenRect = screenbounds;
    else
        m_screenRect = QRect(x, y, width, height);

    if (m_screenRect.width() < 160 || m_screenRect.height() < 160)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Strange screen size: %1x%2 - forcing 640x480")
            .arg(m_screenRect.width()).arg(m_screenRect.height()));
        m_screenRect.setSize(QSize(640, 480));
    }

    m_screenHorizScale = m_screenRect.width()  / static_cast<float>(m_themeSize.width());
    m_screenVertScale  = m_screenRect.height() / static_cast<float>(m_themeSize.height());

    GetMythUI()->SetScreenSize(m_screenRect.size());

    // Default font, _ALL_ fonts inherit from this!
    // e.g All fonts will be 19 pixels unless a new size is explicitly defined.
    QFont font = QFont("Arial");
    if (!font.exactMatch())
        font = QFont();

    font.setStyleHint(QFont::SansSerif, QFont::PreferAntialias);
    font.setPixelSize(static_cast<int>(lroundf(19.0F * m_screenHorizScale)));
    int stretch = static_cast<int>(lround(100.0 / mDisplay->GetPixelAspectRatio()));
    font.setStretch(stretch); // QT
    m_fontStretch = stretch; // MythUI
    QApplication::setFont(font);
}

QRect MythUIScreenBounds::GetUIScreenRect()
{
    return m_uiScreenRect;
}

void MythUIScreenBounds::SetUIScreenRect(const QRect Rect)
{
    if (Rect == m_uiScreenRect)
        return;
    m_uiScreenRect = Rect;
    LOG(VB_GENERAL, LOG_INFO, LOC +  QString("New UI bounds: %1x%2+%3+%4")
        .arg(m_uiScreenRect.width()).arg(m_uiScreenRect.height())
        .arg(m_uiScreenRect.left()).arg(m_uiScreenRect.top()));
    emit UIScreenRectChanged(m_uiScreenRect);
}

QRect MythUIScreenBounds::GetScreenRect()
{
    return m_screenRect;
}

QSize MythUIScreenBounds::NormSize(const QSize Size) const
{
    QSize result;
    result.setWidth(static_cast<int>(Size.width() * m_screenHorizScale));
    result.setHeight(static_cast<int>(Size.height() * m_screenVertScale));
    return result;
}

int MythUIScreenBounds::NormX(int X) const
{
    return qRound(X * m_screenHorizScale);
}

int MythUIScreenBounds::NormY(int Y) const
{
    return qRound(Y * m_screenVertScale);
}

void MythUIScreenBounds::GetScalingFactors(float& Horizontal, float& Vertical) const
{
    Horizontal = m_screenHorizScale;
    Vertical   = m_screenVertScale;
}

void MythUIScreenBounds::SetScalingFactors(float Horizontal,  float  Vertical)
{
    m_screenHorizScale = Horizontal;
    m_screenVertScale  = Vertical;
}

QSize MythUIScreenBounds::GetThemeSize()
{
    return m_themeSize;
}

int MythUIScreenBounds::GetFontStretch() const
{
    return m_fontStretch;
}

void MythUIScreenBounds::SetFontStretch(int Stretch)
{
    m_fontStretch = Stretch;
}
