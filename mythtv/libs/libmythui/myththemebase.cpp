#include "myththemebase.h"
#include "mythuiimage.h"
#include "mythmainwindow.h"
#include "mythscreentype.h"
#include "xmlparsebase.h"
#include "mythfontproperties.h"
#include "mythfontmanager.h"

#include "mythdirs.h"
#include "oldsettings.h"
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
    MythMainWindow *mainWindow = GetMythMainWindow();
    QRect uiSize = mainWindow->GetUIScreenRect();

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
        QString backgroundname = GetMythUI()->qtconfig()->GetSetting("BackgroundPixmap"
);
        backgroundname = GetMythUI()->GetThemeDir() + backgroundname;

        d->backimg = new MythUIImage(backgroundname, d->backgroundscreen,
                                     "backimg");
        d->backimg->SetPosition(mainWindow->NormPoint(QPoint(0, 0)));
        d->backimg->SetSize(uiSize.width(), uiSize.height());
        d->backimg->Load();
    }

    d->background->AddScreen(d->backgroundscreen, false);
}

void MythThemeBase::Init(void)
{
    MythMainWindow *mainWindow = GetMythMainWindow();
    QRect uiSize = mainWindow->GetUIScreenRect();

    d->background = new MythScreenStack(mainWindow, "background");
    d->background->DisableEffects();

    GetGlobalFontManager()->LoadFonts(GetFontsDir(), "Shared");
    GetGlobalFontManager()->LoadFonts(GetMythUI()->GetThemeDir(), "UI");
    XMLParseBase::LoadBaseTheme();
    d->backgroundscreen = new MythScreenType(d->background, "backgroundscreen");

    if (!XMLParseBase::CopyWindowFromBase("backgroundwindow",
                                          d->backgroundscreen))
    {
        QString backgroundname = GetMythUI()->qtconfig()->GetSetting("BackgroundPixmap"
);
        backgroundname = GetMythUI()->GetThemeDir() + backgroundname;

        d->backimg = new MythUIImage(backgroundname, d->backgroundscreen,
                                     "backimg");
        d->backimg->SetPosition(mainWindow->NormPoint(QPoint(0, 0)));
        d->backimg->SetSize(uiSize.width(), uiSize.height());
        d->backimg->Load();
    }

    d->background->AddScreen(d->backgroundscreen, false);

    new MythScreenStack(mainWindow, "main stack", true);

    new MythScreenStack(mainWindow, "popup stack");
}

