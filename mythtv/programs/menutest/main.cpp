#include <qapplication.h>
#include <qsqldatabase.h>
#include <unistd.h>

#include <iostream>
using namespace std;

#include "mythcontext.h"
#include "themedmenu.h"

MythContext *gContext;

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    gContext = new MythContext(MYTH_BINARY_VERSION);

    if (argc != 3)
    {
        cout << "usage: menutest [directory] [menu file]\n";
        exit(0);
    }

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!gContext->OpenDatabase(db))
    {
        printf("couldn't open db\n");
        return -1;
    }

    gContext->LoadQtConfig();

    MythMainWindow *mainWindow = new MythMainWindow();
    mainWindow->Show();
    gContext->SetMainWindow(mainWindow);

    ThemedMenu *diag = new ThemedMenu(argv[1], argv[2], mainWindow, "menutest");

    diag->Show();
    int result = diag->exec();

    cout << diag->getSelection() << endl;

    delete diag;
    delete gContext;

    return result;
}
