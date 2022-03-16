// MythTV
#include "libmythbase/mythdirs.h"

#include "myththemebase.h"
#include "mythuiimage.h"
#include "mythmainwindow.h"
#include "mythscreentype.h"
#include "xmlparsebase.h"
#include "mythfontproperties.h"
#include "mythfontmanager.h"
#include "mythuihelper.h"

MythThemeBase::MythThemeBase(MythMainWindow* MainWindow)
{
    m_background = new MythScreenStack(MainWindow, "background");
    m_background->DisableEffects();

    GetGlobalFontManager()->LoadFonts(GetFontsDir(), "Shared");
    GetGlobalFontManager()->LoadFonts(GetMythUI()->GetThemeDir(), "UI");
    XMLParseBase::LoadBaseTheme();
    m_backgroundscreen = new MythScreenType(m_background, "backgroundscreen");

    (void)XMLParseBase::CopyWindowFromBase("backgroundwindow", m_backgroundscreen);
    m_background->AddScreen(m_backgroundscreen, false);
    new MythScreenStack(MainWindow, "main stack", true);
    new MythScreenStack(MainWindow, "popup stack");
}

MythThemeBase::~MythThemeBase()
{
    GetGlobalFontMap()->Clear();
    XMLParseBase::ClearGlobalObjectStore();
    GetGlobalFontManager()->ReleaseFonts("UI");
    GetGlobalFontManager()->ReleaseFonts("Shared");
}

void MythThemeBase::Reload()
{
    GetGlobalFontMap()->Clear();
    XMLParseBase::ClearGlobalObjectStore();
    GetGlobalFontManager()->ReleaseFonts("UI");
    GetGlobalFontManager()->LoadFonts(GetMythUI()->GetThemeDir(), "UI");
    XMLParseBase::LoadBaseTheme();

    m_background->PopScreen(nullptr, false, true);
    m_backgroundscreen = new MythScreenType(m_background, "backgroundscreen");
    (void)XMLParseBase::CopyWindowFromBase("backgroundwindow", m_backgroundscreen);
    m_background->AddScreen(m_backgroundscreen, false);
}
