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

    if (!MSqlQuery::testDBConnection())
    {
        printf("couldn't open db\n");
        return -1;
    }

    gContext->LoadQtConfig();

    MythMainWindow *mainWindow = GetMythMainWindow();

    mainWindow->Init();
    mainWindow->Show();

    MythScreenStack *background = new MythScreenStack(mainWindow, "background");
    MythScreenType *backgroundscreen = new MythScreenType(background, 
                                                          "backgroundscreen");
    MythUIImage *backimg = new MythUIImage("images/grey.jpeg", backgroundscreen,
                                           "backimg");
    backimg->SetPosition(QPoint(0, 0));
    backimg->Load();

    MythUIImage *logo = new MythUIImage("images/myth_logo.png", 
                                        backgroundscreen, "logo");
    logo->SetPosition(QPoint(20, 170));
    logo->AdjustAlpha(2, 2);
    logo->Load();

    background->AddScreen(backgroundscreen, false);

    MythScreenStack *mainStack = new MythScreenStack(mainWindow, "main stack");

    TestScreen1 *test = new TestScreen1(mainStack, "maintest");
    mainStack->AddScreen(test);

    qApp->setMainWidget(mainWindow);
    qApp->exec();

    return 0;
}
