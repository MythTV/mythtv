#include "myththemebase.h"
#include "mythuiimage.h"
#include "mythmainwindow.h"
#include "mythscreentype.h"
#include "xmlparsebase.h"
#include "mythfontproperties.h"

#include "mythcontext.h"
#include "oldsettings.h"

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
    delete d;
}

void MythThemeBase::Reload(void)
{
    MythMainWindow *mainWindow = GetMythMainWindow();
    QRect uiSize = mainWindow->GetUIScreenRect();

    GetGlobalFontMap()->Clear();
    XMLParseBase::ClearGlobalObjectStore();
    XMLParseBase::LoadBaseTheme();

    d->background->PopScreen();

    d->backgroundscreen = new MythScreenType(d->background, "backgroundscreen");

    if (!XMLParseBase::CopyWindowFromBase("backgroundwindow",
                                          d->backgroundscreen))
    {
        QString backgroundname = gContext->qtconfig()->GetSetting("BackgroundPixmap"
);
        backgroundname = gContext->GetThemeDir() + backgroundname;

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

    XMLParseBase::LoadBaseTheme();
    d->backgroundscreen = new MythScreenType(d->background, "backgroundscreen");

    if (!XMLParseBase::CopyWindowFromBase("backgroundwindow", 
                                          d->backgroundscreen))
    {
        QString backgroundname = gContext->qtconfig()->GetSetting("BackgroundPixmap"
);
        backgroundname = gContext->GetThemeDir() + backgroundname;

        d->backimg = new MythUIImage(backgroundname, d->backgroundscreen,
                                     "backimg");
        d->backimg->SetPosition(mainWindow->NormPoint(QPoint(0, 0)));
        d->backimg->SetSize(uiSize.width(), uiSize.height());
        d->backimg->Load();
    }

    d->background->AddScreen(d->backgroundscreen, false);

    new MythScreenStack(mainWindow, "main stack", true);
}

