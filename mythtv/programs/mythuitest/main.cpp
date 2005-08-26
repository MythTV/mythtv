#include <qapplication.h>
#include <qsqldatabase.h>

#include <iostream>
using namespace std;

#include "mythmainwindow.h"
#include "mythuiimage.h"
#include "mythscreenstack.h"
#include "mythscreentype.h"
#include "mythcontext.h"
#include "test1.h"
#include "oldsettings.h"
//#include "btnlisttest.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    gContext->Init();

    QString themename = gContext->GetSetting("Theme");
    QString themedir = gContext->FindThemeDir(themename);
    if (themedir == "")
    {
        cerr << "Couldn't find theme " << themename << endl;
        exit(0);
    }

    gContext->LoadQtConfig();

    MythMainWindow *mainWindow = GetMythMainWindow();

    mainWindow->Init();
    mainWindow->Show();

    QRect uiSize = mainWindow->GetUIScreenRect();

    MythScreenStack *background = new MythScreenStack(mainWindow, "background");
    MythScreenType *backgroundscreen = new MythScreenType(background, 
                                                          "backgroundscreen");
    QString backgroundname = gContext->qtconfig()->GetSetting("BackgroundPixmap");
    backgroundname = gContext->GetThemeDir() + backgroundname;

    MythUIImage *backimg = new MythUIImage(backgroundname, backgroundscreen,
                                           "backimg");
    backimg->SetPosition(mainWindow->NormPoint(QPoint(0, 0)));
    backimg->SetSize(uiSize.width(), uiSize.height());
    backimg->Load();

    MythUIImage *logo = new MythUIImage("images/myth_logo.png", 
                                        backgroundscreen, "logo");
    QPoint logoPos = QPoint(mainWindow->NormX(20), 
                            uiSize.height() - mainWindow->NormY(430));
    logo->SetPosition(logoPos);
    logo->AdjustAlpha(2, 2);
    logo->Load();

    background->AddScreen(backgroundscreen, false);

    MythScreenStack *mainStack = new MythScreenStack(mainWindow, "main stack");

#ifdef BTNLISTTEST_H
    TestWindow *test = new TestWindow(mainStack);
#else
    TestScreen1 *test = new TestScreen1(mainStack, "maintest");
#endif
    mainStack->AddScreen(test);

    qApp->setMainWidget(mainWindow);
    qApp->exec();

    return 0;
}
