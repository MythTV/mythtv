#include <qapplication.h>
#include <qsqldatabase.h>
#include <unistd.h>

#include "themedmenu.h"
#include "settings.h"

Settings *globalsettings;
char installprefix[] = PREFIX;

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    globalsettings = new Settings;

    globalsettings->LoadSettingsFiles("theme.txt", installprefix);
    globalsettings->LoadSettingsFiles("mysql.txt", installprefix);

    if (argc != 3)
    {
        cout << "usage: menutest [directory] [menu file]\n";
        exit(0);
    }

    ThemedMenu *diag = new ThemedMenu(argv[1], installprefix, argv[2]);

    diag->Show();
    int result = diag->exec();

    cout << diag->getSelection() << endl;

    delete diag;

    return result;
}
