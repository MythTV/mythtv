#include <qapplication.h>
#include <qsqldatabase.h>
#include <stdlib.h>

#include "libmyth/guidegrid.h"
#include "libmyth/settings.h"
#include "libmyth/themedmenu.h"

Settings *globalsettings;
char installprefix[] = "/usr/local";

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    globalsettings = new Settings;

    globalsettings->LoadSettingsFiles("theme.txt", installprefix);
    globalsettings->LoadSettingsFiles("mysql.txt", installprefix);
    globalsettings->LoadSettingsFiles("settings.txt", installprefix);

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

    QString startchannel = globalsettings->GetSetting("DefaultTVChannel");
    if (startchannel == "")
        startchannel = "3";

    if (a.argc() > 1)
    {
        startchannel = a.argv()[1];
    }

    QString themename = globalsettings->GetSetting("Theme");
    QString themedir = findThemeDir(themename, installprefix);
  
    themedir += "/qtlook.txt";
    globalsettings->ReadSettings(themedir);

    GuideGrid gg(startchannel);

    gg.exec();

    QString chanstr = gg.getLastChannel();
    int chan = 0;
    
    if (chanstr != QString::null)
        atoi(chanstr.ascii());
    return chan;
}
