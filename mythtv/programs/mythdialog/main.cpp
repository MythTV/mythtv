#include <qapplication.h>

#include "libmyth/dialogbox.h"
#include "libmyth/settings.h"

Settings *globalsettings;
char installprefix[] = "/usr/local";

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    if (argc < 3)
    {
        printf("Not enough args to run with\n");
        return 0;
    }

    globalsettings = new Settings;

    globalsettings->LoadSettingsFiles("theme.txt", installprefix);
    globalsettings->LoadSettingsFiles("mysql.txt", installprefix);

    DialogBox diag(argv[1]);
    a.setMainWidget(&diag);

    for (int i = 2; i < argc; i++)
    {
        diag.AddButton(argv[i]);
    }

    diag.Show();

    int result = diag.exec();

    printf("result was %d\n", result);

    return (result);
}
