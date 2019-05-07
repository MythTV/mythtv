#include "myththemebase.h"
#include "mythuiimage.h"
#include "mythmainwindow.h"
#include "mythscreentype.h"
#include "xmlparsebase.h"
#include "mythfontproperties.h"
#include "mythfontmanager.h"

#include "mythdirs.h"
#include "mythuihelper.h"

class MythThemeBasePrivate
{
  public:
    MythScreenStack *m_background       {nullptr};
    MythScreenType  *m_backgroundscreen {nullptr};
};


MythThemeBase::MythThemeBase()
{
    d = new MythThemeBasePrivate();

    Init();
}

MythThemeBase::~MythThemeBase()
{
    GetGlobalFontMap()->Clear();
    XMLParseBase::ClearGlobalObjectStore();
    GetGlobalFontManager()->ReleaseFonts("UI");
    GetGlobalFontManager()->ReleaseFonts("Shared");
    delete d;
}

void MythThemeBase::Reload(void)
{
    GetGlobalFontMap()->Clear();
    XMLParseBase::ClearGlobalObjectStore();
    GetGlobalFontManager()->ReleaseFonts("UI");
    GetGlobalFontManager()->LoadFonts(GetMythUI()->GetThemeDir(), "UI");
    XMLParseBase::LoadBaseTheme();

    d->m_background->PopScreen(nullptr, false, true);

    d->m_backgroundscreen = new MythScreenType(d->m_background, "backgroundscreen");

    if (!XMLParseBase::CopyWindowFromBase("backgroundwindow",
                                          d->m_backgroundscreen))
    {
        // Nada. All themes should use the MythUI code now.
    }

    d->m_background->AddScreen(d->m_backgroundscreen, false);
}

void MythThemeBase::Init(void)
{
    MythMainWindow *mainWindow = GetMythMainWindow();

    d->m_background = new MythScreenStack(mainWindow, "background");
    d->m_background->DisableEffects();

    GetGlobalFontManager()->LoadFonts(GetFontsDir(), "Shared");
    GetGlobalFontManager()->LoadFonts(GetMythUI()->GetThemeDir(), "UI");
    XMLParseBase::LoadBaseTheme();
    d->m_backgroundscreen = new MythScreenType(d->m_background, "backgroundscreen");

    if (!XMLParseBase::CopyWindowFromBase("backgroundwindow",
                                          d->m_backgroundscreen))
    {
        // Nada. All themes should use the MythUI code now.
    }

    d->m_background->AddScreen(d->m_backgroundscreen, false);

    new MythScreenStack(mainWindow, "main stack", true);

    new MythScreenStack(mainWindow, "popup stack");
}
