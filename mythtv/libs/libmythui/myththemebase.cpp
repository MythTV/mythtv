#include "myththemebase.h"
#include "mythuiimage.h"
#include "mythmainwindow.h"
#include "mythscreentype.h"
#include "xmlparsebase.h"

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
    QRect uiSize = GetMythMainWindow()->GetUIScreenRect();

    QString backgroundname = gContext->qtconfig()->GetSetting("BackgroundPixmap"
);
    backgroundname = gContext->GetThemeDir() + backgroundname;

    d->backimg->SetFilename(backgroundname);
    d->backimg->SetPosition(GetMythMainWindow()->NormPoint(QPoint(0, 0)));
    d->backimg->SetSize(uiSize.width(), uiSize.height());
    d->backimg->Load();

    XMLParseBase::ClearGlobalObjectStore();
    XMLParseBase::LoadBaseTheme();
}

void MythThemeBase::Init(void)
{
    MythMainWindow *mainWindow = GetMythMainWindow();

    QRect uiSize = mainWindow->GetUIScreenRect();

    d->background = new MythScreenStack(mainWindow, "background");
    d->backgroundscreen = new MythScreenType(d->background, "backgroundscreen");

    QString backgroundname = gContext->qtconfig()->GetSetting("BackgroundPixmap"
);
    backgroundname = gContext->GetThemeDir() + backgroundname;

    d->backimg = new MythUIImage(backgroundname, d->backgroundscreen,
                                 "backimg");
    d->backimg->SetPosition(mainWindow->NormPoint(QPoint(0, 0)));
    d->backimg->SetSize(uiSize.width(), uiSize.height());
    d->backimg->Load();

    d->background->AddScreen(d->backgroundscreen, false);

    XMLParseBase::LoadBaseTheme();

    new MythScreenStack(mainWindow, "main stack", true);
}

