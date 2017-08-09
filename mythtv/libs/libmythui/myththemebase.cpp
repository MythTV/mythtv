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
    MythScreenStack *background;
    MythScreenType *backgroundscreen;

    MythUIImage *backimg; // just for now
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

    d->background->PopScreen(NULL, false, true);

    d->backgroundscreen = new MythScreenType(d->background, "backgroundscreen");

    if (!XMLParseBase::CopyWindowFromBase("backgroundwindow",
                                          d->backgroundscreen))
    {
        // Nada. All themes should use the MythUI code now.
    }

    d->background->AddScreen(d->backgroundscreen, false);
}

void MythThemeBase::Init(void)
{
    MythMainWindow *mainWindow = GetMythMainWindow();

    d->background = new MythScreenStack(mainWindow, "background");
    d->background->DisableEffects();

    GetGlobalFontManager()->LoadFonts(GetFontsDir(), "Shared");
    GetGlobalFontManager()->LoadFonts(GetMythUI()->GetThemeDir(), "UI");
    XMLParseBase::LoadBaseTheme();
    d->backgroundscreen = new MythScreenType(d->background, "backgroundscreen");

    if (!XMLParseBase::CopyWindowFromBase("backgroundwindow",
                                          d->backgroundscreen))
    {
        // Nada. All themes should use the MythUI code now.
    }

    d->background->AddScreen(d->backgroundscreen, false);

    new MythScreenStack(mainWindow, "main stack", true);

    new MythScreenStack(mainWindow, "popup stack");
}
