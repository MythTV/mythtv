#include <qapplication.h>
#include <qsqldatabase.h>
#include <unistd.h>

#include "mythcontext.h"
#include "themedmenu.h"

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    MythContext *context = new MythContext;

    if (argc != 3)
    {
        cout << "usage: menutest [directory] [menu file]\n";
        exit(0);
    }

    ThemedMenu *diag = new ThemedMenu(context, argv[1], argv[2]);

    diag->Show();
    int result = diag->exec();

    cout << diag->getSelection() << endl;

    delete diag;
    delete context;

    return result;
}
