#include <qapplication.h>
#include <qsqldatabase.h>
#include <stdlib.h>

#include "guidegrid.h"
#include "settings.h"

Settings *globalsettings;
char installprefix[] = "/usr/local";

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    globalsettings = new Settings;

    globalsettings->LoadSettingsFiles("theme.txt", installprefix);
    globalsettings->LoadSettingsFiles("mysql.txt", installprefix);

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    db->setDatabaseName(globalsettings->GetSetting("DBName"));
    db->setUserName(globalsettings->GetSetting("DBUserName"));
    db->setPassword(globalsettings->GetSetting("DBPassword"));
    db->setHostName(globalsettings->GetSetting("DBHostName"));

    if (!db->open())
    {
        printf("couldn't open db\n");
        return -1;
    }

    int startchannel = 0;

    if (a.argc() > 1)
    {
        startchannel = atoi(a.argv()[1]);
    }

    GuideGrid gg(startchannel);

    gg.exec();

    int chan = gg.getLastChannel();
    return chan;
}
