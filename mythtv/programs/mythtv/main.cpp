#include <qapplication.h>
#include <qsqldatabase.h>
#include <qstring.h>
#include <unistd.h>
#include "tv.h"

Settings *globalsettings;
char installprefix[] = "/usr/local";

int main(int argc, char *argv[])
{
    globalsettings = new Settings;

    globalsettings->LoadSettingsFiles("theme.txt", installprefix);
    globalsettings->LoadSettingsFiles("mysql.txt", installprefix);
		
    QApplication a(argc, argv);

    QString startChannel = globalsettings->GetSetting("DefaultTVChannel");
    if (startChannel == "")
        startChannel = "3";

    TV *tv = new TV(startChannel, 1, 2);
    tv->LiveTV();

    while (tv->GetState() == kState_None)
        usleep(1000);

    while (tv->GetState() != kState_None)
        usleep(1000);

    sleep(1);
    delete tv;

    exit(0);
}
