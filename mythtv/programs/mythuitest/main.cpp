#include <qapplication.h>
#include <qsqldatabase.h>

#include <iostream>
using namespace std;

#include "mythmainwindow.h"
#include "mythuiimage.h"
#include "mythscreenstack.h"
#include "mythscreentype.h"
#include "myththemebase.h"

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

    new MythThemeBase();

    MythScreenStack *mainStack = mainWindow->GetMainStack();

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
